"""Genera archivos opcodes.txt para uglyoldbob/l1j-client.

Formato de l1j-client (sdl_user.cpp): 512 líneas con un número cada una.
  - líneas   0..255: convert_client_packets[interno] = opcode en el cable
  - líneas 256..511: convert_server_packets[cable]   = opcode interno
  - 255 = sin traducción

Fuentes:
  - 2009: opcodes identificados en S3EP1U.bin (ver COMPARACION_L1J_CLIENT.md)
  - familias públicas: Opcodes.java / Opcode.xml de emuladores L1J (opcodes/fuentes/)

Uso: python3 generar_opcodes.py   (escribe en analisis/opcodes/)
"""
import os, re

AQUI = os.path.dirname(os.path.abspath(__file__))
OPC = os.path.join(AQUI, '..', 'opcodes')
FUENTES = os.path.join(OPC, 'fuentes')


def enum_l1j_client():
    """Lee los enums de opcodes internos de l1j-client (globals.h)."""
    src = open(os.path.join(FUENTES, 'l1j-client_globals.h')).read()
    out = {}
    for enum in ('server_packet_types', 'client_packet_types'):
        body = re.search(r'enum %s\s*\{(.*?)\};' % enum, src, re.S).group(1)
        val = -1
        for line in body.split('\n'):
            line = line.split('//')[0].strip().rstrip(',')
            if not line:
                continue
            if '=' in line:
                name, v = [x.strip() for x in line.split('=')]
                val = int(v)
            else:
                name, val = line, val + 1
            out[name] = val
    return out


INTERNO = enum_l1j_client()

# Nombre interno de l1j-client -> nombre de opcode L1J.
# Validado contra cot_opcodes.txt + analyzer/Opcodes.java del mismo repo (ver validar()).
SERVIDOR = {
    'SERVER_KEY': 'S_OPCODE_INITPACKET',
    'SERVER_VERSIONS': 'S_OPCODE_SERVERVERSION',
    'SERVER_DISCONNECT': 'S_OPCODE_DISCONNECT',
    'SERVER_ENTERGAME': 'S_OPCODE_LOGINRESULT',
    'SERVER_CHAR_DELETE': 'S_OPCODE_DETELECHAROK',
    'SERVER_CHAR_STAT': 'S_OPCODE_OWNCHARSTATUS',
    'SERVER_CHAR_ALIGNMENT': 'S_OPCODE_LAWFUL',
    'SERVER_TIME': 'S_OPCODE_GAMETIME',
    'SERVER_MPVALS': 'S_OPCODE_MPUPDATE',
    'SERVER_HPVALS': 'S_OPCODE_HPUPDATE',
    'SERVER_GROUNDITEM': 'S_OPCODE_CHARPACK',
    'SERVER_LIGHT': 'S_OPCODE_LIGHT',
    'SERVER_WEATHER': 'S_OPCODE_WEATHER',
    'SERVER_INV_ITEMS': 'S_OPCODE_INVLIST',
    'SERVER_ITEM_BLESS_STATUS': 'S_OPCODE_ITEMCOLOR',  # cot_opcodes.txt dice 20; el formato d+c coincide con S_ItemColor
    'SERVER_MESSAGE': 'S_OPCODE_SERVERMSG',
    'SERVER_CHANGE_SPMR': 'S_OPCODE_SPMR',
    'SERVER_MOVE_OBJECT': 'S_OPCODE_MOVEOBJECT',
    'SERVER_CHANGE_HEADING': 'S_OPCODE_CHANGEHEADING',
    'SERVER_REMOVE_OBJECT': 'S_OPCODE_REMOVE_OBJECT',
    'SERVER_MAP': 'S_OPCODE_MAPID',
    'SERVER_NEWS': 'S_OPCODE_COMMONNEWS',
    'SERVER_LOGIN_CHAR': 'S_OPCODE_CHARLIST',
    'SERVER_CREATE_STAT': 'S_OPCODE_NEWCHARWRONG',
    'SERVER_DEX_UPDATE': 'S_OPCODE_DEXUP',
    'SERVER_STR_UPDATE': 'S_OPCODE_STRUP',
    'SERVER_NUM_CHARS': 'S_OPCODE_CHARAMOUNT',
    'SERVER_TITLE': 'S_OPCODE_CHARTITLE',
    'SERVER_AC_ELEMENTAL_UPDATE': 'S_OPCODE_OWNCHARATTRDEF',
    'SERVER_CHAT_NORM': 'S_OPCODE_NORMALCHAT',
    'SERVER_CHAT_WHISPER': 'S_OPCODE_WHISPERCHAT',
    'SERVER_CHAT_GLOBAL': 'S_OPCODE_GLOBALCHAT',
    'SERVER_CHAT_SHOUT': 'S_OPCODE_NPCSHOUT',
}
CLIENTE = {
    'CLIENT_LOGIN': 'C_OPCODE_LOGINPACKET',
    'CLIENT_CHAT_WHISPER': 'C_OPCODE_CHATWHISPER',
    'CLIENT_DELETE_CHAR': 'C_OPCODE_DELETECHAR',
    'CLIENT_CHAT_GLOBAL': 'C_OPCODE_CHATGLOBAL',
    'CLIENT_CLICK': 'C_OPCODE_COMMONCLICK',
    'CLIENT_ALIVE': 'C_OPCODE_KEEPALIVE',
    'CLIENT_VERSION': 'C_OPCODE_CLIENTVERSION',
    'CLIENT_CREATE_CHAR': 'C_OPCODE_NEWCHAR',
    'CLIENT_USE_CHAR': 'C_OPCODE_LOGINTOSERVER',
    'CLIENT_INITGAME': 'C_OPCODE_LOGINTOSERVEROK',
    'CLIENT_CHAT': 'C_OPCODE_CHAT',
}

