// Ventana del juego: en ventana, sin borde del sistema y con la barra propia (título, minimizar y
// cerrar); a pantalla completa, sin barra. La barra va encima de todo, como los diálogos.
#include "app/app.h"
#include "render/graficos.h"
#include "ui/ui.h"

namespace lin {

namespace {

// Mismo nivel que los diálogos y avisos de pantalla_juego.cpp: la barra queda sobre las ventanas.
constexpr u32 NIVEL_BARRA = 31;

}  // namespace

int alto_barra(const App& a) { return a.cfg.pantalla_completa || !a.g ? 0 : ui::BARRA_VENTANA_ALTO; }

void dibujar_barra_ventana(App& a) {
    if (alto_barra(a) == 0) return;
    bool cerrar = false, minimizar = false;
    a.gpu.nivel_ventana(NIVEL_BARRA);
    ui::barra_ventana(*a.g, a.entrada, "Lineage", a.gpu.ancho(), cerrar, minimizar);
    a.gpu.nivel_ventana(0);
    if (minimizar) SDL_MinimizeWindow(a.ventana);
    if (cerrar) a.salir = true;
}

void alternar_pantalla_completa(App& a) {
    a.cfg.pantalla_completa = !a.cfg.pantalla_completa;
    if (a.ventana) SDL_SetWindowFullscreen(a.ventana, a.cfg.pantalla_completa);
}

SDL_HitTestResult zona_ventana_juego(SDL_Window* ventana, const SDL_Point* p, void* datos) {
    const App& a = *static_cast<const App*>(datos);
    if (alto_barra(a) == 0) return SDL_HITTEST_NORMAL;  // a pantalla completa, o antes de cargar la fuente
    return ui::prueba_barra(ventana, a.gpu, p, true);
}

}  // namespace lin
