// Explorador de recursos del juego original: muestra un gfx en las 8 direcciones parado sobre
// rombos de suelo (con una cruz en el centro de cada rombo) para validar los decodificadores y
// calibrar el ancla de los sprites. También muestra imágenes .img/.png sueltas.
//
// Uso: explorador --juego=<carpeta> [--gfx=N] [--accion=A] [--img=N | --png=N] [--captura=x.bmp]
// Teclas: ←/→ gfx, ↑/↓ acción, Esc salir.
#include <SDL3/SDL.h>
#include <string>
#include "plataforma/rutas.h"
#include "recursos/tabla_sprites.h"
#include "render/graficos.h"

using namespace lin;

int main(int argc, char** argv) {
    Config cfg = cargar_config();  // la carpeta del juego de settings.ini, o la de --juego=
    int gfx = 0, accion = ACC_QUIETO, img = -1, png = -1;
    std::string captura;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s.rfind("--juego=", 0) == 0) cfg.carpeta_juego = s.substr(8) + (s.back() == '/' ? "" : "/");
        else if (s.rfind("--gfx=", 0) == 0) gfx = std::atoi(s.c_str() + 6);
        else if (s.rfind("--accion=", 0) == 0) accion = std::atoi(s.c_str() + 9);
        else if (s.rfind("--img=", 0) == 0) img = std::atoi(s.c_str() + 6);
        else if (s.rfind("--png=", 0) == 0) png = std::atoi(s.c_str() + 6);
        else if (s.rfind("--captura=", 0) == 0) captura = s.substr(10);
    }
    Recursos rec;
    FuenteBitmap fuente;
    TablaSprites tabla;
    if (!rec.abrir(cfg.carpeta_juego) || !fuente.cargar(cfg.carpeta_juego)) {
        LIN_LOG("Carpeta del juego inválida: %s", cfg.carpeta_juego.c_str());
        return 1;
    }
    tabla.cargar_archivo("datos/sprite_table.txt");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* v = SDL_CreateWindow("Explorador", 1280, 960, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    Gpu gpu;
    if (!gpu.iniciar(v, 640, 480)) return 1;
    Graficos g(gpu, rec, fuente);
    bool salir = false;
    u64 inicio = SDL_GetTicks();
    for (int frame = 0; !salir; ++frame) {
        g.nuevo_frame();  // lo que trajo la precarga
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) salir = true;
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) salir = true;
                if (ev.key.key == SDLK_RIGHT) ++gfx;
                if (ev.key.key == SDLK_LEFT && gfx > 0) --gfx;
                if (ev.key.key == SDLK_UP) ++accion;
                if (ev.key.key == SDLK_DOWN && accion > 0) --accion;
            }
        }
        u64 ahora = SDL_GetTicks() - inicio;
        char t[128];
        if (img >= 0 || png >= 0) {
            gpu.dibujar(img >= 0 ? g.img(static_cast<u32>(img)) : g.png(static_cast<u32>(png)), 0, 0, clave_orden(CAPA_FONDO, 0));
        } else {
            const RangoAccion* r = tabla.accion(static_cast<GfxId>(gfx), static_cast<u8>(accion));
            for (int d = 0; d < 8; ++d) {
                int cx = 40 + (d % 4) * 150, cy = 120 + (d / 4) * 200;  // esquina sup. izq. de la celda
                // Rombo de referencia (celda de 48×24) y cruz en su centro.
                for (int fila = 0; fila < 24; ++fila) {
                    int ancho = fila < 12 ? 4 * (fila + 1) : 4 * (24 - fila);
                    gpu.rectangulo(cx + 24 - ancho / 2, cy + fila, ancho, 1, clave_orden(CAPA_SUELO, 0), rgba(60, 90, 60));
                }
                gpu.rectangulo(cx + 24, cy + 6, 1, 12, clave_orden(CAPA_EFECTOS, 0), rgba(255, 0, 0));
                gpu.rectangulo(cx + 18, cy + 12, 12, 1, clave_orden(CAPA_EFECTOS, 0), rgba(255, 0, 0));
                if (!r) continue;
                bool termino;
                const FrameAccion& f = tabla.frame_en(*r, ahora, termino);
                u16 sub = static_cast<u16>(f.sub + (r->direccional ? d : 0));
                gpu.dibujar(g.frame_sprite(tabla.gfx_archivos(static_cast<GfxId>(gfx)), sub, f.frame), cx, cy,
                            clave_orden(CAPA_MUNDO, 0));
                std::snprintf(t, sizeof t, "dir %d", d);
                g.texto(t, cx, cy + 30, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 0));
            }
            std::snprintf(t, sizeof t, "gfx %d (archivos %d)  accion %d  %s", gfx, tabla.gfx_archivos(static_cast<GfxId>(gfx)),
                          accion, r ? "" : "(sin esta accion)");
            g.texto(t, 4, 4, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
        }
        if (!captura.empty() && frame == 30) {
            gpu.pedir_captura(captura);
            salir = true;
        }
        gpu.presentar();
    }
    gpu.cerrar();
    SDL_DestroyWindow(v);
    SDL_Quit();
    return 0;
}
