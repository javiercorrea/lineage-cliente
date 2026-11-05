// Idiomas de la interfaz del cliente: español, inglés, portugués, alemán y francés. Los textos están
// en src/i18n/catalogo.def; se piden por clave con tr(). Los textos del servidor (CP949) no se
// traducen: vienen del servidor tal cual.
#pragma once
#include <string_view>
#include "core/tipos.h"

namespace lin {

enum class Idioma : u8 { ES, EN, PT, DE, FR, CANTIDAD };

// "es", "en", "pt", "de", "fr" -> el idioma. false si no es uno de los cinco.
bool idioma_de_codigo(std::string_view codigo, Idioma& salida);
const char* codigo_de_idioma(Idioma i);
// El primer idioma del sistema que tengamos (SDL_GetPreferredLocales); inglés si no hay ninguno.
Idioma idioma_del_sistema();

void fijar_idioma(Idioma i);
Idioma idioma_actual();

// Clave del catálogo (catalogo.def): UI_F2_ORIGINAL, LOGIN_CUENTA, ...
enum class Tx : u16 {
#define X(clave, ...) clave,
#include "i18n/catalogo.def"
#undef X
    CANTIDAD
};

// Texto en el idioma activo (UTF-8, válido hasta el próximo fijar_idioma).
const char* tr(Tx clave);
// Texto en un idioma concreto (para los tests y para el editor de ajustes).
const char* tr_en(Tx clave, Idioma i);
// Si el catálogo trae ese texto en ese idioma (tr_en vuelve al inglés cuando falta).
bool hay_traduccion(Tx clave, Idioma i);

}  // namespace lin
