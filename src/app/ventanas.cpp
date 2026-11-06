// Ventanas del juego: dónde está cada una, moverlas arrastrando la barra de título (la posición se
// guarda en settings.ini, "ventanas="), los botones para abrirlas y la ventana de opciones.
//  - Sin moverlas, cada una va en su lugar de siempre: inventario contra el borde derecho, estado y
//    hechizos a su izquierda (según cuáles estén abiertas) y el diálogo del NPC a la izquierda.
//  - Botones: con la UI original, los del panel (1005–1012.img: magia, estado, opciones,
//    inventario, cada uno con su versión resaltada); con la simple, botones de texto sobre la
//    línea de entrada del chat.
#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include "app/app.h"

namespace lin {

namespace {

constexpr std::string_view NOMBRES[] = {"estado", "inventario", "hechizos", "npc", "opciones", "salida", "social", "intercambio"};
static_assert(std::size(NOMBRES) == static_cast<size_t>(IdVentana::CANTIDAD));
static_assert(static_cast<int>(IdVentana::CANTIDAD) == 8, "App::orden_ventanas se inicializa con 0..7");
// Doce filas de ALTO_GLIFO + 12 px, más el título y el margen: 352 px. Con 276 (y luego 300) la última quedaba afuera.
constexpr int ANCHO_OPCIONES = 280, ALTO_OPCIONES = 456;
constexpr int ANCHO_SALIDA = 200, ALTO_SALIDA = 120;
constexpr int SEPARACION = 4, MARGEN = 7;
constexpr int PASO_LUGAR = 8;  // px entre los lugares que se prueban para una ventana que se abre

int indice(IdVentana v) { return static_cast<int>(v); }

void cargar_posiciones(App& a) {
    a.ventanas_cargadas = true;
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
        std::string_view v = valor_de_personaje(a.cfg.ventanas, NOMBRES[i]);
        size_t coma = v.find(',');
        if (v.empty() || coma == std::string_view::npos) continue;
        PosVentana& p = a.pos_ventanas[i];
        std::from_chars(v.data(), v.data() + coma, p.x);
        std::from_chars(v.data() + coma + 1, v.data() + v.size(), p.y);
        p.movida = true;
    }
}

void guardar_posiciones(App& a) {
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
        const PosVentana& p = a.pos_ventanas[i];
        if (p.movida) poner_valor_de_personaje(a.cfg.ventanas, NOMBRES[i], std::to_string(p.x) + ',' + std::to_string(p.y));
    }
    guardar_config(a.cfg);
}

// Lugar de siempre (sin moverla). Contra el borde derecho se apilan inventario, estado y hechizos:
// cada una a la izquierda de las anteriores que estén abiertas y en su lugar (una movida deja libre
// el suyo).
Rect por_defecto(const App& a, IdVentana v) {
    const int w = ui::VENTANA_ANCHO, h = ui::VENTANA_ALTO, y = ui::VENTANA_Y;
    auto ocupa = [&](bool abierta_, IdVentana o) {
        const PosVentana& p = a.pos_ventanas[indice(o)];
        return abierta_ && !p.movida && !p.colocada ? w + SEPARACION : 0;
    };
    const int x_inv = a.gpu.ancho() - w - MARGEN;
    const int x_estado = x_inv - ocupa(a.ventana_inventario, IdVentana::INVENTARIO);
    switch (v) {
    case IdVentana::INVENTARIO: return {x_inv, y, w, h};
    case IdVentana::ESTADO: return {x_estado, y, w, h};
    case IdVentana::HECHIZOS: return {x_estado - ocupa(a.ventana_estado, IdVentana::ESTADO), y, w, h};
    case IdVentana::NPC: return {MARGEN, y, w, h};
    case IdVentana::OPCIONES: return {(a.gpu.ancho() - ANCHO_OPCIONES) / 2, y + 40, ANCHO_OPCIONES, ALTO_OPCIONES};
    case IdVentana::SALIDA: return {(a.gpu.ancho() - ANCHO_SALIDA) / 2, a.gpu.alto() / 3, ANCHO_SALIDA, ALTO_SALIDA};
    case IdVentana::SOCIAL: return rect_social(a);
    case IdVentana::INTERCAMBIO: return rect_intercambio(a);
    default: return {0, y, w, h};
    }
}

