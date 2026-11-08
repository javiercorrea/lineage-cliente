// Paso 6.6: polymorph. El sprite nuevo llega con S_ChangeShape (mundo.cpp); aquí está el diálogo
// para elegir el monstruo, que se abre:
//  - al usar un pergamino de polymorph (uso 16): se responde con C_UseItem + el nombre;
//  - con la lista "monlist" de Shape Change (S_ShowHTML): se responde con C_NPCAction.
// Como en el original, se escribe el nombre del monstruo (en inglés, como en PolyTable del
// servidor). Abajo se sugieren los de polymorph.tbl del cliente que contienen lo escrito; clic en
// uno lo elige. "none" (botón Undo) vuelve a la forma normal.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "app/app.h"
#include <SDL3/SDL.h>

namespace lin {

namespace {

constexpr int ANCHO = 280, ALTO = 226, SUGERENCIAS = 9;
constexpr size_t MAX_NOMBRE = 40;

bool contiene(std::string_view t, std::string_view buscado) {
    auto minuscula = [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c); };
    if (buscado.empty()) return true;
    for (size_t i = 0; i + buscado.size() <= t.size(); ++i) {
        size_t k = 0;
        while (k < buscado.size() && minuscula(t[i + k]) == minuscula(buscado[k])) ++k;
        if (k == buscado.size()) return true;
    }
    return false;
}

void elegir(App& a, std::string_view nombre) {
    if (!nombre.empty()) {
        if (a.forma_item) enviar(a, cmd::usar_item_texto(a.op, a.forma_item, nombre));
        else enviar(a, cmd::accion_npc(a.op, a.mundo.jugador.id, nombre));
    }
    a.ventana_forma = false;
}

}  // namespace

bool cargar_formas(App& a) {
    a.formas.clear();
    // datos/formas.txt: "<nivel><TAB><nombre>" por línea, ya ordenado por nivel.
    const char* base = SDL_GetBasePath();
    std::ifstream in(std::string(base ? base : "") + "datos/formas.txt");
    if (!in) in.open("datos/formas.txt");
    for (std::string linea; std::getline(in, linea);) {
        const size_t tab = linea.find('\t');
        if (tab == std::string::npos) continue;
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        a.formas.push_back({std::atoi(linea.substr(0, tab).c_str()), linea.substr(tab + 1)});
    }
    if (!a.formas.empty()) return true;
    // Sin datos/formas.txt: los nombres de polymorph.tbl del juego, sin nivel.
    LIN_LOG("Aviso: no se encontró datos/formas.txt: la lista de polymorph sale de polymorph.tbl, sin niveles");
    // polymorph.tbl: cantidad y luego "<gfx> <n> \"<nombre>\" ; <nombre coreano>".
    std::vector<u8> d;
    if (!a.rec.texto("polymorph.tbl", d)) return false;
    std::string_view t(reinterpret_cast<const char*>(d.data()), d.size());
    size_t p = 0;
    while ((p = t.find('"', p)) != std::string_view::npos) {
        size_t q = t.find('"', p + 1);
        if (q == std::string_view::npos) break;
        a.formas.push_back({0, std::string(t.substr(p + 1, q - p - 1))});
        p = t.find('\n', q);
        if (p == std::string_view::npos) break;
    }
    return !a.formas.empty();
}

void abrir_forma(App& a, ObjId pergamino) {
    a.ventana_forma = true;
    a.forma_item = pergamino;
    a.forma_texto.clear();
    a.scroll.forma_pos = UINT32_MAX;  // la barra lo acota: abre en las formas de más nivel
}

void eventos_forma(App& a, const Evento& e) {
    if (e.tipo == Ev::HTML && e.s[0] == "monlist" && e.id == a.mundo.jugador.id) abrir_forma(a, 0);
}

