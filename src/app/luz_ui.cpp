// Luz, agua y visión del mundo (PLAN_LUZ_AGUA.md, F3 a F5b). Cada frame se elige un solo efecto
// sobre lo que va por debajo de CAPA_EFECTOS:
//   bajo el agua: tinte azul (1416.img, por filas) y ola;
//   elfo o elfo oscuro en la oscuridad: el filtro de visión (reemplaza a la luz);
//   noche y mazmorras: el mapa de brillo, con las luces de los personajes en pantalla.
// Cada efecto se puede apagar en la configuración (cfg.efecto_*).
#include <algorithm>
#include <cmath>
#include <cstring>
#include "app/app.h"
#include "mundo/luz.h"
#include "recursos/imagen.h"

namespace lin {

namespace {

constexpr int CELDA = 2;          // píxeles lógicos por celda (la luz del original va a media resolución)
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

// Promedio de los píxeles con color de una fila de 1416.img (el transparente, valor 0, no cuenta).
// 0 si la fila es toda transparente.
u32 color_de_fila(const ImagenRGBA& img, int y) {
    u32 sr = 0, sg = 0, sb = 0, n = 0;
    for (int x = 0; x < img.ancho; ++x) {
        const u32 p = img.px[static_cast<size_t>(y) * img.ancho + x];
        if (!p) continue;
        sr += p & 0xFF;
        sg += (p >> 8) & 0xFF;
        sb += (p >> 16) & 0xFF;
        ++n;
    }
    return n ? rgba(static_cast<u8>(sr / n), static_cast<u8>(sg / n), static_cast<u8>(sb / n)) : 0;
}

// Color por fila del agua (el degradado es vertical). Las filas transparentes toman el color de la
// anterior, y las de arriba del todo, el de la primera con datos (si no, quedarían sin tinte). Se carga
// una vez; si la imagen no está, queda un blanco (sin tinte).
void cargar_agua_filas(App& a) {
    ImagenRGBA img;
    if (!decodificar_img(a.rec.sprite("1416.img"), img) || img.alto <= 0) {
        a.agua_filas.assign(1, BLANCO);
        return;
    }
    u32 primero = 0;
    for (int y = 0; y < img.alto && !primero; ++y) primero = color_de_fila(img, y);
    u32 anterior = primero ? primero : BLANCO;
    a.agua_filas.resize(img.alto);
    for (int y = 0; y < img.alto; ++y) {
        const u32 c = color_de_fila(img, y);
        if (c) anterior = c;
        a.agua_filas[y] = anterior;
    }
}

void aplicar_agua(App& a, u64 ahora, int w, int h, int fase_borracho) {
    if (a.cfg.efecto_agua_tinte) {
        static const u8 uno = 255;
        a.gpu.multiplicar_mundo(&uno, 1, 1, std::max(w, h) + 1, BLANCO, a.agua_filas.data(),
                                static_cast<int>(a.agua_filas.size()));
    }
    // La ola avanza una columna cada 50 ms y da una vuelta cada 5 s (ver desplazamiento_ola).
    const int fase_ola = a.cfg.efecto_agua_ola ? static_cast<int>((ahora / 50) % 100) : -1;
    a.gpu.reemplazar_mundo(fase_ola, 0, fase_borracho);
}

// Brillo por celda: la LUT del nivel sobre el máximo de las máscaras de las luces en pantalla.
void aplicar_oscuridad(App& a, int nivel, u32 color, int w, int h) {
    const int bw = (w + CELDA - 1) / CELDA, bh = (h + CELDA - 1) / CELDA;
    const size_t n = static_cast<size_t>(bw) * bh;
    a.luz_mascara.assign(n, 0);

    const Bytes lmp = a.rec.sprite("source.lmp");
    for (const App::LuzEnPantalla& l : a.luces) {
        Mascara mk;
        if (!leer_mascara(lmp, l.radio, mk)) continue;
        estampar(a.luz_mascara, bw, bh, mk, static_cast<int>(std::lround(l.x / CELDA)),
                 static_cast<int>(std::lround(l.y / CELDA)));
    }

    a.luz_brillo.resize(n);
    for (size_t i = 0; i < n; ++i) a.luz_brillo[i] = static_cast<u8>(brillo_lut(nivel, a.luz_mascara[i]) * 255 / 31);
    a.gpu.multiplicar_mundo(a.luz_brillo.data(), bw, bh, CELDA, color);
}

}  // namespace

void preparar_luz(App& a) {
    cargar_agua_filas(a);
    a.luces.reserve(512);
    // Lo que pida la vista más grande de esta ventana (zoom x1). Solo una ventana que crezca después
    // vuelve a reservar, y una sola vez.
    int pw = 0, ph = 0;
    if (a.ventana) SDL_GetWindowSizeInPixels(a.ventana, &pw, &ph);
    if (pw <= 0 || ph <= 0) return;
    const int bw = (pw + CELDA - 1) / CELDA, bh = (ph + CELDA - 1) / CELDA;
    const size_t n = static_cast<size_t>(bw) * bh;
    a.luz_mascara.reserve(n);
    a.luz_brillo.reserve(n);
    a.gpu.reservar_brillo(bw, bh);
}

i32 hora_de_juego(const App& a) { return a.hora_forzada >= 0 ? a.hora_forzada : a.mundo.hora; }

void aplicar_luz(App& a, u64 ahora) {
    const Mundo& m = a.mundo;
    const int w = a.gpu.ancho(), h = a.gpu.alto();
    // Borrachera (S_Liquor 1): el mundo se mece por filas; va en el mismo pase que la ola y la visión.
    const int fase_borracho = m.jugador.borracho ? static_cast<int>((ahora / 50) % 100) : -1;
    if (m.submarino) {
        aplicar_agua(a, ahora, w, h, fase_borracho);
        return;
    }

    const i32 hora = hora_de_juego(a);
    const int seg = static_cast<int>(((hora % DIA) + DIA) % DIA);
    const bool lluvia = m.nieve > 0 || m.lluvia > 0;
    int nivel = nivel_ambiente(m.mapa, seg / 60, dia_del_anio(hora), lluvia);
    if (m.jugador.luz_hechizo) nivel = nivel_con_luz_hechizo(nivel);

    // Elfo y elfo oscuro: la visión reemplaza a la luz (no hay luces ni mapa de brillo).
    const int clase = (a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size())) ? a.pjs[a.pj_sel].v[PJ_CLASE] : -1;
    const Vision vis = modo_vision(static_cast<u8>(clase < 0 ? 255 : clase), a.cfg.efecto_infravision, nivel, false,
                                   m.jugador.ceguera);
    if (vis != Vision::NINGUNA) {
        a.gpu.reemplazar_mundo(-1, vis == Vision::ELFO ? 1 : 2, fase_borracho);
        return;
    }

    if (fase_borracho >= 0) a.gpu.reemplazar_mundo(-1, 0, fase_borracho);
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