bool abierta(const App& a, IdVentana v) {
    switch (v) {
    case IdVentana::ESTADO: return a.ventana_estado;
    case IdVentana::INVENTARIO: return a.ventana_inventario;
    case IdVentana::HECHIZOS: return a.ventana_hechizos;
    case IdVentana::NPC: return a.npc || a.tienda != TipoTienda::NINGUNA;
    case IdVentana::OPCIONES: return a.ventana_opciones;
    case IdVentana::SALIDA: return a.ventana_salida;
    case IdVentana::SOCIAL: return a.ventana_social;
    case IdVentana::INTERCAMBIO: return a.intercambio.abierto;
    default: return false;
    }
}

bool solapan(const Rect& a, const Rect& b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }

bool contiene_puntero(const App& a, IdVentana v) {
    if (!abierta(a, v)) return false;
    Rect r = rect_ventana(a, v);
    return ui::sobre(a.entrada, r.x, r.y, r.w, r.h);
}

// La de más arriba abierta bajo el puntero (índice), o -1.
int ventana_bajo_puntero(const App& a) {
    for (int i = indice(IdVentana::CANTIDAD) - 1; i >= 0; --i)
        if (contiene_puntero(a, static_cast<IdVentana>(a.orden_ventanas[i]))) return a.orden_ventanas[i];
    return -1;
}

void al_frente(App& a, IdVentana v) {
    u8* o = a.orden_ventanas;
    u8* fin = o + indice(IdVentana::CANTIDAD);
    std::rotate(std::find(o, fin, static_cast<u8>(v)), std::find(o, fin, static_cast<u8>(v)) + 1, fin);
}

// Lugar para una ventana que se abre sin haberla movido: el de siempre si no tapa a otra abierta;
// si no, el primero libre recorriendo de arriba a la derecha hacia abajo a la izquierda, sobre la
// barra de atajos. Si no hay ninguno, el de siempre (y queda adelante).
void colocar(App& a, IdVentana v) {
    PosVentana& p = a.pos_ventanas[indice(v)];
    p.colocada = false;
    if (p.movida) return;
    const Rect r = rect_ventana(a, v);
    auto libre = [&](const Rect& c) {
        for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
            const IdVentana o = static_cast<IdVentana>(i);
            if (o != v && abierta(a, o) && solapan(c, rect_ventana(a, o))) return false;
        }
        return true;
    };
    if (libre(r)) return;
    const int abajo = a.barra_y > r.h ? a.barra_y - SEPARACION : a.gpu.alto();
    for (int y = ui::VENTANA_Y; y + r.h <= abajo; y += PASO_LUGAR) {
        for (int x = a.gpu.ancho() - r.w - MARGEN; x >= MARGEN; x -= PASO_LUGAR) {
            if (!libre({x, y, r.w, r.h})) continue;
            p.x = x;
            p.y = y;
            p.colocada = true;
            return;
        }
    }
}

}  // namespace

void actualizar_ventanas(App& a) {
    if (!a.ventanas_cargadas) cargar_posiciones(a);
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
        const IdVentana v = static_cast<IdVentana>(i);
        const bool ab = abierta(a, v);
        if (ab && !a.ventana_abierta_antes[i]) {
            colocar(a, v);
            al_frente(a, v);
        }
        if (!ab) a.pos_ventanas[i].colocada = false;
        a.ventana_abierta_antes[i] = ab;
    }
    a.ventana_arriba = ventana_bajo_puntero(a);
    if (a.entrada.pulsado && a.ventana_arriba >= 0) al_frente(a, static_cast<IdVentana>(a.ventana_arriba));
}

Rect rect_ventana(const App& a, IdVentana v) {
    Rect r = por_defecto(a, v);
    const PosVentana& p = a.pos_ventanas[indice(v)];
    if (p.movida || p.colocada) {
        // Siempre con la barra de título dentro de la pantalla (la ventana puede haber quedado fuera
        // al achicar la ventana del sistema o cambiar el zoom).
        r.x = std::clamp(p.x, 40 - r.w, a.gpu.ancho() - 40);
        r.y = std::clamp(p.y, 0, a.gpu.alto() - ui::ALTO_TITULO);
    }
    return r;
}

bool sobre_ventana(const App& a, IdVentana v) { return ventana_bajo_puntero(a) == indice(v); }

