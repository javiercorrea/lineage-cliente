#include <algorithm>
#include "ui/ui.h"

namespace lin::ui {

bool sobre(const EntradaFrame& e, int x, int y, int w, int h) { return e.mx >= x && e.my >= y && e.mx < x + w && e.my < y + h; }

void imagen(Graficos& g, u32 n, int x, int y, Capa capa) { g.gpu().dibujar(g.img(n), x, y, clave_orden(capa, 0)); }

bool boton(Graficos& g, const EntradaFrame& e, u32 n, int x, int y) {
    RegionAtlas r = g.img(n);
    bool encima = r.valida() && sobre(e, x + r.ox, y + r.oy, r.w, r.h);
    if (encima) {
        RegionAtlas r2 = g.img(n + 1);
        if (r2.valida()) r = r2;
    }
    g.gpu().dibujar(r, x, y, clave_orden(CAPA_UI, 1));
    return encima && e.clic;
}

bool caja_texto(Graficos& g, const EntradaFrame& e, std::string& valor, int x, int y, int ancho, bool foco,
                bool es_clave, size_t maximo) {
    if (foco) {
        for (int i = 0; i < e.ntexto; ++i) {
            char c = e.texto[i];
            if (c >= 0x20 && c < 0x7F && valor.size() < maximo) valor.push_back(c);
        }
        if (e.retroceso && !valor.empty()) valor.pop_back();
    }
    char buf[64];
    size_t n = valor.size() < sizeof buf - 1 ? valor.size() : sizeof buf - 1;
    for (size_t i = 0; i < n; ++i) buf[i] = es_clave ? '*' : valor[i];
    std::string_view t(buf, n);
    int w = g.texto(t, x, y, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
    // Cursor parpadeante como en el original.
    if (foco && (e.ahora_ms / 500) % 2 == 0 && w + 1 < ancho)
        g.gpu().rectangulo(x + w + 1, y, 1, ALTO_GLIFO, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
    return foco && e.enter;
}

bool boton_texto(Graficos& g, const EntradaFrame& e, std::string_view t, int x, int y, int w, int h) {
    bool encima = sobre(e, x, y, w, h);
    u64 k = clave_orden(CAPA_UI, 100);  // por encima de los paneles
    g.gpu().rectangulo(x, y, w, h, k, encima ? rgba(90, 70, 40, 230) : rgba(40, 30, 20, 210));
    g.gpu().rectangulo(x, y, w, 1, k + (1u << 24), rgba(160, 130, 80));
    g.gpu().rectangulo(x, y + h - 1, w, 1, k + (1u << 24), rgba(160, 130, 80));
    int tw = ancho_texto(t.data(), t.size());
    g.texto(t, x + (w - tw) / 2, y + (h - ALTO_GLIFO) / 2, clave_orden(CAPA_UI_TEXTO, 100), rgba(255, 240, 200));
    return encima && e.clic;
}

void recuadro(Graficos& g, int x, int y, int w, int h, u8 opacidad) {
    const u32 borde = rgba(150, 130, 90);
    const u64 k = clave_orden(CAPA_UI, 2);
    g.gpu().rectangulo(x, y, w, h, k, rgba(10, 8, 6, opacidad));
    g.gpu().rectangulo(x, y, w, 1, k + (1ull << 24), borde);
    g.gpu().rectangulo(x, y + h - 1, w, 1, k + (1ull << 24), borde);
    g.gpu().rectangulo(x, y, 1, h, k + (1ull << 24), borde);
    g.gpu().rectangulo(x + w - 1, y, 1, h, k + (1ull << 24), borde);
}

bool campo(Graficos& g, const EntradaFrame& e, std::string& valor, int x, int y, int ancho, bool foco, bool es_clave,
           size_t maximo) {
    g.gpu().rectangulo(x - 4, y - 4, ancho + 8, ALTO_GLIFO + 8, clave_orden(CAPA_UI, 3),
                       foco ? rgba(60, 48, 30, 255) : rgba(30, 24, 16, 255));
    g.gpu().rectangulo(x - 4, y + ALTO_GLIFO + 3, ancho + 8, 1, clave_orden(CAPA_UI, 4), rgba(150, 130, 90));
    return caja_texto(g, e, valor, x, y, ancho, foco, es_clave, maximo);
}

bool desplegable(Graficos& g, EntradaFrame& e, std::span<const std::string> opciones, size_t& elegido, bool& abierto, int x,
                 int y, int ancho) {
    const int fila = ALTO_GLIFO + 8;
    const bool sobre_caja = sobre(e, x - 4, y - 4, ancho + 8, fila);
    g.gpu().rectangulo(x - 4, y - 4, ancho + 8, fila, clave_orden(CAPA_UI, 3), abierto || sobre_caja ? rgba(60, 48, 30, 255) : rgba(30, 24, 16, 255));
    g.gpu().rectangulo(x - 4, y + ALTO_GLIFO + 3, ancho + 8, 1, clave_orden(CAPA_UI, 4), rgba(150, 130, 90));
    if (elegido < opciones.size()) g.texto(opciones[elegido], x, y, clave_orden(CAPA_UI_TEXTO, 0), COLOR_TEXTO);
    g.texto("v", x + ancho - 6, y, clave_orden(CAPA_UI_TEXTO, 0), rgba(214, 190, 140));
    bool cambio = false;
    if (abierto) {
        // La lista va en un nivel de ventana propio, encima de todo: si no, el texto de los
        // controles de abajo (que va en CAPA_UI_TEXTO) se vería a través de ella.
        const u32 nivel_antes = g.gpu().nivel_ventana();
        g.gpu().nivel_ventana(31);
        const u64 kf = clave_orden(CAPA_UI, 200), kt = clave_orden(CAPA_UI_TEXTO, 200);
        const int ly = y - 4 + fila, lh = fila * static_cast<int>(opciones.size());
        g.gpu().rectangulo(x - 5, ly - 1, ancho + 10, lh + 2, kf, rgba(150, 130, 90));
        for (size_t i = 0; i < opciones.size(); ++i) {
            const int fy = ly + fila * static_cast<int>(i);
            const bool encima = sobre(e, x - 4, fy, ancho + 8, fila);
            g.gpu().rectangulo(x - 4, fy, ancho + 8, fila, kf + (1ull << 24),
                               encima ? rgba(90, 70, 40, 255) : i == elegido ? rgba(50, 40, 26, 255) : rgba(20, 16, 11, 255));
            g.texto(opciones[i], x, fy + 4, kt, COLOR_TEXTO);
            if (encima && e.clic) {
                cambio = i != elegido;
                elegido = i;
            }
        }
        g.gpu().nivel_ventana(nivel_antes);
        if (e.clic || e.escape) abierto = false;  // eligió, o tocó afuera
        e.clic = e.escape = false;
    } else if (sobre_caja && e.clic) {
        abierto = true;
        e.clic = false;
    }
    return cambio;
}

void texto_centrado(Graficos& g, std::string_view t, int x, int y, int w, u32 color) {
    g.texto(t, static_cast<float>(x + (w - ancho_texto(t.data(), t.size())) / 2), static_cast<float>(y),
            clave_orden(CAPA_UI_TEXTO, 0), color);
}

bool ventana(Graficos& g, const EntradaFrame& e, bool simple, int x, int y, std::string_view titulo, u8 opacidad) {
    const u8 marco = static_cast<u8>(std::min(255, opacidad + 60));
    constexpr u32 IMG_MARCO = 4131;  // PENDIENTE: las ventanas propias del original (estado, inventario)
    const int w = VENTANA_ANCHO, h = VENTANA_ALTO;
    if (simple) {
        const u32 borde = rgba(150, 130, 90);
        g.gpu().rectangulo(x, y, w, h, clave_orden(CAPA_UI, 10), rgba(10, 8, 6, opacidad));
        g.gpu().rectangulo(x + 1, y + 1, w - 2, ALTO_TITULO - 2, clave_orden(CAPA_UI, 10) + (1ull << 24), rgba(40, 32, 22, marco));
        g.gpu().rectangulo(x, y, w, 1, clave_orden(CAPA_UI, 11), borde);
        g.gpu().rectangulo(x, y + h - 1, w, 1, clave_orden(CAPA_UI, 11), borde);
        g.gpu().rectangulo(x, y, 1, h, clave_orden(CAPA_UI, 11), borde);
        g.gpu().rectangulo(x + w - 1, y, 1, h, clave_orden(CAPA_UI, 11), borde);
        g.texto(titulo, x + 8, y + 6, clave_orden(CAPA_UI_TEXTO, 10), rgba(214, 190, 140));
        g.texto("[x]", x + w - 26, y + 6, clave_orden(CAPA_UI_TEXTO, 10), rgba(255, 255, 254));
    } else {
        // El marco original tiene el interior transparente: se rellena detrás.
        g.gpu().rectangulo(x + 6, y + 24, w - 12, h - 30, clave_orden(CAPA_UI, 10), rgba(14, 12, 10, opacidad));
        g.gpu().dibujar(g.img(IMG_MARCO), x, y, clave_orden(CAPA_UI, 11), rgba(255, 255, 255, marco));
        g.texto(titulo, x + 12, y + 8, clave_orden(CAPA_UI_TEXTO, 10), rgba(214, 190, 140));
    }
    // Cerrar: la X del marco (o el "[x]"), arriba a la derecha.
    return e.clic && sobre(e, x + w - 28, y + 2, 26, 20);
}

BotonesBarra botones_barra(int ancho) {
    constexpr int LADO = 18, M = 4;
#ifdef __APPLE__
    return {M, M + LADO + M, LADO};  // [cerrar][minimizar], como en macOS
#else
    // MULTIPLATAFORMA: sin probar en Linux ni Windows.
    return {ancho - LADO - M, ancho - 2 * LADO - 2 * M, LADO};
#endif
}

void barra_ventana(Graficos& g, const EntradaFrame& e, std::string_view titulo, int x0, int y0, int ancho) {
    g.gpu().rectangulo(x0, y0, ancho, BARRA_VENTANA_ALTO, clave_orden(CAPA_UI, 0), rgba(44, 34, 24));
    texto_centrado(g, titulo, x0, y0 + 7, ancho, COLOR_TITULO);
    const BotonesBarra b = botones_barra(ancho);
    // Solo se dibujan (el resultado de los clics lo decide barra_evento).
    boton_texto(g, e, "-", x0 + b.minimizar, y0 + 4, b.lado, b.lado);
    boton_texto(g, e, "x", x0 + b.cerrar, y0 + 4, b.lado, b.lado);
}

namespace {

constexpr int BORDE_GRUESO = 6;  // puntos
constexpr int TAM_MIN_W = 320, TAM_MIN_H = 240;

bool en_rango(int v, int inicio, int lado) { return v >= inicio && v < inicio + lado; }

// Botón de la barra bajo (gx, gy) de la cuadrícula: 1 minimizar, 2 cerrar, 0 la barra sin botón, -1 fuera.
int boton_bajo(const Gpu& gpu, int gx, int gy) {
    const int ancho = gpu.grid_ancho();
    if (gy < 0 || gy >= BARRA_VENTANA_ALTO || gx < 0 || gx >= ancho) return -1;
    const BotonesBarra b = botones_barra(ancho);
    if (en_rango(gx, b.cerrar, b.lado)) return 2;
    if (en_rango(gx, b.minimizar, b.lado)) return 1;
    return 0;
}

// Bordes de la ventana bajo (x, y) en puntos de la ventana.
int bordes_bajo(SDL_Window* v, float x, float y) {
    int ww = 0, wh = 0;
    SDL_GetWindowSize(v, &ww, &wh);
    int b = 0;
    if (x < BORDE_GRUESO) b |= BORDE_IZQ;
    if (x >= ww - BORDE_GRUESO) b |= BORDE_DER;
    if (y < BORDE_GRUESO) b |= BORDE_ARR;
    if (y >= wh - BORDE_GRUESO) b |= BORDE_ABA;
    return b;
}

void empezar_arrastre(SDL_Window* v, Barra& b, int bordes) {
    SDL_GetGlobalMouseState(&b.raton_x, &b.raton_y);
    SDL_GetWindowPosition(v, &b.vx, &b.vy);
    SDL_GetWindowSize(v, &b.vw, &b.vh);
    b.bordes = bordes;
    b.arrastrando = true;
}

}  // namespace

bool barra_evento(Gpu& gpu, SDL_Window* v, Barra& b, const SDL_Event& ev, bool activa) {
    if (!activa) return false;
    switch (ev.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (ev.button.button != SDL_BUTTON_LEFT) return false;
        int gx = 0, gy = 0;
        gpu.a_grilla(ev.button.x, ev.button.y, gx, gy);
        const int boton = boton_bajo(gpu, gx, gy);
        if (boton > 0) {  // los botones van primero (también sobre el borde superior)
            b.pulsada = true;
            b.boton = boton;
            return true;
        }
        const int bordes = b.redimensionable ? bordes_bajo(v, ev.button.x, ev.button.y) : 0;
        if (bordes) {
            b.pulsada = true;
            b.boton = 0;
            empezar_arrastre(v, b, bordes);
            return true;
        }
        if (boton == 0) {  // la barra sin botón: arrastra la ventana
            b.pulsada = true;
            b.boton = 0;
            empezar_arrastre(v, b, 0);
            return true;
        }
        return false;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (ev.button.button != SDL_BUTTON_LEFT || !b.pulsada) return false;
        int gx = 0, gy = 0;
        gpu.a_grilla(ev.button.x, ev.button.y, gx, gy);
        // Un botón cuenta si se soltó sobre el mismo que se presionó (y no se arrastró).
        if (!b.arrastrando && b.boton > 0 && boton_bajo(gpu, gx, gy) == b.boton)
            b.accion = b.boton == 1 ? AccionBarra::MINIMIZAR : AccionBarra::CERRAR;
        b.pulsada = false;
        b.arrastrando = false;
        b.boton = 0;
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION:
        return b.pulsada;  // lo que empezó en la barra no llega al juego
    default:
        return false;
    }
}

AccionBarra barra_frame(SDL_Window* v, Barra& b) {
    if (b.arrastrando) {
        float x = 0, y = 0;
        if (!(SDL_GetGlobalMouseState(&x, &y) & SDL_BUTTON_LMASK)) {
            b.arrastrando = false;  // soltado fuera de la ventana: el evento de subida cierra el resto
        } else {
            const int dx = static_cast<int>(x - b.raton_x), dy = static_cast<int>(y - b.raton_y);
            if (b.bordes == 0) {
                SDL_SetWindowPosition(v, b.vx + dx, b.vy + dy);
            } else {
                int x0 = b.vx, y0 = b.vy, w = b.vw, h = b.vh;
                if (b.bordes & BORDE_IZQ) {
                    w = std::max(TAM_MIN_W, b.vw - dx);
                    x0 = b.vx + b.vw - w;
                }
                if (b.bordes & BORDE_DER) w = std::max(TAM_MIN_W, b.vw + dx);
                if (b.bordes & BORDE_ARR) {
                    h = std::max(TAM_MIN_H, b.vh - dy);
                    y0 = b.vy + b.vh - h;
                }
                if (b.bordes & BORDE_ABA) h = std::max(TAM_MIN_H, b.vh + dy);
                SDL_SetWindowPosition(v, x0, y0);
                SDL_SetWindowSize(v, w, h);
            }
        }
    }
    const AccionBarra acc = b.accion;
    b.accion = AccionBarra::NINGUNA;
    return acc;
}

}  // namespace lin::ui
