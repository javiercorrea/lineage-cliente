#!/usr/bin/env python3
"""Genera src/recursos/fuente_ui_datos.inc: Departure Mono (SIL OFL 1.1, licencias/DepartureMono-OFL.txt)
recortada a ASCII, Latin-1 y puntuación. Es la fuente de la interfaz (ver recursos/fuente_ttf.cpp).

El release trae OTF (contornos CFF), y stb_truetype solo lee TrueType: por eso se convierte antes.

Requiere fonttools (solo para generar, no para compilar):  pip install fonttools
Uso: python3 tools/generar_fuente_ui.py
"""
import os
import subprocess
import sys
import tempfile

from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont, newTable

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGEN = os.path.join(RAIZ, "datos", "fuentes", "DepartureMono-Regular.otf")
SALIDA = os.path.join(RAIZ, "src", "recursos", "fuente_ui_datos.inc")
# Lo que usan los cinco idiomas: ASCII, Latin-1 (á é ç ã ü ß ñ…) y comillas, guion largo, euro.
RANGOS = "U+0020-007E,U+00A0-00FF,U+2013-2014,U+2018-201E,U+2026,U+20AC"


def otf_a_ttf(ruta_otf, ruta_ttf):
    """OTF con contornos cúbicos -> TTF con contornos cuadráticos (mismo diseño, tolerancia de 1 unidad)."""
    font = TTFont(ruta_otf)
    orden = font.getGlyphOrder()
    glyphset = font.getGlyphSet()
    glyf = newTable("glyf")
    glyf.glyphOrder = orden
    glyf.glyphs = {}
    for nombre in orden:
        pen = TTGlyphPen(glyphset)
        glyphset[nombre].draw(Cu2QuPen(pen, max_err=1.0, reverse_direction=True))
        glyf.glyphs[nombre] = pen.glyph()
    font["glyf"] = glyf
    font["loca"] = newTable("loca")
    del font["CFF "]
    if "VORG" in font:
        del font["VORG"]
    maxp = font["maxp"]
    maxp.tableVersion = 0x00010000
    for atributo in ("maxZones", "maxTwilightPoints", "maxStorage", "maxFunctionDefs",
                     "maxInstructionDefs", "maxStackElements", "maxSizeOfInstructions"):
        setattr(maxp, atributo, 0)
    maxp.maxZones = 1
    font["post"].formatType = 3.0  # sin nombres de glifos: basta con el cmap
    font.sfntVersion = "\x00\x01\x00\x00"
    font["head"].glyphDataFormat = 0
    font.save(ruta_ttf)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        ttf = os.path.join(tmp, "completa.ttf")
        recortada = os.path.join(tmp, "recortada.ttf")
        otf_a_ttf(ORIGEN, ttf)
        subprocess.run([sys.executable, "-m", "fontTools.subset", ttf, "--unicodes=" + RANGOS,
                        "--layout-features=", "--no-hinting", "--output-file=" + recortada], check=True)
        with open(recortada, "rb") as f:
            datos = f.read()

    lineas = [
        "// Generado por tools/generar_fuente_ui.py desde datos/fuentes/DepartureMono-Regular.otf (SIL OFL 1.1).",
        "// No editar a mano.",
        f"constexpr unsigned int FUENTE_UI_TAM = {len(datos)};",
        "constexpr unsigned char FUENTE_UI_DATOS[] = {",
    ]
    for i in range(0, len(datos), 16):
        lineas.append("    " + ", ".join(f"0x{b:02x}" for b in datos[i:i + 16]) + ",")
    lineas.append("};")
    with open(SALIDA, "w", encoding="utf-8") as f:
        f.write("\n".join(lineas) + "\n")
    print(f"{SALIDA}: {len(datos)} bytes")


if __name__ == "__main__":
    main()
