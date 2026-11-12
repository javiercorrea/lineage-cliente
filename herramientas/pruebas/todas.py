#!/usr/bin/env python3
"""Corre las pruebas en vivo, una tras otra, y resume cuáles pasan.

    herramientas/pruebas/todas.py <settings.ini de prueba> [carpeta de capturas] [--servidor]

Sin --servidor, solo las que no dejan cambios (personajes en su lugar, nada comprado ni gastado). Con
--servidor, también las que modifican y reinician el servidor local (casas, castillos, guerra): las
restauran solas, pero tardan varios minutos y cortan a cualquier otro cliente conectado.
"""
import os
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
SIN_CAMBIOS = ["login", "moverse", "chat_estado", "combate_hechizos", "inventario", "equipo", "npc",
               "tablon_correo", "subastas", "social"]
CON_SERVIDOR = ["casas", "castillos", "guerra"]


def main():
    args = [a for a in sys.argv[1:] if a != "--servidor"]
    if not args:
        print(__doc__)
        return 2
    pruebas = SIN_CAMBIOS + (CON_SERVIDOR if "--servidor" in sys.argv else [])
    resultados = []
    for nombre in pruebas:
        t0 = time.time()
        r = subprocess.run([sys.executable, os.path.join(AQUI, nombre + ".py")] + args, capture_output=True, text=True)
        ultima = (r.stdout.strip().splitlines() or ["(sin salida)"])[-1]
        resultados.append((nombre, r.returncode == 0, time.time() - t0, ultima))
        print("%-18s %s  %5.0f s  %s" % (nombre, "bien " if r.returncode == 0 else "FALLA", time.time() - t0, ultima), flush=True)
        if r.returncode != 0 and r.stderr.strip():
            print(r.stderr.strip()[-2000:])
    fallas = [n for n, ok, _, _ in resultados if not ok]
    print("\n%d de %d bien%s" % (len(resultados) - len(fallas), len(resultados), (": fallan " + ", ".join(fallas)) if fallas else ""))
    return 1 if fallas else 0


if __name__ == "__main__":
    sys.exit(main())
