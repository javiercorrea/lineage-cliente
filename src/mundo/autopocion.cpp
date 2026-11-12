#include "mundo/autopocion.h"

namespace lin {

namespace {

// $N del nombre (desc-e.tbl) e ícono de inventario de cada poción de curación: HealingPotion.xml y
// etc_items del servidor L1J-JP (ver la tabla de PLAN_JUGABILIDAD.md, sección 0); los mismos en
// L1J-EN. Colores: 0 roja, 1 naranja, 2 transparente.
// El 3.80C (L1JGO-Whale) manda los nombres en chino, sin $N, y con el mismo tipo de uso que un ítem
// común: ahí se las reconoce por el ícono. Los íconos son de esas pociones, salvo el 80 (también la
// poción de curación de la Torre de Marfil, que cura igual) y el 1161 (también la solución de mithril
// negro, un ítem de misión que no se apila): por eso el 1161 solo cuenta con más de una unidad.
enum Familia : u8 { CONDENSADA, ANTIGUA, NORMAL, FAMILIAS };
struct TipoPocion {
    u32 n;
    Familia familia;
    u8 color;
    u16 gfx;
};
constexpr u16 GFX_COMPARTIDO = 1161;  // antigua roja y solución de mithril negro
constexpr TipoPocion POCIONES[] = {
    {1251, CONDENSADA, 0, 535}, {1252, CONDENSADA, 1, 532}, {1253, CONDENSADA, 2, 536},
    {2575, ANTIGUA, 0, 1161},   {2576, ANTIGUA, 1, 1162},   {2577, ANTIGUA, 2, 1163},
    {26, NORMAL, 0, 80},        {235, NORMAL, 1, 78},       {238, NORMAL, 2, 81},
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
        const u16 gfx = t.col<I_GFX>()[s];
        for (const TipoPocion& p : POCIONES) {
            const bool es = n ? p.n == n : p.gfx == gfx && (gfx != GFX_COMPARTIDO || t.col<I_CANTIDAD>()[s] > 1);
            if (es && !hay[p.familia][p.color]) hay[p.familia][p.color] = t.ids()[s];
        }
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
