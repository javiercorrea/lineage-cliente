// Entregable 10: music.def y envsound.def (formato de muestra y los archivos reales).
#include <doctest/doctest.h>
#include "comun.h"
#include "recursos/pak.h"
#include "recursos/sonidos.h"

using namespace lin;

TEST_CASE("Sonidos: zonas de música y ambiente de muestra") {
    const char* musica =
        "System Setting Begin\n\tLogin=0;\n\tDeath=120;\nSystemMenu Exit\n"
        ";PUEBLO\nLOC 4 100 100 200 200 \\ 4 300 300 310 310\nMUSIC 83\nINTERVAL 180\n\n"
        ";EL RESTO\nLOC 4\nMUSIC 18\nINTERVAL\n\n"
        ";MAZMORRAS\nLOC 1 \\ 2\n   \\ 7\nMUSIC 63\n";
    const char* ambiente =
        "[BIRD_SOUND:80]\n 2100 2101\n[/BIRD_SOUND]\n[OWL:30]\n2200\n[/OWL]\n"
        "        <LOC>4 \\ 69</LOC>\n        <TIME>DAY</TIME>\n\t<MAIN_SOUND>2000</MAIN_SOUND>\n"
        "\t<ONE_SOUND>BIRD_SOUND</ONE_SOUND>\n        <TIME>NIGHT</TIME>\n\t<MAIN_SOUND>2001</MAIN_SOUND>\n"
        "        <ONE_SOUND>OWL BIRD_SOUND</ONE_SOUND>\n";
    DefSonidos d;
    d.cargar(musica, ambiente);
    CHECK(d.musica_login == 0);
    CHECK(d.musica_muerte == 120);
    REQUIRE(d.musica.size() == 3);
    REQUIRE(d.musica_en(4, 150, 150));
    CHECK(d.musica_en(4, 150, 150)->musica == 83);
    CHECK(d.musica_en(4, 150, 150)->intervalo_s == 180);
    CHECK(d.musica_en(4, 305, 305)->musica == 83);
    CHECK(d.musica_en(4, 250, 250)->musica == 18);  // fuera del pueblo: el mapa entero
    CHECK(d.musica_en(4, 250, 250)->intervalo_s == 0);
    CHECK(d.musica_en(7, 1, 1)->musica == 63);       // "\" al principio de la línea siguiente
    CHECK(d.musica_en(5, 1, 1) == nullptr);

    REQUIRE(d.grupos.size() == 2);
    CHECK(d.grupos[0].nombre == "BIRD_SOUND");
    CHECK(d.grupos[0].intervalo_s == 80);
    CHECK(d.grupos[0].cantidad == 2);
    const AmbienteZona* dia = d.ambiente_en(69, 0, 0, false);
    const AmbienteZona* noche = d.ambiente_en(69, 0, 0, true);
    REQUIRE(dia);
    REQUIRE(noche);
    CHECK(dia->principal == 2000);
    CHECK(dia->n_grupos == 1);
    CHECK(noche->principal == 2001);
    REQUIRE(noche->n_grupos == 2);
    CHECK(d.grupos[d.grupos_ambiente[noche->primer_grupo]].nombre == "OWL");
    CHECK(d.ambiente_en(3, 0, 0, false) == nullptr);
}

TEST_CASE("Sonidos: music.def y envsound.def de la instalación") {
    const std::string dir = carpeta_juego();
    if (dir.empty()) return;
    Recursos r;
    REQUIRE(r.abrir(dir));
    std::vector<u8> m, e;
    REQUIRE(r.texto("music.def", m));
    REQUIRE(r.texto("envsound.def", e));
    DefSonidos d;
    d.cargar({reinterpret_cast<const char*>(m.data()), m.size()}, {reinterpret_cast<const char*>(e.data()), e.size()});
    CHECK(d.musica_login == 0);
    CHECK(d.musica.size() > 100);
    // Pueblo de Gludio antes que el campo de Gludio (mismo mapa 4).
    CHECK(d.musica_en(4, 32600, 32780)->musica == 83);
    CHECK(d.musica_en(4, 32700, 32900)->musica == 18);
    CHECK(d.musica_en(0, 32580, 32940)->musica == 30);  // pueblo de Talking Island
    CHECK(d.musica_en(69, 32691, 32864)->musica == 53); // pueblo del valle escondido
    CHECK(d.grupos.size() > 30);
    const AmbienteZona* z = d.ambiente_en(0, 32580, 32940, false);
    REQUIRE(z);
    CHECK(z->principal == 2000);
    CHECK(z->n_grupos == 3);
}
