#include <algorithm>
#include <cstdlib>
#include <utility>
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

// .s32 y .seg (el formato viejo, que algunos segmentos todavía usan: el Chaotic Temple y el norte de
// Gludio en el mapa 4) tienen el mismo recorrido; el .seg lleva tiles en u16 (mismo valor:
// til << 8 | subtile), atributos en u8, entradas de 4 bytes en el primer bloque y la lista de .til
// con largo u8 (ver DESCUBRIMIENTOS.md).
static bool decodificar_s32_o_seg(Bytes datos, int base_x, int base_y, SegmentoMapa& s, bool seg) {
    constexpr size_t CELDAS = CELDAS_SEGMENTO * CELDAS_SEGMENTO;
    LectorBinario l{datos};
    auto tile = [&]() -> u32 { return seg ? l.u16_() : l.u32_(); };
    s.base_x = static_cast<u16>(base_x);
    s.base_y = static_cast<u16>(base_y);
    s.suelo.resize(CELDAS * 2);
    for (u32& v : s.suelo) v = tile();
    // Segunda capa de suelo: tiles transparentes puestos sobre el suelo (ver mapa.h).
    std::vector<TileObjetoMapa> encima;
    const u16 n = l.u16_();
    for (u16 i = 0; i < n && !l.error; ++i) {
        TileObjetoMapa t;
        t.x = l.u8_();
        t.y = l.u8_();
        t.capa = 0;
        t.base = 0;
        t.tile = tile() & 0xFFFFFF;
        if (t.x < 2 * CELDAS_SEGMENTO && t.y < CELDAS_SEGMENTO) encima.push_back(t);
    }
    s.atributos.resize(CELDAS * 2);
    for (u16& v : s.atributos) v = seg ? l.u8_() : l.u16_();
    if (l.error) return false;
    s.tiles_objetos.clear();
    s.objetos.clear();
    u32 nobj = l.u32_();
    for (u32 i = 0; i < nobj && !l.error; ++i) {
        const u16 gid = l.u16_();
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
            t.tile = tile();
            s.tiles_objetos.push_back(t);
        }
        calcular_bases(s.tiles_objetos.data() + inicio, s.tiles_objetos.size() - inicio);
        if (s.tiles_objetos.size() == inicio) continue;
        ObjetoMapa o{static_cast<u32>(inicio), static_cast<u32>(s.tiles_objetos.size() - inicio), 255, 255, 0, 0, gid};
        for (size_t k = inicio; k < s.tiles_objetos.size(); ++k) {
            const TileObjetoMapa& t = s.tiles_objetos[k];
            o.x0 = std::min<u8>(o.x0, t.x / 2);
            o.x1 = std::max<u8>(o.x1, t.x / 2);
            o.y0 = std::min(o.y0, t.y);
            o.y1 = std::max(o.y1, t.y);
        }
        s.objetos.push_back(o);
    }
    if (!encima.empty()) {
        ObjetoMapa o{static_cast<u32>(s.tiles_objetos.size()), static_cast<u32>(encima.size()), 255, 255, 0, 0};
        for (const TileObjetoMapa& t : encima) {
            o.x0 = std::min<u8>(o.x0, t.x / 2);
            o.x1 = std::max<u8>(o.x1, t.x / 2);
            o.y0 = std::min(o.y0, t.y);
            o.y1 = std::max(o.y1, t.y);
        }
        s.tiles_objetos.insert(s.tiles_objetos.end(), encima.begin(), encima.end());
        s.objetos.push_back(o);
    }
    // Lo que sigue también es opcional: un segmento sin portales o cortado no invalida el resto.
    s.portales.clear();
    // Registros de techo: u8 x (medias celdas), u8 y, u16 gid, u8 tipo; se descartan los que caen fuera del segmento.
    s.techos.clear();
    const u32 n_techos = l.u32_();
    for (u32 i = 0; i < n_techos && !l.error; ++i) {
        if (!l.quedan(5)) {  // cortado: lo que sigue (los .til y los portales) se leería de otro lado
            l.error = true;
            break;
        }
        const u8 x = l.u8_(), y = l.u8_();
        const u16 gid = l.u16_();
        const u8 tipo = l.u8_();
        if (x / 2 < CELDAS_SEGMENTO && y < CELDAS_SEGMENTO && tipo <= 1) s.techos.push_back({static_cast<u8>(x / 2), y, gid, tipo});
    }
    std::sort(s.techos.begin(), s.techos.end(), [](const TechoMapa& a, const TechoMapa& b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    l.saltar(seg ? l.u8_() : static_cast<size_t>(l.u32_()) * 4);  // los .til que usa
    const u16 nportales = !l.error && l.quedan(2) ? l.u16_() : 0;  // con los techos cortados, sin portales
    for (u16 i = 0; i < nportales && !l.error; ++i) {
        l.saltar(l.u8_());  // nombre
        const u8 x = l.u8_(), y = l.u8_();
        const u16 mapa = l.u16_();
        l.saltar(4);
        if (!l.error) s.portales.push_back({static_cast<u8>(x / 2), y, mapa});
    }
    s.efectos.clear();
    const u16 nefectos = !l.error && l.quedan(2) ? l.u16_() : 0;
    const size_t extra = (nefectos & 0x8000) ? 4 : 0;
    for (u16 i = 0; i < (nefectos & 0x7FFF) && l.quedan(6 + extra); ++i) {
        const u16 gfx = l.u16_(), x = l.u16_(), y = l.u16_();
        l.saltar(extra);
        s.efectos.push_back({gfx, x, y});
    }
    return true;  // los objetos son opcionales: un error ahí no invalida el suelo
}

std::span<const TechoMapa> techos_en(const SegmentoMapa& s, int x, int y) {
    const int lx = x - s.base_x, ly = y - s.base_y;
    if (lx < 0 || ly < 0 || lx >= CELDAS_SEGMENTO || ly >= CELDAS_SEGMENTO) return {};
    const auto cmp = [](const TechoMapa& t, std::pair<int, int> k) { return t.y != k.second ? t.y < k.second : t.x < k.first; };
    auto it = std::lower_bound(s.techos.begin(), s.techos.end(), std::make_pair(lx, ly), cmp);
    auto fin = it;
    while (fin != s.techos.end() && fin->x == lx && fin->y == ly) ++fin;
    return it == fin ? std::span<const TechoMapa>{} : std::span<const TechoMapa>{&*it, static_cast<size_t>(fin - it)};
}

bool decodificar_s32(Bytes datos, int base_x, int base_y, SegmentoMapa& s) {
    return decodificar_s32_o_seg(datos, base_x, base_y, s, false);
}

bool decodificar_seg(Bytes datos, int base_x, int base_y, SegmentoMapa& s) {
    return decodificar_s32_o_seg(datos, base_x, base_y, s, true);
}

bool decodificar_mg(Bytes datos, int base_x, int base_y, SegmentoMapa& s) {
    constexpr int N = CELDAS_SEGMENTO;
    constexpr int N_CLASES = 4;  // horizontal, vertical, esquina, interior (ver generador_mapas)
    LectorBinario l{datos};
    if (l.u8_() != 'M' || l.u8_() != 'G' || l.u8_() != '0' || l.u8_() != '3') return false;
    l.u16_();  // base_x del archivo
    l.u16_();  // base_y del archivo (se usa el pasado por parámetro, como en decodificar_s32)
    // Paleta de piso: varios pares (izq, der); el byte de cada celda-piso elige uno (menos repetitivo).
    const u16 n_piso = l.u16_();
    if (n_piso == 0 || l.error) return false;
    std::vector<u32> piso_izq(n_piso), piso_der(n_piso);
    for (u16 i = 0; i < n_piso; ++i) { piso_izq[i] = l.u32_(); piso_der[i] = l.u32_(); }
    // Una estampa de pared por clase de orientación (piezas relativas: dx medias celdas, dy celdas).
    std::vector<TileObjetoMapa> estampa[N_CLASES];
    for (int c = 0; c < N_CLASES; ++c) {
        const u16 n = l.u16_();
        estampa[c].reserve(n);
        for (u16 i = 0; i < n; ++i) {
            const u8 dx = l.u8_(), dy = l.u8_(), capa = l.u8_();
            const u32 tile = l.u32_();
            estampa[c].push_back({dx, dy, capa, 0, tile});
        }
    }
    if (l.error) return false;
    s.base_x = static_cast<u16>(base_x);
    s.base_y = static_cast<u16>(base_y);
    s.suelo.resize(static_cast<size_t>(N) * N * 2);
    s.atributos.assign(static_cast<size_t>(N) * N * 2, 0);
    s.tiles_objetos.clear();
    s.objetos.clear();
    s.portales.clear();
    // Primera pasada: leer la grilla. 1 byte/celda: bit7 = pared; bits0-6 = índice de piso (celda-piso)
    // o clase de pared (celda-pared). Suelo y colisión.
    std::vector<u8> cel(static_cast<size_t>(N) * N);
    for (size_t i = 0; i < cel.size(); ++i) {
        const u8 b = l.u8_();
        cel[i] = b;
        if (b & 0x80) {  // pared
            s.atributos[i * 2] = 1;
            s.atributos[i * 2 + 1] = 1;
            s.suelo[i * 2] = piso_izq[0];  // piso bajo la pared (casi no se ve)
            s.suelo[i * 2 + 1] = piso_der[0];
        } else {
            const u16 p = (b & 0x7F) < n_piso ? (b & 0x7F) : 0;
            s.suelo[i * 2] = piso_izq[p];
            s.suelo[i * 2 + 1] = piso_der[p];
        }
    }
    if (l.error) return false;
    // Segunda pasada: en cada celda-pared, estampar la roca de SU clase, pero SOLO las piezas que caen
    // sobre otra celda-pared (máscara). Así la roca no invade los pasillos y la estructura se lee.
    // Cada celda es su propio objeto, para que la oclusión del jugador sea local.
    auto es_pared = [&](int x, int y) { return x >= 0 && x < N && y >= 0 && y < N && (cel[static_cast<size_t>(y) * N + x] & 0x80); };
    for (int ly = 0; ly < N; ++ly) {
        for (int lx = 0; lx < N; ++lx) {
            if (!es_pared(lx, ly)) continue;
            const int clase = cel[static_cast<size_t>(ly) * N + lx] & 0x7F;
            const std::vector<TileObjetoMapa>& est = estampa[clase < N_CLASES ? clase : 0];
            if (est.empty()) continue;
            const u32 inicio = static_cast<u32>(s.tiles_objetos.size());
            u8 x0 = 63, y0 = 63, x1 = 0, y1 = 0;
            for (const TileObjetoMapa& e : est) {
                const int cx = lx + e.x / 2, cy = ly + e.y;  // celda donde cae la pieza
                if (!es_pared(cx, cy)) continue;
                const u8 px = static_cast<u8>(lx * 2 + e.x), py = static_cast<u8>(cy);
                s.tiles_objetos.push_back({px, py, e.capa, 0, e.tile});
                x0 = std::min<u8>(x0, px / 2); x1 = std::max<u8>(x1, px / 2);
                y0 = std::min<u8>(y0, py);     y1 = std::max<u8>(y1, py);
            }
            const u32 n = static_cast<u32>(s.tiles_objetos.size()) - inicio;
            if (!n) continue;
            calcular_bases(s.tiles_objetos.data() + inicio, n);
            s.objetos.push_back({inicio, n, x0, y0, x1, y1});
        }
    }
    return !l.error;
}

bool decodificar_segmento(std::string_view ruta, Bytes datos, int base_x, int base_y, SegmentoMapa& s) {
    if (ruta.size() >= 3 && ruta.substr(ruta.size() - 3) == ".mg")
        return decodificar_mg(datos, base_x, base_y, s);
    if (ruta.size() >= 4 && ruta.substr(ruta.size() - 4) == ".seg")
        return decodificar_seg(datos, base_x, base_y, s);
    return decodificar_s32(datos, base_x, base_y, s);
}

}  // namespace lin
