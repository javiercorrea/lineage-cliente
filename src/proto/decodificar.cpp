// Formatos tomados de las clases S_*.java de L1J-JP 3.63c y verificados contra el servidor local
// con herramientas/prueba_login.py.
#include "proto/decodificar.h"
#include <cstdio>
#include <cstring>

namespace lin {

namespace {

struct Lector : LectorBinario {
    Arena* arena;
    u8 c() { return u8_(); }
    u16 h() { return u16_(); }
    u32 d() { return u32_(); }
    // Cadena CP949 terminada en \0, copiada a la arena del frame.
    std::string_view s() {
        size_t ini = pos;
        while (pos < datos.size() && datos[pos] != 0) ++pos;
        std::string_view v(reinterpret_cast<const char*>(datos.data() + ini), pos - ini);
        if (pos < datos.size()) ++pos;
        else error = true;
        return arena->copiar(v);
    }
};

using Decodificador = bool (*)(Lector&, Evento&);

bool ignorar(Lector&, Evento& e) { e.tipo = Ev::IGNORADO; return true; }

// S_ServerVersion: c c d d d d d c c c d d h
bool version(Lector& l, Evento& e) { e.tipo = Ev::VERSION_SERVIDOR; l.c(); e.v[0] = l.c(); return true; }
// S_LoginResult: c motivo (0 ok, 8 usuario/clave, 22 cuenta en uso, 7 ya existe)
bool login(Lector& l, Evento& e) { e.tipo = Ev::RESULTADO_LOGIN; e.v[0] = l.c(); return true; }
bool noticias(Lector& l, Evento& e) { e.tipo = Ev::NOTICIAS; e.s[0] = l.s(); return true; }
// S_CharAmount: c cantidad, c máximo
bool cantidad(Lector& l, Evento& e) { e.tipo = Ev::CANTIDAD_PERSONAJES; e.v[0] = l.c(); e.v[1] = l.c(); return true; }
// S_CharPacks / S_NewCharPacket: s nombre, s clan, c clase, c sexo, h legal, h hp, h mp, c ca,
// c nivel, c fue, c des, c con, c sab, c car, c int, c 0, d cumpleaños, c verificación
bool personaje(Lector& l, Evento& e) {
    e.tipo = (e.origen == Srv::NEWCHARPACK) ? Ev::PERSONAJE_NUEVO : Ev::PERSONAJE;
    e.s[0] = l.s();
    e.s[1] = l.s();
    e.v[PJ_CLASE] = l.c();
    e.v[PJ_SEXO] = l.c();
    e.v[PJ_LEGAL] = static_cast<i16>(l.h());
    e.v[PJ_HP] = l.h();
    e.v[PJ_MP] = l.h();
    e.v[PJ_CA] = static_cast<i8>(l.c());
    e.v[PJ_NIVEL] = l.c();
    e.v[PJ_FUE] = l.c();
    e.v[PJ_DES] = l.c();
    e.v[PJ_CON] = l.c();
    e.v[PJ_SAB] = l.c();
    e.v[PJ_CAR] = l.c();
    e.v[PJ_INT] = l.c();
    return true;
}
// S_CharCreateStatus: c motivo (2 ok, 6 ya existe, 9 nombre inválido, 21 reparto/cantidad)
bool crear(Lector& l, Evento& e) { e.tipo = Ev::RESULTADO_CREAR; e.v[0] = l.c(); return true; }
bool borrado(Lector& l, Evento& e) { e.tipo = Ev::PERSONAJE_BORRADO; e.v[0] = l.c(); return true; }
bool entrar(Lector&, Evento& e) { e.tipo = Ev::ENTRAR_JUEGO; return true; }
bool desconexion(Lector&, Evento& e) { e.tipo = Ev::DESCONECTADO; return true; }
// S_OwnCharStatus: d id, c nivel, d exp, c fue int sab des con car, h hp hpmax mp mpmax, c ca,
// d hora, c comida, c peso, h legal, c fuego agua viento tierra, d muertes
bool estado(Lector& l, Evento& e) {
    e.tipo = Ev::ESTADO_PROPIO;
    e.id = l.d();
    e.v[EP_NIVEL] = l.c();
    e.v[EP_EXP] = static_cast<i32>(l.d());
    for (int i = EP_FUE; i <= EP_CAR; ++i) e.v[i] = l.c();
    e.v[EP_HP] = l.h();
    e.v[EP_HPMAX] = l.h();
    e.v[EP_MP] = l.h();
    e.v[EP_MPMAX] = l.h();
    e.v[EP_CA] = static_cast<i8>(l.c());
    l.d();
    e.v[EP_COMIDA] = l.c();
    e.v[EP_PESO] = l.c();
    e.v[EP_LEGAL] = static_cast<i16>(l.h());
    return true;
}
// S_MapID: h mapa, c bajo el agua
bool mapa(Lector& l, Evento& e) { e.tipo = Ev::MAPA; e.v[0] = l.h(); e.v[1] = l.c(); return true; }
// S_*Pack (personajes, NPCs, puertas...): h x, h y, d id, h gfx, c estado/arma, c dirección,
// c luz, c velocidad, d exp, h legal, s nombre, s título, c flags, d clan, s clan, s ?, c ?, c hp%
bool poner(Lector& l, Evento& e) {
    e.tipo = Ev::PONER_OBJETO;
    e.v[OB_X] = l.h();
    e.v[OB_Y] = l.h();
    e.id = l.d();
    e.v[OB_GFX] = l.h();
    e.v[OB_ESTADO] = l.c();
    e.v[OB_DIR] = l.c() & 7;
    e.v[OB_LUZ] = l.c();
    e.v[OB_VEL] = l.c();
    e.v[OB_CANTIDAD] = static_cast<i32>(l.d());
    e.v[OB_LEGAL] = static_cast<i16>(l.h());
    e.s[0] = l.s();
    e.s[1] = l.s();
    e.v[OB_FLAGS] = l.c();
    e.v[OB_CLAN] = static_cast<i32>(l.d());
    l.s();
    l.s();
    l.c();
    e.v[OB_HP] = l.c();
    l.error = false;  // las variantes (puerta, mascota...) difieren al final; lo leído alcanza
    return true;
}
// S_MoveCharPacket: d id, h x, h y (posición ANTERIOR), c dirección
bool mover(Lector& l, Evento& e) {
    e.tipo = Ev::MOVER_OBJETO;
    e.id = l.d();
    int x = l.h(), y = l.h(), dir = l.c() & 7;
    e.v[0] = x + DIR_DX[dir];
    e.v[1] = y + DIR_DY[dir];
    e.v[2] = dir;
    return true;
}
bool quitar(Lector& l, Evento& e) { e.tipo = Ev::QUITAR_OBJETO; e.id = l.d(); return true; }
bool direccion(Lector& l, Evento& e) { e.tipo = Ev::CAMBIAR_DIRECCION; e.id = l.d(); e.v[0] = l.c() & 7; return true; }
bool luz(Lector& l, Evento& e) { e.tipo = Ev::LUZ; e.id = l.d(); e.v[0] = l.c(); return true; }
bool hora(Lector& l, Evento& e) { e.tipo = Ev::HORA_JUEGO; e.v[0] = static_cast<i32>(l.d()); return true; }
bool clima(Lector& l, Evento& e) { e.tipo = Ev::CLIMA; e.v[0] = l.c(); return true; }
bool hp(Lector& l, Evento& e) { e.tipo = Ev::HP; e.v[0] = l.h(); e.v[1] = l.h(); return true; }
bool mp(Lector& l, Evento& e) { e.tipo = Ev::MP; e.v[0] = l.h(); e.v[1] = l.h(); return true; }
// S_ServerMessage / S_BlueMessage: h id del mensaje (string-e.tbl), c n, n × s argumentos
// (se guardan 2; ningún mensaje de L1J usa más).
bool mensaje(Lector& l, Evento& e) {
    e.tipo = Ev::MENSAJE_SERVIDOR;
    e.v[MS_ID] = l.h();
    int n = l.c();
    e.v[MS_NARGS] = n < 2 ? n : 2;
    e.v[MS_AZUL] = e.origen == Srv::BLUEMESSAGE;
    for (int i = 0; i < n && !l.error; ++i) {
        std::string_view a = l.s();
        if (i < 2) e.s[i] = a;
    }
    return true;
}
// S_ChatPacket / S_NpcChatPacket (NORMALCHAT, NPCSHOUT): c tipo, d id, s texto (el grito agrega
// h x, h y). S_ChatPacket / S_SystemMessage (GLOBALCHAT): c tipo, s texto.
// S_ChatPacket tipo 16 (WHISPERCHAT): s remitente, s texto.
bool chat(Lector& l, Evento& e) {
    e.tipo = Ev::CHAT;
    if (e.origen == Srv::WHISPERCHAT) {
        e.v[0] = CHAT_SUSURRO;
        e.s[1] = l.s();
        e.s[0] = l.s();
        return true;
    }
    e.v[0] = l.c();
    if (e.origen != Srv::GLOBALCHAT) e.id = l.d();
    e.s[0] = l.s();
    return true;
}
// S_Exp: c nivel, d experiencia acumulada
bool experiencia(Lector& l, Evento& e) { e.tipo = Ev::EXPERIENCIA; e.v[0] = l.c(); e.v[1] = static_cast<i32>(l.d()); return true; }
// S_Lawful: d id, h alineamiento, d 0
bool legal(Lector& l, Evento& e) { e.tipo = Ev::LEGAL; e.id = l.d(); e.v[0] = static_cast<i16>(l.h()); return true; }
// S_SpMr: c SP agregado por equipo, c MR agregada
bool sp_mr(Lector& l, Evento& e) { e.tipo = Ev::SP_MR; e.v[0] = static_cast<i8>(l.c()); e.v[1] = static_cast<i8>(l.c()); return true; }
// S_OwnCharAttrDef: c CA, c fuego, c agua, c viento, c tierra
bool defensa(Lector& l, Evento& e) {
    e.tipo = Ev::DEFENSA;
    for (int i = DF_CA; i <= DF_TIERRA; ++i) e.v[i] = static_cast<i8>(l.c());
    return true;
}
// S_OwnCharStatus2: c fue int sab des con car, c peso (0–240). Mismos índices que ESTADO_PROPIO.
bool atributos(Lector& l, Evento& e) {
    e.tipo = Ev::ATRIBUTOS;
    for (int i = EP_FUE; i <= EP_CAR; ++i) e.v[i] = l.c();
    e.v[EP_PESO] = l.c();
    return true;
}
// S_PacketBox: c subtipo y datos según el subtipo. Por ahora:
//  - peso (10, escala de 240) y comida (11, 0–225);
//  - íconos de efectos: S_SkillIconAura (0x16: c índice del hechizo, h segundos) y los de
//    S_SkillIconGFX que manda L1J para efectos con duración (34 poción azul, 40 Immune to Harm:
//    h segundos).
// El resto (grupo, mensajes de guerra...) llega en entregables posteriores.
constexpr int CAJA_AURA = 0x16, CAJA_POCION_AZUL = 34, CAJA_INMUNE = 40;
bool caja(Lector& l, Evento& e) {
    int sub = l.c();
    if (sub == 10 || sub == 11) {
        e.tipo = sub == 10 ? Ev::PESO : Ev::COMIDA;
        e.v[0] = l.c();
    } else if (sub == CAJA_AURA) {
        e.tipo = Ev::ICONO_EFECTO;
        e.v[IE_CLASE] = IE_AURA;
        e.v[IE_TIPO] = l.c();
        e.v[IE_SEG] = l.h();
    } else if (sub == CAJA_POCION_AZUL || sub == CAJA_INMUNE) {
        e.tipo = Ev::ICONO_EFECTO;
        e.v[IE_CLASE] = IE_ICONO;
        e.v[IE_TIPO] = sub;
        e.v[IE_SEG] = l.h();
    } else {
        e.tipo = Ev::IGNORADO;
    }
    return true;
}
// S_DoActionGFX: d id, c acción
bool accion(Lector& l, Evento& e) { e.tipo = Ev::ACCION_OBJETO; e.id = l.d(); e.v[0] = l.c(); return true; }
// S_CharVisualUpdate: d id, c arma/estado
bool visual(Lector& l, Evento& e) { e.tipo = Ev::CAMBIO_VISUAL; e.id = l.d(); e.v[0] = l.c(); return true; }

// S_AttackPacket / S_AttackMissPacket / S_AttackPacketForNpc / S_UseAttackSkill / S_UseArrowSkill:
// c acción, d atacante, d objetivo, h golpe (0 = falló), c dirección, y un resto según la variante:
//  - golpe cuerpo a cuerpo o fallo: 5 bytes (posición del objetivo y marcas);
//  - hechizo o flecha: d número de secuencia, h gráfico del proyectil, c tipo de uso (6 hechizo,
//    127 flecha), h x, h y del atacante, h x, h y del objetivo y 5 bytes en 0.
// Al atacante le llega el suyo, pero no el S_DoActionGFX de daño del objetivo: el cliente lo
// deduce del golpe.
bool ataque(Lector& l, Evento& e) {
    e.tipo = Ev::ATAQUE;
    e.v[AT_ACCION] = l.c();
    e.id = l.d();
    e.v[AT_OBJETIVO] = static_cast<i32>(l.d());
    e.v[AT_GOLPE] = l.h() != 0;
    e.v[AT_DIR] = l.c() & 7;
    if (l.quedan(4 + 2 + 1 + 8)) {
        l.d();
        e.v[AT_GFX] = l.h();
        l.c();
        e.v[AT_X0] = l.h();
        e.v[AT_Y0] = l.h();
        e.v[AT_X1] = l.h();
        e.v[AT_Y1] = l.h();
    }
    return true;
}

// S_AddSkill / S_DelSkill: c cabecera (32, 50 o 100 según los niveles que haya), 28 filas de
// bits (fila = nivel del hechizo; bit c = hechizo fila*8+c+1) y relleno.
bool hechizos(Lector& l, Evento& e) {
    e.tipo = Ev::HECHIZOS;
    e.v[HZ_AGREGAR] = e.origen == Srv::ADDSKILL;
    l.c();
    for (int i = 0; i < FILAS_HECHIZOS; ++i)
        e.v[HZ_FILAS + i / 4] |= static_cast<i32>(static_cast<u32>(l.c()) << (8 * (i % 4)));
    l.error = false;  // el relleno final varía entre versiones
    return true;
}
// S_SkillSound: d objeto, h gráfico (y relleno)
bool efecto_objeto(Lector& l, Evento& e) {
    e.tipo = Ev::EFECTO;
    e.id = l.d();
    e.v[EF_GFX] = l.h();
    return true;
}
// S_EffectLocation: h x, h y, h gráfico
bool efecto_celda(Lector& l, Evento& e) {
    e.tipo = Ev::EFECTO;
    e.v[EF_X] = l.h();
    e.v[EF_Y] = l.h();
    e.v[EF_GFX] = l.h();
    return true;
}
// S_SkillHaste / S_SkillBrave: d objeto, c tipo, h segundos
bool velocidad(Lector& l, Evento& e) {
    e.tipo = Ev::VELOCIDAD;
    e.id = l.d();
    e.v[VE_CLASE] = e.origen == Srv::SKILLHASTE ? VE_HASTE : VE_BRAVE;
    e.v[VE_TIPO] = l.c();
    e.v[VE_SEG] = l.h();
    return true;
}
// S_ChangeShape: d objeto, h gfx, h 29 (o 0 si se sacó el arma)
bool forma(Lector& l, Evento& e) {
    e.tipo = Ev::CAMBIO_FORMA;
    e.id = l.d();
    e.v[CF_GFX] = l.h();
    e.v[CF_ARMA] = l.h() != 0;
    return true;
}
// S_ShowHTML (S_NPCTalkReturn, S_ShowPolyList, S_NoSell...): d objeto, s id de la página y, si
// tiene datos, h 1, h n, n × s (los <var src="#N"> de la página). Los datos se juntan en s[1].
bool html(Lector& l, Evento& e) {
    e.tipo = Ev::HTML;
    e.id = l.d();
    e.s[0] = l.s();
    // S_Party / S_Pledge mandan h 1 o 2 y la cantidad de datos; sin datos, h 0, h 0.
    if (l.quedan(4) && l.h() != 0) {
        int n = std::min<int>(l.h(), 16);
        std::string_view datos[16];
        size_t total = 0;
        for (int i = 0; i < n; ++i) {
            const size_t ini = l.pos;
            while (l.pos < l.datos.size() && l.datos[l.pos] != 0) ++l.pos;
            datos[i] = {reinterpret_cast<const char*>(l.datos.data()) + ini, l.pos - ini};
            total += datos[i].size();
            ++l.pos;
        }
        // S_Pledge manda 3 datos (clan, conectados, todos; nombres separados por espacios) pero
        // las páginas "pledge"/"pledgeM" leen 5: #0 clan, #1 cantidad y #2 nombres de los
        // conectados, #3 y #4 los de todos. Las cantidades las cuenta el cliente.
        char cuentas[2][8] = {};
        if (n == 3 && e.s[0].starts_with("pledge")) {
            for (int j = 0; j < 2; ++j) {
                int c = 0;
                for (size_t i = 0; i < datos[j + 1].size(); ++i)
                    c += datos[j + 1][i] != ' ' && (i == 0 || datos[j + 1][i - 1] == ' ');
                int m = std::snprintf(cuentas[j], sizeof cuentas[j], "%d", c);
                total += static_cast<size_t>(m);
            }
            datos[4] = datos[2];
            datos[3] = {cuentas[1], std::strlen(cuentas[1])};
            datos[2] = datos[1];
            datos[1] = {cuentas[0], std::strlen(cuentas[0])};
            n = 5;
        }
        char* p = l.arena->reservar_arreglo<char>(total + static_cast<size_t>(n));
        if (!p) return false;
        size_t k = 0;
        for (int i = 0; i < n; ++i) {
            if (i) p[k++] = SEPARADOR_DATOS;
            for (char c : datos[i]) p[k++] = c;
        }
        e.s[1] = {p, k};
        e.v[HT_DATOS] = n;
    }
    l.error = false;  // S_NoSell y otras variantes terminan distinto
    return true;
}
// S_MessageYN: h 0, d número de pregunta, h tipo (string-e.tbl), 1 a 3 × s argumentos
bool pregunta(Lector& l, Evento& e) {
    e.tipo = Ev::PREGUNTA;
    l.h();
    e.v[PR_NUMERO] = static_cast<i32>(l.d());
    e.v[PR_TIPO] = l.h();
    if (l.quedan(1)) e.s[0] = l.s();
    if (l.quedan(1)) e.s[1] = l.s();
    l.error = false;
    return true;
}
// S_Trade: s con quién
bool intercambio_abrir(Lector& l, Evento& e) {
    e.tipo = Ev::INTERCAMBIO_ABRIR;
    e.s[0] = l.s();
    return true;
}
// S_TradeAddItem: c lado (0 lo propio, 1 lo del otro), h gfx, s nombre, c bendición, c 0
bool intercambio_item(Lector& l, Evento& e) {
    e.tipo = Ev::INTERCAMBIO_ITEM;
    e.v[IN_LADO] = l.c();
    e.v[IN_GFX] = l.h();
    e.s[0] = l.s();
    e.v[IN_BENDICION] = l.c();
    l.error = false;
    return true;
}
// S_TradeStatus: c 0 terminado, 1 cancelado
bool intercambio_fin(Lector& l, Evento& e) {
    e.tipo = Ev::INTERCAMBIO_FIN;
    e.v[IN_CANCELADO] = l.c();
    return true;
}
// S_CharTitle: d personaje, s título
bool titulo(Lector& l, Evento& e) {
    e.tipo = Ev::TITULO;
    e.id = l.d();
    e.s[0] = l.s();
    return true;
}
// S_Emblem: d clan, 384 bytes (16 × 12 píxeles RGB555), c 0, h 0
bool emblema(Lector& l, Evento& e) {
    e.tipo = Ev::EMBLEMA;
    e.id = l.d();
    if (!l.quedan(BYTES_EMBLEMA)) return false;
    char* p = l.arena->reservar_arreglo<char>(BYTES_EMBLEMA);
    if (!p) return false;
    std::memcpy(p, l.datos.data() + l.pos, BYTES_EMBLEMA);
    l.saltar(BYTES_EMBLEMA);
    e.s[0] = {p, BYTES_EMBLEMA};
    l.error = false;
    return true;
}
// S_SkillBuy: d 100, h n, n × d índice del hechizo (skill id - 1) que todavía no se sabe
bool magia_en_venta(Lector& l, Evento& e) {
    e.tipo = Ev::MAGIA_EN_VENTA;
    l.d();
    int n = l.h();
    for (int i = 0; i < n && !l.error; ++i) {
        u32 k = l.d();
        if (k < 64) e.v[MV_MASCARA + k / 32] |= static_cast<i32>(1u << (k % 32));
    }
    return true;
}
// S_Door: h x, h y, c dirección (0 ／, 1 ＼), c 0 se puede pasar / 1 no
bool puerta(Lector& l, Evento& e) {
    e.tipo = Ev::PUERTA;
    e.v[PU_X] = l.h();
    e.v[PU_Y] = l.h();
    l.c();
    e.v[PU_BLOQUEADA] = l.c() != 0;
    return true;
}
// S_Paralysis: c subtipo (2/3 parálisis, 4/5 parálisis 2, 7 fin de la espera del teleport, 10/11
// sueño, 12/13 congelado, 22/23 aturdido, 24/25 atado; el par empieza y termina)
bool paralisis(Lector& l, Evento& e) { e.tipo = Ev::PARALISIS; e.v[0] = l.c(); return true; }
// S_Poison: d objeto, c verde (veneno), c gris (congelado)
bool veneno(Lector& l, Evento& e) {
    e.tipo = Ev::VENENO;
    e.id = l.d();
    e.v[VE_VERDE] = l.c();
    e.v[VE_GRIS] = l.c();
    return true;
}
// S_CurseBlind: h 0 ve, 1 ciego, 2 ciego con Floating Eye
bool ceguera(Lector& l, Evento& e) { e.tipo = Ev::CEGUERA; e.v[0] = l.h(); return true; }
// S_InputAmount: d objeto, d precio, d inicial, d mínimo, d máximo, h ?, y textos según quién lo
// manda: S_HowManyMake "request", acción; S_ApplyAuction/S_SellHouse "agapply", "agapply N";
// S_HowManyKey (posada): página, h 1, h 2, s NPC, s precio. Se devuelve la acción: el segundo
// texto si el primero es "request" o su prefijo; si no, el primero.
bool pedir_cantidad(Lector& l, Evento& e) {
    e.tipo = Ev::PEDIR_CANTIDAD;
    e.id = l.d();
    e.v[PC_PRECIO] = static_cast<i32>(l.d());
    e.v[PC_INICIAL] = static_cast<i32>(l.d());
    e.v[PC_MIN] = static_cast<i32>(l.d());
    e.v[PC_MAX] = static_cast<i32>(l.d());
    l.h();
    std::string_view s1 = l.s();
    if (l.error) return false;
    std::string_view s2 = l.quedan(1) ? l.s() : std::string_view{};
    const bool segundo = s1 == "request" || (s2.size() > s1.size() && s2.substr(0, s1.size()) == s1 && s2[s1.size()] == ' ');
    e.s[0] = segundo ? s2 : s1;
    if (!segundo && l.quedan(4)) {  // posada: h 1 (se leyó como el segundo texto), h 2, s NPC
        l.h();
        e.s[1] = l.s();
    }
    l.error = false;
    return true;
}
// S_PetCtrlMenu: c 0x0c, h ?, d 0 entra / 1 sale, d mascota y, si entra: h mapa, h 0, h x, h y,
// s nombre. Con otro primer byte es S_CharReset (mismo número), que se ignora.
bool mascota(Lector& l, Evento& e) {
    if (l.c() != 0x0c) {
        e.tipo = Ev::IGNORADO;
        return true;
    }
    e.tipo = Ev::MASCOTA;
    l.h();
    e.v[MA_AGREGAR] = l.d() == 0;
    e.id = l.d();
    if (e.v[MA_AGREGAR]) {
        l.h();
        l.h();
        l.h();
        l.h();
        e.s[0] = l.s();
    }
    return true;
}
// S_SelectTarget: d mascota, c 0, c 0, c 2
bool elegir_objetivo_mascota(Lector& l, Evento& e) { e.tipo = Ev::ELEGIR_OBJETIVO_MASCOTA; e.id = l.d(); return true; }
// S_TrueTarget: d objetivo, d quien lo marcó, s mensaje
bool marca_objetivo(Lector& l, Evento& e) {
    e.tipo = Ev::MARCA_OBJETIVO;
    e.id = l.d();
    l.d();
    e.s[0] = l.s();
    return true;
}
// S_Bookmarks: s nombre, h mapa, d id, h x, h y (uno por marcador; todos al entrar)
bool marcador(Lector& l, Evento& e) {
    e.tipo = Ev::MARCADOR;
    e.s[0] = l.s();
    e.v[MK_MAPA] = l.h();
    e.id = l.d();
    e.v[MK_X] = l.h();
    e.v[MK_Y] = l.h();
    return true;
}
// S_Teleport: c 0, c 0x40, d personaje
bool teletransporte(Lector& l, Evento& e) {
    e.tipo = Ev::TELETRANSPORTE;
    l.c();
    l.c();
    e.id = l.d();
    return true;
}
// S_SkillIconShield: h segundos, c tipo (5 Shield, 3 Shadow Armor, 7 Earth Bless, 10 Iron Skin,
// 6 Earth Skin)
bool icono_escudo(Lector& l, Evento& e) {
    e.tipo = Ev::ICONO_EFECTO;
    e.v[IE_CLASE] = IE_ESCUDO;
    e.v[IE_SEG] = l.h();
    e.v[IE_TIPO] = l.c();
    return true;
}
// S_Strup: h segundos (0 = terminó), c FUE, c peso (/240), c tipo, d 0. S_Dexup: lo mismo sin el
// peso. Tipo 5 Enchant Mighty/Dexterity, 2 Dress Mighty/Dexterity, y las pociones (otros).
bool fuerza_destreza(Lector& l, Evento& e) {
    e.tipo = Ev::ICONO_EFECTO;
    e.v[IE_CLASE] = e.origen == Srv::STRUP ? IE_FUERZA : IE_DESTREZA;
    e.v[IE_SEG] = l.h();
    l.c();
    if (e.origen == Srv::STRUP) l.c();
    e.v[IE_TIPO] = l.c();
    return true;
}
// S_HpMeter: d id, c HP en % (0xFF = ocultar la barra). En L1J-JP solo se manda para el grupo y
// las mascotas; el % de los demás llega en S_CharPack.
bool hp_objeto(Lector& l, Evento& e) { e.tipo = Ev::HP_OBJETO; e.id = l.d(); e.v[0] = l.c(); return true; }

// Ítem de S_InvList y S_AddItem: d id, h catalizador, c uso, c cargas, h gfx de inventario,
// c bendición (0 normal, 1 bendito, 2 maldito...), d cantidad, c identificado, s nombre visible,
// c n + n bytes de atributos, c 10, d 0, d 0, h 0.
// c n + n bytes de atributos (L1ItemInstance.getStatusBytes; 0 si no está identificado), a la arena.
std::string_view atributos_item(Lector& l) {
    const Bytes b = l.tomar(l.c());
    return l.arena->copiar({reinterpret_cast<const char*>(b.data()), b.size()});
}
bool leer_item(Lector& l, Evento& e) {
    e.tipo = Ev::ITEM;
    e.id = l.d();
    l.h();
    e.v[IT_USO] = l.c();
    e.v[IT_CARGAS] = l.c();
    e.v[IT_GFX] = l.h();
    e.v[IT_BENDICION] = l.c();
    e.v[IT_CANTIDAD] = static_cast<i32>(l.d());
    e.v[IT_IDENTIFICADO] = l.c();
    e.s[0] = l.s();
    e.s[1] = atributos_item(l);
    l.saltar(1 + 4 + 4 + 2);
    return true;
}
bool item_nombre(Lector& l, Evento& e) { e.tipo = Ev::ITEM_NOMBRE; e.id = l.d(); e.s[0] = l.s(); return true; }
// S_ItemAmount / S_ItemStatus: d id, s nombre, d cantidad, c n + n bytes de atributos
bool item_cantidad(Lector& l, Evento& e) {
    e.tipo = Ev::ITEM_CANTIDAD;
    e.id = l.d();
    e.s[0] = l.s();
    e.v[IT_CANTIDAD] = static_cast<i32>(l.d());
    e.s[1] = atributos_item(l);
    return true;
}
// S_IdentifyDesc (al usar un pergamino de identificación): h desc del ítem y después lo mismo que
// S_ServerMessage (h mensaje 134–138, c n, n textos: nombre, daño o defensa, peso...).
bool identificado(Lector& l, Evento& e) {
    l.h();
    return mensaje(l, e);
}
// S_Invis: d id (el propio personaje), c 1 invisible / 0 visible. A los demás les llega un S_RemoveObject.
bool invisible(Lector& l, Evento& e) { e.tipo = Ev::INVISIBLE; e.id = l.d(); e.v[0] = l.c(); return true; }
// S_Resurrection: d quién revive, c tipo, d quién lo revivió, d clase. Va a él y a quienes lo ven.
bool resucitar(Lector& l, Evento& e) { e.tipo = Ev::RESUCITAR; e.id = l.d(); return true; }
bool item_quitar(Lector& l, Evento& e) { e.tipo = Ev::ITEM_QUITAR; e.id = l.d(); return true; }
bool item_bendicion(Lector& l, Evento& e) { e.tipo = Ev::ITEM_BENDICION; e.id = l.d(); e.v[IT_BENDICION] = l.c(); return true; }

constexpr Decodificador TABLA[] = {
    ignorar,     // DESCONOCIDO
    ignorar,     // INITPACKET (lo procesa el hilo de red)
    version, login, noticias, cantidad, personaje, personaje, crear, borrado, entrar, desconexion,
    estado, mapa, poner, mover, quitar, direccion, luz, hora, clima, mensaje, hp, mp,
    chat,        // NORMALCHAT
    chat,        // GLOBALCHAT
    visual,
    teletransporte,  // TELEPORT
    accion,
    sp_mr,
    defensa,     // OWNCHARATTRDEF
    titulo,      // CHARTITLE
    experiencia, // EXP
    legal,       // LAWFUL
    chat,        // WHISPERCHAT
    chat,        // NPCSHOUT
    atributos,   // OWNCHARSTATUS2
    caja,        // PACKETBOX
    mensaje,     // BLUEMESSAGE
    ataque,      // ATTACKPACKET
    hp_objeto,   // HPMETER
    ignorar,     // INVLIST (varios eventos: ver decodificar)
    leer_item,   // ADDITEM
    item_quitar, // DELETEINVENTORYITEM
    item_nombre, // ITEMNAME
    item_cantidad,  // ITEMAMOUNT / ITEMSTATUS
    item_bendicion, // ITEMCOLOR
    hechizos,       // ADDSKILL
    hechizos,       // DELSKILL
    efecto_objeto,  // SKILLSOUNDGFX
    efecto_celda,   // EFFECTLOCATION
    velocidad,      // SKILLHASTE
    velocidad,      // SKILLBRAVE
    icono_escudo,   // SKILLICONSHIELD
    ignorar,        // RANGESKILLS (varios eventos: ver decodificar)
    forma,          // POLY
    html,           // SHOWHTML
    ignorar,        // SHOWSHOPBUYLIST (varios eventos: ver decodificar)
    ignorar,        // SHOWSHOPSELLLIST
    ignorar,        // SHOWRETRIEVELIST
    pregunta,       // YES_NO
    intercambio_abrir,  // TRADE
    intercambio_item,   // TRADEADDITEM
    intercambio_fin,    // TRADESTATUS
    emblema,        // EMBLEM
    magia_en_venta, // SKILLBUY
    puerta,         // DOOR
    paralisis,      // PARALYSIS
    veneno,         // POISON
    ceguera,        // CURSEBLIND
    pedir_cantidad, // INPUTAMOUNT
    mascota,        // PETCTRL
    elegir_objetivo_mascota,  // SELECTTARGET
    marca_objetivo, // TRUETARGET
    marcador,       // BOOKMARKS
    ignorar,        // SELECTLIST: varios eventos (decodificar)
    identificado,   // IDENTIFYDESC
    invisible,      // INVIS
    resucitar,      // RESURRECTION
    fuerza_destreza,  // STRUP
    fuerza_destreza,  // DEXUP
};
static_assert(sizeof(TABLA) / sizeof(TABLA[0]) == static_cast<size_t>(Srv::CANTIDAD));

}  // namespace

bool decodificar(Bytes cuerpo, const TablaOpcodes& tabla, Arena& arena, std::vector<Evento>& salida) {
    if (cuerpo.empty()) return false;
    Evento e{};
    e.origen = tabla.srv(cuerpo[0]);
    e.extra = cuerpo[0];
    Lector l;
    l.datos = cuerpo.subspan(1);
    l.arena = &arena;
    if (e.origen == Srv::INVLIST) {
        // S_InvList: c n + n ítems. Vacía el inventario y lo vuelve a llenar.
        int n = l.c();
        Evento vaciar = e;
        vaciar.tipo = Ev::INVENTARIO_VACIAR;
        salida.push_back(vaciar);
        for (int i = 0; i < n && !l.error; ++i) {
            Evento it = e;
            leer_item(l, it);
            if (!l.error) salida.push_back(it);
        }
        return !l.error;
    }
    if (e.origen == Srv::SELECTLIST) {
        // S_FixWeaponList: d precio por punto (200), h n, n × (d arma, c daño). Una lista de tienda
        // (del NPC con el que se habla) con las armas dañadas.
        Evento lista = e;
        lista.tipo = Ev::LISTA_TIENDA;
        const u32 precio = l.d();
        const int n = l.h();
        lista.v[LT_TIPO] = LISTA_REPARAR;
        lista.v[LT_CANTIDAD] = n;
        if (l.error) return false;
        salida.push_back(lista);
        for (int i = 0; i < n && !l.error; ++i) {
            Evento it = e;
            it.tipo = Ev::ITEM_TIENDA;
            it.id = l.d();
            it.v[IT_CANTIDAD] = l.c();
            it.v[TI_PRECIO] = static_cast<i32>(precio * static_cast<u32>(it.v[IT_CANTIDAD]));
            if (!l.error) salida.push_back(it);
        }
        return !l.error;
    }
    if (e.origen == Srv::SHOWSHOPBUYLIST || e.origen == Srv::SHOWSHOPSELLLIST || e.origen == Srv::SHOWRETRIEVELIST) {
        // Listas de tienda y almacén: un LISTA_TIENDA y un ITEM_TIENDA por ítem.
        //  - S_ShopSellList (lo que vende el NPC): d npc, h n, n × (d índice, h gfx, d precio, s nombre,
        //    c largo + atributos), h moneda;
        //  - S_ShopBuyList (lo que compra): d npc, h n, n × (d id del ítem, d precio), h moneda;
        //  - S_RetrieveList (almacén): d npc, h n, c 3, n × (d id, c 0, h gfx, c bendición, d cantidad,
        //    c identificado, s nombre), d 30 (precio por ítem) ...
        Evento lista = e;
        lista.tipo = Ev::LISTA_TIENDA;
        lista.id = l.d();
        int n = l.h();
        lista.v[LT_TIPO] = e.origen == Srv::SHOWSHOPBUYLIST ? LISTA_COMPRA
                           : e.origen == Srv::SHOWSHOPSELLLIST ? LISTA_VENTA : LISTA_RETIRO;
        lista.v[LT_CANTIDAD] = n;
        // Almacén: c 3; inventario de una mascota (S_PetInventory, mismo formato de ítems): c 0x0b.
        if (e.origen == Srv::SHOWRETRIEVELIST && l.c() == 0x0b) lista.v[LT_TIPO] = LISTA_MASCOTA;
        if (l.error) return false;
        salida.push_back(lista);
        for (int i = 0; i < n && !l.error; ++i) {
            Evento it = e;
            it.tipo = Ev::ITEM_TIENDA;
            it.id = l.d();
            it.v[IT_CANTIDAD] = 1;
            if (e.origen == Srv::SHOWSHOPBUYLIST) {
                it.v[IT_GFX] = l.h();
                it.v[TI_PRECIO] = static_cast<i32>(l.d());
                it.s[0] = l.s();
                l.saltar(l.c());
            } else if (e.origen == Srv::SHOWSHOPSELLLIST) {
                it.v[TI_PRECIO] = static_cast<i32>(l.d());
            } else {
                l.c();
                it.v[IT_GFX] = l.h();
                it.v[IT_BENDICION] = l.c();
                it.v[IT_CANTIDAD] = static_cast<i32>(l.d());
                it.v[IT_IDENTIFICADO] = l.c();
                it.s[0] = l.s();
            }
            if (!l.error) salida.push_back(it);
        }
        return !l.error;
    }
    if (e.origen == Srv::RANGESKILLS) {
        // S_RangeSkill: c acción, d lanzador, h x, h y, c dirección, d secuencia, h gráfico, c tipo,
        // h 0, h n, n × (d objetivo, c marca). El lanzador hace la acción y el gráfico se ve sobre
        // cada objetivo.
        Evento acc = e;
        acc.tipo = Ev::ACCION_OBJETO;
        acc.v[0] = l.c();
        acc.id = l.d();
        l.h();
        l.h();
        Evento dir = e;
        dir.tipo = Ev::CAMBIAR_DIRECCION;
        dir.id = acc.id;
        dir.v[0] = l.c() & 7;
        l.d();
        int gfx = l.h();
        l.c();
        l.h();
        int n = l.h();
        if (l.error) return false;
        salida.push_back(dir);
        salida.push_back(acc);
        for (int i = 0; i < n && !l.error; ++i) {
            Evento ef = e;
            ef.tipo = Ev::EFECTO;
            ef.id = l.d();
            ef.v[EF_GFX] = gfx;
            l.c();
            if (!l.error) salida.push_back(ef);
        }
        return !l.error;
    }
    bool ok = TABLA[static_cast<int>(e.origen)](l, e) && !l.error;
    if (ok && e.tipo != Ev::IGNORADO) salida.push_back(e);
    return ok;
}

}  // namespace lin
