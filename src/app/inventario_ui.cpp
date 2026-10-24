// Entregable 6: ventana de inventario (Alt+I). Grilla de íconos originales (N.tbt), nombre al
// pasar el mouse, ítems equipados marcados, peso; doble clic usa (equipa, bebe...) y arrastrar
// un ítem fuera de la ventana lo suelta al suelo.
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr int COLUMNAS = 6, FILAS = 7, CELDA = 36;  // 6 × 7 celdas de 36 px (íconos de 32)
constexpr int GRILLA_X = 14, GRILLA_Y = 30;          // dentro de la ventana
constexpr u64 MS_DOBLE_CLIC = 400;

// Celda bajo el puntero (índice en la tabla, ya desplazado) o -1.
int celda_bajo_puntero(const App& a, int x0, int y0) {
    const EntradaFrame& e = a.entrada;
    int cx = e.mx - (x0 + GRILLA_X), cy = e.my - (y0 + GRILLA_Y);
    if (cx < 0 || cy < 0 || cx >= COLUMNAS * CELDA || cy >= FILAS * CELDA) return -1;
    int i = static_cast<int>(a.inventario_desplazamiento) * COLUMNAS + (cy / CELDA) * COLUMNAS + cx / CELDA;
    return i < static_cast<int>(a.mundo.inventario.items.tamano()) ? i : -1;
}

// Primer ítem del inventario cuyo nombre (con "$N" resueltos) contenga `texto`; 0 si no hay.
ObjId buscar_item(const App& a, std::string_view texto) {
    const TablaItems& t = a.mundo.inventario.items;
    char buf[256];
    for (Slot s = 0; s < t.tamano(); ++s) {
        size_t n = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), buf, sizeof buf);
        if (std::string_view(buf, n).find(texto) != std::string_view::npos) return t.ids()[s];
    }
    return 0;
}

}  // namespace

bool sobre_inventario(const App& a) { return sobre_ventana(a, IdVentana::INVENTARIO); }

// Uso 16 (pergamino de polymorph): primero se elige el monstruo (forma_ui.cpp).
constexpr u8 USO_POLYMORPH = 16;

void usar_item(App& a, ObjId item) {
    Slot s = a.mundo.inventario.items.buscar(item);
    if (s != SIN_SLOT && a.mundo.inventario.items.col<I_USO>()[s] == USO_POLYMORPH) {
        abrir_forma(a, item);
        return;
    }
    enviar(a, cmd::usar_item(a.op, item));
}

// Al suelo, en la celda propia y con toda la cantidad.
void soltar_item(App& a, ObjId item) {
    Slot yo = a.mundo.objetos.buscar(a.mundo.jugador.id);
    Slot s = a.mundo.inventario.items.buscar(item);
    if (yo == SIN_SLOT || s == SIN_SLOT) return;
    enviar(a, cmd::soltar(a.op, a.mundo.objetos.col<O_X>()[yo], a.mundo.objetos.col<O_Y>()[yo], item,
                          a.mundo.inventario.items.col<I_CANTIDAD>()[s]));
}

void acciones_item_automaticas(App& a) {
    Automatizacion& au = a.aut;
    if (au.items_hechos >= au.items.size() || a.frames_en_pantalla < 240 + 240 * au.items_hechos) return;
    const Automatizacion::AccionItem& ac = au.items[au.items_hechos++];
    ObjId item = buscar_item(a, ac.nombre);
    if (item == 0) {
        LIN_LOG("Prueba automática: no hay un ítem \"%s\" en el inventario", ac.nombre.c_str());
        return;
    }
    if (ac.soltar) soltar_item(a, item);
    else usar_item(a, item);
}

