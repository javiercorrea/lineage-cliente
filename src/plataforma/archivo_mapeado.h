// Archivo mapeado en memoria de solo lectura. Los .pak del juego (hasta ~1,9 GB) se leen sin
// copiarlos: el sistema operativo carga las páginas que se tocan.
#pragma once
#include <string>
#include <vector>
#include "core/tipos.h"

namespace lin {

class ArchivoMapeado {
public:
    ArchivoMapeado() = default;
    ~ArchivoMapeado() { cerrar(); }
    ArchivoMapeado(const ArchivoMapeado&) = delete;
    ArchivoMapeado& operator=(const ArchivoMapeado&) = delete;
    ArchivoMapeado(ArchivoMapeado&& o) noexcept { *this = static_cast<ArchivoMapeado&&>(o); }
    ArchivoMapeado& operator=(ArchivoMapeado&& o) noexcept;

    bool abrir(const std::string& ruta);
    void cerrar();
    Bytes datos() const { return {datos_, tam_}; }

private:
    const u8* datos_ = nullptr;
    size_t tam_ = 0;
};

// Trae del disco las páginas de un tramo del mapeo (para no esperar la lectura después, al usarlo).
// Bloquea hasta que están en memoria: se llama desde el hilo de precarga.
void traer_a_memoria(Bytes tramo);

// Lee un archivo pequeño completo (fuentes, mapas). Devuelve vacío si no existe.
bool leer_archivo(const std::string& ruta, std::vector<u8>& salida);

}  // namespace lin
