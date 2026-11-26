#include <doctest/doctest.h>
#include "mundo/clases.h"

using namespace lin;

TEST_CASE("clases_estandar: las 7 clases del juego") {
    const TablaClases t = clases_estandar();
    REQUIRE(t.n == 7);
    for (int c = 0; c < 7; ++c) {
        int suma = t.c[c].bonus;
        for (int i = 0; i < 6; ++i) suma += t.c[c].base[i];
        CHECK(suma == 75);
    }
    CHECK(t.c[0].anim[0].primero == 714);
    CHECK(t.c[6].boton == 1751);
    CHECK(&t.de(7) == &t.c[0]);  // clase desconocida: no se sale de la tabla
}

TEST_CASE("separar_extension") {
    std::string_view ext;
    CHECK(separar_extension("tw380+reborn", ext) == "tw380");
    CHECK(ext == "reborn");
    CHECK(separar_extension("jp363", ext) == "jp363");
    CHECK(ext.empty());
}

TEST_CASE("cargar_clases_ini: el guerrero de reborn") {
    TablaClases t = clases_estandar();
    const char* ini =
        "; comentario\n"
        "[clase 7]\r\n"
        "base=16,13,16,7,9,10\n"
        "bonus = 4\n"
        "boton=2257  ; ícono\n"
        "posicion=542,166\n"
        "hombre=1135,84,1132,1134\n"
        "mujer=1220,83,1133,1219\n";
    REQUIRE(cargar_clases_ini(ini, t));
    REQUIRE(t.n == 8);
    const Clase& g = t.c[7];
    CHECK(g.base[0] == 16);
    CHECK(g.base[5] == 10);
    CHECK(g.bonus == 4);
    CHECK(g.tope[0] == 20);  // sin tope=: base + bonus
    CHECK(g.tope[3] == 11);
    CHECK(g.boton == 2257);
    CHECK(g.x == 542);
    CHECK(g.y == 166);
    CHECK(g.anim[1].primero == 1220);
    CHECK(g.anim[1].cantidad == 83);
    CHECK(t.c[1].bonus == 4);  // las demás no cambian
}

TEST_CASE("cargar_clases_ini: rechaza lo mal formado") {
    TablaClases t = clases_estandar();
    CHECK_FALSE(cargar_clases_ini("base=1,2,3,4,5,6\n", t));        // clave fuera de una sección
    CHECK_FALSE(cargar_clases_ini("[clase 9]\n", t));               // hueco: falta la 7 y la 8
    CHECK_FALSE(cargar_clases_ini("[clase 7]\nbase=1,2,3\n", t));   // faltan números
    CHECK_FALSE(cargar_clases_ini("[clase 7]\nfuerza=20\n", t));    // clave desconocida
    CHECK_FALSE(cargar_clases_ini("[clase 7]\nbonus=4x\n", t));     // basura después del número
}
