#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "mundo/mundo.h"
#include "mundo/subida_nivel.h"
#include "mundo/tabla_buffs.h"

namespace lin {

void Mundo::limpiar() {
    objetos.limpiar();
    jugador = {};
    segmentos.clear();
    indice_segmentos.limpiar();
    puertas_cerradas.limpiar();
    textos.clear();
    inventario.limpiar();
    en_juego = false;
}

TextoRef guardar_texto(Mundo& m, std::string_view t) {
    if (m.textos.size() > MAX_ARENA_TEXTOS)
        compactar_textos(m.textos, [&](auto&& f) {
            for (Slot s = 0; s < m.objetos.tamano(); ++s) {
                f(m.objetos.col<O_NOMBRE>()[s]);
                f(m.objetos.col<O_TITULO>()[s]);
            }
        });
    TextoRef r{static_cast<u32>(m.textos.size()), static_cast<u16>(t.size() > 0xFFFF ? 0xFFFF : t.size())};
    m.textos.insert(m.textos.end(), t.begin(), t.begin() + r.len);
    return r;
}

// Experiencia acumulada necesaria para cada nivel (1..100). L1J: ExpTable._expTable.
static constexpr i64 EXP_NIVEL[] = {
    0, 125, 300, 500, 750, 1296, 2401, 4096, 6581, 10000, 14661, 20756, 28581, 38436, 50645, 0x10014,
    0x14655, 0x19a24, 0x1fd25, 0x27114, 0x2f7c5, 0x39324, 0x44535, 0x51010, 0x5f5f1, 0x6f920, 0x81c01,
    0x96110, 0xacae1, 0xc5c20, 0xe1791, 0x100010, 0x121891, 0x146420, 0x16e5e1, 0x19a110, 0x1c9901,
    0x1fd120, 0x234cf1, 0x271010, 0x2b1e31, 0x2f7b21, 0x342ac2, 0x393111, 0x3e9222, 0x49b332, 0x60b772,
    0x960cd1, 0x12d4c4e, 0x3539b92, 0x579ead6, 0x7a03a1a, 0x9c6895e, 0xbecd8a2, 0xe1327e6, 0x1039772a,
    0x125fc66e, 0x148615b2, 0x16ac64f6, 0x18d2b43a, 0x1af9037e, 0x1d1f52c2, 0x1f45a206, 0x216bf14a,
    0x2392408e, 0x25b88fd2, 0x27dedf16, 0x2a052e5a, 0x2c2b7d9e, 0x2e51cce2, 0x30781c26, 0x329e6b6a,
    0x34c4baae, 0x36eb09f2, 0x39115936, 0x3b37a87a, 0x3d5df7be, 0x3f844702, 0x41aa9646, 0x43d0e58a,
    0x45f734ce, 0x481d8412, 0x4a43d356, 0x4c6a229a, 0x4e9071de, 0x50b6c122, 0x52dd1066, 0x55035faa,
    0x5729aeee, 0x594ffe32, 0x5b764d76, 0x5d9c9cba, 0x5fc2ebfe, 0x61e93b42, 0x640f8a86, 0x6635d9ca,
    0x685c290e, 0x6a827852, 0x6ca8c796, 0x6ecf16da,
};
static constexpr i32 NIVELES = static_cast<i32>(sizeof(EXP_NIVEL) / sizeof(EXP_NIVEL[0]));

float porcentaje_exp(i32 nivel, i32 exp) {
    if (nivel < 1 || nivel >= NIVELES) return 0.0f;
    i64 base = EXP_NIVEL[nivel - 1], sig = EXP_NIVEL[nivel];
    float p = 100.0f * static_cast<float>(exp - base) / static_cast<float>(sig - base);
    return p < 0 ? 0 : (p > 100 ? 100 : p);
}

i64 exp_escalada(i32 nivel, i32 exp, i64 escala) {
    if (nivel < 1 || nivel >= NIVELES) return 0;
    const i64 base = EXP_NIVEL[nivel - 1], sig = EXP_NIVEL[nivel];
    return exp < base || sig <= base ? 0 : (exp - base) * escala / (sig - base);
}

double factor_intervalo(const EstadoJugador& j, Intervalo t) {
    double f = 1.0;
    if (j.haste == 1) f *= 0.75;
    if (j.haste == 2) f /= 0.75;
    if (j.brave == 1) f *= 0.75;
    if (j.brave == 3) f *= 0.87;
    if (t == Intervalo::CAMINAR && j.brave == 4) f *= 0.75;
    if (t == Intervalo::ATACAR && j.brave == 6) f *= 0.75 * 0.87;
    if (j.tercera) f *= 0.87;
    return f;
}

u32 intervalo_propio(const EstadoJugador& j, Intervalo t, u32 base, u32 ms_encadenar) {
    double minimo = base * factor_intervalo(j, t);  // lo que exige el servidor
    double ms = minimo * 1.05;
    // Un paso encadenado puede empezar hasta ms_encadenar antes, y el servidor mide × 0,97.
    if (ms_encadenar) ms = std::max(ms, minimo / 0.97 + ms_encadenar + 1);
    return static_cast<u32>(ms + 0.999);
}

void agregar_efecto(Mundo& m, const EfectoVisual& e) {
    m.efectos[m.siguiente_efecto] = e;
    m.siguiente_efecto = (m.siguiente_efecto + 1) % MAX_EFECTOS_VISUALES;
}

// Encola el mensaje de fin de efecto de una casilla (cliente original: tabla +0x10), que se muestra un segundo después (ver eventos_hechizos).
static void encolar_fin(EstadoJugador& j, const EfectoActivo& ef, u64 ahora) {
    const int r = ranura_de_efecto_activo(ef.clase, ef.tipo);
    if (r < 0 || !BUFF_FIN_MSG[r] || j.n_fin_pendientes == MAX_FIN_PENDIENTES) return;
    j.fin_pendientes[j.n_fin_pendientes++] = {BUFF_FIN_MSG[r], ahora + 1000};
}

void vencer_efectos(EstadoJugador& j, u64 ahora) {
    for (u32 i = 0; i < j.n_efectos;) {
        if (j.efectos[i].fin <= ahora) {
            encolar_fin(j, j.efectos[i], ahora);
            j.efectos[i] = j.efectos[--j.n_efectos];
        } else {
            ++i;
        }
    }
}

void poner_efecto_activo(EstadoJugador& j, u8 clase, u8 tipo, i32 segundos, u64 ahora) {
    // Haste, brave, fuerza y destreza son uno de cada clase, sea cual sea el tipo (la poción de
    // fuerza termina con otro tipo que el que empezó); los demás, uno por tipo.
    const bool unico = clase == EA_HASTE || clase == EA_BRAVE || clase == EA_FUERZA || clase == EA_DESTREZA;
    auto es_este = [&](const EfectoActivo& e) { return e.clase == clase && (unico || e.tipo == tipo); };
    if (segundos > 0) {
        // Grupos de exclusión de la tabla de buffs (+8): el efecto nuevo quita las otras casillas activas de su grupo distinto de cero.
        const int nueva = ranura_de_efecto_activo(clase, tipo);
        // Fijar una casilla en el original es idempotente: un segundo paquete para la misma casilla reemplaza al primero.
        for (u32 k = 0; k < j.n_efectos && nueva >= 0;) {
            if (!es_este(j.efectos[k]) && ranura_de_efecto_activo(j.efectos[k].clase, j.efectos[k].tipo) == nueva) j.efectos[k] = j.efectos[--j.n_efectos];
            else ++k;
        }
        if (nueva >= 0 && BUFF_GRUPO[nueva]) {
            for (u32 k = 0; k < j.n_efectos;) {
                const int r = ranura_de_efecto_activo(j.efectos[k].clase, j.efectos[k].tipo);
                if (r >= 0 && r != nueva && BUFF_GRUPO[r] == BUFF_GRUPO[nueva] && !es_este(j.efectos[k])) j.efectos[k] = j.efectos[--j.n_efectos];
                else ++k;
            }
        }
    }
    u32 i = 0;
    while (i < j.n_efectos && !es_este(j.efectos[i])) ++i;
    if (segundos <= 0) {
        if (i < j.n_efectos) {
            encolar_fin(j, j.efectos[i], ahora);
            j.efectos[i] = j.efectos[--j.n_efectos];
        }
        return;
    }
    if (i == j.n_efectos) {
        if (j.n_efectos == MAX_EFECTOS_ACTIVOS) return;
        ++j.n_efectos;
    }
    j.efectos[i] = {clase, tipo, ahora + static_cast<u64>(segundos) * 1000};
}

// El exe guarda un byte por casilla de buff, [0x81680c + casilla], activo mientras la casilla corre (0x4647a0 / 0x464760).
bool ranura_activa(const EstadoJugador& j, int ranura) {
    for (u32 k = 0; k < j.n_efectos; ++k)
        if (ranura_de_efecto_activo(j.efectos[k].clase, j.efectos[k].tipo) == ranura) return true;
    return false;
}

// 0x464760: quita el ícono de una casilla en silencio (sin mensaje de fin de efecto).
void quitar_ranura(EstadoJugador& j, int ranura) {
    for (u32 k = 0; k < j.n_efectos;) {
        if (ranura_de_efecto_activo(j.efectos[k].clase, j.efectos[k].tipo) == ranura) j.efectos[k] = j.efectos[--j.n_efectos];
        else ++k;
    }
}

// S_SkillSound propio con el gráfico 870 (Cancellation), exe 0x48bc15..0x48bdb4: el cliente quita los íconos de buff.
void cancelacion_propia(EstadoJugador& j) {
    if (ranura_activa(j, 0x4d)) quitar_ranura(j, 0x4d);
    else if (ranura_activa(j, 0x4e)) quitar_ranura(j, 0x4e);
    if (!ranura_activa(j, 0x2b)) {  // mientras corre la casilla 0x2b (S_SkillSound 0x8ba), los dos rangos de abajo se mantienen
        for (int r = 0xc; r < 0x3b; ++r)
            if (r != 0x2c) quitar_ranura(j, r);
        for (int r = 0x42; r <= 0x48; ++r)
            if (r != 0x46 && r != 0x47) quitar_ranura(j, r);
    }
    for (int r : {3, 0x65, 0x6b, 0x6c, 0x6f, 0x71, 0x72, 0x74, 0x75, 0x78, 0x7b, 0x7c, 0x7d, 0x80, 0x81, 0x84, 0x99}) quitar_ranura(j, r);
    for (int r = 0x53; r <= 0x5a; ++r) quitar_ranura(j, r);
    for (int r = 0x5d; r <= 0x64; ++r) quitar_ranura(j, r);
    for (int r = 0x88; r <= 0x8f; ++r) quitar_ranura(j, r);
    for (int r : {0x51, 0x4f, 0x50})  // solo la primera activa de estas tres
        if (ranura_activa(j, r)) {
            quitar_ranura(j, r);
            break;
        }
}

void poner_ranura(EstadoJugador& j, int ranura, i32 segundos, u64 ahora) {
    poner_efecto_activo(j, EA_ICONO, static_cast<u8>(EFECTO_RANURA + ranura), segundos, ahora);
}

void icono_de_caja(EstadoJugador& j, int subtipo, int segundos, const i32 (&extra)[4], int gfx_propio, int clase, u64 ahora) {
    auto poner = [&](int ranura, i32 seg) { poner_efecto_activo(j, EA_ICONO, static_cast<u8>(EFECTO_RANURA + ranura), seg, ahora); };
    switch (subtipo) {
    case 35:  // polymorph: casilla 0x4b para los sprites 0x16dd-0x16e6, si no 0x27; 0 quita todas las casillas de polymorph
        for (int r : {0x27, 0x4b, 0x6d, 0x70, 0x73}) poner(r, 0);
        if (segundos > 0) poner(gfx_propio >= 0x16dd && gfx_propio <= 0x16e6 ? 0x4b : 0x27, segundos);
        break;
    case 36: poner(0x29, segundos); break;  // chat ban
    case 56:  // casilla 0x5c cuando el segundo h está en 0xc20-0xc29, si no 0x52; t <= 0 quita las dos
        poner(0x52, 0);
        poner(0x5c, 0);
        if (segundos > 0) poner(extra[0] >= 0xc20 && extra[0] <= 0xc29 ? 0x5c : 0x52, segundos);
        break;
    case 59: poner(0x87, 0); break;
    case 74: poner(0x4a, segundos); break;
    case 75: poner(0x87, extra[0] > 0 ? 86400 : 0); break;  // hasta el 59 o n = 0 (el paquete no trae duración)
    case 21: {
        // Cuatro íconos por clase, cada uno de b*16-8 s: elfo oscuro 0x1f 0x1d 0x1e 0x20, ilusionista b3 -> 0x74, el resto 0xe 0x11 0xf 0x10.
        static constexpr int OSCURO[4] = {0x1f, 0x1d, 0x1e, 0x20}, OTRAS[4] = {0xe, 0x11, 0xf, 0x10};
        for (int i = 0; i < 4; ++i) {
            const int r = clase == 4 ? OSCURO[i] : (clase == 6 && i == 3) ? 0x74 : OTRAS[i];
            poner(r, extra[i] > 0 ? extra[i] * 16 - 8 : 0);
        }
        break;
    }
    default: break;
    }
}

// Reproduce el efecto de subida de nivel sobre el jugador cuando el nivel informado subió desde `previo`.
static void efecto_subida_nivel(Mundo& m, i32 previo, u64 ahora) {
    if (!subida_de_nivel(previo, m.jugador.nivel) || m.jugador.id == 0) return;
    EfectoVisual ef;
    ef.gfx = static_cast<GfxId>(GFX_SUBIDA_NIVEL);
    ef.objeto = m.jugador.id;
    ef.inicio = ahora;
    agregar_efecto(m, ef);
}

// Largo del paso de un objeto: la animación de caminar (duracion_paso) con los factores de 0x4cda90 para pasos: brave 1 y 4 x3/4,
// elven wafer (3) x7/8, brave 5 x3/8, tercer acelerador x29/32, y la mitad mientras esperan dos o más pasos en la cola.
u16 duracion_paso_objeto(const Mundo& m, Slot s) {
    const TablaObjetos& o = m.objetos;
    u32 ms = duracion_paso(m, o.col<O_GFX>()[s], o.col<O_ESTADO>()[s], o.col<O_VEL>()[s]);
    const u8 brave = o.col<O_BRAVE>()[s];
    if (brave == 1 || brave == 4) ms = ms * 3 / 4;
    else if (brave == 3) ms = ms * 7 / 8;
    else if (brave == 5) ms = ms * 3 / 8;  // 0x4cda90 lo aplica a toda acción, también a los pasos
    if (o.col<O_TERCERA>()[s]) ms = ms * 29 / 32;
    if (o.col<O_COLA>()[s].n >= 2) ms /= 2;
    return static_cast<u16>(std::max<u32>(ms, 1));
}

void gastar_rafaga(TablaObjetos& o, Slot s, i32 accion) {
    if (accion == ACC_ATACAR && o.col<O_RAFAGA>()[s] > 0) --o.col<O_RAFAGA>()[s];
}

void mover_objeto(Mundo& m, const Evento& e, u64 ahora);
void accion_objeto(Mundo& m, const Evento& e, u64 ahora);
void ataque_objeto(Mundo& m, const Evento& e, u64 ahora);

// El exe encola un paso, una acción o un ataque de un objeto ocupado (S_MoveCharPacket 0x484397, S_AttackPacket 0x487d80...)
// en vez de cortar la animación en curso. El personaje propio no se encola: sus pasos y ataques se predicen localmente.
bool encolar(Mundo& m, const Evento& e, u64 ahora) {
    TablaObjetos& o = m.objetos;
    const Slot s = o.buscar(e.id);
    if (s == SIN_SLOT || e.id == m.jugador.id || !ocupado(m, s, ahora)) return false;
    ColaObjeto& c = o.col<O_COLA>()[s];
    if (c.n == MAX_COLA) {  // muy atrasado: el más viejo se reproduce ya para hacer lugar
        const Evento primero = c.e[0];
        std::copy(c.e + 1, c.e + c.n, c.e);
        --c.n;
        switch (primero.tipo) {
        case Ev::MOVER_OBJETO: mover_objeto(m, primero, ahora); break;
        case Ev::ACCION_OBJETO: accion_objeto(m, primero, ahora); break;
        default: ataque_objeto(m, primero, ahora); break;
        }
    }
    Evento& q = o.col<O_COLA>()[s].e[o.col<O_COLA>()[s].n++];
    q = e;
    q.s[0] = q.s[1] = {};  // estos eventos no traen texto; la arena del cuadro tampoco sobreviviría a la cola
    return true;
}

void mover_objeto(Mundo& m, const Evento& e, u64 ahora) {
    TablaObjetos& o = m.objetos;
    Slot s = o.buscar(e.id);
    if (s == SIN_SLOT) return;
    // Desde donde se lo ve ahora: si el paso anterior no terminó (el servidor adelantó el
    // siguiente), se sigue desde la posición interpolada y no hay salto.
    double x, y;
    posicion_interpolada(m, s, ahora, x, y);
    o.col<O_XANT>()[s] = x;
    o.col<O_YANT>()[s] = y;
    o.col<O_DURMOV>()[s] = duracion_paso_objeto(m, s);
    o.col<O_X>()[s] = static_cast<u16>(e.v[0]);
    o.col<O_Y>()[s] = static_cast<u16>(e.v[1]);
    o.col<O_DIR>()[s] = static_cast<u8>(e.v[2]);
    o.col<O_TMOV>()[s] = ahora;
    o.col<O_ZANCADA>()[s] ^= 1;
    return;
}

void accion_objeto(Mundo& m, const Evento& e, u64 ahora) {
    TablaObjetos& o = m.objetos;
    Slot s = o.buscar(e.id);
    if (s == SIN_SLOT || o.col<O_ESTADO>()[s] == ESTADO_MUERTO) return;  // un cadáver no actúa
    if (e.v[0] == ACC_MORIR) {
        o.col<O_ESTADO>()[s] = ESTADO_MUERTO;
        if (e.id == m.jugador.id) m.jugador.muerto = true;
    }
    // El propio hechizo ya se mostró al pedirlo (predicción): el eco no lo reinicia.
    if (e.id == m.jugador.id && o.col<O_ACCION>()[s] == e.v[0] && ahora - o.col<O_TACCION>()[s] < 500) return;
    o.col<O_ACCION>()[s] = static_cast<u8>(e.v[0]);
    o.col<O_TACCION>()[s] = ahora;
    o.col<O_DANO_RAPIDO>()[s] = 0;  // S_DoActionGFX(2) reproduce el daño con su largo completo
    gastar_rafaga(o, s, e.v[0]);
    fijar_ritmo(m, s);
    // Una puerta queda abierta o cerrada (un portón o una torre, dañados): el estado guarda la
    // última (ver elegir_accion).
    if (m.sprites && accion_fija(m.sprites->tipo(o.col<O_GFX>()[s]), static_cast<u8>(e.v[0]))) {
        o.col<O_ESTADO>()[s] = static_cast<u8>(e.v[0]);
        o.col<O_ACCION>()[s] = 0xFF;
    }
    return;
}

void ataque_objeto(Mundo& m, const Evento& e, u64 ahora) {
    TablaObjetos& o = m.objetos;
    Slot sa = o.buscar(e.id);
    u64 inicio = ahora;
    if (sa != SIN_SLOT && o.col<O_ESTADO>()[sa] != ESTADO_MUERTO) {
        u8 accion = static_cast<u8>(e.v[AT_ACCION]);
        // El propio ataque ya se mostró al pedirlo (predicción): no se reinicia con el eco.
        bool previsto = e.id == m.jugador.id && o.col<O_ACCION>()[sa] == accion && ahora - o.col<O_TACCION>()[sa] < 500;
        if (previsto) {
            inicio = o.col<O_TACCION>()[sa];
        } else {
            o.col<O_ACCION>()[sa] = accion;
            o.col<O_TACCION>()[sa] = ahora;
            o.col<O_DANO_RAPIDO>()[sa] = 0;
            gastar_rafaga(o, sa, e.v[AT_ACCION]);  // el ataque predicho ya lo gastó (atacar, pantalla_juego.cpp)
            fijar_ritmo(m, sa);
            // El atacante gira hacia su objetivo cuando se lo conoce (0x4c64d0 -> 0x4cc6c0); si no, solo el rumbo
            // del paquete (S_AttackPacketForNpc trae el rumbo del objetivo).
            o.col<O_DIR>()[sa] = static_cast<u8>(e.v[AT_DIR]);
            if (const Slot st = o.buscar(static_cast<ObjId>(e.v[AT_OBJETIVO])); st != SIN_SLOT && st != sa) {
                const int dx = (o.col<O_X>()[st] > o.col<O_X>()[sa]) - (o.col<O_X>()[st] < o.col<O_X>()[sa]);
                const int dy = (o.col<O_Y>()[st] > o.col<O_Y>()[sa]) - (o.col<O_Y>()[st] < o.col<O_Y>()[sa]);
                for (int d = 0; d < 8; ++d)
                    if ((dx || dy) && DIR_DX[d] == dx && DIR_DY[d] == dy) o.col<O_DIR>()[sa] = static_cast<u8>(d);
            }
        }
    }
    // El golpe sale en el frame "!" del ataque. Un hechizo o una flecha sale ahí y vuela
    // hasta el objetivo.
    u64 impacto = 0;
    if (sa != SIN_SLOT && m.sprites) {
        // Solo el ataque (1) y el daño (2) suman la postura del arma (SetAction 0x4c96c0); si falta la fila del arma se usa
        // la acción sola.
        GfxId g = o.col<O_GFX>()[sa];
        const u8 base = static_cast<u8>(e.v[AT_ACCION]);
        u8 accion = base == ACC_ATACAR || base == ACC_DANADO ? static_cast<u8>(base + o.col<O_ESTADO>()[sa]) : base;
        if (!m.sprites->accion(g, accion)) accion = base;
        impacto = m.sprites->ms_hasta_impacto(g, accion);
        if (impacto != TablaSprites::SIN_IMPACTO) {  // haste, brave... también corren el cuadro "!"
            const Ritmo rt = ritmo_de(m, sa, accion);
            impacto = static_cast<u32>(u64{impacto} * rt.num / rt.den);
        }
    }
    const bool con_impacto = impacto != TablaSprites::SIN_IMPACTO;
    if (!con_impacto) impacto = 0;
    if (e.v[AT_GFX] > 0) {
        EfectoVisual ef;
        ef.gfx = static_cast<GfxId>(e.v[AT_GFX]);
        ef.dir = static_cast<u8>(e.v[AT_DIR]);
        ef.x0 = e.v[AT_X0];
        ef.y0 = e.v[AT_Y0];
        ef.x1 = e.v[AT_X1];
        ef.y1 = e.v[AT_Y1];
        int celdas = std::max(std::abs(e.v[AT_X1] - e.v[AT_X0]), std::abs(e.v[AT_Y1] - e.v[AT_Y0]));
        ef.vuelo_ms = static_cast<u32>(std::max(1, celdas)) * MS_VUELO_CELDA;
        ef.inicio = inicio + impacto;
        if (m.sprites && m.sprites->flash_de_luz(ef.gfx)) ef.flash = 1;
        agregar_efecto(m, ef);
        impacto += ef.vuelo_ms;
    }
    // Si acertó, el objetivo muestra el daño cuando lo alcanza el golpe.
    Slot so = o.buscar(static_cast<ObjId>(e.v[AT_OBJETIVO]));
    // El estremecimiento ocurre solo en el cuadro "!" del atacante (0x4c8950 -> 0x4cbef0): un ataque sin "!" nunca estremece
    // (un hechizo o una flecha estremece cuando llega su gráfico).
    if (e.v[AT_GOLPE] && (con_impacto || e.v[AT_GFX] > 0) && so != SIN_SLOT && o.col<O_ESTADO>()[so] != ESTADO_MUERTO) {
        // Si el flinch ya se está mostrando (p. ej. predicho por la compensación de ataque, con
        // su t de impacto todavía en el futuro), no reiniciarlo con el eco del servidor.
        const u64 t = o.col<O_TACCION>()[so];
        const bool ya = o.col<O_ACCION>()[so] == ACC_DANADO && (t >= ahora || ahora - t < 300);
        if (!ya) {
            o.col<O_ACCION>()[so] = ACC_DANADO;
            o.col<O_TACCION>()[so] = std::max(ahora, inicio + impacto);
            o.col<O_DANO_RAPIDO>()[so] = 1;  // [+0x11d]: a la mitad de largo
            fijar_ritmo(m, so);
        }
    }
    return;
}

void aplicar_eventos(Mundo& m, std::span<const Evento> eventos, u64 ahora) {
    TablaObjetos& o = m.objetos;
    for (const Evento& e : eventos) {
        switch (e.tipo) {
        case Ev::ENTRAR_JUEGO: m.en_juego = true; break;
        case Ev::ESTADO_PROPIO: {
            EstadoJugador& j = m.jugador;
            const i32 nivel_previo = j.nivel;
            j.id = e.id;
            j.nivel = e.v[EP_NIVEL];
            j.exp = e.v[EP_EXP];
            j.fue = e.v[EP_FUE];
            j.int_ = e.v[EP_INT];
            j.sab = e.v[EP_SAB];
            j.des = e.v[EP_DES];
            j.con = e.v[EP_CON];
            j.car = e.v[EP_CAR];
            j.hp = e.v[EP_HP];
            j.hp_max = e.v[EP_HPMAX];
            j.mp = e.v[EP_MP];
            j.mp_max = e.v[EP_MPMAX];
            j.ca = e.v[EP_CA];
            j.comida = e.v[EP_COMIDA];
            j.peso = e.v[EP_PESO];
            j.legal = e.v[EP_LEGAL];
            efecto_subida_nivel(m, nivel_previo, ahora);
            break;
        }
        case Ev::MAPA:
            if (m.mapa != e.v[0]) {
                // Cambio de mapa: los objetos y segmentos anteriores ya no sirven.
                ObjId yo = m.jugador.id;
                Slot s = o.buscar(yo);
                bool conservar = s != SIN_SLOT;
                u16 gfx = conservar ? o.col<O_GFX>()[s] : 0;
                o.limpiar();
                m.segmentos.clear();
                m.indice_segmentos.limpiar();
                m.puertas_cerradas.limpiar();
                if (conservar) o.col<O_GFX>()[o.insertar(yo)] = gfx;
            }
            m.mapa = static_cast<MapId>(e.v[0]);
            m.submarino = e.v[1] != 0;
            break;
        case Ev::PONER_OBJETO: {
            const bool existia = o.buscar(e.id) != SIN_SLOT;
            Slot s = o.insertar(e.id);
            const u8 accion_previa = existia ? o.col<O_ACCION>()[s] : u8{0xFF};
            const u64 t_previa = existia ? o.col<O_TACCION>()[s] : 0;
            o.col<O_X>()[s] = static_cast<u16>(e.v[OB_X]);
            o.col<O_Y>()[s] = static_cast<u16>(e.v[OB_Y]);
            o.col<O_XANT>()[s] = o.col<O_X>()[s];
            o.col<O_YANT>()[s] = o.col<O_Y>()[s];
            o.col<O_TMOV>()[s] = 0;
            // El byte de velocidad 8 es el tercer acelerador, no una velocidad de paso (0x4843fb).
            o.col<O_VEL>()[s] = e.v[OB_VEL] == 8 ? u8{0} : static_cast<u8>(e.v[OB_VEL]);
            o.col<O_TERCERA>()[s] = e.v[OB_VEL] == 8;
            o.col<O_BRAVE>()[s] = static_cast<u8>(e.v[OB_FLAGS] >> 4 & 7);  // 0x4cceff
            o.col<O_COLA>()[s].n = 0;  // un objeto reenviado (teletransporte, aspecto nuevo) descarta lo que esperaba
            if (!existia) o.col<O_RAFAGA>()[s] = 0;
            o.col<O_CANTIDAD>()[s] = static_cast<u32>(e.v[OB_CANTIDAD]);
            o.col<O_CLAN>()[s] = static_cast<u32>(e.v[OB_CLAN]);
            if (e.id == m.jugador.id) m.jugador.rango_clan = e.v[OB_RANGO];
            o.col<O_LEGAL>()[s] = static_cast<i16>(e.v[OB_LEGAL]);
            o.col<O_DURMOV>()[s] = duracion_paso(m, static_cast<GfxId>(e.v[OB_GFX]), static_cast<u8>(e.v[OB_ESTADO]),
                                                 o.col<O_VEL>()[s]);
            o.col<O_DIR>()[s] = static_cast<u8>(e.v[OB_DIR]);
            o.col<O_GFX>()[s] = static_cast<u16>(e.v[OB_GFX]);
            o.col<O_ESTADO>()[s] = static_cast<u8>(e.v[OB_ESTADO]);
            // Muerto al aparecer (un cadáver en el suelo): directamente el último frame de morir.
            o.col<O_ACCION>()[s] = e.v[OB_ESTADO] == ESTADO_MUERTO ? ACC_MORIR : 0xFF;
            o.col<O_TACCION>()[s] = 0;
            // El servidor manda S_DoActionGFX (aparecer / ocultarse) justo antes del S_NPCPack con el estado nuevo:
            // se deja seguir esa acción reciente en vez de cortarla.
            if (existia && e.v[OB_ESTADO] != ESTADO_MUERTO && accion_previa != 0xFF && ahora - t_previa < 1500) {
                o.col<O_ACCION>()[s] = accion_previa;
                o.col<O_TACCION>()[s] = t_previa;
            }
            fijar_ritmo(m, s);
            if (e.id == m.jugador.id) m.jugador.muerto = e.v[OB_ESTADO] == ESTADO_MUERTO;
            // Tres gfx traen siempre luz 14, mande lo que mande el servidor (4ccf04 del original).
            const i32 gfx_luz = e.v[OB_GFX];
            o.col<O_LUZ>()[s] = gfx_luz == 0x17ae || gfx_luz == 0x17b7 || gfx_luz == 0x17ba ? u8{14} : static_cast<u8>(e.v[OB_LUZ]);
            o.col<O_HP>()[s] = static_cast<u8>(e.v[OB_HP]);
            o.col<O_FLAGS>()[s] = static_cast<u8>(e.v[OB_FLAGS]);
            o.col<O_NOMBRE>()[s] = guardar_texto(m, e.s[0]);
            o.col<O_TITULO>()[s] = guardar_texto(m, e.s[1]);
            o.col<O_NIVEL>()[s] = static_cast<u8>(e.v[OB_NIVEL]);
            o.col<O_MASCOTA>()[s] = static_cast<u8>(e.v[OB_MASCOTA]);
            break;
        }
        case Ev::LICOR: {
            if (const Slot sl = o.buscar(e.id); sl != SIN_SLOT && (e.v[0] == 8 || e.v[0] == 0))
                o.col<O_TERCERA>()[sl] = e.v[0] == 8;  // 0x4894a7
            if (e.id != m.jugador.id) break;
            EstadoJugador& j = m.jugador;
            if (e.v[0] == 8 || e.v[0] == 0) j.tercera = e.v[0] == 8;
            if (e.v[0] == 1 || e.v[0] == 0) j.borracho = e.v[0] == 1;
            // El servidor no manda el tiempo del tercer acelerador: son 600 s (L1BuffUtil).
            if (e.v[0] == 8) poner_efecto_activo(j, EA_TERCERA, 0, 600, ahora);
            if (e.v[0] == 0) poner_efecto_activo(j, EA_TERCERA, 0, 0, ahora);
            break;
        }
        case Ev::ANILLO: {
            const u8 bit = static_cast<u8>(1u << (e.v[0] & 7));
            m.jugador.anillos = static_cast<u8>(e.v[1] ? m.jugador.anillos | bit : m.jugador.anillos & ~bit);
            if (e.v[0] >= 0 && e.v[0] < 6) m.jugador.habilidad[e.v[0]] = static_cast<u8>(e.v[1]);
            break;
        }
        case Ev::NOMBRE_ROSA: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_ROSA>()[s] = e.v[0] > 0 ? ahora + static_cast<u64>(e.v[0]) * 1000 : 0;
            break;
        }
        case Ev::MOVER_OBJETO:
            if (!encolar(m, e, ahora)) mover_objeto(m, e, ahora);
            break;
        case Ev::QUITAR_OBJETO: o.quitar(e.id); break;
        case Ev::CAMBIAR_DIRECCION: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_DIR>()[s] = static_cast<u8>(e.v[0]);
            break;
        }
        case Ev::CAMBIO_FORMA: {
            // Polymorph: otro sprite, con sus propias animaciones y velocidades (también para el
            // anti-acelerador, que mide con el sprite de la forma actual).
            Slot s = o.buscar(e.id);
            if (s == SIN_SLOT) break;
            const GfxId anterior = o.col<O_GFX>()[s];
            o.col<O_GFX>()[s] = static_cast<GfxId>(e.v[CF_GFX]);
            // Atributo 113: el gfx nuevo nombra un efecto que se reproduce en el objeto cuando se transforma en él.
            if (m.sprites && anterior != o.col<O_GFX>()[s])
                if (const GfxId morfo = m.sprites->morfo(o.col<O_GFX>()[s])) {
                    EfectoVisual ef;
                    ef.gfx = morfo;
                    ef.objeto = e.id;
                    ef.inicio = ahora;
                    agregar_efecto(m, ef);
                }
            if (!e.v[CF_ARMA] && o.col<O_ESTADO>()[s] != ESTADO_MUERTO) o.col<O_ESTADO>()[s] = 0;
            if (!moviendose(m, s, ahora))
                o.col<O_DURMOV>()[s] = duracion_paso(m, o.col<O_GFX>()[s], o.col<O_ESTADO>()[s], o.col<O_VEL>()[s]);
            break;
        }
        case Ev::CAMBIO_VISUAL: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_ESTADO>()[s] = static_cast<u8>(e.v[0]);
            break;
        }
        case Ev::ACCION_OBJETO:
            if (!encolar(m, e, ahora)) accion_objeto(m, e, ahora);
            break;
        case Ev::ATAQUE:
            if (!encolar(m, e, ahora)) ataque_objeto(m, e, ahora);
            break;
        case Ev::HECHIZOS:
            for (int i = 0; i < FILAS_HECHIZOS; ++i) {
                u8 bits = static_cast<u8>(static_cast<u32>(e.v[HZ_FILAS + i / 4]) >> (8 * (i % 4)));
                if (e.v[HZ_AGREGAR]) m.jugador.hechizos[i] |= bits;
                else m.jugador.hechizos[i] &= static_cast<u8>(~bits);
            }
            break;
        case Ev::EFECTO: {
            EfectoVisual ef;
            ef.gfx = static_cast<GfxId>(e.v[EF_GFX]);
            ef.objeto = e.id;
            ef.x0 = ef.x1 = e.v[EF_X];
            ef.y0 = ef.y1 = e.v[EF_Y];
            ef.inicio = ahora;
            if (gfx_con_flash(ef.gfx)) ef.flash = 2;
            if (ef.gfx > 0 && !e.v[EF_AREA]) agregar_efecto(m, ef);  // los de área los reparte la app
            // Triple Arrow (4394) y Foe Slayer (7020) sobre un objeto: sus próximos ataques duran un tercio (0x48babd).
            if (ef.gfx == 0x112a || ef.gfx == 0x1b6c)
                if (const Slot sr = o.buscar(e.id); sr != SIN_SLOT) o.col<O_RAFAGA>()[sr] = ef.gfx == 0x112a ? 4 : 3;
            if (ef.gfx > 0 && !e.v[EF_AREA] && e.id == m.jugador.id && m.jugador.id != 0) {
                // S_SkillSound propio: algunos gráficos inician una casilla de buff que dura el tiempo de la tabla (exe 0x48a5c0).
                if (ef.gfx == 0xbe) {
                    // Gráfico 0xbe: la casilla depende del sprite propio (exe 0x48a656). La casilla 0x5b reemplaza a 0x26; 0x4f, 0x50, 0x51
                    // y 0x84 se reemplazan entre sí.
                    const Slot yo = o.buscar(m.jugador.id);
                    const u32 sprite = yo == SIN_SLOT ? 0 : o.col<O_GFX>()[yo];
                    const int r = ranura_de_gfx_be(sprite);
                    if (r == 0x5b) poner_ranura(m.jugador, 0x26, 0, ahora);
                    if (r >= 0x4f && r != 0x5b)
                        for (int otro : {0x4f, 0x50, 0x51, 0x84})
                            if (otro != r) poner_ranura(m.jugador, otro, 0, ahora);
                    if (r >= 0) poner_ranura(m.jugador, r, sprite == 0x9d8 ? 300 : segundos_de_ranura(r), ahora);
                } else {
                    int r = ranura_de_skillsound(static_cast<u32>(ef.gfx));
                    if (ef.gfx == 0x1130 && ranura_activa(m.jugador, 0x42)) r = 0x42;
                    else if (ef.gfx == 0x16c1 && ranura_activa(m.jugador, 0x38)) r = 0x38;
                    else if (ef.gfx == 0xf47 && ranura_activa(m.jugador, 0x44)) quitar_ranura(m.jugador, 0x44);
                    else if (ef.gfx == 0x16c5 && ranura_activa(m.jugador, 0x24)) r = -1;
                    if (ef.gfx == 870) cancelacion_propia(m.jugador);
                    if (const i32 seg = segundos_de_ranura(r); seg > 0) poner_ranura(m.jugador, r, seg, ahora);
                    // Línea de inicio (cola de 0x48b679): el gfx 0x1130 es silencioso en ambas ramas; 0x16c1 al reiniciar la casilla 0x38 no tiene línea.
                    if (r >= 0 && ef.gfx != 0x1130)
                        if (const u16 msg = mensaje_inicio_ranura(r); msg && m.jugador.n_fin_pendientes < MAX_FIN_PENDIENTES)
                            m.jugador.fin_pendientes[m.jugador.n_fin_pendientes++] = {msg, ahora};
                }
            }
            break;
        }
        case Ev::VELOCIDAD: {
            const u8 tipo = static_cast<u8>(e.v[VE_TIPO]);
            if (e.id == m.jugador.id) {
                EstadoJugador& j = m.jugador;
                (e.v[VE_CLASE] == VE_HASTE ? j.haste : j.brave) = tipo;
                if (e.v[VE_CLASE] == VE_HASTE) poner_efecto_activo(j, EA_HASTE, tipo, tipo ? e.v[VE_SEG] : 0, ahora);
                else if (tipo == 0) {  // el tipo 0 limpia las casillas de brave 2, 0x25 y 0x6e
                    poner_efecto_activo(j, EA_BRAVE, 0, 0, ahora);
                } else {
                    poner_efecto_activo(j, EA_BRAVE, tipo_brave(tipo, j.clase), e.v[VE_SEG], ahora);
                }
            }
            // Los pasos de los demás: rápido o lento (los de brave llegan antes y se encadenan solos).
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT && e.v[VE_CLASE] == VE_HASTE) o.col<O_VEL>()[s] = tipo <= 2 ? tipo : 0;
            if (s != SIN_SLOT && e.v[VE_CLASE] == VE_BRAVE) o.col<O_BRAVE>()[s] = tipo;
            break;
        }
        case Ev::ICONO_EFECTO: {
            if (e.v[IE_CLASE] == IE_CONTEXTO) break;  // lo resuelve la app (necesita la clase): icono_de_caja
            static constexpr u8 CLASES[] = {EA_ESCUDO, EA_AURA, EA_ICONO, EA_FUERZA, EA_DESTREZA, EA_EVA};
            poner_efecto_activo(m.jugador, CLASES[e.v[IE_CLASE]], static_cast<u8>(e.v[IE_TIPO]), e.v[IE_SEG], ahora);
            break;
        }
        case Ev::HP_OBJETO: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_HP>()[s] = static_cast<u8>(e.v[0]);
            break;
        }
        case Ev::LUZ: {
            Slot s = o.buscar(e.id);
            if (s == SIN_SLOT) break;
            // sub_4681f0 del original: el propio tiene mínimo 5 y su 20 es el hechizo LIGHT (radio 14);
            // el 20 de los demás vale 5.
            int luz = e.v[0];
            if (e.id == m.jugador.id) {
                m.jugador.luz_hechizo = luz == 20;
                luz = luz == 20 ? 14 : std::max(luz, 5);
            } else if (luz == 20) {
                luz = 5;
            }
            o.col<O_LUZ>()[s] = static_cast<u8>(luz);
            break;
        }
        case Ev::HORA_JUEGO: m.hora = e.v[0]; ++m.hora_n; break;
        case Ev::CLIMA: {
            // S_Weather (489c82 del original): 0xTN, tipo e intensidad. Cada tipo guarda lo suyo: el
            // servidor manda 0x13 y después 0x00, que apaga la nieve y deja la lluvia. Una intensidad
            // mayor que 3 vale 0. El tipo 0x20 no lo manda L1J y no se sabe qué es.
            const int tipo = e.v[0] & 0xF0, n = e.v[0] & 0x0F;
            const u8 intensidad = static_cast<u8>(n <= 3 ? n : 0);
            if (tipo == 0x00) m.nieve = intensidad;
            else if (tipo == 0x10) m.lluvia = intensidad;
            break;
        }
        case Ev::HP: m.jugador.hp = e.v[0]; m.jugador.hp_max = e.v[1]; break;
        case Ev::MP: m.jugador.mp = e.v[0]; m.jugador.mp_max = e.v[1]; break;
        case Ev::EXPERIENCIA: {
            const i32 nivel_previo = m.jugador.nivel;
            m.jugador.nivel = e.v[0];
            m.jugador.exp = e.v[1];
            efecto_subida_nivel(m, nivel_previo, ahora);
            break;
        }
        case Ev::LEGAL: {
            if (e.id == m.jugador.id) m.jugador.legal = e.v[0];
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_LEGAL>()[s] = static_cast<i16>(e.v[0]);
            break;
        }
        case Ev::CAMBIO_NOMBRE: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_NOMBRE>()[s] = guardar_texto(m, e.s[0]);
            break;
        }
        case Ev::TITULO: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_TITULO>()[s] = guardar_texto(m, e.s[0]);
            break;
        }
        case Ev::CLAN_EMBLEMA_OBJETO: {  // sincronización de emblemas de 3.80C: cambió el id de clan (emblema) del objeto
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_CLAN>()[s] = static_cast<u32>(e.v[0]);
            break;
        }
        case Ev::SP_MR: m.jugador.sp = e.v[0]; m.jugador.mr = e.v[1]; break;
        case Ev::DEFENSA: {
            EstadoJugador& j = m.jugador;
            j.ca = e.v[DF_CA];
            j.fuego = e.v[DF_FUEGO];
            j.agua = e.v[DF_AGUA];
            j.viento = e.v[DF_VIENTO];
            j.tierra = e.v[DF_TIERRA];
            break;
        }
        case Ev::ATRIBUTOS: {
            EstadoJugador& j = m.jugador;
            j.fue = e.v[EP_FUE];
            j.int_ = e.v[EP_INT];
            j.sab = e.v[EP_SAB];
            j.des = e.v[EP_DES];
            j.con = e.v[EP_CON];
            j.car = e.v[EP_CAR];
            j.peso = e.v[EP_PESO];
            break;
        }
        // El comentario de S_PacketBox dice "30 pasos", pero el servidor manda la misma escala de
        // 240 que S_OwnCharStatus (verificado: 106 en los dos paquetes para el mismo personaje).
        case Ev::PESO: m.jugador.peso = e.v[0]; break;
        case Ev::COMIDA: m.jugador.comida = e.v[0]; break;
        case Ev::PUERTA: {
            const u64 k = (static_cast<u64>(e.v[PU_X]) << 16) | static_cast<u64>(e.v[PU_Y]);
            if (e.v[PU_BLOQUEADA]) m.puertas_cerradas.poner(k, 1);
            else m.puertas_cerradas.quitar(k);
            break;
        }
        case Ev::INVISIBLE: {
            Slot s = o.buscar(e.id);
            if (s == SIN_SLOT) break;
            u8& f = o.col<O_FLAGS>()[s];
            f = static_cast<u8>(e.v[0] ? f | FLAG_INVISIBLE : f & ~FLAG_INVISIBLE);
            break;
        }
        case Ev::RESUCITAR: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT && o.col<O_ESTADO>()[s] == ESTADO_MUERTO) {
                o.col<O_ESTADO>()[s] = 0;  // el S_CharVisualUpdate que sigue pone el arma
                o.col<O_ACCION>()[s] = 0xFF;
            }
            if (e.id == m.jugador.id) m.jugador.muerto = false;
            break;
        }
        case Ev::VENENO: {
            Slot s = o.buscar(e.id);
            if (s == SIN_SLOT) break;
            u8& f = o.col<O_FLAGS>()[s];
            f = static_cast<u8>((f & ~(FLAG_VENENO | FLAG_CONGELADO)) | (e.v[VE_VERDE] ? FLAG_VENENO : 0) |
                                (e.v[VE_GRIS] ? FLAG_CONGELADO : 0));
            break;
        }
        case Ev::PARALISIS: {
            const u8 t = static_cast<u8>(e.v[0]);
            // Aturdimiento (22 / 23) y atadura (24 / 25) también muestran sus casillas de buff (0x3e, 0x3d).
            if (t == ATURDIDO || t == ATADO) poner_ranura(m.jugador, t == ATURDIDO ? 0x3e : 0x3d, segundos_de_ranura(t == ATURDIDO ? 0x3e : 0x3d), ahora);
            if (t == ATURDIDO + 1 || t == ATADO + 1) poner_ranura(m.jugador, t == ATURDIDO + 1 ? 0x3e : 0x3d, 0, ahora);
            if (t == PARALISIS || t == PARALISIS_2 || t == SUENO || t == CONGELADO || t == ATURDIDO || t == ATADO)
                m.jugador.inmovil = t;
            else if (t != 7 && t == m.jugador.inmovil + 1)
                m.jugador.inmovil = 0;
            break;
        }
        case Ev::MENSAJE_SERVIDOR: {
            // Mensajes que manejan la casilla 0x3f: 0x11e la fija por atoi(arg) * 20 s, 0x120 la limpia, 0x99 fija el valor por omisión de la tabla.
            const int id = e.v[MS_ID];
            if (id == 0x11e) {
                i32 n = 0;
                for (char ch : e.s[0]) {
                    if (ch < '0' || ch > '9') break;
                    n = std::min(n * 10 + (ch - '0'), 100000);
                }
                poner_ranura(m.jugador, 0x3f, n * 20, ahora);
            } else if (id == 0x120) {
                poner_ranura(m.jugador, 0x3f, 0, ahora);
            } else if (id == 0x99) {
                poner_ranura(m.jugador, 0x3f, segundos_de_ranura(0x3f), ahora);
            }
            break;
        }
        case Ev::CEGUERA: m.jugador.ceguera = static_cast<u8>(e.v[0]); break;
        default: break;
        }
    }
    aplicar_inventario(m.inventario, eventos);
}

