// Mapa hash de direccionamiento abierto u64 -> u32, con sondeo lineal y borrado por
// desplazamiento hacia atrás (sin lápidas). Memoria contigua; crece al 70 % de ocupación.
#pragma once
#include <algorithm>
#include <vector>
#include "core/tipos.h"

namespace lin {

class MapaHash {
public:
    static constexpr u64 VACIA = ~0ull;  // clave reservada

    explicit MapaHash(size_t capacidad_inicial = 64) { redimensionar(potencia2(capacidad_inicial)); }

    const u32* buscar(u64 clave) const {
        size_t m = claves_.size() - 1;
        for (size_t i = mezclar(clave) & m;; i = (i + 1) & m) {
            if (claves_[i] == clave) return &valores_[i];
            if (claves_[i] == VACIA) return nullptr;
        }
    }
    u32* buscar(u64 clave) { return const_cast<u32*>(static_cast<const MapaHash*>(this)->buscar(clave)); }

    void poner(u64 clave, u32 valor) {
        if ((n_ + 1) * 10 > claves_.size() * 7) redimensionar(claves_.size() * 2);
        size_t m = claves_.size() - 1;
        for (size_t i = mezclar(clave) & m;; i = (i + 1) & m) {
            if (claves_[i] == clave) { valores_[i] = valor; return; }
            if (claves_[i] == VACIA) { claves_[i] = clave; valores_[i] = valor; ++n_; return; }
        }
    }

    bool quitar(u64 clave) {
        size_t m = claves_.size() - 1;
        size_t i = mezclar(clave) & m;
        while (claves_[i] != clave) {
            if (claves_[i] == VACIA) return false;
            i = (i + 1) & m;
        }
        // Desplazamiento hacia atrás: mueve los elementos siguientes del grupo al hueco.
        size_t j = i;
        for (;;) {
            j = (j + 1) & m;
            if (claves_[j] == VACIA) break;
            size_t ideal = mezclar(claves_[j]) & m;
            bool mover = (i <= j) ? (ideal <= i || ideal > j) : (ideal <= i && ideal > j);
            if (mover) {
                claves_[i] = claves_[j];
                valores_[i] = valores_[j];
                i = j;
            }
        }
        claves_[i] = VACIA;
        --n_;
        return true;
    }

    void limpiar() {
        std::fill(claves_.begin(), claves_.end(), VACIA);
        n_ = 0;
    }
    size_t tamano() const { return n_; }

private:
    static size_t potencia2(size_t n) {
        size_t p = 16;
        while (p < n) p <<= 1;
        return p;
    }
    static u64 mezclar(u64 x) {  // splitmix64
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }
    void redimensionar(size_t nueva) {
        std::vector<u64> ck = std::move(claves_);
        std::vector<u32> cv = std::move(valores_);
        claves_.assign(nueva, VACIA);
        valores_.assign(nueva, 0);
        n_ = 0;
        for (size_t i = 0; i < ck.size(); ++i)
            if (ck[i] != VACIA) poner(ck[i], cv[i]);
    }

    std::vector<u64> claves_;
    std::vector<u32> valores_;
    size_t n_ = 0;
};

}  // namespace lin
