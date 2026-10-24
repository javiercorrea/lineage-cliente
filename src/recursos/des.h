// DES estándar (FIPS 46-3) en modo ECB, solo lo necesario para Text.pak / Text.idx.
// La clave del juego es "~!@#%^$<" (ver resources/pack.cpp de l1j-client).
#pragma once
#include "core/tipos.h"

namespace lin {

class Des {
public:
    explicit Des(const u8 clave[8]);
    void cifrar_bloque(u8 bloque[8]) const;
    void descifrar_bloque(u8 bloque[8]) const;
    // Descifra in situ los bloques completos de 8 bytes; el resto final queda igual (como el juego).
    void descifrar(u8* datos, size_t n) const;

private:
    void procesar(u8 bloque[8], bool descifrando) const;
    u64 subclaves_[16];
};

extern const u8 CLAVE_DES_LINEAGE[8];

}  // namespace lin
