"""Genera datos/formas.txt (nivel<TAB>nombre, ordenado por nivel) desde las formas del servidor.

Uso: python3 herramientas/generar_formas.py [polymorphs.csv]. Por defecto lee la tabla de polymorph del
servidor local (servidor_local/l1j-jp/db/csv/ja/polymorphs.csv, la que carga instalar.sh). Los nombres
son los que el servidor busca al elegir la forma, tal cual (en minúsculas, como en su tabla).
"""
import csv
import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGEN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "servidor_local/l1j-jp/db/csv/ja/polymorphs.csv")
DESTINO = os.path.join(RAIZ, "datos/formas.txt")

with open(ORIGEN, encoding="utf-8", errors="replace", newline="") as f:
    filas = [(int(r["min_level"]), r["name"].strip()) for r in csv.DictReader(f) if r["name"].strip()]
filas.sort()
with open(DESTINO, "w", encoding="utf-8", newline="\n") as f:
    for nivel, nombre in filas:
        f.write(f"{nivel}\t{nombre}\n")
print(f"{len(filas)} formas -> {DESTINO}")
