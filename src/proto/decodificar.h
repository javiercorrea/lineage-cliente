// Decodificador: cuerpo de paquete (descifrado) -> Evento. Tabla de punteros a función indexada
// por id interno; sin allocations (los textos se copian a la arena del frame).
#pragma once
#include "proto/eventos.h"

namespace lin {

// Agrega los eventos del paquete a `salida` (0 o 1; S_InvList, uno por ítem más uno que vacía el
// inventario). Devuelve false si el paquete estaba truncado.
bool decodificar(Bytes cuerpo, const TablaOpcodes& tabla, Arena& arena, std::vector<Evento>& salida);

}  // namespace lin
