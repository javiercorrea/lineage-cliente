"""Lista las secciones PE y su entropía para detectar binarios empaquetados.
Uso: python3 secciones_pe.py <exe> [<exe> ...]"""
import sys, struct, math, collections, datetime

for f in sys.argv[1:]:
    d = open(f, 'rb').read()
    print('==', f, len(d), 'bytes')
    if d[:2] != b'MZ':
        print('   no es PE'); continue
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    ts = struct.unpack_from('<I', d, pe + 8)[0]
    print('   compilado:', datetime.datetime.utcfromtimestamp(ts))
    so = pe + 24 + opt
    for i in range(n):
        name, vs, va, rs, ro = struct.unpack_from('<8sIIII', d, so + 40 * i)
        chunk = d[ro:ro + rs]
        c = collections.Counter(chunk)
        e = -sum(v / len(chunk) * math.log2(v / len(chunk)) for v in c.values()) if chunk else 0
        print('   %-10r va=%#09x raw=%#09x entropia=%.2f' % (name.rstrip(b'\0'), va, rs, e))
