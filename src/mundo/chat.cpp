#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "mundo/chat.h"
#include "recursos/imagen.h"

namespace lin {

namespace {

constexpr u32 color(u8 r, u8 g, u8 b) { return r | (g << 8) | (b << 16) | 0xFF000000u; }

constexpr u32 COLOR_MENSAJE = color(255, 255, 255);
constexpr u32 COLOR_AZUL = color(140, 180, 255);

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

// Colores de \fX, RGB555, para X de '0' a 'd': tabla 0x500430 del cliente original (S3EP1U.bin).
constexpr u16 PALETA_F[0x65 - 0x30] = {
    0x0000, 0x001f, 0x03e0, 0x7c00, 0x509e, 0x3421, 0x50a5, 0x5ef7, 0x2108, 0x3def, 0x5f7f, 0x63b6, 0x7afe,
    0x7fe0, 0x7fff, 0x6318, 0x4200, 0x5ca7, 0x1577, 0x7db2, 0x4a3f, 0x1eb2, 0x02e0, 0x4a52, 0x7fd6, 0x4e74,
    0x662e, 0x6737, 0x14a5, 0x5fbc, 0x675f, 0x733f, 0x7f39, 0x7f99, 0x5e8d, 0x2e91, 0x3e8c, 0x3696, 0x525a,
    0x5e38, 0x5a36, 0x6631, 0x767d, 0x102d, 0x57bd, 0x7df0, 0x5000, 0x7e1f, 0x564e, 0x3dff, 0x0014, 0x5949,
    0x3214};

u32 color_codigo(char c, u32 normal) {
    if (c == '>') return normal;  // blanco en la tabla; en los mensajes vuelve a su color
    return es_codigo_color(c) ? rgb555(PALETA_F[c - 0x30]) : normal;
}

u32 color_alineamiento(i32 legal) {
    constexpr int MEDIO = 8000, TOPE = 16000;
    struct Rgb { int r, g, b; };
    constexpr Rgb BLANCO_{240, 240, 240}, CELESTE{150, 215, 255}, AZUL{70, 120, 255}, ROSA{255, 170, 195}, ROJO{255, 60, 50};
    const int d = std::min(std::abs(legal), TOPE);
    const Rgb a = d < MEDIO ? BLANCO_ : legal > 0 ? CELESTE : ROSA;
    const Rgb b = d < MEDIO ? (legal > 0 ? CELESTE : ROSA) : legal > 0 ? AZUL : ROJO;
    const int t = d < MEDIO ? d : d - MEDIO;  // de 0 a MEDIO dentro del tramo
    auto mezcla = [&](int x, int y) { return static_cast<u8>(x + (y - x) * t / MEDIO); };
    return color(mezcla(a.r, b.r), mezcla(a.g, b.g), mezcla(a.b, b.b));
}

const char* con_miles(u64 v, char* buf, size_t n) {
    char t[32];
    const int d = std::snprintf(t, sizeof t, "%llu", static_cast<unsigned long long>(v));
    size_t k = 0;
    for (int i = 0; i < d && k + 1 < n; ++i) {
        if (i && (d - i) % 3 == 0 && k + 2 < n) buf[k++] = '.';
        buf[k++] = t[i];
    }
    buf[k] = 0;
    return buf;
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
