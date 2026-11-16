#include "mundo/modo_auto.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include "mundo/autobuffs.h"

namespace lin {

namespace {

// Las vocales con tilde, la diéresis y la eñe (UTF-8, dos bytes desde 0xC3) -> la letra sin marca.
char sin_tilde(unsigned char b) {
    switch (b) {
    case 0xA1: case 0x81: return 'a';
    case 0xA9: case 0x89: return 'e';
    case 0xAD: case 0x8D: return 'i';
    case 0xB3: case 0x93: return 'o';
    case 0xBA: case 0x9A: case 0xBC: case 0x9C: return 'u';
    case 0xB1: case 0x91: return 'n';
    default: return 0;
    }
}

// Minúsculas y sin tildes; `solo_alnum` además saca todo lo que no es letra o dígito.
std::string plegar(std::string_view s, bool solo_alnum) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xC3 && i + 1 < s.size()) {
            if (char t = sin_tilde(static_cast<unsigned char>(s[i + 1]))) {
                r += t;
                ++i;
                continue;
            }
        }
        if (solo_alnum && !std::isalnum(c)) continue;
        r += static_cast<char>(std::tolower(c));
    }
    return r;
}

// ---- Lexer ----
enum class TipoTok : u8 { PALABRA, TEXTO, NUMERO, SIMBOLO };
struct Tok {
    TipoTok tipo;
    std::string s;  // PALABRA: plegada; TEXTO: tal cual; SIMBOLO: "<", "<=", ";"...
    i32 n = 0;
};

bool es_letra(unsigned char c) { return std::isalpha(c) || c >= 0x80 || c == '-' || c == '_'; }

// Falso con `error` si hay comillas sin cerrar o un carácter que no se reconoce.
bool tokenizar(std::string_view l, std::vector<Tok>& out, std::string& error) {
    size_t i = 0;
    while (i < l.size()) {
        const unsigned char c = static_cast<unsigned char>(l[i]);
        if (std::isspace(c)) {
            ++i;
        } else if (c == '"') {
            const size_t fin = l.find('"', i + 1);
            if (fin == std::string_view::npos) {
                error = "missing closing quote";
                return false;
            }
            out.push_back({TipoTok::TEXTO, std::string(l.substr(i + 1, fin - i - 1))});
            i = fin + 1;
        } else if (std::isdigit(c)) {
            i32 n = 0;
            while (i < l.size() && std::isdigit(static_cast<unsigned char>(l[i])) && n < 100000000) n = n * 10 + (l[i++] - '0');
            out.push_back({TipoTok::NUMERO, {}, n});
        } else if (es_letra(c)) {
            const size_t ini = i;
            while (i < l.size() && (es_letra(static_cast<unsigned char>(l[i])) || std::isdigit(static_cast<unsigned char>(l[i])))) ++i;
            out.push_back({TipoTok::PALABRA, plegar(l.substr(ini, i - ini), false)});
        } else if ((c == '<' || c == '>') && i + 1 < l.size() && l[i + 1] == '=') {
            out.push_back({TipoTok::SIMBOLO, std::string(l.substr(i, 2))});
            i += 2;
        } else if (std::string_view("<>=;,:()%").find(static_cast<char>(c)) != std::string_view::npos) {
            out.push_back({TipoTok::SIMBOLO, std::string(1, static_cast<char>(c))});
            ++i;
        } else {
            error = "unexpected character '" + std::string(1, static_cast<char>(c)) + "'";
            return false;
        }
    }
    return true;
}

// ---- Parser de una línea ----
struct Lector {
    const std::vector<Tok>& t;
    size_t i = 0;
    std::string error;

