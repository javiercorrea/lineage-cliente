// Tipos base del cliente. Sin dependencias externas (ni SDL ni STL pesada).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

namespace lin {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using Bytes = std::span<const u8>;

// Ids de dominio. ObjId es el id del servidor; Slot, el índice local en una tabla.
using ObjId = u32;
using Slot = u32;
using GfxId = u16;
using MapId = u16;
constexpr Slot SIN_SLOT = 0xFFFFFFFFu;

struct TilePos {
    u16 x, y;
};

// Direcciones (heading) del protocolo L1J: 0 = arriba (y-1), en sentido horario.
// Delta de celda al avanzar en cada dirección (confirmado en S_MoveCharPacket.java).
constexpr i8 DIR_DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr i8 DIR_DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

// Lectura little-endian acotada: nunca lee fuera del span (devuelve 0 y marca error).
struct LectorBinario {
    Bytes datos;
    size_t pos = 0;
    bool error = false;

    bool quedan(size_t n) const { return pos + n <= datos.size(); }
    u8 u8_() {
        if (!quedan(1)) { error = true; return 0; }
        return datos[pos++];
    }
    i8 i8_() { return static_cast<i8>(u8_()); }
    u16 u16_() {
        if (!quedan(2)) { error = true; pos = datos.size(); return 0; }
        u16 v = static_cast<u16>(datos[pos] | (datos[pos + 1] << 8));
        pos += 2;
        return v;
    }
    i16 i16_() { return static_cast<i16>(u16_()); }
    u32 u32_() {
        if (!quedan(4)) { error = true; pos = datos.size(); return 0; }
        u32 v = static_cast<u32>(datos[pos]) | (static_cast<u32>(datos[pos + 1]) << 8) |
                (static_cast<u32>(datos[pos + 2]) << 16) | (static_cast<u32>(datos[pos + 3]) << 24);
        pos += 4;
        return v;
    }
    void saltar(size_t n) {
        if (!quedan(n)) { error = true; pos = datos.size(); return; }
        pos += n;
    }
    Bytes tomar(size_t n) {
        if (!quedan(n)) { error = true; pos = datos.size(); return {}; }
        Bytes b = datos.subspan(pos, n);
        pos += n;
        return b;
    }
};

#define LIN_LOG(...) (std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))

}  // namespace lin