bool sobre_alguna_ventana(const App& a) { return ventana_bajo_puntero(a) >= 0; }

void arrastrar_ventana(App& a, IdVentana v) {
    if (!a.ventanas_cargadas) cargar_posiciones(a);
    const EntradaFrame& e = a.entrada;
    Rect r = rect_ventana(a, v);
    PosVentana& p = a.pos_ventanas[indice(v)];
    // Se toma de la barra de título, salvo el botón de cerrar (los últimos 30 px).
    if (e.pulsado && a.ventana_arrastrada < 0 && ui::sobre(e, r.x, r.y, r.w - 30, ui::ALTO_TITULO)) {
        a.ventana_arrastrada = indice(v);
        a.arrastre_dx = e.mx - r.x;
        a.arrastre_dy = e.my - r.y;
        a.clic_consumido = true;  // no camina
    }
    if (a.ventana_arrastrada != indice(v)) return;
    if (e.presionado) {
        p.x = e.mx - a.arrastre_dx;
        p.y = e.my - a.arrastre_dy;
        p.movida = true;
        p.colocada = false;
    } else {
        a.ventana_arrastrada = -1;
        guardar_posiciones(a);
    }
}

// La de las opciones; opaca para la ventana que se está dibujando si tapa a otra (se leería el
// texto de la de abajo a través de su fondo).
u8 opacidad_ventanas(const App& a) {
    return a.ventana_sobre_otra ? 255 : static_cast<u8>(std::clamp(a.cfg.opacidad_ventanas, 60, 255));
}

bool tapa_a_otra(const App& a, IdVentana v) {
    const Rect r = rect_ventana(a, v);
    for (int i = 0; i < indice(IdVentana::CANTIDAD) && a.orden_ventanas[i] != indice(v); ++i) {
        const IdVentana o = static_cast<IdVentana>(a.orden_ventanas[i]);
        if (abierta(a, o) && solapan(r, rect_ventana(a, o))) return true;
    }
    return false;
}

