"""Empareja los opcodes del cliente 2009 con nombres L1J comparando formatos de paquete.

Entradas:
- resultados/formatos_opcodes_juego.txt   (lo que lee cada manejador servidor -> cliente)
- resultados/opcodes_cliente_servidor.txt (lo que envía el cliente)
- uno o más archivos de formatos_l1j.py   (formatos de los servidores L1J públicos)

Puntuación de un par (formato del binario b, formato L1J l, ambos sin la c del opcode):
- idénticos: len(b)
- b es prefijo de l (el cliente 2009 lee menos campos que un servidor más moderno): 0,8·len(b)
- l es prefijo de b: 0,6·len(l)
Solo cuentan formatos de 3 o más campos: los cortos (dc, d, hc...) los comparten demasiados paquetes.

Uso: python3 emparejar_formatos.py <carpeta resultados> formatos_l1j_A.txt [formatos_l1j_B.txt ...]
"""
import os, re, sys
from collections import defaultdict


def leer_binario(resultados):
    srv, cli = {}, {}
    for linea in open(os.path.join(resultados, 'formatos_opcodes_juego.txt'), encoding='utf-8'):
        m = re.match(r'\s*(\d+) 0x[0-9A-F]+\s+0x[0-9a-f]+\s+(.*)$', linea)
        if m:
            fmts = [f.strip() for f in m.group(2).split('|')]
            srv[int(m.group(1))] = [f for f in fmts if f and f != '-' and not f.startswith('>')]
    for linea in open(os.path.join(resultados, 'opcodes_cliente_servidor.txt'), encoding='utf-8'):
        m = re.match(r'\s*(\d+) 0x[0-9A-F]+\s+(.*)$', linea)
        if m:
            cli[int(m.group(1))] = [f.strip()[1:] for f in m.group(2).split('|')]
    return srv, cli


def leer_l1j(rutas):
    srv, cli = defaultdict(set), defaultdict(set)
    for ruta in rutas:
        fuente = os.path.basename(ruta)
        for linea in open(ruta, encoding='utf-8'):
            p = linea.split()
            if len(p) == 4 and p[0] in 'SC':
                fmt = p[3].replace('[', '').replace(']', '')[1:]
                (srv if p[0] == 'S' else cli)[p[1]].add((fmt, p[2], fuente))
    return srv, cli


def puntuar(b, l):
    if len(b) < 3:
        return 0
    if b == l:
        return len(b)
    if l.startswith(b):
        return 0.8 * len(b)
    if len(l) >= 3 and b.startswith(l):
        return 0.6 * len(l)
    return 0


def emparejar(binario, l1j):
    """Devuelve {opcode 2009: [(puntos, nombre L1J, formato binario, formato L1J, clase)]} ordenado."""
    res = {}
    for op, fmts in binario.items():
        cands = {}
        for nombre, variantes in l1j.items():
            for b in fmts:
                for l, cls, _ in variantes:
                    s = puntuar(b, l)
                    if s and s > cands.get(nombre, (0,))[0]:
                        cands[nombre] = (s, nombre, b, l, cls)
        res[op] = sorted(cands.values(), reverse=True)
    return res


def informe(titulo, res):
    print('#', titulo)
    print('# opcode 2009: candidatos (puntos nombre [formato binario ~ formato L1J, clase])')
    # cuántos opcodes 2009 tienen a cada nombre como mejor candidato
    mejores = defaultdict(list)
    for op, c in res.items():
        if c:
            mejores[c[0][1]].append(op)
    for op in sorted(res):
        c = res[op]
        if not c:
            continue
        top = c[0]
        unico = len(c) == 1 or c[1][0] < top[0]
        exclusivo = mejores[top[1]] == [op]
        marca = 'UNICO' if unico and exclusivo else ('empate' if not unico else 'compartido')
        print('%3d  %-7s %s' % (op, marca, '; '.join('%.1f %s [%s ~ %s, %s]' % x for x in c[:4])))


if __name__ == '__main__':
    bsrv, bcli = leer_binario(sys.argv[1])
    lsrv, lcli = leer_l1j(sys.argv[2:])
    informe('servidor -> cliente', emparejar(bsrv, lsrv))
    print()
    informe('cliente -> servidor', emparejar(bcli, lcli))