namespace {

constexpr u32 SEG_NO_EXISTE = 0xFFFFFFFFu, SEG_CARGANDO = 0xFFFFFFFEu;

u64 clave_segmento(MapId mapa, int x, int y) {
    return (static_cast<u64>(mapa) << 32) | (static_cast<u64>(x >> 6) << 16) | static_cast<u64>(y >> 6);
}

void guardar_segmento(Mundo& m, u64 k, SegmentoMapa&& s) {
    m.indice_segmentos.poner(k, static_cast<u32>(m.segmentos.size()));
    m.segmentos.push_back(std::move(s));
}

}  // namespace

// LIN_MAPAS: carpeta de mapas generados (ver generador_mapas/). Si el segmento existe ahí como .mg,
// se usa en vez de la instalación, sin ensuciarla. Solo desarrollo; si no, el .s32 (o .seg) de la instalación.
static std::string ruta_segmento(const Recursos& rec, MapId mapa, int x, int y) {
    if (const char* base = std::getenv("LIN_MAPAS"); base && *base) {
        char seg[48];
        std::snprintf(seg, sizeof seg, "/%u/%04x%04x.mg", static_cast<unsigned>(mapa),
                      (x >> 6) + 0x7E00, (y >> 6) + 0x7E00);
        std::string ruta = base;
        ruta += seg;
        if (std::filesystem::exists(ruta)) return ruta;
    }
    const std::string ruta = rec.ruta_archivo(nombre_segmento(mapa, x, y));
    // Sin el .s32, el formato viejo .seg: Chaotic Temple y el norte de Gludio solo existen así.
    if (!std::filesystem::exists(ruta)) {
        std::string nombre = nombre_segmento(mapa, x, y);
        nombre.replace(nombre.size() - 3, 3, "seg");
        const std::string seg = rec.ruta_archivo(nombre);
        if (std::filesystem::exists(seg)) return seg;
    }
    return ruta;
}

