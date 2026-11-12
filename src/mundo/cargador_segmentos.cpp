#include "mundo/cargador_segmentos.h"
#include "plataforma/archivo_mapeado.h"

namespace lin {

CargadorSegmentos::CargadorSegmentos() : hilo_([this] { bucle(); }) {}

CargadorSegmentos::~CargadorSegmentos() {
    {
        std::lock_guard l(mu_);
        salir_ = true;
    }
    cv_.notify_one();
    hilo_.join();
}

void CargadorSegmentos::pedir(std::string ruta, u64 clave, int base_x, int base_y) {
    {
        std::lock_guard l(mu_);
        pedidos_.push_back({std::move(ruta), clave, base_x, base_y});
    }
    cv_.notify_one();
}

void CargadorSegmentos::recoger(std::vector<Hecho>& salida) {
    std::lock_guard l(mu_);
    for (Hecho& h : hechos_) salida.push_back(std::move(h));
    hechos_.clear();
}

void CargadorSegmentos::bucle() {
    std::vector<u8> datos;
    for (;;) {
        Pedido p;
        {
            std::unique_lock l(mu_);
            cv_.wait(l, [&] { return salir_ || !pedidos_.empty(); });
            if (salir_) return;
            p = std::move(pedidos_.front());
            pedidos_.erase(pedidos_.begin());
        }
        Hecho h{p.clave, false, {}};
        h.ok = leer_archivo(p.ruta, datos) && decodificar_segmento(p.ruta, datos, p.base_x, p.base_y, h.s);
        std::lock_guard l(mu_);
        hechos_.push_back(std::move(h));
    }
}

}  // namespace lin
