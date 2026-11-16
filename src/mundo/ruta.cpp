#include "mundo/ruta.h"
#include <algorithm>
#include <cstdlib>
#include <queue>
#include <unordered_map>

namespace lin {

namespace {

i32 distancia(i32 x0, i32 y0, i32 x1, i32 y1) { return std::max(std::abs(x1 - x0), std::abs(y1 - y0)); }

u64 clave(i32 x, i32 y) { return (static_cast<u64>(static_cast<u32>(x)) << 32) | static_cast<u32>(y); }

struct Nodo {
    i32 x, y, g;
    u64 padre;
    bool cerrado;
};

struct Abierto {
    i32 f, g;
    i32 x, y;
    // Menor f primero; a igual f, mayor g (el más avanzado), para no abrir de más en mapas abiertos.
    bool operator<(const Abierto& o) const { return f != o.f ? f > o.f : g < o.g; }
};

}  // namespace

PuedePasar paso_por_celdas(std::function<bool(i32, i32)> pisable) {
    return [pisable = std::move(pisable)](i32 x, i32 y, int dir) {
        const i32 nx = x + DIR_DX[dir], ny = y + DIR_DY[dir];
        if (!pisable(nx, ny)) return false;
        if (DIR_DX[dir] && DIR_DY[dir]) return pisable(nx, y) && pisable(x, ny);
        return true;
    };
}

std::vector<std::pair<i32, i32>> buscar_ruta(i32 x0, i32 y0, i32 x1, i32 y1, const PuedePasar& puede, i32 cerca, i32 max_nodos) {
    std::vector<std::pair<i32, i32>> ruta;
    if (distancia(x0, y0, x1, y1) <= cerca) return ruta;
    std::unordered_map<u64, Nodo> nodos;
    std::priority_queue<Abierto> abiertos;
    nodos[clave(x0, y0)] = {x0, y0, 0, clave(x0, y0), false};
    abiertos.push({distancia(x0, y0, x1, y1), 0, x0, y0});
    i32 explorados = 0;
    while (!abiertos.empty()) {
        const Abierto a = abiertos.top();
        abiertos.pop();
        Nodo& n = nodos[clave(a.x, a.y)];
        if (n.cerrado || a.g > n.g) continue;  // una entrada vieja de la cola
        n.cerrado = true;
        if (distancia(a.x, a.y, x1, y1) <= cerca) {
            // Llegó: se arma la ruta de atrás para adelante.
            for (u64 k = clave(a.x, a.y); k != clave(x0, y0); k = nodos[k].padre) ruta.emplace_back(nodos[k].x, nodos[k].y);
            std::reverse(ruta.begin(), ruta.end());
            return ruta;
        }
        if (++explorados > max_nodos) break;
        for (int d = 0; d < 8; ++d) {
            if (!puede(a.x, a.y, d)) continue;
            const i32 nx = a.x + DIR_DX[d], ny = a.y + DIR_DY[d], g = a.g + 1;
            const u64 k = clave(nx, ny);
            auto it = nodos.find(k);
            if (it != nodos.end() && (it->second.cerrado || it->second.g <= g)) continue;
            nodos[k] = {nx, ny, g, clave(a.x, a.y), false};
            abiertos.push({g + distancia(nx, ny, x1, y1), g, nx, ny});
        }
    }
    return ruta;
}

void fijar_destino(Enrutador& r, i32 x, i32 y, i32 cerca) {
    if (!r.hay_destino) r.recalcular_ms = 0;  // un destino nuevo (no el mismo de cada frame): buscar ya
    r.hay_destino = true;
    r.dx = x;
    r.dy = y;
    r.cerca = cerca;
}

void soltar_destino(Enrutador& r) {
    r.hay_destino = false;
    r.ruta.clear();
    r.i = 0;
    r.sin_camino = false;
}

int siguiente_paso(Enrutador& r, i32 x, i32 y, const PuedePasar& puede, u64 ahora) {
    if (!r.hay_destino) return -1;
    if (distancia(x, y, r.dx, r.dy) <= r.cerca) {
        r.ruta.clear();
        r.i = 0;
        return -1;
    }
    if (r.i < r.ruta.size() && r.ruta[r.i] == std::pair<i32, i32>{x, y}) ++r.i;  // ya pisó la que seguía
    const bool fuera = r.i < r.ruta.size() && distancia(x, y, r.ruta[r.i].first, r.ruta[r.i].second) != 1;
    const bool acabada = r.i >= r.ruta.size() && !r.sin_camino;  // sin ruta o al final y todavía lejos
    if (acabada || fuera || ahora >= r.recalcular_ms) {
        r.ruta = buscar_ruta(x, y, r.dx, r.dy, puede, r.cerca);
        r.i = 0;
        r.sin_camino = r.ruta.empty();
        r.semilla ^= r.semilla << 13;
        r.semilla ^= r.semilla >> 17;
        r.semilla ^= r.semilla << 5;
        r.recalcular_ms = ahora + RECALCULO_MIN_MS + r.semilla % (RECALCULO_MAX_MS - RECALCULO_MIN_MS + 1);
    }
    if (r.i >= r.ruta.size()) return -1;
    const auto [nx, ny] = r.ruta[r.i];
    for (int d = 0; d < 8; ++d) {
        if (x + DIR_DX[d] != nx || y + DIR_DY[d] != ny) continue;
        if (puede(x, y, d)) return d;
        break;
    }
    r.recalcular_ms = 0;  // bloqueado (alguien se paró ahí, se cerró una puerta): buscar de nuevo el próximo frame
    r.sin_camino = false;
    return -1;
}

bool punto_al_azar(i32 x, i32 y, i32 min, i32 max, const std::function<bool(i32, i32)>& pisable, u32& semilla,
                   std::pair<i32, i32>& punto, int intentos) {
    if (max < 1 || min > max) return false;
    auto azar = [&semilla]() {
        if (!semilla) semilla = 0x9E3779B9u;  // xorshift no sale del 0
        semilla ^= semilla << 13;
        semilla ^= semilla >> 17;
        semilla ^= semilla << 5;
        return semilla;
    };
    const i32 lado = 2 * max + 1;
    for (int i = 0; i < intentos; ++i) {
        const i32 dx = static_cast<i32>(azar() % static_cast<u32>(lado)) - max;
        const i32 dy = static_cast<i32>(azar() % static_cast<u32>(lado)) - max;
        if (std::max(std::abs(dx), std::abs(dy)) < min || !pisable(x + dx, y + dy)) continue;
        punto = {x + dx, y + dy};
        return true;
    }
    return false;
}

}  // namespace lin