const SegmentoMapa* segmento_cargado(Mundo& m, const Recursos& rec, int x, int y) {
    if (x < 0 || y < 0) return nullptr;
    const u64 k = clave_segmento(m.mapa, x, y);
    if (const u32* i = m.indice_segmentos.buscar(k)) return *i >= SEG_CARGANDO ? nullptr : &m.segmentos[*i];
    if (!m.cargador) m.cargador = std::make_unique<CargadorSegmentos>();
    m.cargador->pedir(ruta_segmento(rec, m.mapa, x, y), k, x & ~63, y & ~63);
    m.indice_segmentos.poner(k, SEG_CARGANDO);
    return nullptr;
}

void actualizar_segmentos(Mundo& m, int x, int y, int radio) {
    if (m.cargador) {
        m.cargador->recoger(m.segmentos_hechos);
        for (CargadorSegmentos::Hecho& h : m.segmentos_hechos) {
            // Si ya no se espera (cambio de mapa, o se cargó en el momento con segmento()), se descarta.
            u32* i = m.indice_segmentos.buscar(h.clave);
            if (!i || *i != SEG_CARGANDO) continue;
            if (h.ok) guardar_segmento(m, h.clave, std::move(h.s));
            else *i = SEG_NO_EXISTE;
        }
        m.segmentos_hechos.clear();
    }
    // Los lejanos se liberan (~100 KB cada uno): el último ocupa el lugar del que se va.
    for (size_t i = 0; i < m.segmentos.size();) {
        const SegmentoMapa& s = m.segmentos[i];
        if (std::abs((s.base_x >> 6) - (x >> 6)) <= radio && std::abs((s.base_y >> 6) - (y >> 6)) <= radio) {
            ++i;
            continue;
        }
        m.indice_segmentos.quitar(clave_segmento(m.mapa, s.base_x, s.base_y));
        if (i + 1 < m.segmentos.size()) {
            m.segmentos[i] = std::move(m.segmentos.back());
            m.indice_segmentos.poner(clave_segmento(m.mapa, m.segmentos[i].base_x, m.segmentos[i].base_y), static_cast<u32>(i));
        }
        m.segmentos.pop_back();
    }
}

