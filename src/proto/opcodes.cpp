#include "proto/opcodes.h"

namespace lin {

namespace {

struct ParSrv {
    u8 cable;
    Srv interno;
};
struct ParCli {
    Cli interno;
    u8 cable;
};

// L1J-JP 3.63c: jp/l1j/server/codes/Opcodes.java (commit 403e35a).
constexpr ParSrv JP363_SRV[] = {
    {161, Srv::INITPACKET},   {151, Srv::SERVERVERSION}, {51, Srv::LOGINRESULT},   {30, Srv::COMMONNEWS},
    {126, Srv::CHARAMOUNT},   {184, Srv::CHARLIST},      {212, Srv::NEWCHARPACK},  {153, Srv::NEWCHARWRONG},
    {5, Srv::DETELECHAROK},   {131, Srv::LOGINTOGAME},   {95, Srv::DISCONNECT},    {145, Srv::OWNCHARSTATUS},
    {150, Srv::MAPID},        {3, Srv::CHARPACK},        {122, Srv::MOVEOBJECT},   {185, Srv::REMOVE_OBJECT},
    {199, Srv::CHANGEHEADING}, {53, Srv::LIGHT},         {194, Srv::GAMETIME},     {193, Srv::WEATHER},
    {14, Srv::SERVERMSG},     {42, Srv::HPUPDATE},       {73, Srv::MPUPDATE},      {76, Srv::NORMALCHAT},
    {10, Srv::GLOBALCHAT},    {113, Srv::CHARVISUALUPDATE}, {4, Srv::TELEPORT},    {218, Srv::DOACTIONGFX},
    {174, Srv::SPMR},         {15, Srv::OWNCHARATTRDEF}, {202, Srv::CHARTITLE},    {121, Srv::EXP},
    {140, Srv::LAWFUL},       {255, Srv::WHISPERCHAT},   {133, Srv::NPCSHOUT},     {216, Srv::OWNCHARSTATUS2},
    {40, Srv::PACKETBOX},     {59, Srv::BLUEMESSAGE},    {142, Srv::ATTACKPACKET}, {128, Srv::HPMETER},
    {180, Srv::INVLIST},      {63, Srv::ADDITEM},        {148, Srv::DELETEINVENTORYITEM}, {195, Srv::ITEMNAME},
    {127, Srv::ITEMAMOUNT},   {144, Srv::ITEMCOLOR},  // ITEMSTATUS usa el mismo número que ITEMAMOUNT
    {48, Srv::ADDSKILL},      {18, Srv::DELSKILL},       {232, Srv::SKILLSOUNDGFX}, {112, Srv::EFFECTLOCATION},
    {149, Srv::SKILLHASTE},   {200, Srv::SKILLBRAVE},    {69, Srv::SKILLICONSHIELD}, {16, Srv::RANGESKILLS},
    {164, Srv::POLY},         {119, Srv::SHOWHTML},      {254, Srv::SHOWSHOPBUYLIST}, {170, Srv::SHOWSHOPSELLLIST},
    {250, Srv::SHOWRETRIEVELIST}, {155, Srv::YES_NO},     {77, Srv::TRADE},          {86, Srv::TRADEADDITEM},
    {239, Srv::TRADESTATUS},  {50, Srv::EMBLEM},        {222, Srv::SKILLBUY},
    {35, Srv::DOOR},          {165, Srv::PARALYSIS},     {93, Srv::POISON},        {238, Srv::CURSEBLIND},
    {253, Srv::INPUTAMOUNT},  {33, Srv::PETCTRL},        {177, Srv::SELECTTARGET}, {110, Srv::TRUETARGET},
    // PETCTRL (33) es también S_CharReset: se distinguen por el primer byte (0x0c, la mascota).
    {11, Srv::BOOKMARKS},     {208, Srv::SELECTLIST},     {43, Srv::IDENTIFYDESC},
    {57, Srv::INVIS},         {227, Srv::RESURRECTION},  {120, Srv::STRUP},        {28, Srv::DEXUP},
    {252, Srv::PINKNAME},     {31, Srv::LIQUOR},         {12, Srv::BLESSOFEVA},    {116, Srv::ABILITY},
    {84, Srv::SOUND},         {190, Srv::PRIVATESHOPLIST},
    {64, Srv::BOARD},         {56, Srv::BOARDREAD},      {1, Srv::MAIL},
    {24, Srv::HOUSELIST},     {44, Srv::HOUSEMAP},       {123, Srv::WAR},          {66, Srv::CASTLEMASTER},
    {72, Srv::TAXRATE},       {203, Srv::DEPOSIT},       {224, Srv::DRAWAL},
    // WARTIME usa el número de MAPID (150): se distinguen por el largo (ver mapa en decodificar.cpp).
    // SKILLICONGFX y SKILLICONAURA usan el número de PACKETBOX (40): son subtipos de esa caja.
};
constexpr ParCli JP363_CLI[] = {
    {Cli::CLIENTVERSION, 127}, {Cli::LOGINPACKET, 57},   {Cli::COMMONCLICK, 53},   {Cli::NEWCHAR, 253},
    {Cli::DELETECHAR, 10},     {Cli::LOGINTOSERVER, 131}, {Cli::MOVECHAR, 95},      {Cli::CHANGEHEADING, 65},
    {Cli::KEEPALIVE, 182},     {Cli::QUITGAME, 104},     {Cli::CHAT, 190},         {Cli::CHATWHISPER, 122},
    {Cli::ATTACK, 68},         {Cli::RESTART, 71},       {Cli::USEITEM, 44},       {Cli::PICKUPITEM, 188},
    {Cli::DROPITEM, 54},       {Cli::USESKILL, 115},    {Cli::NPCACTION, 37},
    {Cli::NPCTALK, 58},        {Cli::RESULT, 40},        {Cli::TELEPORT, 242},    {Cli::ATTR, 61},
    {Cli::CREATEPARTY, 166},   {Cli::PARTYLIST, 42},     {Cli::LEAVEPARTY, 204},   {Cli::BANPARTY, 70},
    {Cli::TRADE, 103},         {Cli::TRADEADDITEM, 241}, {Cli::TRADEADDOK, 110},   {Cli::TRADEADDCANCEL, 167},
    {Cli::CREATECLAN, 154},    {Cli::JOINCLAN, 30},      {Cli::LEAVECLAN, 121},    {Cli::BANCLAN, 222},
    {Cli::TITLE, 96},          {Cli::PLEDGE, 225},       {Cli::WHO, 49},           {Cli::CLAN, 246},
    {Cli::EMBLEM, 107},        {Cli::SKILLBUY, 173},     {Cli::SKILLBUYOK, 207},
    {Cli::DOOR, 199},          {Cli::GIVEITEM, 244},     {Cli::AMOUNT, 109},       {Cli::PETMENU, 217},
    {Cli::USEPETITEM, 213},    {Cli::SELECTTARGET, 155},  {Cli::CHANGECHAR, 237},   {Cli::FIXWEAPONLIST, 106},
    {Cli::SELECTLIST, 238},    {Cli::BOOKMARK, 134},     {Cli::BOOKMARKDELETE, 223},
    {Cli::DELETEINVENTORYITEM, 209}, {Cli::EXITGHOST, 210}, {Cli::RETURNTOLOGIN, 218},
    {Cli::ENTERPORTAL, 249},  {Cli::CHARRESET, 236},    {Cli::EXCLUDE, 101},      {Cli::BUDDYLIST, 60},
    {Cli::ADDBUDDY, 99},      {Cli::DELETEBUDDY, 211},  {Cli::CHATPARTY, 113},
    {Cli::SHOP, 16},          {Cli::SHOPLIST, 193},
    {Cli::BOARD, 73},         {Cli::BOARDREAD, 59},     {Cli::BOARDNEXT, 221},    {Cli::BOARDWRITE, 14},
    {Cli::BOARDDELETE, 12},   {Cli::MAIL, 22},
    {Cli::WAR, 235},          {Cli::TAXRATE, 200},      {Cli::DEPOSIT, 35},       {Cli::DRAWAL, 192},
    {Cli::CHANGEWARTIME, 4},
};

// L1J 3.80C de Taiwán: internal/net/packet/opcodes.go de L1JGO-Whale (commit 9a19405), por nombre.
// Sin equivalente en esa tabla (quedan en 0 y no se mandan o no se reconocen): COMMONNEWS, TELEPORT,
// BLUEMESSAGE, DELSKILL, PETCTRL y, del cliente, CLAN, PETMENU, USEPETITEM, SELECTTARGET,
// SELECTLIST, RETURNTOLOGIN, BOARDNEXT, CHANGEWARTIME.
// El servidor manda y el cliente aún no decodifica: S_CharSynAck (64), S_RedMessage (105),
// S_ChangeName (46), S_ClanName (72), S_ClanAttention (200), S_ClanMatching (0). Ver
// docs/analisis/OPCODES_PENDIENTES.md.
constexpr ParSrv TW380_SRV[] = {
    {139, Srv::SERVERVERSION}, {233, Srv::LOGINRESULT},   {178, Srv::CHARAMOUNT},   {93, Srv::CHARLIST},
    {127, Srv::NEWCHARPACK},   {98, Srv::NEWCHARWRONG},   {6, Srv::DETELECHAROK},   {223, Srv::LOGINTOGAME},
    {227, Srv::DISCONNECT},    {8, Srv::OWNCHARSTATUS},   {206, Srv::MAPID},        {87, Srv::CHARPACK},
    {10, Srv::MOVEOBJECT},     {120, Srv::REMOVE_OBJECT}, {122, Srv::CHANGEHEADING}, {40, Srv::LIGHT},
    {123, Srv::GAMETIME},      {115, Srv::WEATHER},       {71, Srv::SERVERMSG},     {225, Srv::HPUPDATE},
    {33, Srv::MPUPDATE},       {81, Srv::NORMALCHAT},     {243, Srv::GLOBALCHAT},   {119, Srv::CHARVISUALUPDATE},
    {158, Srv::DOACTIONGFX},   {37, Srv::SPMR},           {174, Srv::OWNCHARATTRDEF}, {183, Srv::CHARTITLE},
    {113, Srv::EXP},           {34, Srv::LAWFUL},         {73, Srv::WHISPERCHAT},   {161, Srv::NPCSHOUT},
    {209, Srv::DOOR},          {250, Srv::PACKETBOX},    {30, Srv::ATTACKPACKET},  {237, Srv::HPMETER},
    {5, Srv::INVLIST},         {15, Srv::ADDITEM},        {57, Srv::DELETEINVENTORYITEM}, {100, Srv::ITEMNAME},
    {24, Srv::ITEMAMOUNT},     {240, Srv::ITEMCOLOR},     {164, Srv::ADDSKILL},     {55, Srv::SKILLSOUNDGFX},
    {106, Srv::EFFECTLOCATION}, {255, Srv::SKILLHASTE},   {67, Srv::SKILLBRAVE},    {216, Srv::SKILLICONSHIELD},
    {42, Srv::RANGESKILLS},    {76, Srv::POLY},           {39, Srv::SHOWHTML},      {70, Srv::SHOWSHOPBUYLIST},
    {65, Srv::SHOWSHOPSELLLIST}, {176, Srv::SHOWRETRIEVELIST}, {219, Srv::YES_NO},  {52, Srv::TRADE},
    {35, Srv::TRADEADDITEM},   {112, Srv::TRADESTATUS},   {118, Srv::EMBLEM},       {23, Srv::SKILLBUY},
    {202, Srv::PARALYSIS},     {165, Srv::POISON},        {47, Srv::CURSEBLIND},    {136, Srv::INPUTAMOUNT},
    {236, Srv::SELECTTARGET},  {11, Srv::TRUETARGET},     {92, Srv::BOOKMARKS},     {83, Srv::SELECTLIST},
    {245, Srv::IDENTIFYDESC},  {171, Srv::INVIS},         {85, Srv::RESURRECTION},  {166, Srv::STRUP},
    {188, Srv::DEXUP},         {60, Srv::PINKNAME},       {103, Srv::LIQUOR},       {22, Srv::SOUND},
    {140, Srv::PRIVATESHOPLIST}, {68, Srv::BOARD},        {148, Srv::BOARDREAD},    {186, Srv::MAIL},
    {156, Srv::HOUSELIST},     {187, Srv::HOUSEMAP},      {84, Srv::WAR},           {69, Srv::CASTLEMASTER},
    {185, Srv::TAXRATE},       {4, Srv::DEPOSIT},         {141, Srv::DRAWAL},
    {231, Srv::MAPID},  // S_WarTime: el decodificador de S_MapID lo distingue por el largo
};
constexpr ParCli TW380_CLI[] = {
    {Cli::CLIENTVERSION, 14},  {Cli::LOGINPACKET, 119},  {Cli::COMMONCLICK, 16},   {Cli::NEWCHAR, 84},
    {Cli::DELETECHAR, 162},    {Cli::LOGINTOSERVER, 137}, {Cli::MOVECHAR, 29},     {Cli::CHANGEHEADING, 225},
    {Cli::KEEPALIVE, 95},      {Cli::QUITGAME, 122},     {Cli::CHAT, 40},          {Cli::CHATWHISPER, 184},
    {Cli::ATTACK, 229},        {Cli::RESTART, 177},      {Cli::USEITEM, 164},      {Cli::PICKUPITEM, 112},
    {Cli::DROPITEM, 25},       {Cli::USESKILL, 6},       {Cli::NPCACTION, 125},    {Cli::NPCTALK, 34},
    {Cli::RESULT, 161},        {Cli::TELEPORT, 52},      {Cli::ATTR, 121},
    {Cli::CREATEPARTY, 43},    {Cli::PARTYLIST, 230},    {Cli::LEAVEPARTY, 33},    {Cli::BANPARTY, 255},
    {Cli::TRADE, 2},           {Cli::TRADEADDITEM, 37},  {Cli::TRADEADDOK, 71},    {Cli::TRADEADDCANCEL, 86},
    {Cli::CREATECLAN, 222},    {Cli::JOINCLAN, 194},     {Cli::LEAVECLAN, 61},     {Cli::BANCLAN, 69},
    {Cli::TITLE, 90},          {Cli::PLEDGE, 68},        {Cli::WHO, 206},          {Cli::EMBLEM, 18},
    {Cli::SKILLBUY, 145},      {Cli::SKILLBUYOK, 39},    {Cli::DOOR, 41},          {Cli::GIVEITEM, 45},
    {Cli::AMOUNT, 11},         {Cli::CHANGECHAR, 7},     {Cli::FIXWEAPONLIST, 118}, {Cli::BOOKMARK, 165},
    {Cli::BOOKMARKDELETE, 3},  {Cli::DELETEINVENTORYITEM, 138}, {Cli::EXITGHOST, 173}, {Cli::ENTERPORTAL, 219},
    {Cli::CHARRESET, 98},      {Cli::EXCLUDE, 171},      {Cli::BUDDYLIST, 4},      {Cli::ADDBUDDY, 207},
    {Cli::DELETEBUDDY, 202},   {Cli::CHATPARTY, 199},    {Cli::SHOP, 38},          {Cli::SHOPLIST, 47},
    {Cli::BOARD, 23},          {Cli::BOARDREAD, 114},    {Cli::BOARDWRITE, 141},   {Cli::BOARDDELETE, 153},
    {Cli::MAIL, 87},           {Cli::WAR, 227},          {Cli::TAXRATE, 19},       {Cli::DEPOSIT, 56},
    {Cli::DRAWAL, 44},
};

// L1J 3.0 English / US 2009 (S3EP1): encryptions/Opcodes.java de l1j-en/classic, por nombre. El
// cifrado es el mismo XOR que jp363. Saludo inicial: opcode 125 (0x7D) + semilla (fija 0x7C98BDFA,
// el cliente la lee igual). Vida/maná en 2 bytes como 3.63.
// Sin equivalente en classic (quedan en 0): PETCTRL; y del cliente: ninguno. En classic el menú de
// control de la mascota (S_PetMenuPacket) es un SHOWHTML (página anicom/moncom), no el binario
// S_PetCtrlMenu del 3.63/3.80; la mascota en sí llega como CHARPACK (S_PetPack) y el "elegir objetivo"
// como SELECTTARGET. El panel nativo de mascotas (que se llena con PETCTRL) queda vacío, como el de grupo.
constexpr ParSrv S3EP1_SRV[] = {
    {89, Srv::SERVERVERSION}, {63, Srv::LOGINRESULT}, {66, Srv::COMMONNEWS}, {80, Srv::CHARAMOUNT},
    {102, Srv::CHARLIST}, {56, Srv::NEWCHARPACK}, {29, Srv::NEWCHARWRONG}, {84, Srv::DETELECHAROK},
    {116, Srv::LOGINTOGAME}, {41, Srv::DISCONNECT}, {59, Srv::OWNCHARSTATUS}, {73, Srv::MAPID},
    {1, Srv::CHARPACK}, {126, Srv::MOVEOBJECT}, {0, Srv::REMOVE_OBJECT}, {15, Srv::CHANGEHEADING},
    {124, Srv::LIGHT}, {58, Srv::GAMETIME}, {31, Srv::WEATHER}, {98, Srv::SERVERMSG},
    {34, Srv::HPUPDATE}, {92, Srv::MPUPDATE}, {71, Srv::NORMALCHAT}, {3, Srv::GLOBALCHAT},
    {13, Srv::CHARVISUALUPDATE}, {11, Srv::TELEPORT}, {6, Srv::DOACTIONGFX}, {52, Srv::SPMR},
    {96, Srv::OWNCHARATTRDEF}, {32, Srv::CHARTITLE}, {95, Srv::EXP}, {119, Srv::LAWFUL},
    {47, Srv::WHISPERCHAT}, {50, Srv::NPCSHOUT}, {19, Srv::OWNCHARSTATUS2}, {62, Srv::PACKETBOX},
    {65, Srv::BLUEMESSAGE}, {35, Srv::ATTACKPACKET}, {104, Srv::HPMETER}, {8, Srv::INVLIST},
    {33, Srv::ADDITEM}, {75, Srv::DELETEINVENTORYITEM}, {9, Srv::ITEMNAME}, {25, Srv::ITEMAMOUNT},
    {107, Srv::ITEMCOLOR}, {28, Srv::ADDSKILL}, {57, Srv::DELSKILL}, {101, Srv::SKILLSOUNDGFX},
    {69, Srv::EFFECTLOCATION}, {49, Srv::SKILLHASTE}, {20, Srv::SKILLBRAVE},
    {114, Srv::SKILLICONSHIELD}, {112, Srv::RANGESKILLS}, {2, Srv::POLY}, {37, Srv::SHOWHTML},
    {46, Srv::SHOWSHOPBUYLIST}, {12, Srv::SHOWSHOPSELLLIST}, {24, Srv::SHOWRETRIEVELIST},
    {105, Srv::YES_NO}, {53, Srv::TRADE}, {21, Srv::TRADEADDITEM}, {122, Srv::TRADESTATUS},
    {7, Srv::EMBLEM}, {48, Srv::SKILLBUY}, {82, Srv::DOOR}, {81, Srv::PARALYSIS}, {10, Srv::POISON},
    {110, Srv::CURSEBLIND}, {123, Srv::INPUTAMOUNT}, {61, Srv::SELECTTARGET}, {70, Srv::TRUETARGET},
    {103, Srv::BOOKMARKS}, {18, Srv::SELECTLIST}, {99, Srv::IDENTIFYDESC}, {22, Srv::INVIS},
    {44, Srv::RESURRECTION}, {106, Srv::STRUP}, {51, Srv::DEXUP}, {87, Srv::PINKNAME},
    {43, Srv::LIQUOR}, {17, Srv::BLESSOFEVA}, {86, Srv::ABILITY}, {83, Srv::SOUND},
    {115, Srv::PRIVATESHOPLIST}, {93, Srv::BOARD}, {91, Srv::BOARDREAD}, {127, Srv::MAIL},
    {72, Srv::HOUSELIST}, {113, Srv::HOUSEMAP}, {94, Srv::WAR}, {111, Srv::CASTLEMASTER},
    {120, Srv::TAXRATE}, {60, Srv::DEPOSIT}, {67, Srv::DRAWAL}, {26, Srv::MAPID},
};
constexpr ParCli S3EP1_CLI[] = {
    {Cli::CLIENTVERSION, 34}, {Cli::LOGINPACKET, 113}, {Cli::COMMONCLICK, 46}, {Cli::NEWCHAR, 50},
    {Cli::DELETECHAR, 54}, {Cli::LOGINTOSERVER, 89}, {Cli::MOVECHAR, 15}, {Cli::CHANGEHEADING, 6},
    {Cli::KEEPALIVE, 4}, {Cli::QUITGAME, 114}, {Cli::CHAT, 18}, {Cli::CHATWHISPER, 92},
    {Cli::ATTACK, 5}, {Cli::RESTART, 82}, {Cli::USEITEM, 119}, {Cli::PICKUPITEM, 7},
    {Cli::DROPITEM, 52}, {Cli::USESKILL, 129}, {Cli::NPCACTION, 65}, {Cli::NPCTALK, 118},
    {Cli::RESULT, 104}, {Cli::TELEPORT, 79}, {Cli::ATTR, 63}, {Cli::CREATEPARTY, 130},
    {Cli::PARTYLIST, 77}, {Cli::LEAVEPARTY, 11}, {Cli::BANPARTY, 72}, {Cli::TRADE, 30},
    {Cli::TRADEADDITEM, 20}, {Cli::TRADEADDOK, 61}, {Cli::TRADEADDCANCEL, 41}, {Cli::CREATECLAN, 62},
    {Cli::JOINCLAN, 94}, {Cli::LEAVECLAN, 27}, {Cli::BANCLAN, 103}, {Cli::TITLE, 66},
    {Cli::PLEDGE, 76}, {Cli::WHO, 57}, {Cli::CLAN, 73}, {Cli::EMBLEM, 120}, {Cli::SKILLBUY, 93},
    {Cli::SKILLBUYOK, 31}, {Cli::DOOR, 75}, {Cli::GIVEITEM, 85}, {Cli::AMOUNT, 124},
    {Cli::PETMENU, 83}, {Cli::USEPETITEM, 111}, {Cli::SELECTTARGET, 47}, {Cli::CHANGECHAR, 16},
    {Cli::FIXWEAPONLIST, 51}, {Cli::SELECTLIST, 112}, {Cli::BOOKMARK, 0}, {Cli::BOOKMARKDELETE, 128},
    {Cli::DELETEINVENTORYITEM, 69}, {Cli::EXITGHOST, 121}, {Cli::RETURNTOLOGIN, 95},
    {Cli::ENTERPORTAL, 26}, {Cli::CHARRESET, 80}, {Cli::EXCLUDE, 116}, {Cli::BUDDYLIST, 109},
    {Cli::ADDBUDDY, 56}, {Cli::DELETEBUDDY, 10}, {Cli::CHATPARTY, 131}, {Cli::SHOP, 8},
    {Cli::SHOPLIST, 17}, {Cli::BOARD, 24}, {Cli::BOARDREAD, 19}, {Cli::BOARDNEXT, 87},
    {Cli::BOARDWRITE, 84}, {Cli::BOARDDELETE, 60}, {Cli::MAIL, 127}, {Cli::WAR, 101},
    {Cli::TAXRATE, 110}, {Cli::DEPOSIT, 68}, {Cli::DRAWAL, 42}, {Cli::CHANGEWARTIME, 98},
};

const char* NOMBRES[] = {
    "?", "INITPACKET", "SERVERVERSION", "LOGINRESULT", "COMMONNEWS", "CHARAMOUNT", "CHARLIST", "NEWCHARPACK",
    "NEWCHARWRONG", "DETELECHAROK", "LOGINTOGAME", "DISCONNECT", "OWNCHARSTATUS", "MAPID", "CHARPACK",
    "MOVEOBJECT", "REMOVE_OBJECT", "CHANGEHEADING", "LIGHT", "GAMETIME", "WEATHER", "SERVERMSG", "HPUPDATE",
    "MPUPDATE", "NORMALCHAT", "GLOBALCHAT", "CHARVISUALUPDATE", "TELEPORT", "DOACTIONGFX", "SPMR",
    "OWNCHARATTRDEF", "CHARTITLE", "EXP", "LAWFUL", "WHISPERCHAT", "NPCSHOUT", "OWNCHARSTATUS2", "PACKETBOX",
    "BLUEMESSAGE", "ATTACKPACKET", "HPMETER", "INVLIST", "ADDITEM", "DELETEINVENTORYITEM", "ITEMNAME",
    "ITEMAMOUNT", "ITEMCOLOR", "ADDSKILL", "DELSKILL", "SKILLSOUNDGFX", "EFFECTLOCATION", "SKILLHASTE",
    "SKILLBRAVE", "SKILLICONSHIELD", "RANGESKILLS", "POLY", "SHOWHTML", "SHOWSHOPBUYLIST",
    "SHOWSHOPSELLLIST", "SHOWRETRIEVELIST", "YES_NO", "TRADE", "TRADEADDITEM", "TRADESTATUS", "EMBLEM", "SKILLBUY",
    "DOOR", "PARALYSIS", "POISON", "CURSEBLIND", "INPUTAMOUNT", "PETCTRL", "SELECTTARGET", "TRUETARGET",
    "BOOKMARKS", "SELECTLIST", "IDENTIFYDESC", "INVIS", "RESURRECTION", "STRUP", "DEXUP", "PINKNAME", "LIQUOR", "BLESSOFEVA", "ABILITY", "SOUND", "PRIVATESHOPLIST", "BOARD", "BOARDREAD", "MAIL",
    "HOUSELIST", "HOUSEMAP", "WAR", "CASTLEMASTER", "TAXRATE", "DEPOSIT", "DRAWAL",
};
static_assert(sizeof(NOMBRES) / sizeof(NOMBRES[0]) == static_cast<size_t>(Srv::CANTIDAD));

}  // namespace

template <size_t NS, size_t NC>
void cargar(TablaOpcodes& t, const ParSrv (&srv)[NS], const ParCli (&cli)[NC], u8 init, PerfilProtocolo perfil) {
    for (Srv& s : t.srv_de_cable) s = Srv::DESCONOCIDO;
    for (u8& c : t.cable_de_cli) c = 0;
    for (const ParSrv& p : srv) t.srv_de_cable[p.cable] = p.interno;
    for (const ParCli& p : cli) t.cable_de_cli[static_cast<int>(p.interno)] = p.cable;
    t.cable_init = init;
    t.perfil = perfil;
}

bool tabla_opcodes(std::string_view version, TablaOpcodes& t) {
    if (version == "jp363") cargar(t, JP363_SRV, JP363_CLI, 161, PerfilProtocolo{});
    else if (version == "tw380") cargar(t, TW380_SRV, TW380_CLI, 150, PerfilProtocolo{.bytes_vida = 4});
    else if (version == "s3ep1") cargar(t, S3EP1_SRV, S3EP1_CLI, 125, PerfilProtocolo{.bytes_catalizador_item = 0, .bytes_cola_item = 0, .bytes_pre_pregunta = 0, .bytes_numero_pregunta = 0});
    else return false;
    return true;
}

const char* nombre_srv(Srv s) { return NOMBRES[static_cast<int>(s) < static_cast<int>(Srv::CANTIDAD) ? static_cast<int>(s) : 0]; }

}  // namespace lin