    bool fin() const { return i >= t.size(); }
    const Tok* actual() const { return fin() ? nullptr : &t[i]; }
    bool simbolo(std::string_view s) {
        if (fin() || t[i].tipo != TipoTok::SIMBOLO || t[i].s != s) return false;
        ++i;
        return true;
    }
    // Consume la primera frase (palabras seguidas) que coincida.
    bool frase(std::initializer_list<std::initializer_list<const char*>> frases) {
        for (const auto& f : frases) {
            size_t k = i;
            bool ok = true;
            for (const char* p : f) {
                if (k >= t.size() || t[k].tipo != TipoTok::PALABRA || t[k].s != p) {
                    ok = false;
                    break;
                }
                ++k;
            }
            if (ok) {
                i = k;
                return true;
            }
        }
        return false;
    }
    bool mira(std::initializer_list<std::initializer_list<const char*>> frases) {
        const size_t k = i;
        const bool r = frase(frases);
        i = k;
        return r;
    }
    bool falla(std::string m) {
        if (error.empty()) error = std::move(m);
        return false;
    }
    std::string que_hay() const {
        if (fin()) return "end of line";
        const Tok& k = t[i];
        if (k.tipo == TipoTok::NUMERO) return "'" + std::to_string(k.n) + "'";
        if (k.tipo == TipoTok::TEXTO) return "\"" + k.s + "\"";
        return "'" + k.s + "'";
    }
    bool numero(i32& n) {
        if (fin() || t[i].tipo != TipoTok::NUMERO) return falla("expected a number, found " + que_hay());
        n = t[i++].n;
        return true;
    }
    bool texto(std::string& s) {
        if (fin() || t[i].tipo != TipoTok::TEXTO) return falla("expected a name in quotes, found " + que_hay());
        s = t[i++].s;
        return true;
    }
    bool hay_texto() const { return !fin() && t[i].tipo == TipoTok::TEXTO; }
    bool comparacion(Comparacion& c) {
        if (simbolo("<=")) c = Comparacion::MENOR_IGUAL;
        else if (simbolo(">=")) c = Comparacion::MAYOR_IGUAL;
        else if (simbolo("<")) c = Comparacion::MENOR;
        else if (simbolo(">")) c = Comparacion::MAYOR;
        else if (simbolo("=")) c = Comparacion::IGUAL;
        else return falla("expected <, <=, >, >= or =, found " + que_hay());
        return true;
    }
    bool punto(i32& x, i32& y) {
        if (!simbolo("(")) return falla("expected (x,y), found " + que_hay());
        if (!numero(x) || !simbolo(",") || !numero(y) || !simbolo(")")) return falla("expected (x,y)");
        return true;
    }
    // "a 8" / "within 8"; sin él, `defecto`.
    bool radio(i32& r, i32 defecto) {
        r = defecto;
        return frase({{"a"}, {"within"}}) ? numero(r) : true;
    }
};

bool es_fin_condicion(Lector& l) {
    return l.fin() || l.mira({{"entonces"}, {"then"}, {"hasta"}, {"until"}}) || (l.actual()->tipo == TipoTok::SIMBOLO && l.actual()->s == ",");
}

bool cada(Lector& l, u64& ms) {
    i32 n = 0;
    if (!l.numero(n)) return false;
    if (l.frase({{"m"}, {"min"}})) ms = static_cast<u64>(n) * 60000;
    else if (l.frase({{"ms"}})) ms = static_cast<u64>(n);
    else {
        l.frase({{"s"}, {"seg"}, {"sec"}});  // segundos por defecto
        ms = static_cast<u64>(n) * 1000;
    }
    return true;
}