// Botón con imagen N (normal) y N+1 (resaltado): ui::boton, con la ayuda arriba al pasar el puntero.
static bool boton_panel(App& a, u32 n, int x, int y, std::string_view ayuda) {
    Graficos& g = *a.g;
    bool encima = ui::sobre(a.entrada, x, y, g.img(n).w, g.img(n).h);
    if (encima) {
        int w = ancho_texto(ayuda.data(), ayuda.size());
        g.gpu().rectangulo(x - 3, y - 19, w + 6, ALTO_GLIFO + 4, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto(ayuda, static_cast<float>(x), static_cast<float>(y - 17), clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
    }
    return ui::boton(g, a.entrada, n, x, y);
}

void dibujar_botones_panel(App& a, int x, int y) {
    // Panel original: arriba del panel derecho (1019.img), en la franja sobre las teclas F.
    if (boton_panel(a, 1005, x, y, "Magic (Alt+S)")) a.ventana_hechizos = !a.ventana_hechizos;
    if (boton_panel(a, 1007, x + 22, y, "Status (Alt+C)")) a.ventana_estado = !a.ventana_estado;
    if (boton_panel(a, 1011, x + 40, y, "Inventory (Alt+I)")) a.ventana_inventario = !a.ventana_inventario;
    if (boton_panel(a, 1009, x + 58, y, "Options")) a.ventana_opciones = !a.ventana_opciones;
    if (boton_panel(a, 1013, x + 74, y, "Restart / Quit")) a.ventana_salida = !a.ventana_salida;
    if (boton_panel(a, 1015, x + 94, y, "Party, trade, clan")) a.ventana_social = !a.ventana_social;
}

// UI simple: 6 botones en 2 columnas × 3 filas, a la izquierda del minimapa.
static constexpr std::string_view BOTONES_INF[6] = {"Magic", "Status", "Items", "Social", "Options", "Exit"};
static bool* boton_inf_abierta(App& a, int i) {
    switch (i) {
    case 0: return &a.ventana_hechizos;
    case 1: return &a.ventana_estado;
    case 2: return &a.ventana_inventario;
    case 3: return &a.ventana_social;
    case 4: return &a.ventana_opciones;
    default: return &a.ventana_salida;
    }
}
static int col_botones_inf() {
    int w = 0;
    for (std::string_view t : BOTONES_INF) w = std::max(w, ancho_texto(t.data(), t.size()) + 10);
    return w;
}

int ancho_botones_texto() { return 2 * col_botones_inf() + 4; }  // ancho del bloque de 2 columnas

// Dibuja el bloque de botones con su borde derecho en x_der y su tope en y_top. Devuelve el borde izq.
int dibujar_botones_texto(App& a, int x_der, int y_top) {
    const int cw = col_botones_inf(), fh = ALTO_GLIFO + 4;
    const int x0 = x_der - (2 * cw + 4);
    for (int i = 0; i < 6; ++i) {
        const int bx = x0 + (i / 3) * (cw + 4), by = y_top + (i % 3) * (fh + 2);
        if (ui::boton_texto(*a.g, a.entrada, BOTONES_INF[i], bx, by, cw, fh)) {
            bool* abierta = boton_inf_abierta(a, i);
            *abierta = !*abierta;
        }
    }
    return x0;
}

// Recuadro con título y "[x]" para las ventanas chicas (opciones, salida). Devuelve true al cerrar.
static bool recuadro(App& a, Rect r, std::string_view titulo) {
    Graficos& g = *a.g;
    const u8 op = opacidad_ventanas(a);
    g.gpu().rectangulo(r.x, r.y, r.w, r.h, clave_orden(CAPA_UI, 10), rgba(10, 8, 6, op));
    g.gpu().rectangulo(r.x + 1, r.y + 1, r.w - 2, ui::ALTO_TITULO - 2, clave_orden(CAPA_UI, 11),
                       rgba(40, 32, 22, static_cast<u8>(std::min(255, op + 60))));
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    g.texto(titulo, static_cast<float>(r.x + 8), static_cast<float>(r.y + 6), kt, rgba(214, 190, 140));
    g.texto("[x]", static_cast<float>(r.x + r.w - 26), static_cast<float>(r.y + 6), kt, rgba(255, 255, 254));
    return (a.entrada.clic && ui::sobre(a.entrada, r.x + r.w - 28, r.y + 2, 26, 20)) || a.entrada.escape;
}

void dibujar_opciones(App& a) {
    if (!a.ventana_opciones) return;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::OPCIONES);
    Rect r = rect_ventana(a, IdVentana::OPCIONES);
    // ui::ventana tiene el alto fijo de las demás: aquí se dibuja un recuadro propio del alto justo.
    if (recuadro(a, r, "Options")) a.ventana_opciones = false;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);

    // Una fila por opción: el nombre y un botón con el valor (clic lo cambia).
    char t[48];
    int y = r.y + ui::ALTO_TITULO + 8;
    auto fila = [&](std::string_view nombre, std::string_view valor) {
        g.texto(nombre, static_cast<float>(r.x + 10), static_cast<float>(y + 3), kt, rgba(230, 220, 190));
        // El botón de valor tiene 120 px: "Enter to type" y sus parecidos no se salen; la etiqueta, a la izquierda.
        bool clic = ui::boton_texto(g, e, valor, r.x + r.w - 130, y, 120, ALTO_GLIFO + 6);
        y += ALTO_GLIFO + 12;
        return clic;
    };
    bool cambio = false;
    if (fila("Interface (F2)", a.cfg.ui_simple ? "Simple" : "Original")) a.cfg.ui_simple = !a.cfg.ui_simple, cambio = true;
    if (fila("Camera (F4)", a.cfg.camara_a_saltos ? "Steps" : "Smooth")) a.cfg.camara_a_saltos = !a.cfg.camara_a_saltos, cambio = true;
    if (fila("Shade in shadows", a.cfg.sombra_personajes ? "On" : "Off"))
        a.cfg.sombra_personajes = !a.cfg.sombra_personajes, cambio = true;
    const int opacidad = std::clamp(a.cfg.opacidad_ventanas, 60, 255);  // la configurada, no la de esta ventana
    std::snprintf(t, sizeof t, "%d%%", opacidad * 100 / 255);
    if (fila("Window opacity", t)) {  // 40 % -> 55 % -> 70 % -> 85 % -> 100 % -> 40 %
        int o = opacidad + 38;
        a.cfg.opacidad_ventanas = o > 255 ? 102 : o;
        cambio = true;
    }
    if (fila("Chat", a.cfg.chat_con_enter ? "Enter to type" : "Always typing")) {
        a.cfg.chat_con_enter = !a.cfg.chat_con_enter;
        a.escribiendo_chat = false;
        cambio = true;
    }
    if (fila("HP/MP bar over char", a.cfg.barra_personaje ? "On" : "Off"))
        a.cfg.barra_personaje = !a.cfg.barra_personaje, cambio = true;
    if (fila("Damage numbers", a.cfg.texto_dano ? "On" : "Off"))
        a.cfg.texto_dano = !a.cfg.texto_dano, cambio = true;
    if (fila("Attack compensation", a.cfg.compensar_ataque ? "On" : "Off"))
        a.cfg.compensar_ataque = !a.cfg.compensar_ataque, cambio = true;
    if (fila("Night and dark dungeons", a.cfg.efecto_luz ? "On" : "Off")) a.cfg.efecto_luz = !a.cfg.efecto_luz, cambio = true;
    if (fila("Underwater tint", a.cfg.efecto_agua_tinte ? "On" : "Off")) a.cfg.efecto_agua_tinte = !a.cfg.efecto_agua_tinte, cambio = true;
    if (fila("Underwater wave", a.cfg.efecto_agua_ola ? "On" : "Off")) a.cfg.efecto_agua_ola = !a.cfg.efecto_agua_ola, cambio = true;
    if (fila("Elf vision", a.cfg.efecto_infravision ? "On" : "Off")) a.cfg.efecto_infravision = !a.cfg.efecto_infravision, cambio = true;
    std::snprintf(t, sizeof t, a.cfg.zoom ? "x%d" : "Auto", a.cfg.zoom);
    if (fila("Zoom (Ctrl +/-)", t)) {  // Auto -> x1 -> x2 ... -> máxima -> Auto
        a.cfg.zoom = a.cfg.zoom >= a.gpu.escala_maxima() ? 0 : a.cfg.zoom + 1;
        cambio = true;
    }
    // Volúmenes: 0 -> 20 -> ... -> 100 -> 0 (Off).
    auto volumen = [&](std::string_view nombre, int& v) {
        std::snprintf(t, sizeof t, v ? "%d%%" : "Off", v);
        if (fila(nombre, t)) {
            v = v >= 100 ? 0 : (v / 20 + 1) * 20;
            aplicar_volumenes(a);
            cambio = true;
        }
    };
    volumen("Music", a.cfg.volumen_musica);
    volumen("Sound effects", a.cfg.volumen_efectos);
    if (fila("Window positions", "Reset")) {
        for (PosVentana& p : a.pos_ventanas) p = {};
        a.cfg.ventanas.clear();
        cambio = true;
    }
    if (cambio) guardar_config(a.cfg);
}

