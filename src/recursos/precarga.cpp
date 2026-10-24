#include "recursos/precarga.h"
#include "plataforma/archivo_mapeado.h"

namespace lin {

namespace {
constexpr u32 EN_CURSO = 0, LISTO = 1;

// Pedido: puntero y tamaño del tramo, y la clave a devolver al terminar.
struct Pedido {
    const u8* datos;
    size_t tam;
    u64 clave;
};
}  // namespace

Precarga::Precarga() : hilo_([this] { bucle(); }) { msg_.reserve(sizeof(Pedido)); }

Precarga::~Precarga() {
    salir_.store(true);
    senal_.fetch_add(1);
    senal_.notify_one();
    hilo_.join();
}

bool Precarga::listo(u64 clave, Bytes datos) {
    if (const u32* e = estado_.buscar(clave)) return *e == LISTO;
    Pedido p{datos.data(), datos.size(), clave};
    if (!pedidos_.empujar({reinterpret_cast<const u8*>(&p), sizeof p})) return false;  // cola llena: se reintenta
    estado_.poner(clave, EN_CURSO);
    ++en_curso_;
    senal_.fetch_add(1, std::memory_order_release);
    senal_.notify_one();
    return false;
}

void Precarga::actualizar() {
    while (hechos_.sacar(msg_)) {
        u64 clave;
        std::memcpy(&clave, msg_.data(), sizeof clave);
        estado_.poner(clave, LISTO);
        --en_curso_;
    }
}

void Precarga::bucle() {
    std::vector<u8> msg;
    msg.reserve(sizeof(Pedido));
    while (!salir_.load()) {
        u32 visto = senal_.load(std::memory_order_acquire);
        if (!pedidos_.sacar(msg)) {
            senal_.wait(visto);  // hasta el próximo pedido (o la salida)
            continue;
        }
        Pedido p;
        std::memcpy(&p, msg.data(), sizeof p);
        traer_a_memoria({p.datos, p.tam});
        // La cola de hechos tiene el mismo tamaño que la de pedidos: siempre hay lugar.
        while (!hechos_.empujar({reinterpret_cast<const u8*>(&p.clave), sizeof p.clave}) && !salir_.load())
            std::this_thread::yield();
    }
}

}  // namespace lin