bool condicion(Lector& l, Condicion& c) {
    c = {};
    for (;;) {
        if (l.frase({{"sin", "jugadores"}, {"no", "players"}})) {
            c.tipo = TipoCond::SIN_JUGADORES;
            return l.radio(c.radio, 0);
        }
        if (l.frase({{"sin", "pociones"}, {"no", "potions"}})) {
            c.tipo = TipoCond::SIN_POCIONES;
            return true;
        }
        if (l.mira({{"sin"}, {"no"}}) && l.i + 1 < l.t.size() && l.t[l.i + 1].tipo == TipoTok::TEXTO) {
            ++l.i;
            c.tipo = TipoCond::SIN_ITEM;
            l.texto(c.nombre);
            c.nombre = normalizar_nombre(c.nombre);
            return true;
        }
        if (!l.frase({{"no"}, {"not"}})) break;
        c.negada = !c.negada;
    }
    if (l.frase({{"hp"}, {"mp"}})) {
        c.tipo = l.t[l.i - 1].s == "hp" ? TipoCond::HP : TipoCond::MP;
        if (!l.comparacion(c.comp) || !l.numero(c.valor)) return false;
        c.porcentaje = l.simbolo("%");
        return true;
    }
    if (l.frase({{"seguro"}, {"safe"}})) {
        c.tipo = TipoCond::SEGURO;
        return l.radio(c.radio, 8);
    }
    if (l.frase({{"monstruos"}, {"monsters"}})) {
        c.tipo = TipoCond::MONSTRUOS;
        return l.comparacion(c.comp) && l.numero(c.valor) && l.radio(c.radio, 8);
    }
    if (l.frase({{"atacando"}, {"attacking"}})) {
        c.tipo = TipoCond::ATACANDO;
        return true;
    }
    if (l.frase({{"tiene"}, {"has"}})) {
        c.tipo = TipoCond::TIENE;
        if (!l.texto(c.nombre)) return false;
        c.nombre = normalizar_nombre(c.nombre);
        return true;
    }
    if (l.frase({{"objetivo", "hp"}, {"target", "hp"}})) {
        c.tipo = TipoCond::HP_OBJETIVO;
        if (!l.comparacion(c.comp) || !l.numero(c.valor)) return false;
        c.porcentaje = true;
        l.simbolo("%");
        return true;
    }
    if (l.frase({{"en", "mapa"}, {"on", "map"}})) {
        c.tipo = TipoCond::EN_MAPA;
        c.comp = Comparacion::IGUAL;
        return l.numero(c.valor);
    }
    if (l.hay_texto()) {  // "Nombre" hp < 50%
        l.texto(c.nombre);
        c.nombre = normalizar_nombre(c.nombre);
        c.tipo = TipoCond::HP_DE;
        if (!l.frase({{"hp"}})) return l.falla("expected 'hp' after the name");
        if (!l.comparacion(c.comp) || !l.numero(c.valor)) return false;
        c.porcentaje = true;
        l.simbolo("%");
        return true;
    }
    return l.falla("unknown condition " + l.que_hay());
}

// Hasta "entonces", "hasta", una coma o el fin. "cada N s" puede ir en medio.
bool expresion(Lector& l, Expresion& e, u64* cada_ms) {
    e.o.assign(1, {});
    for (;;) {
        if (cada_ms && l.frase({{"cada"}, {"every"}})) {
            if (!cada(l, *cada_ms)) return false;
            if (es_fin_condicion(l)) break;
            l.frase({{"y"}, {"and"}});
            continue;
        }
        Condicion c;
        if (!condicion(l, c)) return false;
        e.o.back().push_back(std::move(c));
        if (cada_ms && l.frase({{"cada"}, {"every"}})) {
            if (!cada(l, *cada_ms)) return false;
        }
        if (es_fin_condicion(l)) break;
        if (l.frase({{"y"}, {"and"}})) continue;
        if (l.frase({{"o"}, {"or"}})) {
            e.o.emplace_back();
            continue;
        }
        return l.falla("expected 'and', 'or' or 'then', found " + l.que_hay());
    }
    if (e.o.size() == 1 && e.o[0].empty()) e.o.clear();  // solo "cada": siempre
    for (const auto& g : e.o)
        if (g.empty()) return l.falla("empty condition");
    return true;
}

bool objetivo(Lector& l, AccionAuto& a) {
    if (!l.frase({{"a"}, {"on"}})) return true;
    if (l.frase({{"mi"}, {"me"}, {"yo"}, {"myself"}})) a.objetivo = Objetivo::YO;
    else if (l.frase({{"objetivo"}, {"target"}})) a.objetivo = Objetivo::OBJETIVO;
    else if (l.frase({{"mas", "cercano"}, {"nearest"}})) a.objetivo = Objetivo::MAS_CERCANO;
    else if (l.frase({{"aliado", "mas", "herido"}, {"most", "hurt", "ally"}})) a.objetivo = Objetivo::ALIADO_MAS_HERIDO;
    else if (l.hay_texto()) {
        l.texto(a.objetivo_nombre);
        a.objetivo_nombre = normalizar_nombre(a.objetivo_nombre);
        a.objetivo = Objetivo::NOMBRE;
    } else {
        return l.falla("expected a target (me, target, nearest, most hurt ally or a name), found " + l.que_hay());
    }
    return true;
}

