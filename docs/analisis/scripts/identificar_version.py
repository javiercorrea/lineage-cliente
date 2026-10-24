"""Identifica la familia de opcodes de un cliente/servidor Lineage a partir de una
captura de tráfico, y decodifica la sesión.

El primer paquete del servidor viaja SIN cifrar: su opcode y su longitud identifican
la familia. Con la semilla que contiene se descifra el resto, y el opcode del paquete
de versión (el primero cifrado del servidor) confirma la familia.

Uso:
  python3 identificar_version.py captura.pcap[ng] [--puerto 2000] [--familia NOMBRE] [--max 200]
  python3 identificar_version.py --hex "<bytes del primer paquete del servidor>"

Requiere solo la biblioteca estándar y lineage_proto.py (misma carpeta).
"""
import argparse, os, struct, sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lineage_proto as lp
import generar_opcodes as gen

# opcode de inicio -> lista de (familia, longitud de cuerpo esperada, opcode de versión del servidor)
FAMILIAS = {
    125: [('2009_S3EP1 (KR, S3EP1U.bin)', 5, 89, None)],
    150: [('KR_3.80', 16, 77, 'KR_3.80_Opcode.xml'),
          ('TW_3.80c', 16, 139, 'TW_3.80c_Opcodes.java')],
    161: [('JP_3.63c', None, 151, 'JP_3.63c_Opcodes.java')],
    27:  [('TW_3.50c / EN', None, 17, 'TW_3.50c_Opcodes.java')],
    65:  [('l1j-client interno (globals.h)', 5, 10, None)],
    50:  [('COT (l1j-client cot_opcodes.txt / analyzer)', None, 119, 'l1j-client_analyzer_Opcodes.java')],
}


# ---------- lectura de capturas (pcap clásico y pcapng; Ethernet/Linux SLL/loopback + IPv4 + TCP)

def _paquetes_pcap(data):
    magic = data[:4]
    if magic in (b'\xd4\xc3\xb2\xa1', b'\x4d\x3c\xb2\xa1'):
        e = '<'
    elif magic in (b'\xa1\xb2\xc3\xd4', b'\xa1\xb2\x3c\x4d'):
        e = '>'
    else:
        return None
    linktype = struct.unpack_from(e + 'I', data, 20)[0]
    p = 24
    while p + 16 <= len(data):
        _, _, incl, _ = struct.unpack_from(e + 'IIII', data, p)
        yield linktype, data[p + 16:p + 16 + incl]
        p += 16 + incl


def _paquetes_pcapng(data):
    if data[:4] != b'\x0a\x0d\x0d\x0a':
        return None
    p, e, links = 0, '<', []
    while p + 12 <= len(data):
        btype = struct.unpack_from(e + 'I', data, p)[0]
        if btype == 0x0A0D0D0A:
            e = '<' if data[p + 8:p + 12] == b'\x4d\x3c\x2b\x1a' else '>'
            links = []
        blen = struct.unpack_from(e + 'I', data, p + 4)[0]
        if btype == 1:      # Interface Description Block
            links.append(struct.unpack_from(e + 'H', data, p + 8)[0])
        elif btype == 6:    # Enhanced Packet Block
            iface, _, _, cap = struct.unpack_from(e + 'IIII', data, p + 8)[:4]
            yield links[iface] if iface < len(links) else 1, data[p + 28:p + 28 + cap]
        elif btype == 3:    # Simple Packet Block
            yield (links[0] if links else 1), data[p + 12:p + blen - 4]
        if blen < 12:
            break
        p += blen


def _ipv4(link, frame):
    if link == 1:                                   # Ethernet
        off, etype = 14, struct.unpack_from('>H', frame, 12)[0]
        if etype == 0x8100:
            off, etype = 18, struct.unpack_from('>H', frame, 16)[0]
        return frame[off:] if etype == 0x0800 else None
    if link == 113:                                 # Linux SLL
        return frame[16:] if struct.unpack_from('>H', frame, 14)[0] == 0x0800 else None
    if link in (0, 108):                            # loopback BSD (macOS)
        return frame[4:]
    if link in (101, 228):                          # IP crudo
        return frame
    return None


def flujos_tcp(ruta, puerto=None):
    """Devuelve {(cliente, servidor): {'c2s': bytes, 's2c': bytes}} reensamblando por número de secuencia."""
    data = open(ruta, 'rb').read()
    gen_paq = _paquetes_pcap(data) or _paquetes_pcapng(data)
    if gen_paq is None:
        sys.exit('Formato de captura no reconocido (se esperaba pcap o pcapng)')
    segs = {}
    syn = {}
    for link, frame in gen_paq:
        ip = _ipv4(link, frame)
        if not ip or len(ip) < 20 or ip[0] >> 4 != 4 or ip[9] != 6:
            continue
        ihl = (ip[0] & 15) * 4
        total = struct.unpack_from('>H', ip, 2)[0]
        src, dst = ip[12:16], ip[16:20]
        tcp = ip[ihl:total]
        sp, dp, seq = struct.unpack_from('>HHI', tcp, 0)
        flags = tcp[13]
        payload = tcp[(tcp[12] >> 4) * 4:]
        a, b = (src, sp), (dst, dp)
        if puerto and puerto not in (sp, dp):
            continue
        if flags & 0x02 and not flags & 0x10:       # SYN sin ACK: a es el cliente
            syn[(a, b)] = True
        if payload:
            segs.setdefault((a, b), {})[seq] = payload
    sesiones = {}
    for (a, b), d in segs.items():
        # decide quién es el cliente: SYN visto, o el puerto conocido es el del servidor
        if (a, b) in syn or (puerto and b[1] == puerto):
            cli, srv, dir_ = a, b, 'c2s'
        elif (b, a) in syn or (puerto and a[1] == puerto):
            cli, srv, dir_ = b, a, 's2c'
        else:
            cli, srv, dir_ = (a, b, 'c2s') if a[1] > b[1] else (b, a, 's2c')
        buf, esperado = bytearray(), None
        for s in sorted(d):
            p = d[s]
            if esperado is not None and s < esperado:   # retransmisión o solapamiento
                p = p[esperado - s:]
            buf += p
            esperado = s + len(d[s]) if esperado is None or s + len(d[s]) > esperado else esperado
        sesiones.setdefault((cli, srv), {})[dir_] = bytes(buf)
    return sesiones


