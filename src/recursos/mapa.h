// Segmentos de mapa map/<id>/<XXXXYYYY>.s32: 64×64 celdas a partir de (X-0x7E00)*64, (Y-0x7E00)*64.
//
// Formato (handle_s32 de l1j-client):
//   64×64 × {u32 suelo izquierdo, u32 suelo derecho}   (valor: juego de tiles << 8 | tile)
//   u16 n, n × 6 bytes (desconocido)
//   64×64 × {u16 atributo izquierdo, u16 atributo derecho}
//   u32 nobjetos, nobjetos × {u16 índice, u16 ntiles, ntiles × {u8 x, u8 y, u8 capa, u32 tile}}
//     (un par x=0xCD,y=0xCD es relleno de 7 bytes que no cuenta)
//   u32 n, n × 5 bytes ("HideSwitches" de l1j-client; sin usar)
//   u32 n, n × u32 (juegos de tiles del segmento; sin usar)
//   u16 n, n portales × {u8 largo, nombre, u8 x en medias celdas, u8 y, u16 mapa destino, i16, i16}
//     (los dos i16 no se sabe qué son; l1j-client leía los portales con otro formato, mal)
// El orden de las celdas en el archivo es fila por fila: y exterior, x interior. Esto está
// verificado contra los mapas del servidor en tests/test_recursos.cpp.
#pragma once
#include <string>
#include <vector>
#include "core/tipos.h"

namespace lin {

constexpr int CELDAS_SEGMENTO = 64;

struct TileObjetoMapa {
    u8 x, y;     // x en medias celdas, y en celdas (relativas al segmento)
    u8 capa;
    i8 base;     // fila del pie de la pieza (ver abajo) menos su propia fila (y - x/2); >= 0
    u32 tile;    // juego << 8 | tile
};

// Un objeto del mapa (árbol, casa, cerca): sus piezas van seguidas en tiles_objetos. El rectángulo
// (en celdas relativas al segmento; las piezas del borde pueden salir de 0..63) deja descartar el
// objeto entero sin mirar sus piezas.
struct ObjetoMapa {
    u32 primera, cantidad;
    u8 x0, y0, x1, y1;
};

// Orden de dibujo de los objetos del mapa. Un objeto (árbol, casa, cerca) son muchas piezas de
// media celda; la copa de un árbol ocupa celdas más arriba en pantalla que el tronco. Para que un
// personaje que pasa por detrás quede tapado, cada pieza se ordena por la fila más baja del objeto
// en su columna de pantalla y en las vecinas (±COLUMNAS_BASE medias columnas): la copa usa la
// fila del tronco, y una pared larga usa la de su propio tramo. Las piezas no pasan de 24 px de
// ancho (verificado en el mapa 2005), así que columnas distintas no se tapan entre sí.
constexpr int COLUMNAS_BASE = 6;

// Las piezas de capa baja van pegadas al suelo: sombras de árboles, parches de arena, flores. En
// el mapa 2005 hay ~36 000 en las capas 1–9 y casi ninguna en 10–15; los objetos que se levantan
// (troncos, copas, cercas, casas) empiezan en la 16 y suben de a 4. Se dibujan con el suelo,
// debajo de todos los personajes, y no cuentan para el pie de los objetos.
constexpr u8 CAPA_MAX_SUELO = 15;
inline bool pieza_de_suelo(const TileObjetoMapa& t) { return t.capa <= CAPA_MAX_SUELO; }

// Portal del mapa (entrada a una mazmorra, escalera, cruce): el original manda C_EnterPortal con
// su celda al intentar entrar. Coincide con DungeonTable del servidor en la mayoría (ver DESCUBRIMIENTOS).
struct PortalMapa {
    u8 x, y;    // celda relativa al segmento
    u16 mapa;   // destino
};

struct SegmentoMapa {
    u16 base_x = 0, base_y = 0;                    // coordenadas de la celda (0,0) del segmento
    std::vector<u32> suelo;                        // [y][x][2]
    std::vector<u16> atributos;                    // [y][x][2]
    std::vector<TileObjetoMapa> tiles_objetos;     // aplanado, en orden de archivo
    std::vector<ObjetoMapa> objetos;
    std::vector<PortalMapa> portales;
    // Celdas con una pieza de sombra (tile translúcido; ver tiles.h), un u64 por fila. Hace falta
    // leer los .til para saberlo: lo calcula el render la primera vez que lo necesita.
    mutable std::vector<u64> sombra;

    u32 suelo_en(int x, int y, int mitad) const { return suelo[(y * CELDAS_SEGMENTO + x) * 2 + mitad]; }
    u16 atributo_en(int x, int y, int mitad) const { return atributos[(y * CELDAS_SEGMENTO + x) * 2 + mitad]; }
};

// Nombre del archivo del segmento que contiene la celda (x, y).
std::string nombre_segmento(MapId mapa, int x, int y);
bool decodificar_s32(Bytes datos, int base_x, int base_y, SegmentoMapa& salida);

}  // namespace lin
