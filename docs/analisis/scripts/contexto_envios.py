"""Para cada paquete cliente -> servidor de S3EP1U.bin, muestra dónde se envía: función que
llama a sendPacket, cadenas que usa esa función y caracteres que compara (por ejemplo los
prefijos de chat '!', '"', '@'). Sirve para poner nombre a los opcodes del cliente.

Uso: python contexto_envios.py <ruta a S3EP1U.bin> [opcode ...]   (requiere capstone)
"""
import sys, os, struct, collections
from capstone.x86 import X86_OP_IMM

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pistas_manejadores as p   # reutiliza el lector de cadenas y el desensamblador

SEND = 0x4a7df0


def inicio_funcion(va):
    """Retrocede hasta el prólogo 'push ebp; mov ebp, esp' (55 8B EC) precedido de relleno o ret."""
    for a in range(va, va - 0x8000, -1):
        if p.d[p.o(a):p.o(a) + 3] == b'\x55\x8b\xec' and p.d[p.o(a) - 1] in (0xcc, 0x90, 0xc3, 0xc2):
            return a
    return None


def envios():
    code = p.d[0x1000:0x4f4000]
    for i in range(len(code) - 5):
        if code[i] == 0xe8 and (0x401000 + i + 5 + struct.unpack_from('<i', code, i + 1)[0]) & 0xffffffff == SEND:
            c = 0x401000 + i
            for back in range(0x60, 0x08, -1):
                ins = list(p.md.disasm(p.d[p.o(c - back):p.o(c) + 5], c - back))
                if ins and ins[-1].address == c:
                    break
            pushes = []
            for x in ins[:-1]:
                if x.mnemonic == 'call':
                    pushes = []
                elif x.mnemonic == 'push':
                    op = x.operands[0]
                    pushes.append(op.imm & 0xffffffff if op.type == X86_OP_IMM else None)
            fmt = None
            if pushes and pushes[-1]:
                s = p.d[p.o(pushes[-1]):p.o(pushes[-1]) + 40].split(b'\0')[0] if 0x401000 <= pushes[-1] < 0x8f4000 else b''
                fmt = s.decode() if s and set(s) <= set(b'cdhsSb') else None
            opc = pushes[-2] if len(pushes) >= 2 and pushes[-2] is not None and pushes[-2] < 256 else None
            yield c, opc, fmt


def contexto(fn):
    cads, chars = [], set()
    if fn is None:
        return cads, chars
    for ins in p.md.disasm(p.d[p.o(fn):p.o(p.limites(fn, 0x4000))], fn):
        for op in ins.operands:
            if op.type == X86_OP_IMM:
                v = op.imm & 0xffffffff
                s = p.cadena(v)
                if s and s not in cads:
                    cads.append(s)
                if ins.mnemonic == 'cmp' and v < 256 and chr(v) in '!"#$%&@/*-+':
                    chars.add(chr(v))
    return cads, chars


def main():
    filtro = {int(x) for x in sys.argv[2:]}
    por_op = collections.defaultdict(list)
    for c, opc, fmt in envios():
        por_op[opc].append((c, fmt))
    for opc in sorted(por_op, key=lambda x: -1 if x is None else x):
        if filtro and opc not in filtro:
            continue
        sitios = por_op[opc]
        print('== opcode %s: %d envíos' % (opc, len(sitios)))
        vistas = set()
        for c, fmt in sitios:
            fn = inicio_funcion(c)
            if fn in vistas:
                continue
            vistas.add(fn)
            cads, chars = contexto(fn)
            print('   %#x en función %s fmt=%s compara=%s | %s' % (
                c, hex(fn) if fn else '?', fmt, ''.join(sorted(chars)) or '-', ' | '.join(cads[:8])))


if __name__ == '__main__':
    main()
