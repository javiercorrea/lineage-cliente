// Empaquetado del atlas y reemplazo LRU de páginas (render/atlas.h), sin GPU.
#include <doctest/doctest.h>
#include "render/atlas.h"

using namespace lin;

TEST_CASE("Atlas: estantes dentro de una página") {
    EmpaquetadoAtlas a(64, 3);
    RegionAtlas r1, r2, r3;
    REQUIRE(a.reservar(20, 20, 1, r1));
    REQUIRE(a.reservar(20, 20, 1, r2));
    CHECK(r1.pagina == 0);
    CHECK(r2.pagina == 0);
    CHECK(r2.x == 21);  // a la derecha, con 1 px de margen
    CHECK(r2.y == r1.y);
    REQUIRE(a.reservar(20, 20, 1, r3));  // 63 px: entra justo
    CHECK(r3.y == 0);
    CHECK_FALSE(a.reservar(64, 10, 1, r3));  // más ancha que la página (con el margen)
}

TEST_CASE("Atlas: cabe dice si una imagen entra alguna vez (con su margen)") {
    EmpaquetadoAtlas a(2048, 2);
    CHECK(a.cabe(2047, 2047));
    CHECK_FALSE(a.cabe(2048, 10));  // del tamaño de la página: con el margen no entra nunca
    CHECK_FALSE(a.cabe(10, 70000));  // más que u16: no se trunca
    RegionAtlas r;
    CHECK(a.reservar(2047, 2047, 1, r));
    CHECK_FALSE(a.reservar(2048, 1, 1, r));
}

TEST_CASE("Atlas: con las páginas llenas se reemplaza la usada hace más tiempo") {
    EmpaquetadoAtlas a(32, 3);  // una imagen de 31×31 llena una página
    RegionAtlas r;
    REQUIRE(a.reservar(31, 31, 1, r));
    CHECK(r.pagina == 0);
    REQUIRE(a.reservar(31, 31, 2, r));
    CHECK(r.pagina == 1);
    REQUIRE(a.reservar(31, 31, 3, r));
    CHECK(r.pagina == 2);
    CHECK(a.tomar_reemplazada() == -1);
    CHECK(a.paginas_usadas() == 3);

    // Frame 10: la página 1 se dibujó en el 5 y la 2 en el 3. Se reemplaza la 2.
    a.usar(1, 5);
    REQUIRE(a.reservar(31, 31, 10, r));
    CHECK(r.pagina == 2);
    CHECK(a.tomar_reemplazada() == 2);
    CHECK(a.tomar_reemplazada() == -1);  // se informa una sola vez

    // La 0 no se reemplaza aunque sea la más vieja: ahora le toca a la 1.
    REQUIRE(a.reservar(31, 31, 11, r));
    CHECK(r.pagina == 1);
    CHECK(a.tomar_reemplazada() == 1);
    CHECK(a.reemplazos() == 2);
}

TEST_CASE("Atlas: nunca se reemplaza una página dibujada en el frame actual") {
    EmpaquetadoAtlas a(32, 2);
    RegionAtlas r;
    REQUIRE(a.reservar(31, 31, 1, r));
    REQUIRE(a.reservar(31, 31, 1, r));
    a.usar(1, 7);
    CHECK_FALSE(a.reservar(31, 31, 7, r));  // la 1 está en la lista de este frame y la 0 es fija
    CHECK(a.tomar_reemplazada() == -1);
    REQUIRE(a.reservar(31, 31, 8, r));  // el frame siguiente sí
    CHECK(r.pagina == 1);
}
