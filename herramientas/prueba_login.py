"""Cliente mínimo en Python contra el servidor L1J JP 3.63c local: recorre login, lista y
creación de personaje y entrada al juego, e imprime cada paquete recibido con su nombre.
Sirve para confirmar el flujo antes de implementarlo en C++ y para depurar el servidor.

Uso: python3 prueba_login.py [usuario] [clave] [personaje]
"""
import os, socket, struct, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'analisis', 'scripts'))
from lineage_proto import Cipher, Reader

S = {161: 'INITPACKET', 151: 'SERVERVERSION', 51: 'LOGINRESULT', 30: 'COMMONNEWS', 126: 'CHARAMOUNT',
     184: 'CHARLIST', 212: 'NEWCHARPACK', 153: 'NEWCHARWRONG', 5: 'DETELECHAROK', 131: 'LOGINTOGAME',
     145: 'OWNCHARSTATUS', 150: 'MAPID', 3: 'CHARPACK', 122: 'MOVEOBJECT', 185: 'REMOVE_OBJECT',
     199: 'CHANGEHEADING', 53: 'LIGHT', 194: 'GAMETIME', 193: 'WEATHER', 14: 'SERVERMSG', 174: 'SPMR',
     180: 'INVLIST', 40: 'PACKETBOX', 48: 'ADDSKILL', 15: 'OWNCHARATTRDEF', 216: 'OWNCHARSTATUS2'}
C = dict(CLIENTVERSION=127, LOGINPACKET=57, COMMONCLICK=53, NEWCHAR=253, LOGINTOSERVER=131, MOVECHAR=95)


class Conexion:
    def __init__(self, host='127.0.0.1', port=2000):
        self.s = socket.create_connection((host, port))
        self.s.settimeout(3)
        n = struct.unpack('<H', self._exacto(2))[0]
        cuerpo = self._exacto(n - 2)
        assert cuerpo[0] == 161, cuerpo[0]
        self.semilla = struct.unpack_from('<I', cuerpo, 1)[0]
        self.cif_rx, self.cif_tx = Cipher(self.semilla), Cipher(self.semilla)
        print('INITPACKET semilla=%08x resto=%s' % (self.semilla, cuerpo[5:].hex()))

    def _exacto(self, n):
        b = b''
        while len(b) < n:
            p = self.s.recv(n - len(b))
            if not p:
                raise EOFError
            b += p
        return b

    def enviar(self, cuerpo):
        cuerpo = bytearray(cuerpo)
        while len(cuerpo) % 4:
            cuerpo.append(0)
        c = self.cif_tx.encrypt(bytes(cuerpo))
        self.s.sendall(struct.pack('<H', len(c) + 2) + c)

    def recibir(self):
        n = struct.unpack('<H', self._exacto(2))[0]
        return self.cif_rx.decrypt(self._exacto(n - 2))

    def drenar(self, segundos=1.5):
        fin, todos = time.time() + segundos, []
        while time.time() < fin:
            try:
                p = self.recibir()
            except socket.timeout:
                break
            todos.append(p)
            print('  <- %3d %-14s %s' % (p[0], S.get(p[0], '?'), p[1:40].hex()))
        return todos


def cad(t):
    return t.encode('cp949') + b'\0'


if __name__ == '__main__':
    usuario, clave, nombre = (sys.argv[1:] + ['prueba', 'prueba', 'Prueba'][len(sys.argv) - 1:])[:3]
    c = Conexion()
    c.enviar(bytes([C['CLIENTVERSION']])); c.drenar(1)
    print('-> login', usuario); c.enviar(bytes([C['LOGINPACKET']]) + cad(usuario) + cad(clave)); c.drenar(1)
    print('-> commonclick'); c.enviar(bytes([C['COMMONCLICK']])); lista = c.drenar(1)
    if not any(p[0] == 184 and Reader(p[1:]).s() == nombre for p in lista):
        # clase 0 (príncipe), sexo 0, stats base de príncipe: str 13+8 dex 10 con 10 wis 11 cha 13 int 10
        print('-> newchar', nombre)
        c.enviar(bytes([C['NEWCHAR']]) + cad(nombre) + bytes([0, 0, 21, 10, 10, 11, 13, 10])); c.drenar(1)
    print('-> logintoserver', nombre); c.enviar(bytes([C['LOGINTOSERVER']]) + cad(nombre)); c.drenar(3)
