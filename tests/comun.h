// Utilidades de los tests. Los que usan la instalación del juego se saltan si LINEAGE_DIR no
// está definida (en CI no hay recursos del juego).
#pragma once
#include <cstdlib>
#include <string>

inline std::string carpeta_juego() {
    const char* d = std::getenv("LINEAGE_DIR");
    if (!d) return {};
    std::string s = d;
    if (!s.empty() && s.back() != '/') s += '/';
    return s;
}
