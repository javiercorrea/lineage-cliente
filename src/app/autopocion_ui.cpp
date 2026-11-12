// Auto-poción (PLAN_JUGABILIDAD.md, punto 5): con el HP por debajo del umbral, toma la poción de
// curación que corresponde (elegir_pocion: familia y color según lo que falta) en cuanto pasa la
// demora de la anterior. Por personaje: encendida (apagada por defecto) y umbral (60 %), en
// settings.ini ("autopocion=Nombre:1,60"), en Opciones y en el programa de configuración. Es una macro
// del lado del cliente: el mismo C_UseItem que un doble clic. Encendida, un indicador junto al HP
// (rojo si no queda ninguna poción de curación).
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr u64 ESPERA_NO_BEBE_MS = 5000;  // mensaje 698 (Decay Potion): no se puede beber
constexpr int PASOS_BOTON[] = {30, 60, 80, 90};  // el botón de la barra: Off -> 30 -> ... -> 90 -> Off
constexpr u16 GFX_POCION = 536;              // ícono del botón sin pociones (Condensed Greater Healing)

int x_boton(const App& a) { return a.autobuffs.x + ANCHO_AUTO + SEPARACION_AUTO; }

// Off -> 30 -> 60 -> 80 -> 90 -> Off. Un umbral puesto en las opciones (45, 95...) pasa al
// siguiente paso mayor, o a Off si no hay.
void girar(App& a) {
    OpcionPocion& o = a.autopocion.op;
    if (!o.encendida) {
        o.encendida = true;
        o.umbral = PASOS_BOTON[0];
    } else {
        int sig = 0;
        for (int u : PASOS_BOTON)
            if (u > o.umbral) {
                sig = u;
                break;
            }
        if (sig) o.umbral = sig;
        else o.encendida = false;
    }
    guardar_autopocion(a);
}

void cargar(App& a) {
    App::AutoPocion& p = a.autopocion;
    p.de = nombre_personaje(a);
    p.op = autopocion_de(a.cfg, p.de);
    p.proxima_ms = p.bebida_ms = 0;
}

}  // namespace

void guardar_autopocion(App& a) {
    poner_autopocion(a.cfg, a.autopocion.de, a.autopocion.op);
    guardar_config(a.cfg);
}

void procesar_autopocion(App& a) {
    App::AutoPocion& p = a.autopocion;
    if (p.de != nombre_personaje(a)) cargar(a);
    const EstadoJugador& j = a.mundo.jugador;
    const u64 ahora = a.entrada.ahora_ms;
    if (!p.op.encendida || j.muerto || j.hp <= 0 || j.hp_max <= 0 || ahora < p.proxima_ms) return;
    if (j.hp * 100 >= p.op.umbral * j.hp_max) return;
    const PocionElegida e = elegir_pocion(a.mundo.inventario, j.hp, j.hp_max);
    if (!e.id) return;
    usar_item(a, e.id);
    p.bebida_ms = ahora;
    p.proxima_ms = ahora + e.demora_ms;
}

void eventos_autopocion(App& a, const Evento& e) {
    App::AutoPocion& p = a.autopocion;
    if (e.tipo == Ev::MENSAJE_SERVIDOR && e.v[MS_ID] == 698 && p.bebida_ms && a.entrada.ahora_ms - p.bebida_ms < 2000)
        p.proxima_ms = a.entrada.ahora_ms + ESPERA_NO_BEBE_MS;
}

int dibujar_indicador_pocion(App& a, float x, float y, u64 clave) {
    const App::AutoPocion& p = a.autopocion;
    if (!p.op.encendida) return 0;
    const EstadoJugador& j = a.mundo.jugador;
    const bool hay = elegir_pocion(a.mundo.inventario, 0, j.hp_max > 0 ? j.hp_max : 1).id != 0;
    char t[24];
    std::snprintf(t, sizeof t, "Pot<%d%%", p.op.umbral);
    return static_cast<int>(a.g->texto_sombra(t, x, y, clave, hay ? rgba(120, 230, 120) : rgba(255, 80, 60)));
}

bool sobre_boton_pocion(const App& a) {
    return ui::sobre(a.entrada, x_boton(a), a.autobuffs.y, CELDA_AUTO, CELDA_AUTO);
}

void dibujar_boton_pocion(App& a) {
    App::AutoPocion& p = a.autopocion;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const int x = x_boton(a), y = a.autobuffs.y, C = CELDA_AUTO;
    const bool sobre = sobre_boton_pocion(a);
    const u64 kf = clave_orden(CAPA_UI, 5), ki = clave_orden(CAPA_UI, 6), kt = clave_orden(CAPA_UI_TEXTO, 5);
    // Ícono: la poción que tomaría con mucho HP perdido; sin ninguna, una fija y el texto en rojo.
    const EstadoJugador& j = a.mundo.jugador;
    const PocionElegida hay = elegir_pocion(a.mundo.inventario, 0, j.hp_max > 0 ? j.hp_max : 1);
    const Slot s = hay.id ? a.mundo.inventario.items.buscar(hay.id) : SIN_SLOT;
    const u16 gfx = s != SIN_SLOT ? a.mundo.inventario.items.col<I_GFX>()[s] : GFX_POCION;
    g.gpu().rectangulo(x, y, C, C, kf, sobre ? rgba(70, 45, 35, 230) : rgba(20, 18, 15, 200));
    const u32 borde = p.op.encendida ? rgba(220, 110, 70) : rgba(110, 90, 80);
    g.gpu().rectangulo(x, y, C, 1, kf + 1, borde);
    g.gpu().rectangulo(x, y + C - 1, C, 1, kf + 1, borde);
    g.gpu().rectangulo(x, y, 1, C, kf + 1, borde);
    g.gpu().rectangulo(x + C - 1, y, 1, C, kf + 1, borde);
    if (const RegionAtlas& r = g.icono(gfx); r.valida()) g.gpu().dibujar(r, x + 1, y + 1, ki, p.op.encendida ? BLANCO : rgba(90, 90, 90));
    char t[16];
    if (p.op.encendida) std::snprintf(t, sizeof t, "%d%%", p.op.umbral);
    else std::snprintf(t, sizeof t, "Off");
    const u32 color = !p.op.encendida ? rgba(170, 160, 150) : hay.id ? rgba(255, 255, 254) : rgba(255, 80, 60);
    g.texto_sombra(t, static_cast<float>(x + C - 2 - ancho_texto(t, std::strlen(t))), static_cast<float>(y + C - 1 - ALTO_GLIFO),
                   kt + (2ull << 24), color);
    // Nombre bajo el puntero.
    if (sobre) {
        char n[48];
        if (p.op.encendida) std::snprintf(n, sizeof n, "Auto potion: below %d%% HP", p.op.umbral);
        else std::snprintf(n, sizeof n, "Auto potion: off");
        const int w = ancho_texto(n, std::strlen(n));
        const int tx = std::max(4, std::min(e.mx - w / 2, a.gpu.ancho() - w - 4)), ty = y - 2 * ALTO_GLIFO - 12;
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto(n, static_cast<float>(tx), static_cast<float>(ty), clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
    }
    // Clic: solo si el botón se apretó sobre él (soltar algo arrastrado no lo cambia).
    const bool arrastrando = a.arrastrando || a.arrastrando_hechizo || a.arrastrando_atajo >= 0 || a.autobuffs.arrastrando >= 0;
    if (e.pulsado) p.apretado_boton = sobre && !arrastrando;
    if (e.clic && sobre && p.apretado_boton) girar(a);
    if (e.clic) p.apretado_boton = false;
}

}  // namespace lin
