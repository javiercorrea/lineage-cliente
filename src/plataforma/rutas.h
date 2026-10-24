// Configuración del cliente: carpeta del juego original, servidor y versión de protocolo.
#pragma once
#include <string>
#include <string_view>
#include "core/tipos.h"

namespace lin {

struct Config {
    std::string carpeta_juego;     // donde están Sprite*.pak, Tile.pak, map/, font/...
    std::string host = "127.0.0.1";
    u16 puerto = 2000;
    std::string protocolo = "jp363";
    std::string usuario;           // prellenado de la pantalla de login (opcional)
    std::string clave;
    int zoom = 0;                  // escala en el juego (píxeles reales por píxel lógico); 0 = automática
    bool camara_a_saltos = false;  // la cámara sigue los frames de caminar, como el original (F4)
    bool ui_simple = true;         // UI de texto (estilo ASCII) en el juego; false = panel original (F2)
    bool sombra_personajes = true; // los personajes parados en la sombra de un árbol se oscurecen (Shift+F4)
    // Atajos de hechizos F5–F12 por personaje: "Nombre:4,3,0,0,0,0,0,0;Otro:..." (skill ids, 0 = vacío).
    std::string atajos;
    // Barra de atajos Alt+1..Alt+0 por personaje: "Nombre:h4,i40308:268435461,0,...;Otro:..."
    // (h = hechizo, i = ítem gfx:id, 0 = vacío).
    std::string barra;
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
};

// Valores por personaje dentro de una opción ("Nombre:valor;Otro:valor").
std::string_view valor_de_personaje(std::string_view opcion, std::string_view personaje);
void poner_valor_de_personaje(std::string& opcion, std::string_view personaje, std::string_view valor);

// Ruta del archivo de configuración del usuario (settings.ini).
// macOS: ~/Library/Application Support/LineageCliente/settings.ini (vía SDL_GetPrefPath).
// MULTIPLATAFORMA: SDL_GetPrefPath ya devuelve %APPDATA% en Windows y ~/.local/share en Linux.
std::string ruta_configuracion();

// Carga settings.ini; las variables de entorno LINEAGE_DIR, LINEAGE_HOST y LINEAGE_PORT y los
// argumentos --juego=, --host=, --puerto= tienen prioridad.
Config cargar_config(int argc, char** argv);
bool guardar_config(const Config& c);

// Comprueba que la carpeta tenga los archivos mínimos del juego.
bool carpeta_juego_valida(const std::string& carpeta);

}  // namespace lin
