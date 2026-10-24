// Hechizos y habilidades, con los datos del cliente original (Text.pak):
//  - spelldesc-e.tbl: "<índice> <Nombre> (<MP>/<HP>[/<material>])", el nombre que se muestra;
//  - spellexdesc-e.tbl: la ficha ("\fHTargets: \f>PC/NPC^..."), de donde sale el objetivo;
//  - spells: "<nombre> <índice> <Clase> mpCost=.. range=.. graphic=.. useDelay=.. ...;";
//  - spellicon.idx (en Sprite.pak): u16 por índice, el ícono del hechizo (N.tbt, 32×32).
// El hechizo de índice i es el skill id i + 1 del servidor (C_UseSkill: fila = i / 8, bit = i % 8).
//
// Contra la tabla skills de L1J-JP (183 hechizos):
//  - objetivo: coincide en 175. Los 8 restantes son casos especiales que el servidor resuelve
//    solo (un arma del inventario, el muro de fuego por celda, "Caster" que ataca al objetivo
//    actual...);
//  - acción con la que se lanza: las clases de "ataque" (CLASES_ATAQUE en hechizos.cpp) usan la
//    18 (SkillAttack) y las demás la 19 (SkillBuff), igual que action_id del servidor salvo 1.
#pragma once
#include <string_view>
#include <vector>
#include "recursos/pak.h"

namespace lin {

constexpr u32 CANTIDAD_HECHIZOS = 28 * 8;  // 28 filas (niveles) de 8 en S_AddSkill
constexpr u8 ACC_HECHIZO_ATAQUE = 18, ACC_HECHIZO = 19;  // ActionCodes.ACTION_SkillAttack / SkillBuff

struct Hechizo {
    std::string_view nombre;  // vacío si el índice no existe
    std::string_view ficha;   // spellexdesc-e.tbl, con los códigos de color (\f) y saltos (^)
    u16 mp = 0, hp = 0;
    u16 demora_ms = 0;        // useDelay: hasta el próximo hechizo (el servidor ignora los de antes)
    u16 gfx = 0;
    u16 icono = 0;            // N.tbt; 0 si no tiene
    u8 alcance = 0;           // en celdas; 0 = sin límite propio
    bool con_objetivo = false;  // se elige un personaje o monstruo; si no, se lanza al instante
    bool de_ataque = false;     // se lanza con la acción 18; si no, con la 19
    bool existe() const { return !nombre.empty(); }
};

class TablaHechizos {
public:
    bool cargar(const Recursos& r);
    // Por skill id (1..CANTIDAD_HECHIZOS); uno vacío si no existe.
    const Hechizo& operator[](u32 id) const;

private:
    std::vector<u8> desc_, ficha_, spells_;
    std::vector<Hechizo> hechizos_;
};

// Clasificación del objetivo a partir del texto "Targets:" de la ficha y de si el hechizo tiene
// alcance (separada para los tests).
bool hechizo_con_objetivo(std::string_view objetivos, bool tiene_alcance);

}  // namespace lin