const SegmentoMapa* segmento(Mundo& m, const Recursos& rec, int x, int y) {
    if (x < 0 || y < 0) return nullptr;
    const u64 k = clave_segmento(m.mapa, x, y);
    if (const u32* i = m.indice_segmentos.buscar(k); i && *i != SEG_CARGANDO)
        return *i == SEG_NO_EXISTE ? nullptr : &m.segmentos[*i];
    std::vector<u8> datos;
    SegmentoMapa s;
    const std::string ruta = ruta_segmento(rec, m.mapa, x, y);
    bool ok = leer_archivo(ruta, datos) && decodificar_segmento(ruta, datos, x & ~63, y & ~63, s);
    // Linux: la carpeta "map" puede venir como "Map"; leer_archivo resuelve con ruta_real.
    if (!ok) {
        m.indice_segmentos.poner(k, SEG_NO_EXISTE);
        return nullptr;
    }
    guardar_segmento(m, k, std::move(s));
    return &m.segmentos.back();
}

bool celda_pasable(Mundo& m, const Recursos& rec, int x, int y) {
    const SegmentoMapa* s = segmento(m, rec, x, y);
    if (!s) return false;
    if (m.puertas_cerradas.buscar((static_cast<u64>(x) << 16) | static_cast<u64>(y))) return false;
    int lx = x - s->base_x, ly = y - s->base_y;
    // Bit 0 del atributo = media celda bloqueada (verificado contra los mapas del servidor).
    return !((s->atributo_en(lx, ly, 0) & 1) && (s->atributo_en(lx, ly, 1) & 1));
}

