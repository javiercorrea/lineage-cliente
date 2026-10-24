#include "plataforma/socket.h"

#if defined(_WIN32)
// MULTIPLATAFORMA (Windows): usar Winsock2 (WSAStartup una vez al inicio, socket/connect/recv/
// send iguales, closesocket en lugar de close, WSAGetLastError en lugar de errno).
#error "socket.cpp: falta la implementación para Windows"
#else
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace lin {

bool SocketTcp::conectar(const std::string& host, u16 puerto, int timeout_ms) {
    cerrar();
    addrinfo pista{};
    pista.ai_family = AF_UNSPEC;
    pista.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string p = std::to_string(puerto);
    if (getaddrinfo(host.c_str(), p.c_str(), &pista, &res) != 0) return false;
    for (addrinfo* a = res; a; a = a->ai_next) {
        int fd = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
        if (::connect(fd, a->ai_addr, a->ai_addrlen) == 0) {
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
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}

}  // namespace lin
