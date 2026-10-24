package com.lineage.server.serverpackets;

public class OpcodesServer {

	// 1810102501 登入階段協定
	public static final int S_KICK = 87;
	public static final int S_SAY_CODE = 18;

	public static final int S_CHARACTER_INFO = 33;

	public static final int S_LOGIN_CHECK = 45;

	public static final int S_PUT_OBJECT = 55;
	public static final int S_SAY = 54;
	public static final int S_PLEDGE = 185;
	public static final int S_MESSAGE = 238;
	public static final int S_EVENT = 246;

	public static final int S_EXTENDED_PROTOBUF = 61;
	public static final int S_REQUEST_SUMMON = 70;
	public static final int S_BLINK = 112;

	public static final int S_KEY = 74;

	public static final int S_VOICE_CHAT = 141;
	public static final int S_EMBLEM = 24;

	public static final int S_NUM_CHARACTER = 243;
	public static final int S_ENTER_WORLD_CHECK = 240;

	// Ver->TW1901142505
	public static final int S_OPCODE_TRADE = 113;

	public static final int S_OPCODE_SHOWSHOPBUYLIST = 223;

	public static final int S_OPCODE_CHARPACK = 240;

	public static final int S_OPCODE_DISCONNECT = 68;

	public static final int S_OPCODE_INITPACKET = 161;

	public static final int S_OPCODE_YES_NO = 208;

	public static final int S_OPCODE_RANGESKILLS = 143;

	public static final int S_OPCODE_ABILITY = 65;

	public static final int S_OPCODE_SKILLBRAVE = 59;

	public static final int S_OPCODE_SKILLSOUNDGFX = 131;

	public static final int S_OPCODE_DOACTIONGFX = 3;

	public static final int S_OPCODE_DEPOSIT = 35;

	public static final int S_OPCODE_CHARLIST = 26;

	public static final int S_OPCODE_MATERIAL = 93;

	public static final int S_OPCODE_TELEPORT = 201;

	public static final int S_OPCODE_EXP = 247;

	public static final int S_OPCODE_LETTER = 187;

	public static final int S_OPCODE_EFFECTLOCATION = 137;

	public static final int S_OPCODE_COMMONNEWS2 = 52;

	public static final int S_OPCODE_TRADEADDITEM = 234;

	public static final int S_OPCODE_SOUND = 215;

	public static final int S_OPCODE_ITEMAMOUNT = 125;

	public static final int S_OPCODE_NEWCHARWRONG = 17;

	public static final int S_OPCODE_TAXRATE = 199;

	public static final int S_OPCODE_LIGHT = 100;

	public static final int S_OPCODE_WAR = 150;

	public static final int S_OPCODE_WHISPERCHAT = 10;

	public static final int S_OPCODE_DEXUP = 231;

	public static final int S_OPCODE_INVLIST = 61;

	public static final int S_OPCODE_MOVEOBJECT = 23;

	public static final int S_OPCODE_CHARTITLE = 89;

	public static final int S_OPCODE_SERVERVERSION = 236;

	public static final int S_OPCODE_EXTENDED_PROTOBUF = 3;

	public static final int S_OPCODE_HPUPDATE = 37;

	public static final int S_OPCODE_INVIS = 7;

	public static final int S_OPCODE_BOARDREAD = 255;

	public static final int S_OPCODE_DELETEINVENTORYITEM = 38;

	public static final int S_OPCODE_CASTLEMASTER = 34;

	public static final int S_OPCODE_NORMALCHAT = 228;

	public static final int S_OPCODE_RESTART = 76;

	public static final int S_OPCODE_MPUPDATE = 57;

	public static final int S_OPCODE_MAPID = 61;

	public static final int S_OPCODE_SPMR = 13;

	public static final int S_OPCODE_WARTIME = 173;//

	public static final int S_OPCODE_CHARGECOUNT = 167;

	public static final int S_OPCODE_WEATHER = 169;

	public static final int S_OPCODE_BLESSOFEVA = 135;

	public static final int S_OPCODE_SURVIALCALL_NEEDTIME = 74;//

	public static final int S_OPCODE_HOUSEMAP = 194;

	public static final int S_OPCODE_SPOLY = 62;

	public static final int S_OPCODE_SHOWRETRIEVELIST = 122;

	public static final int S_OPCODE_IDENTIFYDESC = 190;

	public static final int S_OPCODE_GAMETIME = 173;

	public static final int S_OPCODE_RESURRECTION = 237;

	public static final int S_OPCODE_NEWMASTER = 60;

	public static final int S_OPCODE_PACKETBOX = 41;

	public static final int S_OPCODE_SHOWSHOPSELLLIST = 206;

	public static final int S_OPCODE_PLEDGE_WATCH = 147;

	public static final int S_OPCODE_TRADESTATUS = 157;

	public static final int S_OPCODE_PINKNAME = 167;