TipoZona tipo_zona_celda(Mundo& m, const Recursos& rec, int x, int y) {
    const SegmentoMapa* s = segmento(m, rec, x, y);
    if (!s) return TipoZona::NORMAL;
    return tipo_zona_de_atributo(s->atributo_en(x - s->base_x, y - s->base_y, 0));
}

bool es_portal(Mundo& m, const Recursos& rec, int x, int y) {
    const SegmentoMapa* s = segmento(m, rec, x, y);
    if (!s) return false;
    for (const PortalMapa& p : s->portales)
        if (s->base_x + p.x == x && s->base_y + p.y == y) return true;
    return false;
}

void reiniciar_jugador(Mundo& m) {
    TablaObjetos& o = m.objetos;
    ObjId yo = m.jugador.id;
    Slot s = o.buscar(yo);
    u16 gfx = s != SIN_SLOT ? o.col<O_GFX>()[s] : 0;
    o.limpiar();
    if (s != SIN_SLOT) {
        Slot n = o.insertar(yo);
        o.col<O_GFX>()[n] = gfx;
        o.col<O_ACCION>()[n] = 0xFF;
    }
    m.jugador.muerto = false;
    m.jugador.inmovil = 0;
    m.jugador.ceguera = 0;
}

u16 duracion_paso(const Mundo& m, GfxId gfx, u8 arma, u8 velocidad) {
    if (arma == ESTADO_MUERTO) arma = 0;
    u32 ms = 0;
    if (m.sprites) {
        ms = m.sprites->duracion_ms(gfx, static_cast<u8>(ACC_CAMINAR + arma));
        if (ms == 0) ms = m.sprites->duracion_ms(gfx, ACC_CAMINAR);
    }
    if (ms == 0) ms = DURACION_PASO_MS;
    if (velocidad == 1) ms = ms * 3 / 4;
    else if (velocidad == 2) ms = ms * 4 / 3;
    return static_cast<u16>(ms < 0xFFFF ? ms : 0xFFFF);
}