void dibujar_ayuda_teclas(App& a) {
    if (!a.ayuda_teclas) return;
    static constexpr const char* FILAS[][2] = {
        {"F1", "This help"},
        {"F2", "Simple / original interface"},
        {"F3", "Debug overlay (fps, CPU)"},
        {"F4 / Shift+F4", "Camera / shade in shadows"},
        {"F5-F12", "Cast the assigned spell"},
        {"Shift+F12", "Screenshot (captura.bmp)"},
        {"1..0 / Alt+1..0", "Shortcut bar (spells, items)"},
        {"Enter", "Open chat / send (Esc: cancel)"},
        {"A", "Keep attacking the target (until it dies or you walk)"},
        {"Ctrl+click", "Force attack (NPCs, other players)"},
        {"/pk", "PK mode: clicking a player attacks"},
        {"/exclude Name", "Block / unblock a player (no name: list)"},
        {"/buddy, /addbuddy, /delbuddy", "Buddy list"},
        {"Middle click", "Select bar slot / use it on target"},
        {"Alt+I / Alt+C / Alt+S", "Inventory / status / magic"},
        {"Ctrl +/- / Ctrl 0", "Zoom in / out / auto"},
        {"Arrows, mouse", "Walk; click a monster to attack"},
        {"Ctrl+click", "Attack a player or NPC"},
        {"Enter / Esc", "Send chat / close window"},
    };
    const int n = static_cast<int>(sizeof FILAS / sizeof FILAS[0]);
    // Columnas a medida del texto: la de la tecla mide lo que su entrada más larga; la descripción ocupa el
    // resto (entre 160 y 300 px) y lo que no cabe se parte en renglones. Con la fuente nueva se salían.
    auto partir = [](std::string_view t, int ancho, auto&& f) {
        while (!t.empty()) {
            size_t k = bytes_que_caben(t.data(), t.size(), ancho);
            if (k == 0) k = t.size();
            f(t.substr(0, k));
            t = t.substr(k);
            while (!t.empty() && t.front() == ' ') t.remove_prefix(1);
        }
    };
    int col_tecla = 0, col_desc = 0;
    for (int i = 0; i < n; ++i) {
        col_tecla = std::max(col_tecla, ancho_texto(FILAS[i][0], std::strlen(FILAS[i][0])));
        col_desc = std::max(col_desc, ancho_texto(FILAS[i][1], std::strlen(FILAS[i][1])));
    }
    col_tecla += 16;
    const int ancho_desc = std::clamp(std::min(col_desc, a.gpu.ancho() - 20 - col_tecla - 24), 160, 300);
    const int ancho_total = col_tecla + ancho_desc + 24;
    constexpr int ALTO_LINEA = ALTO_GLIFO + 4, SEPARACION = 4;
    int renglones_total = 0;
    for (int i = 0; i < n; ++i) {
        int lineas = 0;
        partir(FILAS[i][1], ancho_desc, [&](std::string_view) { ++lineas; });
        renglones_total += std::max(1, lineas);
    }
    Rect r{(a.gpu.ancho() - ancho_total) / 2, 60, ancho_total,
           ui::ALTO_TITULO + 14 + renglones_total * ALTO_LINEA + n * SEPARACION};
    if (recuadro(a, r, "Keys (F1)") || a.entrada.escape) {
        a.ayuda_teclas = false;
        return;
    }
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    int linea = 0;
    for (int i = 0; i < n; ++i) {
        const int primera = linea;
        partir(FILAS[i][1], ancho_desc, [&](std::string_view t) {
            const float y = static_cast<float>(r.y + ui::ALTO_TITULO + 8 + linea * ALTO_LINEA + i * SEPARACION);
            if (linea == primera) a.g->texto(FILAS[i][0], static_cast<float>(r.x + 12), y, kt, rgba(255, 230, 120));
            a.g->texto(t, static_cast<float>(r.x + 12 + col_tecla), y, kt, rgba(230, 220, 190));
            ++linea;
        });
        if (linea == primera) ++linea;  // fila vacía: ocupa su renglón igual que en el cálculo
    }
}

