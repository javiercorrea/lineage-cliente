// Tablón de los pueblos (L1BoardInstance; gfx 1544, 1546, 1557, 1559, 2205, 2207). Un clic camina
// hasta él y manda C_Board (no C_NPCTalk, que el servidor no responde para el tablón). S_Board trae
// una página de hasta 8 temas (los más nuevos primero); "Next" pide la siguiente con el último tema
// visto (C_BoardBack). Un clic en un tema lo lee (C_BoardRead / S_BoardRead). "Write" escribe uno
// (C_BoardWrite: 300 adena; el servidor los cobra sin revisar que alcancen, así que se revisa aquí)
// y el autor puede borrar el suyo (C_BoardDelete).
#include <algorithm>
#include <cstdio>
#include "app/app.h"

namespace lin {

namespace {

constexpr int ANCHO = 420, ALTO = 330, FILA = 32;
constexpr u32 COSTO_TEMA = 300;

int x0(const App& a) { return (a.gpu.ancho() - ANCHO) / 2; }
int y0() { return 50; }

u32 adena(const App& a) {
    const TablaItems& t = a.mundo.inventario.items;
    for (Slot s = 0; s < t.tamano(); ++s)
        if (nombre_item(a.mundo.inventario, s).starts_with("$4")) return t.col<I_CANTIDAD>()[s];  // $4 = Adena
    return 0;
}

}  // namespace

// Texto en varias líneas, cortado por palabras al ancho; devuelve la y siguiente.
int texto_ajustado(Graficos& g, std::string_view t, int x, int y, int ancho, u64 clave, u32 color, int max_lineas) {
    for (int lineas = 0; !t.empty() && lineas < max_lineas; ++lineas) {
        size_t corte = t.size();
        while (corte > 0 && ancho_texto(t.data(), corte) > ancho) {
            const size_t esp = t.rfind(' ', corte - 1);
            corte = esp == std::string_view::npos || esp == 0 ? corte - 1 : esp;
        }
        if (corte == 0) corte = 1;
        g.texto(t.substr(0, corte), static_cast<float>(x), static_cast<float>(y), clave, color);
        t.remove_prefix(corte);
        while (!t.empty() && t.front() == ' ') t.remove_prefix(1);
        y += ALTO_GLIFO + 3;
    }
    return y;
}

// Sin espacios al principio ni al final (un destinatario con un espacio de más no se encuentra).
std::string_view sin_espacios(std::string_view s) {
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
    return s;
}

bool es_tablon(GfxId gfx) {
    return gfx == 1544 || gfx == 1546 || gfx == 1557 || gfx == 1559 || gfx == 2205 || gfx == 2207;
}

void eventos_tablon(App& a, const Evento& e) {
    Tablon& t = a.tablon;
    switch (e.tipo) {
    case Ev::TABLON:
        t.id = e.id;
        t.temas.clear();
        t.leyendo = 0;
        break;
    case Ev::TABLON_TEMA: t.temas.push_back({e.id, std::string(e.s[0]), std::string(e.s[1])}); break;
    case Ev::TABLON_LEER:
        t.leyendo = e.id;
        t.cabecera.assign(e.s[0]);
        t.texto.assign(e.s[1]);
        break;
    default: break;
    }
}

bool sobre_tablon(const App& a) { return a.tablon.id && ui::sobre(a.entrada, x0(a), y0(), ANCHO, ALTO); }

void dibujar_tablon(App& a) {
    Tablon& t = a.tablon;
    if (!t.id) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = x0(a), y = y0();
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 70), kf = clave_orden(CAPA_UI, 70);
    ui::recuadro(g, x, y, ANCHO, ALTO, 235);
    g.texto("Bulletin Board", static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    const int by = y + ALTO - 26;
    char txt[160];

    if (t.escribiendo) {
        std::snprintf(txt, sizeof txt, "New topic (%u adena)", COSTO_TEMA);
        g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 30), kt, rgba(170, 160, 140));
        g.texto("Title", static_cast<float>(x + 10), static_cast<float>(y + 54), kt, ui::COLOR_TEXTO);
        g.texto("Text", static_cast<float>(x + 10), static_cast<float>(y + 84), kt, ui::COLOR_TEXTO);
        if (e.clic && ui::sobre(e, x + 60, y + 50, ANCHO - 70, 20)) t.foco = 0;
        if (e.clic && ui::sobre(e, x + 60, y + 80, ANCHO - 70, 20)) t.foco = 1;
        if (e.tab) t.foco = 1 - t.foco;
        g.gpu().rectangulo(x + 60, y + 50, ANCHO - 70, 20, kf, rgba(40, 32, 25, 255));
        g.gpu().rectangulo(x + 60, y + 80, ANCHO - 70, 20, kf, rgba(40, 32, 25, 255));
        ui::caja_texto(g, e, t.nuevo_titulo, x + 64, y + 53, ANCHO - 78, t.foco == 0, false, 40);
        ui::caja_texto(g, e, t.nuevo_texto, x + 64, y + 83, ANCHO - 78, t.foco == 1, false, 300);
        texto_ajustado(g, t.nuevo_texto, x + 10, y + 110, ANCHO - 20, kt, rgba(200, 190, 160), 10);
        const bool alcanza = adena(a) >= COSTO_TEMA;
        if (!alcanza) g.texto("Not enough adena.", static_cast<float>(x + 10), static_cast<float>(by + 3), kt, rgba(255, 120, 100));
        if (alcanza && !t.nuevo_titulo.empty() && ui::boton_texto(g, e, "Post", x + ANCHO - 170, by, 76, 20)) {
            enviar(a, cmd::escribir_tablon(a.op, t.id, sin_espacios(t.nuevo_titulo), sin_espacios(t.nuevo_texto)));
            enviar(a, cmd::tablon(a.op, Cli::BOARD, t.id));  // la página con el tema nuevo
            t.escribiendo = false;
            t.nuevo_titulo.clear();
            t.nuevo_texto.clear();
        }
        if (ui::boton_texto(g, e, "Cancel", x + ANCHO - 86, by, 76, 20) || e.escape) {
            t.escribiendo = false;
            e.escape = false;
        }
        e.ntexto = 0;  // el teclado es de la ventana
        e.enter = false;
        return;
    }

    if (t.leyendo) {
        g.texto(t.cabecera, static_cast<float>(x + 10), static_cast<float>(y + 30), kt, rgba(255, 220, 130));
        texto_ajustado(g, t.texto, x + 10, y + 54, ANCHO - 20, kt, ui::COLOR_TEXTO, 14);
        // El autor puede borrarlo (la cabecera empieza con su nombre).
        const std::string_view yo = nombre_personaje(a);
        if (!yo.empty() && std::string_view(t.cabecera).starts_with(std::string(yo) + " ") &&
            ui::boton_texto(g, e, "Delete", x + 10, by, 76, 20)) {
            enviar(a, cmd::tablon(a.op, Cli::BOARDDELETE, t.id, t.leyendo));
            enviar(a, cmd::tablon(a.op, Cli::BOARD, t.id));
        }
        if (ui::boton_texto(g, e, "Back", x + ANCHO - 86, by, 76, 20) || e.escape) {
            t.leyendo = 0;
            e.escape = false;
        }
        return;
    }

    // La página de temas.
    if (t.temas.empty()) g.texto("No topics.", static_cast<float>(x + 12), static_cast<float>(y + 36), kt, ui::COLOR_TEXTO);
    for (size_t i = 0; i < t.temas.size() && i < 8; ++i) {
        const TemaTablon& tm = t.temas[i];
        const int fy = y + 30 + static_cast<int>(i) * FILA;
        const bool encima = ui::sobre(e, x + 8, fy, ANCHO - 16, FILA - 2);
        g.gpu().rectangulo(x + 8, fy, ANCHO - 16, FILA - 2, kf, encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        g.texto(tm.titulo, static_cast<float>(x + 12), static_cast<float>(fy + 3), kt, ui::COLOR_TEXTO);
        g.texto(tm.autor, static_cast<float>(x + 12), static_cast<float>(fy + 16), kt, rgba(170, 160, 140));
        if (encima && e.clic) enviar(a, cmd::tablon(a.op, Cli::BOARDREAD, t.id, tm.id));
    }
    if (t.temas.size() >= 8 && ui::boton_texto(g, e, "Next", x + 10, by, 76, 20))
        enviar(a, cmd::tablon(a.op, Cli::BOARDNEXT, t.id, t.temas.back().id));
    if (ui::boton_texto(g, e, "Write", x + ANCHO - 170, by, 76, 20)) {
        t.escribiendo = true;
        t.foco = 0;
    }
    if (ui::boton_texto(g, e, "Close", x + ANCHO - 86, by, 76, 20) || e.escape) {
        t.id = 0;
        e.escape = false;
    }
}

}  // namespace lin
