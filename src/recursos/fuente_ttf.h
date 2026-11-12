// Fuente TTF de la interfaz (Departure Mono, Latin-1) rasterizada con stb_truetype. Los textos de la
// interfaz están en UTF-8 y se dibujan con ella; el texto del servidor (CP949) sigue en la fuente
// bitmap del juego (recursos/fuente.h). Los glifos son celdas de ALTO_GLIFO píxeles de alto con la
// línea base en la misma fila, así que el dibujo común (Graficos::texto) no cambia.
#pragma once
#include "core/tipos.h"
#include "recursos/imagen.h"

namespace lin {

// Glifos TTF en el espacio de glifos del atlas: GLIFO_TTF_BASE + punto de código Unicode.
constexpr u32 GLIFO_TTF_BASE = 0x100000;

// Avance del carácter en píxeles (proporcional). Un carácter que la fuente no tiene se dibuja como '?'.
int ancho_ttf(u32 codepunto);
// Celda RGBA premultiplicada (blanca con alfa), del ancho del avance y ALTO_GLIFO de alto.
bool rasterizar_ttf(u32 codepunto, ImagenRGBA& salida);

}  // namespace lin
