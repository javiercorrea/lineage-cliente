// Conexión con el servidor en hilos propios; el hilo principal nunca bloquea.
//
// - Hilo lector: lee [u16 longitud][cuerpo], descifra y encola el cuerpo en `entrada`.
//   El primer paquete (INITPACKET, sin cifrar) trae la semilla y crea ambos cifrados.
// - Hilo escritor: saca cuerpos de `salida`, los cifra y los envía.
// Cada cifrado lo usa un solo hilo, así que no hay locks sobre el estado criptográfico.
#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "core/anillo.h"
#include "plataforma/socket.h"
#include "proto/cifrado.h"
#include "proto/opcodes.h"

namespace lin {

enum class EstadoRed : int { INACTIVA, CONECTANDO, CONECTADA, CERRADA, ERROR };

class HiloRed {
public:
    ~HiloRed() { cerrar(); }
    void iniciar(const std::string& host, u16 puerto, const TablaOpcodes& tabla);
    void cerrar();
    // Hilo principal.
    bool sacar(std::vector<u8>& cuerpo) { return entrada_.sacar(cuerpo); }
    void enviar(Bytes cuerpo);
    EstadoRed estado() const { return estado_.load(); }
    const std::string& error() const { return error_; }

private:
    void leer();
    void escribir();

    SocketTcp socket_;
    TablaOpcodes tabla_{};
    std::string host_, error_;
    u16 puerto_ = 0;
    std::unique_ptr<Cifrado> rx_, tx_;
    AnilloBytes entrada_{1u << 22};
    AnilloBytes salida_{1u << 18};
    std::thread lector_, escritor_;
    std::mutex mtx_;  // solo para despertar al escritor
    std::condition_variable cv_;
    std::atomic<EstadoRed> estado_{EstadoRed::INACTIVA};
    std::atomic<bool> parar_{false};
};

}  // namespace lin
