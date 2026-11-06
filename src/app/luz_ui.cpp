// Oscuridad del mundo (PLAN_LUZ_AGUA.md, F3). El brillo de cada celda de 2×2 píxeles sale del
// nivel ambiente (hora, estación, mapa) y del máximo de las luces de los personajes en pantalla.
// Cada luz es una máscara de source.lmp centrada en el personaje, con el radio de su S_Light.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include "app/app.h"
#include "mundo/luz.h"

namespace lin {

namespace {

constexpr int CELDA = 2;        // píxeles lógicos por celda (la luz del original va a media resolución)
constexpr int RADIO_JUGADOR = 5;  // el jugador ve a este radio aunque no tenga luz (mínimo del original)
constexpr int DIA = 86400;      // segundos de un día de juego (S_GameTime)

// source.lmp: 16 desplazamientos u32, 16 pares (ancho, alto) i32 y los datos. La máscara del radio r
// es la entrada r (radio 14 = 312 × 156 px).
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

}  // namespace

void aplicar_luz(App& a, u64 ahora) {
    (void)ahora;
    const Mundo& m = a.mundo;
    if (m.submarino) return;  // bajo el agua no hay noche (lo hace el tinte azul, F4)
    const int w = a.gpu.ancho(), h = a.gpu.alto();

    const int seg = static_cast<int>(((m.hora % DIA) + DIA) % DIA);
    const bool lluvia = (m.clima >= 1 && m.clima <= 3) || (m.clima >= 17 && m.clima <= 19);
    std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_r(&t, &local);
    const int nivel = nivel_ambiente(m.mapa, seg / 60, local.tm_yday + 1, lluvia);
    u8 tr = 255, tg = 255, tb = 255;
    const bool tinte = tinte_mapa(m.mapa, tr, tg, tb);
    const u32 color = tinte ? rgba(tr, tg, tb) : BLANCO;

    if (nivel >= 15) {
        // Día pleno: solo el tinte de mapa, con una sola celda que cubre toda la vista.
        if (tinte) {
            static const u8 uno = 255;
            a.gpu.multiplicar_mundo(&uno, 1, 1, std::max(w, h) + 1, color);
        }
        return;
    }

    const int bw = (w + CELDA - 1) / CELDA, bh = (h + CELDA - 1) / CELDA;
    const size_t n = static_cast<size_t>(bw) * bh;
    a.luz_mascara.assign(n, 0);

    const Bytes lmp = a.rec.sprite("source.lmp");
    const auto& objs = m.objetos;
    // Jugador: su propia luz (S_OwnCharPack no la trae) o el radio mínimo.
    const Slot sj = objs.buscar(m.jugador.id);
    const int radio_j = std::max(RADIO_JUGADOR, sj != SIN_SLOT ? static_cast<int>(objs.col<O_LUZ>()[sj]) : 0);
    for (const App::ObjetoEnPantalla& r : a.en_pantalla) {
        const bool yo = r.id == m.jugador.id;
        int radio = 0;
        if (yo) {
            radio = radio_j;
        } else {
            const Slot s = objs.buscar(r.id);
            if (s == SIN_SLOT) continue;
            radio = static_cast<int>(objs.col<O_LUZ>()[s]);
        }
        if (radio <= 0) continue;
        Mascara mk;
        if (!leer_mascara(lmp, radio, mk)) continue;
        const int cx = static_cast<int>(std::lround(r.pie_x / CELDA)), cy = static_cast<int>(std::lround(r.pie_y / CELDA));
        estampar(a.luz_mascara, bw, bh, mk, cx, cy);
    }

    // Brillo final: la LUT del nivel sobre cada máscara, en 0..255 para la textura.
    a.luz_brillo.resize(n);
    for (size_t i = 0; i < n; ++i)
        a.luz_brillo[i] = static_cast<u8>(brillo_lut(nivel, a.luz_mascara[i]) * 255 / 31);
    a.gpu.multiplicar_mundo(a.luz_brillo.data(), bw, bh, CELDA, color);
}

}  // namespace lin
