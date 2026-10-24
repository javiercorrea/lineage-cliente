// Interfaz en modo inmediato: cada frame se llama a las funciones de los controles, que dibujan
// en la lista del renderer y devuelven la interacción. El estado de cada pantalla es una struct
// plana (foco, textos); no hay árbol de widgets.
#pragma once
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

// UI simple (login, selección y creación de personaje): recuadro oscuro con borde, y caja de
// texto con fondo (como caja_texto; true con Enter). Colores de la UI simple del juego.
constexpr u32 COLOR_FONDO_SIMPLE = 0xFF080A0Cu, COLOR_TITULO = 0xFF8CBED6u, COLOR_TEXTO = 0xFFFEFFFFu;
void recuadro(Graficos& g, int x, int y, int w, int h, u8 opacidad = 235);
bool campo(Graficos& g, const EntradaFrame& e, std::string& valor, int x, int y, int ancho, bool foco, bool es_clave,
           size_t maximo);
// Texto centrado horizontalmente en [x, x + w).
void texto_centrado(Graficos& g, std::string_view t, int x, int y, int w, u32 color = COLOR_TEXTO);

// Ventana del juego (estado, inventario): el marco original 4131.img o, con la UI simple, un
// recuadro de texto con título y "[x]". `opacidad`: la del fondo (el juego se ve detrás); el
// marco y el texto quedan un poco más opacos. Devuelve true si se hizo clic en cerrar.
constexpr int VENTANA_ANCHO = 247, VENTANA_ALTO = 304, VENTANA_Y = 36;
constexpr int ALTO_TITULO = 22;  // barra de título: de ahí se arrastra la ventana
bool ventana(Graficos& g, const EntradaFrame& e, bool simple, int x, int y, std::string_view titulo, u8 opacidad = 235);

}  // namespace ui
}  // namespace lin
