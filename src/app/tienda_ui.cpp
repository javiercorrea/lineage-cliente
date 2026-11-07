// Tienda de jugadores (solo en las salas del mercado de Giran: mapas 340, 350, 360 y 370).
//  - Vendedores: S_DoActionGFX con la acción 70 y el título (S_DoActionShop); se los ve sentados con
//    el título sobre el nombre. Un clic camina hasta él y pide su lista (C_ShopList); S_PrivateShop
//    trae lo que vende (tipo 0) o lo que compra (tipo 1, solo los ítems propios que le sirven).
//  - Comprar: C_Result tipo 0 con (orden, cantidad); venderle: C_Result tipo 1 con el ítem propio,
//    la cantidad (3 bytes) y el orden. La cantidad se pregunta con el diálogo de cantidad.
//  - La propia: "/shop Título" abre la ventana para armarla; un clic en un ítem del inventario pide
//    el precio por unidad y lo agrega (toda la pila); "Open" la abre (C_Shop 0) y "Close" la cierra.
// shortcut: la propia solo vende (la lista de compra de C_Shop va vacía); agregarla si se usa.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "app/app.h"

namespace lin {

namespace {

constexpr int ANCHO = 320, FILA = 36, FILAS = 8;  // FILAS: filas a la vista (de la tienda mirada o la propia)
constexpr int FILA_VENTA = ALTO_GLIFO + 6;

int x_ventana(const App& a) { return (a.gpu.ancho() - ANCHO) / 2; }
int y_ventana() { return 60; }
int alto_ventana(const App& a) {
    const TiendaJugadores& t = a.tienda_pj;
    const int filas = t.vista ? FILAS : std::clamp<int>(static_cast<int>(t.venta.size()), 1, FILAS);
    return 64 + filas * (t.vista ? FILA : FILA_VENTA) + 30;
}

void quitar_vendedor(App& a, ObjId id) {
    std::erase_if(a.tienda_pj.vendedores, [&](const Vendedor& v) { return v.id == id; });
}

void preguntar(App& a, u8 modo, ObjId objeto, ObjId item, i32 max, std::string_view titulo, const char* pregunta) {
    DialogoCantidad& d = a.cantidad;
    d = {};
    d.abierto = true;
    d.tienda = modo;
    d.objeto = objeto;
    d.item = item;
    d.titulo.assign(titulo);
    d.accion = pregunta;
    d.min = 1;
    d.max = std::max(max, 1);
    d.valor = 1;
}


}  // namespace

const Vendedor* vendedor(const App& a, ObjId id) {
    for (const Vendedor& v : a.tienda_pj.vendedores)
        if (v.id == id) return &v;
    return nullptr;
}

void eventos_tienda(App& a, const Evento& e) {
    TiendaJugadores& t = a.tienda_pj;
    switch (e.tipo) {
    case Ev::ACCION_OBJETO:
        if (e.v[0] == 70) {  // abrió la tienda (también al aparecer, si ya la tenía)
            quitar_vendedor(a, e.id);
            t.vendedores.push_back({e.id, std::string(e.s[0])});
        } else if (e.v[0] == ACC_QUIETO) {  // la cerró
            quitar_vendedor(a, e.id);
            if (t.vista == e.id) t.vista = 0;
        }
        break;
    case Ev::QUITAR_OBJETO:
        quitar_vendedor(a, e.id);
        if (t.vista == e.id) t.vista = 0;
        break;
    case Ev::TIENDA:
        t.vista = e.id;
        t.tipo = static_cast<u8>(e.v[TP_TIPO]);
        t.lineas.clear();
        break;
    case Ev::TIENDA_LINEA: {
        if (e.id != t.vista) break;
        LineaTiendaVista l;
        l.orden = static_cast<u8>(e.v[TP_ORDEN]);
        l.cantidad = static_cast<u32>(e.v[TP_CANTIDAD]);
        l.precio = static_cast<u32>(e.v[TP_PRECIO]);
        if (t.tipo == 0) {
            l.bendicion = static_cast<u8>(e.v[TP_BENDICION]);
            l.gfx = static_cast<u16>(e.v[TP_GFX]);
            l.nombre.assign(e.s[0]);
        } else {
            // Lo que compra: el servidor manda el ítem propio que le sirve; nombre e ícono del inventario.
            l.mi_item = static_cast<ObjId>(e.v[TP_ITEM]);
            const Slot s = a.mundo.inventario.items.buscar(l.mi_item);
            if (s != SIN_SLOT) {
                l.gfx = a.mundo.inventario.items.col<I_GFX>()[s];
                l.nombre.assign(nombre_item(a.mundo.inventario, s));
            }
        }
        t.lineas.push_back(std::move(l));
        break;
    }
    default: break;
    }
}

void abrir_tienda_de(App& a, ObjId v) { enviar(a, cmd::ver_tienda(a.op, 0, v)); }

bool comando_tienda(App& a, std::string_view orden, std::string_view arg) {
    if (orden != "shop" && orden != "Shop" && orden != "SHOP") return false;
    TiendaJugadores& t = a.tienda_pj;
    if (t.abierta) {
        enviar(a, cmd::cerrar_tienda(a.op));
        t.abierta = false;
        return true;
    }
    t.armando = true;
    t.vista = 0;
    if (!arg.empty()) t.titulo.assign(arg);
    a.ventana_inventario = true;
    return true;
}

bool clic_item_tienda(App& a, ObjId item) {
    TiendaJugadores& t = a.tienda_pj;
    if (!t.armando || t.abierta) return false;
    const Slot s = a.mundo.inventario.items.buscar(item);
    if (s == SIN_SLOT || item_equipado(nombre_item(a.mundo.inventario, s))) return true;
    char nombre[256];
    const size_t n = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), nombre, sizeof nombre);
    preguntar(a, 3, 0, item, 2000000000, std::string_view(nombre, n), "Price per unit (adena)");
    return true;
}

