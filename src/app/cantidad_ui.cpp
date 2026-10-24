// Diálogo de cantidad y dar ítems.
//  - S_InputAmount: el servidor pide un número (llaves de la posada, cuántos fabricar, subastas)
//    entre un mínimo y un máximo; se responde con C_Amount (la cantidad y la acción que mandó).
//  - Dar un ítem (C_GiveItem): arrastrarlo del inventario sobre un NPC o una mascota. De una pila,
//    el mismo diálogo pregunta cuántos.
// El diálogo va al centro: números con el teclado, −/+ , Enter acepta y Esc cancela.
#include <charconv>
#include "app/app.h"

namespace lin {

namespace {

constexpr int ANCHO = 260, ALTO = 112;

void abrir(App& a, std::string_view titulo, i32 min, i32 max, i32 inicial) {
    DialogoCantidad& d = a.cantidad;
    d.abierto = true;
    d.titulo.assign(titulo);
    d.min = std::max(min, 0);
    d.max = std::max(max, d.min);
    d.valor = std::clamp(inicial, d.min, d.max);
}

void aceptar(App& a) {
    DialogoCantidad& d = a.cantidad;
    d.abierto = false;
    d.valor = std::clamp(d.valor, d.min, d.max);
    if (d.dar) {
        Slot s = a.mundo.objetos.buscar(d.objeto);
        if (s == SIN_SLOT) return;
        const TablaObjetos& o = a.mundo.objetos;
        enviar(a, cmd::dar_item(a.op, d.objeto, static_cast<u16>(o.col<O_X>()[s]), static_cast<u16>(o.col<O_Y>()[s]), d.item,
                                static_cast<u32>(d.valor)));
    } else {
        enviar(a, cmd::cantidad(a.op, d.objeto, static_cast<u32>(d.valor), d.accion));
    }
}

}  // namespace

void eventos_cantidad(App& a, const Evento& e) {
    if (e.tipo != Ev::PEDIR_CANTIDAD) return;
    DialogoCantidad& d = a.cantidad;
    d.dar = false;
    d.objeto = e.id;
    d.precio = e.v[PC_PRECIO];
    d.accion.assign(e.s[0]);
    abrir(a, e.s[1].empty() ? "How many?" : e.s[1], e.v[PC_MIN], e.v[PC_MAX], e.v[PC_INICIAL]);
    if (a.aut.cantidad >= 0) {  // prueba automática: esa cantidad y aceptar
        d.valor = a.aut.cantidad;
        aceptar(a);
    }
}

void dar_item(App& a, ObjId objetivo, ObjId item) {
    const TablaItems& t = a.mundo.inventario.items;
    Slot s = t.buscar(item);
    if (s == SIN_SLOT) return;
    DialogoCantidad& d = a.cantidad;
    d.dar = true;
    d.objeto = objetivo;
    d.item = item;
    d.precio = 0;
    const i32 n = static_cast<i32>(t.col<I_CANTIDAD>()[s]);
    char titulo[256];
    const size_t m = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), titulo, sizeof titulo);
    abrir(a, std::string_view(titulo, m), 1, std::max(n, 1), std::max(n, 1));
    if (n <= 1 || a.aut.cantidad >= 0) {  // uno solo, o prueba automática: sin preguntar
        if (a.aut.cantidad >= 0) d.valor = std::clamp(a.aut.cantidad, 1, std::max(n, 1));
        aceptar(a);
    }
}

bool sobre_cantidad(const App& a) {
    return a.cantidad.abierto && ui::sobre(a.entrada, (a.gpu.ancho() - ANCHO) / 2, a.gpu.alto() / 3, ANCHO, ALTO);
}

void dibujar_cantidad(App& a) {
    DialogoCantidad& d = a.cantidad;
    if (!d.abierto) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = (a.gpu.ancho() - ANCHO) / 2, y = a.gpu.alto() / 3;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 90);
    ui::recuadro(g, x, y, ANCHO, ALTO, 240);
    g.texto(d.titulo, static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    g.texto(d.dar ? "Give how many?" : "Amount", static_cast<float>(x + 10), static_cast<float>(y + 30), kt,
            rgba(170, 160, 140));

    // Números con el teclado (se reemplaza el valor al escribir el primero), −/+ y la rueda.
    for (int i = 0; i < e.ntexto; ++i) {
        if (e.texto[i] < '0' || e.texto[i] > '9') continue;
        const i64 v = static_cast<i64>(d.escribiendo ? d.valor : 0) * 10 + (e.texto[i] - '0');
        d.valor = static_cast<i32>(std::min<i64>(v, d.max));
        d.escribiendo = true;
    }
    if (e.retroceso) {
        d.valor /= 10;
        d.escribiendo = true;
    }
    if (e.rueda) d.valor = std::clamp(d.valor + e.rueda, d.min, d.max);
    const int by = y + 50;
    if (ui::boton_texto(g, e, "-", x + 10, by, 24, 20)) d.valor = std::max(d.min, d.valor - 1);
    char t[48];
    std::snprintf(t, sizeof t, "%d", d.valor);
    g.gpu().rectangulo(x + 40, by, 100, 20, clave_orden(CAPA_UI, 3), rgba(40, 32, 22, 255));
    g.texto(t, static_cast<float>(x + 46), static_cast<float>(by + 4), kt, ui::COLOR_TEXTO);
    if (ui::boton_texto(g, e, "+", x + 146, by, 24, 20)) d.valor = std::min(d.max, d.valor + 1);
    if (ui::boton_texto(g, e, "Max", x + 176, by, 40, 20)) d.valor = d.max;
    if (d.precio > 0) {
        std::snprintf(t, sizeof t, "%lld adena", static_cast<long long>(d.precio) * d.valor);
        g.texto(t, static_cast<float>(x + 10), static_cast<float>(by + 26), kt, rgba(255, 230, 150));
    }
    const bool ok = ui::boton_texto(g, e, "OK", x + ANCHO - 170, y + ALTO - 26, 76, 20) || e.enter;
    const bool cancelar = ui::boton_texto(g, e, "Cancel", x + ANCHO - 86, y + ALTO - 26, 76, 20) || e.escape;
    // El teclado es del diálogo: no llega al chat ni a las ventanas.
    e.ntexto = 0;
    e.enter = e.escape = e.retroceso = false;
    if (ok && d.valor >= d.min) aceptar(a);
    else if (cancelar) d.abierto = false;
    if (!d.abierto) d.escribiendo = false;
}

}  // namespace lin
