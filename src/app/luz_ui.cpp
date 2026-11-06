// Luz, agua y visión del mundo (PLAN_LUZ_AGUA.md, F3 a F5b). Cada frame se elige un solo efecto
// sobre lo que va por debajo de CAPA_EFECTOS:
//   bajo el agua: tinte azul (1416.img, por filas) y ola;
//   elfo o elfo oscuro en la oscuridad: el filtro de visión (reemplaza a la luz);
//   noche y mazmorras: el mapa de brillo, con las luces de los personajes en pantalla.
// Cada efecto se puede apagar en la configuración (cfg.efecto_*).
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include "app/app.h"
#include "mundo/luz.h"
#include "recursos/imagen.h"

namespace lin {

namespace {

constexpr int CELDA = 2;          // píxeles lógicos por celda (la luz del original va a media resolución)
constexpr int RADIO_JUGADOR = 5;  // el jugador ve a este radio aunque no tenga luz (mínimo del original)
constexpr int DIA = 86400;        // segundos de un día de juego (S_GameTime)

// source.lmp: 16 desplazamientos u32, 16 pares (ancho, alto) i32 y los datos. La máscara del radio r
// es la entrada r (radio 14 = 312 × 156 celdas).
struct Mascara {
    const u8* px;
    int w, h;
};

bool leer_mascara(Bytes lmp, int radio, Mascara& m) {
    if (radio < 0 || radio > 15 || lmp.size() < 192) return false;
    u32 off;
    i32 w, h;
    std::memcpy(&off, lmp.data() + 4 * radio, 4);
    std::memcpy(&w, lmp.data() + 64 + 8 * radio, 4);
    std::memcpy(&h, lmp.data() + 68 + 8 * radio, 4);
    if (w <= 0 || h <= 0 || off + static_cast<size_t>(w) * h > lmp.size()) return false;
    m = {lmp.data() + off, w, h};
    return true;
}

// La máscara va a media resolución: una entrada por celda, no por píxel. Se centra en la celda
// (cx, cy) de los pies y se toma el máximo con lo que ya había.
void estampar(std::vector<u8>& mascara, int bw, int bh, const Mascara& m, int cx, int cy) {
    const int izq = cx - m.w / 2, arr = cy - m.h / 2;
    const int x0 = std::max(0, izq), x1 = std::min(bw - 1, izq + m.w - 1);
    const int y0 = std::max(0, arr), y1 = std::min(bh - 1, arr + m.h - 1);
    for (int y = y0; y <= y1; ++y) {
        const int my = y - arr;
        for (int x = x0; x <= x1; ++x) {
            u8& d = mascara[static_cast<size_t>(y) * bw + x];
            d = std::max(d, m.px[static_cast<size_t>(my) * m.w + (x - izq)]);
        }
    }
}

// Color por fila del agua: el promedio de los píxeles con color de cada fila de 1416.img (el degradado
// es vertical; el transparente, valor 0, no cuenta: si no, las filas con huecos se verían negras). Se
// carga una vez; si la imagen no está, queda un blanco (sin tinte).
void cargar_agua_filas(App& a) {
    ImagenRGBA img;
    if (decodificar_img(a.rec.sprite("1416.img"), img) && img.alto > 0) {
        a.agua_filas.resize(img.alto);
        u32 anterior = BLANCO;
        for (int y = 0; y < img.alto; ++y) {
            u32 sr = 0, sg = 0, sb = 0, n = 0;
            for (int x = 0; x < img.ancho; ++x) {
                const u32 p = img.px[static_cast<size_t>(y) * img.ancho + x];
                if (!p) continue;
                sr += p & 0xFF;
                sg += (p >> 8) & 0xFF;
                sb += (p >> 16) & 0xFF;
                ++n;
            }
            if (n) anterior = rgba(static_cast<u8>(sr / n), static_cast<u8>(sg / n), static_cast<u8>(sb / n));
            a.agua_filas[y] = anterior;
        }
    } else {
        a.agua_filas.assign(1, BLANCO);
    }
}

void aplicar_agua(App& a, u64 ahora, int w, int h) {
    if (a.cfg.efecto_agua_tinte) {
        if (a.agua_filas.empty()) cargar_agua_filas(a);
        static const u8 uno = 255;
        a.gpu.multiplicar_mundo(&uno, 1, 1, std::max(w, h) + 1, BLANCO, a.agua_filas.data(),
                                static_cast<int>(a.agua_filas.size()));
    }
    // La ola avanza una columna cada 50 ms y da una vuelta cada 5 s (ver desplazamiento_ola).
    if (a.cfg.efecto_agua_ola) a.gpu.reemplazar_mundo(static_cast<int>((ahora / 50) % 100), 0);
}

// Brillo por celda: la LUT del nivel sobre el máximo de las máscaras de las luces en pantalla.
void aplicar_oscuridad(App& a, int nivel, u32 color, int w, int h) {
    const Mundo& m = a.mundo;
    const int bw = (w + CELDA - 1) / CELDA, bh = (h + CELDA - 1) / CELDA;
    const size_t n = static_cast<size_t>(bw) * bh;
    a.luz_mascara.assign(n, 0);

    const Bytes lmp = a.rec.sprite("source.lmp");
    const auto& objs = m.objetos;
    // Jugador: su propia luz (S_OwnCharPack no la trae) o el radio mínimo.
    const Slot sj = objs.buscar(m.jugador.id);
    const int radio_j = std::max(RADIO_JUGADOR, sj != SIN_SLOT ? static_cast<int>(objs.col<O_LUZ>()[sj]) : 0);
    for (const App::ObjetoEnPantalla& r : a.en_pantalla) {
        int radio = 0;
        if (r.id == m.jugador.id) {
            radio = radio_j;
        } else {
            const Slot s = objs.buscar(r.id);
            if (s == SIN_SLOT) continue;
            radio = static_cast<int>(objs.col<O_LUZ>()[s]);
        }
        if (radio <= 0) continue;
        Mascara mk;
        if (!leer_mascara(lmp, radio, mk)) continue;
        estampar(a.luz_mascara, bw, bh, mk, static_cast<int>(std::lround(r.pie_x / CELDA)),
                 static_cast<int>(std::lround(r.pie_y / CELDA)));
    }

    a.luz_brillo.resize(n);
    for (size_t i = 0; i < n; ++i) a.luz_brillo[i] = static_cast<u8>(brillo_lut(nivel, a.luz_mascara[i]) * 255 / 31);
    a.gpu.multiplicar_mundo(a.luz_brillo.data(), bw, bh, CELDA, color);
}

}  // namespace

void aplicar_luz(App& a, u64 ahora) {
    const Mundo& m = a.mundo;
    const int w = a.gpu.ancho(), h = a.gpu.alto();
    if (m.submarino) {
        aplicar_agua(a, ahora, w, h);
        return;
    }

    const int seg = static_cast<int>(((m.hora % DIA) + DIA) % DIA);
    const bool lluvia = (m.clima >= 1 && m.clima <= 3) || (m.clima >= 17 && m.clima <= 19);
    std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_r(&t, &local);
    const int nivel = nivel_ambiente(m.mapa, seg / 60, local.tm_yday + 1, lluvia);

    // Elfo y elfo oscuro: la visión reemplaza a la luz (no hay luces ni mapa de brillo).
    const int clase = (a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size())) ? a.pjs[a.pj_sel].v[PJ_CLASE] : -1;
    const Vision vis = modo_vision(static_cast<u8>(clase < 0 ? 255 : clase), a.cfg.efecto_infravision, nivel, false,
                                   m.jugador.ceguera);
    if (vis != Vision::NINGUNA) {
        a.gpu.reemplazar_mundo(-1, vis == Vision::ELFO ? 1 : 2);
        return;
    }

    u8 tr = 255, tg = 255, tb = 255;
    const bool tinte = tinte_mapa(m.mapa, tr, tg, tb);
    const u32 color = tinte ? rgba(tr, tg, tb) : BLANCO;
    if (!(a.cfg.efecto_luz && nivel < 15)) {
        // Día, o luz apagada: solo el tinte de mapa, con una celda que cubre toda la vista.
        if (tinte) {
            static const u8 uno = 255;
            a.gpu.multiplicar_mundo(&uno, 1, 1, std::max(w, h) + 1, color);
        }
        return;
    }
    aplicar_oscuridad(a, nivel, color, w, h);
}

}  // namespace lin