	public static final int S_OPCODE_OWNCHARATTRDEF = 229;

	public static final int S_OPCODE_SERVERMSG = 228;

	public static final int S_OPCODE_TELEPORTLOCK = 202;

	public static final int S_OPCODE_CHARRESET = 141;

	public static final int S_OPCODE_OWNCHARSTATUS2 = 97;

	public static final int S_OPCODE_SHOWHTML = 149;

	public static final int S_OPCODE_EMBLEM = 141;

	public static final int S_OPCODE_CHANGEHEADING = 225;

	public static final int S_OPCODE_ADDSKILL = 21;

	public static final int S_OPCODE_GLOBALCHAT = 238;// 2288

	public static final int S_OPCODE_TRUETARGET = 246;

	public static final int S_OPCODE_UPDATECLANID = 61;

	public static final int S_OPCODE_CHARAMOUNT = 5;

	public static final int S_OPCODE_POISON = 198;

	public static final int S_OPCODE_ITEMNAME = 121;// 2288

	public static final int S_OPCODE_ATTRIBUTE = 181;

	public static final int S_OPCODE_DUNGEONTELEPORT = 255;

	public static final int S_OPCODE_CHARVISUALUPDATE = 104;

	public static final int S_OPCODE_SELECTLIST = 168;

	public static final int S_OPCODE_NPCSHOUT = 130;

	public static final int S_OPCODE_MAIL = 25;

	public static final int S_OPCODE_DETELECHAROK = 159;

	public static final int S_OPCODE_SKILLICONSHIELD = 170; //2288

	public static final int S_OPCODE_HOUSELIST = 72;

	public static final int S_OPCODE_LOGINRESULT = 99;

	public static final int S_OPCODE_PRIVATESHOPLIST = 211;

	public static final int S_OPCODE_NEWCHARPACK = 188;

	public static final int S_OPCODE_BOARD = 253;

	public static final int S_OPCODE_ITEMCOLOR = 213;

	public static final int S_OPCODE_BLUEMESSAGE = 172;//2288

	public static final int S_OPCODE_STRUP = 4;

	public static final int S_OPCODE_MATCH_MAKING = -1;// 202

	public static final int S_OPCODE_PARALYSIS = 19;

	public static final int S_OPCODE_POLY = 98;

	public static final int S_OPCODE_COMMONNEWS = 207;

	public static final int S_OPCODE_OWNCHARSTATUS = 200;

	public static final int S_OPCODE_CURSEBLIND = 144;

	public static final int S_OPCODE_LIQUOR = 123;

	public static final int S_OPCODE_UPDATELEVELRANGE = 28;

	public static final int S_OPCODE_DELSKILL = 47;

	public static final int S_OPCODE_SKILLHASTE = 6;

	public static final int S_OPCODE_LAWFUL = 78;

	public static final int S_OPCODE_DRAWAL = 174;

	public static final int S_OPCODE_LOGINTOGAME = 240;

	public static final int S_OPCODE_SKILLBUY = 67;

	public static final int S_OPCODE_ADDITEM = 61;

	public static final int S_OPCODE_ATTACKPACKET = 128;

	public static final int S_OPCODE_SKILLBUYITEM = 130;

	public static final int S_OPCODE_SELECTTARGET = 73;

	public static final int S_OPCODE_INPUTAMOUNT = 214;

	public static final int S_OPCODE_REMOVE_OBJECT = 91;

	public static final int S_OPCODE_CHANGENAME = 53;

	public static final int S_OPCODE_HPMETER = 109;

	public static final int S_OPCODE_USEMAP = 135;

	public static final int S_OPCODE_BOOKMARKS = 164;

	// 8.8C->未用到
	// public static final int S_CHANGE_ITEM_TYPE = 32;
	// public static final int S_EXTENDED_HYBRID = 36;
	// public static final int S_BOOK_LIST = 40;
	// public static final int S_DECREE = 48;
	// public static final int S_SERVER_LIST = 58;
	// public static final int S_ARCHERARRANGE = 60;
	// public static final int S_ROLL_RESULT = 79;
	// public static final int S_PING = 86;
	// public static final int S_NEW_ACCOUNT_CHECK = 131;
	// public static final int S_MATCH_MAKING = 152;
	// public static final int S_CHANGE_COUNT = 155;
	// public static final int S_WANTED_LOGIN = 164;
	// public static final int S_MERCENARYSELECT = 165;
	// public static final int S_ATTACK_ALL = 168;
	// public static final int S_MERCENARYNAME = 177;
	// public static final int S_CHANGE_PASSWORD_CHECK = 187;
	// public static final int S_MERCENARYARRANGE = 207;
	// public static final int S_CLIENT_READY = 210;
	// public static final int S_CHANGE_ACCOUNTINFO_CHECK = 245;
	// public static final int S_MERCENARYEMPLOY = 253;
}
