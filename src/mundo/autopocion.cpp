#include "mundo/autopocion.h"

namespace lin {

namespace {

// $N del nombre (desc-e.tbl) de cada poción de curación: HealingPotion.xml y etc_items del servidor
// L1J-JP (ver la tabla de PLAN_JUGABILIDAD.md, sección 0). Colores: 0 roja, 1 naranja, 2 transparente.
enum Familia : u8 { CONDENSADA, ANTIGUA, NORMAL, FAMILIAS };
struct TipoPocion {
    u32 n;
    Familia familia;
    u8 color;
};
constexpr TipoPocion POCIONES[] = {
    {1251, CONDENSADA, 0}, {1252, CONDENSADA, 1}, {1253, CONDENSADA, 2},
    {2575, ANTIGUA, 0},    {2576, ANTIGUA, 1},    {2577, ANTIGUA, 2},
    {26, NORMAL, 0},       {235, NORMAL, 1},      {238, NORMAL, 2},
};
// Curación media de cada color (el centro del rango de HealingPotion.xml) y demora de la familia.
constexpr i32 MEDIA[FAMILIAS][3] = {{35, 60, 90}, {20, 32, 57}, {35, 60, 90}};
constexpr u32 DEMORA_MS[FAMILIAS] = {1000, 500, 1000};

}  // namespace

u32 numero_de_nombre(std::string_view nombre) {
    if (nombre.size() < 2 || nombre[0] != '$') return 0;
    u32 n = 0;
    for (size_t i = 1; i < nombre.size() && nombre[i] >= '0' && nombre[i] <= '9'; ++i) n = n * 10 + static_cast<u32>(nombre[i] - '0');
    return n;
}

PocionElegida elegir_pocion(const Inventario& inv, i32 hp, i32 hp_max) {
    // La primera de cada familia y color que haya en el inventario.
    ObjId hay[FAMILIAS][3] = {};
    const TablaItems& t = inv.items;
    for (Slot s = 0; s < t.tamano(); ++s) {
        if (!t.col<I_CANTIDAD>()[s]) continue;
        const u32 n = numero_de_nombre(nombre_item(inv, s));
        for (const TipoPocion& p : POCIONES)
            if (p.n == n && !hay[p.familia][p.color]) hay[p.familia][p.color] = t.ids()[s];
    }
    const i32 falta = hp_max - hp;
    for (int f = 0; f < FAMILIAS; ++f) {
        const int deseado = falta >= MEDIA[f][2] ? 2 : falta >= MEDIA[f][1] ? 1 : 0;
        for (int c = deseado; c >= 0; --c)  // el pedido o uno menor
            if (hay[f][c]) return {hay[f][c], DEMORA_MS[f]};
        for (int c = deseado + 1; c < 3; ++c)  // si no, uno mayor
            if (hay[f][c]) return {hay[f][c], DEMORA_MS[f]};
    }
    return {};
}

}  // namespace lin
