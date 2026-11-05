#include <doctest/doctest.h>
#include <string>
#include "recursos/fuente.h"
#include "recursos/fuente_ttf.h"

using namespace lin;

TEST_CASE("es_utf8: distingue UTF-8 de CP949") {
    CHECK(es_utf8("Canción ñ", std::string("Canción ñ").size()));
    CHECK(es_utf8("solo ascii", 10));
    const std::string coreano = "\xB0\xA1";  // 가 en CP949: el byte de continuación queda suelto
    CHECK_FALSE(es_utf8(coreano.data(), coreano.size()));
    const std::string sobrelargo = "\xC0\xAF";  // overlong de '/'
    CHECK_FALSE(es_utf8(sobrelargo.data(), sobrelargo.size()));
}

TEST_CASE("largo_utf8: decodifica el punto de código") {
    const std::string e = "é";
    u32 cp = 0;
    CHECK(largo_utf8(e.data(), e.size(), 0, cp) == 2);
    CHECK(cp == 0xE9);
}

TEST_CASE("glifos_de_texto: un carácter UTF-8 es un solo glifo TTF") {
    const std::string t = "ñ";
    std::vector<GlifoTexto> g;
    glifos_de_texto(t.data(), t.size(), g);
    REQUIRE(g.size() == 1);
    CHECK(g[0].glifo == GLIFO_TTF_BASE + 0xF1);
    CHECK(g[0].ancho == ancho_ttf(0xF1));
}

TEST_CASE("ancho_ttf: Departure Mono a 11 px avanza 7 px por letra (cuadrícula entera)") {
    // 350 unidades de avance x 11 px / 550 de em = 7 px exactos. La bitmap original usa 6 (no es lo mismo).
    CHECK(ancho_ttf('a') == 7);
    CHECK(ancho_ttf('M') == 7);
}

TEST_CASE("ancho_texto: suma los avances de cada carácter") {
    const std::string t = "ñaé";
    CHECK(ancho_texto(t.data(), t.size()) == ancho_ttf(0xF1) + ancho_ttf('a') + ancho_ttf(0xE9));
}

TEST_CASE("bytes_que_caben: no parte un carácter UTF-8 y corta en el espacio") {
    const std::string t = "ñandú ñandú";
    const int limite = ancho_texto(t.data(), 8);  // "ñandú " = 6 caracteres
    CHECK(bytes_que_caben(t.data(), t.size(), limite) == 8);
    // Un límite de un carácter debe dejar al menos un carácter completo (2 bytes si es 'ñ').
    CHECK(bytes_que_caben(t.data(), t.size(), 1) == 2);
}

TEST_CASE("rasterizar_ttf: la celda es premultiplicada y tiene trazo") {
    ImagenRGBA img;
    REQUIRE(rasterizar_ttf(0xE9, img));
    CHECK(img.alto == ALTO_GLIFO);
    CHECK(img.ancho == ancho_ttf(0xE9));
    bool hay_trazo = false, premultiplicado = true;
    for (u32 p : img.px) {
        const u32 a = p >> 24;
        if (a) hay_trazo = true;
        if ((p & 0xFF) != a || ((p >> 8) & 0xFF) != a || ((p >> 16) & 0xFF) != a) premultiplicado = false;
    }
    CHECK(hay_trazo);
    CHECK(premultiplicado);
}

// Fila más alta y más baja con trazo: la tilde de la É y el descendente de la g no se pueden recortar.
static bool fila_con_trazo(const ImagenRGBA& img, int fila) {
    for (int x = 0; x < img.ancho; ++x)
        if (img.px[static_cast<size_t>(fila) * img.ancho + x] >> 24) return true;
    return false;
}

TEST_CASE("rasterizar_ttf: la tilde y el descendente caben en la celda") {
    ImagenRGBA acentuada, descendente;
    REQUIRE(rasterizar_ttf(0xC9, acentuada));  // É
    REQUIRE(rasterizar_ttf('g', descendente));
    CHECK(fila_con_trazo(acentuada, 0));            // la tilde llega hasta la primera fila
    // El descendente baja de la línea base (fila 11 con em 14) y se ve en la fila siguiente.
    CHECK(fila_con_trazo(descendente, ALTO_GLIFO - 2));
}

TEST_CASE("FuenteBitmap: los glifos TTF no necesitan la carpeta del juego") {
    FuenteBitmap f;
    ImagenRGBA img;
    CHECK(f.rasterizar(GLIFO_TTF_BASE + 'A', img));
    CHECK(img.alto == ALTO_GLIFO);
}
