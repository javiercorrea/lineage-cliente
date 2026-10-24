// Precarga de archivos de los .pak en un hilo aparte.
//
// Los .pak están mapeados en memoria (Tile.pak 450 MB, Sprite*.pak ~5 GB): la primera vez que se
// lee un .til o un .spr, el sistema lo trae del disco y quien lo lee espera. En el hilo principal
// eso eran ~50 µs por archivo, 20–40 ms en el primer frame de una zona y frames de 6–9 ms al
// caminar. Ahora el hilo principal pide el archivo (listo() devuelve false), lo sigue dibujando
// sin él y lo decodifica cuando este hilo ya tocó sus páginas, uno o dos frames después. El suelo
// y los objetos del mapa se piden en un margen fuera de la pantalla, así que no se nota.
//
// Las colas son SPSC sin locks (AnilloBytes) y la memoria se reserva al crearla.
#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include "core/anillo.h"
#include "core/mapa_hash.h"

namespace lin {

class Precarga {
public:
    Precarga();
    ~Precarga();
    Precarga(const Precarga&) = delete;
    Precarga& operator=(const Precarga&) = delete;

    // ¿Ya está en memoria el archivo `clave` (sus datos son `datos`, dentro de un mapeo)? La
    // primera vez lo pide al hilo y devuelve false; true desde que el hilo terminó.
    bool listo(u64 clave, Bytes datos);
    // Al comienzo de cada frame (hilo principal): marca los pedidos terminados.
    void actualizar();
    u32 en_curso() const { return en_curso_; }

private:
    void bucle();

    MapaHash estado_{4096};  // clave -> EN_CURSO o LISTO
    AnilloBytes pedidos_{1u << 16}, hechos_{1u << 16};
    std::vector<u8> msg_;    // del hilo principal
    u32 en_curso_ = 0;
    std::atomic<u32> senal_{0};
    std::atomic<bool> salir_{false};
    std::thread hilo_;
};

}  // namespace lin
