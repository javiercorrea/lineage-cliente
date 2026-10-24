// Tabla columnar (struct of arrays) indexada por ObjId del servidor.
//
// Cada columna es un std::vector contiguo. El borrado intercambia la fila con la última
// (swap-remove) para que la tabla quede siempre compacta y los sistemas recorran 0..n-1 sin
// huecos. El mapa ObjId -> Slot se mantiene consistente en cada borrado.
//
//   TablaSoa<u16, u16, u8> t;           // columnas x, y, heading
//   Slot s = t.insertar(id);
//   t.col<0>()[s] = 32691;
#pragma once
#include <tuple>
#include <vector>
#include "core/mapa_hash.h"

namespace lin {

template <typename... Cols>
class TablaSoa {
public:
    size_t tamano() const { return ids_.size(); }
    const std::vector<ObjId>& ids() const { return ids_; }

    template <size_t I>
    auto& col() { return std::get<I>(cols_); }
    template <size_t I>
    const auto& col() const { return std::get<I>(cols_); }

    Slot buscar(ObjId id) const {
        const u32* s = indice_.buscar(id);
        return s ? *s : SIN_SLOT;
    }

    // Inserta una fila con valores por defecto; si el id ya existe devuelve su slot.
    Slot insertar(ObjId id) {
        Slot s = buscar(id);
        if (s != SIN_SLOT) return s;
        s = static_cast<Slot>(ids_.size());
        ids_.push_back(id);
        std::apply([](auto&... c) { (c.emplace_back(), ...); }, cols_);
        indice_.poner(id, s);
        return s;
    }

    bool quitar(ObjId id) {
        Slot s = buscar(id);
        if (s == SIN_SLOT) return false;
        Slot ultimo = static_cast<Slot>(ids_.size() - 1);
        if (s != ultimo) {
            ids_[s] = ids_[ultimo];
            std::apply([&](auto&... c) { ((c[s] = std::move(c[ultimo])), ...); }, cols_);
            indice_.poner(ids_[s], s);
        }
        ids_.pop_back();
        std::apply([](auto&... c) { (c.pop_back(), ...); }, cols_);
        indice_.quitar(id);
        return true;
    }

    void limpiar() {
        ids_.clear();
        std::apply([](auto&... c) { (c.clear(), ...); }, cols_);
        indice_.limpiar();
    }

private:
    std::vector<ObjId> ids_;
    std::tuple<std::vector<Cols>...> cols_;
    MapaHash indice_;
};

}  // namespace lin
