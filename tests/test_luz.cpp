#include <doctest/doctest.h>
#include "comun.h"
#include "mundo/luz.h"
#include "mundo/mundo.h"
#include "recursos/imagen.h"
#include "recursos/pak.h"

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
    CHECK(nivel_ambiente(1, hm(12), 1, false) == 3);  // lista cerrada, valor general (D2: el del binario)
    CHECK(nivel_ambiente(1, hm(0), 1, false) == 3);
    CHECK(nivel_ambiente(0x4000, hm(12), 1, false) == 3);  // casa
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

TEST_CASE("Luz: la ola se mueve entre -6 y 6 px y repite cada 100 columnas") {
    for (int fase = 0; fase < 100; fase += 7)
        for (int x = 0; x < 300; ++x) {
            int d = desplazamiento_ola(x, fase);
            CHECK(d >= -6);
            CHECK(d <= 6);
            CHECK(d == desplazamiento_ola(x + 100, fase));
        }
    CHECK(desplazamiento_ola(0, 0) == 0);
    CHECK(desplazamiento_ola(25, 0) >= 5);  // pico: sin(pi/2) * AMPLITUD_OLA (6)
}

TEST_CASE("Luz: el vaivén de la borrachera va de -3 a 3 px y repite cada 100 filas") {
    for (int fase = 0; fase < 100; fase += 9)
        for (int f = 0; f < 300; ++f) {
            int d = desplazamiento_borracho(f, fase);
            CHECK(d >= -3);
            CHECK(d <= 3);
            CHECK(d == desplazamiento_borracho(f + 100, fase));
        }
}

TEST_CASE("Luz: brillo por celda según nivel y máscara") {
    CHECK(brillo_lut(15, 0) == 31);
    CHECK(brillo_lut(15, 255) == 31);
    CHECK(brillo_lut(7, 0) == 18);  // ambiente de noche
    CHECK(brillo_lut(7, 255) == 31);  // máscara llena: sin oscuridad
    CHECK(brillo_lut(7, 128) > brillo_lut(7, 64));  // más máscara, más brillo
    u8 r, g, b;
    CHECK(tinte_mapa(430, r, g, b));
    CHECK(r == 203);
    CHECK_FALSE(tinte_mapa(1, r, g, b));
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

TEST_CASE("Luz: el día del año sale de la hora de juego") {
    const i32 dia = 86400;
    CHECK(dia_del_anio(0) == 0);
    CHECK(dia_del_anio(dia - 1) == 0);
    CHECK(dia_del_anio(31 * dia) == 31);           // 1 de febrero de 1970
    CHECK(dia_del_anio(364 * dia) == 364);         // 31 de diciembre de 1970
    CHECK(dia_del_anio(365 * dia) == 0);           // 1 de enero de 1971
    CHECK(dia_del_anio((730 + 60) * dia) == 60);   // 1 de marzo de 1972, bisiesto
    CHECK(dia_del_anio(125570204) == 357);         // un S_GameTime real: 24 de diciembre de 1973
}

TEST_CASE("Luz: radio de la máscara de cada objeto") {
    CHECK(radio_de_luz(false, 0) == 0);
    CHECK(radio_de_luz(true, 0) == 5);  // S_OwnCharPack manda 0
    CHECK(radio_de_luz(true, 3) == 5);
    CHECK(radio_de_luz(true, 11) == 11);
    CHECK(radio_de_luz(false, 11) == 11);
    CHECK(radio_de_luz(false, 15) == 14);  // la máscara 15 de source.lmp está vacía
}

TEST_CASE("Mundo: reglas de S_Light del original") {
    Mundo m;
    auto poner = [&](ObjId id, int gfx, int luz) {
        Evento e{};
        e.tipo = Ev::PONER_OBJETO;
        e.id = id;
        e.v[OB_GFX] = gfx;
        e.v[OB_LUZ] = luz;
        e.s[0] = "x";
        Evento ev[] = {e};
        aplicar_eventos(m, ev, 1000);
    };
    auto luz = [&](ObjId id, int valor) {
        Evento e{};
        e.tipo = Ev::LUZ;
        e.id = id;
        e.v[0] = valor;
        Evento ev[] = {e};
        aplicar_eventos(m, ev, 1000);
        return static_cast<int>(m.objetos.col<O_LUZ>()[m.objetos.buscar(id)]);
    };
    m.jugador.id = 1;
    poner(1, 100, 0);
    poner(2, 100, 0);
    CHECK(luz(1, 3) == 5);  // el propio, mínimo 5
    CHECK(luz(1, 8) == 8);
    CHECK_FALSE(m.jugador.luz_hechizo);
    CHECK(luz(1, 20) == 14);  // hechizo LIGHT
    CHECK(m.jugador.luz_hechizo);
    CHECK(luz(1, 13) == 13);
    CHECK_FALSE(m.jugador.luz_hechizo);
    CHECK(luz(2, 3) == 3);  // los demás no tienen mínimo
    CHECK(luz(2, 20) == 5);
    CHECK_FALSE(m.jugador.luz_hechizo);
    poner(3, 0x17ae, 0);  // gfx con luz fija
    CHECK(m.objetos.col<O_LUZ>()[m.objetos.buscar(3)] == 14);
}

TEST_CASE("Mundo: S_Weather guarda la nieve y la lluvia por separado") {
    Mundo m;
    auto clima = [&](int valor) {
        Evento e{};
        e.tipo = Ev::CLIMA;
        e.v[0] = valor;
        Evento ev[] = {e};
        aplicar_eventos(m, ev, 1000);
    };
    clima(0x13);
    clima(0x00);  // lo que manda el servidor después de la lluvia: no la apaga
    CHECK(m.lluvia == 3);
    CHECK(m.nieve == 0);
    clima(0x02);
    CHECK(m.nieve == 2);
    CHECK(m.lluvia == 3);
    clima(0x00);
    clima(0x10);  // lo que manda el servidor para despejar
    CHECK(m.nieve == 0);
    CHECK(m.lluvia == 0);
    clima(0x14);  // intensidad fuera de rango: 0
    CHECK(m.lluvia == 0);
}

TEST_CASE("Agua: 1416.img es azul arriba (B mayor que R)") {
    const std::string dir = carpeta_juego();
    if (dir.empty()) return;  // sin la instalación del juego
    Recursos rec;
    REQUIRE(rec.abrir(dir));
    ImagenRGBA img;
    REQUIRE(decodificar_img(rec.sprite("1416.img"), img));
    // Columna central, píxeles con color (el transparente es 0): en conjunto, B supera a R (0xAABBGGRR).
    long r = 0, b = 0;
    for (int y = 0; y < img.alto; ++y) {
        const u32 p = img.px[static_cast<size_t>(y) * img.ancho + img.ancho / 2];
        if (!p) continue;
        r += p & 0xFF;
        b += (p >> 16) & 0xFF;
    }
    CHECK(b > r);
}
