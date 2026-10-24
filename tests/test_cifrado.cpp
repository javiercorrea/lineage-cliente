#include <doctest/doctest.h>
#include <vector>
#include "proto/cifrado.h"

using namespace lin;

static std::vector<u8> hex(const char* s) {
    std::vector<u8> v;
    for (; s[0] && s[1]; s += 2) v.push_back(static_cast<u8>(std::stoi(std::string(s, 2), nullptr, 16)));
    return v;
}

// Vectores generados con docs/analisis/scripts/lineage_proto.py, que coincide byte a byte con las
// rutinas del cliente original ejecutadas en emulación (docs/analisis/resultados/verificacion_cifrado.txt).
TEST_CASE("Cifrado: coincide con lineage_proto.py, incluida la evolución de la clave") {
    Cifrado c(0x60002c4c);
    std::vector<u8> a;
    for (u8 i = 1; i <= 16; ++i) a.push_back(i);
    c.cifrar(a.data(), a.size());
    CHECK(a == hex("6138aba9f52cf9d55bb014d328f92410"));
    std::vector<u8> b = {95, 0xb3, 0x7f, 0x60, 0x80, 3, 0, 0};
    c.cifrar(b.data(), b.size());
    CHECK(b == hex("fafccc59cc10410c"));

    Cifrado d(0x12345678);
    std::vector<u8> x1 = {0x7a, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, x2 = x1;
    d.descifrar(x1.data(), x1.size());
    d.descifrar(x2.data(), x2.size());
    CHECK(x1 == hex("1113410e5e4f72f42b710f1b"));
    CHECK(x2 == hex("004d434f524ff31c3a624e15"));
}

TEST_CASE("Cifrado: ida y vuelta con dos extremos sincronizados") {
    Cifrado tx(0xdeadbeef), rx(0xdeadbeef);
    for (int n = 4; n < 64; n += 4) {
        std::vector<u8> m(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) m[i] = static_cast<u8>(n * 7 + i);
        std::vector<u8> orig = m;
        tx.cifrar(m.data(), m.size());
        CHECK(m != orig);
        rx.descifrar(m.data(), m.size());
        CHECK(m == orig);
    }
}