# Cliente 2009 (S3EP1U.bin): solo opcodes con certeza verificada o alta.
# nombre interno -> (opcode en el cable, certeza, evidencia)
TABLA_2009_SERVIDOR = {
    'SERVER_KEY': (125, 'verificada', 'manejador 0x48bee0 inicializa las claves'),
    'SERVER_VERSIONS': (89, 'alta', 'formato c[cdddddcc]c y tabla de páginas de códigos idéntica'),
    'SERVER_NEWS': (66, 'verificada', 'guarda el texto en Capture\\News%04d.txt'),
    'SERVER_NUM_CHARS': (80, 'alta', 'formato cc; reinicia 8 ranuras de personaje'),
    'SERVER_LOGIN_CHAR': (56, 'alta', 'formato sscchhhccccccccc'),
    # el mismo mensaje tiene un opcode antes de entrar al juego (74) y otro dentro (65)
    'SERVER_MESSAGE': ((74, 65), 'alta', '74: tabla previa al juego, h+c+s; 65: tabla del juego, hc+s, busca el texto por número'),
    'SERVER_MOVE_OBJECT': (126, 'alta', 'formato único dhhc'),
    'SERVER_CHAR_STAT': (59, 'verificada', 'formato dcdcccccchhhhcdcchcccc; reparte los campos en las variables del personaje'),
    'SERVER_AC_ELEMENTAL_UPDATE': (96, 'verificada', 'ccccc: escribe la CA y las 4 resistencias que también escribe el 59'),
    'SERVER_GROUNDITEM': (1, 'alta', 'formato hhdhccccdhsscdsscccc'),
    'SERVER_ENTERGAME': (41, 'alta', 'c: código de resultado de login; 22 = cuenta en uso, como REASON_ACCOUNT_IN_USE de L1J'),
    'SERVER_HPVALS': (34, 'verificada', 'hh: escribe el HP protegido (0x8ed488) y el HP máximo (0x8f07e4), los mismos que el 59'),
    'SERVER_MPVALS': (92, 'verificada', 'hh: escribe el MP protegido (0x8ed478) y el MP máximo (0x8f07e2), los mismos que el 59'),
    'SERVER_TIME': (58, 'alta', 'd: escribe la hora del juego (0x8f07d4), la misma que el 59'),
    'SERVER_CHAR_ALIGNMENT': (119, 'verificada', 'dh: guarda el alineamiento en el objeto y, si es el jugador, en 0x8f07e0 (el del 59)'),
    'SERVER_DEX_UPDATE': (51, 'alta', 'hc: duración y DEX (0x8f07db), con icono de efecto'),
    'SERVER_STR_UPDATE': (106, 'alta', 'hc: duración, STR (0x8f07d8) y peso (0x8f07f3)'),
    'SERVER_LIGHT': (124, 'alta', 'dc: tamaño de luz del objeto; mínimo 5 para el jugador'),
    'SERVER_WEATHER': (31, 'alta', 'c: nibble alto = tipo (0 nieve, 16 lluvia), nibble bajo = intensidad 0..3'),
    'SERVER_MAP': (73, 'alta', 'hc: guarda el número de mapa (0x816764) y el modo submarino'),
    'SERVER_INV_ITEMS': (8, 'alta', 'lista de dcchcdcsc; inicializa la ventana de inventario (LinInv1, .inv)'),
    'SERVER_CHAT_WHISPER': (47, 'alta', 'ss: lo muestra como "(%s) %s" (remitente y texto)'),
    'SERVER_CHAT_NORM': (71, 'media', 'cdshh; el tipo 2 (grito) calcula la dirección con las coordenadas'),
    'SERVER_CHAT_SHOUT': (50, 'media', 'cdshh, con casos especiales según el gráfico del NPC'),
    'SERVER_CHAT_GLOBAL': (3, 'media', 'cs: tipo de chat (1..12) y texto'),
    'SERVER_ITEM_BLESS_STATUS': (107, 'media', 'dc: cambia el estado de un objeto del inventario; el bit 0x80 muestra un mensaje'),
    'SERVER_REMOVE_OBJECT': (0, 'media', 'd: busca el objeto por id y lo elimina'),
}
TABLA_2009_CLIENTE = {
    'CLIENT_VERSION': (34, 'verificada', 'se envía en 0x45c0b0 justo después de recibir la semilla'),
    'CLIENT_CLICK': (46, 'verificada', 'se envía al cerrar la ventana de noticias (0x46cf60)'),
    'CLIENT_USE_CHAR': (89, 'alta', 'se envía con el nombre del personaje elegido (0x470a80)'),
    'CLIENT_CREATE_CHAR': (50, 'alta', 'formato único cscccccccc, coincide campo a campo'),
    'CLIENT_INITGAME': (49, 'media', 'formato único ccdd, igual al que envía l1j-client'),
    'CLIENT_CHAT': (18, 'verificada', 'ccs desde la caja de chat (0x420020); el tipo sale del prefijo: 0 normal, 2 grito, 4 @, 11 #, 13 %, 14 *, 15 ~'),
    'CLIENT_CHAT_GLOBAL': (115, 'verificada', 'ccs desde la misma caja de chat, solo para & (tipo 3) y $ (tipo 12), como C_ChatGlobal de L1J'),
    'CLIENT_CHAT_WHISPER': (92, 'media', 'css (destinatario y texto), enviado por la función contigua a la del chat (0x41ffa0)'),
}


