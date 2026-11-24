#include <cstring>
#include <doctest/doctest.h>
#include "recursos/aes.h"

using namespace lin;

TEST_CASE("AES-128: vector de FIPS-197 (apéndice C.1)") {
    u8 clave[16], b[16];
    for (int i = 0; i < 16; ++i) clave[i] = static_cast<u8>(i);
    const u8 cifrado[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};
    std::memcpy(b, cifrado, 16);
    Aes128(clave).descifrar_bloque(b);
    for (int i = 0; i < 16; ++i) CHECK(b[i] == static_cast<u8>(i * 0x11));
}

// NIST SP 800-38A, F.2.2 (CBC-AES128.Decrypt), los dos primeros bloques.
static const u8 CLAVE_NIST[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
static const u8 IV_NIST[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const u8 CIFRADO_NIST[32] = {0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
                                    0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee, 0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2};
static const u8 CLARO_NIST[32] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                                  0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51};

TEST_CASE("AES-128 CBC: vector de NIST SP 800-38A") {
    u8 d[32];
    std::memcpy(d, CIFRADO_NIST, 32);
    descifrar_cbc(Aes128(CLAVE_NIST), IV_NIST, d, 32);
    CHECK(std::memcmp(d, CLARO_NIST, 32) == 0);
}

TEST_CASE("AES-128 CBC: el resto va con XOR contra el último bloque cifrado") {
    u8 d[35];
    std::memcpy(d, CIFRADO_NIST, 32);
    d[32] = 0xAA, d[33] = 0x55, d[34] = 0x00;
    descifrar_cbc(Aes128(CLAVE_NIST), IV_NIST, d, sizeof d);
    CHECK(std::memcmp(d, CLARO_NIST, 32) == 0);
    CHECK(d[32] == (0xAA ^ CIFRADO_NIST[16]));
    CHECK(d[33] == (0x55 ^ CIFRADO_NIST[17]));
    CHECK(d[34] == CIFRADO_NIST[18]);

    u8 corto[5] = {1, 2, 3, 4, 5};  // sin un bloque completo no se toca
    descifrar_cbc(Aes128(CLAVE_NIST), IV_NIST, corto, sizeof corto);
    CHECK(corto[0] == 1);
    CHECK(corto[4] == 5);
}
