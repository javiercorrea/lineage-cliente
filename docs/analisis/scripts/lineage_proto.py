"""Implementación de referencia del protocolo de red del cliente Lineage.

Reconstruido a partir del volcado desempaquetado S3EP1U.bin (cliente 2009).
Ver analisis/PROTOCOLO_RED.md para la descripción completa.
"""
import struct

ROTL = lambda x, n: ((x << n) | (x >> (32 - n))) & 0xFFFFFFFF

C1 = 0x9C30D539  # constantes tomadas de las S-box de Blowfish
C2 = 0x930FD7E2
C3 = 0x7C72E993
C4 = 0x287EFFC3


def derive_key(seed):
    """Clave inicial de 8 bytes a partir de la semilla del paquete de inicio."""
    k0 = ROTL(seed ^ C1, 19)
    k1 = C2 ^ k0 ^ C3
    return bytearray(struct.pack('<II', k0, k1))


class Cipher:
    """Una instancia por sentido. El cliente cifra con `encrypt` y el servidor descifra con `decrypt`."""

    def __init__(self, seed):
        self.key = derive_key(seed)

    def _advance(self, plain):
        # Tras cada paquete: key[0..3] ^= los 4 primeros bytes en claro; key[4..7] += C4
        k0, k1 = struct.unpack('<II', self.key)
        k0 ^= struct.unpack_from('<I', plain)[0]
        k1 = (k1 + C4) & 0xFFFFFFFF
        self.key[:] = struct.pack('<II', k0, k1)

    def encrypt(self, data):
        d, k = bytearray(data), self.key
        plain = bytes(d[:4])
        d[0] ^= k[0]
        for i in range(1, len(d)):
            d[i] ^= d[i - 1] ^ k[i & 7]
        d[3] ^= k[2]
        d[2] ^= d[3] ^ k[3]
        d[1] ^= d[2] ^ k[4]
        d[0] ^= d[1] ^ k[5]
        self._advance(plain)
        return bytes(d)

    def decrypt(self, data):
        d, k = bytearray(data), self.key
        b3 = d[3]; d[3] ^= k[2]
        b2 = d[2]; d[2] ^= b3 ^ k[3]
        b1 = d[1]; d[1] ^= b2 ^ k[4]
        b0 = d[0] ^ b1 ^ k[5]
        d[0] = b0 ^ k[0]
        prev = b0
        for i in range(1, len(d)):
            cur = d[i]
            d[i] = cur ^ prev ^ k[i & 7]
            prev = cur
        self._advance(bytes(d[:4]))
        return bytes(d)


def frame(body):
    """Cabecera de 2 bytes LE con la longitud total (cabecera incluida)."""
    return struct.pack('<H', len(body) + 2) + body


def pad_client_body(body):
    """El cliente rellena con ceros el cuerpo hasta un múltiplo de 4 antes de cifrarlo."""
    return body + b'\0' * (-len(body) % 4)


class Reader:
    """Lector de campos con los mismos códigos de formato que el cliente: c h d s S."""

    def __init__(self, body):
        self.b, self.p = body, 0

    def c(self):
        v = self.b[self.p]; self.p += 1; return v

    def h(self):
        v = struct.unpack_from('<H', self.b, self.p)[0]; self.p += 2; return v

    def d(self):
        v = struct.unpack_from('<I', self.b, self.p)[0]; self.p += 4; return v

    def s(self, encoding='cp949'):
        end = self.b.index(b'\0', self.p)
        v = self.b[self.p:end].decode(encoding, 'replace'); self.p = end + 1; return v

    def S(self):
        end = self.p
        while self.b[end:end + 2] != b'\0\0':
            end += 2
        v = self.b[self.p:end].decode('utf-16-le'); self.p = end + 2; return v

    def read(self, fmt):
        return [getattr(self, ch)() for ch in fmt]
