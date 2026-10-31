#!/usr/bin/env python3
"""Prueba en vivo: la pestaña Equipment de la ventana de estado.

    herramientas/pruebas/equipo.py <settings.ini de prueba> [carpeta de capturas]
"""
from comun import Prueba, correr, ventana


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla", volver=False)
        c("abrir estado")
        c("esperar ventana=estado")
        x, y, w, h = ventana(c, "estado")
        c("clic %d %d" % (x + 128 + 48, y + h - 24 + 9))  # la pestaña "Equipment"
        c("esperar frames=5")
        p.captura(c, "equipo")


correr(prueba)
