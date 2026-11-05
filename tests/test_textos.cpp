#include <doctest/doctest.h>
#include <cstring>
#include <string>
#include "i18n/textos.h"
#include "recursos/fuente.h"

using namespace lin;

// Etiquetas del editor de ajustes (la columna izquierda mide 210 px: ver tools/ajustes.cpp). Si una
// traducción es más ancha, se mete debajo del campo.
TEST_CASE("editor de ajustes: las etiquetas caben en 210 px en los cinco idiomas") {
    const Tx etiquetas[] = {Tx::AJ_CARPETA, Tx::AJ_CAMARA, Tx::AJ_CHAT_ENTER, Tx::AJ_SOMBRA, Tx::AJ_COMPENSAR,
                            Tx::AJ_TEXTO_DANO, Tx::AJ_BARRA_PJ, Tx::AJ_UI_SIMPLE, Tx::AJ_OPACIDAD, Tx::AJ_LOGO,
                            Tx::AJ_VENTANA_ANCHO, Tx::AJ_VENTANA_ALTO, Tx::AJ_ZOOM, Tx::AJ_IDIOMA, Tx::AJ_MUSICA,
                            Tx::AJ_EFECTOS, Tx::AJ_SERVIDOR_EDITAR, Tx::AJ_NOMBRE, Tx::AJ_HOST, Tx::AJ_PUERTO,
                            Tx::AJ_PROTOCOLO, Tx::AJ_CUENTA, Tx::AJ_SERVIDOR_ENTRAR, Tx::AJ_PERSONAJE};
    for (Tx k : etiquetas)
        for (size_t i = 0; i < static_cast<size_t>(Idioma::CANTIDAD); ++i) {
            const char* s = tr_en(k, static_cast<Idioma>(i));
            CHECK_MESSAGE(ancho_texto(s, std::strlen(s)) <= 210, "etiqueta demasiado ancha: " << std::string(s));
        }
}

TEST_CASE("catalogo: cada clave tiene texto en los cinco idiomas") {
    for (size_t k = 0; k < static_cast<size_t>(Tx::CANTIDAD); ++k)
        for (size_t i = 0; i < static_cast<size_t>(Idioma::CANTIDAD); ++i)
            CHECK_MESSAGE(hay_traduccion(static_cast<Tx>(k), static_cast<Idioma>(i)),
                          "falta el texto de la clave " << k << " en el idioma " << i);
}

TEST_CASE("catalogo: los formatos (%d, %s) son los mismos en todos los idiomas") {
    auto cuenta = [](const char* s) {
        int n = 0;
        for (const char* p = s; *p; ++p) n += *p == '%';
        return n;
    };
    for (size_t k = 0; k < static_cast<size_t>(Tx::CANTIDAD); ++k) {
        const int esperado = cuenta(tr_en(static_cast<Tx>(k), Idioma::ES));
        for (size_t i = 0; i < static_cast<size_t>(Idioma::CANTIDAD); ++i)
            CHECK_MESSAGE(cuenta(tr_en(static_cast<Tx>(k), static_cast<Idioma>(i))) == esperado,
                          "distinto número de %% en la clave " << k << ", idioma " << i);
    }
}

TEST_CASE("idioma_de_codigo: acepta variantes regionales y rechaza lo que no tenemos") {
    Idioma i = Idioma::EN;
    REQUIRE(idioma_de_codigo("pt_BR", i));
    CHECK(i == Idioma::PT);
    REQUIRE(idioma_de_codigo("fr-CA", i));
    CHECK(i == Idioma::FR);
    REQUIRE(idioma_de_codigo("de", i));
    CHECK(i == Idioma::DE);
    CHECK_FALSE(idioma_de_codigo("ja", i));
    CHECK_FALSE(idioma_de_codigo("", i));
}

TEST_CASE("tr: sigue al idioma activo") {
    fijar_idioma(Idioma::DE);
    CHECK(std::string(tr(Tx::UI_VOLVER)) == "Zurück");
    fijar_idioma(Idioma::FR);
    CHECK(std::string(tr(Tx::UI_VOLVER)) == "Retour");
    fijar_idioma(Idioma::EN);
    CHECK(std::string(tr(Tx::UI_VOLVER)) == "Back");
}
