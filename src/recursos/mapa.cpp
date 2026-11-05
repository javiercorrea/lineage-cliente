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
    s.objetos.clear();
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
        if (s.tiles_objetos.size() == inicio) continue;
        ObjetoMapa o{static_cast<u32>(inicio), static_cast<u32>(s.tiles_objetos.size() - inicio), 255, 255, 0, 0};
        for (size_t k = inicio; k < s.tiles_objetos.size(); ++k) {
            const TileObjetoMapa& t = s.tiles_objetos[k];
            o.x0 = std::min<u8>(o.x0, t.x / 2);
            o.x1 = std::max<u8>(o.x1, t.x / 2);
            o.y0 = std::min(o.y0, t.y);
            o.y1 = std::max(o.y1, t.y);
        }
        s.objetos.push_back(o);
    }
    // Lo que sigue también es opcional: un segmento sin portales o cortado no invalida el resto.
    s.portales.clear();
    l.saltar(static_cast<size_t>(l.u32_()) * 5);
    l.saltar(static_cast<size_t>(l.u32_()) * 4);
    const u16 nportales = l.quedan(2) ? l.u16_() : 0;
    for (u16 i = 0; i < nportales && !l.error; ++i) {
        l.saltar(l.u8_());  // nombre
        const u8 x = l.u8_(), y = l.u8_();
        const u16 mapa = l.u16_();
        l.saltar(4);
        if (!l.error) s.portales.push_back({static_cast<u8>(x / 2), y, mapa});
    }
    return true;  // los objetos son opcionales: un error ahí no invalida el suelo
}

bool decodificar_mg(Bytes datos, int base_x, int base_y, SegmentoMapa& s) {
    constexpr int N = CELDAS_SEGMENTO;
    LectorBinario l{datos};
    if (l.u8_() != 'M' || l.u8_() != 'G' || l.u8_() != '0' || l.u8_() != '2') return false;
    l.u16_();  // base_x del archivo
    l.u16_();  // base_y del archivo (se usa el pasado por parámetro, como en decodificar_s32)
    const u32 piso_izq = l.u32_(), piso_der = l.u32_();
    // Estampa de pared: piezas relativas (dx en medias celdas, dy en celdas) de un chunk de roca real.
    const u16 n_estampa = l.u16_();
    std::vector<TileObjetoMapa> estampa;
    estampa.reserve(n_estampa);
    for (u16 i = 0; i < n_estampa; ++i) {
        const u8 dx = l.u8_(), dy = l.u8_(), capa = l.u8_();
        const u32 tile = l.u32_();
        estampa.push_back({dx, dy, capa, 0, tile});
    }
    if (l.error) return false;
    s.base_x = static_cast<u16>(base_x);
    s.base_y = static_cast<u16>(base_y);
    s.suelo.resize(static_cast<size_t>(N) * N * 2);
    s.atributos.assign(static_cast<size_t>(N) * N * 2, 0);
    s.tiles_objetos.clear();
    s.objetos.clear();
    s.portales.clear();
    // Suelo uniforme, atributos de colisión, y la estampa de pared entera en cada celda bloqueada.
    // El solapamiento entre celdas vecinas forma la masa de roca; cada celda es su propio objeto para
    // que la oclusión del jugador sea local (una pared tapa solo cuando está delante).
    for (int ly = 0; ly < N; ++ly) {
        for (int lx = 0; lx < N; ++lx) {
            const size_t c = (static_cast<size_t>(ly) * N + lx) * 2;
            s.suelo[c] = piso_izq;
            s.suelo[c + 1] = piso_der;
            if (!l.u8_()) continue;  // piso
            s.atributos[c] = 1;
            s.atributos[c + 1] = 1;
            if (estampa.empty()) continue;
            const u32 inicio = static_cast<u32>(s.tiles_objetos.size());
            u8 x0 = 63, y0 = 63, x1 = 0, y1 = 0;
            for (const TileObjetoMapa& e : estampa) {
                const u8 px = static_cast<u8>(lx * 2 + e.x), py = static_cast<u8>(ly + e.y);
                s.tiles_objetos.push_back({px, py, e.capa, 0, e.tile});
                x0 = std::min<u8>(x0, px / 2); x1 = std::max<u8>(x1, px / 2);
                y0 = std::min<u8>(y0, py);     y1 = std::max<u8>(y1, py);
            }
            calcular_bases(s.tiles_objetos.data() + inicio, estampa.size());
            s.objetos.push_back({inicio, static_cast<u32>(estampa.size()), x0, y0, x1, y1});
        }
    }
    return !l.error;
}

bool decodificar_segmento(std::string_view ruta, Bytes datos, int base_x, int base_y, SegmentoMapa& s) {
    if (ruta.size() >= 3 && ruta.substr(ruta.size() - 3) == ".mg")
        return decodificar_mg(datos, base_x, base_y, s);
    return decodificar_s32(datos, base_x, base_y, s);
}

}  // namespace lin
