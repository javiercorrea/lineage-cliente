// AES-128 estándar (FIPS-197), solo descifrado: lo que hace falta para los archivos cifrados de Text.pak
// (list.spz) y del Remastered (.xml, .json, .ui). Formato en docs/analisis/FORMATOS_PAK.md.
#pragma once
#include "core/tipos.h"

namespace lin {

class Aes128 {
public:
    explicit Aes128(const u8 clave[16]);
    void descifrar_bloque(u8 bloque[16]) const;

private:
    u8 subclaves_[176];
};

// Descifra in situ como el juego: CBC sin relleno sobre los bloques completos de 16 bytes; los n % 16
// bytes finales no pasan por AES, van con XOR contra los primeros bytes del último bloque cifrado.
// Con menos de 16 bytes no hay bloque y los datos quedan igual.
void descifrar_cbc(const Aes128& aes, const u8 iv[16], u8* datos, size_t n);
// descifrar_cbc con la clave y el vector del cliente.
void descifrar_aes_lineage(u8* datos, size_t n);

}  // namespace lin
