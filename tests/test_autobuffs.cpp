// Auto-buffs: elegir qué buff relanzar (mundo/autobuffs.h).
#include <doctest/doctest.h>
#include "mundo/autobuffs.h"

using namespace lin;

namespace {
CasilleroAuto casillero(u32 hechizo, i32 mp = 10, u64 duracion_ms = 60000) {
    CasilleroAuto c;
    c.hechizo = hechizo;
    c.mp = mp;
    c.duracion_ms = duracion_ms;
    return c;
}

EstadoJugador jugador(i32 mp = 100) {
    EstadoJugador j;
    j.mp = mp;
    return j;
}

void poner(EstadoJugador& j, u8 clase, u8 tipo, u64 fin) { j.efectos[j.n_efectos++] = {clase, tipo, fin}; }
}  // namespace

TEST_CASE("elegir_autobuff: el primero sin efecto, de izquierda a derecha") {
    EstadoJugador j = jugador();
    CasilleroAuto c[CASILLEROS_AUTO] = {casillero(43), casillero(42)};  // Haste, Enchant Mighty
    CHECK(elegir_autobuff(j, c, CASILLEROS_AUTO, 100000) == 0);
    poner(j, EA_HASTE, 1, 400000);  // Haste activo
    CHECK(elegir_autobuff(j, c, CASILLEROS_AUTO, 100000) == 1);
    poner(j, EA_FUERZA, 5, 400000);  // Enchant Mighty activo
    CHECK(elegir_autobuff(j, c, CASILLEROS_AUTO, 100000) == -1);
}

TEST_CASE("elegir_autobuff: se renueva con menos de MARGEN_AUTO_MS") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(43)};
    poner(j, EA_HASTE, 1, 100000 + MARGEN_AUTO_MS);
    CHECK(elegir_autobuff(j, c, 1, 100000) == -1);
    CHECK(elegir_autobuff(j, c, 1, 100001) == 0);
}

TEST_CASE("elegir_autobuff: efecto que no se ve, no antes de su duración") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(14, 10, 60000)};
    c[0].lanzado_ms = 100000;
    CHECK(elegir_autobuff(j, c, 1, 100000 + 59999) == -1);
    CHECK(elegir_autobuff(j, c, 1, 100000 + 60000) == 0);
}

TEST_CASE("elegir_autobuff: un efecto que se vio y terminó antes de tiempo se relanza") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(43, 10, 300000)};  // Haste, 300 s
    c[0].lanzado_ms = 100000;
    c[0].efecto_visto = true;  // tuvo ícono; ya no está (murió, se lo quitaron)
    CHECK(elegir_autobuff(j, c, 1, 160000) == 0);
}

TEST_CASE("elegir_autobuff: espera la respuesta del lanzado anterior aunque el efecto esté por vencer") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(43)};
    poner(j, EA_HASTE, 1, 101000);  // le queda 1 s
    c[0].lanzado_ms = 99000;
    CHECK(elegir_autobuff(j, c, 1, 100000) == -1);
    c[0].lanzado_ms = 100000 - ESPERA_RESPUESTA_MS;
    CHECK(elegir_autobuff(j, c, 1, 100000) == 0);
}

TEST_CASE("elegir_autobuff: MP, fallo, muerto e inmóvil") {
    EstadoJugador j = jugador(5);
    CasilleroAuto c[2] = {casillero(43, 10), casillero(42, 5)};
    CHECK(elegir_autobuff(j, c, 2, 100000) == 1);  // el primero no alcanza el MP: el siguiente
    c[1].reintentar_ms = 100000 + ESPERA_FALLO_MS;
    CHECK(elegir_autobuff(j, c, 2, 100000) == -1);
    CHECK(elegir_autobuff(j, c, 2, 100000 + ESPERA_FALLO_MS) == 1);
    j.inmovil = 1;
    CHECK(elegir_autobuff(j, c, 2, 200000) == -1);
    j.inmovil = 0;
    j.muerto = true;
    CHECK(elegir_autobuff(j, c, 2, 200000) == -1);
}

TEST_CASE("acepta_autobuff: solo buffs propios") {
    Hechizo h;
    h.nombre = "Haste";
    h.duracion_s = 300;
    CHECK(acepta_autobuff(h));
    h.de_ataque = true;
    CHECK_FALSE(acepta_autobuff(h));
    h.de_ataque = false;
    h.duracion_s = 0;  // una curación
    CHECK_FALSE(acepta_autobuff(h));
    h.duracion_s = 300;
    h.sobre_item = Hechizo::SOBRE_ARMA;  // Enchant Weapon: pide elegir el arma
    CHECK_FALSE(acepta_autobuff(h));
}
