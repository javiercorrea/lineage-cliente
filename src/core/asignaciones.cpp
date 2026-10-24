// operator new reemplazado: cuenta las allocations por hilo y delega en malloc. Va en las fuentes
// del ejecutable (no en una biblioteca estática, donde el enlazador podría descartarlo).
#include <cstdlib>
#include <new>
#include "core/asignaciones.h"

namespace lin {
namespace {
thread_local u64 contador = 0;
}
u64 asignaciones_hilo() { return contador; }
}  // namespace lin

namespace {
void* reservar(std::size_t n) {
    ++lin::contador;
    if (void* p = std::malloc(n ? n : 1)) return p;
    std::abort();  // sin excepciones (-fno-exceptions): sin memoria no hay cómo seguir
}
void* reservar_alineado(std::size_t n, std::align_val_t al) {
    ++lin::contador;
    std::size_t a = static_cast<std::size_t>(al);
    if (void* p = std::aligned_alloc(a, (n + a - 1) / a * a)) return p;
    std::abort();  // sin excepciones (-fno-exceptions): sin memoria no hay cómo seguir
}
}  // namespace

void* operator new(std::size_t n) { return reservar(n); }
void* operator new[](std::size_t n) { return reservar(n); }
void* operator new(std::size_t n, std::align_val_t al) { return reservar_alineado(n, al); }
void* operator new[](std::size_t n, std::align_val_t al) { return reservar_alineado(n, al); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { ++lin::contador; return std::malloc(n ? n : 1); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { ++lin::contador; return std::malloc(n ? n : 1); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
