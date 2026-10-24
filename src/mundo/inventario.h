// Inventario del jugador (módulo "Inventory" del plan): tabla columnar por id de ítem.
//
// S_InvList lo llena al entrar; S_AddItem, S_DeleteInventoryItem, S_ItemName, S_ItemAmount y
// S_ItemColor lo actualizan. El nombre llega armado por el servidor ("$N", cantidad, encantamiento
// y si está equipado: " ($9)" arma, " ($117)" armadura).
#pragma once
#include <string_view>
#include <vector>
#include "core/tabla_soa.h"
#include "proto/eventos.h"

namespace lin {

struct TextoRef {
    u32 ofs = 0;
    u16 len = 0;
};

enum ColItem : size_t {
    I_GFX,          // ícono de inventario (N.tbt)
    I_USO,          // tipo de uso (L1J ItemTable: 1 arma, 2 armadura, 59 curación...)
    I_BENDICION,    // 0 normal, 1 bendito, 2 maldito (color del nombre)
    I_CANTIDAD,
    I_IDENTIFICADO,
    I_NOMBRE,
};
using TablaItems = TablaSoa<u16, u8, u8, u32, u8, TextoRef>;

constexpr u8 USO_ARMA = 1, USO_ARMADURA = 2;

struct Inventario {
    TablaItems items;
    std::vector<char> textos;  // nombres; se vacía con cada S_InvList
    void limpiar() {
        items.limpiar();
        textos.clear();
    }
};

std::string_view nombre_item(const Inventario& inv, Slot s);
// Equipado según el sufijo que agrega el servidor al nombre.
bool item_equipado(std::string_view nombre);
// Aplica los eventos de ítems (los demás se ignoran).
void aplicar_inventario(Inventario& inv, std::span<const Evento> eventos);

}  // namespace lin
