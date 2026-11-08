#include <cstdio>
#include "mundo/chat.h"
#include "recursos/fuente.h"

namespace lin {

namespace {

constexpr u32 color(u8 r, u8 g, u8 b) { return r | (g << 8) | (b << 16) | 0xFF000000u; }

constexpr u32 COLOR_MENSAJE = color(255, 255, 255);
constexpr u32 COLOR_AZUL = color(140, 180, 255);

// Códigos \fX de string-e.tbl. PENDIENTE: la paleta exacta está en lin.bin; estos colores son
// aproximados ("\f>" vuelve al color normal).
u32 color_codigo(char c, u32 normal) {
    switch (c) {
    case '1': return color(255, 100, 100);
    case '2': return color(130, 255, 130);
    case '3': return color(130, 180, 255);
    case '4': return color(255, 255, 130);
    case '5': return color(255, 130, 255);
    case '6': return color(130, 255, 255);
    case 'Q': return color(255, 220, 120);
    default: return normal;
    }
}

// Escritura acotada en un buffer de tamaño fijo.
struct Salida {
    char* p;
    size_t cap, n = 0;
    void poner(std::string_view t) {
        for (char c : t)
            if (n < cap) p[n++] = c;
    }
};

// Cantidad de bytes de `t` que entran en `cap` sin cortar un carácter CP949 de 2 bytes.
size_t recorte_cp949(std::string_view t, size_t cap) {
    size_t i = 0;
    while (i < t.size()) {
        size_t w = static_cast<u8>(t[i]) >= 0x81 && i + 1 < t.size() ? 2 : 1;
        if (i + w > cap) break;
        i += w;
    }
    return i;
}

}  // namespace

u32 color_chat(u8 tipo) {
    switch (tipo) {
    case CHAT_GRITO: return color(255, 170, 80);
    case CHAT_GLOBAL: return color(255, 255, 130);
    case CHAT_CLAN: return color(130, 255, 255);
    case CHAT_SISTEMA: return color(190, 220, 255);
    case CHAT_GRUPO:
    case CHAT_GRUPO_CHAT: return color(130, 255, 130);
    case CHAT_COMERCIO: return color(255, 200, 130);
    case CHAT_ALIANZA: return color(170, 200, 255);
    case CHAT_SUSURRO: return color(255, 150, 220);
    case CHAT_MENSAJE: return COLOR_MENSAJE;
    default: return color(255, 255, 255);
    }
}

void Chat::agregar(std::string_view t, u32 c, u8 tipo) {
    LineaChat& l = lineas[total % CAPACIDAD_CHAT];
    size_t n = recorte_cp949(t, LARGO_LINEA_CHAT);
    for (size_t i = 0; i < n; ++i) l.texto[i] = t[i];
    l.largo = static_cast<u8>(n);
    l.color = c;
    l.tipo = tipo;
    ++total;
}

std::string_view Chat::globo(ObjId id, u64 ahora) const {
    if (id == 0) return {};
    for (const GloboChat& g : globos) {
        if (g.id != id || ahora - g.inicio >= MS_GLOBO || total - g.linea > CAPACIDAD_CHAT) continue;
        const LineaChat& l = lineas[g.linea % CAPACIDAD_CHAT];
        return {l.texto, l.largo};
    }
    return {};
}

std::string sin_codigos_color(std::string_view t) {
    // Fuera de UTF-8 hay caracteres de dos bytes cuyo segundo byte puede ser una barra invertida.
    const bool doble_byte = !es_utf8(t.data(), t.size());
    std::string r;
    for (size_t i = 0; i < t.size(); ++i) {
        const char c = t[i];
        if (doble_byte && static_cast<u8>(c) >= 0x80 && i + 1 < t.size()) {
            r += c;
            r += t[++i];
            continue;
        }
        if (c == '\f' || c == '\b') {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < t.size() && t[j] == '\\') ++j;
        if (j > i && j < t.size() && (t[j] == 'f' || t[j] == 'b')) {
            i = j + 1;  // y el ++i del for salta la letra del color
            continue;
        }
        r += c;
    }
    return r;
}

size_t expandir_desc(const TablaTexto& desc, std::string_view t, char* destino, size_t capacidad) {
    Salida s{destino, capacidad};
    for (size_t i = 0; i < t.size();) {
        if (t[i] == '$' && i + 1 < t.size() && t[i + 1] >= '0' && t[i + 1] <= '9') {
            size_t f = i + 1;
            while (f < t.size() && t[f] >= '0' && t[f] <= '9') ++f;
            std::string_view r = desc.resolver(t.substr(i, f - i));
            s.poner(r);
            i = f;
        } else {
            s.poner(t.substr(i, 1));
            ++i;
        }
    }
    return s.n;
}

size_t formatear_mensaje(const TablaTexto& mensajes, const TablaTexto& desc, const Evento& e, char* destino,
                         size_t capacidad, u32& c) {
    u32 normal = e.v[MS_AZUL] ? COLOR_AZUL : COLOR_MENSAJE;
    c = normal;
    std::string_view plantilla = mensajes.linea(static_cast<size_t>(e.v[MS_ID]));
    Salida s{destino, capacidad};
    if (plantilla.empty()) {
        // Mensaje que esta instalación no tiene: se muestra el id y los argumentos.
        char t[24];
        std::snprintf(t, sizeof t, "(mensaje %d)", e.v[MS_ID]);
        s.poner(t);
        for (int i = 0; i < e.v[MS_NARGS]; ++i) {
            s.poner(" ");
            s.n += expandir_desc(desc, e.s[i], destino + s.n, capacidad - s.n);
        }
        return s.n;
    }
    bool con_color = false;
    for (size_t i = 0; i < plantilla.size(); ++i) {
        char ch = plantilla[i];
        if (ch == '\\' && i + 2 < plantilla.size() && plantilla[i + 1] == 'f') {
            if (!con_color) c = color_codigo(plantilla[i + 2], normal);
            con_color = true;
            i += 2;
        } else if (ch == '%' && i + 1 < plantilla.size() && (plantilla[i + 1] >= '0' && plantilla[i + 1] <= '9')) {
            int k = plantilla[i + 1] - '0';
            if (k < e.v[MS_NARGS]) s.n += expandir_desc(desc, e.s[k], destino + s.n, capacidad - s.n);
            ++i;
        } else if (ch == '%' && i + 1 < plantilla.size() && plantilla[i + 1] == 's') {
            if (e.v[MS_NARGS] > 0) s.n += expandir_desc(desc, e.s[0], destino + s.n, capacidad - s.n);  // "%s" = %0
            ++i;
        } else {
            s.poner(plantilla.substr(i, 1));
        }
    }
    return s.n;
}

void aplicar_chat(Chat& c, const TablaTexto& mensajes, const TablaTexto& desc, std::span<const Evento> eventos, u64 ahora) {
    char buf[LARGO_LINEA_CHAT * 2];
    for (const Evento& e : eventos) {
        if (e.tipo == Ev::MENSAJE_SERVIDOR) {
            u32 col;
            size_t n = formatear_mensaje(mensajes, desc, e, buf, sizeof buf, col);
            c.agregar({buf, n}, col, CHAT_MENSAJE);
        } else if (e.tipo == Ev::CHAT) {
            u8 tipo = static_cast<u8>(e.v[0]);
            size_t n;
            if (tipo == CHAT_SUSURRO) {
                // Llega solo el remitente y el texto. Se arma como el eco que recibe quien envía
                // ("-> (destino) texto", tipo 9) pero en sentido contrario.
                Salida s{buf, sizeof buf};
                s.poner("<- (");
                s.poner(e.s[1]);
                s.poner(") ");
                s.poner(e.s[0]);
                n = s.n;
            } else {
                n = expandir_desc(desc, e.s[0], buf, sizeof buf);  // los NPC hablan como "$1422: ..."
            }
            u32 linea = c.total;
            c.agregar({buf, n}, color_chat(tipo), tipo);
            // Globo sobre la cabeza: chat normal y grito de quien está a la vista.
            if (e.id != 0 && (tipo == CHAT_NORMAL || tipo == CHAT_GRITO)) {
                GloboChat* destino = &c.globos[0];
                for (GloboChat& g : c.globos) {
                    if (g.id == e.id) { destino = &g; break; }
                    if (g.inicio < destino->inicio) destino = &g;  // el más viejo
                }
                *destino = {e.id, ahora, linea};
            }
        }
    }
}

bool interpretar_entrada(std::string_view linea, EntradaChat& s) {
    s = {};
    while (!linea.empty() && linea.back() == ' ') linea.remove_suffix(1);
    if (linea.empty()) return false;
    switch (linea[0]) {
    case '!': s.tipo = CHAT_GRITO; break;
    case '&': s.tipo = CHAT_GLOBAL; break;
    case '$': s.tipo = CHAT_COMERCIO; break;
    case '@': s.tipo = CHAT_CLAN; break;
    case '#': s.tipo = CHAT_GRUPO; break;
    case '%': s.tipo = CHAT_ALIANZA; break;
    case '*': s.tipo = CHAT_GRUPO_CHAT; break;
    case '"': {
        std::string_view r = linea.substr(1);
        size_t esp = r.find(' ');
        if (esp == 0 || esp == std::string_view::npos) return false;
        s.susurro = true;
        s.destino = r.substr(0, esp);
        s.texto = r.substr(esp + 1);
        while (!s.texto.empty() && s.texto[0] == ' ') s.texto.remove_prefix(1);
        return !s.texto.empty();
    }
    default:
        s.texto = linea;
        return true;
    }
    s.texto = linea.substr(1);
    return !s.texto.empty();
}

}  // namespace lin
