#include "proto/hilo_red.h"

namespace lin {

void HiloRed::iniciar(const std::string& host, u16 puerto, const TablaOpcodes& tabla) {
    cerrar();
    host_ = host;
    puerto_ = puerto;
    tabla_ = tabla;
    error_.clear();
    parar_ = false;
    estado_ = EstadoRed::CONECTANDO;
    lector_ = std::thread(&HiloRed::leer, this);
}

void HiloRed::cerrar() {
    parar_ = true;
    socket_.cerrar();  // desbloquea recv
    cv_.notify_all();
    if (lector_.joinable()) lector_.join();
    if (escritor_.joinable()) escritor_.join();
    if (estado_ != EstadoRed::INACTIVA) estado_ = EstadoRed::CERRADA;
}

void HiloRed::enviar(Bytes cuerpo) {
    salida_.empujar(cuerpo);
    cv_.notify_one();
}

void HiloRed::leer() {
    if (!socket_.conectar(host_, puerto_)) {
        error_ = "No se pudo conectar a " + host_ + ":" + std::to_string(puerto_);
        estado_ = EstadoRed::ERROR;
        return;
    }
    std::vector<u8> buf(65536);
    bool primero = true;
    while (!parar_) {
        u8 cab[2];
        if (!socket_.leer_exacto(cab, 2)) break;
        size_t n = static_cast<size_t>(cab[0] | (cab[1] << 8));
        if (n < 2) break;
        n -= 2;
        if (!socket_.leer_exacto(buf.data(), n)) break;
        if (primero) {
            // INITPACKET: [opcode][u32 semilla][...] sin cifrar
            if (n < 5 || buf[0] != tabla_.cable_init) {
                error_ = "El servidor no envió el paquete inicial esperado";
                estado_ = EstadoRed::ERROR;
                socket_.cerrar();
                return;
            }
            u32 semilla = static_cast<u32>(buf[1]) | (static_cast<u32>(buf[2]) << 8) | (static_cast<u32>(buf[3]) << 16) |
                          (static_cast<u32>(buf[4]) << 24);
            rx_ = std::make_unique<Cifrado>(semilla);
            tx_ = std::make_unique<Cifrado>(semilla);
            primero = false;
            escritor_ = std::thread(&HiloRed::escribir, this);
            estado_ = EstadoRed::CONECTADA;
            continue;
        }
        rx_->descifrar(buf.data(), n);
        while (!entrada_.empujar({buf.data(), n}) && !parar_) std::this_thread::yield();
    }
    if (!parar_) {
        if (error_.empty()) error_ = "El servidor cerró la conexión";
        estado_ = EstadoRed::CERRADA;
    }
    cv_.notify_all();
}

void HiloRed::escribir() {
    std::vector<u8> cuerpo, trama;
    while (!parar_) {
        if (!salida_.sacar(cuerpo)) {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait_for(lk, std::chrono::milliseconds(50));
            continue;
        }
        tx_->cifrar(cuerpo.data(), cuerpo.size());
        trama.resize(cuerpo.size() + 2);
        trama[0] = static_cast<u8>(trama.size());
        trama[1] = static_cast<u8>(trama.size() >> 8);
        std::memcpy(trama.data() + 2, cuerpo.data(), cuerpo.size());
        if (!socket_.escribir(trama.data(), trama.size())) break;
    }
}

}  // namespace lin
