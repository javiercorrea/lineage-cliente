#!/usr/bin/env python3
"""Prueba en vivo: combate y hechizos (entregables 5 y 6.5).

    herramientas/pruebas/combate_hechizos.py <settings.ini de prueba> [carpeta de capturas]

Aprendiz (maga, GM: no muere) invoca un Jack-O-Lantern (.spawn 45166) a su lado y lo mata con
Energy Bolt; después vuelve a su lugar.
"""
from comun import Fallo, Prueba, comprobar, correr, objetos


def prueba():
    with Prueba() as p:
        c = p.entrar("Aprendiz")
        antes = {o["id"] for o in objetos(c)}
        c('decir ".spawn 45166 1 0"')
        c("esperar paquete=CHARPACK plazo=5000")
        c("esperar frames=10")
        nuevos = [o for o in objetos(c) if o["id"] not in antes and o.get("tipo") == "10"]
        comprobar(nuevos, "no apareció el monstruo")
        objetivo = nuevos[0]["id"]
        print("objetivo:", nuevos[0]["nombre"], objetivo)
        for intento in range(30):
            vivo = [o for o in objetos(c) if o["id"] == objetivo and "muerto" not in o]
            if not vivo:
                break
            try:
                c("lanzar \"Energy Bolt\" " + objetivo)
            except Exception as e:  # la espera entre hechizos: se reintenta
                print("lanzar:", e)
            c("esperar frames=120")
        else:
            raise Fallo("el monstruo no murió en 30 hechizos")
        print("muerto en", intento, "hechizos")
        p.captura(c, "combate_hechizos")


correr(prueba)
