"""Extrae de un servidor L1J (Java) el formato de cada paquete, para compararlo con el binario.

- Servidor -> cliente: en serverpackets/S_*.java, la secuencia writeC/H/D/S/... que sigue a
  cada writeC(Opcodes.S_OPCODE_X).
- Cliente -> servidor: en clientpackets/C_*.java, la secuencia readC/H/D/S/... que lee la
  clase, y el nombre C_OPCODE_X que PacketHandler.java le asocia.

Es una aproximación textual: en un if/else solo se toma la primera rama, y los bucles se
cuentan una vez (se marcan con [ ]). Basta para comparar con los formatos del binario.

Uso: python3 formatos_l1j.py <carpeta con PacketHandler.java> > formatos_l1j.txt
(las clases están en serverpackets/ y clientpackets/, o en server/ y client/)
"""
import os, re, sys

TIPOS = {'C': 'c', 'H': 'h', 'D': 'd', 'S': 's', 'CH': 'h', 'F': 'f', 'Byte': 'b', 'P': 'b', 'L': 'l', 'Exp': 'd'}


def _secuencia(cuerpo, patron):
    """Recorre el código: devuelve la secuencia de llamadas, saltando ramas else y marcando bucles."""
    out, pila, i = [], [], 0
    tok = re.compile(r'\belse\b\s*(\{|if)|\b(for|while)\s*\(|\{|\}|' + patron)
    saltar = 0
    while True:
        m = tok.search(cuerpo, i)
        if not m:
            break
        i = m.end()
        t = m.group(0)
        if m.group(1) == '{':                        # else { ... }: se salta
            pila.append('else'); saltar += 1
        elif m.group(1) == 'if':                     # else if (...) { ... }: también
            j = cuerpo.find('{', i)
            if j < 0:
                break
            i = j + 1
            pila.append('else'); saltar += 1
        elif m.group(2):                             # bucle
            j, prof = i, 1                           # cierra el paréntesis del bucle
            while j < len(cuerpo) and prof:
                prof += {'(': 1, ')': -1}.get(cuerpo[j], 0); j += 1
            i = j
            k = re.match(r'\s*\{', cuerpo[i:])
            if k:
                i += k.end(); pila.append('loop')
                if not saltar:
                    out.append('[')
        elif t == '{':
            pila.append('{')
        elif t == '}':
            if not pila:
                continue
            p = pila.pop()
            if p == 'else':
                saltar -= 1
            elif p == 'loop' and not saltar:
                out.append(']')
        elif not saltar:
            out.append(m)
    return out


def _dir(raiz, *nombres):
    for n in nombres:
        if os.path.isdir(os.path.join(raiz, n)):
            return os.path.join(raiz, n)
    sys.exit('no encuentro %s en %s' % (' ni '.join(nombres), raiz))


def servidor(raiz):
    res = []
    d = _dir(raiz, 'serverpackets', 'server')
    for f in sorted(os.listdir(d)):
        if not f.startswith('S_'):
            continue
        src = open(os.path.join(d, f), encoding='utf-8', errors='replace').read()
        seq = _secuencia(src, r'write(CH|Exp|Byte|[CHDSFPL])\s*\(([^;]*)\)\s*;')
        actual = None
        for m in seq:
            if isinstance(m, str):
                if actual:
                    actual[2].append(m)
                continue
            arg = m.group(m.lastindex)
            op = re.search(r'S_OPCODE_\w+', arg)
            if m.group(m.lastindex - 1) == 'C' and op:
                actual = [op.group(0), f[:-5], []]
                res.append(actual)
            elif actual:
                actual[2].append(TIPOS[m.group(m.lastindex - 1)])
    return [(op, cls, ''.join(fmt)) for op, cls, fmt in res]


def cliente(raiz):
    ph = open(os.path.join(raiz, 'PacketHandler.java'), encoding='utf-8', errors='replace').read()
    clases = {}
    for m in re.finditer(r'case\s+(C_OPCODE_\w+)\s*:\s*new\s+(C_\w+)', ph):
        clases.setdefault(m.group(2), []).append(m.group(1))
    res = []
    d = _dir(raiz, 'clientpackets', 'client')
    for cls, ops in sorted(clases.items()):
        ruta = os.path.join(d, cls + '.java')
        if not os.path.exists(ruta):
            continue
        src = open(ruta, encoding='utf-8', errors='replace').read()
        seq = _secuencia(src, r'\bread(CH|Byte|[CHDSF])\s*\(\s*\)')
        fmt = ''.join(x if isinstance(x, str) else TIPOS[x.group(x.lastindex)] for x in seq)
        for op in ops:
            res.append((op, cls, 'c' + fmt))
    return res


if __name__ == '__main__':
    raiz = sys.argv[1]
    print('# servidor -> cliente: opcode L1J, clase, formato (c del opcode incluido)')
    for op, cls, fmt in servidor(raiz):
        print('S %-34s %-26s c%s' % (op, cls, fmt))
    print('# cliente -> servidor')
    for op, cls, fmt in cliente(raiz):
        print('C %-34s %-26s %s' % (op, cls, fmt))