void dibujar_salida(App& a) {
    if (!a.ventana_salida) return;
    arrastrar_ventana(a, IdVentana::SALIDA);
    Rect r = rect_ventana(a, IdVentana::SALIDA);
    if (recuadro(a, r, "Exit")) {
        a.ventana_salida = false;
        return;
    }
    // Restart: vuelve al lugar de retorno de la zona (también sirve para salir de un mapa que esta
    // instalación no tiene). El servidor deja el HP en el nivel del personaje.
    if (ui::boton_texto(*a.g, a.entrada, "Restart (back to town)", r.x + 12, r.y + 32, r.w - 24, 20)) {
        reiniciar(a);
        a.ventana_salida = false;
    }
    // Elegir otro personaje (C_ChangeChar): el servidor saca al personaje y manda la lista.
    if (ui::boton_texto(*a.g, a.entrada, "Character select", r.x + 12, r.y + 60, r.w - 24, 20)) {
        enviar(a, cmd::cambiar_personaje(a.op));
        a.ventana_salida = false;
    }
    if (ui::boton_texto(*a.g, a.entrada, "Quit game", r.x + 12, r.y + 88, r.w - 24, 20)) a.salir = true;
}

void avisar_mapa(App& a, MapId mapa) {
    char t[160];
    std::snprintf(t, sizeof t, "map/%u", mapa);
    std::error_code ec;
    if (std::filesystem::is_directory(a.cfg.carpeta_juego + t, ec)) return;
    std::snprintf(t, sizeof t,
                  "The server sent you to map %u, which this game installation does not have. Use Exit > Restart to go back.",
                  mapa);
    a.chat.agregar(t, rgba(255, 120, 100), CHAT_SISTEMA);
}

}  // namespace lin
