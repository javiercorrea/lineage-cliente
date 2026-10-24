#include <algorithm>
#include "recursos/sprites.h"

namespace lin {

bool ArchivoSpr::abrir(Bytes spr) {
    frames.clear();
    tiles_frame.clear();
    tiles.clear();
    paleta.clear();
    LectorBinario l{spr};
    if (spr.size() >= 5 && std::memcmp(spr.data(), "SPR_", 4) == 0) {
        if (spr[4] != '0') return false;  // SPR_1: formato no soportado (tampoco en l1j-client)
        l.saltar(5);
    }
    i8 n = l.i8_();
    if (n < 0) {
        u32 entradas = l.u8_();
        if (entradas == 0) entradas = 256;
        paleta.resize(entradas);
        for (u16& c : paleta) c = l.u16_();
        n = l.i8_();
    }
    if (n < 0) return false;
    frames.resize(static_cast<size_t>(n));
    for (FrameSpr& f : frames) {
        f.x1 = l.i16_();
        f.y1 = l.i16_();
        f.x2 = l.i16_();
        f.y2 = l.i16_();
        l.u32_();
        f.ntiles = l.u16_();
        f.primer_tile = static_cast<u32>(tiles_frame.size());
        for (u32 j = 0; j < f.ntiles; ++j) {
            TileDeFrame t;
            t.x = l.i8_();
            t.y = l.i8_();
            t.h = l.u8_();
            t.tile = l.u16_();
            tiles_frame.push_back(t);
        }
    }
    u32 ntiles = l.u32_();
    if (l.error || !l.quedan(static_cast<size_t>(ntiles) * 4 + 4)) return false;
    std::vector<u32> offsets(ntiles);
    for (u32& o : offsets) o = l.u32_();
    l.u32_();  // tamaño total de los datos de tiles
    size_t base = l.pos;
    tiles.resize(ntiles);
    // Los offsets no siempre están ordenados: cada tile se delimita solo al leerlo (con lectura
    // acotada), así que su vista va hasta el final del archivo.
    for (u32 i = 0; i < ntiles; ++i) {
        size_t ini = base + offsets[i];
        if (ini > spr.size()) return false;
        tiles[i] = spr.subspan(ini);
    }
    return !l.error;
}

// Píxeles de un tile de sprite sobre `salida` en (px, py).
static void pintar_tile(Bytes t, const std::vector<u16>& paleta, ImagenRGBA& salida, int px, int py) {
    LectorBinario l{t};
    int x = l.u8_(), y = l.u8_(), w = l.u8_(), h = l.u8_();
    (void)w;
    for (int fila = 0; fila < h && !l.error; ++fila) {
        int tramos = l.u8_();
        int col = 0;
        for (int s = 0; s < tramos && !l.error; ++s) {
            col += l.u8_() / 2;
            int n = l.u8_();
            for (int k = 0; k < n; ++k, ++col) {
                u16 c = paleta.empty() ? l.u16_() : paleta[l.u8_() % paleta.size()];
                int dx = px + x + col, dy = py + y + fila;
                if (dx >= 0 && dy >= 0 && dx < salida.ancho && dy < salida.alto) salida.px[dy * salida.ancho + dx] = rgb555(c);
            }
        }
    }
}

// Posición en píxeles del tile (x en medias celdas, y en celdas), con división hacia abajo.
static void posicion_tile(const TileDeFrame& t, int& px, int& py) {
    int hx = (t.x >= 0) ? t.x / 2 : -((1 - t.x) / 2);
    int impar = t.x - 2 * hx;
    px = 24 * (hx + t.y) + 24 * impar;
    py = 12 * (t.y - hx);
}

bool ArchivoSpr::componer_frame(u32 frame, ImagenRGBA& salida) const {
    if (frame >= frames.size()) return false;
    const FrameSpr& f = frames[frame];
    if (f.ntiles == 0) return false;
    // Caja: cada tile ocupa como máximo su cabecera x+ancho, y+alto desde su posición.
    int x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20);
    for (u32 j = 0; j < f.ntiles; ++j) {
        const TileDeFrame& t = tiles_frame[f.primer_tile + j];
        if (t.tile >= tiles.size() || tiles[t.tile].size() < 4) continue;
        Bytes d = tiles[t.tile];
        int px, py;
        posicion_tile(t, px, py);
        x0 = std::min(x0, px + d[0]);
        y0 = std::min(y0, py + d[1]);
        x1 = std::max(x1, px + d[0] + d[2]);
        y1 = std::max(y1, py + d[1] + d[3]);
    }
    if (x1 <= x0 || y1 <= y0 || x1 - x0 > 2048 || y1 - y0 > 2048) return false;
    salida.preparar(static_cast<u16>(x1 - x0), static_cast<u16>(y1 - y0));
    salida.ox = static_cast<i16>(x0);
    salida.oy = static_cast<i16>(y0);
    for (u32 j = 0; j < f.ntiles; ++j) {
        const TileDeFrame& t = tiles_frame[f.primer_tile + j];
        if (t.tile >= tiles.size() || tiles[t.tile].size() < 4) continue;
        int px, py;
        posicion_tile(t, px, py);
        pintar_tile(tiles[t.tile], paleta, salida, px - x0, py - y0);
    }
    return true;
}

}  // namespace lin
