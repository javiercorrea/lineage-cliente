#include "recursos/fuente_ttf.h"
#include <algorithm>
#include <cmath>
#include <vector>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include "recursos/fuente.h"
#include "recursos/fuente_ui_datos.inc"

namespace lin {
namespace {

// Tamaño del em en píxeles. Con la celda de ALTO_GLIFO (12) la tilde de una mayúscula acentuada
// (0,8 em sobre la base) y el descendente (0,2 em bajo ella) tienen que caber: 13 px lo logra.
// shortcut: a 16 px los descendentes se recortan; si se cambia el alto de celda, revisar este valor.
constexpr float PX_EM = 13.0f;

struct Ttf {
    stbtt_fontinfo info{};
    float escala = 0;
    int ascenso = 0;  // píxeles desde el tope de la celda hasta la línea base
    bool ok = false;
};

// Se carga una sola vez; el inicio de una variable local static es seguro entre hilos.
const Ttf& ttf() {
    static const Ttf t = [] {
        Ttf r;
        r.ok = stbtt_InitFont(&r.info, FUENTE_UI_DATOS, 0) != 0;
        if (r.ok) {
            r.escala = stbtt_ScaleForPixelHeight(&r.info, PX_EM);
            int asc = 0, desc = 0, gap = 0;
            stbtt_GetFontVMetrics(&r.info, &asc, &desc, &gap);
            r.ascenso = static_cast<int>(std::lround(asc * r.escala));
        }
        return r;
    }();
    return t;
}

// Índice del glifo de `cp`; si la fuente no lo tiene, el del '?'.
int indice_de(const Ttf& t, u32& cp) {
    int idx = stbtt_FindGlyphIndex(&t.info, static_cast<int>(cp));
    if (idx == 0 && cp != '?') {
        cp = '?';
        idx = stbtt_FindGlyphIndex(&t.info, '?');
    }
    return idx;
}

}  // namespace

int ancho_ttf(u32 cp) {
    const Ttf& t = ttf();
    if (!t.ok) return ANCHO_GLIFO_ASCII;
    const int idx = indice_de(t, cp);
    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&t.info, idx, &adv, &lsb);
    return static_cast<int>(std::lround(adv * t.escala));
}

bool rasterizar_ttf(u32 cp, ImagenRGBA& salida) {
    const Ttf& t = ttf();
    if (!t.ok) return false;
    const int idx = indice_de(t, cp);
    const int ancho = std::max(1, ancho_ttf(cp));
    salida.preparar(static_cast<u16>(ancho), ALTO_GLIFO);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(&t.info, idx, t.escala, t.escala, &x0, &y0, &x1, &y1);
    const int w = x1 - x0, h = y1 - y0;
    if (w <= 0 || h <= 0) return true;  // espacio: solo avance
    static thread_local std::vector<unsigned char> alfa;  // sin asignar en cada glifo
    alfa.assign(static_cast<size_t>(w) * h, 0);
    stbtt_MakeGlyphBitmap(&t.info, alfa.data(), w, h, w, t.escala, t.escala, idx);
    for (int y = 0; y < h; ++y) {
        const int cy = t.ascenso + y0 + y;
        for (int x = 0; x < w; ++x) {
            const int cx = x0 + x;
            if (cx < 0 || cy < 0 || cx >= ancho || cy >= ALTO_GLIFO) continue;
            const u32 a = alfa[static_cast<size_t>(y) * w + x];
            // Premultiplicado (el blend de la GPU lo es): blanco con alfa a es a en los cuatro bytes.
            salida.px[static_cast<size_t>(cy) * ancho + cx] = a * 0x01010101u;
        }
    }
    return true;
}

}  // namespace lin
