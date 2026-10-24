// Decodificación de imágenes del juego a RGBA8. Las imágenes decodificadas son transitorias:
// se suben al atlas de la GPU y el buffer se reutiliza para la siguiente.
#pragma once
#include <vector>
#include "core/tipos.h"

namespace lin {

struct ImagenRGBA {
    u16 ancho = 0, alto = 0;
    i16 ox = 0, oy = 0;      // desplazamiento de dibujo respecto del ancla
    std::vector<u32> px;     // 0xAABBGGRR (bytes R,G,B,A en memoria)

    void preparar(u16 w, u16 h) {
        ancho = w;
        alto = h;
        ox = oy = 0;
        px.assign(static_cast<size_t>(w) * h, 0);
    }
    bool vacia() const { return ancho == 0 || alto == 0; }
};

// RGB555 del juego (0RRRRRGGGGGBBBBB) a RGBA8 opaco.
inline u32 rgb555(u16 c) {
    u32 r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return r | (g << 8) | (b << 16) | 0xFF000000u;
}

// .img: u16 ancho, u16 alto, u16 ?, u16 color transparente, luego ancho×alto píxeles RGB555.
bool decodificar_img(Bytes datos, ImagenRGBA& salida);
// .png: algunos vienen "mangled" (firma \x89PNX y XOR encadenado con 0x52); se reparan antes de
// decodificar. El fondo negro se vuelve transparente (quitar_fondo_negro).
bool decodificar_png(Bytes datos, ImagenRGBA& salida);
// Negro puro -> transparente, y los oscuros unidos a él (resplandores) -> translúcidos (ver imagen.cpp).
void quitar_fondo_negro(ImagenRGBA& img);

}  // namespace lin
