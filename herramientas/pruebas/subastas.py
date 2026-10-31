#!/usr/bin/env python3
"""Prueba en vivo: tablón de subastas de Giran (lista, una casa y su mapa).

    herramientas/pruebas/subastas.py <settings.ini de prueba> [carpeta de capturas]

Usa a Charla (cuenta prueba2, GM) contra el servidor local; la lleva al tablón con .move y la
devuelve adonde estaba. El settings.ini tiene que tener "juego=" y el servidor local elegido.
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lin_ctl import Control  # noqa: E402


def main():
    config = sys.argv[1]
    capturas = sys.argv[2] if len(sys.argv) > 2 else "."
    c = Control.arrancar(config=config)
    try:
        c("mudo on")
        c("login prueba2 prueba2")
        c("personaje Charla")
        antes = re.search(r"mapa=(\d+) x=(\d+) y=(\d+)", c("estado")[1]).groups()
        c('decir ".move 33421 32826 4"')
        c("esperar paquete=MAPID")
        c("esperar frames=30")
        c("hablar Auction")
        c("esperar ventana=subastas")
        casas = c("objetos 3")
        print("cerca del tablón:", len(casas), "objetos")
        c("clic 300 101")  # la primera casa de la lista
        c("esperar html=agsel")
        print("\n".join(c("html")))
        c("accion map")
        c("esperar ventana=mapa-casa")
        c("captura %s/subastas_mapa.bmp" % capturas)
        c("tecla esc")
        mapa, x, y = antes
        c('decir ".move %s %s %s"' % (x, y, mapa))
        c("esperar paquete=MAPID")
        print("de vuelta:", c("estado")[1])
    finally:
        c.cerrar()


if __name__ == "__main__":
    main()
