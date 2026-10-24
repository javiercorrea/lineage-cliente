#include <doctest/doctest.h>
#include <random>
#include <unordered_map>
#include "core/anillo.h"
#include "core/arena.h"
#include "core/tabla_soa.h"

using namespace lin;

TEST_CASE("MapaHash: comparado con unordered_map en operaciones aleatorias") {
    MapaHash m;
    std::unordered_map<u64, u32> ref;
    std::mt19937_64 rng(1);
    for (int i = 0; i < 20000; ++i) {
        u64 k = rng() % 500;
        int op = static_cast<int>(rng() % 3);
        if (op == 0) {
            u32 v = static_cast<u32>(rng());
            m.poner(k, v);
            ref[k] = v;
        } else if (op == 1) {
            CHECK(m.quitar(k) == (ref.erase(k) == 1));
        } else {
            const u32* v = m.buscar(k);
            auto it = ref.find(k);
            REQUIRE((v != nullptr) == (it != ref.end()));
            if (v) CHECK(*v == it->second);
        }
    }
    CHECK(m.tamano() == ref.size());
}

TEST_CASE("TablaSoa: el índice queda consistente tras borrar con intercambio") {
    TablaSoa<u16, u32> t;
    for (ObjId id = 100; id < 200; ++id) {
        Slot s = t.insertar(id);
        t.col<0>()[s] = static_cast<u16>(id);
        t.col<1>()[s] = id * 10;
    }
    for (ObjId id = 100; id < 200; id += 3) CHECK(t.quitar(id));
    CHECK_FALSE(t.quitar(100));
    for (ObjId id = 100; id < 200; ++id) {
        Slot s = t.buscar(id);
        if ((id - 100) % 3 == 0) {
            CHECK(s == SIN_SLOT);
        } else {
            REQUIRE(s != SIN_SLOT);
            CHECK(t.ids()[s] == id);
            CHECK(t.col<0>()[s] == id);
            CHECK(t.col<1>()[s] == id * 10);
        }
    }
    CHECK(t.tamano() == 66);
}

TEST_CASE("AnilloBytes: mensajes de longitud variable con vuelta del anillo") {
    AnilloBytes a(64);
    std::vector<u8> salida;
    for (int ronda = 0; ronda < 50; ++ronda) {
        u8 m[10];
        for (int i = 0; i < 10; ++i) m[i] = static_cast<u8>(ronda + i);
        REQUIRE(a.empujar({m, static_cast<size_t>(1 + ronda % 10)}));
        REQUIRE(a.sacar(salida));
        CHECK(salida.size() == static_cast<size_t>(1 + ronda % 10));
        CHECK(salida[0] == static_cast<u8>(ronda));
    }
    CHECK_FALSE(a.sacar(salida));
    u8 grande[70] = {};
    CHECK_FALSE(a.empujar({grande, 70}));  // no entra
}

TEST_CASE("Arena: alineación, copia y reset") {
    Arena a(256);
    u8* p1 = static_cast<u8*>(a.reservar(3, 1));
    u64* p2 = a.reservar_arreglo<u64>(2);
    REQUIRE(p1);
    REQUIRE(p2);
    CHECK(reinterpret_cast<uintptr_t>(p2) % alignof(u64) == 0);
    CHECK(a.copiar("hola") == "hola");
    CHECK(a.reservar(1000) == nullptr);
    a.resetear();
    CHECK(a.usado() == 0);
}
