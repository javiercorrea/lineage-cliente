#include <algorithm>
#include <cstdlib>
#include "recursos/mapa.h"

namespace lin {

std::string nombre_segmento(MapId mapa, int x, int y) {
    char n[64];
    std::snprintf(n, sizeof n, "map/%u/%04x%04x.s32", mapa, (x >> 6) + 0x7E00, (y >> 6) + 0x7E00);
    return n;
}

// Fila de pantalla (en celdas) y media columna de una pieza: y - x/2 y x + 2y (ver mapa.h).
static int fila_pieza(const TileObjetoMapa& t) { return t.y - t.x / 2; }
static int columna_pieza(const TileObjetoMapa& t) { return t.x + 2 * t.y; }

// Para cada pieza del objeto: la fila más baja del objeto en su columna y en las vecinas, sin
// contar las piezas de suelo. Los objetos tienen pocas decenas de piezas: alcanza con O(n²).
static void calcular_bases(TileObjetoMapa* p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        int c = columna_pieza(p[i]), fila = fila_pieza(p[i]), base = fila;
        if (pieza_de_suelo(p[i])) {
            p[i].base = 0;
            continue;
        }
        for (size_t j = 0; j < n; ++j)
            if (!pieza_de_suelo(p[j]) && std::abs(columna_pieza(p[j]) - c) <= COLUMNAS_BASE)
                base = std::max(base, fila_pieza(p[j]));
        p[i].base = static_cast<i8>(std::min(base - fila, 127));
    }
}

bool decodificar_s32(Bytes datos, int base_x, int base_y, SegmentoMapa& s) {
    constexpr size_t CELDAS = CELDAS_SEGMENTO * CELDAS_SEGMENTO;
    LectorBinario l{datos};
    s.base_x = static_cast<u16>(base_x);
    s.base_y = static_cast<u16>(base_y);
    s.suelo.resize(CELDAS * 2);
    for (u32& v : s.suelo) v = l.u32_();
    u16 n = l.u16_();
    l.saltar(static_cast<size_t>(n) * 6);
    s.atributos.resize(CELDAS * 2);
    for (u16& v : s.atributos) v = l.u16_();
    if (l.error) return false;
    s.tiles_objetos.clear();
    u32 nobj = l.u32_();
    for (u32 i = 0; i < nobj && !l.error; ++i) {
        l.u16_();
        int ntiles = l.u16_();
        size_t inicio = s.tiles_objetos.size();
        for (int j = 0; j < ntiles && !l.error; ++j) {
            u8 x = l.u8_(), y = l.u8_();
            if (x == 0xCD && y == 0xCD) {
                l.saltar(5);
                --j;
                --ntiles;
                continue;
            }
            TileObjetoMapa t;
            t.x = x;
            t.y = y;
            t.capa = l.u8_();
            t.base = 0;
            t.tile = l.u32_();
            s.tiles_objetos.push_back(t);
        }
        calcular_bases(s.tiles_objetos.data() + inicio, s.tiles_objetos.size() - inicio);
    }
    return true;  // los objetos son opcionales: un error ahí no invalida el suelo
}

}  // namespace lin
