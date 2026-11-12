#!/usr/bin/env python3
"""Prueba en vivo: el tablón de subastas de Giran (lista, una casa y su mapa), sin pujar.

    herramientas/pruebas/subastas.py <settings.ini de prueba> [carpeta de capturas]

Charla (GM) va al tablón con .move y vuelve adonde estaba. La puja y la casa ganada: casas.py.
"""
from comun import Prueba, comprobar, correr, estado, mover

TABLON_GIRAN = (4, 33421, 32826)


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla")
        mover(c, *TABLON_GIRAN)
        c('hablar "Auction Board"')
        c("esperar ventana=subastas")
        x0 = (int(estado(c)["vista"].split("x")[0]) - 420) // 2
        c("clic %d %d" % (x0 + 90, 50 + 44 + 7))  # la primera casa
        c("esperar html=agsel")
        acciones = c("html")
        print("\n".join(acciones))
        comprobar(any(l.startswith("accion=map") for l in acciones), "la página de la casa no tiene 'map'")
        c("accion map")
        c("esperar ventana=mapa-casa")
        p.captura(c, "subastas_mapa")
        c("tecla esc")


correr(prueba)
