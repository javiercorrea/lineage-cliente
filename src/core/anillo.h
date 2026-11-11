// Cola SPSC (un productor, un consumidor) sin locks para mensajes de longitud variable.
// Cada mensaje se guarda como [u16 longitud][bytes]. Comunica el hilo de red con el principal.
#pragma once
#include <atomic>
#include <vector>
#include "core/tipos.h"

namespace lin {

class AnilloBytes {
public:
    explicit AnilloBytes(size_t capacidad_pot2 = 1u << 20) : buf_(capacidad_pot2), mascara_(capacidad_pot2 - 1) {}

    // Productor. Devuelve false si no hay espacio (el mensaje no se encola).
    bool empujar(Bytes msg) {
        if (msg.size() > 0xFFFF) return false;
        size_t total = 2 + msg.size();
        size_t cab = cabeza_.load(std::memory_order_relaxed);
        size_t cola = cola_.load(std::memory_order_acquire);
        if (buf_.size() - (cab - cola) < total) return false;
        u8 len[2] = {static_cast<u8>(msg.size()), static_cast<u8>(msg.size() >> 8)};
        copiar_en(cab, len, 2);
        copiar_en(cab + 2, msg.data(), msg.size());
        cabeza_.store(cab + total, std::memory_order_release);
        return true;
    }

    // Consumidor. Copia el siguiente mensaje en `salida` y devuelve true; false si está vacía.
    bool sacar(std::vector<u8>& salida) {
        size_t cola = cola_.load(std::memory_order_relaxed);
        size_t cab = cabeza_.load(std::memory_order_acquire);
        if (cab == cola) return false;
        u8 len[2];
        copiar_de(cola, len, 2);
        size_t n = len[0] | (len[1] << 8);
        salida.resize(n);
        copiar_de(cola + 2, salida.data(), n);
        cola_.store(cola + 2 + n, std::memory_order_release);
        return true;
    }

    bool vacio() const { return cabeza_.load(std::memory_order_acquire) == cola_.load(std::memory_order_acquire); }
    // Descarta todo. Solo sin productor ni consumidor corriendo (antes de arrancar los hilos).
    void vaciar() {
        cabeza_.store(0, std::memory_order_relaxed);
        cola_.store(0, std::memory_order_relaxed);
    }

private:
    void copiar_en(size_t pos, const u8* src, size_t n) {
        for (size_t i = 0; i < n; ++i) buf_[(pos + i) & mascara_] = src[i];
    }
    void copiar_de(size_t pos, u8* dst, size_t n) const {
        for (size_t i = 0; i < n; ++i) dst[i] = buf_[(pos + i) & mascara_];
    }

    std::vector<u8> buf_;
    size_t mascara_;
    alignas(64) std::atomic<size_t> cabeza_{0};
    alignas(64) std::atomic<size_t> cola_{0};
};

}  // namespace lin