void aceptar_cantidad_tienda(App& a) {
    DialogoCantidad& d = a.cantidad;
    TiendaJugadores& t = a.tienda_pj;
    const u32 valor = static_cast<u32>(d.valor);
    if (d.tienda == 1) {  // comprar: C_Result tipo 0 con (orden, cantidad)
        const LineaResultado l{static_cast<u32>(d.item), valor};
        enviar(a, cmd::resultado(a.op, d.objeto, RESULTADO_COMPRA, std::span(&l, 1)));
        enviar(a, cmd::ver_tienda(a.op, t.tipo, d.objeto));  // la lista actualizada
    } else if (d.tienda == 2) {  // venderle
        const auto it = std::find_if(t.lineas.begin(), t.lineas.end(), [&](const LineaTiendaVista& x) { return x.mi_item == d.item; });
        if (it != t.lineas.end()) enviar(a, cmd::vender_a_tienda(a.op, d.objeto, d.item, valor, it->orden));
        enviar(a, cmd::ver_tienda(a.op, t.tipo, d.objeto));
    } else if (d.tienda == 3) {  // precio: toda la pila
        const Slot s = a.mundo.inventario.items.buscar(d.item);
        if (s == SIN_SLOT) return;
        std::erase_if(t.venta, [&](const cmd::LineaTienda& l) { return l.item == d.item; });
        t.venta.push_back({static_cast<u32>(d.item), valor, a.mundo.inventario.items.col<I_CANTIDAD>()[s]});
    }
    d.tienda = 0;
}

bool sobre_tienda(const App& a) {
    const TiendaJugadores& t = a.tienda_pj;
    if (!t.vista && !t.armando) return false;
    return ui::sobre(a.entrada, x_ventana(a), y_ventana(), ANCHO, alto_ventana(a));
}

