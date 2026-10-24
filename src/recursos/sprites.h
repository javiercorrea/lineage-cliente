// Archivos de sprite "G-S.spr" (G = gfx, S = sub-sprite; para acciones con dirección el
// sub-sprite es base + heading). Formato (sprite_motion.cpp de l1j-client):
//
//   [opcional "SPR_0"]  i8 n
//   si n < 0: u8 entradas_paleta (0 = 256), paleta RGB555, i8 n
//   n × frame { i16 x1, y1, x2, y2; u32 ?; u16 ntiles; ntiles × {i8 x, i8 y, u8 h, u16 tile} }
//   u32 ntiles, ntiles × u32 offset, u32 tamaño, datos de tiles
//
// Cada tile es una imagen con transparencia: x, y, ancho, alto (bytes) y filas en tramos; con
// paleta los píxeles son índices de 1 byte, si no RGB555. La posición de un tile en el frame usa
// coordenadas de media celda: x en medias celdas, y en celdas (proyección isométrica 24×12).
#pragma once
#include <vector>
#include "recursos/imagen.h"

namespace lin {

struct TileDeFrame {
    i8 x, y;
    u8 h;
    u16 tile;
};

struct FrameSpr {
    i16 x1, y1, x2, y2;
    u32 primer_tile, ntiles;  // rango en ArchivoSpr::tiles_frame
};

struct ArchivoSpr {
    std::vector<FrameSpr> frames;
    std::vector<TileDeFrame> tiles_frame;  // aplanado: los tiles de todos los frames
    std::vector<Bytes> tiles;              // datos de cada tile (vistas al .spr)
    std::vector<u16> paleta;               // vacía si los píxeles son RGB555

    bool abrir(Bytes spr);
    // Compone el frame completo en una sola imagen; ox/oy quedan relativos al ancla de la celda.
    bool componer_frame(u32 frame, ImagenRGBA& salida) const;
};

}  // namespace lin
