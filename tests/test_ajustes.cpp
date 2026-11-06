#include <doctest/doctest.h>
#include "plataforma/rutas.h"

using namespace lin;

TEST_CASE("quitar_valor_de_personaje: quita solo al personaje pedido") {
    std::string opcion = "Ana:4,0;Luis:1;Ana:9";
    quitar_valor_de_personaje(opcion, "Ana");
    CHECK(opcion == "Luis:1;");
    quitar_valor_de_personaje(opcion, "Nadie");  // no existe: no cambia nada
    CHECK(opcion == "Luis:1;");
    quitar_valor_de_personaje(opcion, "Luis");
    CHECK(opcion.empty());
}

TEST_CASE("personajes_con_datos: une atajos y barra sin repetir") {
    Config c;
    c.atajos = "Ana:4,0;Luis:1;";
    c.barra = "Ana:h4,0;Mario:i40308:268435461;";
    const std::vector<std::string> n = personajes_con_datos(c);
    REQUIRE(n.size() == 3);
    CHECK(n[0] == "Ana");
    CHECK(n[1] == "Luis");
    CHECK(n[2] == "Mario");
}

TEST_CASE("personajes_con_datos: sin datos devuelve lista vacía") {
    Config c;
    CHECK(personajes_con_datos(c).empty());
}

TEST_CASE("escribir y leer settings.ini: ida y vuelta de los campos del editor") {
    Config c = leer_config("juego=/juegos/lineage/\nui_simple=0\nzoom=3\nvolumen_musica=40\n"
                           "servidor=Local|127.0.0.1|2000|jp363|prueba\nservidor_elegido=Local\n");
    c.opacidad_ventanas = 99;
    c.logo = "logo.png";
    CHECK_FALSE(c.mostrar_fps);
    c.mostrar_fps = true;
    const Config d = leer_config(escribir_config(c));
    CHECK(d.mostrar_fps);
    CHECK_FALSE(d.ui_simple);
    CHECK(d.zoom == 3);
    CHECK(d.volumen_musica == 40);
    CHECK(d.opacidad_ventanas == 99);
    CHECK(d.logo == "logo.png");
    CHECK(d.servidores[d.servidor].usuario == "prueba");
}