u8 accion_con_arma(const Mundo& m, Slot s, u8 base) {
    const TablaObjetos& o = m.objetos;
    const u8 estado = o.col<O_ESTADO>()[s];
    if ((base != ACC_ATACAR && base != ACC_DANADO) || estado == ESTADO_MUERTO) return base;
    const u8 a = static_cast<u8>(base + estado);
    return !m.sprites || m.sprites->accion(o.col<O_GFX>()[s], a) ? a : base;
}

Ritmo ritmo_accion(const Mundo& m, Slot s, u8 accion) {
    const TablaObjetos& o = m.objetos;
    Ritmo r;
    auto por = [&r](u32 num, u32 den) { r.num *= num, r.den *= den; };
    if (o.col<O_COLA>()[s].n >= 2) por(1, 2);  // recuperando una cola
    // [+0x11d]: el daño de un golpe va a la mitad, con la fila del arma o, si falta, la de daño sola (accion_con_arma).
    if (o.col<O_DANO_RAPIDO>()[s] && o.col<O_ACCION>()[s] == ACC_DANADO &&
        (accion == static_cast<u8>(o.col<O_ESTADO>()[s] + ACC_DANADO) || accion == ACC_DANADO))
        por(1, 2);
    // Ataques rápidos de Triple Arrow / Foe Slayer: las filas de ataque de espada, hacha, arco, lanza y espada a dos manos.
    if (o.col<O_RAFAGA>()[s] > 0 && (accion == 5 || accion == 12 || accion == 21 || accion == 25 || accion == 51)) por(1, 3);
    if (o.col<O_TERCERA>()[s]) por(29, 32);
    if (o.col<O_VEL>()[s] == 1) por(3, 4);
    else if (o.col<O_VEL>()[s] == 2) por(2, 1);
    // Brave: poción (1) x3/4, elven wafer (3) x7/8, blood lust (6) x3/4 en acciones; Holy Walk (4) solo acelera los pasos.
    switch (o.col<O_BRAVE>()[s]) {
    case 1: case 6: por(3, 4); break;
    case 3: por(7, 8); break;
    case 5: por(3, 8); break;
    default: break;
    }
    return r;
}

