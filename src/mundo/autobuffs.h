// Auto-buffs (PLAN_JUGABILIDAD.md, punto 4): qué buff relanzar y cuándo. Sin estado de la app,
// para los tests; la interfaz y el lanzado están en app/autobuffs_ui.cpp.
#pragma once
#include "mundo/mundo.h"
#include "recursos/hechizos.h"

namespace lin {

constexpr int CASILLEROS_AUTO = 6;
constexpr u64 MARGEN_AUTO_MS = 2000;       // se renueva cuando al efecto le queda menos que esto
constexpr u64 ESPERA_FALLO_MS = 10000;     // tras "MP insuficiente" o "el hechizo falló"
constexpr u64 ESPERA_RESPUESTA_MS = 3000;  // tras lanzarlo, lo que puede tardar en verse el efecto

struct CasilleroAuto {
    u32 hechizo = 0;  // 0 = vacío
    i32 mp = 0;
    u64 duracion_ms = 0;
    u64 lanzado_ms = 0;     // cuándo salió su último C_UseSkill (0 = nunca)
    u64 reintentar_ms = 0;  // tras un fallo, no antes de esto
};

// Hechizo que da un efecto activo, o 0 si viene de un ítem.
u32 hechizo_de_efecto(const EfectoActivo& ef);
// Lo que le queda (ms) al efecto que da el hechizo; 0 si no está activo.
u64 resta_efecto_de_hechizo(const EstadoJugador& j, u32 hechizo, u64 ahora);
// Buff que se puede mantener solo: no es de ataque, dura, y no pide elegir un ítem.
bool acepta_autobuff(const Hechizo& h);
// El primer casillero (de izquierda a derecha) que hay que lanzar ahora, o -1. Lanzar: el efecto no
// está activo o le queda menos de MARGEN_AUTO_MS; si no se lo ve, tampoco se lanzó hace menos de
// su duración (hay buffs sin ícono ni gráfico). Además alcanza el MP, pasó un posible fallo y la
// respuesta del lanzado anterior, y el personaje no está muerto ni inmóvil.
int elegir_autobuff(const EstadoJugador& j, const CasilleroAuto* c, int n, u64 ahora);

}  // namespace lin
