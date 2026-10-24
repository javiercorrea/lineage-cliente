#include <doctest/doctest.h>
#include "recursos/des.h"

using namespace lin;

TEST_CASE("DES: vector clásico de prueba (clave 133457799BBCDFF1)") {
    const u8 clave[8] = {0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF1};
    u8 b[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    const u8 esperado[8] = {0x85, 0xE8, 0x13, 0x54, 0x0F, 0x0A, 0xB4, 0x05};
    Des d(clave);
    d.cifrar_bloque(b);
    CHECK(std::memcmp(b, esperado, 8) == 0);
    d.descifrar_bloque(b);
    const u8 claro[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    CHECK(std::memcmp(b, claro, 8) == 0);
}

TEST_CASE("DES: descifrar deja intacto el resto que no completa un bloque") {
    Des d(CLAVE_DES_LINEAGE);
    u8 datos[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    d.descifrar(datos, sizeof datos);
    CHECK(datos[8] == 9);
    CHECK(datos[10] == 11);
}
