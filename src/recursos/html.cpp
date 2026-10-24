#include "recursos/html.h"
#include <cctype>
#include "recursos/fuente.h"

namespace lin {

namespace {

u32 rgba_hex(std::string_view hex) {
    u32 v = 0;
    for (char c : hex.substr(0, 6)) {
        int d = std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : (std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
        if (d < 0 || d > 15) return COLOR_TEXTO_HTML;
        v = v * 16 + static_cast<u32>(d);
    }
    if (hex.size() < 6) return COLOR_TEXTO_HTML;
    return ((v >> 16) & 0xFF) | (v & 0xFF00) | ((v & 0xFF) << 16) | 0xFF000000u;
}

// Valor del atributo `nombre` dentro de una etiqueta ("a action=\"buy\"", "font fg=ffffff").
std::string_view atributo(std::string_view tag, std::string_view nombre) {
    for (size_t p = 0; (p = tag.find(nombre, p)) != std::string_view::npos; p += nombre.size()) {
        if (p > 0 && !std::isspace(static_cast<unsigned char>(tag[p - 1]))) continue;
        size_t q = p + nombre.size();
        while (q < tag.size() && tag[q] == ' ') ++q;
        if (q >= tag.size() || tag[q] != '=') continue;
        ++q;
        while (q < tag.size() && tag[q] == ' ') ++q;
        if (q < tag.size() && (tag[q] == '"' || tag[q] == '\'')) {
            char comilla = tag[q++];
            size_t fin = tag.find(comilla, q);
            return tag.substr(q, fin == std::string_view::npos ? std::string_view::npos : fin - q);
        }
        size_t fin = q;
        while (fin < tag.size() && !std::isspace(static_cast<unsigned char>(tag[fin]))) ++fin;
        return tag.substr(q, fin - q);
    }
    return {};
}

// "#12" -> 12
u32 numero_src(std::string_view s) {
    u32 v = 0;
    for (char c : s)
        if (c >= '0' && c <= '9') v = v * 10 + static_cast<u32>(c - '0');
    return v;
}

class Maquetador {
public:
    Maquetador(PaginaHtml& p, int ancho, MedirImagen medir, void* ctx) : p_(p), ancho_(ancho), medir_(medir), ctx_(ctx) {}

    void color(u32 c) {
        if (n_colores_ < 16) colores_[n_colores_++] = c;
    }
    void fin_color() {
        if (n_colores_ > 1) --n_colores_;
    }
    void vinculo(std::string_view v, bool es_enlace) {
        if (v.empty()) {
            vinculo_len_ = 0;
            return;
        }
        vinculo_ofs_ = guardar(v);
        vinculo_len_ = static_cast<u32>(v.size());
        es_enlace_ = es_enlace;
    }
    void espacio() { espacio_ = true; }
    void centrar(bool c) {
        if (x_ > 0) nueva_linea();
        centrado_ = c;
    }
    void salto() { nueva_linea(true); }

    void palabra(std::string_view t) {
        if (t.empty()) return;
        int w = ancho_texto(t.data(), t.size());
        int sp = (espacio_ && x_ > 0) ? ancho_texto(" ", 1) : 0;
        espacio_ = false;
        if (x_ > 0 && x_ + sp + w > ancho_) {
            nueva_linea();
            sp = 0;
        }
        u32 col = vinculo_len_ ? COLOR_VINCULO_HTML : colores_[n_colores_ - 1];
        // Se junta con la palabra anterior si comparte color y vínculo (menos elementos que dibujar).
        if (!p_.elementos.empty() && p_.elementos.size() > inicio_linea_) {
            ElementoHtml& u = p_.elementos.back();
            if (u.tipo == ElementoHtml::Tipo::TEXTO && u.color == col && u.vinculo_len == vinculo_len_ &&
                u.vinculo_ofs == vinculo_ofs_ && u.texto_ofs + u.texto_len == p_.textos.size() && u.x + u.w == x_) {
                if (sp) p_.textos.push_back(' ');
                p_.textos.append(t);
                u.texto_len += static_cast<u32>(t.size()) + (sp ? 1 : 0);
                u.w = static_cast<i16>(u.w + sp + w);
                x_ += sp + w;
                return;
            }
        }
        ElementoHtml e;
        e.tipo = ElementoHtml::Tipo::TEXTO;
        e.x = static_cast<i16>(x_ + sp);
        e.y = static_cast<i16>(y_);
        e.w = static_cast<i16>(w);
        e.h = ALTO_GLIFO;
        e.color = col;
        e.texto_ofs = guardar(t);
        e.texto_len = static_cast<u32>(t.size());
        poner_vinculo(e);
        p_.elementos.push_back(e);
        x_ += sp + w;
    }

    void imagen(u32 n, std::string_view accion, std::string_view enlace, std::string_view ayuda) {
        int w = 0, h = 0;
        if (!medir_ || !medir_(ctx_, n, w, h)) return;
        int sp = (espacio_ && x_ > 0) ? ancho_texto(" ", 1) : 0;
        espacio_ = false;
        if (x_ > 0 && x_ + sp + w > ancho_) {
            nueva_linea();
            sp = 0;
        }
        ElementoHtml e;
        e.tipo = ElementoHtml::Tipo::IMAGEN;
        e.x = static_cast<i16>(x_ + sp);
        e.y = static_cast<i16>(y_);
        e.w = static_cast<i16>(w);
        e.h = static_cast<i16>(h);
        e.imagen = n;
        if (!accion.empty() || !enlace.empty()) {
            std::string_view v = accion.empty() ? enlace : accion;
            e.vinculo_ofs = guardar(v);
            e.vinculo_len = static_cast<u32>(v.size());
            e.es_enlace = accion.empty();
        } else {
            poner_vinculo(e);
        }
        if (!ayuda.empty()) {
            e.ayuda_ofs = guardar(ayuda);
            e.ayuda_len = static_cast<u32>(ayuda.size());
        }
        p_.elementos.push_back(e);
        x_ += sp + w;
        alto_linea_ = std::max(alto_linea_, h + 2);
    }

    void terminar() {
        if (x_ > 0) nueva_linea();
        p_.alto = y_;
    }

private:
    u32 guardar(std::string_view t) {
        u32 ofs = static_cast<u32>(p_.textos.size());
        p_.textos.append(t);
        return ofs;
    }
    void poner_vinculo(ElementoHtml& e) const {
        e.vinculo_ofs = vinculo_ofs_;
        e.vinculo_len = vinculo_len_;
        e.es_enlace = es_enlace_;
    }
    // Cierra la línea (centrándola si hace falta). `forzar`: un <br> en una línea vacía también baja.
    void nueva_linea(bool forzar = false) {
        if (x_ == 0 && !forzar) return;
        if (centrado_ && x_ < ancho_) {
            i16 d = static_cast<i16>((ancho_ - x_) / 2);
            for (size_t i = inicio_linea_; i < p_.elementos.size(); ++i) p_.elementos[i].x = static_cast<i16>(p_.elementos[i].x + d);
        }
        // Las imágenes más altas que el texto bajan la línea: el texto se alinea abajo con ellas.
        for (size_t i = inicio_linea_; i < p_.elementos.size(); ++i)
            if (p_.elementos[i].tipo == ElementoHtml::Tipo::TEXTO)
                p_.elementos[i].y = static_cast<i16>(y_ + (alto_linea_ - ALTO_LINEA_HTML) / 2);
        y_ += alto_linea_;
        x_ = 0;
        alto_linea_ = ALTO_LINEA_HTML;
        inicio_linea_ = p_.elementos.size();
        espacio_ = false;
    }

    PaginaHtml& p_;
    int ancho_;
    MedirImagen medir_;
    void* ctx_;
    int x_ = 0, y_ = 0, alto_linea_ = ALTO_LINEA_HTML;
    size_t inicio_linea_ = 0;
    bool centrado_ = false, espacio_ = false, es_enlace_ = false;
    u32 colores_[16] = {COLOR_TEXTO_HTML};
    int n_colores_ = 1;
    u32 vinculo_ofs_ = 0, vinculo_len_ = 0;
};

// El dato N (separados por `sep`).
std::string_view dato(std::string_view datos, char sep, u32 n) {
    for (u32 i = 0; i < n; ++i) {
        size_t p = datos.find(sep);
        if (p == std::string_view::npos) return {};
        datos.remove_prefix(p + 1);
    }
    return datos.substr(0, datos.find(sep));
}

}  // namespace

void maquetar_html(std::string_view html, std::string_view datos, char separador, std::string_view usuario, int ancho,
                   MedirImagen medir, void* contexto, PaginaHtml& salida) {
    salida.limpiar();
    Maquetador m(salida, ancho, medir, contexto);
    std::string palabra;
    auto cortar = [&] {
        m.palabra(palabra);
        palabra.clear();
    };
    // Texto suelto (de la página o de un dato): palabras separadas por espacios.
    auto texto = [&](std::string_view t) {
        for (char c : t) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                cortar();
                m.espacio();
            } else {
                palabra.push_back(c);
            }
        }
    };
    for (size_t i = 0; i < html.size();) {
        char c = html[i];
        if (c == '<') {
            size_t fin = html.find('>', i);
            if (fin == std::string_view::npos) break;
            std::string_view tag = html.substr(i + 1, fin - i - 1);
            i = fin + 1;
            bool cierre = !tag.empty() && tag[0] == '/';
            if (cierre) tag.remove_prefix(1);
            size_t fin_nombre = 0;
            while (fin_nombre < tag.size() && std::isalpha(static_cast<unsigned char>(tag[fin_nombre]))) ++fin_nombre;
            char nombre[16] = {};
            for (size_t k = 0; k < fin_nombre && k < 15; ++k) nombre[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(tag[k])));
            std::string_view n(nombre);
            if (n == "br") {
                cortar();
                m.salto();
            } else if (n == "p") {
                cortar();
                m.centrar(!cierre && atributo(tag, "align") == "center");
            } else if (n == "center") {
                cortar();
                m.centrar(!cierre);
            } else if (n == "font" || n == "fong") {
                cortar();
                if (cierre) {
                    m.fin_color();
                } else {
                    std::string_view fg = atributo(tag, "fg");
                    m.color(fg.empty() ? COLOR_TEXTO_HTML : rgba_hex(fg));
                }
            } else if (n == "a") {
                cortar();
                if (cierre) {
                    m.vinculo({}, false);
                } else {
                    std::string_view accion = atributo(tag, "action");
                    std::string_view enlace = atributo(tag, "link");
                    if (enlace.empty()) enlace = atributo(tag, "href");
                    m.vinculo(accion.empty() ? enlace : accion, accion.empty());
                }
            } else if (n == "img" && !cierre) {
                cortar();
                m.imagen(numero_src(atributo(tag, "src")), atributo(tag, "action"), atributo(tag, "link"), atributo(tag, "tooltip"));
            } else if (n == "var" && !cierre) {
                if (tag.find("img") != std::string_view::npos) {
                    cortar();
                    m.imagen(numero_src(atributo(tag, "src")), atributo(tag, "action"), atributo(tag, "link"), {});
                } else {
                    texto(dato(datos, separador, numero_src(atributo(tag, "src"))));
                }
            } else if (n == "username" && !cierre) {
                texto(usuario);
            }
            continue;
        }
        if (c == '&') {
            size_t fin = html.find(';', i);
            if (fin != std::string_view::npos && fin - i <= 6) {
                std::string_view ent = html.substr(i + 1, fin - i - 1);
                i = fin + 1;
                if (ent == "nbsp") palabra.push_back(' ');  // espacio que no se junta ni corta la línea
                else if (ent == "amp") palabra.push_back('&');
                else if (ent == "lt") palabra.push_back('<');
                else if (ent == "gt") palabra.push_back('>');
                else if (ent == "quot") palabra.push_back('"');
                continue;
            }
        }
        texto({&html[i], 1});
        ++i;
    }
    cortar();
    m.terminar();
}

}  // namespace lin
