#include <doctest/doctest.h>
#include <cmath>
#include <cstdlib>
#include <string>
#include "core/asignaciones.h"
#include "plataforma/archivo_mapeado.h"
#include "recursos/modelo.h"

using namespace lin;

static std::string carpeta_modelos() {
    const char* d = std::getenv("LINEAGE_MODELOS");
    if (!d) return {};
    std::string s = d;
    if (!s.empty() && s.back() != '/') s += '/';
    return s;
}

TEST_CASE("Modelo: cuaterniones y matrices") {
    const float a[4] = {0, 0, 0, 1};
    const float b[4] = {0, std::sin(0.5f), 0, std::cos(0.5f)};  // 60° sobre Y
    float m[4];
    cuat_slerp(a, b, 0.5f, m);
    CHECK(m[1] == doctest::Approx(std::sin(0.25f)).epsilon(1e-4));
    CHECK(m[3] == doctest::Approx(std::cos(0.25f)).epsilon(1e-4));
    const float c[4] = {0, -std::sin(0.5f), 0, -std::cos(0.5f)};  // el mismo giro con signo opuesto
    cuat_slerp(a, c, 0.5f, m);
    CHECK(std::abs(m[1]) == doctest::Approx(std::sin(0.25f)).epsilon(1e-4));
    Trs t;
    t.t[0] = 2;
    t.s[1] = 3;
    float x[16], y[16];
    mat_de_trs(t, x);
    mat_identidad(y);
    mat_mult(x, y, y);
    CHECK(y[12] == 2);
    CHECK(y[5] == 3);
}

TEST_CASE("Modelo: gólem animado (.glb de prueba_3d)" * doctest::skip(carpeta_modelos().empty())) {
    std::vector<u8> glb;
    REQUIRE(leer_archivo(carpeta_modelos() + "golem.glb", glb));
    Modelo m;
    REQUIRE(m.cargar(glb));
    CHECK(m.cantidad_huesos() == 17);
    CHECK(m.cantidad_clips() == 5);
    for (const char* n : {"quieto", "caminar", "atacar", "dano", "morir"}) CHECK(m.clip(n) >= 0);
    CHECK(m.textura().ancho > 0);
    CHECK(m.minimo()[1] == doctest::Approx(-0.99).epsilon(0.02));

    // En reposo, las matrices de los huesos son la identidad (global × inversa de bind).
    Pose p;
    float h[MAX_HUESOS][16];
    m.pose_reposo(p);
    m.matrices_huesos(p, h);
    for (int j = 0; j < m.cantidad_huesos(); ++j)
        for (int k = 0; k < 16; ++k) CHECK(h[j][k] == doctest::Approx(k % 5 == 0 ? 1.0 : 0.0).epsilon(1e-3));

    // Caminar es un ciclo: el final coincide con el principio, y a mitad de dos claves la pose es
    // la interpolación (distinta de las dos).
    const int caminar = m.clip("caminar");
    Pose p0, p1, pm;
    m.muestrear(caminar, 0, true, p0);
    m.muestrear(caminar, m.duracion(caminar), true, p1);
    for (int n = 0; n < MAX_NODOS && n < 19; ++n)
        for (int i = 0; i < 4; ++i) CHECK(p0.local[n].r[i] == doctest::Approx(p1.local[n].r[i]).epsilon(1e-3));
    m.muestrear(caminar, 0.25f * m.duracion(caminar), true, pm);
    bool distinta = false;
    for (int n = 0; n < 19; ++n) distinta |= std::abs(pm.local[n].r[0] - p0.local[n].r[0]) > 1e-3f;
    CHECK(distinta);

    // Muestrear, mezclar y calcular matrices no asigna memoria (va en el bucle del frame).
    const u64 antes = asignaciones_hilo();
    for (int i = 0; i < 100; ++i) {
        m.muestrear(caminar, i * 0.01f, true, p0);
        m.muestrear(m.clip("atacar"), i * 0.01f, false, p1);
        m.mezclar(p0, p1, 0.5f, pm);
        m.matrices_huesos(pm, h);
    }
    CHECK(asignaciones_hilo() == antes);
}