def tramas(buf):
    p = 0
    while p + 2 <= len(buf):
        n = struct.unpack_from('<H', buf, p)[0]
        if n < 2 or p + n > len(buf):
            break
        yield buf[p + 2:p + n]
        p += n


# ---------- identificación

def identificar(primer_cuerpo, segundo_cuerpo_descifrado=None):
    op = primer_cuerpo[0]
    cands = FAMILIAS.get(op)
    if not cands:
        return op, None, 'opcode de inicio %d desconocido: familia nueva (posible cliente 2014)' % op
    if segundo_cuerpo_descifrado:
        for c in cands:
            if segundo_cuerpo_descifrado[0] == c[2]:
                return op, c, 'confirmada por el paquete de versión (opcode %d)' % c[2]
    if len(cands) == 1:
        return op, cands[0], 'por el opcode de inicio (sin confirmar con el paquete de versión)'
    return op, None, 'ambigua entre: ' + ', '.join(c[0] for c in cands)


def nombres(fuente):
    if not fuente:
        return {}, {}
    t = gen.leer_tabla_publica(fuente)
    s = {v: k for k, v in t.items() if k.startswith('S_')}
    c = {v: k for k, v in t.items() if k.startswith('C_')}
    return s, c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('captura', nargs='?')
    ap.add_argument('--hex', help='primer paquete del servidor en hexadecimal (con o sin cabecera de longitud)')
    ap.add_argument('--puerto', type=int, default=None, help='puerto TCP del servidor (por ejemplo 2000)')
    ap.add_argument('--max', type=int, default=200, help='paquetes a mostrar por sentido')
    a = ap.parse_args()

    if a.hex:
        try:
            b = bytes.fromhex(a.hex.replace(' ', '').replace(':', ''))
        except ValueError:
            sys.exit('--hex: no es hexadecimal válido (¿falta un dígito?)')
        if len(b) >= 2 and struct.unpack_from('<H', b)[0] == len(b):
            b = b[2:]
        op, fam, motivo = identificar(b)
        print('opcode de inicio: %d (0x%02X), cuerpo de %d bytes' % (op, op, len(b)))
        print('familia:', fam[0] if fam else '-', '|', motivo)
        if len(b) >= 5:
            print('semilla: 0x%08X' % struct.unpack_from('<I', b, 1)[0])
        return

    if not a.captura:
        ap.error('indica una captura o --hex')
    for (cli, srv), d in flujos_tcp(a.captura, a.puerto).items():
        s2c, c2s = list(tramas(d.get('s2c', b''))), list(tramas(d.get('c2s', b'')))
        if not s2c:
            continue
        ip = lambda x: '%d.%d.%d.%d:%d' % (*x[0], x[1])
        print('=== sesión %s -> %s: %d paquetes del servidor, %d del cliente' % (ip(cli), ip(srv), len(s2c), len(c2s)))
        primero = s2c[0]
        if len(primero) < 5:
            print('primer paquete demasiado corto, no parece Lineage'); continue
        seed = struct.unpack_from('<I', primero, 1)[0]
        rx, tx = lp.Cipher(seed), lp.Cipher(seed)
        srv_plano = [primero] + [rx.decrypt(x) for x in s2c[1:]]
        cli_plano = [tx.decrypt(x) for x in c2s]
        op, fam, motivo = identificar(primero, srv_plano[1] if len(srv_plano) > 1 else None)
        print('opcode de inicio %d, cuerpo %d bytes, semilla 0x%08X' % (op, len(primero), seed))
        print('familia: %s | %s' % (fam[0] if fam else '-', motivo))
        n_s, n_c = nombres(fam[3] if fam else None)
        print('opcodes más frecuentes servidor->cliente:', Counter(x[0] for x in srv_plano).most_common(12))
        print('opcodes más frecuentes cliente->servidor:', Counter(x[0] for x in cli_plano).most_common(12))
        print('--- primeros paquetes descifrados (S = servidor, C = cliente; orden por sentido)')
        for etiqueta, lista, tabla in (('S', srv_plano, n_s), ('C', cli_plano, n_c)):
            for x in lista[:a.max]:
                print('%s %3d %-28s %4d  %s' % (etiqueta, x[0], tabla.get(x[0], ''), len(x), x[:24].hex(' ')))


if __name__ == '__main__':
    main()
