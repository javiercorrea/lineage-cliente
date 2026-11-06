// Luz y noche (PLAN_LUZ_AGUA.md, F1): reglas del cliente original, sin GPU. Son funciones puras;
// el dibujo (mapa de luz, filtros de infravisión) se apoya en ellas en fases posteriores.
#pragma once
#include "core/tipos.h"

namespace lin {

// Brillo de una celda del mundo, 0..31, para el nivel ambiente y la máscara de luz (0..255).
// Es la tabla LUT del original (0x6e3638), que se aproxima con esta fórmula (error máximo 1).
int brillo_lut(int nivel, int mascara);

// Tinte fijo de algunos mapas (el original lo multiplica sobre todo el mundo). False si el mapa no tiene.
bool tinte_mapa(MapId m, u8& r, u8& g, u8& b);

// Desplazamiento vertical de la ola bajo el agua, en píxeles (de -6 a 6) para la columna x y la fase
// (0..99, avanza cada 50 ms). El shader hace lo mismo; esta versión es la de referencia para los tests.
int desplazamiento_ola(int x, int fase);

// Vaivén horizontal del mundo al estar borracho (S_Liquor 1), en píxeles (de -3 a 3) para la fila y la
// fase (0..99). Igual que el shader (fs_mundo, gpu.cpp).
int desplazamiento_borracho(int fila, int fase);

// Visión especial de la clase del jugador (campo de clase de S_CharPack: 2 elfo, 4 elfo oscuro).
enum class Vision : u8 { NINGUNA, ELFO, ELFO_OSCURO };

// Mapas cerrados (mazmorras, casas): el original los deja oscuros a cualquier hora. Lista del
// anexo A del plan (0x6ddae8) más todo mapa >= 0x4000 (casas).
bool mapa_oscuro(MapId m);

// Nivel de luz ambiente, 0..15 (15 = día sin oscuridad). `minuto` es la hora de juego en minutos
// del día (0..1439) y `dia_anio` (1..366) fija amanecer y atardecer. `lluvia` es el clima de
// lluvia o nieve. Mapas con nivel fijo (ver luz.cpp) ignoran la hora.
int nivel_ambiente(MapId m, int minuto, int dia_anio, bool lluvia);

// Radio de la máscara de un objeto a partir de su luz: el jugador nunca baja de 5 (S_OwnCharPack manda 0)
// y el tope es 14, la última máscara de source.lmp con datos.
int radio_de_luz(bool es_jugador, int luz);

// Hechizo LIGHT: suma 5 al nivel, con tope 15.
int nivel_con_luz_hechizo(int nivel);

// Infravisión: el elfo y el elfo oscuro ven en la oscuridad con un filtro propio en lugar de la
// luz normal. No aplica con agua (submarino), con ceguera, con el nivel en día ni si la opción
// está apagada.
Vision modo_vision(u8 clase, bool opcion_activa, int nivel, bool submarino, bool ceguera);

}  // namespace lin
