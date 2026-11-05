// Fuentes bitmap del cliente original (carpeta font/).
//
// eng.fnt: 95 glifos ASCII (0x20–0x7E) de 6×12, 1 byte por fila (bit 7 = píxel izquierdo).
// kor.fnt: 2401 glifos de 12×12, 2 bytes por fila (bit 15 = píxel izquierdo): los 51 jamo de
//          compatibilidad (KS X 1001 0xA4A1–0xA4D3) y luego las 2350 sílabas (0xB0A1–0xC8FE).
//          El orden se dedujo del tamaño del archivo (57624 = 2401 × 24); ver cp949.h.
#pragma once
#include <string>
#include <vector>
#include "recursos/imagen.h"

namespace lin {

constexpr int ANCHO_GLIFO_ASCII = 6;
constexpr int ANCHO_GLIFO_KOR = 12;
constexpr int ALTO_GLIFO = 12;

struct FuenteBitmap {
    std::vector<u8> ascii;   // eng.fnt
    std::vector<u8> coreana; // kor.fnt (opcional)

    bool cargar(const std::string& carpeta_juego);
    // Glifo blanco sobre transparente; el color se aplica al dibujar. `glifo`: 0–94 ASCII,
    // 1000+ índice coreano.
    bool rasterizar(u32 glifo, ImagenRGBA& salida) const;
};

// Texto UTF-8 válido: se dibuja con la fuente TTF (recursos/fuente_ttf.h), proporcional. Cualquier
// otro (CP949 del servidor): fuente bitmap. Los bytes no representables se omiten.
struct GlifoTexto {
    u32 glifo;
    u8 ancho;
};
// Carácter UTF-8 que empieza en t[i]: devuelve sus bytes (0 si no es UTF-8 válido) y su punto en cp.
size_t largo_utf8(const char* t, size_t n, size_t i, u32& cp);
bool es_utf8(const char* t, size_t n);
void glifos_de_texto(const char* texto, size_t n, std::vector<GlifoTexto>& salida);
int ancho_texto(const char* texto, size_t n);
// Bytes del comienzo del texto que entran en `ancho_max` píxeles, sin partir caracteres CP949 y
// cortando preferentemente en un espacio (para el ajuste de línea del chat). Al menos 1 carácter.
size_t bytes_que_caben(const char* texto, size_t n, int ancho_max);

}  // namespace lin
