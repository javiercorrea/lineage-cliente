#include <charconv>
#include "mundo/clases.h"

namespace lin {

TablaClases clases_estandar() {
    // Atributos y reglas de C_CreateChar.java (L1J-JP); topes = maxFixedStats del servidor (iguales entre
    // versiones); botones y posiciones de drawmode/draw_new_char.cpp y animaciones de draw_char_sel.cpp de l1j-client.
    static constexpr Clase ESTANDAR[7] = {
        {{13, 10, 10, 11, 13, 10}, {20, 18, 18, 18, 18, 18}, 8, 1753, 332, 11, {{714, 86, 799, 801}, {629, 82, 711, 713}}},
        {{16, 12, 14, 9, 12, 8}, {20, 16, 18, 13, 16, 12}, 4, 1755, 542, 11, {{378, 71, 449, 451}, {315, 60, 375, 377}}},
        {{11, 12, 12, 12, 9, 12}, {18, 18, 18, 18, 16, 18}, 7, 1757, 332, 67, {{245, 67, 312, 314}, {166, 76, 242, 244}}},
        {{8, 7, 12, 12, 8, 12}, {20, 18, 18, 18, 18, 18}, 16, 1759, 542, 67, {{531, 95, 626, 628}, {452, 76, 528, 530}}},
        {{12, 15, 8, 10, 9, 11}, {18, 18, 18, 18, 18, 18}, 10, 1761, 332, 118, {{90, 73, 163, 165}, {25, 62, 87, 89}}},
        {{13, 11, 14, 12, 8, 11}, {19, 16, 18, 17, 14, 17}, 6, 1749, 542, 118, {{841, 65, 906, 907}, {908, 58, 966, 967}}},
        {{11, 10, 12, 12, 8, 12}, {19, 16, 18, 18, 18, 18}, 10, 1751, 332, 166, {{969, 68, 1037, 1038}, {1039, 87, 1126, 1127}}},
    };
    TablaClases t;
    for (int i = 0; i < 7; ++i) t.c[i] = ESTANDAR[i];
    t.n = 7;
    return t;
}

std::string_view separar_extension(std::string_view protocolo, std::string_view& extension) {
    const size_t mas = protocolo.find('+');
    extension = mas == std::string_view::npos ? std::string_view{} : protocolo.substr(mas + 1);
    return protocolo.substr(0, mas);
}

namespace {

std::string_view recortar(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

// Lee exactamente `n` enteros separados por comas.
bool numeros(std::string_view s, int* salida, int n) {
    for (int i = 0; i < n; ++i) {
        s = recortar(s);
        auto [fin, err] = std::from_chars(s.data(), s.data() + s.size(), salida[i]);
        if (err != std::errc{}) return false;
        s = recortar(s.substr(static_cast<size_t>(fin - s.data())));
        if (i + 1 < n) {
            if (s.empty() || s.front() != ',') return false;
            s.remove_prefix(1);
        }
    }
    return s.empty();
}

bool aplicar(Clase& k, std::string_view clave, std::string_view valor, bool& con_tope) {
    int v[6];
    if (clave == "base" || clave == "tope") {
        if (!numeros(valor, v, 6)) return false;
        u8* d = clave == "base" ? k.base : k.tope;
        for (int i = 0; i < 6; ++i) d[i] = static_cast<u8>(v[i]);
        if (clave == "tope") con_tope = true;
    } else if (clave == "bonus" || clave == "boton") {
        if (!numeros(valor, v, 1)) return false;
        if (clave == "bonus") k.bonus = static_cast<u8>(v[0]);
        else k.boton = static_cast<u32>(v[0]);
    } else if (clave == "posicion") {
        if (!numeros(valor, v, 2)) return false;
        k.x = static_cast<i16>(v[0]);
        k.y = static_cast<i16>(v[1]);
    } else if (clave == "hombre" || clave == "mujer") {
        if (!numeros(valor, v, 4)) return false;
        k.anim[clave == "mujer"] = {static_cast<u16>(v[0]), static_cast<u16>(v[1]), static_cast<u16>(v[2]), static_cast<u16>(v[3])};
    } else {
        return false;  // clave desconocida: mejor avisar que ignorar un error de tipeo
    }
    return true;
}

}  // namespace

bool cargar_clases_ini(std::string_view texto, TablaClases& t) {
    Clase* k = nullptr;
    bool con_tope = false;
    auto cerrar = [&] {  // sin "tope=" no hay más límite que los puntos libres
        if (k && !con_tope)
            for (int i = 0; i < 6; ++i) k->tope[i] = static_cast<u8>(k->base[i] + k->bonus);
    };
    while (!texto.empty()) {
        const size_t fin = texto.find('\n');
        std::string_view linea = texto.substr(0, fin);
        texto = fin == std::string_view::npos ? std::string_view{} : texto.substr(fin + 1);
        linea = recortar(linea.substr(0, linea.find(';')));
        if (linea.empty()) continue;
        if (linea.front() == '[') {
            int numero;
            if (linea.back() != ']' || !linea.starts_with("[clase ") || !numeros(linea.substr(7, linea.size() - 8), &numero, 1) ||
                numero < 0 || numero >= CLASES_MAX || numero > t.n)  // sin huecos: la clase N necesita la N-1
                return false;
            cerrar();
            k = &t.c[numero];
            *k = {};
            con_tope = false;
            if (numero == t.n) ++t.n;
            continue;
        }
        const size_t igual = linea.find('=');
        if (!k || igual == std::string_view::npos) return false;
        if (!aplicar(*k, recortar(linea.substr(0, igual)), linea.substr(igual + 1), con_tope)) return false;
    }
    cerrar();
    return true;
}

}  // namespace lin
