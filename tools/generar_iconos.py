#!/usr/bin/env python3
"""Íconos propios del cliente: una L cursiva roja sobre fondo transparente (el juego) y la misma L con
un engranaje chico abajo a la derecha (settings).

Uso: tools/generar_iconos.py [--icns CARPETA]
  - escribe datos/iconos/lineage.png y datos/iconos/settings.png (1024 × 1024, RGBA);
  - con --icns (solo macOS: sips e iconutil) arma además lineage.icns y settings.icns en CARPETA, para
    el .app del empaquetado.

Sin dependencias: dibuja con muestreo 2 × 2 por píxel y escribe el PNG con zlib.
"""
import math
import os
import struct
import subprocess
import sys
import tempfile
import zlib

TAM = 1024
MUESTRAS = 2  # por lado y por píxel (antialiasing)
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ROJO = (212, 32, 42)
ROJO_BORDE = (90, 8, 12)
ACERO = (200, 204, 212)
ACERO_BORDE = (42, 45, 51)

# La L, en coordenadas de 0 a 1 (y hacia abajo), antes de inclinarla: palo y pie.
PALO = (0.28, 0.12, 0.45, 0.86)  # x0, y0, x1, y1
PIE = (0.28, 0.70, 0.76, 0.86)
INCLINACION = 0.24  # cuánto se corre a la derecha la parte de arriba (cursiva)
CORRIMIENTO = -0.07  # para centrar la L inclinada
BORDE = 0.018

# Engranaje de settings: centro, radio de la punta de los dientes, de la base y del agujero.
ENG_C = (0.79, 0.79)
ENG_R, ENG_BASE, ENG_AGUJERO, DIENTES = 0.165, 0.128, 0.054, 8
ENG_HUECO = 0.03  # separación transparente entre el engranaje y la L


def en_l(x, y, d=0.0):
    """¿(x, y) está en la L (agrandada d, para el borde)? Se deshace la inclinación y se mira el rectángulo."""
    xr = x - CORRIMIENTO - INCLINACION * (PALO[3] - y)
    for x0, y0, x1, y1 in (PALO, PIE):
        if x0 - d <= xr <= x1 + d and y0 - d <= y <= y1 + d:
            return True
    return False


def radio_engranaje(x, y):
    """Distancia al centro del engranaje y radio del contorno en ese ángulo (dientes trapezoidales)."""
    dx, dy = x - ENG_C[0], y - ENG_C[1]
    r = math.hypot(dx, dy)
    fase = (math.atan2(dy, dx) / (2 * math.pi) * DIENTES) % 1.0
    # Diente en el centro de cada período: plano arriba, flancos inclinados.
    t = min(abs(fase - 0.5) / 0.5, 1.0)  # 0 en el centro del diente, 1 en el centro del hueco
    if t < 0.32:
        borde = ENG_R
    elif t < 0.55:
        borde = ENG_R - (ENG_R - ENG_BASE) * (t - 0.32) / 0.23
    else:
        borde = ENG_BASE
    return r, borde


def color_en(x, y, engranaje):
    """RGBA (0–255) de una muestra."""
    if engranaje:
        r, borde = radio_engranaje(x, y)
        if ENG_AGUJERO + BORDE < r < borde - BORDE:
            return ACERO + (255,)
        if ENG_AGUJERO <= r <= borde:
            return ACERO_BORDE + (255,)
        if r < borde + ENG_HUECO:
            return (0, 0, 0, 0)  # el agujero y el hueco que lo separa de la L
    if en_l(x, y):
        return ROJO + (255,) if en_l(x, y, -BORDE) else ROJO_BORDE + (255,)
    return (0, 0, 0, 0)


