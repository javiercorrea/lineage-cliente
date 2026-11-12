#!/usr/bin/env python3
"""Prueba en vivo: el tablón de Talking Island (escribir, leer y borrar un tema) y el correo.

    herramientas/pruebas/tablon_correo.py <settings.ini de prueba> [carpeta de capturas]

Escribir cuesta 300 adena: al final se le devuelven a Charla con ".adena 300" (es GM).
"""
import time

from comun import Prueba, comprobar, correr, estado

ANCHO, ALTO, Y0 = 420, 330, 50  # la ventana del tablón (tablon_ui.cpp)


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla")
        c('hablar "Message Board"')
        c("esperar ventana=tablon")
        x0 = (int(estado(c)["vista"].split("x")[0]) - ANCHO) // 2
        botones_y = Y0 + ALTO - 26 + 10
        # Escribir: "Write", el título, Tab, el texto y "Post".
        titulo = "Prueba %d" % (time.time() % 10000)
        c("clic %d %d" % (x0 + ANCHO - 170 + 38, botones_y))
        c("esperar frames=3")
        c('texto "%s"' % titulo)
        c("tecla tab")
        c('texto "Tema de la prueba en vivo"')
        c("esperar frames=5")
        p.captura(c, "tablon_escribir")
        c("clic %d %d" % (x0 + ANCHO - 170 + 38, botones_y))
        c("esperar paquete=BOARD plazo=5000")
        c("esperar frames=5")
        p.captura(c, "tablon")
        # El primero de la lista es el nuevo: leerlo y borrarlo.
        c("clic %d %d" % (x0 + 60, Y0 + 30 + 8))
        c("esperar paquete=BOARDREAD plazo=5000")
        c("esperar frames=5")
        p.captura(c, "tablon_tema")
        c("clic %d %d" % (x0 + 10 + 38, botones_y))  # "Delete" (el autor)
        c("esperar paquete=BOARD plazo=5000")
        c('decir ".adena 300"')
        c("tecla esc")
        c("esperar frames=5")
        comprobar("tablon" not in c("ventanas")[0], "Esc no cerró el tablón")
        c('decir "/mail"')
        c("esperar ventana=correo")
        p.captura(c, "correo")
        c("tecla esc")


correr(prueba)
