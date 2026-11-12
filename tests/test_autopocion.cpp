// Auto-poción: elegir la poción de curación (mundo/autopocion.h).
#include <doctest/doctest.h>
#include <string>
#include "mundo/autopocion.h"

using namespace lin;

namespace {
// Agrega un ítem con ese nombre (como lo manda el servidor) y cantidad.
void agregar(Inventario& inv, ObjId id, std::string_view nombre, u32 cantidad = 10) {
    const Slot s = inv.items.insertar(id);
    inv.items.col<I_CANTIDAD>()[s] = cantidad;
    inv.items.col<I_NOMBRE>()[s] = {static_cast<u32>(inv.textos.size()), static_cast<u16>(nombre.size())};
    inv.textos.insert(inv.textos.end(), nombre.begin(), nombre.end());
}
}  // namespace

TEST_CASE("numero_de_nombre: el $N del principio") {
    CHECK(numero_de_nombre("$1251 (12)") == 1251);
    CHECK(numero_de_nombre("$26") == 26);
    CHECK(numero_de_nombre("Red Potion") == 0);
    CHECK(numero_de_nombre("$") == 0);
}

TEST_CASE("elegir_pocion: familias en orden condensada, antigua, normal") {
    Inventario inv;
    agregar(inv, 1, "$26 (10)");    // normal roja
    agregar(inv, 2, "$2575 (10)");  // antigua roja
    CHECK(elegir_pocion(inv, 90, 100).id == 2);
    CHECK(elegir_pocion(inv, 90, 100).demora_ms == 500);
    agregar(inv, 3, "$1251 (10)");  // condensada roja
    CHECK(elegir_pocion(inv, 90, 100).id == 3);
    CHECK(elegir_pocion(inv, 90, 100).demora_ms == 1000);
}

TEST_CASE("elegir_pocion: el color según lo que falta") {
    Inventario inv;
    agregar(inv, 1, "$1251");  // roja (media 35)
    agregar(inv, 2, "$1252");  // naranja (60)
    agregar(inv, 3, "$1253");  // transparente (90)
    CHECK(elegir_pocion(inv, 41, 100).id == 1);   // falta 59
    CHECK(elegir_pocion(inv, 40, 100).id == 2);   // falta 60
    CHECK(elegir_pocion(inv, 11, 100).id == 2);   // falta 89
    CHECK(elegir_pocion(inv, 10, 100).id == 3);   // falta 90
}

TEST_CASE("elegir_pocion: las medias de la antigua son otras") {
    Inventario inv;
    agregar(inv, 1, "$2575");
    agregar(inv, 2, "$2576");
    agregar(inv, 3, "$2577");
    CHECK(elegir_pocion(inv, 69, 100).id == 1);  // falta 31
    CHECK(elegir_pocion(inv, 68, 100).id == 2);  // falta 32
    CHECK(elegir_pocion(inv, 43, 100).id == 3);  // falta 57
}

TEST_CASE("elegir_pocion: sin el color, primero uno menor y después uno mayor") {
    Inventario inv;
    agregar(inv, 1, "$1251");  // solo roja y transparente
    agregar(inv, 3, "$1253");
    CHECK(elegir_pocion(inv, 40, 100).id == 1);  // falta 60 (naranja): la roja, no la transparente
    Inventario solo_grande;
    agregar(solo_grande, 3, "$1253");
    CHECK(elegir_pocion(solo_grande, 90, 100).id == 3);  // falta 10 (roja): la única que hay
}

TEST_CASE("elegir_pocion: sin pociones de curación, ninguna") {
    Inventario inv;
    agregar(inv, 1, "$1251", 0);  // sin cantidad
    agregar(inv, 2, "Arrow (100)");
    CHECK(elegir_pocion(inv, 10, 100).id == 0);
}
