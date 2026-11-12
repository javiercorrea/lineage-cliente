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

}  // namespace lin
