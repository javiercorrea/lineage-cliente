// Contador de allocations del hilo actual (operator new instrumentado en asignaciones.cpp).
// Sirve para verificar "cero allocations en régimen" (PLAN_CLIENTE.md, principio 5): el bucle
// principal mide cuántas hace cada frame. Solo cuenta si asignaciones.cpp está enlazado (el
// ejecutable y los tests); si no, queda en 0.
#pragma once
#include "core/tipos.h"

namespace lin {

u64 asignaciones_hilo();

}  // namespace lin
