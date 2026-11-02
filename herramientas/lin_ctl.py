#!/usr/bin/env python3
"""Control en vivo del cliente (solo en las versiones de desarrollo, LIN_CONTROL=ON).

En la terminal:
    lin_ctl.py <socket> estado            un comando
    lin_ctl.py <socket>                   interactivo: una línea por comando ("ayuda" los lista)

Como módulo, para escribir pruebas:
    from lin_ctl import Control
    c = Control.arrancar(config="/tmp/prueba.ini")   # lanza build/debug/lineage
    c("login prueba2 prueba2")
    c("personaje Charla")
    print(c("estado"))
    c("captura /tmp/x.bmp")
    c.cerrar()

Cada comando devuelve las líneas de la respuesta (sin el "ok" final) o lanza ErrorControl con el
motivo. El protocolo está en src/app/control.cpp y docs/analisis/PLAN_CONTROL_VIVO.md.
"""
import os
import socket
import subprocess
import sys
import tempfile
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
BINARIO = os.path.join(AQUI, "..", "build", "debug", "lineage")


class ErrorControl(Exception):
    pass


class Control:
    lanzados = 0

    def __init__(self, ruta, proceso=None):
        self.ruta = ruta
        self.proceso = proceso
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(ruta)
        self.resto = b""

    @classmethod
    def arrancar(cls, config, binario=BINARIO, plazo=30, entorno=None, registro=None):
        """Lanza el cliente con su propio settings.ini y socket, y se conecta. `registro`: archivo
        donde queda su salida (la traza de "traza on", por ejemplo)."""
        cls.lanzados += 1  # varios clientes desde el mismo script: un socket cada uno
        ruta = os.path.join(tempfile.gettempdir(), "lineage-control-py-%d-%d.sock" % (os.getpid(), cls.lanzados))
        env = dict(os.environ, LIN_CONFIG=config, LIN_CONTROL=ruta, **(entorno or {}))
        if os.path.exists(ruta):
            os.unlink(ruta)
        salida = open(registro, "w") if registro else subprocess.DEVNULL
        proceso = subprocess.Popen([binario], env=env, stdout=salida, stderr=subprocess.STDOUT)
        fin = time.time() + plazo
        while not os.path.exists(ruta):
            if proceso.poll() is not None or time.time() > fin:
                raise ErrorControl("el cliente no abrió el socket %s" % ruta)
            time.sleep(0.1)
        return cls(ruta, proceso)

    def __call__(self, linea, plazo=60):
        """Manda un comando y devuelve las líneas de la respuesta."""
        self.sock.settimeout(plazo)
        self.sock.sendall(linea.encode("cp949", errors="replace") + b"\n")  # el cliente habla CP949
        lineas = []
        while True:
            while b"\n" not in self.resto:
                datos = self.sock.recv(4096)
                if not datos:
                    raise ErrorControl("el cliente cerró la conexión")
                self.resto += datos
            l, self.resto = self.resto.split(b"\n", 1)
            l = l.decode("cp949", errors="replace")
            if l == "ok":
                return lineas
            if l.startswith("error"):
                raise ErrorControl("%s (%s)" % (l[6:] or "error", linea))
            lineas.append(l)

    def cerrar(self):
        """Pide salir y espera al proceso (si lo lanzó esta clase)."""
        try:
            self("salir", plazo=5)
        except (ErrorControl, OSError):
            pass
        self.sock.close()
        if self.proceso:
            self.proceso.wait(timeout=15)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    c = Control(sys.argv[1])
    lineas = [" ".join(sys.argv[2:])] if len(sys.argv) > 2 else None
    if lineas is None:
        try:
            while True:
                l = input("> ").strip()
                if not l:
                    continue
                try:
                    for r in c(l):
                        print(r)
                    print("ok")
                except ErrorControl as e:
                    print("error", e)
                if l == "salir":
                    return 0
        except EOFError:
            return 0
    try:
        for r in c(lineas[0]):
            print(r)
    except ErrorControl as e:
        print("error", e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
