#include "i18n/textos.h"
#include <SDL3/SDL.h>
#include <cstddef>
#include <string>

namespace lin {
namespace {

Idioma g_idioma = Idioma::EN;

struct Fila {
    const char* t[static_cast<size_t>(Idioma::CANTIDAD)];
};

#define X(clave, es, en, pt, de, fr) {{es, en, pt, de, fr}},
const Fila CATALOGO[] = {
#include "i18n/catalogo.def"
};
#undef X

static_assert(sizeof(CATALOGO) / sizeof(CATALOGO[0]) == static_cast<size_t>(Tx::CANTIDAD),
              "catalogo.def: cada clave de Tx necesita su fila (y al revés)");

constexpr const char* CODIGOS[] = {"es", "en", "pt", "de", "fr"};

}  // namespace

bool idioma_de_codigo(std::string_view codigo, Idioma& salida) {
    // Acepta "es_MX", "pt-BR": solo importa la parte de la lengua.
    const std::string_view lengua = codigo.substr(0, codigo.find_first_of("_-"));
    for (size_t i = 0; i < static_cast<size_t>(Idioma::CANTIDAD); ++i) {
        if (lengua == CODIGOS[i]) {
            salida = static_cast<Idioma>(i);
            return true;
        }
    }
    return false;
}

const char* codigo_de_idioma(Idioma i) { return CODIGOS[static_cast<size_t>(i)]; }

Idioma idioma_del_sistema() {
    int n = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&n);
    Idioma elegido = Idioma::EN;
    for (int i = 0; locales && i < n; ++i) {
        if (locales[i] && locales[i]->language && idioma_de_codigo(locales[i]->language, elegido)) break;
        elegido = Idioma::EN;
    }
    SDL_free(locales);
    return elegido;
}

void fijar_idioma(Idioma i) { g_idioma = i; }
Idioma idioma_actual() { return g_idioma; }

const char* tr_en(Tx clave, Idioma i) {
    const Fila& f = CATALOGO[static_cast<size_t>(clave)];
    const char* s = f.t[static_cast<size_t>(i)];
    return (s && *s) ? s : f.t[static_cast<size_t>(Idioma::EN)];  // nunca debería faltar; por si acaso
}

bool hay_traduccion(Tx clave, Idioma i) {
    const char* s = CATALOGO[static_cast<size_t>(clave)].t[static_cast<size_t>(i)];
    return s && *s;
}

const char* tr(Tx clave) { return tr_en(clave, g_idioma); }

}  // namespace lin
