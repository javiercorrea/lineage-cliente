#include "recursos/hechizos.h"
#include <cctype>
#include <charconv>

namespace lin {

namespace {

// Clases de "spells" que se lanzan con la acción de ataque (18).
constexpr std::string_view CLASES_ATAQUE[] = {
    "SpellAttack", "SpellAttackDir", "SpellMassAttack", "SpellChillTouch", "SpellBlizzard",
    "SpellTornado", "SpellIceLance", "SpellHold", "SpellFireWall", "SpellManaAttack",
    "SpellSmashAttack", "SpellTripleAttack", "SpellTripleShot",
};

bool empieza(std::string_view t, std::string_view p) { return t.substr(0, p.size()) == p; }

std::string_view minusculas(std::string_view t, char* buf, size_t cap) {
    size_t n = std::min(t.size(), cap);
    for (size_t i = 0; i < n; ++i) buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(t[i])));
    return {buf, n};
}

u32 numero(std::string_view t, size_t& pos) {
    while (pos < t.size() && (t[pos] < '0' || t[pos] > '9')) ++pos;
    u32 v = 0;
    auto r = std::from_chars(t.data() + pos, t.data() + t.size(), v);
    pos = static_cast<size_t>(r.ptr - t.data());
    return v;
}

// Valor entero del atributo "clave=N" en una línea de spells (0 si no está).
u32 atributo(std::string_view linea, std::string_view clave, bool& esta) {
    size_t p = 0;
    while ((p = linea.find(clave, p)) != std::string_view::npos) {
        if ((p == 0 || linea[p - 1] == ' ') && p + clave.size() < linea.size() && linea[p + clave.size()] == '=') {
            esta = true;
            size_t q = p + clave.size() + 1;
            return numero(linea, q);
        }
        p += clave.size();
    }
    esta = false;
    return 0;
}

// Recorre las líneas de un texto (CRLF o LF).
template <class F>
void lineas(const std::vector<u8>& datos, F&& f) {
    std::string_view t(reinterpret_cast<const char*>(datos.data()), datos.size());
    while (!t.empty()) {
        size_t fin = t.find('\n');
        std::string_view l = t.substr(0, fin);
        t = fin == std::string_view::npos ? std::string_view{} : t.substr(fin + 1);
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.remove_suffix(1);
        if (!l.empty()) f(l);
    }
}

// "<índice> <resto>": índice y resto; false si la línea no empieza con un número.
bool indice_y_resto(std::string_view l, u32& i, std::string_view& resto) {
    auto r = std::from_chars(l.data(), l.data() + l.size(), i);
    if (r.ec != std::errc() || r.ptr == l.data()) return false;
    resto = l.substr(static_cast<size_t>(r.ptr - l.data()));
    while (!resto.empty() && resto.front() == ' ') resto.remove_prefix(1);
    return true;
}

}  // namespace

bool hechizo_con_objetivo(std::string_view objetivos, bool tiene_alcance) {
    char buf[96];
    std::string_view t = minusculas(objetivos, buf, sizeof buf);
    if (t == "caster" || empieza(t, "every") || empieza(t, "all ") || empieza(t, "hidden") || empieza(t, "invisible") ||
        empieza(t, "pledge members within") || (empieza(t, "area") && t.find("caster") != std::string_view::npos))
        return false;
    // Ítems del inventario (encantar un arma o una armadura): el servidor usa los equipados.
    if (t.find("weapon") != std::string_view::npos || t.find("armor") != std::string_view::npos ||
        t.find("item") != std::string_view::npos)
        return false;
    for (std::string_view k : {"pc", "npc", "monster", "member", "player", "area of effect within", "target"})
        if (t.find(k) != std::string_view::npos) return true;
    return tiene_alcance;
}

bool TablaHechizos::cargar(const Recursos& r) {
    hechizos_.assign(CANTIDAD_HECHIZOS, Hechizo{});
    if (!r.texto("spelldesc-e.tbl", desc_)) return false;
    r.texto("spellexdesc-e.tbl", ficha_);
    r.texto("spells", spells_);

    // Nombre y costo: "Lesser Heal (4/0)".
    bool primera = true;
    lineas(desc_, [&](std::string_view l) {
        u32 i;
        std::string_view resto;
        if (primera) {  // la cantidad
            primera = false;
            return;
        }
        if (!indice_y_resto(l, i, resto) || i >= CANTIDAD_HECHIZOS) return;
        Hechizo& h = hechizos_[i];
        size_t par = resto.rfind(" (");
        h.nombre = par == std::string_view::npos ? resto : resto.substr(0, par);
        if (par != std::string_view::npos) {
            size_t p = par;
            h.mp = static_cast<u16>(numero(resto, p));
            h.hp = static_cast<u16>(numero(resto, p));
        }
    });

    // Atributos (la primera definición de cada índice: después vienen las de monstruos).
    std::vector<bool> visto(CANTIDAD_HECHIZOS, false);
    lineas(spells_, [&](std::string_view l) {
        if (l.empty() || l[0] != '"') return;
        size_t cierre = l.find('"', 1);
        if (cierre == std::string_view::npos) return;
        std::string_view resto = l.substr(cierre + 1);
        while (!resto.empty() && resto.front() == ' ') resto.remove_prefix(1);
        u32 i;
        std::string_view tras;
        if (!indice_y_resto(resto, i, tras) || i >= CANTIDAD_HECHIZOS || visto[i]) return;
        visto[i] = true;
        Hechizo& h = hechizos_[i];
        std::string_view clase = tras.substr(0, tras.find(' '));
        for (std::string_view c : CLASES_ATAQUE)
            if (clase == c) h.de_ataque = true;
        bool esta;
        h.alcance = static_cast<u8>(std::min<u32>(atributo(tras, "range", esta), 255));
        bool tiene_alcance = esta;
        h.gfx = static_cast<u16>(atributo(tras, "graphic", esta));
        h.demora_ms = static_cast<u16>(std::min<u32>(atributo(tras, "useDelay", esta), 0xFFFF));
        if (h.mp == 0) h.mp = static_cast<u16>(atributo(tras, "mpCost", esta));
        h.con_objetivo = tiene_alcance;  // se corrige con la ficha, abajo
    });

    // Objetivo, según la ficha: "...^\fHTargets: \f>PC/NPC^...".
    lineas(ficha_, [&](std::string_view l) {
        u32 i;
        std::string_view resto;
        if (!indice_y_resto(l, i, resto) || i >= CANTIDAD_HECHIZOS) return;
        Hechizo& h = hechizos_[i];
        h.ficha = resto;
        constexpr std::string_view MARCA = "Targets: \\f>";
        size_t p = resto.find(MARCA);
        if (p == std::string_view::npos) return;
        std::string_view obj = resto.substr(p + MARCA.size());
        obj = obj.substr(0, obj.find('^'));
        h.con_objetivo = hechizo_con_objetivo(obj, h.con_objetivo);
    });
    // Íconos: spellicon.idx, u16 por índice.
    Bytes iconos = r.sprite("spellicon.idx");
    for (size_t i = 0; i + 1 < iconos.size() && i / 2 < CANTIDAD_HECHIZOS; i += 2)
        hechizos_[i / 2].icono = static_cast<u16>(iconos[i] | (iconos[i + 1] << 8));
    return true;
}

const Hechizo& TablaHechizos::operator[](u32 id) const {
    static const Hechizo vacio;
    return id >= 1 && id <= hechizos_.size() ? hechizos_[id - 1] : vacio;
}

}  // namespace lin
