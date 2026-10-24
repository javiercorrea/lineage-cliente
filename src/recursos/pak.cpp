#include <algorithm>
#include "recursos/des.h"
#include "recursos/pak.h"

namespace lin {

static char minuscula(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

u64 hash_nombre(std::string_view nombre) {
    u64 h = 0xcbf29ce484222325ull;
    for (char c : nombre) {
        h ^= static_cast<u8>(minuscula(c));
        h *= 0x100000001b3ull;
    }
    return h;
}

bool Pak::abrir(const std::string& base, bool indice_cifrado) {
    std::vector<u8> idx;
    if (!leer_archivo(base + ".idx", idx) || idx.size() < 4) return false;
    if (!datos_.abrir(base + ".pak")) return false;
    LectorBinario l{idx};
    u32 n = l.u32_();
    if (static_cast<size_t>(n) * 28 + 4 > idx.size()) return false;
    if (indice_cifrado) Des(CLAVE_DES_LINEAGE).descifrar(idx.data() + 4, static_cast<size_t>(n) * 28);
    entradas_.resize(n);
    size_t validas = 0;
    for (u32 i = 0; i < n; ++i) {
        Entrada& e = entradas_[validas];
        e.offset = l.u32_();
        Bytes nom = l.tomar(20);
        e.tam = l.u32_();
        size_t len = 0;
        while (len < 20 && nom[len]) {
            e.nombre[len] = minuscula(static_cast<char>(nom[len]));
            ++len;
        }
        e.nombre[len] = 0;
        // Se descartan entradas que salen del .pak (el índice original tiene algunas rotas).
        if (static_cast<u64>(e.offset) + e.tam > datos_.datos().size()) continue;
        e.hash = hash_nombre({e.nombre, len});
        ++validas;
    }
    entradas_.resize(validas);
    // Ante nombres repetidos gana la última entrada del índice: stable_sort + búsqueda por el final.
    std::stable_sort(entradas_.begin(), entradas_.end(), [](const Entrada& a, const Entrada& b) { return a.hash < b.hash; });
    return true;
}

Bytes Pak::buscar(std::string_view nombre) const {
    u64 h = hash_nombre(nombre);
    auto it = std::upper_bound(entradas_.begin(), entradas_.end(), h, [](u64 v, const Entrada& e) { return v < e.hash; });
    if (it == entradas_.begin() || (it - 1)->hash != h) return {};
    const Entrada& e = *(it - 1);
    return datos_.datos().subspan(e.offset, e.tam);
}

bool Recursos::abrir(const std::string& carpeta) {
    carpeta_ = carpeta;
    bool ok = sprite_base_.abrir(carpeta + "Sprite", false) && tile_.abrir(carpeta + "Tile", false) &&
              texto_.abrir(carpeta + "Text", true);
    char nombre[16];
    for (int i = 0; i < 16; ++i) {
        std::snprintf(nombre, sizeof nombre, "Sprite%02d", i);
        sprite_hash_[i].abrir(carpeta + nombre, false);  // opcionales: instalaciones viejas no los tienen
    }
    return ok;
}

Bytes Recursos::sprite(std::string_view nombre) const {
    unsigned suma = 0;
    for (char c : nombre) suma += static_cast<u8>(minuscula(c));
    Bytes b = sprite_hash_[suma & 15].buscar(nombre);
    return b.empty() ? sprite_base_.buscar(nombre) : b;
}

bool Recursos::texto(std::string_view nombre, std::vector<u8>& salida) const {
    Bytes b = texto_.buscar(nombre);
    if (b.empty()) return false;
    salida.assign(b.begin(), b.end());
    Des(CLAVE_DES_LINEAGE).descifrar(salida.data(), salida.size());
    return true;
}

}  // namespace lin
