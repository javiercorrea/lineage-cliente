// Socket TCP bloqueante mínimo para el hilo de red.
#pragma once
#include <string>
#include "core/tipos.h"

namespace lin {

class SocketTcp {
public:
    ~SocketTcp() { cerrar(); }
    bool conectar(const std::string& host, u16 puerto, int timeout_ms = 5000);
    // Lee exactamente n bytes (bloquea). false si se cerró la conexión o hubo error.
    bool leer_exacto(u8* destino, size_t n);
    bool escribir(const u8* datos, size_t n);
    // Desbloquea un leer_exacto pendiente desde otro hilo (shutdown) y libera el socket.
    void cerrar();

private:
    int fd_ = -1;  // MULTIPLATAFORMA (Windows): SOCKET (UINT_PTR) en lugar de int
};

}  // namespace lin
