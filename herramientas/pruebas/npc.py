#!/usr/bin/env python3
"""Prueba en vivo: hablar con un NPC y abrir el almacén (entregable 7).

    herramientas/pruebas/npc.py <settings.ini de prueba> [carpeta de capturas]

Charla habla con Storage^Dorin (Talking Island), mira su página y abre la lista para guardar (la arma
el cliente con el inventario; "retrieve" depende de lo que haya en el almacén), sin guardar nada.
"""
from comun import Prueba, comprobar, correr


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla")
        c("hablar Dorin")
        c("esperar ventana=npc")
        pagina = c("html")
        print("\n".join(pagina))
        p.captura(c, "npc_pagina")
        acciones = [l.split()[0].split("=", 1)[1] for l in pagina[1:]]
        comprobar("deposit" in acciones, "la página del almacén no tiene 'deposit'")
        c("accion deposit")
        c("esperar ventana=tienda")
        p.captura(c, "npc_almacen")
        c("tecla esc")


correr(prueba)