def cables(v):
    """Un opcode o una tupla de opcodes (mismo mensaje con varios opcodes en el cable)."""
    return v if isinstance(v, tuple) else (v,)


def leer_tabla_publica(nombre):
    """Devuelve {NOMBRE_L1J: valor} de un Opcodes.java o un Opcode.xml."""
    ruta = os.path.join(FUENTES, nombre)
    t = open(ruta, encoding='utf-8-sig', errors='replace').read()
    if nombre.endswith('.xml'):
        tabla = {k: int(v) for k, v in re.findall(r'name="(\w+)" (?:code|key)="(\d+)"', t)}
    else:
        t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
        t = '\n'.join(l.split('//')[0] for l in t.split('\n'))
        tabla = {k: int(v) for k, v in re.findall(r'\b([SC]_OPCODE_\w+)\s*=\s*(\d+)\s*;', t)}
    return tabla


FAMILIAS = {
    # archivo de salida: (fuente, valores fijos que la fuente no declara)
    'opcodes_TW_3.80c.txt': ('TW_3.80c_Opcodes.java', {}),
    'opcodes_KR_3.80.txt': ('KR_3.80_Opcode.xml', {'S_OPCODE_INITPACKET': 150}),  # KeyPacket.java: 0x96
    'opcodes_TW_3.50c_EN.txt': ('TW_3.50c_Opcodes.java', {}),  # idéntica a EN_l1j-wk_Opcodes.java
    'opcodes_JP_3.63c.txt': ('JP_3.63c_Opcodes.java', {}),
}


