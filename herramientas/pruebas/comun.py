"""Lo común de las pruebas en vivo (herramientas/pruebas/*.py).

Cada prueba se corre así:
    herramientas/pruebas/<prueba>.py <settings.ini de prueba> [carpeta de capturas]

El settings.ini tiene que tener "juego=" y el servidor local elegido (o ningún "servidor=": se usa
127.0.0.1:2000 jp363). Nunca el settings.ini del usuario. Las pruebas usan las cuentas de prueba
(prueba / prueba2) contra el servidor local, dejan a los personajes donde estaban y cierran los
clientes aunque fallen.
"""
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lin_ctl import Control, ErrorControl  # noqa: E402,F401

CUENTAS = {"Charla": "prueba2", "Aprendiz": "prueba2", "Prueba": "prueba", "Cloud": "prueba"}


class Fallo(Exception):
    pass


def comprobar(condicion, mensaje):
    if not condicion:
        raise Fallo(mensaje)


def campos(linea):
    """'a=1 b="x y" c' -> {'a': '1', 'b': 'x y', 'c': ''}"""
    return {k: (v1 if v1 else v2) for k, v1, v2 in re.findall(r'([\w-]+)(?:="([^"]*)"|=(\S*))?', linea)}


class Prueba:
    """Uno o más clientes; al salir del `with`, devuelve a cada personaje a su lugar y los cierra."""

    def __init__(self):
        if len(sys.argv) < 2:
            print(sys.modules["__main__"].__doc__)
            sys.exit(2)
        self.config = sys.argv[1]
        self.capturas = sys.argv[2] if len(sys.argv) > 2 else "."
        os.makedirs(self.capturas, exist_ok=True)
        self.clientes = []  # (control, personaje, lugar inicial o None)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        for c, nombre, lugar in self.clientes:
            try:
                if lugar and lugar != posicion(c):
                    mover(c, *lugar)
            except (ErrorControl, OSError, Fallo) as e:
                print("aviso: no se pudo devolver a %s a su lugar: %s" % (nombre, e))
            c.cerrar()
        return False

    def cliente(self, config=None):
        """Un cliente en el login (sin entrar). Varios clientes: un settings.ini cada uno."""
        if config is None and self.clientes:
            config = copia_config(self.config, len(self.clientes))
        c = Control.arrancar(config=config or self.config)
        c("mudo on")
        c("esperar pantalla=login")
        self.clientes.append((c, None, None))
        return c

    def entrar(self, personaje, volver=True):
        """Un cliente con ese personaje en el juego. `volver`: al final, de vuelta a donde estaba
        (con .move: solo los personajes GM)."""
        c = self.cliente()
        c("login %s %s" % (CUENTAS[personaje], CUENTAS[personaje]))
        c("personaje " + personaje)
        c("esperar frames=30")
        if c("html"):  # una página que abre el servidor al entrar (RaiseAttr de Charla): se cierra
            c("tecla esc")
            c("esperar frames=3")
        self.clientes[-1] = (c, personaje, posicion(c) if volver else None)
        return c

    def captura(self, c, nombre):
        """Captura <capturas>/<nombre>.png (en macOS; si no, queda el .bmp)."""
        bmp = os.path.join(self.capturas, nombre + ".bmp")
        c("captura " + bmp)
        if shutil.which("sips"):
            png = bmp[:-4] + ".png"
            subprocess.run(["sips", "-s", "format", "png", bmp, "--out", png], capture_output=True)
            os.unlink(bmp)
            return png
        return bmp


def copia_config(config, n):
    """Otro settings.ini para el cliente n (dos clientes no comparten el archivo)."""
    destino = "%s.cliente%d.ini" % (config, n)
    shutil.copyfile(config, destino)
    return destino


def estado(c):
    """Los campos de "estado" en un solo diccionario."""
    d = {}
    for linea in c("estado"):
        d.update(campos(linea))
    return d


def posicion(c):
    e = estado(c)
    return int(e["mapa"]), int(e["x"]), int(e["y"])


def mover(c, mapa, x, y):
    """Teletransporte de GM (.move) y esperar a llegar."""
    c("parar")  # si no, sigue caminando hacia el último objetivo
    c('decir ".move %d %d %d"' % (x, y, mapa))
    c("esperar paquete=MAPID plazo=5000")
    c("esperar frames=20")
    comprobar(posicion(c)[1:] == (x, y), "el .move no llegó a %d, %d" % (x, y))


def objetos(c, radio=20):
    return [campos(l) for l in c("objetos %d" % radio)]


def objeto(c, nombre, radio=20):
    """El primero cuyo nombre contenga `nombre` (la lista viene por slot, no por distancia)."""
    for o in objetos(c, radio):
        if nombre in o.get("nombre", ""):
            return o
    raise Fallo("no hay un \"%s\" a la vista" % nombre)


def yo(c):
    for o in objetos(c, 0):
        if "yo" in o:
            return o
    raise Fallo("el propio personaje no está en la lista de objetos")


def ventana(c, nombre):
    """(x, y, ancho, alto) de una ventana móvil abierta."""
    for l in c("ventanas"):
        if l.startswith(nombre + "="):
            return tuple(int(v) for v in l.split("=", 1)[1].split(","))
    raise Fallo("la ventana %s no está abierta" % nombre)


def correr(funcion):
    """Corre la prueba y sale con 0 (bien) o 1 (falló), con el motivo."""
    try:
        funcion()
    except (Fallo, ErrorControl) as e:
        print("FALLA:", e)
        sys.exit(1)
    print("bien")
