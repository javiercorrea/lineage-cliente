// Ventana del juego: en ventana, sin borde del sistema y con la barra propia (título, minimizar y
// cerrar); a pantalla completa, sin barra. La barra va encima de todo, como los diálogos.
#include "app/app.h"
#include "render/graficos.h"
#include "ui/ui.h"

namespace lin {

namespace {

// La barra queda sobre las ventanas, como los diálogos.
constexpr u32 NIVEL_BARRA = NIVEL_SOBRE_VENTANAS;

}  // namespace

int alto_barra(const App& a) {
    // Sin barra con el juego cargando o mostrando una captura, ni a pantalla completa (lo que diga la ventana).
    if (!a.g || a.render.activo || !a.ventana) return 0;
    return SDL_GetWindowFlags(a.ventana) & SDL_WINDOW_FULLSCREEN ? 0 : ui::BARRA_VENTANA_ALTO;
}

void dibujar_barra_ventana(App& a) {
    // Siempre: sigue un arrastre en curso y ejecuta lo que pidieron los botones.
    const ui::AccionBarra acc = ui::barra_frame(a.ventana, a.ventana_sin_borde);
    if (acc == ui::AccionBarra::MINIMIZAR) SDL_MinimizeWindow(a.ventana);
    if (acc == ui::AccionBarra::CERRAR) a.salir = true;
    if (alto_barra(a) == 0) return;
    // En coordenadas del contenido: la barra cae en la franja de arriba de la cuadrícula.
    a.gpu.nivel_ventana(NIVEL_BARRA);
    ui::barra_ventana(*a.g, a.entrada, "Lineage", -a.gpu.origen_x(), -a.gpu.origen_y(), a.gpu.grid_ancho());
    a.gpu.nivel_ventana(0);
}

void alternar_pantalla_completa(App& a) {
    a.cfg.pantalla_completa = !a.cfg.pantalla_completa;
    if (a.ventana) SDL_SetWindowFullscreen(a.ventana, a.cfg.pantalla_completa);
}

}  // namespace lin
