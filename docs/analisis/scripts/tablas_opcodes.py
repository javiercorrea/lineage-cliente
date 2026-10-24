"""Extrae las tablas de despacho de opcodes servidor->cliente de S3EP1U.bin.
Uso: python3 tablas_opcodes.py <ruta a S3EP1U.bin>"""
import sys, struct, collections

d = open(sys.argv[1], 'rb').read()
o = lambda va: va - 0x400000

game = struct.unpack_from('<128I', d, o(0x4934f0))
default = collections.Counter(game).most_common(1)[0][0]
handled = [i for i, h in enumerate(game) if h != default]
print('En juego: tabla de 128 entradas en 0x4934f0, manejador por defecto %#x' % default)
print('%d opcodes con manejador propio:' % len(handled))
for i in handled:
    print('  %3d (0x%02X) -> %#x' % (i, i, game[i]))

idx = d[o(0x493720):o(0x493720) + 0x7a]
stubs = struct.unpack_from('<11I', d, o(0x4936f0))
dflt = collections.Counter(idx).most_common(1)[0][0]
print('\nFuera del juego: indice en 0x493720 (opcode-4), stubs en 0x4936f0')
for i, k in enumerate(idx):
    if k != dflt:
        stub = stubs[k]
        target = stub + 5 + struct.unpack_from('<i', d, o(stub) + 1)[0]  # jmp rel32
        print('  %3d (0x%02X) -> stub %#x -> %#x' % (i + 4, i + 4, stub, target & 0xffffffff))
