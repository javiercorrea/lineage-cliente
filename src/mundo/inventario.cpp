#include "mundo/inventario.h"

namespace lin {

namespace {

TextoRef guardar(Inventario& inv, std::string_view t) {
    TextoRef r{static_cast<u32>(inv.textos.size()), static_cast<u16>(t.size() > 0xFFFF ? 0xFFFF : t.size())};
    inv.textos.insert(inv.textos.end(), t.begin(), t.begin() + r.len);
    return r;
}

}  // namespace

std::string_view nombre_item(const Inventario& inv, Slot s) {
    TextoRef r = inv.items.col<I_NOMBRE>()[s];
    return {inv.textos.data() + r.ofs, r.len};
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
