// Modo auto, fase 2: búsqueda de ruta (mundo/ruta.h) y el avance entre puntos (mundo/modo_auto.h).
#include <doctest/doctest.h>
#include <cstdlib>
#include <string>
#include <vector>
#include "mundo/modo_auto.h"
#include "mundo/ruta.h"

using namespace lin;

namespace {
// Un mapa dibujado: '#' bloqueada; fuera del dibujo, bloqueada.
struct Mapa {
    std::vector<std::string> filas;
    bool pisable(i32 x, i32 y) const {
        return y >= 0 && y < static_cast<i32>(filas.size()) && x >= 0 && x < static_cast<i32>(filas[static_cast<size_t>(y)].size()) &&
               filas[static_cast<size_t>(y)][static_cast<size_t>(x)] != '#';
    }
    PuedePasar paso() const {
        return paso_por_celdas([this](i32 x, i32 y) { return pisable(x, y); });
    }
};

// Cada paso es a una celda vecina que se puede pisar, sin cortar esquinas.
void revisar(const Mapa& m, i32 x, i32 y, const std::vector<std::pair<i32, i32>>& ruta) {
    for (const auto& [nx, ny] : ruta) {
        CHECK(std::max(std::abs(nx - x), std::abs(ny - y)) == 1);
        CHECK(m.pisable(nx, ny));
        if (nx != x && ny != y) CHECK((m.pisable(nx, y) && m.pisable(x, ny)));
        x = nx;
        y = ny;
    }
}
}  // namespace

TEST_CASE("buscar_ruta: en un mapa abierto, la más corta (en diagonal cuenta 1)") {
    const Mapa m{{"..........", "..........", "..........", "..........", ".........."}};
    const auto r = buscar_ruta(0, 0, 7, 3, m.paso());
    REQUIRE(r.size() == 7);
    CHECK(r.back() == std::pair<i32, i32>{7, 3});
    revisar(m, 0, 0, r);
    CHECK(buscar_ruta(2, 2, 2, 2, m.paso()).empty());  // ya está
}

TEST_CASE("buscar_ruta: rodea una pared por el hueco") {
    const Mapa m{{
        ".....#.....",
        ".....#.....",
        ".....#.....",
        "...........",  // el hueco
        ".....#.....",
    }};
    const auto r = buscar_ruta(0, 0, 10, 0, m.paso());
    REQUIRE_FALSE(r.empty());
    CHECK(r.back() == std::pair<i32, i32>{10, 0});
    revisar(m, 0, 0, r);
    CHECK(std::any_of(r.begin(), r.end(), [](const auto& p) { return p == std::pair<i32, i32>{5, 3}; }));
}

TEST_CASE("buscar_ruta: sin camino, vacía; con 'cerca', se detiene antes") {
    const Mapa cerrado{{
        "......",
        ".####.",
        ".#..#.",
        ".####.",
        "......",
    }};
    CHECK(buscar_ruta(0, 0, 2, 2, cerrado.paso()).empty());
    CHECK(buscar_ruta(0, 0, 2, 2, cerrado.paso(), 2).empty());  // ya está a 2: no se mueve
    const auto r = buscar_ruta(5, 4, 2, 2, cerrado.paso(), 2);  // a 2 o menos: alcanza con llegar junto a la pared
    REQUIRE_FALSE(r.empty());
    CHECK(std::max(std::abs(r.back().first - 2), std::abs(r.back().second - 2)) <= 2);
    revisar(cerrado, 5, 4, r);
}

TEST_CASE("buscar_ruta: no corta esquinas en diagonal") {
    const Mapa m{{
        ".#",
        "#.",
    }};
    CHECK(buscar_ruta(0, 0, 1, 1, m.paso()).empty());
}

TEST_CASE("buscar_ruta: un destino imposible no explora más de max_nodos") {
    int llamadas = 0;
    const PuedePasar infinito = [&](i32 x, i32 y, int dir) {
        ++llamadas;
        return !(x + DIR_DX[dir] == 100 && y + DIR_DY[dir] == 100);  // todo libre salvo el destino
    };
    CHECK(buscar_ruta(0, 0, 100, 100, infinito, 0, 500).empty());
    CHECK(llamadas <= 8 * 501);
}

TEST_CASE("punto_al_azar: dentro del rango, pisable y repetible con la misma semilla") {
    const Mapa m{{"..........", "....##....", "..........", "....##....", ".........."}};
    auto pisable = [&](i32 x, i32 y) { return m.pisable(x, y); };
    u32 s1 = 42, s2 = 42;
    std::pair<i32, i32> a, b;
    for (int i = 0; i < 20; ++i) {
        REQUIRE(punto_al_azar(5, 2, 1, 3, pisable, s1, a));
        REQUIRE(punto_al_azar(5, 2, 1, 3, pisable, s2, b));
        CHECK(a == b);
        const i32 d = std::max(std::abs(a.first - 5), std::abs(a.second - 2));
        CHECK(d >= 1);
        CHECK(d <= 3);
        CHECK(m.pisable(a.first, a.second));
    }
    u32 s = 0;  // con semilla 0 también avanza
    const Mapa lleno{{"###", "###", "###"}};
    CHECK_FALSE(punto_al_azar(1, 1, 1, 1, [&](i32 x, i32 y) { return lleno.pisable(x, y); }, s, a));
}

