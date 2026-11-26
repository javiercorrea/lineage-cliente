// Clases de personaje como datos: las 7 del juego y las que agregue una extensión de servidor
// (datos/extensiones/<nombre>.ini, elegida con "protocolo+extensión" en la línea servidor=, p. ej.
// "tw380+reborn"). Con un servidor estándar la tabla es la de siempre (clases_estandar).
#pragma once
#include <string_view>
#include "core/tipos.h"

namespace lin {

// Animación de la selección y la creación (imágenes N.png): primer cuadro, cantidad, quieto (ranura sin
// elegir) y resaltado (mouse encima).
struct AnimClase {
    u16 primero = 0, cantidad = 0, quieto = 0, resaltado = 0;
};

struct Clase {
    u8 base[6] = {};  // fue, des, con, sab, car, int (orden de C_CreateChar)
    u8 tope[6] = {};  // máximo absoluto por stat al crear (maxFixedStats del servidor)
    u8 bonus = 0;     // puntos libres al crear: la suma de base + bonus es 75
    u32 boton = 0;    // img del botón en la creación original (boton + 1 = marcado); 0 = sin botón
    i16 x = 0, y = 0;   // posición de ese botón
    AnimClase anim[2];  // hombre, mujer
};

constexpr int CLASES_MAX = 8;  // las 7 del juego (0 príncipe .. 6 ilusionista) y el guerrero (7)

struct TablaClases {
    Clase c[CLASES_MAX];
    int n = 0;  // clases disponibles: 0..n-1

    // La clase pedida, o el príncipe si no existe (como el servidor, que no manda clases que no conoce).
    const Clase& de(int clase) const { return c[clase >= 0 && clase < n ? clase : 0]; }
};

TablaClases clases_estandar();

// "tw380+reborn" -> devuelve "tw380" y deja "reborn" en `extension` ("" si no hay '+').
std::string_view separar_extension(std::string_view protocolo, std::string_view& extension);

// Agrega o reemplaza clases desde el texto de un .ini de extensión:
//   [clase 7]
//   base=16,13,16,7,9,10    ; fue des con sab car int
//   bonus=4
//   tope=20,17,20,11,13,14  ; opcional: por omisión base + bonus
//   boton=2257
//   posicion=542,166
//   hombre=1132,87,1132,1134 ; primer cuadro, cantidad, quieto, resaltado
//   mujer=1219,84,1219,1221
// Devuelve false si el texto está mal formado (la tabla puede quedar a medias: descartarla).
bool cargar_clases_ini(std::string_view texto, TablaClases& t);

}  // namespace lin
