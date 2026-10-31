#!/usr/bin/env python3
"""Prueba en vivo: chat, ventana de estado y F3 (entregable 4).

    herramientas/pruebas/chat_estado.py <settings.ini de prueba> [carpeta de capturas]
"""
import time

from comun import Prueba, comprobar, correr, estado


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla", volver=False)
        texto = "prueba de chat %d" % (time.time() % 10000)
        c('decir "%s"' % texto)
        c('esperar chat="%s"' % texto)  # el eco del servidor
        c("abrir estado")
        c("esperar ventana=estado")
        c("depuracion on")
        c("esperar frames=10")
        e = estado(c)
        comprobar(int(e["nivel"]) > 0 and "/" in e["hp"], "el estado no trae nivel y vida")
        p.captura(c, "chat_estado")


correr(prueba)
