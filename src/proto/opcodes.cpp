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
// BLUEMESSAGE, DELSKILL, DOOR, PETCTRL y, del cliente, CLAN, PETMENU, USEPETITEM, SELECTTARGET,
// SELECTLIST, RETURNTOLOGIN, BOARDNEXT, CHANGEWARTIME.
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
void cargar(TablaOpcodes& t, const ParSrv (&srv)[NS], const ParCli (&cli)[NC], u8 init, bool v380) {
    for (Srv& s : t.srv_de_cable) s = Srv::DESCONOCIDO;
    for (u8& c : t.cable_de_cli) c = 0;
    for (const ParSrv& p : srv) t.srv_de_cable[p.cable] = p.interno;
    for (const ParCli& p : cli) t.cable_de_cli[static_cast<int>(p.interno)] = p.cable;
    t.cable_init = init;
    t.v380 = v380;
}

bool tabla_opcodes(std::string_view version, TablaOpcodes& t) {
    if (version == "jp363") cargar(t, JP363_SRV, JP363_CLI, 161, false);
    else if (version == "tw380") cargar(t, TW380_SRV, TW380_CLI, 150, true);
    else return false;
    return true;
}

const char* nombre_srv(Srv s) { return NOMBRES[static_cast<int>(s) < static_cast<int>(Srv::CANTIDAD) ? static_cast<int>(s) : 0]; }

}  // namespace lin
