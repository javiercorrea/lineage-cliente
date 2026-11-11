#include "plataforma/socket.h"

#if defined(_WIN32)
// MULTIPLATAFORMA (Windows): usar Winsock2 (WSAStartup una vez al inicio, socket/connect/recv/
// send iguales, closesocket en lugar de close, WSAGetLastError en lugar de errno).
#error "socket.cpp: falta la implementación para Windows"
#else
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace lin {

namespace {

// connect sin bloqueo: espera en tramos de 100 ms (para poder cancelar) hasta `timeout_ms`, y deja
// el socket otra vez bloqueante (lo usan leer_exacto y escribir).
bool conectar_con_plazo(int fd, const sockaddr* dir, socklen_t largo, int timeout_ms, const std::atomic<bool>* cancelar) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return false;
    bool ok = ::connect(fd, dir, largo) == 0;
    if (!ok && errno == EINPROGRESS) {
        for (int esperado = 0; esperado < timeout_ms && !(cancelar && cancelar->load()); esperado += 100) {
            pollfd p{fd, POLLOUT, 0};
            const int r = ::poll(&p, 1, 100);
            if (r < 0 && errno != EINTR) break;
            if (r > 0) {
                int error = 0;
                socklen_t n = sizeof error;
                ok = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &n) == 0 && error == 0;
                break;
            }
        }
    }
    return ok && fcntl(fd, F_SETFL, flags) == 0;
}

}  // namespace

bool SocketTcp::conectar(const std::string& host, u16 puerto, int timeout_ms, const std::atomic<bool>* cancelar) {
    cerrar();
    addrinfo pista{};
    pista.ai_family = AF_UNSPEC;
    pista.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string p = std::to_string(puerto);
    if (getaddrinfo(host.c_str(), p.c_str(), &pista, &res) != 0) return false;
    for (addrinfo* a = res; a && !(cancelar && cancelar->load()); a = a->ai_next) {
        int fd = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        if (conectar_con_plazo(fd, a->ai_addr, a->ai_addrlen, timeout_ms, cancelar)) {
            int uno = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &uno, sizeof uno);
#if defined(__APPLE__)
            setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &uno, sizeof uno);  // macOS: sin SIGPIPE
#endif
            fd_ = fd;
            break;
        }
        ::close(fd);
    }
    freeaddrinfo(res);
    return fd_ >= 0;
}

bool SocketTcp::leer_exacto(u8* destino, size_t n) {
    size_t leido = 0;
    while (leido < n) {
        ssize_t r = ::recv(fd_, destino + leido, n - leido, 0);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) continue;
            return false;
        }
        leido += static_cast<size_t>(r);
    }
    return true;
}

bool SocketTcp::escribir(const u8* datos, size_t n) {
#if defined(__linux__)
    const int flags = MSG_NOSIGNAL;  // Linux: evita SIGPIPE por envío
#else
    const int flags = 0;
#endif
    size_t enviado = 0;
    while (enviado < n) {
        ssize_t r = ::send(fd_, datos + enviado, n - enviado, flags);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) continue;
            return false;
        }
        enviado += static_cast<size_t>(r);
    }
    return true;
}

void SocketTcp::cerrar() {
    // exchange: si dos hilos cierran a la vez, solo uno cierra el descriptor (el otro podría cerrar
    // uno ya reutilizado por otro archivo).
    if (const int fd = fd_.exchange(-1); fd >= 0) {
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }
}

}  // namespace lin
