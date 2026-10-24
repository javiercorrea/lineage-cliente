#include "recursos/sonidos.h"
#include <cctype>
#include <cstdlib>

namespace lin {

namespace {

// Palabras del texto, sin comentarios (";" hasta el fin de línea). Separa también "<TAG>",
// "</TAG>", "[...]" y "\" para que el lector no dependa de los espacios.
class Fichas {
public:
    explicit Fichas(std::string_view t) : t_(t) {}

    std::string_view siguiente() {
        for (;;) {
            while (i_ < t_.size() && (std::isspace(static_cast<unsigned char>(t_[i_])) || t_[i_] == '=')) ++i_;
            if (i_ >= t_.size()) return {};
            if (t_[i_] != ';') break;
            while (i_ < t_.size() && t_[i_] != '\n') ++i_;
        }
        const size_t ini = i_;
        const char c = t_[i_];
        if (c == '<' || c == '[') {
            const char fin = c == '<' ? '>' : ']';
            while (i_ < t_.size() && t_[i_] != fin && t_[i_] != '\n') ++i_;
            if (i_ < t_.size() && t_[i_] == fin) ++i_;
            return t_.substr(ini, i_ - ini);
        }
        if (c == '\\') return t_.substr(i_++, 1);
        while (i_ < t_.size() && !std::isspace(static_cast<unsigned char>(t_[i_])) && t_[i_] != '<' && t_[i_] != '[' &&
               t_[i_] != '\\' && t_[i_] != ';' && t_[i_] != '=')
            ++i_;
        return t_.substr(ini, i_ - ini);
    }
    // La próxima ficha sin consumirla.
    std::string_view mirar() {
        size_t guardado = i_;
        std::string_view f = siguiente();
        i_ = guardado;
        return f;
    }

private:
    std::string_view t_;
    size_t i_ = 0;
};

bool es_numero(std::string_view f) {
    if (f.empty()) return false;
    for (char c : f)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}
i32 numero(std::string_view f) { return std::atoi(std::string(f).c_str()); }

// Grupos "mapa [x1 y1 x2 y2]" separados por "\", hasta la primera ficha que no es número ni "\".
void leer_rects(Fichas& f, std::vector<RectZona>& rects, u32& primero, u32& n) {
    primero = static_cast<u32>(rects.size());
    i32 v[5];
    int k = 0;
    auto cerrar = [&] {
        if (k == 1) rects.push_back({static_cast<u16>(v[0]), true, 0, 0, 0, 0});
        else if (k == 5) rects.push_back({static_cast<u16>(v[0]), false, v[1], v[2], v[3], v[4]});
        k = 0;
    };
    for (std::string_view m = f.mirar(); es_numero(m) || m == "\\"; m = f.mirar()) {
        f.siguiente();
        if (m == "\\") {
            cerrar();
        } else if (k < 5) {
            v[k++] = numero(m);
        }
    }
    cerrar();
    n = static_cast<u32>(rects.size()) - primero;
}

}  // namespace

void DefSonidos::cargar(std::string_view music_def, std::string_view envsound_def) {
    *this = {};
    Fichas m(music_def);
    for (std::string_view f = m.siguiente(); !f.empty(); f = m.siguiente()) {
        if (f == "Login" && es_numero(m.mirar())) musica_login = static_cast<i16>(numero(m.siguiente()));
        else if (f == "Death" && es_numero(m.mirar())) musica_muerte = static_cast<i16>(numero(m.siguiente()));
        else if (f == "LOC") {
            ZonaMusica z;
            leer_rects(m, rects, z.primer_rect, z.n_rects);
            musica.push_back(z);
        } else if (f == "MUSIC" && !musica.empty() && es_numero(m.mirar())) {
            musica.back().musica = static_cast<i16>(numero(m.siguiente()));
        } else if (f == "INTERVAL" && !musica.empty() && es_numero(m.mirar())) {
            musica.back().intervalo_s = static_cast<u16>(numero(m.siguiente()));
        }
    }

    // envsound.def: primero se juntan los grupos; las zonas los nombran.
    Fichas e(envsound_def);
    struct Pendiente {
        u32 zona;
        std::string_view nombre;
    };
    std::vector<Pendiente> pendientes;
    u32 rect0 = 0, nrects = 0;
    bool hay_loc = false;
    u8 momento = 0;
    auto zona_actual = [&]() -> AmbienteZona& {
        // Una zona por (LOC, momento): la última si coincide, si no una nueva.
        if (ambiente.empty() || ambiente.back().primer_rect != rect0 || ambiente.back().momento != momento) {
            AmbienteZona z;
            z.primer_rect = rect0;
            z.n_rects = nrects;
            z.momento = momento;
            ambiente.push_back(z);
        }
        return ambiente.back();
    };
    for (std::string_view f = e.siguiente(); !f.empty(); f = e.siguiente()) {
        if (f.size() > 2 && f[0] == '[' && f[1] != '/') {
            // [NOMBRE:segundos]
            std::string_view dentro = f.substr(1, f.size() - 2);
            size_t dos = dentro.find(':');
            GrupoSonido g;
            g.nombre = dentro.substr(0, dos);
            g.intervalo_s = dos == std::string_view::npos ? 0 : static_cast<u16>(numero(dentro.substr(dos + 1)));
            g.primero = static_cast<u32>(ids_grupos.size());
            while (es_numero(e.mirar())) ids_grupos.push_back(static_cast<u16>(numero(e.siguiente())));
            g.cantidad = static_cast<u32>(ids_grupos.size()) - g.primero;
            grupos.push_back(g);
        } else if (f == "<LOC>") {
            leer_rects(e, rects, rect0, nrects);
            hay_loc = true;
            momento = 0;
        } else if (f == "<TIME>") {
            std::string_view t = e.siguiente();
            momento = t == "DAY" ? 1 : t == "NIGHT" ? 2 : 0;
        } else if (f == "<MAIN_SOUND>" && hay_loc && es_numero(e.mirar())) {
            zona_actual().principal = numero(e.siguiente());
        } else if ((f == "<ONE_SOUND>" || f == "<ONESOUND>") && hay_loc) {
            zona_actual();
            const u32 zi = static_cast<u32>(ambiente.size() - 1);
            for (std::string_view n = e.mirar(); !n.empty() && n[0] != '<'; n = e.mirar()) pendientes.push_back({zi, e.siguiente()});
        }
    }
    // Nombres de grupo → índices, agrupados por zona (los pendientes ya vienen en orden de zona).
    for (AmbienteZona& z : ambiente) z.n_grupos = 0;
    for (u32 zi = 0, p = 0; zi < ambiente.size(); ++zi) {
        ambiente[zi].primer_grupo = static_cast<u32>(grupos_ambiente.size());
        for (; p < pendientes.size() && pendientes[p].zona == zi; ++p)
            for (u32 g = 0; g < grupos.size(); ++g)
                if (grupos[g].nombre == pendientes[p].nombre) {
                    grupos_ambiente.push_back(g);
                    break;
                }
        ambiente[zi].n_grupos = static_cast<u32>(grupos_ambiente.size()) - ambiente[zi].primer_grupo;
    }
}

bool DefSonidos::contiene(u32 primer, u32 n, u16 mapa, i32 x, i32 y) const {
    for (u32 i = primer; i < primer + n; ++i) {
        const RectZona& r = rects[i];
        if (r.mapa != mapa) continue;
        if (r.todo_el_mapa || (x >= r.x1 && x <= r.x2 && y >= r.y1 && y <= r.y2)) return true;
    }
    return false;
}

const ZonaMusica* DefSonidos::musica_en(u16 mapa, i32 x, i32 y) const {
    for (const ZonaMusica& z : musica)
        if (z.musica >= 0 && contiene(z.primer_rect, z.n_rects, mapa, x, y)) return &z;
    return nullptr;
}

const AmbienteZona* DefSonidos::ambiente_en(u16 mapa, i32 x, i32 y, bool noche) const {
    for (const AmbienteZona& z : ambiente) {
        if (z.momento == 1 && noche) continue;
        if (z.momento == 2 && !noche) continue;
        if (contiene(z.primer_rect, z.n_rects, mapa, x, y)) return &z;
    }
    return nullptr;
}

}  // namespace lin