def dibujar(engranaje):
    filas = []
    paso = 1.0 / (TAM * MUESTRAS)
    for py in range(TAM):
        fila = bytearray([0])  # filtro PNG "ninguno"
        for px in range(TAM):
            acc = [0, 0, 0, 0]
            for sy in range(MUESTRAS):
                y = (py * MUESTRAS + sy + 0.5) * paso
                for sx in range(MUESTRAS):
                    x = (px * MUESTRAS + sx + 0.5) * paso
                    r, g, b, a = color_en(x, y, engranaje)
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            if acc[3]:
                fila += bytes((acc[0] // acc[3], acc[1] // acc[3], acc[2] // acc[3], acc[3] // (MUESTRAS * MUESTRAS)))
            else:
                fila += b"\0\0\0\0"
        filas.append(bytes(fila))
    return filas


def png(filas):
    def bloque(tipo, datos):
        return struct.pack(">I", len(datos)) + tipo + datos + struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF)
    cabecera = struct.pack(">IIBBBBB", TAM, TAM, 8, 6, 0, 0, 0)  # 8 bits, RGBA
    return b"\x89PNG\r\n\x1a\n" + bloque(b"IHDR", cabecera) + bloque(b"IDAT", zlib.compress(b"".join(filas), 9)) + bloque(b"IEND", b"")


def pixel(filas, x, y):
    i = 1 + 4 * x
    return tuple(filas[y][i:i + 4])


def comprobar(filas, engranaje):
    """Que el dibujo sea el pedido: esquinas transparentes, palo y pie rojos, engranaje gris con agujero."""
    assert pixel(filas, 0, 0)[3] == 0 and pixel(filas, TAM - 1, 0)[3] == 0, "las esquinas deben ser transparentes"
    palo = int((0.365 + CORRIMIENTO + INCLINACION * (PALO[3] - 0.4)) * TAM), int(0.4 * TAM)
    assert pixel(filas, *palo) == ROJO + (255,), ("el palo debe ser rojo", pixel(filas, *palo))
    pie = int((0.40 + CORRIMIENTO + INCLINACION * (PALO[3] - 0.78)) * TAM), int(0.78 * TAM)
    assert pixel(filas, *pie) == ROJO + (255,), ("el pie debe ser rojo", pixel(filas, *pie))
    centro = int(ENG_C[0] * TAM), int(ENG_C[1] * TAM)
    if engranaje:
        assert pixel(filas, *centro)[3] == 0, "el agujero del engranaje debe ser transparente"
        anillo = int((ENG_C[0] + (ENG_AGUJERO + ENG_BASE) / 2) * TAM), centro[1]
        assert pixel(filas, *anillo) == ACERO + (255,), ("el engranaje debe ser gris", pixel(filas, *anillo))
    else:
        assert pixel(filas, *centro)[3] == 0, "sin engranaje, esa zona queda transparente"


def icns(png_1024, destino):
    """macOS: .iconset con los tamaños que pide iconutil, desde el PNG de 1024."""
    with tempfile.TemporaryDirectory() as tmp:
        conjunto = os.path.join(tmp, "icono.iconset")
        os.mkdir(conjunto)
        for lado in (16, 32, 128, 256, 512):
            for escala in (1, 2):
                nombre = "icon_%dx%d%s.png" % (lado, lado, "@2x" if escala == 2 else "")
                subprocess.run(["sips", "-z", str(lado * escala), str(lado * escala), png_1024, "--out",
                                os.path.join(conjunto, nombre)], check=True, capture_output=True)
        subprocess.run(["iconutil", "-c", "icns", conjunto, "-o", destino], check=True)


def main():
    carpeta = os.path.join(RAIZ, "datos", "iconos")
    os.makedirs(carpeta, exist_ok=True)
    destino_icns = sys.argv[sys.argv.index("--icns") + 1] if "--icns" in sys.argv else None
    for nombre, engranaje in (("lineage", False), ("settings", True)):
        filas = dibujar(engranaje)
        comprobar(filas, engranaje)
        ruta = os.path.join(carpeta, nombre + ".png")
        with open(ruta, "wb") as f:
            f.write(png(filas))
        print(ruta)
        if destino_icns:
            os.makedirs(destino_icns, exist_ok=True)
            icns(ruta, os.path.join(destino_icns, nombre + ".icns"))
            print(os.path.join(destino_icns, nombre + ".icns"))


if __name__ == "__main__":
    main()
