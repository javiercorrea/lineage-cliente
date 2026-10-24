#include "recursos/cp949.h"

namespace lin {

int indice_glifo_coreano(u8 b1, u8 b2) {
    if (b2 < 0xA1 || b2 > 0xFE) return -1;
    if (b1 == 0xA4 && b2 <= 0xD3) return b2 - 0xA1;                      // 51 jamo
    if (b1 >= 0xB0 && b1 <= 0xC8) return 51 + (b1 - 0xB0) * 94 + (b2 - 0xA1);  // 2350 sílabas
    return -1;
}

}  // namespace lin
