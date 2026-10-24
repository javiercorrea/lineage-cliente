#include <algorithm>
#include <vector>
#include "plataforma/archivo_mapeado.h"

#if defined(_WIN32)
// MULTIPLATAFORMA (Windows): implementar con CreateFileW + CreateFileMappingW + MapViewOfFile
// y liberar con UnmapViewOfFile + CloseHandle. Las rutas deben convertirse de UTF-8 a UTF-16.
#error "archivo_mapeado.cpp: falta la implementación para Windows"
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lin {

ArchivoMapeado& ArchivoMapeado::operator=(ArchivoMapeado&& o) noexcept {
    if (this != &o) {
        cerrar();
        datos_ = o.datos_;
        tam_ = o.tam_;
        o.datos_ = nullptr;
        o.tam_ = 0;
    }
    return *this;
}

// POSIX: válido para macOS y Linux.
bool ArchivoMapeado::abrir(const std::string& ruta) {
    cerrar();
    int fd = ::open(ruta.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size == 0) {
        ::close(fd);
        return false;
    }
    void* p = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);  // el mapeo sigue válido tras cerrar el descriptor
    if (p == MAP_FAILED) return false;
    datos_ = static_cast<const u8*>(p);
    tam_ = static_cast<size_t>(st.st_size);
    return true;
}

void ArchivoMapeado::cerrar() {
    if (datos_) munmap(const_cast<u8*>(datos_), tam_);
    datos_ = nullptr;
    tam_ = 0;
}

void traer_a_memoria(Bytes tramo) {
    if (tramo.empty()) return;
    // MULTIPLATAFORMA (Windows): PrefetchVirtualMemory en lugar de madvise.
    const uintptr_t pagina = static_cast<uintptr_t>(getpagesize());
    const uintptr_t ini = reinterpret_cast<uintptr_t>(tramo.data()) & ~(pagina - 1);
    const uintptr_t fin = reinterpret_cast<uintptr_t>(tramo.data()) + tramo.size();
    // Pide la lectura de todo el tramo de una vez (en paralelo) y después toca cada página.
    madvise(reinterpret_cast<void*>(ini), fin - ini, MADV_WILLNEED);
    volatile u8 suma = 0;
    for (uintptr_t p = ini; p < fin; p += pagina) suma = suma + *reinterpret_cast<const u8*>(std::max(p, reinterpret_cast<uintptr_t>(tramo.data())));
    (void)suma;
}

bool leer_archivo(const std::string& ruta, std::vector<u8>& salida) {
    FILE* f = std::fopen(ruta.c_str(), "rb");  // MULTIPLATAFORMA (Windows): _wfopen con ruta UTF-16
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    salida.resize(n > 0 ? static_cast<size_t>(n) : 0);
    bool ok = n > 0 && std::fread(salida.data(), 1, salida.size(), f) == salida.size();
    std::fclose(f);
    return ok;
}

}  // namespace lin
