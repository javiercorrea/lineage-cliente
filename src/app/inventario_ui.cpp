// Entregable 6: ventana de inventario (Alt+I). Grilla de íconos originales (N.tbt), nombre al
// pasar el mouse, ítems equipados marcados, peso; doble clic usa (equipa, bebe...) y arrastrar
// un ítem fuera de la ventana lo suelta al suelo. Arrastrarlo al basurero (abajo a la derecha) lo
// elimina, para lo que no se puede soltar ni vender (el servidor rechaza lo equipado y lo que no se
// puede borrar, con su mensaje).
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr int COLUMNAS = 6, FILAS = 7, CELDA = 36;  // 6 × 7 celdas de 36 px (íconos de 32)
constexpr int GRILLA_X = 14, GRILLA_Y = 30;          // dentro de la ventana
constexpr u64 MS_DOBLE_CLIC = 400;
constexpr int BASURA_X = GRILLA_X + COLUMNAS * CELDA - 22, BASURA_Y = GRILLA_Y + FILAS * CELDA + 2, BASURA_W = 20,
              BASURA_H = 18;  // el basurero, dentro de la ventana

// El basurero del original (1003; 1004 resaltado, con un ítem encima); si no está, con rectángulos.
void dibujar_basurero(Graficos& g, int x, int y, bool encima) {
    const u64 k = clave_orden(CAPA_UI, 13);
    if (const RegionAtlas& r = g.img(encima ? 1004 : 1003); r.valida()) {
        g.gpu().dibujar(r, x + 3, y, k);
        return;
    }
    const u32 c = encima ? rgba(230, 110, 90) : rgba(170, 160, 140), oscuro = rgba(40, 32, 22, 255);
    g.gpu().rectangulo(x + 7, y, 6, 2, k, c);
    g.gpu().rectangulo(x + 1, y + 2, 18, 3, k, c);
    g.gpu().rectangulo(x + 3, y + 6, 14, 12, k, c);
    for (int i = 0; i < 3; ++i) g.gpu().rectangulo(x + 6 + i * 3, y + 8, 1, 8, clave_orden(CAPA_UI, 14), oscuro);
}

// Celda bajo el puntero (índice en la tabla, ya desplazado) o -1.
int celda_bajo_puntero(const App& a, int x0, int y0) {
    const EntradaFrame& e = a.entrada;
    int cx = e.mx - (x0 + GRILLA_X), cy = e.my - (y0 + GRILLA_Y);
    if (cx < 0 || cy < 0 || cx >= COLUMNAS * CELDA || cy >= FILAS * CELDA) return -1;
    int i = static_cast<int>(a.inventario_desplazamiento) * COLUMNAS + (cy / CELDA) * COLUMNAS + cx / CELDA;
    return i < static_cast<int>(a.mundo.inventario.items.tamano()) ? i : -1;
}

}  // namespace

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

namespace {

}  // namespace

bool sobre_inventario(const App& a) { return sobre_ventana(a, IdVentana::INVENTARIO); }

// Uso 16 (pergamino de polymorph): primero se elige el monstruo (forma_ui.cpp).
constexpr u8 USO_POLYMORPH = 16, USO_TELEPORT = 6, USO_TELEPORT_BENDITO = 29;  // ItemTable (useType)
// Se usan sobre otro ítem (C_UseItem lee el objetivo): identificar, encantar arma y armadura...
bool se_usa_sobre_item(u8 uso) { return uso == 7 || uso == 14 || uso == 26 || uso == 27; }

