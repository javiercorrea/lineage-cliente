// Diálogo de cantidad y dar ítems.
//  - S_InputAmount: el servidor pide un número (llaves de la posada, cuántos fabricar, subastas)
//    entre un mínimo y un máximo; se responde con C_Amount (la cantidad y la acción que mandó).
//  - Dar un ítem (C_GiveItem): arrastrarlo del inventario sobre un NPC o una mascota. De una pila,
//    el mismo diálogo pregunta cuántos.
//  - El basurero del inventario (C_DeleteInventoryItem): para lo que no se puede soltar ni vender.
//    Siempre pregunta (no se puede deshacer): cuántos de una pila, o que confirme si es uno.
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
    if (d.tienda) {
        aceptar_cantidad_tienda(a);
    } else if (d.castillo == CF_TASA + 1) {
        enviar(a, cmd::tasa_castillo(a.op, d.objeto, static_cast<u8>(d.valor)));
    } else if (d.castillo) {
        if (d.valor > 0) enviar(a, cmd::fondos_castillo(a.op, d.castillo == CF_DEPOSITAR + 1, d.objeto, static_cast<u32>(d.valor)));
    } else if (d.borrar) {
        if (a.mundo.inventario.items.buscar(d.item) != SIN_SLOT && d.valor > 0)  // 0 sería toda la pila
            enviar(a, cmd::borrar_item(a.op, d.item, static_cast<u32>(d.valor)));
    } else if (d.dar) {
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

void aceptar_cantidad(App& a, i32 valor) {
    if (!a.cantidad.abierto) return;
    a.cantidad.valor = valor;
    aceptar(a);
}

void eventos_cantidad(App& a, const Evento& e) {
    if (e.tipo != Ev::PEDIR_CANTIDAD) return;
    DialogoCantidad& d = a.cantidad;
    d.dar = d.borrar = false;
    d.tienda = d.castillo = 0;
    d.objeto = e.id;
    d.precio = e.v[PC_PRECIO];
    d.accion.assign(e.s[0]);
    abrir(a, e.s[1].empty() ? "How many?" : e.s[1], e.v[PC_MIN], e.v[PC_MAX], e.v[PC_INICIAL]);
}

// El chambelán: impuesto (CF_TASA), poner (CF_DEPOSITAR) o sacar (CF_RETIRAR) fondos del castillo.
void abrir_cantidad_castillo(App& a, int tipo, ObjId personaje, i32 min, i32 max, i32 inicial) {
    DialogoCantidad& d = a.cantidad;
    d.dar = d.borrar = false;
    d.tienda = 0;
    d.castillo = static_cast<u8>(tipo + 1);
    d.objeto = personaje;
    d.precio = 0;
    static const char* TEXTOS[] = {"Tax rate (%)", "Deposit how much adena?", "Withdraw how much adena?"};
    d.accion = TEXTOS[tipo];
    abrir(a, "Castle", min, max, inicial);
}

void dar_item(App& a, ObjId objetivo, ObjId item) {
    const TablaItems& t = a.mundo.inventario.items;
    Slot s = t.buscar(item);
    if (s == SIN_SLOT) return;
    DialogoCantidad& d = a.cantidad;
    d.dar = true;
    d.borrar = false;
    d.tienda = d.castillo = 0;
    d.objeto = objetivo;
    d.item = item;
    d.precio = 0;
    const i32 n = static_cast<i32>(t.col<I_CANTIDAD>()[s]);
    char titulo[256];
    const size_t m = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), titulo, sizeof titulo);
    abrir(a, std::string_view(titulo, m), 1, std::max(n, 1), std::max(n, 1));
    if (n <= 1) aceptar(a);  // uno solo: sin preguntar
}

void borrar_item(App& a, ObjId item) {
    const TablaItems& t = a.mundo.inventario.items;
    Slot s = t.buscar(item);
    if (s == SIN_SLOT) return;
    DialogoCantidad& d = a.cantidad;
    d.dar = false;
    d.borrar = true;
    d.tienda = d.castillo = 0;
    d.item = item;
    d.precio = 0;
    const i32 n = std::max(static_cast<i32>(t.col<I_CANTIDAD>()[s]), 1);
    char titulo[256];
    const size_t m = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), titulo, sizeof titulo);
    abrir(a, std::string_view(titulo, m), 1, n, n);
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
    const char* pregunta = d.tienda || d.castillo ? d.accion.c_str() : d.borrar ? (d.max > 1 ? "Delete how many? (cannot be undone)" : "Delete it? (cannot be undone)")
                                    : d.dar ? "Give how many?" : "Amount";
    g.texto(pregunta, static_cast<float>(x + 10), static_cast<float>(y + 30), kt, d.borrar ? rgba(255, 140, 110) : rgba(170, 160, 140));

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
    const bool numero = !(d.borrar && d.max == 1);  // borrar uno solo: solo confirmar
    if (numero && ui::boton_texto(g, e, "-", x + 10, by, 24, 20)) d.valor = std::max(d.min, d.valor - 1);
    char t[48];
    std::snprintf(t, sizeof t, "%d", d.valor);
    if (numero) {
        g.gpu().rectangulo(x + 40, by, 100, 20, clave_orden(CAPA_UI, 3), rgba(40, 32, 22, 255));
        g.texto(t, static_cast<float>(x + 46), static_cast<float>(by + 4), kt, ui::COLOR_TEXTO);
        if (ui::boton_texto(g, e, "+", x + 146, by, 24, 20)) d.valor = std::min(d.max, d.valor + 1);
        if (ui::boton_texto(g, e, "Max", x + 176, by, 40, 20)) d.valor = d.max;
    }
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
