#include <SDL3/SDL.h>
#include <cstdlib>
#include <string_view>
#include <vector>
#include "plataforma/archivo_mapeado.h"
#include "plataforma/rutas.h"

namespace lin {

std::string ruta_configuracion() {
    // LIN_CONFIG: otro settings.ini (pruebas en paralelo sin pisar la configuración del usuario).
    if (const char* e = std::getenv("LIN_CONFIG"); e && *e) return e;
    char* p = SDL_GetPrefPath("Lineage", "LineageCliente");
    std::string r = p ? std::string(p) + "settings.ini" : "settings.ini";
    SDL_free(p);
    return r;
}

static void aplicar(Config& c, std::string_view clave, std::string_view valor) {
    if (clave == "juego") c.carpeta_juego = valor;
    else if (clave == "host") c.host = valor;
    else if (clave == "puerto") c.puerto = static_cast<u16>(std::atoi(std::string(valor).c_str()));
    else if (clave == "protocolo") c.protocolo = valor;
    else if (clave == "usuario") c.usuario = valor;
    else if (clave == "clave") c.clave = valor;
    else if (clave == "zoom") c.zoom = std::atoi(std::string(valor).c_str());
    else if (clave == "camara_a_saltos") c.camara_a_saltos = valor == "1";
    else if (clave == "ui_simple") c.ui_simple = valor != "0";
    else if (clave == "sombra_personajes") c.sombra_personajes = valor != "0";
    else if (clave == "atajos") c.atajos = valor;
    else if (clave == "barra") c.barra = valor;
    else if (clave == "ventanas") c.ventanas = valor;
    else if (clave == "opacidad_ventanas") c.opacidad_ventanas = std::atoi(std::string(valor).c_str());
    else if (clave == "volumen_musica") c.volumen_musica = std::atoi(std::string(valor).c_str());
    else if (clave == "volumen_efectos") c.volumen_efectos = std::atoi(std::string(valor).c_str());
    else if (clave == "logo") c.logo = valor;
    else if (clave == "chat_con_enter") c.chat_con_enter = valor != "0";
    else if (clave == "modelos") c.modelos = valor;
    else if (clave == "modelos_3d") c.modelos_3d = valor != "0";
}

Config cargar_config(int argc, char** argv) {
    Config c;
    std::vector<u8> ini;
    if (leer_archivo(ruta_configuracion(), ini)) {
        std::string_view t(reinterpret_cast<const char*>(ini.data()), ini.size());
        while (!t.empty()) {
            size_t fin = t.find('\n');
            std::string_view linea = t.substr(0, fin);
            t = fin == std::string_view::npos ? std::string_view{} : t.substr(fin + 1);
            while (!linea.empty() && (linea.back() == '\r' || linea.back() == ' ')) linea.remove_suffix(1);
            size_t igual = linea.find('=');
            if (linea.empty() || linea[0] == '#' || igual == std::string_view::npos) continue;
            aplicar(c, linea.substr(0, igual), linea.substr(igual + 1));
        }
    }
    if (const char* e = std::getenv("LINEAGE_DIR")) c.carpeta_juego = e;
    if (const char* e = std::getenv("LINEAGE_HOST")) c.host = e;
    if (const char* e = std::getenv("LINEAGE_PORT")) c.puerto = static_cast<u16>(std::atoi(e));
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a.substr(0, 2) != "--") continue;
        size_t igual = a.find('=');
        if (igual != std::string_view::npos) aplicar(c, a.substr(2, igual - 2), a.substr(igual + 1));
    }
    if (!c.carpeta_juego.empty() && c.carpeta_juego.back() != '/') c.carpeta_juego += '/';
    return c;
}

std::string_view valor_de_personaje(std::string_view opcion, std::string_view personaje) {
    while (!opcion.empty()) {
        std::string_view par = opcion.substr(0, opcion.find(';'));
        opcion = par.size() < opcion.size() ? opcion.substr(par.size() + 1) : std::string_view{};
        size_t dos = par.find(':');
        if (dos != std::string_view::npos && par.substr(0, dos) == personaje) return par.substr(dos + 1);
    }
    return {};
}

void poner_valor_de_personaje(std::string& opcion, std::string_view personaje, std::string_view valor) {
    std::string nuevo;
    std::string_view t = opcion;
    while (!t.empty()) {  // los de los demás personajes, igual
        std::string_view par = t.substr(0, t.find(';'));
        t = par.size() < t.size() ? t.substr(par.size() + 1) : std::string_view{};
        if (!par.empty() && par.substr(0, par.find(':')) != personaje) (nuevo += par) += ';';
    }
    ((nuevo += personaje) += ':') += valor;
    opcion = std::move(nuevo);
}

bool guardar_config(const Config& c) {
    FILE* f = std::fopen(ruta_configuracion().c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "# Configuración del cliente Lineage\njuego=%s\nhost=%s\npuerto=%u\nprotocolo=%s\nusuario=%s\nzoom=%d\ncamara_a_saltos=%d\nui_simple=%d\nsombra_personajes=%d\natajos=%s\nbarra=%s\nventanas=%s\nopacidad_ventanas=%d\nvolumen_musica=%d\nvolumen_efectos=%d\nlogo=%s\nchat_con_enter=%d\nmodelos=%s\nmodelos_3d=%d\n",
                 c.carpeta_juego.c_str(), c.host.c_str(), c.puerto, c.protocolo.c_str(), c.usuario.c_str(), c.zoom,
                 c.camara_a_saltos ? 1 : 0, c.ui_simple ? 1 : 0, c.sombra_personajes ? 1 : 0, c.atajos.c_str(), c.barra.c_str(), c.ventanas.c_str(),
                 c.opacidad_ventanas, c.volumen_musica, c.volumen_efectos, c.logo.c_str(), c.chat_con_enter ? 1 : 0, c.modelos.c_str(), c.modelos_3d ? 1 : 0);
    std::fclose(f);
    return true;
}

bool carpeta_juego_valida(const std::string& carpeta) {
    // MULTIPLATAFORMA: en macOS y Windows el sistema de archivos no distingue mayúsculas; en Linux
    // sí, y la instalación original mezcla "Sprite.pak", "Tile.pak", "map/". Habrá que buscar los
    // nombres sin distinguir mayúsculas en Linux.
    for (const char* f : {"Sprite.idx", "Sprite.pak", "Tile.idx", "Tile.pak", "Text.idx", "Text.pak"}) {
        if (!SDL_GetPathInfo((carpeta + f).c_str(), nullptr)) return false;
    }
    return true;
}

}  // namespace lin