void usar_item(App& a, ObjId item) {
    Slot s = a.mundo.inventario.items.buscar(item);
    if (s != SIN_SLOT) {
        a.gfx_item_usado = a.mundo.inventario.items.col<I_GFX>()[s];
        a.t_item_usado = a.entrada.ahora_ms;
    }
    if (s != SIN_SLOT && se_usa_sobre_item(a.mundo.inventario.items.col<I_USO>()[s])) {
        a.usar_sobre = item;  // el próximo clic en el inventario elige el objetivo
        a.ventana_inventario = true;
        return;
    }
    if (s != SIN_SLOT && a.mundo.inventario.items.col<I_USO>()[s] == USO_POLYMORPH) {
        abrir_forma(a, item);
        return;
    }
    // Pergamino de teleport: con marcadores guardados se elige el destino; si no, al azar.
    if (s != SIN_SLOT && (a.mundo.inventario.items.col<I_USO>()[s] == USO_TELEPORT || a.mundo.inventario.items.col<I_USO>()[s] == USO_TELEPORT_BENDITO)) {
        if (!elegir_destino(a, item, 0)) enviar(a, cmd::usar_teleport(a.op, item, 0, 0));
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
    if (!ac.a_quien.empty()) {
        // --dar: al más cercano cuyo nombre contenga a_quien (sin contar al propio personaje).
        const TablaObjetos& o = a.mundo.objetos;
        const Slot yo = o.buscar(a.mundo.jugador.id);
        Slot mejor = SIN_SLOT;
        int mejor_d = 1 << 30;
        for (Slot s = 0; yo != SIN_SLOT && s < o.tamano(); ++s) {
            if (s == yo || a.desc.resolver(texto(a.mundo, o.col<O_NOMBRE>()[s])).find(ac.a_quien) == std::string_view::npos) continue;
            const int d = std::max(std::abs(o.col<O_X>()[s] - o.col<O_X>()[yo]), std::abs(o.col<O_Y>()[s] - o.col<O_Y>()[yo]));
            if (d < mejor_d) mejor_d = d, mejor = s;
        }
        if (mejor == SIN_SLOT) LIN_LOG("Prueba automática: no hay un \"%s\" a la vista", ac.a_quien.c_str());
        else dar_item(a, o.ids()[mejor], item);
    } else if (!ac.sobre.empty()) {
        const ObjId objetivo = buscar_item(a, ac.sobre);
        if (objetivo) enviar(a, cmd::usar_item_sobre(a.op, item, objetivo));
        else LIN_LOG("Prueba automática: no hay un ítem \"%s\" en el inventario", ac.sobre.c_str());
    } else if (ac.borrar) {
        borrar_item(a, item);
    } else if (ac.soltar) {
        soltar_item(a, item);
    } else {
        usar_item(a, item);
    }
}

void dibujar_inventario(App& a) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
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
    if (a.usar_sobre) g.texto("Click the target item (Esc cancels)", x0 + GRILLA_X, y0 + GRILLA_Y + FILAS * CELDA + 6, kt, rgba(255, 230, 120));
    else g.texto(pie, x0 + GRILLA_X, y0 + GRILLA_Y + FILAS * CELDA + 6, kt, rgba(214, 190, 140));
    const bool sobre_basura = ui::sobre(e, x0 + BASURA_X, y0 + BASURA_Y, BASURA_W, BASURA_H);
    dibujar_basurero(g, x0 + BASURA_X, y0 + BASURA_Y, sobre_basura && a.arrastrando);
    if (sobre_basura && !a.arrastrando) {
        constexpr std::string_view AYUDA = "Drag an item here to delete it";
        const int w = ancho_texto(AYUDA.data(), AYUDA.size());
        const int tx = std::min(e.mx + 12, a.gpu.ancho() - w - 8), ty = e.my + 16;
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto(AYUDA, tx, ty, clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
    }

    // Nombre del ítem bajo el puntero (blanco; bendito amarillo, maldito rojo) y, si está
    // identificado, sus atributos debajo (daño, defensa, encantamiento, bonos...).
    if (bajo >= 0 && !a.arrastrando) {
        Slot s = static_cast<Slot>(bajo);
        char buf[256], det[1024];
        size_t n = expandir_desc(a.desc, nombre_item(inv, s), buf, sizeof buf);
        size_t nd = describir_atributos(atributos_item(inv, s), det, sizeof det);
        if (nd == 0 && !t.col<I_IDENTIFICADO>()[s]) nd = static_cast<size_t>(std::snprintf(det, sizeof det, "Unidentified"));
        u8 b = t.col<I_BENDICION>()[s];
        u32 color = b == 1 ? rgba(255, 230, 120) : (b == 2 ? rgba(255, 110, 100) : rgba(255, 255, 254));
        int w = ancho_texto(buf, n), lineas = 0;
        for (std::string_view resto(det, nd); !resto.empty(); ++lineas) {
            const size_t fin = std::min(resto.find('\n'), resto.size());
            w = std::max(w, ancho_texto(resto.data(), fin));
            resto.remove_prefix(std::min(fin + 1, resto.size()));
        }
        const int alto_linea = ALTO_GLIFO + 3;
        int tx = std::min(e.mx + 12, a.gpu.ancho() - w - 8);
        int ty = std::min(e.my + 16, a.gpu.alto() - (lineas + 1) * alto_linea - 8);
        // El fondo en la capa de textos (debajo de los suyos): si no, los textos de la ventana se ven a través.
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, (lineas + 1) * alto_linea + 4, clave_orden(CAPA_UI_TEXTO, 29), rgba(0, 0, 0, 230));
        g.texto({buf, n}, tx, ty, clave_orden(CAPA_UI_TEXTO, 30), color);
        int y = ty + alto_linea;
        for (std::string_view resto(det, nd); !resto.empty(); y += alto_linea) {
            const size_t fin = std::min(resto.find('\n'), resto.size());
            g.texto(resto.substr(0, fin), tx, y, clave_orden(CAPA_UI_TEXTO, 30), rgba(200, 190, 160));
            resto.remove_prefix(std::min(fin + 1, resto.size()));
        }
    }

    // Arrastrar: empieza al apretar sobre un ítem; al soltarlo sobre el mundo (fuera de toda
    // ventana) va al suelo.
    // Pergamino esperando su objetivo: el clic en un ítem lo usa sobre él; Esc o un clic fuera cancela.
    if (a.usar_sobre) {
        if (t.buscar(a.usar_sobre) == SIN_SLOT) a.usar_sobre = 0;  // se gastó o desapareció
        if (e.escape) {
            a.usar_sobre = 0;
            e.escape = false;
        } else if (e.clic) {
            if (bajo >= 0) enviar(a, cmd::usar_item_sobre(a.op, a.usar_sobre, t.ids()[static_cast<Slot>(bajo)]));
            a.usar_sobre = 0;
            a.arrastrando = 0;
            return;
        }
        return;  // mientras tanto no se arrastra ni se usa otra cosa
    }
    if (e.clic && bajo >= 0 && clic_item_tienda(a, t.ids()[static_cast<Slot>(bajo)])) return;  // armando la tienda
    if (e.pulsado && bajo >= 0) a.arrastrando = t.ids()[static_cast<Slot>(bajo)];
    if (a.arrastrando && e.presionado) {
        Slot s = t.buscar(a.arrastrando);
        if (s != SIN_SLOT) g.gpu().dibujar(g.icono(t.col<I_GFX>()[s]), e.mx - 16, e.my - 16, clave_orden(CAPA_ARRASTRE, 0));
    }
    if (e.clic) {
        if (a.arrastrando && sobre_basura) {
            borrar_item(a, a.arrastrando);  // pregunta antes
        } else if (a.arrastrando && sobre_ventana(a, IdVentana::INTERCAMBIO)) {
            ofrecer_en_intercambio(a, a.arrastrando);  // al intercambio
        } else if (a.arrastrando && casillero_bajo_puntero(a) >= 0) {
            // A la barra de atajos (Alt+número lo usa).
            Slot s = t.buscar(a.arrastrando);
            if (s != SIN_SLOT) poner_atajo(a, casillero_bajo_puntero(a), {TipoAtajo::ITEM, a.arrastrando, t.col<I_GFX>()[s]});
        } else if (a.arrastrando && !sobre_alguna_ventana(a) && !sobre_barra(a)) {  // solo sobre el mundo
            // Sobre un NPC o una mascota se le da (C_GiveItem; el servidor decide si lo acepta); si
            // no, va al suelo.
            const ObjId obj = objeto_bajo_puntero(a);
            const Slot so = obj ? a.mundo.objetos.buscar(obj) : SIN_SLOT;
            const u8 tipo = so != SIN_SLOT ? tipo_objeto(a, so) : 0;
            if (so != SIN_SLOT && tipo != TIPO_PERSONAJE && tipo != TIPO_ITEM && tipo != TIPO_PUERTA) dar_item(a, obj, a.arrastrando);
            else soltar_item(a, a.arrastrando);
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
