#include "render/atlas.h"
#include <algorithm>

namespace lin {

EmpaquetadoAtlas::EmpaquetadoAtlas(u16 tam_pagina, u16 paginas)
    : tam_(tam_pagina), paginas_(paginas), estantes_(paginas), y_libre_(paginas, 0), uso_(paginas, 0), dibujos_(paginas, 0) {}

void EmpaquetadoAtlas::vaciar(u16 p) {
    actual_ = p;
    estantes_[p].clear();
    y_libre_[p] = 0;
    reemplazada_ = p;
    ++reemplazos_;
}

void EmpaquetadoAtlas::fin_de_frame() {
    if (falta_lugar_ && paginas_ > 1) {
        u16 victima = 1;
        for (u16 p = 2; p < paginas_; ++p)
            if (dibujos_[p] < dibujos_[victima]) victima = p;
        vaciar(victima);
    }
    falta_lugar_ = false;
    std::fill(dibujos_.begin(), dibujos_.end(), 0u);
}

// Un estante de altura parecida con lugar a la derecha, o uno nuevo debajo.
bool EmpaquetadoAtlas::en_pagina(u16 p, u16 w, u16 h, RegionAtlas& r) {
    Estante* elegido = nullptr;
    for (Estante& e : estantes_[p]) {
        if (e.h >= h && e.h <= h + h / 2 + 4 && e.x + w <= tam_) {
            elegido = &e;
            break;
        }
    }
    if (!elegido) {
        if (y_libre_[p] + h > tam_) return false;
        estantes_[p].push_back({y_libre_[p], h, 0});
        y_libre_[p] = static_cast<u16>(y_libre_[p] + h);
        elegido = &estantes_[p].back();
    }
    r.pagina = p;
    r.x = elegido->x;
    r.y = elegido->y;
    elegido->x = static_cast<u16>(elegido->x + w);
    return true;
}

bool EmpaquetadoAtlas::reservar(u16 w, u16 h, u64 frame, RegionAtlas& r) {
    if (!cabe(w, h)) return false;
    w = static_cast<u16>(w + 1);  // 1 px de margen
    h = static_cast<u16>(h + 1);
    if (!en_pagina(actual_, w, h, r)) {
        if (abiertas_ < paginas_) {
            actual_ = static_cast<u16>(abiertas_++);
        } else {
            // Todas llenas: la menos usada recientemente, salvo la 0 y las de este frame.
            int victima = -1;
            for (u16 p = 1; p < paginas_; ++p)
                if (uso_[p] < frame && (victima < 0 || uso_[p] < uso_[victima])) victima = p;
            if (victima < 0) {
                falta_lugar_ = true;  // fin_de_frame hace lugar para el frame siguiente
                return false;
            }
            vaciar(static_cast<u16>(victima));
        }
        if (!en_pagina(actual_, w, h, r)) return false;
    }
    uso_[actual_] = frame;
    return true;
}

}  // namespace lin
