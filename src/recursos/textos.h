// Tablas de texto .tbl de Text.pak (descifradas con DES): primera línea = cantidad, luego una
// entrada por línea. desc-e.tbl resuelve los nombres "$N" que manda el servidor.
#pragma once
#include <string_view>
#include <vector>
#include "recursos/pak.h"

namespace lin {

class TablaTexto {
public:
    bool cargar(const Recursos& r, std::string_view nombre);
    size_t cantidad() const { return lineas_.size(); }
    std::string_view linea(size_t i) const { return i < lineas_.size() ? lineas_[i] : std::string_view{}; }
    // "$1422" -> entrada 1422; cualquier otro texto se devuelve igual.
    std::string_view resolver(std::string_view texto) const;

private:
    std::vector<u8> datos_;
    std::vector<std::string_view> lineas_;
};

}  // namespace lin
