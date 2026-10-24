// Cursores del juego: mano (moverse, interfaz), espada (sobre un monstruo, o con Ctrl sobre otro
// personaje) y círculo de objetivo (un hechizo espera su objetivo), como en el original.
//
// Los del original no están en esta instalación: no hay archivos de cursor, Lineage.exe solo trae
// los cursores estándar de Windows (ayuda y flechas de desplazamiento) y en los .pak no aparecen
// (se buscaron entre todas las imágenes y sprites chicos). Seguramente van dentro del ejecutable
// cifrado (lin.bin). Estos son dibujos propios en el mismo estilo: 16×16 en texto, escalados a
// 32 px (y 64 px para pantallas Retina), como cursores del sistema (SDL_CreateColorCursor): se
// mueven sin el retraso de un frame que tendría dibujarlos con el juego.
#include <cmath>
#include "app/app.h"

namespace lin {

namespace {

// Paleta: K contorno, Y/y dorado claro/oscuro, W/G/g acero claro/medio/oscuro, B cuero, R rojo.
u32 color(char c) {
    switch (c) {
    case 'K': return rgba(24, 16, 8);
    case 'Y': return rgba(240, 204, 104);
    case 'y': return rgba(186, 136, 52);
    case 'W': return rgba(244, 244, 236);
    case 'G': return rgba(176, 182, 194);
    case 'g': return rgba(112, 118, 130);
    case 'B': return rgba(128, 76, 34);
    case 'R': return rgba(226, 54, 42);
    default: return 0;
    }
}

// Mano dorada que apunta (punto activo: la punta del índice).
constexpr const char* MANO[16] = {
    "..KK............", ".KYYK...........", ".KYyK...........", ".KYyK...........",
    ".KYyKKK.........", ".KYyKYyKKK......", ".KYyKYyKYyKK....", ".KYyyYyyYyKyK...",
    ".KYyyyyyyyyyyK..", "KKYyyyyyyyyyyK..", "KYKyyyyyyyyyyK..", "KYYyyyyyyyyyK...",
    ".KYyyyyyyyyyK...", "..KYyyyyyyyK....", "...KYyyyyyK.....", "...KKKKKKKK.....",
};
// Espada en diagonal (punto activo: la punta).
constexpr const char* ESPADA[16] = {
    "KK..............", "KWGK............", ".KWGK...........", "..KWGK..........",
    "...KWGK.........", "....KWGK........", ".....KWGK.......", "......KWGK......",
    ".......KWGK..K..", "........KWGKKYK.", ".........KWYYK..", "..........KYyK..",
    ".........KYKKBK.", "........KYK..KBK", ".........K....KB", "..............KK",
};

// Círculo de objetivo: anillo rojo con contorno, marcas en cruz y un punto al centro.
char objetivo(int x, int y) {
    const double dx = x - 7.5, dy = y - 7.5, r = std::sqrt(dx * dx + dy * dy);
    if (r > 7.6) return '.';
    if (r > 6.6) return 'K';
    if (r > 5.0) return 'R';
    if (r > 4.2) return 'K';
    if ((x == 7 || x == 8) && (y <= 4 || y >= 11)) return 'Y';
    if ((y == 7 || y == 8) && (x <= 4 || x >= 11)) return 'Y';
    if (r < 1.3) return 'R';
    return '.';
}

SDL_Surface* superficie(char (*pixel)(int, int, const char* const*), const char* const* arte, int escala) {
    SDL_Surface* s = SDL_CreateSurface(16 * escala, 16 * escala, SDL_PIXELFORMAT_RGBA32);
    if (!s) return nullptr;
    u8* px = static_cast<u8*>(s->pixels);
    for (int y = 0; y < 16 * escala; ++y)
        for (int x = 0; x < 16 * escala; ++x) {
            u32 c = color(pixel(x / escala, y / escala, arte));
            std::memcpy(px + y * s->pitch + x * 4, &c, 4);  // RGBA32 = bytes R, G, B, A (como rgba())
        }
    return s;
}

char de_arte(int x, int y, const char* const* arte) { return arte[y][x]; }
char de_objetivo(int x, int y, const char* const*) { return objetivo(x, y); }

SDL_Cursor* crear(char (*pixel)(int, int, const char* const*), const char* const* arte, int hx, int hy) {
    SDL_Surface* base = superficie(pixel, arte, 2);
    SDL_Surface* doble = superficie(pixel, arte, 4);  // pantallas Retina
    if (!base) return nullptr;
    if (doble) SDL_AddSurfaceAlternateImage(base, doble);
    SDL_Cursor* c = SDL_CreateColorCursor(base, hx * 2, hy * 2);
    SDL_DestroySurface(doble);
    SDL_DestroySurface(base);
    return c;
}

}  // namespace

void crear_cursores(App& a) {
    a.cursores[static_cast<int>(Cursor::MANO)] = crear(de_arte, MANO, 2, 0);
    a.cursores[static_cast<int>(Cursor::ESPADA)] = crear(de_arte, ESPADA, 0, 0);
    a.cursores[static_cast<int>(Cursor::OBJETIVO)] = crear(de_objetivo, nullptr, 7, 7);
}

void aplicar_cursor(App& a) {
    if (a.cursor == a.cursor_aplicado) return;
    if (SDL_Cursor* c = a.cursores[static_cast<int>(a.cursor)]) SDL_SetCursor(c);
    a.cursor_aplicado = a.cursor;
}

void destruir_cursores(App& a) {
    for (SDL_Cursor*& c : a.cursores) {
        if (c) SDL_DestroyCursor(c);
        c = nullptr;
    }
}

}  // namespace lin
