// Empaquetado del atlas: dónde va cada imagen dentro de las páginas y qué página se reemplaza
// cuando no hay más lugar. Sin GPU (Gpu lo usa para subir; se prueba solo en los tests).
//
// - Cada página se llena por estantes (filas de altura parecida), de arriba hacia abajo.
// - Reemplazo LRU por página: se recuerda en qué frame se dibujó por última vez algo de cada página.
//   Con todas llenas, se vacía la que lleva más tiempo sin usarse, nunca una usada en el frame
//   actual (sus instancias ya están en la lista de dibujo) ni la página 0 (el blanco de los
//   rectángulos, los glifos y la interfaz del inicio quedan ahí). Quien guarda regiones (Graficos)
//   tiene que olvidar las de la página reemplazada: ver tomar_reemplazada().
#pragma once
#include <vector>
#include "core/tipos.h"

namespace lin {

struct RegionAtlas {
    u16 pagina = 0, x = 0, y = 0, w = 0, h = 0;
    i16 ox = 0, oy = 0;  // desplazamiento de dibujo de la imagen original
    bool valida() const { return w != 0; }
};

class EmpaquetadoAtlas {
public:
    EmpaquetadoAtlas(u16 tam_pagina, u16 paginas);

    // Lugar para una imagen de w×h (con 1 px de margen). false si no entra en ninguna página
    // (todas llenas y usadas en este frame, o la imagen es más grande que una página).
    bool reservar(u16 w, u16 h, u64 frame, RegionAtlas& r);
    // ¿Entra alguna vez una imagen de w×h (con su margen) en una página vacía?
    bool cabe(u32 w, u32 h) const { return w + 1 <= tam_ && h + 1 <= tam_; }
    // Algo de `pagina` se dibuja en `frame`.
    void usar(u16 pagina, u64 frame) { uso_[pagina] = frame; }
    // Página vaciada por el último reservar (-1 si ninguna). Se consulta después de cada reservar.
    int tomar_reemplazada() {
        int p = reemplazada_;
        reemplazada_ = -1;
        return p;
    }
    // Páginas con algo (las que ya se empezaron a llenar).
    u32 paginas_usadas() const { return abiertas_; }
    u32 reemplazos() const { return reemplazos_; }

private:
    struct Estante {
        u16 y, h, x;
    };
    bool en_pagina(u16 p, u16 w, u16 h, RegionAtlas& r);

    u16 tam_, paginas_;
    std::vector<std::vector<Estante>> estantes_;
    std::vector<u16> y_libre_;
    std::vector<u64> uso_;
    u16 actual_ = 0;
    u32 abiertas_ = 1;
    int reemplazada_ = -1;
    u32 reemplazos_ = 0;
};

}  // namespace lin