bool accion(Lector& l, AccionAuto& a, bool de_tecla) {
    a = {};
    if (l.frase({{"atacar"}, {"attack"}})) {
        a.tipo = TipoAccion::ATACAR;
        l.frase({{"mas", "cercano"}, {"nearest"}});
        return true;
    }
    if (l.frase({{"lanzar"}, {"cast"}, {"usar"}, {"use"}, {"equipar"}, {"equip"}})) {
        const std::string& v = l.t[l.i - 1].s;
        a.tipo = v == "lanzar" || v == "cast" ? TipoAccion::LANZAR : v == "usar" || v == "use" ? TipoAccion::USAR : TipoAccion::EQUIPAR;
        if (!l.texto(a.nombre)) return false;
        a.nombre = normalizar_nombre(a.nombre);
        return a.tipo == TipoAccion::EQUIPAR || objetivo(l, a);
    }
    if (l.frase({{"detener"}, {"stop"}})) a.tipo = TipoAccion::DETENER;
    else if (l.frase({{"esperar"}, {"wait"}})) a.tipo = TipoAccion::ESPERAR;
    else if (l.frase({{"terminar"}, {"end"}})) a.tipo = TipoAccion::TERMINAR;
    else if (l.frase({{"ir", "a"}, {"go", "to"}})) {
        a.tipo = TipoAccion::IR_A;
        return l.punto(a.x, a.y);
    } else if (l.mira({{"pausar"}, {"pause"}, {"siguiente", "punto"}, {"next", "waypoint"}, {"regla"}, {"rule"}})) {
        if (!de_tecla) return l.falla(l.que_hay() + " can only be used in a key");
        if (l.frase({{"pausar"}, {"pause"}})) a.tipo = TipoAccion::PAUSAR;
        else if (l.frase({{"siguiente", "punto"}, {"next", "waypoint"}})) a.tipo = TipoAccion::SIGUIENTE_PUNTO;
        else {
            l.frase({{"regla"}, {"rule"}});
            a.tipo = TipoAccion::REGLA;
            return l.numero(a.n);
        }
    } else {
        return l.falla("unknown action " + l.que_hay());
    }
    return true;
}

// Acciones separadas por ";", hasta "hasta" / "cada" o el fin.
bool acciones(Lector& l, std::vector<AccionAuto>& out, bool de_tecla) {
    do {
        AccionAuto a;
        if (!accion(l, a, de_tecla)) return false;
        out.push_back(std::move(a));
    } while (l.simbolo(";"));
    return true;
}

// "hasta <expresión>" y "cada N s" al final de una regla o una tecla.
bool cola(Lector& l, Expresion& hasta, bool& con_hasta, u64* cada_ms) {
    for (;;) {
        if (l.fin()) return true;
        if (l.frase({{"hasta"}, {"until"}})) {
            con_hasta = true;
            if (!expresion(l, hasta, cada_ms)) return false;
            if (hasta.vacia()) return l.falla("empty condition after 'until'");
        } else if (cada_ms && l.frase({{"cada"}, {"every"}})) {
            if (!cada(l, *cada_ms)) return false;
        } else {
            return l.falla("unexpected " + l.que_hay());
        }
    }
}

std::string_view sin_espacios(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

// La línea sin el comentario (# fuera de comillas).
std::string_view sin_comentario(std::string_view l) {
    bool en_texto = false;
    for (size_t i = 0; i < l.size(); ++i) {
        if (l[i] == '"') en_texto = !en_texto;
        else if (l[i] == '#' && !en_texto) return l.substr(0, i);
    }
    return l;
}

}  // namespace

// "Shift + Ctrl + 1" -> "ctrl+shift+1": minúsculas, sin espacios y los modificadores en orden fijo (ctrl,
// alt, shift), como los arma el cliente al apretar la tecla. Vacía si no tiene forma de tecla.
std::string tecla_normalizada(std::string_view s) {
    std::string r;
    for (char c : s)
        if (!std::isspace(static_cast<unsigned char>(c))) r += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool ctrl = false, alt = false, shift = false;
    std::string_view v = r;
    for (size_t mas; (mas = v.find('+')) != std::string_view::npos; v.remove_prefix(mas + 1)) {
        const std::string_view m = v.substr(0, mas);
        if (m == "ctrl" || m == "control" || m == "cmd") ctrl = true;
        else if (m == "alt" || m == "option") alt = true;
        else if (m == "shift") shift = true;
        else return {};
    }
    if (v.empty()) return {};
    return std::string(ctrl ? "ctrl+" : "") + (alt ? "alt+" : "") + (shift ? "shift+" : "") + std::string(v);
}

