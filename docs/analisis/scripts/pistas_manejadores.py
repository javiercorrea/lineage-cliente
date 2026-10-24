"""Para cada manejador servidor -> cliente de S3EP1U.bin, lista las pistas de comportamiento:
cadenas que referencia (ASCII o coreano CP949), formatos que lee y funciones a las que llama.
Sirve para poner nombre a los opcodes que el formato solo no distingue.

Uso: python pistas_manejadores.py <ruta a S3EP1U.bin>   (requiere capstone)
"""
import sys, struct, collections
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

d = open(sys.argv[1], 'rb').read()
o = lambda va: va - 0x400000
md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
READER, SEND = 0x483100, 0x4a7df0
DATOS = (0x4f4000, 0x7f0000)   # .rdata/.data del volcado; las cadenas no están en el código


def cadena(va):
    if not (DATOS[0] <= va < DATOS[1]):
        return None
    s = d[o(va):o(va) + 120].split(b'\0')[0]
    if len(s) < 3 or len(s) >= 119:
        return None
    if all(32 <= c < 127 or c in (9, 10, 13) for c in s):
        t = s.decode()
        # descarta formatos del lector y basura corta sin letras
        if set(t) <= set('cdhsSb') or sum(ch.isalpha() for ch in t) < 2:
            return None
        return t
    try:
        t = s.decode('cp949')
    except UnicodeDecodeError:
        return None
    return t if any('가' <= ch <= '힣' for ch in t) else None


def limites(fn, maximo=0x3000):
    """Fin de la función: primer ret seguido de relleno, o el salto más lejano alcanzado."""
    fin, lejos = fn + maximo, fn
    for ins in md.disasm(d[o(fn):o(fn) + maximo], fn):
        if ins.mnemonic.startswith('j') and ins.operands and ins.operands[0].type == X86_OP_IMM:
            t = ins.operands[0].imm & 0xffffffff
            if fn <= t < fn + maximo:
                lejos = max(lejos, t)
        if ins.mnemonic == 'ret' and ins.address >= lejos:
            return ins.address + ins.size
        if ins.mnemonic == 'int3':
            return ins.address
    return fin


def pistas(fn, prof=1, vistos=None):
    vistos = vistos if vistos is not None else set()
    if fn in vistos:
        return [], [], []
    vistos.add(fn)
    if d[o(fn)] == 0xe9:
        fn = fn + 5 + struct.unpack_from('<i', d, o(fn) + 1)[0]
    cads, llamadas, envios = [], [], []
    ultimo_push = []
    for ins in md.disasm(d[o(fn):o(limites(fn))], fn):
        for op in ins.operands:
            v = None
            if op.type == X86_OP_IMM:
                v = op.imm & 0xffffffff
            elif op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                v = op.mem.disp & 0xffffffff
            if v:
                s = cadena(v)
                if s and s not in cads:
                    cads.append(s)
        if ins.mnemonic == 'push' and ins.operands[0].type == X86_OP_IMM:
            ultimo_push.append(ins.operands[0].imm & 0xffffffff)
        if ins.mnemonic == 'call' and ins.operands[0].type == X86_OP_IMM:
            t = ins.operands[0].imm & 0xffffffff
            if t == SEND and len(ultimo_push) >= 2:
                # sendPacket(fmt, opcode, ...): los dos últimos push son opcode y formato
                envios.append(ultimo_push[-2] if ultimo_push[-2] < 256 else None)
            elif t != READER:
                llamadas.append(t)
                if prof > 0 and 0x401000 <= t < 0x4f0000:
                    c2, _, e2 = pistas(t, prof - 1, vistos)
                    cads += ['>' + x for x in c2 if x not in cads and '>' + x not in cads]
                    envios += e2
            ultimo_push = []
    return cads, llamadas, envios


def main():
    game = struct.unpack_from('<128I', d, o(0x4934f0))
    defecto = collections.Counter(game).most_common(1)[0][0]
    stubs = {}
    idx = d[o(0x493720):o(0x493720) + 122]
    tabla_pre = struct.unpack_from('<20I', d, o(0x4936f0))
    for i, b in enumerate(idx):
        if tabla_pre[b] != tabla_pre[-1] and b < 20:
            stubs[i + 4] = tabla_pre[b]
    print('# manejadores dentro del juego (> = cadena encontrada en una subrutina llamada)')
    for op in range(128):
        if game[op] == defecto:
            continue
        # el caso de la tabla es un bloque "call manejador; jmp fin"
        ins = next(md.disasm(d[o(game[op]):o(game[op]) + 16], game[op]))
        fn = ins.operands[0].imm & 0xffffffff if ins.mnemonic == 'call' else game[op]
        cads, _, env = pistas(fn)
        print('%3d 0x%06x envia=%s | %s' % (op, fn, sorted(set(x for x in env if x is not None)), ' | '.join(cads[:14])))


if __name__ == '__main__':
    main()
