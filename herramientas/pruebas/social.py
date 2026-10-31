#!/usr/bin/env python3
"""Prueba en vivo con dos clientes: grupo e intercambio (entregable 9).

    herramientas/pruebas/social.py <settings.ini de prueba> [carpeta de capturas]

Charla (GM) va al lado de Prueba (mapa 2005), la invita al grupo y Prueba acepta; Prueba se va del
grupo. Después Prueba le pide un intercambio, Charla acepta y Prueba lo cancela (no se da nada).
Charla vuelve a su lugar.
"""
from comun import Prueba, comprobar, correr, mover, posicion


def prueba():
    with Prueba() as p:
        prueba_ = p.entrar("Prueba", volver=False)  # no es GM: no se mueve
        charla = p.entrar("Charla")
        mapa, x, y = posicion(prueba_)
        mover(charla, mapa, x + 1, y)  # al este de Prueba
        charla("girar 6")  # mirándose: Prueba al este (2), Charla al oeste (6)
        prueba_("girar 2")
        charla("esperar frames=30")
        # Grupo.
        charla('decir "/invite Prueba"')
        prueba_("esperar ventana=pregunta")
        prueba_("responder si")
        charla("esperar paquete=PACKETBOX plazo=5000")
        charla("esperar frames=20")
        miembros = charla("grupo")
        print("grupo de Charla:", miembros)
        comprobar(any("Prueba" in m for m in miembros), "Prueba no aparece en el grupo de Charla")
        p.captura(charla, "social_grupo")
        prueba_('decir "/leave"')
        for _ in range(20):  # de a dos, el grupo se disuelve: la lista queda vacía
            charla("esperar ms=250")
            if not charla("grupo"):
                break
        comprobar(not charla("grupo"), "el grupo no se disolvió")
        # Intercambio, que se cancela.
        prueba_('decir "/trade"')
        charla("esperar ventana=pregunta")
        charla("responder si")
        charla("esperar ventana=intercambio")
        prueba_("esperar ventana=intercambio")
        p.captura(charla, "social_intercambio")
        prueba_("cancelar-intercambio")
        charla("esperar paquete=TRADESTATUS plazo=5000")
        charla("esperar frames=5")
        comprobar("intercambio" not in charla("ventanas")[0], "el intercambio no se cerró")


correr(prueba)
