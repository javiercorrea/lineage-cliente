#include "recursos/tiles.h"

namespace lin {

bool JuegoTiles::abrir(Bytes til) {
    LectorBinario l{til};
    cantidad = l.u16_();
    l.saltar(2);  // los offsets empiezan en el byte 4 (TIL usa u32 para la cantidad en algunos)
    if (l.error || !l.quedan((cantidad + 1) * 4ull)) return false;
    datos = til;
    return true;
}

Bytes JuegoTiles::tile(u32 i) const {
    if (i >= cantidad) return {};
    LectorBinario l{datos};
    l.pos = 4 + i * 4;
    u32 a = l.u32_(), b = l.u32_();
    size_t base = 4 * (2 + static_cast<size_t>(cantidad));
    if (b < a || base + b > datos.size()) return {};
    return datos.subspan(base + a, b - a);
}

// Tile con transparencia: x, y, ancho, alto (bytes) y por fila: nº de tramos, y por tramo
// (salto en bytes, ancho en píxeles, píxeles RGB555).
static bool decodificar_transparente(Bytes t, ImagenRGBA& salida) {
    LectorBinario l{t};
    u8 x = l.u8_(), y = l.u8_(), w = l.u8_(), h = l.u8_();
    if (l.error || w == 0 || h == 0) return false;
    salida.preparar(w, h);
    salida.ox = x;
    salida.oy = y;
    for (int fila = 0; fila < h && !l.error; ++fila) {
        int tramos = l.u8_();
        int col = 0;
        for (int s = 0; s < tramos && !l.error; ++s) {
            col += l.u8_() / 2;
            int n = l.u8_();
            for (int k = 0; k < n; ++k, ++col) {
                u16 c = l.u16_();
                if (col < w) salida.px[fila * w + col] = rgb555(c);
            }
        }
    }
    return !l.error;
}

bool decodificar_tbt(Bytes tbt, ImagenRGBA& salida) { return decodificar_transparente(tbt, salida); }

bool JuegoTiles::translucido(u32 i) const {
    Bytes t = tile(i);
    return !t.empty() && (t[0] & 4);
}

bool JuegoTiles::decodificar(u32 i, bool derecha, ImagenRGBA& salida) const {
    Bytes t = tile(i);
    if (t.empty()) return false;
    if (t[0] & 2) {
        if (!decodificar_transparente(t.subspan(1), salida)) return false;
        if (t[0] & 4)
            for (u32& p : salida.px)
                if (p >> 24) p = (p & 0xFFFFFF) | (static_cast<u32>(ALFA_TRANSLUCIDO) << 24);
        return true;
    }
    LectorBinario l{t.subspan(1)};
    salida.preparar(ANCHO_MEDIO_TILE, ALTO_TILE);
    for (int fila = 0; fila < ALTO_TILE; ++fila) {
        int ancho = 2 * (fila + 1);
        if (fila > 11) ancho -= 4 * (fila - 11);
        int x0 = derecha ? 0 : ANCHO_MEDIO_TILE - ancho;
        for (int k = 0; k < ancho; ++k) salida.px[fila * ANCHO_MEDIO_TILE + x0 + k] = rgb555(l.u16_());
    }
    return !l.error;
}

}  // namespace lin