void fijar_ritmo(Mundo& m, Slot s) {
    TablaObjetos& o = m.objetos;
    const u8 base = o.col<O_ACCION>()[s];
    if (base == 0xFF) {
        o.col<O_RITMO>()[s] = 0;
        return;
    }
    const Ritmo r = ritmo_accion(m, s, accion_con_arma(m, s, base));
    o.col<O_RITMO>()[s] = (r.num << 16) | (r.den & 0xFFFF);  // num <= 29·2·7 = 406, den <= 2·2·3·32·4·8 = 12288: entran en 16 bits
}

Ritmo ritmo_de(const Mundo& m, Slot s, u8 accion) {
    const TablaObjetos& o = m.objetos;
    const u32 fijo = o.col<O_RITMO>()[s];
    const u8 base = o.col<O_ACCION>()[s];
    if (fijo && base != 0xFF && accion == accion_con_arma(m, s, base)) return {fijo >> 16, fijo & 0xFFFF};
    return ritmo_accion(m, s, accion);
}

namespace {
// Fin de la acción puntual en curso (0 si no hay o no se sabe), con sus factores de cuadro.
u64 fin_accion(const Mundo& m, Slot s) {
    const TablaObjetos& o = m.objetos;
    const u8 base = o.col<O_ACCION>()[s];
    if (base == 0xFF || !m.sprites || o.col<O_ESTADO>()[s] == ESTADO_MUERTO) return 0;
    const u8 accion = accion_con_arma(m, s, base);
    const RangoAccion* r = m.sprites->accion(o.col<O_GFX>()[s], accion);
    if (!r || r->velocidad == 0) return 0;
    const Ritmo rt = ritmo_de(m, s, accion);
    return o.col<O_TACCION>()[s] + u64{m.sprites->ticks(*r)} * 1000 * rt.num / (u64{r->velocidad} * rt.den);
}
}  // namespace

