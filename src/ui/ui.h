// Interfaz en modo inmediato: cada frame se llama a las funciones de los controles, que dibujan
// en la lista del renderer y devuelven la interacción. El estado de cada pantalla es una struct
// plana (foco, textos); no hay árbol de widgets.
#pragma once
#include <SDL3/SDL.h>
#include <span>
#include <string>
#include "render/graficos.h"

namespace lin {

// Entrada del frame, ya en coordenadas lógicas (ver Gpu::ajustar_vista).
struct EntradaFrame {
    int mx = -1, my = -1;
    bool clic = false;            // botón izquierdo soltado este frame
    bool pulsado = false;         // botón izquierdo apretado este frame
    bool presionado = false;      // botón izquierdo mantenido
    bool ctrl = false;            // Ctrl (o Cmd) mantenido
    bool shift = false;           // Shift mantenido
    bool clic_derecho = false;
    bool derecho_presionado = false;
    bool clic_medio = false;      // botón del medio (rueda) soltado este frame
    bool medio_presionado = false;  // botón del medio mantenido
    bool enter = false, tab = false, escape = false, retroceso = false;
    bool arriba = false, abajo = false, izquierda = false, derecha = false;  // mantenidas
    bool pagina_arriba = false, pagina_abajo = false;  // Re Pág / Av Pág este frame
    bool alt_c = false;           // Alt+C este frame (ventana de estado)
    bool alt_i = false;           // Alt+I este frame (inventario)
    bool alt_s = false;           // Alt+S este frame (hechizos)
    int tecla_f = 0;              // F5..F12 apretada este frame (5..12; 0 = ninguna): atajos de hechizos
    int alt_numero = 0;           // Alt+1..Alt+9, Alt+0 este frame (1..10; 0 = ninguna): barra de atajos
    bool tecla_a = false;         // A sin modificadores este frame (ataque sostenido, si no se escribe en el chat)
    int numero = 0;               // 1..9, 0 sin modificadores este frame (1..10): barra, si no se escribe en el chat
    int rueda = 0;                // rueda del mouse este frame (+ hacia arriba)
    int zoom = 0;                 // Ctrl/Cmd + (+1) o - (-1) este frame; 2 = Ctrl/Cmd 0 (automático)
    char texto[64] = {};          // texto escrito este frame (UTF-8; solo se acepta ASCII)
    int ntexto = 0;
    u64 ahora_ms = 0;
};

namespace ui {

bool sobre(const EntradaFrame& e, int x, int y, int w, int h);
// Imagen de interfaz (N.img) en (x, y).
void imagen(Graficos& g, u32 n, int x, int y, Capa capa = CAPA_UI);
// Botón con imagen N (normal) y N+1 (resaltado). Devuelve true al hacer clic.
bool boton(Graficos& g, const EntradaFrame& e, u32 n, int x, int y);
// Caja de texto de una línea. Devuelve true si se presionó Enter con foco.
bool caja_texto(Graficos& g, const EntradaFrame& e, std::string& valor, int x, int y, int ancho, bool foco,
                bool es_clave, size_t maximo);
// Botón de texto simple (para controles que el original no tiene como imagen).
bool boton_texto(Graficos& g, const EntradaFrame& e, std::string_view t, int x, int y, int w, int h);

// Barra de desplazamiento vertical: flechas arriba y abajo, pulgar que se arrastra y un clic en la
// pista avanza una página; con `sobre_lista`, también la rueda. `pos` va de 0 a total - visibles.
// Se llama ANTES de dibujar las filas: el clic que empieza en la barra es suyo hasta soltarlo (quita
// `pulsado` y `clic` de `e`), así soltar el pulgar sobre una fila no la activa.
struct BarraScroll {
    bool presionada = false;   // el botón se apretó sobre la barra y sigue apretado
    bool arrastrando = false;  // ... sobre el pulgar
    bool movido = false;       // el puntero ya se movió: apretar sin mover no cambia la posición
    int agarre = 0;            // distancia del puntero al borde superior del pulgar al empezar
    int my0 = 0;               // y del puntero al empezar
};
void barra_scroll(Graficos& g, EntradaFrame& e, BarraScroll& st, int x, int y, int ancho, int alto, u32 total,
                  u32 visibles, u32& pos, u64 clave, bool sobre_lista);

// UI simple (login, selección y creación de personaje): recuadro oscuro con borde, y caja de
// texto con fondo (como caja_texto; true con Enter). Colores de la UI simple del juego.
constexpr u32 COLOR_FONDO_SIMPLE = 0xFF080A0Cu, COLOR_TITULO = 0xFF8CBED6u, COLOR_TEXTO = 0xFFFEFFFFu;
void recuadro(Graficos& g, int x, int y, int w, int h, u8 opacidad = 235);
bool campo(Graficos& g, const EntradaFrame& e, std::string& valor, int x, int y, int ancho, bool foco, bool es_clave,
           size_t maximo);
// Menú desplegable (con el aspecto de `campo`): el elegido y "v"; abierto, la lista debajo, encima de
// todo. Un clic lo abre o lo cierra; uno en una opción la elige y devuelve true. Mientras está
// abierto, el clic y Escape son suyos: quita `clic` y `escape` de `e` para los controles que se
// dibujan después.
bool desplegable(Graficos& g, EntradaFrame& e, std::span<const std::string> opciones, size_t& elegido, bool& abierto, int x,
                 int y, int ancho);
// Texto centrado horizontalmente en [x, x + w).
void texto_centrado(Graficos& g, std::string_view t, int x, int y, int w, u32 color = COLOR_TEXTO);

// Ventana del juego (estado, inventario): el marco original 4131.img o, con la UI simple, un
// recuadro de texto con título y "[x]". `opacidad`: la del fondo (el juego se ve detrás); el
// marco y el texto quedan un poco más opacos. Devuelve true si se hizo clic en cerrar.
constexpr int VENTANA_ANCHO = 247, VENTANA_ALTO = 304, VENTANA_Y = 36;
constexpr int ALTO_TITULO = 22;  // barra de título: de ahí se arrastra la ventana
bool ventana(Graficos& g, const EntradaFrame& e, bool simple, int x, int y, std::string_view titulo, u8 opacidad = 235);

// Barra propia de las ventanas sin borde (el juego y los ajustes): título y dos botones, cerrar y
// minimizar. En macOS van a la izquierda, como los del sistema.
// MULTIPLATAFORMA: en Linux y Windows van a la derecha; sin probar en esas plataformas.
constexpr int BARRA_VENTANA_ALTO = 26;  // píxeles lógicos
struct BotonesBarra {
    int cerrar, minimizar, lado;  // x de cada uno, en píxeles lógicos
};
BotonesBarra botones_barra(int ancho);
// Ventana sin borde: barra (título, minimizar, cerrar) y, si `redimensionable`, los bordes. Sus
// eventos de ratón los maneja barra_evento, no SDL_SetWindowHitTest: en macOS, con esa prueba, el
// ratón se pierde al soltar sobre la barra. El arrastre sigue la posición global del ratón.
constexpr int BORDE_IZQ = 1, BORDE_DER = 2, BORDE_ARR = 4, BORDE_ABA = 8;
enum class AccionBarra { NINGUNA, MINIMIZAR, CERRAR };
struct Barra {
    bool redimensionable = false;
    bool pulsada = false;      // el press empezó en la barra o en un borde: lo que sigue es suyo
    bool arrastrando = false;  // mover (bordes == 0) o redimensionar por `bordes`
    int bordes = 0;            // BORDE_IZQ | BORDE_DER | BORDE_ARR | BORDE_ABA
    int boton = 0;             // botón de la barra bajo el press: 1 minimizar, 2 cerrar, 0 ninguno
    AccionBarra accion = AccionBarra::NINGUNA;  // al soltar sobre un botón; la toma barra_frame
    float raton_x = 0, raton_y = 0;             // posición global del ratón al empezar
    int vx = 0, vy = 0, vw = 0, vh = 0;         // la ventana al empezar
};
// Un evento de ratón: true si es de la barra o de un borde (el resto de la ventana no lo ve).
// `activa`: la barra se ve (ventana en modo ventana).
bool barra_evento(Gpu& gpu, SDL_Window* ventana, Barra& b, const SDL_Event& ev, bool activa);
// Cada frame: sigue el arrastre (lo suelta si el botón ya no está) y devuelve la acción pedida.
AccionBarra barra_frame(SDL_Window* ventana, Barra& b);

// Ícono de la ventana (y del Dock en macOS): datos/iconos/<archivo> junto al ejecutable o en el directorio
// actual (tools/generar_iconos.py). Sin el archivo, queda el del sistema.
void poner_icono(SDL_Window* ventana, const char* archivo);
// Dibuja la barra arriba del todo, en coordenadas del llamador (`x0`, `y0`: su esquina).
void barra_ventana(Graficos& g, const EntradaFrame& e, std::string_view titulo, int x0, int y0, int ancho);

}  // namespace ui
}  // namespace lin
