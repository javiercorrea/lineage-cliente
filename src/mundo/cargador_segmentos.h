// Carga de segmentos de mapa (.s32) en un hilo aparte. Leer y decodificar uno toma 0,5–1 ms, y al
// cruzar a una zona nueva entraban varios en el mismo frame (hasta ~5 ms). El render los pide
// con margen fuera de la pantalla (segmento_cargado) y los recibe uno o dos frames después.
// Los pedidos son pocos (uno por segmento nuevo): una cola con mutex alcanza.
#pragma once
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "recursos/mapa.h"

namespace lin {

class CargadorSegmentos {
public:
    struct Hecho {
        u64 clave;
        bool ok;  // false: no existe en el disco
        SegmentoMapa s;
    };

    CargadorSegmentos();
    ~CargadorSegmentos();
    CargadorSegmentos(const CargadorSegmentos&) = delete;
    CargadorSegmentos& operator=(const CargadorSegmentos&) = delete;

    void pedir(std::string ruta, u64 clave, int base_x, int base_y);
    // Agrega a `salida` los segmentos terminados desde la última llamada.
    void recoger(std::vector<Hecho>& salida);

private:
    struct Pedido {
        std::string ruta;
        u64 clave;
        int base_x, base_y;
    };
    void bucle();

    std::mutex mu_;
    std::condition_variable cv_;
    std::vector<Pedido> pedidos_;
    std::vector<Hecho> hechos_;
    bool salir_ = false;
    std::thread hilo_;
};

}  // namespace lin
