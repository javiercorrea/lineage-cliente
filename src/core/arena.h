// Arena de asignación lineal (bump allocator). Se resetea entera de una vez, típicamente al
// final de cada frame: los datos temporales del frame no generan allocations sueltas.
#pragma once
#include <cstdlib>
#include <new>
#include <string_view>
#include "core/tipos.h"

namespace lin {

class Arena {
public:
    explicit Arena(size_t capacidad) : base_(static_cast<u8*>(std::malloc(capacidad))), cap_(capacidad) {}
    ~Arena() { std::free(base_); }
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* reservar(size_t n, size_t alineacion = alignof(std::max_align_t)) {
        size_t inicio = (usado_ + alineacion - 1) & ~(alineacion - 1);
        if (inicio + n > cap_) return nullptr;
        usado_ = inicio + n;
        return base_ + inicio;
    }
    template <typename T>
    T* reservar_arreglo(size_t n) {
        return static_cast<T*>(reservar(sizeof(T) * n, alignof(T)));
    }
    // Copia un texto a la arena (terminado en \0) y devuelve una vista a la copia.
    std::string_view copiar(std::string_view s) {
        char* p = reservar_arreglo<char>(s.size() + 1);
        if (!p) return {};
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = 0;
        return {p, s.size()};
    }
    void resetear() { usado_ = 0; }
    size_t usado() const { return usado_; }
    size_t capacidad() const { return cap_; }

private:
    u8* base_;
    size_t cap_;
    size_t usado_ = 0;
};

}  // namespace lin