bool ocupado(const Mundo& m, Slot s, u64 ahora) {
    const TablaObjetos& o = m.objetos;
    if (o.col<O_COLA>()[s].n > 0 || moviendose(m, s, ahora)) return true;
    // Una acción que todavía no empezó (un estremecimiento esperando el "!" del atacante) no ocupa al objeto.
    return o.col<O_TACCION>()[s] <= ahora && ahora < fin_accion(m, s);
}

void avanzar_colas(Mundo& m, u64 ahora) {
    TablaObjetos& o = m.objetos;
    for (Slot s = 0; s < o.tamano(); ++s) {
        while (o.col<O_COLA>()[s].n > 0) {
            // La siguiente empieza donde terminó la actual, así los pasos se encadenan sin pausa.
            const u64 fin_paso = o.col<O_TMOV>()[s] + o.col<O_DURMOV>()[s];
            const u64 fa = o.col<O_TACCION>()[s] <= ahora ? fin_accion(m, s) : 0;
            const u64 fin = std::max(fin_paso, fa);
            if (fin > ahora) break;
            ColaObjeto& c = o.col<O_COLA>()[s];
            const Evento e = c.e[0];
            std::copy(c.e + 1, c.e + c.n, c.e);
            --c.n;
            const u64 t = std::max(fin, ahora > 1000 ? ahora - 1000 : u64{0});
            const ObjId id = o.ids()[s];
            switch (e.tipo) {
            case Ev::MOVER_OBJETO: mover_objeto(m, e, t); break;
            case Ev::ACCION_OBJETO: accion_objeto(m, e, t); break;
            default: ataque_objeto(m, e, t); break;
            }
            if (o.buscar(id) != s) break;  // los manejadores nunca quitan objetos, pero por las dudas
        }
    }
}

bool caminando(const Mundo& m, Slot s, u64 ahora) {
    const TablaObjetos& o = m.objetos;
    return o.col<O_TMOV>()[s] > 0 && ahora < o.col<O_TMOV>()[s] + o.col<O_DURMOV>()[s] * 5u / 4u;
}

bool moviendose(const Mundo& m, Slot s, u64 ahora) {
    const TablaObjetos& o = m.objetos;
    return ahora < o.col<O_TMOV>()[s] + o.col<O_DURMOV>()[s];
}

void origen_paso(const Mundo& m, Slot s, u64 ahora, i32& x, i32& y) {
    const TablaObjetos& o = m.objetos;
    x = o.col<O_X>()[s];
    y = o.col<O_Y>()[s];
    if (moviendose(m, s, ahora)) {
        u8 d = o.col<O_DIR>()[s] & 7;
        x -= DIR_DX[d];
        y -= DIR_DY[d];
    }
}

double fraccion_frame(const Mundo& m, Slot s, u64 ahora) {
    if (!moviendose(m, s, ahora)) return 0.0;
    const TablaObjetos& o = m.objetos;
    const u64 transcurrido = ahora - o.col<O_TMOV>()[s], duracion = o.col<O_DURMOV>()[s];
    const RangoAccion* r = nullptr;
    if (m.sprites) {
        const GfxId gfx = o.col<O_GFX>()[s];
        if (m.sprites->zancada(gfx)) r = m.sprites->accion(gfx, m.sprites->accion_caminar(gfx, o.col<O_ZANCADA>()[s]));
        if (!r) r = m.sprites->accion(gfx, static_cast<u8>(ACC_CAMINAR + o.col<O_ESTADO>()[s]));
        if (!r) r = m.sprites->accion(gfx, ACC_CAMINAR);
    }
    u32 total = r ? m.sprites->ticks(*r) : 0;
    if (total == 0) return static_cast<double>(transcurrido * 4 / duracion) / 4.0;
    // La misma cuenta entera que TablaSprites::frame_en con ciclo = duración del paso, para que el
    // frame visible y esta fracción cambien en el mismo instante.
    u64 tt = transcurrido * total / duracion;
    u64 acumulado = 0;
    for (u32 i = 0; i < r->cantidad; ++i) {
        u32 d = m.sprites->frame(r->primer + i).duracion;
        if (tt < acumulado + d) return static_cast<double>(acumulado) / total;
        acumulado += d;
    }
    return 0.0;
}

void posicion_visible(const Mundo& m, Slot s, u64 ahora, double& x, double& y) {
    i32 x0, y0;
    origen_paso(m, s, ahora, x0, y0);
    double avance = fraccion_frame(m, s, ahora);
    u8 d = m.objetos.col<O_DIR>()[s] & 7;
    x = x0 + avance * DIR_DX[d];
    y = y0 + avance * DIR_DY[d];
}

void posicion_interpolada(const Mundo& m, Slot s, u64 ahora, double& x, double& y) {
    const TablaObjetos& o = m.objetos;
    double t = 1.0;
    if (moviendose(m, s, ahora)) {
        t = static_cast<double>(ahora - o.col<O_TMOV>()[s]) / o.col<O_DURMOV>()[s];
    }
    x = o.col<O_XANT>()[s] + (o.col<O_X>()[s] - o.col<O_XANT>()[s]) * t;
    y = o.col<O_YANT>()[s] + (o.col<O_Y>()[s] - o.col<O_YANT>()[s]) * t;
}

}  // namespace lin
