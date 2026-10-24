"""Lista los paquetes cliente->servidor de S3EP1U.bin: busca cada llamada a
sendPacket(fmt, opcode, ...) (0x4a7df0) y recupera el formato y el opcode
empujados en la pila. Uso: python opcodes_cliente.py <S3EP1U.bin> (requiere capstone)"""
import sys, struct, collections
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM

d = open(sys.argv[1], 'rb').read()
o = lambda va: va - 0x400000
SEND = 0x4a7df0
md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True

def cstr(va):
    if not (0x401000 <= va < 0x8f4000): return None
    s = d[o(va):o(va) + 64].split(b'\0')[0]
    return s.decode() if s and set(s) <= set(b'cdhsSb') else None

calls = []
code = d[0x1000:0x4f4000]
for i in range(len(code) - 5):
    if code[i] == 0xe8 and (0x401000 + i + 5 + struct.unpack_from('<i', code, i + 1)[0]) & 0xffffffff == SEND:
        calls.append(0x401000 + i)

res = collections.defaultdict(set)
for c in calls:
    # desensambla hacia atrás una ventana y toma los push previos a la llamada
    for back in range(0x60, 0x08, -1):
        ins = list(md.disasm(d[o(c - back):o(c) + 5], c - back))
        if ins and ins[-1].address == c: break
    pushes = []
    for x in ins[:-1]:
        if x.mnemonic == 'call': pushes = []
        elif x.mnemonic == 'push':
            op = x.operands[0]
            pushes.append(op.imm & 0xffffffff if op.type == X86_OP_IMM else None)
    # cdecl: el último push es el primer argumento (fmt), el penúltimo el opcode
    fmt = cstr(pushes[-1]) if pushes and pushes[-1] is not None else None
    opc = pushes[-2] if len(pushes) >= 2 else None
    if fmt and fmt.startswith('c') and opc is not None and opc < 256:
        res[opc].add(fmt)
    else:
        res['?'].add('%#x fmt=%s' % (c, fmt))

print('# %d llamadas a sendPacket' % len(calls))
for k in sorted([k for k in res if k != '?']):
    print('%3d 0x%02X  %s' % (k, k, ' | '.join(sorted(res[k]))))
print('# sin resolver (opcode en registro):', len(res['?']))
for s in sorted(res['?']): print('   ', s)