void dibujar_tienda(App& a) {
    TiendaJugadores& t = a.tienda_pj;
    if (!t.vista && !t.armando) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = x_ventana(a), y = y_ventana(), alto = alto_ventana(a);
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 80), ki = clave_orden(CAPA_UI, 81);
    ui::recuadro(g, x, y, ANCHO, alto, 235);
    char txt[160];

    if (t.vista) {
        // La tienda de otro: lo que vende o lo que compra; un clic en una línea pregunta cuántos.
        const Slot sv = a.mundo.objetos.buscar(t.vista);
        char nombre[64];
        const size_t nn = sv != SIN_SLOT ? expandir_desc(a.desc, texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[sv]), nombre, sizeof nombre) : 0;
        std::snprintf(txt, sizeof txt, "Shop of %.*s", static_cast<int>(nn), nombre);
        g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
        if (ui::boton_texto(g, e, "Sells", x + 10, y + 28, 70, 18)) enviar(a, cmd::ver_tienda(a.op, 0, t.vista));
        if (ui::boton_texto(g, e, "Buys", x + 86, y + 28, 70, 18)) enviar(a, cmd::ver_tienda(a.op, 1, t.vista));
        g.texto(t.tipo == 0 ? "It sells:" : "It buys (your items):", static_cast<float>(x + 166), static_cast<float>(y + 31), kt,
                rgba(170, 160, 140));
        if (t.lineas.empty()) g.texto("Nothing.", static_cast<float>(x + 12), static_cast<float>(y + 64), kt, ui::COLOR_TEXTO);
        // Las filas a la vista se recorren con la rueda o la barra; la barra va a la derecha.
        const u32 total = static_cast<u32>(t.lineas.size());
        const u32 max = total > FILAS ? total - FILAS : 0;
        if (e.rueda && sobre_tienda(a)) {
            const i64 n = static_cast<i64>(t.desplaza_vista) - e.rueda;
            t.desplaza_vista = static_cast<u32>(std::clamp<i64>(n, 0, max));
        }
        t.desplaza_vista = std::min(t.desplaza_vista, max);
        for (u32 k = 0; k < FILAS && t.desplaza_vista + k < total; ++k) {
            const size_t i = t.desplaza_vista + k;
            const LineaTiendaVista& l = t.lineas[i];
            const int fy = y + 56 + static_cast<int>(k) * FILA;
            const bool encima = ui::sobre(e, x + 8, fy, ANCHO - 26, FILA - 2);
            g.gpu().rectangulo(x + 8, fy, ANCHO - 26, FILA - 2, clave_orden(CAPA_UI, 80), encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
            if (l.gfx) g.gpu().dibujar(g.icono(l.gfx), x + 10, fy + 1, ki);
            char nom[256];
            const size_t n = expandir_desc(a.desc, l.nombre, nom, sizeof nom);
            g.texto({nom, n}, static_cast<float>(x + 48), static_cast<float>(fy + 4), kt, l.bendicion == 1 ? rgba(255, 230, 120) : ui::COLOR_TEXTO);
            std::snprintf(txt, sizeof txt, "%u adena  x%u", l.precio, l.cantidad);
            g.texto(txt, static_cast<float>(x + 48), static_cast<float>(fy + 18), kt, rgba(255, 220, 130));
            if (encima && e.clic) {
                if (t.tipo == 0) preguntar(a, 1, t.vista, l.orden, static_cast<i32>(l.cantidad), {nom, n}, "Buy how many?");
                else preguntar(a, 2, t.vista, l.mi_item, static_cast<i32>(l.cantidad), {nom, n}, "Sell how many?");
            }
        }
        ui::barra_scroll(g, e, t.barra_vista, x + ANCHO - 12, y + 56, 10, FILAS * FILA, total, FILAS, t.desplaza_vista,
                         clave_orden(CAPA_UI, 82));
        if (ui::boton_texto(g, e, "Close", x + ANCHO - 86, y + alto - 26, 76, 20) || e.escape) {
            t.vista = 0;
            e.escape = false;
        }
        return;
    }

    // La propia: lo que se va a vender, el título y abrir/cerrar.
    std::snprintf(txt, sizeof txt, "My shop: %s", t.titulo.empty() ? "(no title: /shop <title>)" : t.titulo.c_str());
    g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    g.texto(t.abierta ? "Open. /shop again or Close to close it." : "Click an inventory item to sell it (asks the price).",
            static_cast<float>(x + 10), static_cast<float>(y + 30), kt, rgba(170, 160, 140));
    const u32 total = static_cast<u32>(t.venta.size());
    const u32 max = total > FILAS ? total - FILAS : 0;
    if (e.rueda && sobre_tienda(a)) {
        const i64 n = static_cast<i64>(t.desplaza_venta) - e.rueda;
        t.desplaza_venta = static_cast<u32>(std::clamp<i64>(n, 0, max));
    }
    t.desplaza_venta = std::min(t.desplaza_venta, max);
    int fy = y + 56;
    for (u32 k = 0; k < FILAS && t.desplaza_venta + k < total; ++k, fy += FILA_VENTA) {
        const size_t i = t.desplaza_venta + k;
        const Slot s = a.mundo.inventario.items.buscar(t.venta[i].item);
        char nom[256];
        const size_t n = s != SIN_SLOT ? expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), nom, sizeof nom) : 0;
        std::snprintf(txt, sizeof txt, "%.*s  - %u adena", static_cast<int>(n), nom, t.venta[i].precio);
        g.texto(txt, static_cast<float>(x + 12), static_cast<float>(fy), kt, ui::COLOR_TEXTO);
        if (!t.abierta && ui::boton_texto(g, e, "x", x + ANCHO - 44, fy - 2, 20, ALTO_GLIFO + 4)) {
            t.venta.erase(t.venta.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    ui::barra_scroll(g, e, t.barra_venta, x + ANCHO - 12, y + 56, 10, FILAS * FILA_VENTA, total, FILAS, t.desplaza_venta,
                     clave_orden(CAPA_UI, 82));
    const int by = y + alto - 26;
    if (!t.abierta && !t.venta.empty() && ui::boton_texto(g, e, "Open", x + ANCHO - 170, by, 76, 20)) {
        enviar(a, cmd::abrir_tienda(a.op, t.venta, t.titulo));
        t.abierta = true;
    }
    if (ui::boton_texto(g, e, t.abierta ? "Close shop" : "Cancel", x + ANCHO - 86, by, 76, 20)) {
        if (t.abierta) enviar(a, cmd::cerrar_tienda(a.op));
        t.abierta = false;
        t.armando = false;
    }
}

}  // namespace lin
