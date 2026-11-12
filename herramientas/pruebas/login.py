#!/usr/bin/env python3
"""Prueba en vivo: login (entregables 1 y 2, servidores en el login).

    herramientas/pruebas/login.py <settings.ini de prueba> [carpeta de capturas]

El menú de servidores se abre y se cierra con Esc; una clave mala vuelve con su motivo; la buena
lleva a la selección de personajes y de ahí al juego con Charla.
"""
from comun import ErrorControl, Fallo, Prueba, comprobar, correr, estado


def prueba():
    with Prueba() as p:
        c = p.cliente()
        e = estado(c)
        comprobar(e["pantalla"] == "login", "no arrancó en el login")
        # El desplegable: en la UI simple, la fila "Server"; en la original, arriba del recuadro.
        c("clic 350 243" if e["ui"] == "simple" else "clic 540 287")
        c("esperar ventana=menu-servidores plazo=2000")
        p.captura(c, "login_menu")
        c("tecla esc")
        c("esperar frames=5")
        comprobar("menu-servidores" not in c("ventanas")[0], "Esc no cerró el menú")
        comprobar(estado(c)["pantalla"] == "login", "Esc con el menú abierto salió del login")
        try:
            c("login prueba2 claveequivocada")
            raise Fallo("una clave mala entró")
        except ErrorControl as motivo:
            print("clave mala:", motivo)
        c("login prueba2 prueba2")
        comprobar(estado(c)["pantalla"] == "personajes", "no llegó a la selección de personajes")
        p.captura(c, "login_personajes")
        c("personaje Charla")
        e = estado(c)
        comprobar(e["pantalla"] == "juego" and e["personaje"] == "Charla", "no entró con Charla")
        p.captura(c, "login_juego")


correr(prueba)
