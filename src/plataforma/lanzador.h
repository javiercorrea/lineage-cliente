// Lanzador de un servidor: un comando del usuario que abre un proxy local hacia el servidor e imprime su puerto
// (ver Servidor::lanzador).
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include "core/tipos.h"

namespace lin {

class Lanzador {
public:
    // Plazo para que el comando dé el puerto; después se da por fallido y se puede reintentar.
    static constexpr u32 PLAZO_MS = 30000;
    // Plazo de "<comando> stop" al salir: el cierre del cliente no espera más que esto.
    static constexpr u32 PLAZO_STOP_MS = 2000;

    // Corre el comando en segundo plano. Si había uno anterior, antes corre "<anterior> stop" en el mismo hilo (así un
    // proxy de una sesión anterior no queda abierto, y el del mismo servidor no se para mientras arranca de nuevo).
    void iniciar(const std::string& comando);
    bool en_curso() const { return estado_ && !estado_->listo.load(); }
    // True una vez, cuando hay respuesta: puerto es el que imprimió (0 = no dio ninguno o venció el plazo).
    bool terminado(u16& puerto);
    // Corre "<comando> stop" del último iniciado, esperando a lo sumo PLAZO_STOP_MS, y lo olvida.
    void parar();

private:
    struct Estado {
        std::atomic<bool> listo{false};
        std::atomic<u16> puerto{0};
        u64 inicio_ms = 0;
    };
    std::shared_ptr<Estado> estado_;
    std::string comando_;  // el último iniciado, para el "stop"
};

// Puerto en la salida de un lanzador: el número que sigue a la palabra "port" ("port 25134", "port: 7001"), si no el que
// sigue a una dirección ("127.0.0.1:7001 (pid 4242)", "localhost:7001"), y si no el último número de 1 a 65535; 0 si no hay.
u16 puerto_en_salida(std::string_view salida);
// Solo las dos formas marcadas ("port N" o "dirección:N"): con ellas no hace falta esperar al final de la salida.
u16 puerto_marcado(std::string_view salida);

}  // namespace lin