def escribir(nombre, cli, srv):
    """cli: {interno: cable}; srv: {cable: interno}."""
    c = [255] * 256
    s = [255] * 256
    for i, w in cli.items():
        c[i] = w
    for w, i in srv.items():
        s[w] = i
    with open(os.path.join(OPC, nombre), 'w') as f:
        f.write('\n'.join(str(x) for x in c + s) + '\n')


def desde_tabla(tabla):
    cli, srv, faltan, conflictos = {}, {}, [], []
    for interno, l1j in CLIENTE.items():
        if l1j in tabla and tabla[l1j] < 256:
            cli[INTERNO[interno]] = tabla[l1j]
        else:
            faltan.append(interno)
    for interno, l1j in SERVIDOR.items():
        w = tabla.get(l1j)
        if w is None or w >= 256:
            faltan.append(interno)
        elif w in srv:
            conflictos.append('%s y %s comparten el opcode %d' % (
                [k for k, v in INTERNO.items() if v == srv[w]][0], interno, w))
        else:
            srv[w] = INTERNO[interno]
    return cli, srv, faltan, conflictos


def validar():
    """Comprueba el emparejamiento de nombres con dos fuentes del propio l1j-client:
    cot_opcodes.txt (traducción interno<->cable) y analyzer/Opcodes.java (nombres L1J)."""
    lineas = [int(x) for x in open(os.path.join(FUENTES, 'l1j-client_cot_opcodes.txt')).read().split()]
    cot_cli, cot_srv = lineas[:256], lineas[256:512]
    analyzer = leer_tabla_publica('l1j-client_analyzer_Opcodes.java')
    ok, mal, sin = [], [], []
    for interno, l1j in list(SERVIDOR.items()) + list(CLIENTE.items()):
        i = INTERNO[interno]
        if interno.startswith('SERVER'):
            cable = [w for w in range(256) if cot_srv[w] == i]
            cable = cable[0] if cable else None
        else:
            cable = cot_cli[i] if cot_cli[i] != 255 else None
        a = analyzer.get(l1j)
        if cable is None or a is None:
            sin.append(interno)
        elif cable == a:
            ok.append(interno)
        else:
            mal.append('%s: cot=%s analyzer(%s)=%s' % (interno, cable, l1j, a))
    return ok, mal, sin


if __name__ == '__main__':
    ok, mal, sin = validar()
    print('Validación del emparejamiento de nombres (cot_opcodes.txt vs analyzer/Opcodes.java):')
    print('  coinciden: %d  ->  %s' % (len(ok), ', '.join(ok)))
    print('  no coinciden: %d  %s' % (len(mal), mal))
    print('  sin dato en alguna fuente: %d  ->  %s' % (len(sin), ', '.join(sin)))

    cli = {INTERNO[k]: v[0] for k, v in TABLA_2009_CLIENTE.items()}
    srv = {w: INTERNO[k] for k, v in TABLA_2009_SERVIDOR.items() for w in cables(v[0])}
    escribir('opcodes_2009_S3EP1.txt', cli, srv)
    print('\nopcodes_2009_S3EP1.txt: %d cliente, %d servidor' % (len(cli), len(srv)))

    for salida, (fuente, fijos) in FAMILIAS.items():
        tabla = leer_tabla_publica(fuente)
        tabla.update(fijos)
        cli, srv, faltan, conflictos = desde_tabla(tabla)
        escribir(salida, cli, srv)
        print('%s: %d cliente, %d servidor; sin valor: %s%s' % (
            salida, len(cli), len(srv), ', '.join(faltan) or '-',
            ('; CONFLICTOS: ' + '; '.join(conflictos)) if conflictos else ''))