void dibujar_forma(App& a) {
    if (!a.ventana_forma) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x0 = (a.gpu.ancho() - ANCHO) / 2, y0 = std::max(20, a.gpu.alto() / 2 - ALTO);
    const u64 kf = clave_orden(CAPA_UI, 90), kt = clave_orden(CAPA_UI_TEXTO, 90);
    g.gpu().rectangulo(x0, y0, ANCHO, ALTO, kf, rgba(15, 12, 10, 235));
    g.gpu().rectangulo(x0, y0, ANCHO, 1, kf + (1u << 24), rgba(160, 130, 80));
    g.gpu().rectangulo(x0, y0 + ALTO - 1, ANCHO, 1, kf + (1u << 24), rgba(160, 130, 80));
    g.texto(a.forma_item ? "Polymorph: monster name" : "Shape Change: monster name", static_cast<float>(x0 + 10),
            static_cast<float>(y0 + 8), kt, rgba(214, 190, 140));
    g.gpu().rectangulo(x0 + 10, y0 + 26, ANCHO - 20, ALTO_GLIFO + 6, kf + (1u << 24), rgba(40, 32, 25, 255));
    bool enter = ui::caja_texto(g, e, a.forma_texto, x0 + 14, y0 + 29, ANCHO - 28, true, false, MAX_NOMBRE);

    // Sugerencias: las que contienen lo escrito y el nivel permite (L1PolyScroll: min_level <= nivel; un
    // GM puede escribir cualquier otra). Van por nivel; al cambiar lo escrito se ven las de más nivel.
    const int nivel = a.mundo.jugador.nivel;
    auto disponible = [&](const App::Forma& f) { return f.nivel <= nivel && contiene(f.nombre, a.forma_texto); };
    u32 total = 0;
    for (const App::Forma& f : a.formas)
        if (disponible(f)) ++total;
    if (a.scroll.forma_filtro != a.forma_texto) {
        a.scroll.forma_filtro = a.forma_texto;
        a.scroll.forma_pos = UINT32_MAX;
    }
    const int alto_fila = ALTO_GLIFO + 2;
    barra_scroll_ventana(a, a.scroll.forma, x0 + ANCHO - 22, y0 + 50, SUGERENCIAS * alto_fila, total, SUGERENCIAS,
                         a.scroll.forma_pos, kf + (2u << 24), sobre_forma(a));
    u32 indice = 0;
    for (const App::Forma& f : a.formas) {
        if (!disponible(f)) continue;
        const u32 k = indice++;
        if (k < a.scroll.forma_pos || k >= a.scroll.forma_pos + SUGERENCIAS) continue;
        const int y = y0 + 50 + static_cast<int>(k - a.scroll.forma_pos) * alto_fila;
        bool encima = ui::sobre(e, x0 + 10, y - 1, ANCHO - 36, alto_fila);
        if (encima) g.gpu().rectangulo(x0 + 10, y - 1, ANCHO - 36, alto_fila, kf + (1u << 24), rgba(60, 50, 35, 255));
        // Con su nivel, que es el orden de la lista: "Lv 52  death knight". Sin nivel, solo el nombre.
        char etiqueta[96];
        if (f.nivel > 0) std::snprintf(etiqueta, sizeof etiqueta, "Lv %d  %s", f.nivel, f.nombre.c_str());
        else std::snprintf(etiqueta, sizeof etiqueta, "%s", f.nombre.c_str());
        g.texto(etiqueta, static_cast<float>(x0 + 14), static_cast<float>(y), kt, rgba(200, 220, 255));
        if (encima && e.clic) a.forma_texto = f.nombre;
    }
    const int yb = y0 + ALTO - 26;
    bool ok = ui::boton_texto(g, e, "OK", x0 + 10, yb, 80, 18);
    bool deshacer = a.forma_item && ui::boton_texto(g, e, "Undo", x0 + 100, yb, 80, 18);
    bool cancelar = ui::boton_texto(g, e, "Cancel", x0 + ANCHO - 90, yb, 80, 18) || e.escape;
    std::string elegido = a.forma_texto;
    // El diálogo tiene el teclado: lo escrito y Enter/Escape no llegan al chat.
    e.ntexto = 0;
    e.enter = e.escape = e.retroceso = false;
    if (cancelar) a.ventana_forma = false;
    else if (deshacer) elegir(a, "none");
    else if (ok || enter) elegir(a, elegido);
}

bool sobre_forma(const App& a) {
    if (!a.ventana_forma) return false;
    const int x0 = (a.gpu.ancho() - ANCHO) / 2, y0 = std::max(20, a.gpu.alto() / 2 - ALTO);
    return ui::sobre(a.entrada, x0, y0, ANCHO, ALTO);
}

}  // namespace lin