TEST_CASE("Enrutador: sigue la ruta paso a paso hasta llegar") {
    const Mapa m{{
        ".....#.....",
        ".....#.....",
        "...........",
    }};
    Enrutador r;
    i32 x = 0, y = 0;
    u64 t = 0;
    int pasos = 0;
    for (; pasos < 50; ++pasos, t += 10) {
        fijar_destino(r, 10, 0);
        const int d = siguiente_paso(r, x, y, m.paso(), t);
        if (d < 0) break;
        x += DIR_DX[d];
        y += DIR_DY[d];
        CHECK(m.pisable(x, y));
    }
    CHECK(x == 10);
    CHECK(y == 0);
    CHECK(pasos < 20);
    // Llamarla de nuevo sin moverse (a mitad de un paso) no avanza.
    Enrutador q;
    fijar_destino(q, 10, 2);
    const int d1 = siguiente_paso(q, 0, 2, m.paso(), 0);
    CHECK(siguiente_paso(q, 0, 2, m.paso(), 1) == d1);
}

TEST_CASE("Enrutador: sigue la última ruta y recalcula hacia el último destino al azar") {
    const Mapa m{{"....................", "....................", "...................."}};
    Enrutador r;
    fijar_destino(r, 19, 0);
    CHECK(siguiente_paso(r, 0, 0, m.paso(), 0) == 2);  // hacia el este
    const u64 recalculo = r.recalcular_ms;
    CHECK(recalculo >= RECALCULO_MIN_MS);
    CHECK(recalculo <= RECALCULO_MAX_MS);
    // El destino se mueve: hasta el recálculo sigue la ruta vieja.
    fijar_destino(r, 0, 2);
    CHECK(siguiente_paso(r, 1, 0, m.paso(), 100) == 2);
    CHECK(r.dx == 0);
    // Pasado el recálculo, va hacia el último destino fijado.
    const int d = siguiente_paso(r, 2, 0, m.paso(), recalculo);
    CHECK((d == 5 || d == 4 || d == 6));  // sudoeste, sur u oeste
}

TEST_CASE("Enrutador: un paso bloqueado busca otra ruta; sin camino no busca en cada frame") {
    Mapa m{{"......", "......", "......"}};
    Enrutador r;
    fijar_destino(r, 5, 1);
    const int d0 = siguiente_paso(r, 0, 1, m.paso(), 0);
    REQUIRE(d0 >= 0);
    m.filas[static_cast<size_t>(1 + DIR_DY[d0])][static_cast<size_t>(DIR_DX[d0])] = '#';  // alguien se paró en la siguiente
    CHECK(siguiente_paso(r, 0, 1, m.paso(), 10) == -1);
    const int d = siguiente_paso(r, 0, 1, m.paso(), 20);  // el próximo frame: rodea
    REQUIRE(d >= 0);
    CHECK(d != d0);
    CHECK(m.pisable(DIR_DX[d], 1 + DIR_DY[d]));

    int llamadas = 0;
    const Mapa cerrado{{"..#.", "..#.", "..#."}};
    const PuedePasar contar = [&](i32 x, i32 y, int dir) {
        ++llamadas;
        return cerrado.paso()(x, y, dir);
    };
    Enrutador s;
    fijar_destino(s, 3, 1);
    CHECK(siguiente_paso(s, 0, 1, contar, 0) == -1);
    CHECK(s.sin_camino);
    const int antes = llamadas;
    CHECK(siguiente_paso(s, 0, 1, contar, 50) == -1);
    CHECK(llamadas == antes);  // hasta el recálculo, no vuelve a buscar
    soltar_destino(s);
    CHECK(siguiente_paso(s, 0, 1, contar, 60) == -1);
}

TEST_CASE("punto_actual: pasa al siguiente al llegar, en bucle o una vez") {
    Movimiento m;
    m.tipo = TipoMovimiento::PUNTOS;
    m.puntos = {{10, 10}, {20, 10}};
    EstadoAuto est;
    const auto* p = punto_actual(m, est, 0, 0);
    REQUIRE(p);
    CHECK(*p == std::pair<i32, i32>{10, 10});
    p = punto_actual(m, est, 9, 11);  // a 1: llegó
    REQUIRE(p);
    CHECK(*p == std::pair<i32, i32>{20, 10});
    p = punto_actual(m, est, 20, 10);
    REQUIRE(p);
    CHECK(*p == std::pair<i32, i32>{10, 10});  // en bucle
    m.una_vez = true;
    est.punto = 1;
    CHECK(punto_actual(m, est, 20, 10) == nullptr);  // el último, una vez: se acabó
    m.puntos.clear();
    CHECK(punto_actual(m, est, 0, 0) == nullptr);
}
