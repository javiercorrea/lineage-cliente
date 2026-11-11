#include <doctest/doctest.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include "plataforma/socket.h"

using namespace lin;

namespace {

// Socket de escucha en 127.0.0.1, en un puerto libre que elige el sistema.
int escuchar(u16& puerto) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in dir{};
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t largo = sizeof dir;
    if (fd < 0 || ::bind(fd, reinterpret_cast<sockaddr*>(&dir), sizeof dir) != 0 || ::listen(fd, 1) != 0 ||
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&dir), &largo) != 0)
        return -1;
    puerto = ntohs(dir.sin_port);
    return fd;
}

}  // namespace

TEST_CASE("SocketTcp: conecta sin bloqueo y después lee y escribe bloqueante") {
    u16 puerto = 0;
    const int servidor = escuchar(puerto);
    REQUIRE(servidor >= 0);
    SocketTcp s;
    REQUIRE(s.conectar("127.0.0.1", puerto, 2000));
    const int cliente = ::accept(servidor, nullptr, nullptr);
    REQUIRE(cliente >= 0);
    const u8 hola[4] = {1, 2, 3, 4};
    REQUIRE(::send(cliente, hola, 2, 0) == 2);
    REQUIRE(::send(cliente, hola + 2, 2, 0) == 2);
    u8 leido[4] = {};
    CHECK(s.leer_exacto(leido, 4));  // bloqueante otra vez: espera los 4 bytes
    CHECK(leido[3] == 4);
    CHECK(s.escribir(hola, 4));
    u8 eco[4] = {};
    CHECK(::recv(cliente, eco, 4, MSG_WAITALL) == 4);
    s.cerrar();
    s.cerrar();  // dos veces no cierra un descriptor ajeno
    ::close(cliente);
    ::close(servidor);
}

TEST_CASE("SocketTcp: un puerto cerrado falla, y cancelar corta la espera") {
    u16 puerto = 0;
    const int fd = escuchar(puerto);
    REQUIRE(fd >= 0);
    ::close(fd);  // nadie escucha en ese puerto
    SocketTcp s;
    CHECK_FALSE(s.conectar("127.0.0.1", puerto, 2000));
    std::atomic<bool> cancelar{true};
    const auto t0 = std::chrono::steady_clock::now();
    CHECK_FALSE(s.conectar("127.0.0.1", puerto, 5000, &cancelar));
    CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(500));
}
