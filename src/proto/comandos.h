// Paquetes cliente -> servidor. Se arman en un buffer reutilizable y se encolan al hilo de red.
#pragma once
#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>
#include "proto/opcodes.h"

namespace lin {

class Escritor {
public:
    Escritor(const TablaOpcodes& t, Cli op) { buf_.push_back(t.cli(op)); }
    Escritor& c(u8 v) { buf_.push_back(v); return *this; }
    Escritor& h(u16 v) { c(static_cast<u8>(v)); return c(static_cast<u8>(v >> 8)); }
    Escritor& d(u32 v) { h(static_cast<u16>(v)); return h(static_cast<u16>(v >> 16)); }
    Escritor& s(std::string_view t) {
        buf_.insert(buf_.end(), t.begin(), t.end());
        return c(0);
    }
    // Cuerpo con relleno a múltiplo de 4 (como mySend del cliente original; el cifrado necesita 4).
    Bytes cuerpo() {
        while (buf_.size() < 4 || buf_.size() % 4) buf_.push_back(0);
        return buf_;
    }

private:
    std::vector<u8> buf_;
};

// Tipos de C_Result: 0 comprar (ítem = índice en la lista de la tienda), 1 vender, 2 guardar en el
// almacén, 3 retirar.
enum TipoResultado : u8 { RESULTADO_COMPRA = 0, RESULTADO_VENTA = 1, RESULTADO_DEPOSITO = 2, RESULTADO_RETIRO = 3 };
struct LineaResultado {
    u32 item, cantidad;
};

namespace cmd {
// Formatos: C_*.java de L1J-JP 3.63c.
inline Escritor version(const TablaOpcodes& t) { return Escritor(t, Cli::CLIENTVERSION); }
inline Escritor login(const TablaOpcodes& t, std::string_view usuario, std::string_view clave) {
    Escritor e(t, Cli::LOGINPACKET);
    e.s(usuario).s(clave);
    return e;
}
inline Escritor lista_personajes(const TablaOpcodes& t) { return Escritor(t, Cli::COMMONCLICK); }
// C_CreateChar: s nombre, c clase, c sexo, c fue, c des, c con, c sab, c car, c int
Escritor crear_personaje(const TablaOpcodes& t, std::string_view nombre, u8 clase, u8 sexo, const u8 stats[6]);
inline Escritor borrar_personaje(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::DELETECHAR);
    e.s(nombre);
    return e;
}
inline Escritor entrar(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::LOGINTOSERVER);
    e.s(nombre);
    return e;
}
// C_MoveChar: h x, h y (posición desde la que se mueve), c dirección
inline Escritor mover(const TablaOpcodes& t, u16 x, u16 y, u8 dir) {
    Escritor e(t, Cli::MOVECHAR);
    e.h(x).h(y).c(dir);
    return e;
}
inline Escritor girar(const TablaOpcodes& t, u8 dir) {
    Escritor e(t, Cli::CHANGEHEADING);
    e.c(dir);
    return e;
}
inline Escritor mantener_vivo(const TablaOpcodes& t) { return Escritor(t, Cli::KEEPALIVE); }
// C_Chat: c tipo (TipoChat), s texto. El servidor arma "Nombre: texto" y lo devuelve también a
// quien habla, así que el cliente no muestra eco local.
inline Escritor chat(const TablaOpcodes& t, u8 tipo, std::string_view texto) {
    Escritor e(t, Cli::CHAT);
    e.c(tipo).s(texto);
    return e;
}
// C_Attack: d objetivo, h x, h y (celda del objetivo; 0 para golpear al aire)
inline Escritor atacar(const TablaOpcodes& t, u32 objetivo, u16 x, u16 y) {
    Escritor e(t, Cli::ATTACK);
    e.d(objetivo).h(x).h(y);
    return e;
}
// C_Restart: sin cuerpo. El servidor devuelve al personaje al punto de reinicio con HP = nivel.
inline Escritor reiniciar(const TablaOpcodes& t) { return Escritor(t, Cli::RESTART); }
// C_UseItem: d ítem y, para pociones, armas y armaduras, c 0. Otros usos (pergaminos de
// teletransporte, varitas...) llevan otros campos: llegan con sus entregables.
inline Escritor usar_item(const TablaOpcodes& t, u32 item) {
    Escritor e(t, Cli::USEITEM);
    e.d(item).c(0);
    return e;
}
// C_PickUpItem / C_DropItem: h x, h y, d ítem, d cantidad (recoger: a 3 celdas o menos)
inline Escritor recoger(const TablaOpcodes& t, u16 x, u16 y, u32 item, u32 cantidad) {
    Escritor e(t, Cli::PICKUPITEM);
    e.h(x).h(y).d(item).d(cantidad);
    return e;
}
inline Escritor soltar(const TablaOpcodes& t, u16 x, u16 y, u32 item, u32 cantidad) {
    Escritor e(t, Cli::DROPITEM);
    e.h(x).h(y).d(item).d(cantidad);
    return e;
}
// C_UseSkill: c fila, c columna (skill id = fila * 8 + columna + 1) y, si tiene objetivo,
// d objetivo, h x, h y. Sin objetivo el paquete queda en 4 bytes y el servidor no lee más.
// (Teletransporte, muro de fuego y llamar al clan llevan otros campos: no se usan todavía.)
inline Escritor usar_hechizo(const TablaOpcodes& t, u32 id, u32 objetivo, u16 x, u16 y) {
    Escritor e(t, Cli::USESKILL);
    e.c(static_cast<u8>((id - 1) / 8)).c(static_cast<u8>((id - 1) % 8));
    if (objetivo) e.d(objetivo).h(x).h(y);
    return e;
}
// C_UseItem de un pergamino de polymorph (uso 16): d ítem, s monstruo (como en PolyTable del
// servidor; "none" vuelve a la forma normal).
// C_UseItem de un ítem que se usa sobre otro (identificar, encantar arma o armadura): d ítem, d objetivo.
inline Escritor usar_item_sobre(const TablaOpcodes& t, u32 item, u32 objetivo) {
    Escritor e(t, Cli::USEITEM);
    e.d(item).d(objetivo);
    return e;
}
inline Escritor usar_item_texto(const TablaOpcodes& t, u32 item, std::string_view texto) {
    Escritor e(t, Cli::USEITEM);
    e.d(item).s(texto);
    return e;
}
// C_NPCAction: d objeto, s acción. Shape Change responde así a la lista "monlist" (con el propio id).
inline Escritor accion_npc(const TablaOpcodes& t, u32 objeto, std::string_view accion) {
    Escritor e(t, Cli::NPCACTION);
    e.d(objeto).s(accion);
    return e;
}
// C_NPCTalk: d NPC. El servidor responde con su página (S_NPCTalkReturn).
inline Escritor hablar_npc(const TablaOpcodes& t, u32 npc) {
    Escritor e(t, Cli::NPCTALK);
    e.d(npc);
    return e;
}
// C_Result: d NPC, c tipo, c cantidad, c 0, cantidad × (d ítem, d cantidad).
inline Escritor resultado(const TablaOpcodes& t, u32 npc, TipoResultado tipo, std::span<const LineaResultado> lineas) {
    Escritor e(t, Cli::RESULT);
    e.d(npc).c(tipo).c(static_cast<u8>(lineas.size())).c(0);
    for (const LineaResultado& l : lineas) e.d(l.item).d(l.cantidad);
    return e;
}
// C_Attr con el código 479 (S_BonusStats, la página "RaiseAttr" desde el nivel 51): c 1 (sí) y el
// atributo a subir ("str", "dex", "con", "int", "wis", "cha"). Los vínculos de esa página son
// "raise-str"...: el cliente los traduce a este paquete (C_NPCAction no los entiende).
constexpr u16 ATTR_SUBIR_ATRIBUTO = 479;
inline Escritor subir_atributo(const TablaOpcodes& t, std::string_view atributo) {
    Escritor e(t, Cli::ATTR);
    e.h(ATTR_SUBIR_ATRIBUTO).c(1).s(atributo);
    return e;
}
// C_Attr como respuesta a S_MessageYN: h 0, d número, h tipo, h 1 sí / 0 no.
inline Escritor responder(const TablaOpcodes& t, u32 numero, u16 tipo, bool si) {
    Escritor e(t, Cli::ATTR);
    e.h(0).d(numero).h(tipo).h(si ? 1 : 0);
    return e;
}
// Grupo. C_CreateParty: c 0 (reparto normal; 1 automático), d personaje invitado.
inline Escritor invitar_grupo(const TablaOpcodes& t, u32 personaje) {
    Escritor e(t, Cli::CREATEPARTY);
    e.c(0).d(personaje);
    return e;
}
inline Escritor ver_grupo(const TablaOpcodes& t) { return Escritor(t, Cli::PARTYLIST); }  // S_Party "party"
inline Escritor dejar_grupo(const TablaOpcodes& t) { return Escritor(t, Cli::LEAVEPARTY); }
inline Escritor echar_del_grupo(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::BANPARTY);
    e.s(nombre);
    return e;
}
// Intercambio. C_Trade no lleva a quién: el servidor toma al personaje de enfrente (una celda en
// la dirección propia, mirándose; FaceToFace). Después C_TradeAddItem (d ítem, d cantidad),
// C_TradeOK y C_TradeCancel.
inline Escritor pedir_intercambio(const TablaOpcodes& t) { return Escritor(t, Cli::TRADE); }
inline Escritor ofrecer_item(const TablaOpcodes& t, u32 item, u32 cantidad) {
    Escritor e(t, Cli::TRADEADDITEM);
    e.d(item).d(cantidad);
    return e;
}
inline Escritor aceptar_intercambio(const TablaOpcodes& t) { return Escritor(t, Cli::TRADEADDOK); }
inline Escritor cancelar_intercambio(const TablaOpcodes& t) { return Escritor(t, Cli::TRADEADDCANCEL); }
// Clan. Crear (s nombre; solo príncipes, 30 000 adena), entrar (frente a frente con su príncipe,
// sin cuerpo), dejarlo, echar a alguien (s nombre), ver los miembros (S_Pledge "pledge") y pedir el
// emblema de un clan (d clan -> S_Emblem).
inline Escritor crear_clan(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::CREATECLAN);
    e.s(nombre);
    return e;
}
inline Escritor entrar_clan(const TablaOpcodes& t) { return Escritor(t, Cli::JOINCLAN); }
inline Escritor dejar_clan(const TablaOpcodes& t) { return Escritor(t, Cli::LEAVECLAN); }
inline Escritor echar_del_clan(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::BANCLAN);
    e.s(nombre);
    return e;
}
inline Escritor ver_clan(const TablaOpcodes& t) { return Escritor(t, Cli::PLEDGE); }
inline Escritor pedir_emblema(const TablaOpcodes& t, u32 clan) {
    Escritor e(t, Cli::CLAN);
    e.d(clan);
    return e;
}
// C_Emblem: 384 bytes (16 × 12 píxeles RGB555): el emblema del propio clan (solo su príncipe).
inline Escritor subir_emblema(const TablaOpcodes& t, std::span<const u8> datos) {
    Escritor e(t, Cli::EMBLEM);
    for (u8 b : datos) e.c(b);
    return e;
}
// Maestro de magia. C_SkillBuy: d NPC (el vínculo "buyspell" de su página); el servidor
// responde con S_SkillBuy. C_SkillBuyOK: h n, n × d índice (skill id - 1).
inline Escritor pedir_magia(const TablaOpcodes& t, u32 npc) {
    Escritor e(t, Cli::SKILLBUY);
    e.d(npc);
    return e;
}
inline Escritor aprender_magia(const TablaOpcodes& t, std::span<const u32> indices) {
    Escritor e(t, Cli::SKILLBUYOK);
    e.h(static_cast<u16>(indices.size()));
    for (u32 i : indices) e.d(i);
    return e;
}
// C_Door: h x, h y, d puerta (abre o cierra; las de llave consumen la llave).
inline Escritor puerta(const TablaOpcodes& t, u16 x, u16 y, u32 id) {
    Escritor e(t, Cli::DOOR);
    e.h(x);
    e.h(y);
    e.d(id);
    return e;
}
// C_DeleteInventoryItem: d ítem, d cantidad (el basurero del inventario; 0 sería toda la pila).
inline Escritor borrar_item(const TablaOpcodes& t, u32 item, u32 cantidad) {
    Escritor e(t, Cli::DELETEINVENTORYITEM);
    e.d(item).d(cantidad);
    return e;
}
// C_EnterPortal: h x, h y de la celda del portal (el servidor la busca en DungeonTable de su mapa).
inline Escritor entrar_portal(const TablaOpcodes& t, u16 x, u16 y) {
    Escritor e(t, Cli::ENTERPORTAL);
    e.h(x).h(y);
    return e;
}
// C_CharReset (reiniciar atributos): 1 + los 6 iniciales (fue, int, sab, des, con, car); 2 + paso
// (0 sube un nivel, 7 diez, 1–6 un nivel y un punto en ese atributo, 8 + atributo termina);
// 3 + los 6 finales (después de repartir los puntos de elixir).
inline Escritor reinicio(const TablaOpcodes& t, u8 etapa, std::initializer_list<u8> datos) {
    Escritor e(t, Cli::CHARRESET);
    e.c(etapa);
    for (u8 v : datos) e.c(v);
    return e;
}
// C_Exclude: s nombre (bloquear o desbloquear: el servidor responde S_PacketBox 18 o 19).
inline Escritor bloquear(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::EXCLUDE);
    e.s(nombre);
    return e;
}
// C_Buddy (lista de amigos: S_Buddy, la página "buddy"), C_AddBuddy / C_DeleteBuddy: s nombre.
inline Escritor amigos(const TablaOpcodes& t) { return Escritor(t, Cli::BUDDYLIST); }
inline Escritor amigo(const TablaOpcodes& t, bool agregar, std::string_view nombre) {
    Escritor e(t, agregar ? Cli::ADDBUDDY : Cli::DELETEBUDDY);
    e.s(nombre);
    return e;
}
// C_ChatParty (grupo de chat): 0 + s nombre echa, 1 sale, 2 muestra la lista.
inline Escritor grupo_chat(const TablaOpcodes& t, u8 tipo, std::string_view nombre = {}) {
    Escritor e(t, Cli::CHATPARTY);
    e.c(tipo);
    if (tipo == 0) e.s(nombre);
    return e;
}
// Tienda de jugadores. C_Shop: c 0, h n venta, n × (d ítem, d precio, d cantidad), h n compra
// (ídem), y el título (los bytes que siguen: se repiten a los demás en S_DoActionShop); c 1 la cierra.
struct LineaTienda {
    u32 item, precio, cantidad;
};
inline Escritor abrir_tienda(const TablaOpcodes& t, std::span<const LineaTienda> venta, std::string_view titulo) {
    Escritor e(t, Cli::SHOP);
    e.c(0).h(static_cast<u16>(venta.size()));
    for (const LineaTienda& l : venta) e.d(l.item).d(l.precio).d(l.cantidad);
    e.h(0).s(titulo);
    return e;
}
inline Escritor cerrar_tienda(const TablaOpcodes& t) {
    Escritor e(t, Cli::SHOP);
    e.c(1);
    return e;
}
// C_ShopList: c tipo (0 lo que vende, 1 lo que compra), d vendedor. Responde S_PrivateShop.
inline Escritor ver_tienda(const TablaOpcodes& t, u8 tipo, u32 vendedor) {
    Escritor e(t, Cli::SHOPLIST);
    e.c(tipo).d(vendedor);
    return e;
}
// Venderle a la tienda (C_Result tipo 1): d vendedor, c 1, c 1, c 0, d ítem propio, 3 bytes de
// cantidad (readCH), c orden. Comprarle es C_Result tipo 0 con (orden, cantidad) por línea.
inline Escritor vender_a_tienda(const TablaOpcodes& t, u32 vendedor, u32 item, u32 cantidad, u8 orden) {
    Escritor e(t, Cli::RESULT);
    e.d(vendedor).c(RESULTADO_VENTA).c(1).c(0).d(item);
    e.c(static_cast<u8>(cantidad)).c(static_cast<u8>(cantidad >> 8)).c(static_cast<u8>(cantidad >> 16)).c(orden);
    return e;
}
// Tablón de los pueblos. C_Board: d tablón (responde S_Board, la primera página). C_BoardBack: d
// tablón, d el último tema visto (la página siguiente). C_BoardRead: d tablón, d tema (S_BoardRead).
// C_BoardWrite: d tablón, s título, s texto (cuesta 300 adena). C_BoardDelete: d tablón, d tema.
inline Escritor tablon(const TablaOpcodes& t, Cli c, u32 tablon_id, u32 tema = 0) {
    Escritor e(t, c);
    e.d(tablon_id);
    if (c != Cli::BOARD) e.d(tema);
    return e;
}
inline Escritor escribir_tablon(const TablaOpcodes& t, u32 tablon_id, std::string_view titulo, std::string_view texto) {
    Escritor e(t, Cli::BOARDWRITE);
    e.d(tablon_id).s(titulo).s(texto);
    return e;
}
// Correo. C_Mail: c tipo; 0–2 abre esa bandeja; 0x10–0x12 + d carta la lee; 0x30–0x32 + d la borra;
// 0x40 + d la guarda; 0x20 (50 adena) o 0x21 (al clan, 1000 adena) + h 0 + s destinatario (o clan)
// + asunto y texto en UTF-16LE, cada uno terminado en 0 0.
inline Escritor correo(const TablaOpcodes& t, u8 tipo, u32 carta = 0) {
    Escritor e(t, Cli::MAIL);
    e.c(tipo);
    if (tipo >= 0x10) e.d(carta);
    return e;
}
inline Escritor escribir_correo(const TablaOpcodes& t, bool al_clan, std::string_view a_quien, std::string_view asunto,
                                std::string_view texto) {
    Escritor e(t, Cli::MAIL);
    e.c(al_clan ? 0x21 : 0x20).h(0).s(a_quien);
    for (std::string_view parte : {asunto, texto}) {
        for (char ch : parte) e.c(static_cast<u8>(ch)).c(0);
        e.c(0).c(0);
    }
    return e;
}
// C_GiveItem: d a quién (NPC o mascota), h x, h y (no se usan), d ítem, d cantidad.
inline Escritor dar_item(const TablaOpcodes& t, u32 objetivo, u16 x, u16 y, u32 item, u32 cantidad) {
    Escritor e(t, Cli::GIVEITEM);
    e.d(objetivo);
    e.h(x);
    e.h(y);
    e.d(item);
    e.d(cantidad);
    return e;
}
// C_Amount: d objeto (el de S_InputAmount), d cantidad, c 0, s acción.
inline Escritor cantidad(const TablaOpcodes& t, u32 objeto, u32 n, std::string_view accion) {
    Escritor e(t, Cli::AMOUNT);
    e.d(objeto);
    e.d(n);
    e.c(0);
    e.s(accion);
    return e;
}
// C_PetMenu: d mascota (responde S_PetInventory).
inline Escritor inventario_mascota(const TablaOpcodes& t, u32 mascota) {
    Escritor e(t, Cli::PETMENU);
    e.d(mascota);
    return e;
}
// C_UsePetItem: c ? (se devuelve en S_PetEquipment), d mascota, c fila de su inventario.
inline Escritor usar_item_mascota(const TablaOpcodes& t, u32 mascota, u8 fila) {
    Escritor e(t, Cli::USEPETITEM);
    e.c(0);
    e.d(mascota);
    e.c(fila);
    return e;
}
// C_SelectTarget: d mascota, c 0, d objetivo (respuesta a S_SelectTarget).
inline Escritor objetivo_mascota(const TablaOpcodes& t, u32 mascota, u32 objetivo) {
    Escritor e(t, Cli::SELECTTARGET);
    e.d(mascota);
    e.c(0);
    e.d(objetivo);
    return e;
}
// C_ChangeChar: sin datos. Sale del personaje y vuelve a la lista de personajes.
inline Escritor cambiar_personaje(const TablaOpcodes& t) { return Escritor(t, Cli::CHANGECHAR); }
// C_FixWeaponList: sin datos (vínculo "fix" del herrero; responde S_FixWeaponList).
inline Escritor armas_a_reparar(const TablaOpcodes& t) { return Escritor(t, Cli::FIXWEAPONLIST); }
// C_SelectList: d arma, d herrero (a 3 celdas o menos; cuesta 200 adena por punto de daño).
inline Escritor reparar_arma(const TablaOpcodes& t, u32 arma, u32 npc) {
    Escritor e(t, Cli::SELECTLIST);
    e.d(arma);
    e.d(npc);
    return e;
}
// C_AddBookmark / C_DeleteBookmark: s nombre.
inline Escritor marcar_lugar(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::BOOKMARK);
    e.s(nombre);
    return e;
}
inline Escritor borrar_marcador(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::BOOKMARKDELETE);
    e.s(nombre);
    return e;
}
// C_UseItem de un pergamino de teleport (usos 6 y 29): d ítem, h mapa, d marcador (0 = al azar).
inline Escritor usar_teleport(const TablaOpcodes& t, u32 item, u16 mapa, u32 marcador) {
    Escritor e(t, Cli::USEITEM);
    e.d(item).h(mapa).d(marcador);
    return e;
}
// C_UseSkill de Teleport / Mass Teleport: c fila, c columna, h mapa, d marcador.
inline Escritor hechizo_teleport(const TablaOpcodes& t, u32 id, u16 mapa, u32 marcador) {
    Escritor e(t, Cli::USESKILL);
    e.c(static_cast<u8>((id - 1) / 8)).c(static_cast<u8>((id - 1) % 8));
    e.h(mapa).d(marcador);
    return e;
}
// C_Title: s personaje, s título (el propio desde nivel 40; el príncipe a los de su clan).
inline Escritor poner_titulo(const TablaOpcodes& t, std::string_view personaje, std::string_view titulo) {
    Escritor e(t, Cli::TITLE);
    e.s(personaje).s(titulo);
    return e;
}
// C_Who: s nombre (vacío: cuántos hay conectados).
inline Escritor quien(const TablaOpcodes& t, std::string_view nombre) {
    Escritor e(t, Cli::WHO);
    e.s(nombre);
    return e;
}
// C_Teleport: sin cuerpo. Respuesta a S_Teleport: recién ahí el servidor mueve al personaje.
inline Escritor teletransporte(const TablaOpcodes& t) { return Escritor(t, Cli::TELEPORT); }
// C_ChatWhisper: s destinatario, s texto
inline Escritor susurrar(const TablaOpcodes& t, std::string_view destino, std::string_view texto) {
    Escritor e(t, Cli::CHATWHISPER);
    e.s(destino).s(texto);
    return e;
}
}  // namespace cmd

}  // namespace lin