void dibujar_inventario(App& a) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const Inventario& inv = a.mundo.inventario;
    const TablaItems& t = inv.items;
    arrastrar_ventana(a, IdVentana::INVENTARIO);
    const Rect rv = rect_ventana(a, IdVentana::INVENTARIO);
    const int x0 = rv.x, y0 = rv.y;
    if (ui::ventana(g, e, a.cfg.ui_simple, x0, y0, "Inventory", opacidad_ventanas(a))) a.ventana_inventario = false;

    // Desplazamiento con la rueda si hay más ítems que celdas.
    u32 filas_total = static_cast<u32>((t.tamano() + COLUMNAS - 1) / COLUMNAS);
    u32 max_desp = filas_total > FILAS ? filas_total - FILAS : 0;
    if (sobre_inventario(a) && e.rueda) {
        i64 d = static_cast<i64>(a.inventario_desplazamiento) - e.rueda;
        a.inventario_desplazamiento = static_cast<u32>(std::clamp<i64>(d, 0, max_desp));
    }
    a.inventario_desplazamiento = std::min(a.inventario_desplazamiento, max_desp);

    const u64 kf = clave_orden(CAPA_UI, 12), ki = clave_orden(CAPA_UI, 13), kt = clave_orden(CAPA_UI_TEXTO, 12);
    const int bajo = celda_bajo_puntero(a, x0, y0);
    const size_t primero = static_cast<size_t>(a.inventario_desplazamiento) * COLUMNAS;
    for (int c = 0; c < COLUMNAS * FILAS; ++c) {
        int x = x0 + GRILLA_X + (c % COLUMNAS) * CELDA, y = y0 + GRILLA_Y + (c / COLUMNAS) * CELDA;
        size_t i = primero + static_cast<size_t>(c);
        bool hay = i < t.tamano();
        bool equipado = hay && item_equipado(nombre_item(inv, static_cast<Slot>(i)));
        u32 fondo = static_cast<int>(i) == bajo ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230);
        g.gpu().rectangulo(x, y, CELDA - 2, CELDA - 2, kf, equipado ? rgba(80, 64, 20, 240) : fondo);
        if (!hay) continue;
        Slot s = static_cast<Slot>(i);
        if (t.ids()[s] != a.arrastrando) g.gpu().dibujar(g.icono(t.col<I_GFX>()[s]), x + 1, y + 1, ki);
        if (equipado) g.texto("E", x + 1, y + 1, kt, rgba(255, 230, 90));
        if (u32 n = t.col<I_CANTIDAD>()[s]; n > 1) {
            char cant[16];
            std::snprintf(cant, sizeof cant, "%u", n);
            g.texto_sombra(cant, x + CELDA - 3 - ancho_texto(cant, std::strlen(cant)), y + CELDA - 2 - ALTO_GLIFO, kt,
                           rgba(255, 255, 254));
        }
    }
    char pie[48];
    std::snprintf(pie, sizeof pie, "Weight %d%%   %zu items", a.mundo.jugador.peso * 100 / 240, t.tamano());
    g.texto(pie, x0 + GRILLA_X, y0 + GRILLA_Y + FILAS * CELDA + 6, kt, rgba(214, 190, 140));

    // Nombre del ítem bajo el puntero (blanco; bendito amarillo, maldito rojo).
    if (bajo >= 0 && !a.arrastrando) {
        Slot s = static_cast<Slot>(bajo);
        char buf[256];
        size_t n = expandir_desc(a.desc, nombre_item(inv, s), buf, sizeof buf);
        u8 b = t.col<I_BENDICION>()[s];
        u32 color = b == 1 ? rgba(255, 230, 120) : (b == 2 ? rgba(255, 110, 100) : rgba(255, 255, 254));
        int w = ancho_texto(buf, n);
        int tx = std::min(e.mx + 12, a.gpu.ancho() - w - 8), ty = e.my + 16;
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto({buf, n}, tx, ty, clave_orden(CAPA_UI_TEXTO, 30), color);
    }

    // Arrastrar: empieza al apretar sobre un ítem; al soltar fuera de la ventana va al suelo.
    if (e.pulsado && bajo >= 0) a.arrastrando = t.ids()[static_cast<Slot>(bajo)];
    if (a.arrastrando && e.presionado) {
        Slot s = t.buscar(a.arrastrando);
        if (s != SIN_SLOT) g.gpu().dibujar(g.icono(t.col<I_GFX>()[s]), e.mx - 16, e.my - 16, clave_orden(CAPA_UI, 31));
    }
    if (e.clic) {
        if (a.arrastrando && sobre_ventana(a, IdVentana::INTERCAMBIO)) {
            ofrecer_en_intercambio(a, a.arrastrando);  // al intercambio
        } else if (a.arrastrando && casillero_bajo_puntero(a) >= 0) {
            // A la barra de atajos (Alt+número lo usa).
            Slot s = t.buscar(a.arrastrando);
            if (s != SIN_SLOT) poner_atajo(a, casillero_bajo_puntero(a), {TipoAtajo::ITEM, a.arrastrando, t.col<I_GFX>()[s]});
        } else if (a.arrastrando && !sobre_inventario(a) && !sobre_barra(a)) {
            soltar_item(a, a.arrastrando);
        } else if (bajo >= 0) {
            // Doble clic: usar (equipar/quitar armas y armaduras, beber pociones...).
            ObjId id = t.ids()[static_cast<Slot>(bajo)];
            if (id == a.ultimo_clic_item && e.ahora_ms - a.t_ultimo_clic_item < MS_DOBLE_CLIC) {
                if (a.intercambio.abierto) ofrecer_en_intercambio(a, id);  // en un intercambio, se ofrece
                else usar_item(a, id);
                a.ultimo_clic_item = 0;
            } else {
                a.ultimo_clic_item = id;
                a.t_ultimo_clic_item = e.ahora_ms;
            }
        }
        a.arrastrando = 0;
    }
}

}  // namespace lin
