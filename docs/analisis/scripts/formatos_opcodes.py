"""Para cada opcode servidor->cliente de S3EP1U.bin, sigue el manejador y lista
las cadenas de formato que pasa al lector de paquetes (0x483100).
Uso: python formatos_opcodes.py <ruta a S3EP1U.bin>   (requiere capstone)"""
import sys, struct, collections
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM

d = open(sys.argv[1], 'rb').read()
BASE, END = 0x401000, 0x8f4000
o = lambda va: va - 0x400000
md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
READER = 0x483100
FMT_CHARS = set(b'cdhsS')

def cstr(va):
    if not (0x401000 <= va < 0x8f4000): return None
    s = d[o(va):o(va) + 64].split(b'\0')[0]
    return s.decode() if s and set(s) <= FMT_CHARS else None

def follow_jmp(va):
    b = d[o(va)]
    if b == 0xe9: return va + 5 + struct.unpack_from('<i', d, o(va) + 1)[0]
    return va

def formats(fn, limit=0x6000, depth=1):
    """Recorre linealmente la función hasta 'limit' bytes y recoge formatos.
    Sigue llamadas directas un nivel (subrutinas que también leen el paquete)."""
    fn = follow_jmp(fn)
    out, last_imm = [], []
    for ins in md.disasm(d[o(fn):o(fn) + limit], fn):
        if ins.mnemonic == 'push' and ins.operands and ins.operands[0].type == X86_OP_IMM:
            last_imm.append(ins.operands[0].imm & 0xffffffff)
        elif ins.mnemonic == 'call' and ins.operands and ins.operands[0].type == X86_OP_IMM:
            tgt = ins.operands[0].imm & 0xffffffff
            if tgt == READER:
                for v in reversed(last_imm[-3:]):
                    s = cstr(v)
                    if s: out.append(s); break
            elif depth > 0 and 0x450000 <= tgt < 0x4c0000:
                out += ['>' + f for f in formats(tgt, 0x1500, depth - 1)]
            last_imm = []
        elif ins.mnemonic in ('int3',) or (ins.mnemonic == 'ret' and ins.address - fn > 0x40 and False):
            break
        if ins.mnemonic == 'ret' and not out and ins.address - fn > limit: break
    return out

def func_end_formats(fn):
    # versión acotada: corta en el primer 'ret' seguido de padding (nop/int3)
    fn = follow_jmp(fn)
    code = d[o(fn):o(fn) + 0x6000]
    end = len(code)
    for ins in md.disasm(code, fn):
        if ins.mnemonic == 'ret':
            nxt = d[o(ins.address + ins.size)]
            if nxt in (0x90, 0xcc):
                end = ins.address + ins.size - fn; break
    return formats(fn, end)

game = struct.unpack_from('<128I', d, o(0x4934f0))
default = collections.Counter(game).most_common(1)[0][0]
print('# opcode  manejador  formatos (">" = leído en una subrutina)')
for op, h in enumerate(game):
    if h == default: continue
    tgt = follow_jmp(h)
    f = func_end_formats(tgt)
    print('%3d 0x%02X  %#x  %s' % (op, op, tgt, ' | '.join(dict.fromkeys(f)) or '-'))
