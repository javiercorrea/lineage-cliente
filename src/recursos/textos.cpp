#include "recursos/textos.h"

namespace lin {

bool TablaTexto::cargar(const Recursos& r, std::string_view nombre) {
    lineas_.clear();
    if (!r.texto(nombre, datos_)) return false;
    std::string_view t(reinterpret_cast<const char*>(datos_.data()), datos_.size());
    bool primera = true;
    while (!t.empty()) {
        size_t fin = t.find('\n');
        std::string_view l = t.substr(0, fin);
        t = fin == std::string_view::npos ? std::string_view{} : t.substr(fin + 1);
        while (!l.empty() && (l.back() == '\r' || l.back() == '\0')) l.remove_suffix(1);
        if (primera) { primera = false; continue; }  // cantidad
        lineas_.push_back(l);
    }
    return true;
}

std::string_view TablaTexto::resolver(std::string_view texto) const {
    if (texto.size() < 2 || texto[0] != '$') return texto;
    size_t n = 0;
    for (size_t i = 1; i < texto.size(); ++i) {
        if (texto[i] < '0' || texto[i] > '9') return texto;
        n = n * 10 + static_cast<size_t>(texto[i] - '0');
    }
    std::string_view r = linea(n);
    return r.empty() ? texto : r;
}

}  // namespace lin
