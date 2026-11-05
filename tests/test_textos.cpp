#include <doctest/doctest.h>
#include <string>
#include "i18n/textos.h"

using namespace lin;

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
