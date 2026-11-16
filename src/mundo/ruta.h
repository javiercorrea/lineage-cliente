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

// Sigue una ruta hacia el último destino fijado. Quien decide fija el destino en cada frame
// (`fijar_destino`); la ruta no se recalcula en cada frame sino al azar cada RECALCULO_MIN_MS a
// RECALCULO_MAX_MS (hacia el último destino fijado: así sigue a un destino que se mueve sin buscar todo el
// tiempo ni moverse siempre igual), o antes si no hay ruta, si se salió de ella o si un paso se bloqueó.
// Sin camino, `sin_camino` y no vuelve a buscar hasta el próximo recálculo.
constexpr u64 RECALCULO_MIN_MS = 400, RECALCULO_MAX_MS = 1600;
struct Enrutador {
    bool hay_destino = false;
    i32 dx = 0, dy = 0, cerca = 0;
    std::vector<std::pair<i32, i32>> ruta;
    size_t i = 0;  // la próxima celda de `ruta`
    u64 recalcular_ms = 0;
    u32 semilla = 0x2545F491u;
    bool sin_camino = false;
};
void fijar_destino(Enrutador& r, i32 x, i32 y, i32 cerca = 0);
void soltar_destino(Enrutador& r);
// La dirección del próximo paso desde (x, y), o -1 (sin destino, ya se llegó, sin camino o bloqueado).
// Se puede llamar en cada frame: solo avanza cuando (x, y) ya es la celda que seguía.
int siguiente_paso(Enrutador& r, i32 x, i32 y, const PuedePasar& puede, u64 ahora);

// Un punto al azar a entre `min` y `max` celdas de (x, y) que se pueda pisar, para "recorrer"; `semilla`
// avanza (xorshift). false si en `intentos` no encontró ninguno.
bool punto_al_azar(i32 x, i32 y, i32 min, i32 max, const std::function<bool(i32, i32)>& pisable, u32& semilla,
                   std::pair<i32, i32>& punto, int intentos = 30);

}  // namespace lin
