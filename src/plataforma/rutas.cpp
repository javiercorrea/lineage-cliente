#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>
#include "plataforma/archivo_mapeado.h"
#include "plataforma/rutas.h"

namespace lin {

std::string ruta_configuracion() {
#if LIN_CONTROL
    // LIN_CONFIG: otro settings.ini (pruebas en paralelo sin pisar la configuración del usuario).
    // Solo en desarrollo: la versión final usa siempre el del usuario.
    if (const char* e = std::getenv("LIN_CONFIG"); e && *e) return e;
#endif
    char* p = SDL_GetPrefPath("Lineage", "LineageCliente");
    std::string r = p ? std::string(p) + "settings.ini" : "settings.ini";
    SDL_free(p);
    return r;
}

namespace {

// Lo que viene del formato anterior (un solo servidor).
struct Anterior {
    std::string host, protocolo, usuario;
    int puerto = 0;
    std::string elegido;  // servidor_elegido=
};

Servidor leer_servidor(std::string_view v) {
    Servidor sv;
    std::string_view campos[5];
    for (int i = 0; i < 5 && !v.empty(); ++i) {
        const size_t b = v.find('|');
        campos[i] = v.substr(0, b);
        v = b == std::string_view::npos ? std::string_view{} : v.substr(b + 1);
    }
    sv.nombre = campos[0];
    sv.host = campos[1];
    if (!campos[2].empty()) sv.puerto = static_cast<u16>(std::atoi(std::string(campos[2]).c_str()));
    if (!campos[3].empty()) sv.protocolo = campos[3];
    sv.usuario = campos[4];
    return sv;
}

void aplicar(Config& c, Anterior& ant, std::string_view clave, std::string_view valor) {
    if (clave == "juego") c.carpeta_juego = valor;
    else if (clave == "servidor") c.servidores.push_back(leer_servidor(valor));
    else if (clave == "servidor_elegido") ant.elegido = valor;
    else if (clave == "host") ant.host = valor;
    else if (clave == "puerto") ant.puerto = std::atoi(std::string(valor).c_str());
    else if (clave == "protocolo") ant.protocolo = valor;
    else if (clave == "usuario") ant.usuario = valor;
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
    else if (clave == "ventana") {
        const size_t x = valor.find('x');
        const int w = std::atoi(std::string(valor.substr(0, x)).c_str());
        const int h = x == std::string_view::npos ? 0 : std::atoi(std::string(valor.substr(x + 1)).c_str());
        if (w >= 320 && h >= 240) c.ventana_ancho = w, c.ventana_alto = h;
    }
}

// Después de leer: migra el formato anterior, garantiza un servidor y elige el guardado.
void completar(Config& c, const Anterior& ant) {
    if (c.servidores.empty()) {
        Servidor sv;
        sv.nombre = ant.host.empty() ? "Local" : "Servidor";
        sv.host = ant.host.empty() ? "127.0.0.1" : ant.host;
        if (ant.puerto) sv.puerto = static_cast<u16>(ant.puerto);
        if (!ant.protocolo.empty()) sv.protocolo = ant.protocolo;
        c.servidores.push_back(std::move(sv));
    }
    c.servidor = 0;
    for (size_t i = 0; i < c.servidores.size(); ++i)
        if (c.servidores[i].nombre == ant.elegido) c.servidor = i;
    if (!ant.usuario.empty()) c.servidores[c.servidor].usuario = ant.usuario;
}

}  // namespace

Config leer_config(std::string_view t) {
    Config c;
    Anterior ant;
    while (!t.empty()) {
        size_t fin = t.find('\n');
        std::string_view linea = t.substr(0, fin);
        t = fin == std::string_view::npos ? std::string_view{} : t.substr(fin + 1);
        while (!linea.empty() && (linea.back() == '\r' || linea.back() == ' ')) linea.remove_suffix(1);
        size_t igual = linea.find('=');
        if (linea.empty() || linea[0] == '#' || igual == std::string_view::npos) continue;
        aplicar(c, ant, linea.substr(0, igual), linea.substr(igual + 1));  // "clave=" de un settings.ini viejo: se ignora
    }
    completar(c, ant);
    return c;
}

Config cargar_config() {
    std::vector<u8> ini;
    Config c = leer_archivo(ruta_configuracion(), ini)
                   ? leer_config({reinterpret_cast<const char*>(ini.data()), ini.size()})
                   : leer_config({});
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

std::string escribir_config(const Config& c) {
    std::string t = "# Configuración del cliente Lineage\n";
    auto linea = [&](const char* clave, std::string_view valor) {
        t += clave;
        t += '=';
        t += valor;
        t += '\n';
    };
    linea("juego", c.carpeta_juego);
    // Servidores: nombre|host|puerto|protocolo|cuenta (la última usada en ese servidor).
    for (const Servidor& sv : c.servidores)
        linea("servidor", sv.nombre + '|' + sv.host + '|' + std::to_string(sv.puerto) + '|' + sv.protocolo + '|' + sv.usuario);
    linea("servidor_elegido", servidor_elegido(c).nombre);
    linea("zoom", std::to_string(c.zoom));
    linea("camara_a_saltos", c.camara_a_saltos ? "1" : "0");
    linea("ui_simple", c.ui_simple ? "1" : "0");
    linea("sombra_personajes", c.sombra_personajes ? "1" : "0");
    linea("atajos", c.atajos);
    linea("barra", c.barra);
    linea("ventanas", c.ventanas);
    linea("opacidad_ventanas", std::to_string(c.opacidad_ventanas));
    linea("volumen_musica", std::to_string(c.volumen_musica));
    linea("volumen_efectos", std::to_string(c.volumen_efectos));
    linea("logo", c.logo);
    linea("chat_con_enter", c.chat_con_enter ? "1" : "0");
    linea("ventana", std::to_string(c.ventana_ancho) + 'x' + std::to_string(c.ventana_alto));
    return t;
}

bool guardar_config(const Config& c) {
    FILE* f = std::fopen(ruta_configuracion().c_str(), "wb");
    if (!f) return false;
    const std::string t = escribir_config(c);
    const bool ok = std::fwrite(t.data(), 1, t.size(), f) == t.size();
    return std::fclose(f) == 0 && ok;
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
