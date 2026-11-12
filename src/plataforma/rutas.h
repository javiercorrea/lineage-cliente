// Configuración del cliente: carpeta del juego original, servidor y versión de protocolo.
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/tipos.h"

namespace lin {

// Un servidor del menú del login: "servidor=Nombre|host|puerto|protocolo|cuenta" en settings.ini
// (la cuenta es la última usada en ese servidor; opcional).
struct Servidor {
    std::string nombre, host;
    u16 puerto = 2000;
    std::string protocolo = "jp363";
    std::string usuario;
};

constexpr int PAGINAS_BARRA_MAX = 9;  // páginas de la barra de atajos

struct Config {
    std::string carpeta_juego;     // donde están Sprite*.pak, Tile.pak, map/, font/...
    std::vector<Servidor> servidores;  // al menos uno (ver leer_config)
    size_t servidor = 0;           // el elegido ("servidor_elegido=Nombre")
    int ventana_ancho = 1280, ventana_alto = 960;  // "ventana=AxB": en puntos, el de la última vez
    int zoom = 0;                  // escala en el juego (píxeles reales por píxel lógico); 0 = automática
    bool camara_a_saltos = false;  // la cámara sigue los frames de caminar, como el original (F4)
    bool ui_simple = true;         // UI de texto (estilo ASCII) en el juego; false = panel original (F2)
    bool mostrar_fps = false;      // fps y ms por frame en la esquina superior derecha (Shift+F3)
    bool pantalla_completa = false;  // juego: a pantalla completa (1) o en ventana sin borde (0, Alt+Enter)
    bool sombra_personajes = true; // los personajes parados en la sombra de un árbol se oscurecen (Shift+F4)
    // Efectos de luz y agua (PLAN_LUZ_AGUA.md), todos activados por defecto: la noche y las mazmorras
    // oscuras (con las luces de los personajes), el tinte azul bajo el agua, la ola, y la visión de elfo
    // y elfo oscuro.
    bool efecto_luz = true, efecto_agua_tinte = true, efecto_agua_ola = true, efecto_infravision = true;
    // Idioma de la interfaz: "es", "en", "pt", "de" o "fr". Vacío = el del sistema (ver i18n/textos.h).
    std::string idioma;
    // Atajos de hechizos F5–F12 por personaje: "Nombre:4,3,0,0,0,0,0,0;Otro:..." (skill ids, 0 = vacío).
    std::string atajos;
    // Barra de atajos Alt+1..Alt+0 por personaje: "Nombre:h4,i40308:268435461,0,...;Otro:..."
    // (h = hechizo, i = ítem gfx:id, 0 = vacío, * tras el elegido); las páginas, separadas por "/".
    std::string barra;
    int paginas_barra = 3;  // páginas a la vista (1 a PAGINAS_BARRA_MAX); las de más se conservan
    // Ventanas del juego: posición de las que se movieron ("inventario:900,36;estado:...") y
    // opacidad del fondo (0–255).
    std::string ventanas;
    int opacidad_ventanas = 170;
    int volumen_musica = 60, volumen_efectos = 80;  // 0–100
    // Logo propio de la pantalla de login con la UI simple: un PNG (de hasta 2048×2048; se achica
    // para entrar). Ruta absoluta o relativa a la carpeta de settings.ini; vacío = el título en texto.
    std::string logo;
    // Chat con Enter: las teclas son acciones (1–0 usan la barra) y Enter abre la línea del chat
    // (Enter envía, Esc cancela). false: se escribe siempre y la barra va con Alt+número.
    bool chat_con_enter = true;
    bool barra_personaje = false;  // barra de HP/MP sobre el propio personaje
    bool texto_dano = false;       // daño recibido / "Miss" flotante sobre el personaje
    bool globos_comic = false;     // lo que dice un personaje, en globo con recuadro y cola (no solo texto)
    // Compensación de ataque: al atacar, mostrar el golpe (swing propio y flinch del objetivo) ya,
    // sin esperar el eco del servidor, para que con latencia alta el combate se sienta instantáneo.
    bool compensar_ataque = true;
};

// Valores por personaje dentro de una opción ("Nombre:valor;Otro:valor").
std::string_view valor_de_personaje(std::string_view opcion, std::string_view personaje);
void poner_valor_de_personaje(std::string& opcion, std::string_view personaje, std::string_view valor);
// Quita las entradas de un personaje de una opción (el editor de ajustes las borra con esto).
void quitar_valor_de_personaje(std::string& opcion, std::string_view personaje);
// Parte `n` (desde 0) de un valor separado por '/' (las páginas de la barra); vacía si no hay.
std::string_view parte_de_valor(std::string_view valor, int n);
// Personajes que tienen datos guardados (atajos o barra), sin repetir y en orden de aparición.
std::vector<std::string> personajes_con_datos(const Config& c);

// Ruta del archivo de configuración del usuario (settings.ini).
// macOS: ~/Library/Application Support/LineageCliente/settings.ini (vía SDL_GetPrefPath).
// MULTIPLATAFORMA: SDL_GetPrefPath ya devuelve %APPDATA% en Windows y ~/.local/share en Linux.
std::string ruta_configuracion();

// Carga y guarda settings.ini (el del usuario, o el de la ruta dada: lo usa el editor de ajustes).
Config cargar_config();
Config cargar_config_de(const std::string& ruta);
bool guardar_config(const Config& c);
bool guardar_config_en(const std::string& ruta, const Config& c);
// El texto de settings.ini, sin archivos (para los tests). leer_config migra el formato anterior
// (host=, puerto=, protocolo=, usuario=: un servidor "Servidor") y, sin ninguno, agrega el local.
Config leer_config(std::string_view texto);
std::string escribir_config(const Config& c);
inline Servidor& servidor_elegido(Config& c) { return c.servidores[c.servidor]; }
inline const Servidor& servidor_elegido(const Config& c) { return c.servidores[c.servidor]; }

// Comprueba que la carpeta tenga los archivos mínimos del juego.
bool carpeta_juego_valida(const std::string& carpeta);

}  // namespace lin
