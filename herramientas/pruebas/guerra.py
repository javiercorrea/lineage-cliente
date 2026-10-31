#!/usr/bin/env python3
"""Prueba en vivo con dos clientes: guerra de clanes. MODIFICA EL SERVIDOR (y lo reinicia al final).

    herramientas/pruebas/guerra.py <settings.ini de prueba> [carpeta de capturas]

Prueba (príncipe) recibe permisos de GM por un rato, sube a nivel 30, deja Prueberos y funda
"Rivales"; le declara la guerra a Prueberos (Charla, la líder, la acepta) y después se rinde (Charla también acepta). Al final se
detiene el servidor, se restauran Prueba, Charla y los clanes (Rivales se borra) y se lo arranca.
"""
import servidor
from comun import Prueba, correr


def responder_preguntas(c):
    """Sí a las preguntas que vengan (dejar el clan pregunta dos veces)."""
    while True:
        try:
            c("esperar ventana=pregunta plazo=2000")
        except Exception:
            return
        c("responder si")
        c("esperar frames=5")


def prueba():
    respaldo = servidor.respaldar(["Charla", "Prueba"])
    servidor.sql("UPDATE characters SET access_level = 200 WHERE name = 'Prueba'")  # para .level y .adena
    try:
        with Prueba() as p:
            prueba_ = p.entrar("Prueba", volver=False)  # se restaura con la base
            prueba_('decir ".level 30"')
            prueba_('decir ".adena 30000"')  # fundar un clan cuesta 30000
            prueba_("esperar ms=1000")
            prueba_('decir "/leaveclan"')
            responder_preguntas(prueba_)
            prueba_('decir "/clan Rivales"')
            prueba_('esperar chat=Rivales plazo=5000')
            charla = p.entrar("Charla", volver=False)
            prueba_('decir "/war Prueberos"')
            # Sin castillo de por medio, el líder del otro clan tiene que aceptar la guerra.
            charla("esperar ventana=pregunta")
            p.captura(charla, "guerra_pregunta")
            charla("responder si")
            charla('esperar chat="declared war" plazo=5000')
            prueba_('esperar chat="declared war" plazo=5000')
            p.captura(prueba_, "guerra_declarada")
            p.captura(charla, "guerra_aviso")
            prueba_('decir "/surrender Prueberos"')
            charla("esperar ventana=pregunta")  # la rendición también se acepta
            charla("responder si")
            charla('esperar chat=surrendered plazo=5000')
            p.captura(charla, "guerra_rendicion")
    finally:
        print("restaurando el servidor...")
        servidor.detener()
        servidor.sql(respaldo)
        servidor.arrancar()


correr(prueba)
