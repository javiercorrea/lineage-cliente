// Búsqueda de ruta para el modo auto (PLAN_MODO_AUTO.md, fase 2): A* en 8 direcciones, todas de costo 1
// (en L1 un paso en diagonal tarda lo mismo). Sin estado de la app, para los tests: quien la usa dice si
// se puede dar cada paso (el servidor valida por los bordes de la celda y la dirección, no solo si la
// celda se puede pisar; ver L1V1Map.isPassable).
#pragma once
#include <functional>
#include <utility>
#include <vector>
#include "core/tipos.h"

namespace lin {

// ¿Se puede dar un paso desde (x, y) en la dirección `dir` (0–7, como DIR_DX / DIR_DY)?
using PuedePasar = std::function<bool(i32 x, i32 y, int dir)>;

// Con solo "¿se puede pisar esta celda?": la celda de destino libre y, en diagonal, también las dos de al
// lado (no corta esquinas; más estricto que el servidor, que pide una de las dos).
PuedePasar paso_por_celdas(std::function<bool(i32 x, i32 y)> pisable);

// Celdas a pisar desde (x0, y0) (sin incluirla) hasta llegar a `cerca` o menos de (x1, y1) (la mayor de
// |dx| y |dy|; con 0, a la celda misma). Vacía si ya se está ahí, si no hay camino o si se exploran más de
// `max_nodos` celdas (para no trabar el frame con un destino imposible).
std::vector<std::pair<i32, i32>> buscar_ruta(i32 x0, i32 y0, i32 x1, i32 y1, const PuedePasar& puede, i32 cerca = 0,
                                             i32 max_nodos = 4000);

// Un punto al azar a entre `min` y `max` celdas de (x, y) que se pueda pisar, para "recorrer"; `semilla`
// avanza (xorshift). false si en `intentos` no encontró ninguno.
bool punto_al_azar(i32 x, i32 y, i32 min, i32 max, const std::function<bool(i32, i32)>& pisable, u32& semilla,
                   std::pair<i32, i32>& punto, int intentos = 30);

}  // namespace lin
