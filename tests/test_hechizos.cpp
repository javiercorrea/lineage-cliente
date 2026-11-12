// Entregable 6.5: hechizos. Paquetes de L1J-JP 3.63c (formatos de S_*.java, y los bytes de
// S_AddSkill y S_SkillIconShield tal como los mandó el servidor local), mundo y datos del cliente.
#include <doctest/doctest.h>
#include "comun.h"
#include "mundo/mundo.h"
#include "proto/comandos.h"
#include "proto/decodificar.h"
#include "plataforma/rutas.h"
#include "recursos/hechizos.h"

using namespace lin;

namespace {
struct Decodificador {
    TablaOpcodes t;
    Arena arena{4096};
    std::vector<Evento> ev;
    Decodificador() { tabla_opcodes("jp363", t); }
    const std::vector<Evento>& operator()(std::initializer_list<u8> p) {
        std::vector<u8> b(p);
        ev.clear();
        REQUIRE(decodificar({b.data(), b.size()}, t, arena, ev));
        return ev;
    }
};
}  // namespace

TEST_CASE("Hechizos: S_AddSkill, S_DelSkill y C_UseSkill") {
    Decodificador d;
    // .addskill de un príncipe: niveles 1 y 2 completos y la fila 15 (príncipe).
    auto& ev = d({48, 32, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::HECHIZOS);
    Mundo m;
    aplicar_eventos(m, ev, 0);
    CHECK(m.jugador.sabe(1));    // Lesser Heal
    CHECK(m.jugador.sabe(4));    // Energy Bolt
    CHECK(m.jugador.sabe(16));
    CHECK_FALSE(m.jugador.sabe(17));
    CHECK(m.jugador.sabe(113));  // True Target (fila 15)
    CHECK(m.jugador.sabe(120));
    // S_DelSkill de Energy Bolt (fila 1, bit 3).
    auto& ev2 = d({18, 32, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    aplicar_eventos(m, ev2, 0);
    CHECK_FALSE(m.jugador.sabe(4));
    CHECK(m.jugador.sabe(3));

    // C_UseSkill: Energy Bolt (id 4: fila 0, columna 3) sobre 0x1002abfa en (32691, 32864); Shield sin objetivo.
    Escritor eb = cmd::usar_hechizo(d.t, 4, 0x1002abfa, 32691, 32864);  // el Bytes apunta a su buffer
    Bytes b = eb.cuerpo();
    const u8 esperado[] = {115, 0, 3, 0xfa, 0xab, 0x02, 0x10, 0xB3, 0x7F, 0x60, 0x80, 0};
    REQUIRE(b.size() == sizeof esperado);
    CHECK(std::memcmp(b.data(), esperado, sizeof esperado) == 0);
    Escritor es = cmd::usar_hechizo(d.t, 3, 0, 0, 0);
    Bytes s = es.cuerpo();
    REQUIRE(s.size() == 4);
    CHECK(s[1] == 0);
    CHECK(s[2] == 2);
}

TEST_CASE("Hechizos: S_UseAttackSkill lleva el proyectil y el daño llega con él") {
    Decodificador d;
    // acción 18, lanzador 0x10, objetivo 0x20, golpe 6, dirección 2, secuencia, gfx 167, tipo 6,
    // de (100, 200) a (104, 200), 5 bytes en 0.
    auto& ev = d({142, 18, 0x10, 0, 0, 0, 0x20, 0, 0, 0, 6, 0, 2, 1, 0, 0, 0, 167, 0, 6, 100, 0, 200, 0, 104, 0, 200, 0,
                  0, 0, 0, 0, 0});
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[AT_GFX] == 167);
    CHECK(ev[0].v[AT_X0] == 100);
    CHECK(ev[0].v[AT_X1] == 104);
    CHECK(ev[0].v[AT_Y1] == 200);
    // El golpe cuerpo a cuerpo no trae proyectil.
    auto& g = d({142, 1, 0x10, 0, 0, 0, 0x20, 0, 0, 0, 1, 0, 3, 0, 0, 0, 0, 0});
    CHECK(g[0].v[AT_GFX] == 0);

    Mundo m;
    Evento p1{}, p2{};
    p1.tipo = p2.tipo = Ev::PONER_OBJETO;
    p1.id = 0x10;
    p2.id = 0x20;
    p1.v[OB_X] = 100;
    p2.v[OB_X] = 104;
    p1.v[OB_Y] = p2.v[OB_Y] = 200;
    Evento poner[] = {p1, p2};
    aplicar_eventos(m, poner, 0);
    Evento ataque = d({142, 18, 0x10, 0, 0, 0, 0x20, 0, 0, 0, 6, 0, 2, 1, 0, 0, 0, 167, 0, 6, 100, 0, 200, 0, 104, 0, 200,
                       0, 0, 0, 0, 0, 0})[0];
    Evento lote[] = {ataque};
    aplicar_eventos(m, lote, 1000);
    const EfectoVisual& ef = m.efectos[0];
    CHECK(ef.gfx == 167);
    CHECK(ef.vuelo_ms == 4 * MS_VUELO_CELDA);
    CHECK(ef.dir == 2);
    Slot so = m.objetos.buscar(0x20);
    CHECK(m.objetos.col<O_ACCION>()[so] == ACC_DANADO);
    CHECK(m.objetos.col<O_TACCION>()[so] == ef.inicio + ef.vuelo_ms);
}

TEST_CASE("Hechizos: efectos, haste, brave e íconos") {
    Decodificador d;
    Mundo m;
    m.jugador.id = 0x10015306;
    // S_SkillSound de Shield (gfx 221 = 0xdd) sobre el jugador, como lo mandó el servidor.
    auto& e1 = d({232, 0x06, 0x53, 0x01, 0x10, 0xdd, 0, 0, 0, 0, 0, 0, 0});
    REQUIRE(e1[0].tipo == Ev::EFECTO);
    aplicar_eventos(m, e1, 500);
    CHECK(m.efectos[0].gfx == 221);
    CHECK(m.efectos[0].objeto == 0x10015306);
    // S_SkillIconShield: 1800 s, tipo 5 (Shield).
    auto& e2 = d({69, 0x08, 0x07, 0x05, 0, 0, 0, 0});
    aplicar_eventos(m, e2, 1000);
    REQUIRE(m.jugador.n_efectos == 1);
    CHECK(m.jugador.efectos[0].clase == EA_ESCUDO);
    CHECK(m.jugador.efectos[0].tipo == 5);
    CHECK(m.jugador.efectos[0].fin == 1000 + 1800 * 1000);
    // S_SkillHaste propio (300 s) y después su fin.
    auto& e3 = d({149, 0x06, 0x53, 0x01, 0x10, 1, 0x2c, 0x01});
    aplicar_eventos(m, e3, 2000);
    CHECK(m.jugador.haste == 1);
    CHECK(m.jugador.n_efectos == 2);
    auto& e4 = d({149, 0x06, 0x53, 0x01, 0x10, 0, 0, 0});
    aplicar_eventos(m, e4, 3000);
    CHECK(m.jugador.haste == 0);
    CHECK(m.jugador.n_efectos == 1);
    // Aura (S_PacketBox 0x16): índice del hechizo y segundos.
    auto& e5 = d({40, 0x16, 113, 0x10, 0x0e});
    aplicar_eventos(m, e5, 4000);
    REQUIRE(m.jugador.n_efectos == 2);
    CHECK(m.jugador.efectos[1].clase == EA_AURA);
    CHECK(m.jugador.efectos[1].tipo == 113);
    // S_Strup (Enchant Mighty: 1200 s, FUE 21, peso 30, tipo 5) y S_Dexup (fin del de elfo, tipo 2).
    auto& e7 = d({120, 0xb0, 0x04, 21, 30, 5, 0, 0, 0, 0});
    aplicar_eventos(m, e7, 4500);
    REQUIRE(m.jugador.n_efectos == 3);
    CHECK(m.jugador.efectos[2].clase == EA_FUERZA);
    CHECK(m.jugador.efectos[2].tipo == 5);
    CHECK(m.jugador.efectos[2].fin == 4500 + 1200 * 1000);
    auto& e8 = d({28, 0, 0, 18, 2, 0, 0, 0, 0});
    REQUIRE(e8[0].tipo == Ev::ICONO_EFECTO);
    CHECK(e8[0].v[IE_CLASE] == IE_DESTREZA);
    CHECK(e8[0].v[IE_TIPO] == 2);
    auto& e9 = d({120, 0, 0, 18, 30, 5, 0, 0, 0, 0});  // fin de Enchant Mighty
    aplicar_eventos(m, e9, 4600);
    CHECK(m.jugador.n_efectos == 2);
    vencer_efectos(m.jugador, 1000 + 1800 * 1000);
    CHECK(m.jugador.n_efectos == 1);
    // S_RangeSkill tipo 0 (área alrededor del lanzador, como Tornado): el lanzador hace la acción y
    // el gráfico se ve una sola vez, sobre él (no sobre cada objetivo).
    auto& e6 = d({16, 18, 0x10, 0, 0, 0, 100, 0, 200, 0, 3, 1, 0, 0, 0, 0x78, 0x17, 0, 0, 0, 2, 0, 0x20, 0, 0, 0, 0x20, 0x21,
                  0, 0, 0, 0x20});
    REQUIRE(e6.size() == 3);
    CHECK(e6[0].tipo == Ev::CAMBIAR_DIRECCION);
    CHECK(e6[1].tipo == Ev::ACCION_OBJETO);
    CHECK(e6[1].v[0] == 18);
    CHECK(e6[2].tipo == Ev::EFECTO);
    CHECK(e6[2].id == 0x10);
    CHECK(e6[2].v[EF_GFX] == 6008);
    CHECK(e6[2].v[EF_AREA] == 1);  // lo reparte la app según el hechizo, en la celda del lanzador
    CHECK(e6[2].v[EF_X] == 100);
}

TEST_CASE("Hechizos: intervalos propios con haste y brave") {
    EstadoJugador j;
    CHECK(intervalo_propio(j, Intervalo::CAMINAR, 640, 10) == 672);
    CHECK(intervalo_propio(j, Intervalo::HECHIZO, 1480) == 1554);
    j.haste = 1;
    j.brave = 1;
    // 640 × 0,75 × 0,75 = 360 ms: con el 5 % no alcanzaría para encadenar pasos (× 0,97 y 10 ms antes).
    u32 paso = intervalo_propio(j, Intervalo::CAMINAR, 640, 10);
    CHECK((paso - 10) * 0.97 >= 360);
    CHECK(paso < 400);
    j.brave = 4;  // caminar rápido: solo al caminar
    CHECK(intervalo_propio(j, Intervalo::ATACAR, 800) == 630);
    {
        EstadoJugador k;
        k.tercera = true;  // tercer acelerador (S_Liquor 8): × 0,87
        CHECK(factor_intervalo(k, Intervalo::ATACAR) == doctest::Approx(0.87));
    }
    j.haste = 2;  // lento
    CHECK(intervalo_propio(j, Intervalo::ATACAR, 750) == 1050);
}

TEST_CASE("Hechizos: objetivo según la ficha del cliente") {
    CHECK_FALSE(hechizo_con_objetivo("Caster", false));
    CHECK(hechizo_con_objetivo("PC/NPC", true));
    CHECK(hechizo_con_objetivo("Caster or 1 Clan Member", false));
    CHECK_FALSE(hechizo_con_objetivo("Area of Effect of 8 Cells From Caster", true));
    CHECK(hechizo_con_objetivo("Area of Effect Within 10 cells", true));   // Lightning Storm: con alcance
    CHECK_FALSE(hechizo_con_objetivo("Area of Effect Within 4 Cells of Caster", false));  // Tornado
    CHECK_FALSE(hechizo_con_objetivo("Any weapon", true));
    CHECK_FALSE(hechizo_con_objetivo("Every player in your party", false));
}

TEST_CASE("Hechizos: datos del cliente original" * doctest::skip(carpeta_juego().empty())) {
    Recursos r;
    REQUIRE(r.abrir(carpeta_juego()));
    TablaHechizos h;
    REQUIRE(h.cargar(r));
    CHECK(h[1].nombre == "Lesser Heal");
    CHECK(h[1].con_objetivo);
    CHECK_FALSE(h[1].de_ataque);
    CHECK(h[3].nombre == "Shield");
    CHECK_FALSE(h[3].con_objetivo);
    CHECK(h[3].mp == 8);
    CHECK(h[4].nombre == "Energy Bolt");
    CHECK(h[4].con_objetivo);
    CHECK(h[4].de_ataque);
    CHECK(h[4].mp == 3);
    CHECK(h[4].alcance == 10);
    CHECK(h[4].gfx == 167);
    CHECK(h[17].demora_ms == 500);  // Lightning
    CHECK(h[43].nombre == "Haste");
    CHECK_FALSE(h[53].con_objetivo);  // Tornado: alrededor del lanzador
    CHECK_FALSE(h[59].con_objetivo);  // Blizzard
    CHECK_FALSE(h[70].con_objetivo);  // Fire Storm
    CHECK_FALSE(h[80].con_objetivo);  // Freezing Blizzard
    CHECK(h[65].con_objetivo);        // Lightning Storm: área sobre un objetivo
    CHECK(h[74].con_objetivo);        // Meteor Strike
    CHECK(h[53].reparto == Hechizo::AREA_CUATRO);
    CHECK(h[53].area == 4);
    CHECK(h[59].reparto == Hechizo::AREA_LLENA);
    CHECK(h[59].area == 10);
    // Encantar arma o armadura: el ítem va en C_UseSkill; los buffs traen su duración.
    CHECK(h[12].sobre_item == Hechizo::SOBRE_ARMA);      // Enchant Weapon
    CHECK(h[12].duracion_s == 1800);
    CHECK(h[21].sobre_item == Hechizo::SOBRE_ARMADURA);  // Blessed Armor
    CHECK(h[48].sobre_arma_actual);                      // Bless Weapon: la equipada
    CHECK(h[48].sobre_item == Hechizo::SIN_ITEM);
    CHECK(h[42].icono != 0);                             // Enchant Mighty
    CHECK_FALSE(h[24].existe());    // el índice 23 no está en spelldesc-e.tbl
}

TEST_CASE("Atajos: valores por personaje en settings.ini") {
    std::string o;
    poner_valor_de_personaje(o, "Charla", "h3,i80:123,0");
    poner_valor_de_personaje(o, "Prueba", "0,h4");
    CHECK(valor_de_personaje(o, "Charla") == "h3,i80:123,0");
    CHECK(valor_de_personaje(o, "Prueba") == "0,h4");
    poner_valor_de_personaje(o, "Charla", "h1");  // reemplaza solo el suyo
    CHECK(valor_de_personaje(o, "Charla") == "h1");
    CHECK(valor_de_personaje(o, "Prueba") == "0,h4");
    CHECK(valor_de_personaje(o, "Otro").empty());
    CHECK(valor_de_personaje(o, "Char").empty());
}

TEST_CASE("Polymorph: S_ChangeShape cambia el sprite y la duración del paso") {
    Decodificador d;
    // Como lo mandó el servidor: Charla pasa a death knight (gfx 0x0ec8 = 3784), conserva el arma.
    auto& ev = d({164, 0x06, 0x53, 0x01, 0x10, 0xc8, 0x0e, 0x1d, 0, 0, 0, 0});
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::CAMBIO_FORMA);
    CHECK(ev[0].v[CF_GFX] == 3784);
    CHECK(ev[0].v[CF_ARMA] == 1);

    TablaSprites sprites;
    // Dos sprites: el 0 camina en 4 frames de 4 ticks (640 ms) y el 1 en 4 de 2 (320 ms).
    std::string tabla = std::string("x") + "1 0 1 4 0 0 4 0 1 4 0 2 4 0 3 4" + '\0' + "1 0 1 4 0 0 2 0 1 2 0 2 2 0 3 2" + '\0';
    REQUIRE(sprites.cargar(std::vector<u8>(tabla.begin(), tabla.end())));
    Mundo m;
    m.sprites = &sprites;
    Evento p{};
    p.tipo = Ev::PONER_OBJETO;
    p.id = 7;
    p.v[OB_ESTADO] = 4;  // con espada
    Evento poner[] = {p};
    aplicar_eventos(m, poner, 0);
    Slot s = m.objetos.buscar(7);
    CHECK(m.objetos.col<O_DURMOV>()[s] == 640);
    Evento f{};
    f.tipo = Ev::CAMBIO_FORMA;
    f.id = 7;
    f.v[CF_GFX] = 1;
    f.v[CF_ARMA] = 0;  // se sacó el arma
    Evento lote[] = {f};
    aplicar_eventos(m, lote, 1000);
    CHECK(m.objetos.col<O_GFX>()[s] == 1);
    CHECK(m.objetos.col<O_ESTADO>()[s] == 0);
    CHECK(m.objetos.col<O_DURMOV>()[s] == 320);

    // S_ShowHTML "monlist" (Shape Change) y las respuestas.
    auto& h = d({119, 0x06, 0x53, 0x01, 0x10, 'm', 'o', 'n', 'l', 'i', 's', 't', 0, 0, 0, 0});
    REQUIRE(h.size() == 1);
    CHECK(h[0].tipo == Ev::HTML);
    CHECK(h[0].s[0] == "monlist");
    Escritor u = cmd::usar_item_texto(d.t, 0x10, "death knight");
    Bytes bu = u.cuerpo();
    CHECK(bu[0] == 44);
    CHECK(std::string_view(reinterpret_cast<const char*>(bu.data()) + 5) == "death knight");
    Escritor n = cmd::accion_npc(d.t, 0x10015306, "death knight");
    Bytes bn = n.cuerpo();
    CHECK(bn[0] == 37);
    CHECK(bn[1] == 0x06);
    CHECK(std::string_view(reinterpret_cast<const char*>(bn.data()) + 5) == "death knight");
}

TEST_CASE("Flechas: alcance por arma y la flecha vuela hasta el objetivo") {
    CHECK(alcance_ataque(ARMA_ARCO) == 18);
    CHECK(alcance_ataque(ARMA_GUANTELETE) == 18);
    CHECK(alcance_ataque(ARMA_LANZA) == 2);
    CHECK(alcance_ataque(4) == 1);  // espada
    CHECK(alcance_ataque(0) == 1);
    // S_UseArrowSkill como lo mandó el servidor: acción 1, Charla (0x10015306) a un goblin, golpe 6,
    // dirección 1, secuencia, flecha 66, 127, de (32691, 32864) a (32695, 32860), 5 bytes en 0.
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(1024);
    std::vector<Evento> ev;
    const u8 p[] = {142, 1, 0x06, 0x53, 0x01, 0x10, 0xab, 0xad, 0x02, 0x10, 6, 0, 1, 7, 0, 0, 0, 66, 0, 127,
                    0xB3, 0x7F, 0x60, 0x80, 0xB7, 0x7F, 0x5C, 0x80, 0, 0, 0, 0, 0};
    REQUIRE(decodificar({p, sizeof p}, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[AT_GFX] == 66);
    CHECK(ev[0].v[AT_X1] == 32695);
    CHECK(ev[0].v[AT_Y1] == 32860);
    Mundo m;
    Evento p1{}, p2{};
    p1.tipo = p2.tipo = Ev::PONER_OBJETO;
    p1.id = 0x10015306;
    p2.id = 0x1002adab;
    p1.v[OB_X] = 32691;
    p1.v[OB_Y] = 32864;
    p2.v[OB_X] = 32695;
    p2.v[OB_Y] = 32860;
    Evento poner[] = {p1, p2, ev[0]};
    aplicar_eventos(m, poner, 5000);
    CHECK(m.efectos[0].gfx == 66);
    CHECK(m.efectos[0].vuelo_ms == 4 * MS_VUELO_CELDA);
    CHECK(m.efectos[0].dir == 1);
}
