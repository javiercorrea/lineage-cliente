// Segmentos de mapa map/<id>/<XXXXYYYY>.s32: 64×64 celdas a partir de (X-0x7E00)*64, (Y-0x7E00)*64.
//
// Formato (handle_s32 de l1j-client):
//   64×64 × {u32 suelo izquierdo, u32 suelo derecho}   (valor: juego de tiles << 8 | tile)
//   u16 n, n × {u8 x en medias celdas, u8 y, u32 tile}   (segunda capa de suelo: ver abajo)
//   64×64 × {u16 atributo izquierdo, u16 atributo derecho}
//   u32 nobjetos, nobjetos × {u16 índice, u16 ntiles, ntiles × {u8 x, u8 y, u8 capa, u32 tile}}
//     (un par x=0xCD,y=0xCD es relleno de 7 bytes que no cuenta)
//   u32 n, n × 5 bytes ("HideSwitches" de l1j-client; sin usar)
//   u32 n, n × u32 (juegos de tiles del segmento; sin usar)
//   u16 n, n portales × {u8 largo, nombre, u8 x en medias celdas, u8 y, u16 mapa destino, i16, i16}
//     (los dos i16 no se sabe qué son; l1j-client leía los portales con otro formato, mal)
//   u16 n (bit alto: entradas de 10 bytes), n × {u16 gfx, u16 x, u16 y [, i32]}  (efectos fijos: ver EfectoMapa)
// El orden de las celdas en el archivo es fila por fila: y exterior, x interior. Esto está
// verificado contra los mapas del servidor en tests/test_recursos.cpp.
#pragma once
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "core/tipos.h"

namespace lin {

constexpr int CELDAS_SEGMENTO = 64;

// Segunda capa de suelo (la tabla de n x 6 bytes después del suelo): tiles transparentes (los tiles de borde de
// pasto de 89.til 134, 156, 178, 182, 187, 190...) puestos sobre el suelo a media celda. La capa de suelo
// sola deja en los bosques una retícula regular de medias celdas de tierra marrón (mapa 4, alrededor de
// 32639,32985) porque son estos tiles los que las tapan. decodificar_s32 los agrega como un
// pseudoobjeto de piezas de capa 0 (la capa más baja, pegada al suelo), así se dibujan sobre el suelo
// y debajo de las sombras.

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
    u16 gid = 0xFFFF;  // id de grupo de objetos del archivo (los registros de techo lo referencian); 0xFFFF = ninguno
};

// Registro de techo del .s32 (bloque después de los objetos de tiles): el grupo de objetos `gid` se desvanece mientras el jugador local está
// en la celda. tipo 0 se desvanece al 50%, tipo 1 hasta ocultarse. x es la columna de la celda (medias celdas / 2), relativa al segmento.
struct TechoMapa {
    u8 x, y;
    u16 gid;
    u8 tipo;
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

// Efecto fijo del mapa (capa 8 del .s32): un sprite en bucle (vapor, chispas, algas) o una fuente de luz invisible ("dot range"). El
// original crea un objeto por entrada al cargar el segmento (0x460580 -> 0x45f840 -> FUN_004cc980) con la luz de luz_efecto_mapa.
// Las entradas extendidas (bit alto de n, cliente Justice nuevo) traen un i32 más que no se usa.
struct EfectoMapa {
    u16 gfx;
    u16 x, y;  // celda absoluta del juego
};

struct SegmentoMapa {
    u16 base_x = 0, base_y = 0;                    // coordenadas de la celda (0,0) del segmento
    std::vector<u32> suelo;                        // [y][x][2]
    std::vector<u16> atributos;                    // [y][x][2]
    std::vector<TileObjetoMapa> tiles_objetos;     // aplanado, en orden de archivo
    std::vector<ObjetoMapa> objetos;
    std::vector<PortalMapa> portales;
    std::vector<TechoMapa> techos;                 // sorted by (y, x); see techos_en
    std::vector<EfectoMapa> efectos;
    // Celdas con una pieza de sombra (tile translúcido; ver tiles.h), un u64 por fila. Hace falta
    // leer los .til para saberlo: lo calcula el render la primera vez que lo necesita.
    mutable std::vector<u64> sombra;

    u32 suelo_en(int x, int y, int mitad) const { return suelo[(y * CELDAS_SEGMENTO + x) * 2 + mitad]; }
    u16 atributo_en(int x, int y, int mitad) const { return atributos[(y * CELDAS_SEGMENTO + x) * 2 + mitad]; }
};

// Tipo de zona de una celda, según el atributo de su mitad IZQUIERDA (verificado contra los mapas del servidor en
// el mapa 4: tile del servidor & 0x30 == 0x10 <-> bit 2 del atributo, == 0x20 <-> bit 3; con ambos bits el
// servidor siempre dice segura).
enum class TipoZona : u8 { NORMAL, SEGURA, COMBATE };
constexpr TipoZona tipo_zona_de_atributo(u16 atributo_izq) {
    if (atributo_izq & 4) return TipoZona::SEGURA;
    if (atributo_izq & 8) return TipoZona::COMBATE;
    return TipoZona::NORMAL;
}
// Etiqueta que el original muestra abajo a la derecha de la pantalla ("Normal zone" / "Safety zone" / "Combat zone").
constexpr const char* rotulo_zona(TipoZona t) {
    return t == TipoZona::SEGURA ? "Safety zone" : t == TipoZona::COMBATE ? "Combat zone" : "Normal zone";
}

// Registros de techo registrados en la celda absoluta (x, y) del segmento (vacío si está fuera).
std::span<const TechoMapa> techos_en(const SegmentoMapa& s, int x, int y);

// Nombre del archivo del segmento que contiene la celda (x, y).
std::string nombre_segmento(MapId mapa, int x, int y);
bool decodificar_s32(Bytes datos, int base_x, int base_y, SegmentoMapa& salida);
// El formato viejo: lo mismo con campos más chicos. Algunos segmentos solo están así (43 del mapa 4).
bool decodificar_seg(Bytes datos, int base_x, int base_y, SegmentoMapa& salida);

// Formato propio liviano de los mapas generados (ver generador_mapas/): piso uniforme + una pieza
// de pared por celda bloqueada + grilla de 64×64. Cabecera "MG01", u16 base_x, u16 base_y,
// u32 piso_izq, u32 piso_der, u8 pared_capa, u32 pared_tile, y 64×64 bytes (0=piso, 1=pared).
bool decodificar_mg(Bytes datos, int base_x, int base_y, SegmentoMapa& salida);
// Elige el decodificador por la extensión de la ruta (.mg -> generado, .seg -> viejo; si no, .s32).
bool decodificar_segmento(std::string_view ruta, Bytes datos, int base_x, int base_y, SegmentoMapa& salida);

}  // namespace lin
