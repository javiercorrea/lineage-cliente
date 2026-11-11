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

// Departure Mono está dibujada sobre una cuadrícula de 50 unidades por píxel y su em son 550 unidades:
// a 11 px de em, cada píxel de la fuente cae exacto en un píxel de pantalla, sin suavizado.
// Avance 7 px, mayúsculas 8, x-height 6, tilde hasta 11 y descendente 2: cabe en ALTO_GLIFO (14).
// shortcut: si se cambia la fuente o el alto de celda, revisar el em (debe dar una cuadrícula entera).
constexpr float PX_EM = 11.0f;
// Cobertura a partir de la cual un píxel se enciende (0–255). Con la cuadrícula alineada, casi todo
// cae en 0 o 255 de todos modos; el umbral solo recorta los bordes de los píxeles que no caen exactos.
constexpr unsigned char UMBRAL_TRAZO = 128;

// Avances precalculados de los puntos de código más comunes (la fuente es Latin-1): medir texto los pide
// varias veces por carácter y por frame, y buscar el glifo en la fuente es una búsqueda binaria más.
constexpr u32 CODEPUNTOS_TABLA = 0x800;

struct Ttf {
    stbtt_fontinfo info{};
    float escala = 0;  // el em en píxeles
    int ascenso = 0;   // píxeles desde el tope de la celda hasta la línea base
    bool ok = false;
    u8 ancho[CODEPUNTOS_TABLA] = {};
};

// Índice del glifo de `cp`; si la fuente no lo tiene, el del '?'.
int indice_de(const Ttf& t, u32& cp) {
    int idx = stbtt_FindGlyphIndex(&t.info, static_cast<int>(cp));
    if (idx == 0 && cp != '?') {
        cp = '?';
        idx = stbtt_FindGlyphIndex(&t.info, '?');
    }
    return idx;
}

int calcular_ancho(const Ttf& t, u32 cp) {
    const int idx = indice_de(t, cp);
    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&t.info, idx, &adv, &lsb);
    return static_cast<int>(std::lround(adv * t.escala));
}

// Se carga una sola vez; el inicio de una variable local static es seguro entre hilos.
const Ttf& ttf() {
    static const Ttf t = [] {
        Ttf r;
        r.ok = stbtt_InitFont(&r.info, FUENTE_UI_DATOS, 0) != 0;
        if (r.ok) {
            // Em, no altura ascendente+descendente (ScaleForPixelHeight daría otra escala con esta fuente).
            r.escala = stbtt_ScaleForMappingEmToPixels(&r.info, PX_EM);
            int asc = 0, desc = 0, gap = 0;
            stbtt_GetFontVMetrics(&r.info, &asc, &desc, &gap);
            r.ascenso = static_cast<int>(std::lround(asc * r.escala));
            for (u32 cp = 0; cp < CODEPUNTOS_TABLA; ++cp) r.ancho[cp] = static_cast<u8>(std::clamp(calcular_ancho(r, cp), 0, 255));
        }
        return r;
    }();
    return t;
}

}  // namespace

int ancho_ttf(u32 cp) {
    const Ttf& t = ttf();
    if (!t.ok) return ANCHO_GLIFO_ASCII;
    return cp < CODEPUNTOS_TABLA ? t.ancho[cp] : calcular_ancho(t, cp);
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
            // Sin suavizado: como la fuente original, cada píxel está encendido o apagado.
            const u32 a = alfa[static_cast<size_t>(y) * w + x] >= UMBRAL_TRAZO ? 255u : 0u;
            // Premultiplicado (el blend de la GPU lo es): con a = 0 o 255 es blanco opaco o transparente.
            salida.px[static_cast<size_t>(cy) * ancho + cx] = a * 0x01010101u;
        }
    }
    return true;
}

}  // namespace lin
