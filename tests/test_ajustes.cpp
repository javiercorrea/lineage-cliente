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

TEST_CASE("personajes_con_datos: también los que solo tienen auto-buffs") {
    Config c;
    c.barra = "Ana:h4;";
    c.autobuffs = "Ana:1|43,0,0,0,0,0;Eva:0|42,0,0,0,0,0;";
    const std::vector<std::string> n = personajes_con_datos(c);
    REQUIRE(n.size() == 2);
    CHECK(n[1] == "Eva");
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
    CHECK_FALSE(c.pantalla_completa);
    c.pantalla_completa = true;
    const Config d = leer_config(escribir_config(c));
    CHECK(d.mostrar_fps);
    CHECK(d.pantalla_completa);
    CHECK_FALSE(d.ui_simple);
    CHECK(d.zoom == 3);
    CHECK(d.volumen_musica == 40);
    CHECK(d.opacidad_ventanas == 99);
    CHECK(d.logo == "logo.png");
    CHECK(d.servidores[d.servidor].usuario == "prueba");
}

TEST_CASE("parte_de_valor: páginas de la barra separadas por /") {
    CHECK(parte_de_valor("h4,0,i80:12*", 0) == "h4,0,i80:12*");  // formato anterior: solo la primera
    CHECK(parte_de_valor("h4,0,i80:12*", 1).empty());
    CHECK(parte_de_valor("h4,0/0,h9/i1:2", 0) == "h4,0");
    CHECK(parte_de_valor("h4,0/0,h9/i1:2", 1) == "0,h9");
    CHECK(parte_de_valor("h4,0/0,h9/i1:2", 2) == "i1:2");
    CHECK(parte_de_valor("h4,0/0,h9/i1:2", 3).empty());
    CHECK(parte_de_valor("", 0).empty());
}

TEST_CASE("paginas_barra: 3 por defecto, se recorta a 1-9 y vuelve igual") {
    CHECK(leer_config("").paginas_barra == 3);
    CHECK(leer_config("paginas_barra=0\n").paginas_barra == 1);
    CHECK(leer_config("paginas_barra=40\n").paginas_barra == PAGINAS_BARRA_MAX);
    Config c = leer_config("");
    c.paginas_barra = 5;
    CHECK(leer_config(escribir_config(c)).paginas_barra == 5);
}

TEST_CASE("ms_mantener_tecla: 300 por defecto, se recorta a 150-1000 y vuelve igual") {
    CHECK(leer_config("").ms_mantener_tecla == 300);
    CHECK(leer_config("ms_mantener_tecla=10\n").ms_mantener_tecla == MS_MANTENER_MIN);
    CHECK(leer_config("ms_mantener_tecla=5000\n").ms_mantener_tecla == MS_MANTENER_MAX);
    Config c = leer_config("");
    c.ms_mantener_tecla = 700;
    CHECK(leer_config(escribir_config(c)).ms_mantener_tecla == 700);
}

TEST_CASE("autopocion: por personaje, apagada con 60 % sin entrada, umbral recortado a 10-95 de a 5") {
    Config c;
    CHECK_FALSE(autopocion_de(c, "Ana").encendida);
    CHECK(autopocion_de(c, "Ana").umbral == 60);
    poner_autopocion(c, "Ana", {true, 70});
    poner_autopocion(c, "Eva", {false, 33});  // se redondea a 30
    CHECK(autopocion_de(c, "Ana").encendida);
    CHECK(autopocion_de(c, "Ana").umbral == 70);
    CHECK(autopocion_de(c, "Eva").umbral == 30);
    c.autopocion = "Ana:1,200;Eva:1,1";
    CHECK(autopocion_de(c, "Ana").umbral == 95);
    CHECK(autopocion_de(c, "Eva").umbral == 10);
    Config d = leer_config("autopocion=Ana:1,45\n");
    CHECK(autopocion_de(leer_config(escribir_config(d)), "Ana").umbral == 45);
    CHECK(personajes_con_datos(d).size() == 1);
}
