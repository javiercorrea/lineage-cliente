// Auto-poción (PLAN_JUGABILIDAD.md, punto 5): qué poción de curación tomar. Sin estado de la app,
// para los tests; el cuándo y la interfaz están en app/autopocion_ui.cpp.
#pragma once
#include <string_view>
#include "mundo/inventario.h"

namespace lin {

struct PocionElegida {
    ObjId id = 0;        // 0 = ninguna
    u32 demora_ms = 0;   // delay_time de la poción: antes no se puede tomar otra
};

// El $N con que empieza el nombre que manda el servidor ("$1251 (12)" -> 1251); 0 si no empieza así.
u32 numero_de_nombre(std::string_view nombre);
// La poción de curación a tomar con ese HP: la familia (condensada, antigua, normal, en ese orden)
// y dentro de ella el color según lo que falta (transparente si falta al menos su curación media,
// naranja si falta al menos la suya, si no roja). Sin ese color, el más cercano de la familia
// (primero uno menor, para no desperdiciar); sin ninguno, la familia siguiente.
PocionElegida elegir_pocion(const Inventario& inv, i32 hp, i32 hp_max);

}  // namespace lin
