#include <doctest/doctest.h>
#include "mundo/luz.h"
#include "mundo/mundo.h"

using namespace lin;

// Minutos del día de una hora del reloj (para leer los casos como en el juego).
static int hm(int h, int m = 0) { return h * 60 + m; }

TEST_CASE("Luz: al aire libre, día pleno a mediodía y noche a medianoche") {
    CHECK(nivel_ambiente(0, hm(12), 1, false) == 15);
    CHECK(nivel_ambiente(0, hm(0), 1, false) == 7);
    CHECK(nivel_ambiente(0, hm(0), 172, false) == 7);
    CHECK(nivel_ambiente(4, hm(12), 364, false) == 15);
}

TEST_CASE("Luz: enero amanece más tarde que junio") {
    // A las 5:00 es de noche en enero y ya hay luz (pleno) en junio.
    CHECK(nivel_ambiente(0, hm(5), 1, false) < 15);
    CHECK(nivel_ambiente(0, hm(5), 172, false) == 15);
}

TEST_CASE("Luz: la transición del amanecer sube de forma monótona") {
    int previo = -1;
    for (int m = hm(6, 30); m <= hm(8, 0); m += 5) {
        int n = nivel_ambiente(0, m, 1, false);
        CHECK(n >= previo);
        previo = n;
    }
    CHECK(previo == 15);
}

TEST_CASE("Luz: lluvia baja un punto en la transición y no de día pleno") {
    // En medio de la transición el nivel es 14 y con lluvia baja a 13.
    CHECK(nivel_ambiente(0, hm(4, 10), 172, false) == 14);
    CHECK(nivel_ambiente(0, hm(4, 10), 172, true) == 13);
    CHECK(nivel_ambiente(0, hm(12), 1, true) == 15);
}

TEST_CASE("Luz: 1 de enero a las 7:20 queda en 13") {
    CHECK(nivel_ambiente(0, hm(7, 20), 1, false) == 13);
}

TEST_CASE("Luz: mazmorras cerradas siguen su nivel sin importar la hora") {
    CHECK(nivel_ambiente(1, hm(12), 1, false) == 1);  // lista cerrada, valor general (D2)
    CHECK(nivel_ambiente(1, hm(0), 1, false) == 1);
    CHECK(nivel_ambiente(0x4000, hm(12), 1, false) == 1);  // casa
    CHECK(nivel_ambiente(75, hm(12), 1, false) == 12);
    CHECK(nivel_ambiente(430, hm(12), 1, false) == 14);
    CHECK(nivel_ambiente(72, hm(12), 1, false) == 5);
    CHECK(nivel_ambiente(5143, hm(0), 1, false) == 15);
}

TEST_CASE("Luz: mapas cerrados y aire libre") {
    CHECK(mapa_oscuro(1));
    CHECK(mapa_oscuro(3));
    CHECK(mapa_oscuro(0x4000));
    CHECK_FALSE(mapa_oscuro(0));
    CHECK_FALSE(mapa_oscuro(4));
}

TEST_CASE("Luz: el hechizo LIGHT suma 5 con tope 15") {
    CHECK(nivel_con_luz_hechizo(7) == 12);
    CHECK(nivel_con_luz_hechizo(12) == 15);
    CHECK(nivel_con_luz_hechizo(15) == 15);
}

TEST_CASE("Luz: infravisión de elfo y elfo oscuro según las condiciones") {
    CHECK(modo_vision(2, true, 7, false, false) == Vision::ELFO);
    CHECK(modo_vision(4, true, 7, false, false) == Vision::ELFO_OSCURO);
    CHECK(modo_vision(1, true, 7, false, false) == Vision::NINGUNA);  // otra clase
    CHECK(modo_vision(2, false, 7, false, false) == Vision::NINGUNA);  // opción apagada
    CHECK(modo_vision(2, true, 15, false, false) == Vision::NINGUNA);  // de día
    CHECK(modo_vision(2, true, 7, true, false) == Vision::NINGUNA);    // bajo el agua
    CHECK(modo_vision(4, true, 7, false, true) == Vision::NINGUNA);    // ceguera
}

TEST_CASE("Mundo: S_MapID guarda el modo submarino") {
    Mundo m;
    Evento e{};
    e.tipo = Ev::MAPA;
    e.v[0] = 63;
    e.v[1] = 1;
    Evento ev[] = {e};
    aplicar_eventos(m, ev, 1000);
    CHECK(m.mapa == 63);
    CHECK(m.submarino);
    e.v[0] = 0;
    e.v[1] = 0;
    Evento otro[] = {e};
    aplicar_eventos(m, otro, 1000);
    CHECK_FALSE(m.submarino);
}
