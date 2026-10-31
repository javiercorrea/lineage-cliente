#!/usr/bin/env python3
"""Prueba en vivo: asedio de Kent y el chambelán. MODIFICA Y REINICIA EL SERVIDOR.

    herramientas/pruebas/castillos.py <settings.ini de prueba> [carpeta de capturas]

Se reinicia el servidor con servidor_local/pruebas/asedio_kent.sql (el asedio empieza ahora; torre
y portón con poca vida). Charla (GM, líder de Prueberos) rompe la torre, toma la corona y, ya señora
de Kent, usa al chambelán: impuesto al 20 %, depósito de 1000 y la fecha del próximo asedio. Al final
se detiene el servidor, se restaura todo (restaurar.sql y Charla, flechas y adena incluidas) y se
lo vuelve a arrancar.
"""
import time

import servidor
from comun import Fallo, Prueba, comprobar, correr, estado, mover, objetos

TORRE, CORONA = "#1672", "#1482"
JUNTO_A_LA_TORRE = (4, 33137, 32771)
JUNTO_A_LA_CORONA = (4, 33139, 32769)  # a una celda: la corona se toma de cerca
CHAMBELAN = (15, 32738, 32785)


def hay(c, gfx, vivo=True):
    return any(o.get("gfx") == gfx[1:] and (not vivo or "muerto" not in o) for o in objetos(c))


def prueba():
    respaldo = servidor.respaldar(["Charla"])
    servidor.reiniciar_con("asedio_kent.sql")
    try:
        with Prueba() as p:
            c = p.entrar("Charla", volver=False)  # se restaura con la base
            comprobar("1" in estado(c)["asedios"].split(","), "no hay asedio en Kent")
            mover(c, *JUNTO_A_LA_TORRE)
            p.captura(c, "asedio_torre")
            c("atacar " + TORRE)
            for _ in range(120):  # la torre (300 de vida) cae y aparece la corona
                if hay(c, CORONA):
                    break
                c("esperar ms=1000")
            else:
                raise Fallo("la torre no cayó en 2 minutos")
            c("parar")
            p.captura(c, "asedio_corona")
            mover(c, *JUNTO_A_LA_CORONA)
            c("atacar " + CORONA)
            for _ in range(20):
                if estado(c).get("senor") == "1":
                    break
                c("esperar ms=500")
            else:
                raise Fallo("Charla no quedó como señora de Kent")
            c("parar")
            p.captura(c, "asedio_tomado")
            # El chambelán.
            mover(c, *CHAMBELAN)
            c("hablar Ishmael")
            c("esperar html=ishmael1")
            c("accion tax")
            c("esperar ventana=cantidad")
            p.captura(c, "castillo_impuesto")
            c("cantidad 20")
            c("esperar ms=1000")
            comprobar(servidor.sql("SELECT tax_rate FROM castles WHERE id = 1").strip() == "20", "el impuesto no quedó en 20")
            antes = int(servidor.sql("SELECT public_money FROM castles WHERE id = 1"))
            c("hablar Ishmael")
            c("esperar html=ishmael1")
            c("accion cdeposit")
            c("esperar ventana=cantidad")
            c("cantidad 1000")
            c("esperar ms=1000")
            despues = int(servidor.sql("SELECT public_money FROM castles WHERE id = 1"))
            comprobar(despues == antes + 1000, "el depósito no llegó a los fondos (%d -> %d)" % (antes, despues))
            c("hablar Ishmael")
            c("esperar html=ishmael1")
            c("accion asktime")
            c('esperar chat="Next siege"')
            p.captura(c, "castillo_fecha")
    finally:
        print("restaurando el servidor...")
        servidor.detener()
        servidor.sql_archivo("restaurar.sql")
        servidor.sql(respaldo)
        servidor.arrancar()


correr(prueba)
