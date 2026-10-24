// Cifrado de flujo de Lineage (PROTOCOLO_RED.md §4; idéntico a Cipher.java de L1J-JP).
// Clave de 8 bytes derivada de la semilla del paquete inicial, con estado independiente en cada
// sentido. Tras cada paquete: k[0..3] ^= primeros 4 bytes en claro; k[4..7] += 0x287EFFC3.
#pragma once
#include "core/tipos.h"

namespace lin {

class Cifrado {
public:
    explicit Cifrado(u32 semilla);
    // El cuerpo debe tener al menos 4 bytes (el cliente rellena a múltiplos de 4).
    void cifrar(u8* d, size_t n);
    void descifrar(u8* d, size_t n);

private:
    u8 k_[8];
    void actualizar(const u8 claro[4]);
};

}  // namespace lin
