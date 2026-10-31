#!/usr/bin/env python3
"""Prueba en vivo: inventario y equipar (entregable 6).

    herramientas/pruebas/inventario.py <settings.ini de prueba> [carpeta de capturas]

Charla se saca el arco y se lo vuelve a poner (y si empieza sin él, se lo pone primero).
"""
from comun import Prueba, comprobar, correr


def arco(c):
    for l in c("inventario"):
        if "Ivory Tower Bow" in l:
            return l
    return ""


def cambiar_arco(c):
    # El servidor ignora un cambio de equipo hecho justo después de otro: un respiro antes.
    c("esperar ms=1500")
    c('usar "Ivory Tower Bow"')
    c("esperar paquete=CHARVISUALUPDATE plazo=5000")
    c("esperar frames=5")


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla", volver=False)
        if "equipado" not in arco(c):  # una corrida anterior lo dejó sin el arco
            cambiar_arco(c)
        comprobar("equipado" in arco(c), "Charla no puede equiparse el arco")
        c("abrir inventario")
        c("esperar ventana=inventario")
        p.captura(c, "inventario")
        cambiar_arco(c)
        comprobar("equipado" not in arco(c), "no se sacó el arco")
        cambiar_arco(c)
        comprobar("equipado" in arco(c), "no se volvió a poner el arco")


correr(prueba)
