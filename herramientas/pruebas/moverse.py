#!/usr/bin/env python3
"""Prueba en vivo: caminar (entregable 3) con el botón apretado a la derecha del personaje.

    herramientas/pruebas/moverse.py <settings.ini de prueba> [carpeta de capturas]

Charla camina unos pasos en Talking Island y después vuelve a su lugar (.move).
"""
from comun import Prueba, comprobar, correr, posicion, yo


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla")
        mapa, x, y = posicion(c)
        px, py = (int(v) for v in yo(c)["pantalla"].split(","))
        c("mantener %d %d 180" % (px + 120, py))  # unos 3 segundos hacia el puntero
        c("esperar frames=200")
        _, x2, y2 = posicion(c)
        print("de %d,%d a %d,%d" % (x, y, x2, y2))
        comprobar((x2, y2) != (x, y), "no se movió")
        p.captura(c, "moverse")


correr(prueba)
