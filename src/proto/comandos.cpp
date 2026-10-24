#include "proto/comandos.h"

namespace lin::cmd {

Escritor crear_personaje(const TablaOpcodes& t, std::string_view nombre, u8 clase, u8 sexo, const u8 stats[6]) {
    Escritor e(t, Cli::NEWCHAR);
    e.s(nombre).c(clase).c(sexo);
    for (int i = 0; i < 6; ++i) e.c(stats[i]);
    return e;
}

}  // namespace lin::cmd
