#include "proto/cifrado.h"

namespace lin {

Cifrado::Cifrado(u32 semilla) {
    u32 k0 = semilla ^ 0x9C30D539u;
    k0 = (k0 << 19) | (k0 >> 13);
    u32 k1 = 0x930FD7E2u ^ k0 ^ 0x7C72E993u;
    for (int i = 0; i < 4; ++i) {
        k_[i] = static_cast<u8>(k0 >> (8 * i));
        k_[4 + i] = static_cast<u8>(k1 >> (8 * i));
    }
}

void Cifrado::actualizar(const u8 claro[4]) {
    for (int i = 0; i < 4; ++i) k_[i] ^= claro[i];
    u32 v = static_cast<u32>(k_[4]) | (static_cast<u32>(k_[5]) << 8) | (static_cast<u32>(k_[6]) << 16) |
            (static_cast<u32>(k_[7]) << 24);
    v += 0x287EFFC3u;
    for (int i = 0; i < 4; ++i) k_[4 + i] = static_cast<u8>(v >> (8 * i));
}

void Cifrado::cifrar(u8* d, size_t n) {
    if (n < 4) return;
    u8 claro[4] = {d[0], d[1], d[2], d[3]};
    d[0] ^= k_[0];
    for (size_t i = 1; i < n; ++i) d[i] ^= d[i - 1] ^ k_[i & 7];
    d[3] ^= k_[2];
    d[2] ^= k_[3] ^ d[3];
    d[1] ^= k_[4] ^ d[2];
    d[0] ^= k_[5] ^ d[1];
    actualizar(claro);
}

void Cifrado::descifrar(u8* d, size_t n) {
    if (n < 4) return;
    d[0] ^= k_[5] ^ d[1];
    d[1] ^= k_[4] ^ d[2];
    d[2] ^= k_[3] ^ d[3];
    d[3] ^= k_[2];
    for (size_t i = n - 1; i >= 1; --i) d[i] ^= d[i - 1] ^ k_[i & 7];
    d[0] ^= k_[0];
    actualizar(d);
}

}  // namespace lin
