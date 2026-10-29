#include "mundo/inventario.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iterator>

namespace lin {

namespace {

TextoRef guardar(Inventario& inv, std::string_view t) {
    if (inv.textos.size() > MAX_ARENA_TEXTOS)
        compactar_textos(inv.textos, [&](auto&& f) {
            for (Slot s = 0; s < inv.items.tamano(); ++s) {
                f(inv.items.col<I_NOMBRE>()[s]);
                f(inv.items.col<I_ATRIBUTOS>()[s]);
            }
        });
    TextoRef r{static_cast<u32>(inv.textos.size()), static_cast<u16>(t.size() > 0xFFFF ? 0xFFFF : t.size())};
    inv.textos.insert(inv.textos.end(), t.begin(), t.begin() + r.len);
    return r;
}

}  // namespace

std::string_view nombre_item(const Inventario& inv, Slot s) {
    TextoRef r = inv.items.col<I_NOMBRE>()[s];
    return {inv.textos.data() + r.ofs, r.len};
}

namespace {

// Agrega líneas con formato separadas por '\n' a un buffer fijo (se corta si no entra).
struct Lineas {
    char* salida;
    size_t capacidad;
    size_t n = 0;
    [[gnu::format(printf, 2, 3)]] void operator()(const char* fmt, ...) {
        if (capacidad == 0 || n + 1 >= capacidad) return;
        if (n > 0) salida[n++] = '\n';
        va_list va;
        va_start(va, fmt);
        const int w = std::vsnprintf(salida + n, capacidad - n, fmt, va);
        va_end(va);
        if (w > 0) n = std::min(capacidad - 1, n + static_cast<size_t>(w));
    }
};

}  // namespace

std::string_view atributos_item(const Inventario& inv, Slot s) {
    TextoRef r = inv.items.col<I_ATRIBUTOS>()[s];
    return {inv.textos.data() + r.ofs, r.len};
}

size_t describir_atributos(std::string_view bytes, char* salida, size_t capacidad) {
    static constexpr const char* MATERIALES[] = {
        "none", "liquid", "web", "vegetation", "animal matter", "paper", "cloth", "leather", "wood", "bone",
        "dragon scale", "iron", "steel", "copper", "silver", "gold", "platinum", "mithril", "black mithril",
        "glass", "gemstone", "mineral", "oriharukon"};
    static constexpr const char* CLASES[] = {"Royal", "Knight", "Elf", "Wizard", "Dark Elf", "Dragon Knight", "Illusionist"};
    static constexpr const char* STATS[] = {"STR", "DEX", "CON", "WIS", "INT", "CHA"};
    static constexpr const char* ELEMENTOS[] = {"Fire", "Water", "Wind", "Earth"};
    static constexpr const char* RESISTENCIAS[] = {"Freeze", "Stone", "Sleep", "Blind", "Stun", "Hold"};
    LectorBinario l{Bytes(reinterpret_cast<const u8*>(bytes.data()), bytes.size())};
    Lineas linea{salida, capacidad};
    size_t& n = linea.n;
    auto material_y_peso = [&] {
        const u8 m = l.u8_();
        const u32 peso = l.u32_();
        if (m < std::size(MATERIALES) && m != 0) linea("Material: %s", MATERIALES[m]);
        linea("Weight %u", peso);
    };
    // Formato de L1ItemInstance.getStatusBytes: c etiqueta + su valor. Algunas etiquetas (1, 19,
    // 21, 22, 23) llevan detrás, sin etiqueta propia, el material y el peso.
    while (l.quedan(1) && !l.error) {
        const u8 et = l.u8_();
        switch (et) {
        case 1: {
            const u8 chico = l.u8_(), grande = l.u8_();
            linea("Damage %u (small) / %u (large)", chico, grande);
            material_y_peso();
            break;
        }
        case 19: {
            linea("AC %u", l.u8_());
            const u8 m = l.u8_();
            l.u8_();  // grado de los accesorios (o 0xFF)
            const u32 peso = l.u32_();
            if (m < std::size(MATERIALES) && m != 0) linea("Material: %s", MATERIALES[m]);
            linea("Weight %u", peso);
            break;
        }
        case 21: linea("Satiety %u", l.u16_()); material_y_peso(); break;
        case 22: linea("Light radius %u", l.u16_()); material_y_peso(); break;
        case 23: material_y_peso(); break;
        case 2: linea("Enchant %+d", l.i8_()); break;
        case 3: linea("Damaged %u", l.u8_()); break;
        case 4: linea("Two-handed"); break;
        case 5: linea("Hit %+d", l.i8_()); break;
        case 6: linea("Extra damage %+d", l.i8_()); break;
        case 7: {
            const u8 bits = l.u8_();
            if ((bits & 0x7F) == 0x7F) {
                linea("Usable by: all classes");
                break;
            }
            char clases[128];
            size_t m = 0;
            for (int i = 0; i < 7; ++i)
                if (bits & (1 << i)) {
                    const int w = std::snprintf(clases + m, sizeof clases - m, "%s%s", m ? ", " : "", CLASES[i]);
                    if (w > 0) m = std::min(sizeof clases - 1, m + static_cast<size_t>(w));
                }
            if (m) linea("Usable by: %s", clases);
            break;
        }
        case 8: case 9: case 10: case 11: case 12: case 13: linea("%s %+d", STATS[et - 8], l.i8_()); break;
        case 14: linea("HP %+d", l.i16_()); break;
        case 32: linea("MP %+d", l.i8_()); break;
        case 17: linea("SP %+d", l.i8_()); break;
        case 18: linea("Haste"); break;
        case 16: linea("Steals MP"); break;
        case 34: linea("Steals HP"); break;
        case 24: linea("Bow hit %+d", l.i8_()); break;
        case 35: linea("Bow damage %+d", l.i8_()); break;
        case 27: case 28: case 29: case 30: linea("%s resistance %+d", ELEMENTOS[et - 27], l.i8_()); break;
        case 15: {
            // MR, o, seguido de 33 y el tipo, resistencia a un estado.
            const i16 v = l.i16_();
            if (l.quedan(2) && l.datos[l.pos] == 33) {
                l.u8_();
                const u8 tipo = l.u8_();
                if (tipo >= 1 && tipo <= std::size(RESISTENCIAS)) linea("%s resistance %+d", RESISTENCIAS[tipo - 1], v);
            } else {
                linea("MR %+d", v);
            }
            break;
        }
        case 36: linea("EXP bonus %+d%%", l.i8_()); break;
        case 37: linea("HP regen %+d", l.i8_()); break;
        case 38: linea("MP regen %+d", l.i8_()); break;
        default: return n;  // etiqueta desconocida: no se sabe cuánto ocupa
        }
    }
    return n;
}

bool item_equipado(std::string_view n) {
    return n.find(" ($9)") != std::string_view::npos || n.find(" ($117)") != std::string_view::npos;
}

void aplicar_inventario(Inventario& inv, std::span<const Evento> eventos) {
    TablaItems& t = inv.items;
    for (const Evento& e : eventos) {
        switch (e.tipo) {
        case Ev::INVENTARIO_VACIAR: inv.limpiar(); break;
        case Ev::ITEM: {
            Slot s = t.insertar(e.id);
            t.col<I_GFX>()[s] = static_cast<u16>(e.v[IT_GFX]);
            t.col<I_USO>()[s] = static_cast<u8>(e.v[IT_USO]);
            t.col<I_BENDICION>()[s] = static_cast<u8>(e.v[IT_BENDICION]);
            t.col<I_CANTIDAD>()[s] = static_cast<u32>(e.v[IT_CANTIDAD]);
            t.col<I_IDENTIFICADO>()[s] = static_cast<u8>(e.v[IT_IDENTIFICADO]);
            t.col<I_NOMBRE>()[s] = guardar(inv, e.s[0]);
            t.col<I_ATRIBUTOS>()[s] = guardar(inv, e.s[1]);
            break;
        }
        case Ev::ITEM_QUITAR: t.quitar(e.id); break;
        case Ev::ITEM_NOMBRE: {
            Slot s = t.buscar(e.id);
            if (s != SIN_SLOT) t.col<I_NOMBRE>()[s] = guardar(inv, e.s[0]);
            break;
        }
        case Ev::ITEM_CANTIDAD: {
            Slot s = t.buscar(e.id);
            if (s == SIN_SLOT) break;
            t.col<I_NOMBRE>()[s] = guardar(inv, e.s[0]);
            t.col<I_CANTIDAD>()[s] = static_cast<u32>(e.v[IT_CANTIDAD]);
            // S_ItemStatus / S_ItemAmount traen los atributos si está identificado (así llega la
            // identificación con el pergamino).
            t.col<I_ATRIBUTOS>()[s] = guardar(inv, e.s[1]);
            if (!e.s[1].empty()) t.col<I_IDENTIFICADO>()[s] = 1;
            break;
        }
        case Ev::ITEM_BENDICION: {
            Slot s = t.buscar(e.id);
            if (s != SIN_SLOT) t.col<I_BENDICION>()[s] = static_cast<u8>(e.v[IT_BENDICION]);
            break;
        }
        default: break;
        }
    }
}

}  // namespace lin
