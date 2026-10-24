"""Compara los opcodes identificados en el cliente 2009 con las tablas públicas de opcodes/fuentes/.
Uso: python3 comparar_tablas.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generar_opcodes as g

# nombre L1J -> opcode(s) 2009, derivado de las tablas de generar_opcodes.py
REF_2009 = {}
for interno, (cable, _, _) in list(g.TABLA_2009_SERVIDOR.items()) + list(g.TABLA_2009_CLIENTE.items()):
    nombre = g.SERVIDOR.get(interno) or g.CLIENTE.get(interno)
    if nombre:
        REF_2009[nombre] = g.cables(cable)
fuentes = sorted(f for f in os.listdir(g.FUENTES) if f.endswith(('.java', '.xml')))
tablas = {f: g.leer_tabla_publica(f) for f in fuentes}
tablas['KR_3.80_Opcode.xml'].setdefault('S_OPCODE_INITPACKET', 150)
cortos = [f.split('_Opcode')[0][:16] for f in fuentes]
print('%-26s %6s ' % ('opcode', '2009') + ' '.join('%16s' % c for c in cortos))
for k, v in REF_2009.items():
    print('%-26s %6s ' % (k, '/'.join(map(str, v))) + ' '.join('%16s' % tablas[f].get(k, '-') for f in fuentes))
print('\nCoincidencias con 2009 por tabla:')
for f in fuentes:
    n = sum(1 for k, v in REF_2009.items() if tablas[f].get(k) in v)
    print('  %-40s %d de %d' % (f, n, len(REF_2009)))
