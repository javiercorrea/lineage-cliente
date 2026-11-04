// Opcodes: el código trabaja con ids internos; el número de cable depende de la versión de
// protocolo y se traduce con una tabla (PLAN_CLIENTE.md §4.2).
#pragma once
#include <string_view>
#include "core/tipos.h"

namespace lin {

// Servidor -> cliente (nombres de L1J).
enum class Srv : u8 {
    DESCONOCIDO = 0,
    INITPACKET, SERVERVERSION, LOGINRESULT, COMMONNEWS, CHARAMOUNT, CHARLIST, NEWCHARPACK,
    NEWCHARWRONG, DETELECHAROK, LOGINTOGAME, DISCONNECT, OWNCHARSTATUS, MAPID, CHARPACK,
    MOVEOBJECT, REMOVE_OBJECT, CHANGEHEADING, LIGHT, GAMETIME, WEATHER, SERVERMSG, HPUPDATE,
    MPUPDATE, NORMALCHAT, GLOBALCHAT, CHARVISUALUPDATE, TELEPORT, DOACTIONGFX, SPMR,
    OWNCHARATTRDEF, CHARTITLE, EXP, LAWFUL, WHISPERCHAT, NPCSHOUT, OWNCHARSTATUS2, PACKETBOX,
    BLUEMESSAGE, ATTACKPACKET, HPMETER, INVLIST, ADDITEM, DELETEINVENTORYITEM, ITEMNAME, ITEMAMOUNT,
    ITEMCOLOR, ADDSKILL, DELSKILL, SKILLSOUNDGFX, EFFECTLOCATION, SKILLHASTE, SKILLBRAVE, SKILLICONSHIELD,
    RANGESKILLS, POLY, SHOWHTML, SHOWSHOPBUYLIST, SHOWSHOPSELLLIST, SHOWRETRIEVELIST,
    YES_NO, TRADE, TRADEADDITEM, TRADESTATUS, EMBLEM, SKILLBUY,
    DOOR, PARALYSIS, POISON, CURSEBLIND, INPUTAMOUNT, PETCTRL, SELECTTARGET, TRUETARGET,
    BOOKMARKS, SELECTLIST, IDENTIFYDESC, INVIS, RESURRECTION, STRUP, DEXUP, PINKNAME, LIQUOR, BLESSOFEVA, ABILITY, SOUND, PRIVATESHOPLIST, BOARD, BOARDREAD, MAIL,
    HOUSELIST, HOUSEMAP, WAR, CASTLEMASTER, TAXRATE, DEPOSIT, DRAWAL,
    CANTIDAD
};

// Cliente -> servidor.
enum class Cli : u8 {
    CLIENTVERSION, LOGINPACKET, COMMONCLICK, NEWCHAR, DELETECHAR, LOGINTOSERVER, MOVECHAR,
    CHANGEHEADING, KEEPALIVE, QUITGAME, CHAT, CHATWHISPER, ATTACK, RESTART, USEITEM, PICKUPITEM,
    DROPITEM, USESKILL, NPCACTION, NPCTALK, RESULT, TELEPORT, ATTR,
    CREATEPARTY, PARTYLIST, LEAVEPARTY, BANPARTY, TRADE, TRADEADDITEM, TRADEADDOK, TRADEADDCANCEL,
    CREATECLAN, JOINCLAN, LEAVECLAN, BANCLAN, TITLE, PLEDGE, WHO, CLAN, EMBLEM, SKILLBUY, SKILLBUYOK,
    DOOR, GIVEITEM, AMOUNT, PETMENU, USEPETITEM, SELECTTARGET,
    CHANGECHAR, FIXWEAPONLIST, SELECTLIST, BOOKMARK, BOOKMARKDELETE, DELETEINVENTORYITEM,
    EXITGHOST, RETURNTOLOGIN, ENTERPORTAL, CHARRESET, EXCLUDE, BUDDYLIST, ADDBUDDY, DELETEBUDDY, CHATPARTY, SHOP, SHOPLIST,
    BOARD, BOARDREAD, BOARDNEXT, BOARDWRITE, BOARDDELETE, MAIL,
    WAR, TAXRATE, DEPOSIT, DRAWAL, CHANGEWARTIME,
    CANTIDAD
};

// Parámetros del formato de los cuerpos que varían entre versiones (los números de opcode van en la
// tabla). Toda diferencia de versión se expresa como un dato de acá, no como un `if` en los
// decodificadores (ver docs/analisis/PLAN_MULTIVERSION.md). Cada campo arranca con el valor del
// 3.63c, que es el caso base.
struct PerfilProtocolo {
    u8 bytes_vida = 2;  // ancho de vida y maná (S_OwnCharStatus, S_HPUpdate, S_MPUpdate); 4 en 3.80C
    // Cuerpo de ítem (S_AddItem, S_InvList): el 3.63/3.80 ponen antes del tipo un campo h de
    // catalizador mágico (2 bytes) y cierran con una cola fija (c 10, d 0, d 0, h 0 = 11 bytes); el
    // US 2009 no tiene ni uno ni otra.
    u8 bytes_catalizador_item = 2;
    u8 bytes_cola_item = 11;
};

struct TablaOpcodes {
    Srv srv_de_cable[256];                          // número de cable -> interno
    u8 cable_de_cli[static_cast<int>(Cli::CANTIDAD)];  // interno -> número de cable
    u8 cable_init;                                  // opcode del paquete inicial (sin cifrar)
    PerfilProtocolo perfil;                         // formato de los cuerpos para esta versión

    Srv srv(u8 cable) const { return srv_de_cable[cable]; }
    u8 cli(Cli c) const { return cable_de_cli[static_cast<int>(c)]; }
};

// Versiones disponibles: "jp363" (L1J-JP 3.63c), "tw380" (3.80C de Taiwán, L1JGO-Whale) y "s3ep1"
// (3.0 English / US 2009, l1j-en/classic).
// PENDIENTE: cargar las demás desde los opcodes.txt de docs/analisis/opcodes (s3ep1, kr380...).
bool tabla_opcodes(std::string_view version, TablaOpcodes& salida);

const char* nombre_srv(Srv s);

}  // namespace lin