namespace {

// F1–F4, Esc, Enter y Tab (con cualquier modificador) y Shift+F12 son del cliente.
bool tecla_reservada(const std::string& t) {
    const size_t mas = t.rfind('+');
    const std::string_view base = mas == std::string::npos ? std::string_view(t) : std::string_view(t).substr(mas + 1);
    for (std::string_view r : {"f1", "f2", "f3", "f4", "esc", "escape", "enter", "return", "tab"})
        if (base == r) return true;
    return base == "f12" && t.find("shift+") != std::string::npos;
}

bool comparar(i64 a, Comparacion c, i64 b) {
    switch (c) {
    case Comparacion::MENOR: return a < b;
    case Comparacion::MENOR_IGUAL: return a <= b;
    case Comparacion::MAYOR: return a > b;
    case Comparacion::MAYOR_IGUAL: return a >= b;
    case Comparacion::IGUAL: return a == b;
    }
    return false;
}

// Vida o maná: en % se compara valor * 100 contra el % pedido de la máxima (sin dividir).
bool comparar_recurso(i32 actual, i32 maximo, const Condicion& c) {
    if (!c.porcentaje) return comparar(actual, c.comp, c.valor);
    if (maximo <= 0) return false;
    return comparar(static_cast<i64>(actual) * 100, c.comp, static_cast<i64>(c.valor) * maximo);
}

bool contiene(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

bool evaluar(const Condicion& c, const EntornoAuto& en) {
    bool r = false;
    switch (c.tipo) {
    case TipoCond::HP: r = comparar_recurso(en.hp, en.hp_max, c); break;
    case TipoCond::MP: r = comparar_recurso(en.mp, en.mp_max, c); break;
    case TipoCond::SEGURO: r = std::none_of(en.monstruos.begin(), en.monstruos.end(), [&](i32 d) { return d <= c.radio; }); break;
    case TipoCond::SIN_JUGADORES:
        r = std::none_of(en.personajes.begin(), en.personajes.end(), [&](const PersonajeVisto& p) { return !c.radio || p.distancia <= c.radio; });
        break;
    case TipoCond::MONSTRUOS:
        r = comparar(std::count_if(en.monstruos.begin(), en.monstruos.end(), [&](i32 d) { return d <= c.radio; }), c.comp, c.valor);
        break;
    case TipoCond::ATACANDO: r = en.atacando; break;
    case TipoCond::SIN_POCIONES: r = !en.hay_pociones; break;
    case TipoCond::SIN_ITEM: r = !contiene(en.items, c.nombre); break;
    case TipoCond::TIENE: r = contiene(en.efectos, c.nombre); break;
    case TipoCond::HP_DE: {
        const auto it = std::find_if(en.personajes.begin(), en.personajes.end(), [&](const PersonajeVisto& p) { return p.nombre == c.nombre; });
        r = it != en.personajes.end() && it->hp_pct >= 0 && comparar(it->hp_pct, c.comp, c.valor);
        break;
    }
    case TipoCond::HP_OBJETIVO: r = en.objetivo_hp_pct >= 0 && comparar(en.objetivo_hp_pct, c.comp, c.valor); break;
    case TipoCond::EN_MAPA: r = en.mapa == c.valor; break;
    }
    return r != c.negada;
}

constexpr u64 NUNCA = ~0ull;

}  // namespace

std::string normalizar_nombre(std::string_view s) { return plegar(s, true); }

bool Regla::termina() const {
    return std::any_of(acciones.begin(), acciones.end(), [](const AccionAuto& a) { return a.tipo == TipoAccion::TERMINAR; });
}

Programa leer_programa(std::string_view texto, std::vector<ErrorPrograma>& errores) {
    Programa p;
    int nlinea = 0;
    while (!texto.empty()) {
        ++nlinea;
        const size_t nl = texto.find('\n');
        std::string_view linea = sin_espacios(sin_comentario(texto.substr(0, nl)));
        texto = nl == std::string_view::npos ? std::string_view{} : texto.substr(nl + 1);
        if (linea.empty()) continue;
        auto error = [&](std::string m) { errores.push_back({nlinea, std::move(m)}); };

        // Las teclas, antes de partir en palabras: el nombre ("ctrl+1") lleva "+".
        const std::string inicio = plegar(linea.substr(0, std::min<size_t>(linea.size(), 6)), false);
        if (inicio.rfind("tecla ", 0) == 0 || inicio.rfind("key ", 0) == 0) {
            const size_t dos_puntos = linea.find(':');
            if (dos_puntos == std::string_view::npos) {
                error("expected ':' after the key");
                continue;
            }
            Tecla k;
            k.linea = nlinea;
            k.tecla = tecla_normalizada(linea.substr(inicio[0] == 't' ? 6 : 4, dos_puntos - (inicio[0] == 't' ? 6 : 4)));
            if (k.tecla.empty()) {
                error("expected a key name (F5, ctrl+1, A...)");
                continue;
            }
            if (tecla_reservada(k.tecla)) {
                error("key '" + k.tecla + "' is reserved by the client");
                continue;
            }
            if (std::any_of(p.teclas.begin(), p.teclas.end(), [&](const Tecla& o) { return o.tecla == k.tecla; })) {
                error("key '" + k.tecla + "' is defined twice");
                continue;
            }
            std::vector<Tok> toks;
            std::string e;
            if (!tokenizar(linea.substr(dos_puntos + 1), toks, e)) {
                error(e);
                continue;
            }
            Lector l{toks, 0, {}};
            if (acciones(l, k.acciones, true) && cola(l, k.hasta, k.con_hasta, nullptr)) p.teclas.push_back(std::move(k));
            else error(l.error);
            continue;
        }

        std::vector<Tok> toks;
        std::string e;
        if (!tokenizar(linea, toks, e)) {
            error(e);
            continue;
        }
        Lector l{toks, 0, {}};
        bool ok = true;
        if (l.frase({{"si"}, {"when"}, {"if"}})) {
            Regla r;
            r.linea = nlinea;
            ok = expresion(l, r.si, &r.cada_ms);
            if (ok && !l.frase({{"entonces"}, {"then"}})) ok = l.falla("expected 'then', found " + l.que_hay());
            ok = ok && acciones(l, r.acciones, false) && cola(l, r.hasta, r.con_hasta, &r.cada_ms);
            if (ok) p.reglas.push_back(std::move(r));
        } else if (l.frase({{"auto-hechizos"}, {"auto-spells"}, {"auto", "hechizos"}, {"auto", "spells"}})) {
            Regla r;
            r.linea = nlinea;
            r.es_auto = true;
            l.simbolo(":");
            do {
                AutoHechizo h;
                ok = l.texto(h.nombre);
                if (!ok) break;
                h.nombre = normalizar_nombre(h.nombre);
                if (l.frase({{"si"}, {"when"}, {"if"}})) ok = expresion(l, h.si, nullptr);
                if (!ok) break;
                r.auto_hechizos.push_back(std::move(h));
            } while (l.simbolo(","));
            if (ok && !l.fin()) ok = l.falla("unexpected " + l.que_hay());
            if (ok && r.auto_hechizos.size() > static_cast<size_t>(CASILLEROS_AUTO))
                ok = l.falla("at most " + std::to_string(CASILLEROS_AUTO) + " auto-spells");
            if (ok) p.reglas.push_back(std::move(r));
        } else if (l.frase({{"plan"}})) {
            ok = l.texto(p.nombre);
        } else if (l.frase({{"mapa"}, {"map"}})) {
            if (l.hay_texto()) ok = l.texto(p.mapa_nombre);
            else ok = l.numero(p.mapa);
        } else if (l.frase({{"arma"}, {"weapon"}})) {
            ok = l.texto(p.arma);
            p.arma = normalizar_nombre(p.arma);
        } else if (l.frase({{"pociones"}, {"potions"}})) {
            l.simbolo(":");
            if (l.frase({{"siempre"}, {"always"}})) p.pociones = ModoPociones::SIEMPRE;
            else if (l.frase({{"atacando"}, {"attacking"}})) p.pociones = ModoPociones::ATACANDO;
            else if (l.frase({{"nunca"}, {"never"}})) p.pociones = ModoPociones::NUNCA;
            else ok = l.falla("expected always, attacking or never, found " + l.que_hay());
        } else if (l.frase({{"moverse"}, {"move"}})) {
            Movimiento& m = p.movimiento;
            m = {};
            l.simbolo(":");
            if (l.frase({{"recorrer"}, {"wander"}})) {
                m.tipo = TipoMovimiento::RECORRER;
            } else if (l.frase({{"quieto"}, {"stay"}})) {
                m.tipo = TipoMovimiento::QUIETO;
            } else if (l.frase({{"puntos"}, {"waypoints"}})) {
                m.tipo = TipoMovimiento::PUNTOS;
                while (ok && !l.fin() && !l.mira({{"una", "vez"}, {"once"}})) {
                    i32 x = 0, y = 0;
                    ok = l.punto(x, y);
                    if (ok) m.puntos.emplace_back(x, y);
                }
                if (ok && l.frase({{"una", "vez"}, {"once"}})) m.una_vez = true;
                if (ok && m.puntos.empty()) ok = l.falla("expected at least one (x,y)");
            } else if (l.frase({{"seguir", "a"}, {"follow"}})) {
                m.tipo = TipoMovimiento::SEGUIR;
                ok = l.texto(m.seguir);
                m.seguir = normalizar_nombre(m.seguir);
                if (ok && l.frase({{"distancia"}, {"distance"}})) ok = l.numero(m.distancia);
            } else {
                ok = l.falla("expected wander, waypoints, follow or stay, found " + l.que_hay());
            }
        } else {
            ok = l.falla("unknown instruction " + l.que_hay());
        }
        if (ok && !l.fin()) ok = l.falla("unexpected " + l.que_hay());
        if (!ok) error(l.error);
    }
    return p;
}

void validar_programa(const Programa& p, const std::function<bool(std::string_view)>& existe_hechizo,
                      std::vector<ErrorPrograma>& errores) {
    auto ver = [&](const std::vector<AccionAuto>& acc, int linea) {
        for (const AccionAuto& a : acc)
            if (a.tipo == TipoAccion::LANZAR && !existe_hechizo(a.nombre)) errores.push_back({linea, "unknown spell '" + a.nombre + "'"});
    };
    for (const Regla& r : p.reglas) {
        ver(r.acciones, r.linea);
        for (const AutoHechizo& h : r.auto_hechizos) {
            const bool pocion = std::any_of(std::begin(POCIONES_AUTO), std::end(POCIONES_AUTO),
                                            [&](const PocionAuto& q) { return normalizar_nombre(q.nombre) == h.nombre; });
            if (!pocion && !existe_hechizo(h.nombre)) errores.push_back({r.linea, "unknown spell or potion '" + h.nombre + "'"});
        }
    }
    for (const Tecla& k : p.teclas) {
        ver(k.acciones, k.linea);
        for (const AccionAuto& a : k.acciones)
            if (a.tipo == TipoAccion::REGLA && (a.n < 1 || a.n > static_cast<i32>(p.reglas.size())))
                errores.push_back({k.linea, "there is no rule " + std::to_string(a.n)});
    }
}

bool cumple(const Expresion& e, const EntornoAuto& en) {
    if (e.vacia()) return true;
    return std::any_of(e.o.begin(), e.o.end(), [&](const std::vector<Condicion>& g) {
        return std::all_of(g.begin(), g.end(), [&](const Condicion& c) { return evaluar(c, en); });
    });
}

namespace {

// La regla i puede correr ahora; en un auto-hechizo, `cual` es el que hay que renovar.
bool elegible(const Programa& p, const EstadoAuto& est, const EntornoAuto& en, size_t i, u64 ahora, std::string& cual) {
    const Regla& r = p.reglas[i];
    if (r.es_auto) {
        for (const AutoHechizo& h : r.auto_hechizos)
            if (contiene(en.renovar, h.nombre) && cumple(h.si, en)) {
                cual = h.nombre;
                return true;
            }
        return false;
    }
    if (r.cada_ms && est.ultima_ms[i] != NUNCA && ahora - est.ultima_ms[i] < r.cada_ms) return false;
    return cumple(r.si, en);
}

Decision disparar(const Programa& p, EstadoAuto& est, size_t i, u64 ahora, std::string cual) {
    const Regla& r = p.reglas[i];
    est.ultima_ms[i] = ahora;
    Decision d;
    d.indice = static_cast<int>(i);
    if (r.es_auto) {
        d.origen = OrigenDecision::AUTO_HECHIZO;
        d.auto_hechizo = std::move(cual);
        return d;
    }
    d.origen = OrigenDecision::REGLA;
    d.acciones = &r.acciones;
    est.sostenida = r.con_hasta ? static_cast<int>(i) : -1;
    return d;
}

Decision mover(const Programa& p) {
    Decision d;
    if (p.movimiento.tipo != TipoMovimiento::QUIETO) d.origen = OrigenDecision::MOVER;
    return d;
}

}  // namespace

Decision decidir(const Programa& p, EstadoAuto& est, const EntornoAuto& en, u64 ahora) {
    if (est.pausado || est.terminado) return {};
    if (p.mapa >= 0 && en.mapa != p.mapa) {
        est.sostenida = est.tecla_sostenida = -1;
        return {};
    }
    est.ultima_ms.resize(p.reglas.size(), NUNCA);
    std::string cual;
    // Una tecla sostenida: solo la interrumpen las reglas que terminan el programa (el escape).
    if (est.tecla_sostenida >= 0 && static_cast<size_t>(est.tecla_sostenida) < p.teclas.size()) {
        for (size_t i = 0; i < p.reglas.size(); ++i)
            if (p.reglas[i].termina() && elegible(p, est, en, i, ahora, cual)) {
                est.tecla_sostenida = -1;
                return disparar(p, est, i, ahora, cual);
            }
        const Tecla& k = p.teclas[static_cast<size_t>(est.tecla_sostenida)];
        if (!cumple(k.hasta, en)) {
            Decision d;
            d.origen = OrigenDecision::TECLA;
            d.indice = est.tecla_sostenida;
            d.acciones = &k.acciones;
            d.continua = true;
            return d;
        }
        est.tecla_sostenida = -1;
    }
    // Una regla sostenida: las de más arriba la interrumpen; si no, sigue hasta su salida.
    size_t desde = 0;
    if (est.sostenida >= 0 && static_cast<size_t>(est.sostenida) < p.reglas.size()) {
        const size_t s = static_cast<size_t>(est.sostenida);
        for (size_t i = 0; i < s; ++i)
            if (elegible(p, est, en, i, ahora, cual)) return disparar(p, est, i, ahora, cual);
        if (!cumple(p.reglas[s].hasta, en)) {
            Decision d;
            d.origen = OrigenDecision::REGLA;
            d.indice = est.sostenida;
            d.acciones = &p.reglas[s].acciones;
            d.continua = true;
            return d;
        }
        est.sostenida = -1;
        desde = s;
    }
    for (size_t i = desde; i < p.reglas.size(); ++i)
        if (elegible(p, est, en, i, ahora, cual)) return disparar(p, est, i, ahora, cual);
    return mover(p);
}

const std::pair<i32, i32>* punto_actual(const Movimiento& m, EstadoAuto& est, i32 x, i32 y) {
    if (m.puntos.empty()) return nullptr;
    auto actual = [&]() -> const std::pair<i32, i32>* {
        if (m.una_vez && est.punto >= m.puntos.size()) return nullptr;
        return &m.puntos[est.punto % m.puntos.size()];
    };
    const std::pair<i32, i32>* p = actual();
    if (p && std::max(std::abs(p->first - x), std::abs(p->second - y)) <= 1) {
        ++est.punto;  // llegó: al siguiente (uno por llamada, aunque el siguiente también esté cerca)
        p = actual();
    }
    return p;
}

Decision pulsar_tecla(const Programa& p, EstadoAuto& est, std::string_view tecla, bool& usada) {
    usada = false;
    const auto it = std::find_if(p.teclas.begin(), p.teclas.end(), [&](const Tecla& k) { return k.tecla == tecla; });
    if (it == p.teclas.end()) return {};
    usada = true;
    bool otras = false;
    for (const AccionAuto& a : it->acciones) {
        switch (a.tipo) {
        case TipoAccion::PAUSAR: est.pausado = !est.pausado; break;
        case TipoAccion::SIGUIENTE_PUNTO: ++est.punto; break;
        case TipoAccion::REGLA:
            if (a.n >= 1 && static_cast<size_t>(a.n) <= p.reglas.size() && !p.reglas[static_cast<size_t>(a.n - 1)].es_auto) {
                est.ultima_ms.resize(p.reglas.size(), NUNCA);
                Decision d;
                d.origen = OrigenDecision::REGLA;
                d.indice = a.n - 1;
                d.acciones = &p.reglas[static_cast<size_t>(a.n - 1)].acciones;
                return d;
            }
            break;
        default: otras = true; break;
        }
    }
    if (!otras) return {};
    Decision d;
    d.origen = OrigenDecision::TECLA;
    d.indice = static_cast<int>(it - p.teclas.begin());
    d.acciones = &it->acciones;
    est.sostenida = -1;
    est.tecla_sostenida = it->con_hasta ? d.indice : -1;
    return d;
}

}  // namespace lin
