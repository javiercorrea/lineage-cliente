#!/usr/bin/env python3
"""Prueba en vivo: casas de clan (subasta, mapa, puja, mayordomo). MODIFICA EL SERVIDOR.

    herramientas/pruebas/casas.py <settings.ini de prueba> [carpeta de capturas]

Charla (GM, líder de Prueberos) recibe 100.000 de adena (.adena), puja por la casa 1 de Giran, la
subasta se cierra con servidor_local/pruebas/subasta_cerrar.sql y Prueberos gana la casa. Con el
mayordomo (Emma) le pone nombre, abre la puerta y mira el impuesto. Al final se detiene el servidor,
se restauran casa, clanes y a Charla (adena incluida) y se lo vuelve a arrancar.
"""
import time

import servidor
from comun import Fallo, Prueba, comprobar, correr, estado, mover

CASA = 262145  # Giran house 1: la primera de la lista de Giran
TABLON_GIRAN = (4, 33421, 32826)
MAYORDOMO = (4, 33373, 32658)  # a 2 celdas de Emma, frente a la puerta


def prueba():
    respaldo = servidor.respaldar(["Charla"])
    charla_id = servidor.sql("SELECT id FROM characters WHERE name = 'Charla'").strip()
    try:
        with Prueba() as p:
            c = p.entrar("Charla")
            c('decir ".adena 100000"')
            mover(c, *TABLON_GIRAN)
            c('hablar "Auction Board"')
            c("esperar ventana=subastas")
            p.captura(c, "casas_subastas")
            x0 = (int(estado(c)["vista"].split("x")[0]) - 420) // 2
            c("clic %d %d" % (x0 + 90, 50 + 44 + 7))  # la primera casa
            c("esperar html=agsel")
            c("accion map")
            c("esperar ventana=mapa-casa")
            p.captura(c, "casas_mapa")
            c("tecla esc")
            c("accion apply")
            c("esperar ventana=cantidad")
            c("cantidad 100000")
            c("esperar ms=1000")
            postor = servidor.sql("SELECT bidder_id FROM auction_houses WHERE house_id = %d" % CASA).strip()
            comprobar(postor == charla_id, "la puja no quedó registrada (postor %s)" % postor)
            # Cerrar la subasta: AuctionTimeController la revisa cada minuto.
            servidor.sql_archivo("subasta_cerrar.sql")
            for _ in range(90):
                if servidor.sql("SELECT house_id FROM clans WHERE name = 'Prueberos'").strip() == str(CASA):
                    break
                time.sleep(2)
            else:
                raise Fallo("la subasta no se cerró a favor de Prueberos")
            # El mayordomo.
            mover(c, *MAYORDOMO)
            c("hablar Emma")
            c("esperar html=agit")
            c("accion name")
            c("esperar ventana=pregunta")
            c('responder "Casa Prueba"')
            c("esperar ms=1000")
            nombre = servidor.sql("SELECT name FROM houses WHERE id = %d" % CASA).strip()
            comprobar(nombre == "Casa Prueba", "la casa no quedó con el nombre (%s)" % nombre)
            c("hablar Emma")
            c("esperar html=agit")
            c("accion open")
            c("esperar paquete=DOOR plazo=5000")
            c("esperar frames=20")
            p.captura(c, "casas_puerta")
            c("hablar Emma")
            c("esperar html=agit")
            c("accion pay")
            c("esperar html=agpay")
            p.captura(c, "casas_impuesto")
    finally:
        print("restaurando el servidor...")
        servidor.detener()
        servidor.sql_archivo("restaurar.sql")
        servidor.sql(respaldo)
        servidor.arrancar()


correr(prueba)
