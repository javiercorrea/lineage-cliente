#include <algorithm>
#include <cstdlib>
#include "mundo/mundo.h"

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

double factor_intervalo(const EstadoJugador& j, Intervalo t) {
    double f = 1.0;
    if (j.haste == 1) f *= 0.75;
    if (j.haste == 2) f /= 0.75;
    if (j.brave == 1) f *= 0.75;
    if (j.brave == 3) f *= 0.87;
    if (t == Intervalo::CAMINAR && j.brave == 4) f *= 0.75;
    if (t == Intervalo::ATACAR && j.brave == 6) f *= 0.75 * 0.87;
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

void vencer_efectos(EstadoJugador& j, u64 ahora) {
    for (u32 i = 0; i < j.n_efectos;) {
        if (j.efectos[i].fin <= ahora) j.efectos[i] = j.efectos[--j.n_efectos];
        else ++i;
    }
}

// Pone (o quita, con 0 segundos) un efecto activo del propio personaje.
static void poner_efecto_activo(EstadoJugador& j, u8 clase, u8 tipo, i32 segundos, u64 ahora) {
    // Haste y brave son uno de cada clase, sea cual sea el tipo; los demás, uno por tipo.
    bool unico = clase == EA_HASTE || clase == EA_BRAVE;
    u32 i = 0;
    while (i < j.n_efectos && !(j.efectos[i].clase == clase && (unico || j.efectos[i].tipo == tipo))) ++i;
    if (segundos <= 0) {
        if (i < j.n_efectos) j.efectos[i] = j.efectos[--j.n_efectos];
        return;
    }
    if (i == j.n_efectos) {
        if (j.n_efectos == MAX_EFECTOS_ACTIVOS) return;
        ++j.n_efectos;
    }
    j.efectos[i] = {clase, tipo, ahora + static_cast<u64>(segundos) * 1000};
}

void aplicar_eventos(Mundo& m, std::span<const Evento> eventos, u64 ahora) {
    TablaObjetos& o = m.objetos;
    for (const Evento& e : eventos) {
        switch (e.tipo) {
        case Ev::ENTRAR_JUEGO: m.en_juego = true; break;
        case Ev::ESTADO_PROPIO: {
            EstadoJugador& j = m.jugador;
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
            break;
        case Ev::PONER_OBJETO: {
            Slot s = o.insertar(e.id);
            o.col<O_X>()[s] = static_cast<u16>(e.v[OB_X]);
            o.col<O_Y>()[s] = static_cast<u16>(e.v[OB_Y]);
            o.col<O_XANT>()[s] = o.col<O_X>()[s];
            o.col<O_YANT>()[s] = o.col<O_Y>()[s];
            o.col<O_TMOV>()[s] = 0;
            o.col<O_VEL>()[s] = static_cast<u8>(e.v[OB_VEL]);
            o.col<O_CANTIDAD>()[s] = static_cast<u32>(e.v[OB_CANTIDAD]);
            o.col<O_CLAN>()[s] = static_cast<u32>(e.v[OB_CLAN]);
            o.col<O_LEGAL>()[s] = static_cast<i16>(e.v[OB_LEGAL]);
            o.col<O_DURMOV>()[s] = duracion_paso(m, static_cast<GfxId>(e.v[OB_GFX]), static_cast<u8>(e.v[OB_ESTADO]),
                                                 static_cast<u8>(e.v[OB_VEL]));
            o.col<O_DIR>()[s] = static_cast<u8>(e.v[OB_DIR]);
            o.col<O_GFX>()[s] = static_cast<u16>(e.v[OB_GFX]);
            o.col<O_ESTADO>()[s] = static_cast<u8>(e.v[OB_ESTADO]);
            // Muerto al aparecer (un cadáver en el suelo): directamente el último frame de morir.
            o.col<O_ACCION>()[s] = e.v[OB_ESTADO] == ESTADO_MUERTO ? ACC_MORIR : 0xFF;
            o.col<O_TACCION>()[s] = 0;
            if (e.id == m.jugador.id) m.jugador.muerto = e.v[OB_ESTADO] == ESTADO_MUERTO;
            o.col<O_LUZ>()[s] = static_cast<u8>(e.v[OB_LUZ]);
            o.col<O_HP>()[s] = static_cast<u8>(e.v[OB_HP]);
            o.col<O_FLAGS>()[s] = static_cast<u8>(e.v[OB_FLAGS]);
            o.col<O_NOMBRE>()[s] = guardar_texto(m, e.s[0]);
            o.col<O_TITULO>()[s] = guardar_texto(m, e.s[1]);
            break;
        }
        case Ev::MOVER_OBJETO: {
            Slot s = o.buscar(e.id);
            if (s == SIN_SLOT) break;
            // Desde donde se lo ve ahora: si el paso anterior no terminó (el servidor adelantó el
            // siguiente), se sigue desde la posición interpolada y no hay salto.
            double x, y;
            posicion_interpolada(m, s, ahora, x, y);
            o.col<O_XANT>()[s] = x;
            o.col<O_YANT>()[s] = y;
            o.col<O_DURMOV>()[s] = duracion_paso(m, o.col<O_GFX>()[s], o.col<O_ESTADO>()[s], o.col<O_VEL>()[s]);
            o.col<O_X>()[s] = static_cast<u16>(e.v[0]);
            o.col<O_Y>()[s] = static_cast<u16>(e.v[1]);
            o.col<O_DIR>()[s] = static_cast<u8>(e.v[2]);
            o.col<O_TMOV>()[s] = ahora;
            break;
        }
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
            o.col<O_GFX>()[s] = static_cast<GfxId>(e.v[CF_GFX]);
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
        case Ev::ACCION_OBJETO: {
            Slot s = o.buscar(e.id);
            if (s == SIN_SLOT || o.col<O_ESTADO>()[s] == ESTADO_MUERTO) break;  // un cadáver no actúa
            if (e.v[0] == ACC_MORIR) {
                o.col<O_ESTADO>()[s] = ESTADO_MUERTO;
                if (e.id == m.jugador.id) m.jugador.muerto = true;
            }
            // El propio hechizo ya se mostró al pedirlo (predicción): el eco no lo reinicia.
            if (e.id == m.jugador.id && o.col<O_ACCION>()[s] == e.v[0] && ahora - o.col<O_TACCION>()[s] < 500) break;
            o.col<O_ACCION>()[s] = static_cast<u8>(e.v[0]);
            o.col<O_TACCION>()[s] = ahora;
            // Una puerta queda abierta o cerrada: el estado guarda la última (ver elegir_accion).
            if ((e.v[0] == ACC_ABRIR || e.v[0] == ACC_CERRAR) && m.sprites && m.sprites->tipo(o.col<O_GFX>()[s]) == TIPO_PUERTA) {
                o.col<O_ESTADO>()[s] = static_cast<u8>(e.v[0]);
                o.col<O_ACCION>()[s] = 0xFF;
            }
            break;
        }
        case Ev::ATAQUE: {
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
                    o.col<O_DIR>()[sa] = static_cast<u8>(e.v[AT_DIR]);
                }
            }
            // El golpe sale en el frame "!" del ataque. Un hechizo o una flecha sale ahí y vuela
            // hasta el objetivo.
            u64 impacto = 0;
            if (sa != SIN_SLOT && m.sprites) {
                GfxId g = o.col<O_GFX>()[sa];
                u8 accion = static_cast<u8>(e.v[AT_ACCION] + o.col<O_ESTADO>()[sa]);
                impacto = m.sprites->ms_hasta_impacto(g, accion);
                if (impacto == 0) impacto = m.sprites->ms_hasta_impacto(g, static_cast<u8>(e.v[AT_ACCION]));
            }
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
                agregar_efecto(m, ef);
                impacto += ef.vuelo_ms;
            }
            // Si acertó, el objetivo muestra el daño cuando lo alcanza el golpe.
            Slot so = o.buscar(static_cast<ObjId>(e.v[AT_OBJETIVO]));
            if (e.v[AT_GOLPE] && so != SIN_SLOT && o.col<O_ESTADO>()[so] != ESTADO_MUERTO) {
                o.col<O_ACCION>()[so] = ACC_DANADO;
                o.col<O_TACCION>()[so] = std::max(ahora, inicio + impacto);
            }
            break;
        }
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
            if (ef.gfx > 0) agregar_efecto(m, ef);
            break;
        }
        case Ev::VELOCIDAD: {
            const u8 tipo = static_cast<u8>(e.v[VE_TIPO]);
            if (e.id == m.jugador.id) {
                EstadoJugador& j = m.jugador;
                (e.v[VE_CLASE] == VE_HASTE ? j.haste : j.brave) = tipo;
                poner_efecto_activo(j, e.v[VE_CLASE] == VE_HASTE ? EA_HASTE : EA_BRAVE, tipo, tipo ? e.v[VE_SEG] : 0, ahora);
            }
            // Los pasos de los demás: rápido o lento (los de brave llegan antes y se encadenan solos).
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT && e.v[VE_CLASE] == VE_HASTE) o.col<O_VEL>()[s] = tipo <= 2 ? tipo : 0;
            break;
        }
        case Ev::ICONO_EFECTO: {
            static constexpr u8 CLASES[] = {EA_ESCUDO, EA_AURA, EA_ICONO};
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
            if (s != SIN_SLOT) o.col<O_LUZ>()[s] = static_cast<u8>(e.v[0]);
            break;
        }
        case Ev::HORA_JUEGO: m.hora = e.v[0]; break;
        case Ev::CLIMA: m.clima = static_cast<u8>(e.v[0]); break;
        case Ev::HP: m.jugador.hp = e.v[0]; m.jugador.hp_max = e.v[1]; break;
        case Ev::MP: m.jugador.mp = e.v[0]; m.jugador.mp_max = e.v[1]; break;
        case Ev::EXPERIENCIA: m.jugador.nivel = e.v[0]; m.jugador.exp = e.v[1]; break;
        case Ev::LEGAL: {
            if (e.id == m.jugador.id) m.jugador.legal = e.v[0];
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_LEGAL>()[s] = static_cast<i16>(e.v[0]);
            break;
        }
        case Ev::TITULO: {
            Slot s = o.buscar(e.id);
            if (s != SIN_SLOT) o.col<O_TITULO>()[s] = guardar_texto(m, e.s[0]);
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
            if (t == PARALISIS || t == PARALISIS_2 || t == SUENO || t == CONGELADO || t == ATURDIDO || t == ATADO)
                m.jugador.inmovil = t;
            else if (t != 7 && t == m.jugador.inmovil + 1)
                m.jugador.inmovil = 0;
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

const SegmentoMapa* segmento_cargado(Mundo& m, const Recursos& rec, int x, int y) {
    if (x < 0 || y < 0) return nullptr;
    const u64 k = clave_segmento(m.mapa, x, y);
    if (const u32* i = m.indice_segmentos.buscar(k)) return *i >= SEG_CARGANDO ? nullptr : &m.segmentos[*i];
    if (!m.cargador) m.cargador = std::make_unique<CargadorSegmentos>();
    m.cargador->pedir(rec.carpeta() + nombre_segmento(m.mapa, x, y), k, x & ~63, y & ~63);
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
    bool ok = leer_archivo(rec.carpeta() + nombre_segmento(m.mapa, x, y), datos) &&
              decodificar_s32(datos, x & ~63, y & ~63, s);
    // MULTIPLATAFORMA (Linux): la carpeta "map" puede venir como "Map"; ver carpeta_juego_valida.
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
        r = m.sprites->accion(o.col<O_GFX>()[s], static_cast<u8>(ACC_CAMINAR + o.col<O_ESTADO>()[s]));
        if (!r) r = m.sprites->accion(o.col<O_GFX>()[s], ACC_CAMINAR);
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
