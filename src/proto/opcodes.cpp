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
    {252, Srv::PINKNAME},
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
    {Cli::ENTERPORTAL, 249},
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
    "BOOKMARKS", "SELECTLIST", "IDENTIFYDESC", "INVIS", "RESURRECTION", "STRUP", "DEXUP", "PINKNAME",
};
static_assert(sizeof(NOMBRES) / sizeof(NOMBRES[0]) == static_cast<size_t>(Srv::CANTIDAD));

}  // namespace

bool tabla_opcodes(std::string_view version, TablaOpcodes& t) {
    if (version != "jp363") return false;
    for (Srv& s : t.srv_de_cable) s = Srv::DESCONOCIDO;
    for (const ParSrv& p : JP363_SRV) t.srv_de_cable[p.cable] = p.interno;
    for (const ParCli& p : JP363_CLI) t.cable_de_cli[static_cast<int>(p.interno)] = p.cable;
    t.cable_init = 161;
    return true;
}

const char* nombre_srv(Srv s) { return NOMBRES[static_cast<int>(s) < static_cast<int>(Srv::CANTIDAD) ? static_cast<int>(s) : 0]; }

}  // namespace lin
