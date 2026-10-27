#include <doctest/doctest.h>
#include "mundo/mundo.h"
#include "proto/comandos.h"
#include "proto/decodificar.h"

using namespace lin;

TEST_CASE("Opcodes JP 3.63c: ida y vuelta de la tabla") {
    TablaOpcodes t;
    REQUIRE(tabla_opcodes("jp363", t));
    CHECK(t.srv(161) == Srv::INITPACKET);
    CHECK(t.srv(122) == Srv::MOVEOBJECT);
    CHECK(t.cli(Cli::MOVECHAR) == 95);
    CHECK(t.cli(Cli::LOGINPACKET) == 57);
    CHECK_FALSE(tabla_opcodes("inexistente", t));
}

TEST_CASE("Escritor: relleno a múltiplo de 4 y formato de C_AuthLogin") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Escritor e = cmd::login(t, "ab", "c");
    Bytes b = e.cuerpo();
    REQUIRE(b.size() == 8);
    const u8 esperado[8] = {57, 'a', 'b', 0, 'c', 0, 0, 0};
    CHECK(std::memcmp(b.data(), esperado, 8) == 0);
    CHECK(cmd::lista_personajes(t).cuerpo().size() == 4);
}

TEST_CASE("Decodificar: S_MoveCharPacket lleva la posición anterior") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    // id 0x10, x 100, y 200, dirección 3 (x+1, y+1)
    const u8 p[] = {122, 0x10, 0, 0, 0, 100, 0, 200, 0, 3, 129, 0, 0, 0, 0};
    Arena arena(1024);
    std::vector<Evento> ev;
    REQUIRE(decodificar({p, sizeof p}, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::MOVER_OBJETO);
    CHECK(ev[0].id == 0x10);
    CHECK(ev[0].v[0] == 101);
    CHECK(ev[0].v[1] == 201);
}

TEST_CASE("Decodificar: S_CharPacks y paquete truncado") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    std::vector<u8> p = {184, 'Z', 'e', 0, 0, 1, 0, 0x10, 0, 14, 0, 2, 0, 10, 1, 21, 10, 10, 11, 13, 10, 0, 0, 0, 0, 0, 0};
    Arena arena(1024);
    std::vector<Evento> ev;
    REQUIRE(decodificar(p, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::PERSONAJE);
    CHECK(ev[0].s[0] == "Ze");
    CHECK(ev[0].v[PJ_CLASE] == 1);
    CHECK(ev[0].v[PJ_HP] == 14);
    CHECK(ev[0].v[PJ_FUE] == 21);
    ev.clear();
    std::vector<u8> corto = {145, 1, 2};  // OWNCHARSTATUS truncado
    CHECK_FALSE(decodificar(corto, t, arena, ev));
    CHECK(ev.empty());
}

TEST_CASE("Combate: S_AttackPacket, S_HpMeter, C_Attack y C_Restart") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(1024);
    std::vector<Evento> ev;
    // S_AttackPacket: acción 1, atacante 0x10, objetivo 0x20, golpe 1, dirección 3, + 5 bytes
    const u8 a[] = {142, 1, 0x10, 0, 0, 0, 0x20, 0, 0, 0, 1, 0, 3, 0, 0, 0, 0, 0};
    REQUIRE(decodificar({a, sizeof a}, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::ATAQUE);
    CHECK(ev[0].id == 0x10);
    CHECK(ev[0].v[AT_OBJETIVO] == 0x20);
    CHECK(ev[0].v[AT_ACCION] == 1);
    CHECK(ev[0].v[AT_GOLPE] == 1);
    CHECK(ev[0].v[AT_DIR] == 3);
    ev.clear();
    const u8 h[] = {128, 0x20, 0, 0, 0, 55};
    REQUIRE(decodificar({h, sizeof h}, t, arena, ev));
    CHECK(ev[0].tipo == Ev::HP_OBJETO);
    CHECK(ev[0].v[0] == 55);

    Escritor e = cmd::atacar(t, 0x20, 32691, 32864);
    Bytes b = e.cuerpo();
    const u8 esperado[] = {68, 0x20, 0, 0, 0, 0xB3, 0x7F, 0x60, 0x80, 0, 0, 0};
    REQUIRE(b.size() == sizeof esperado);
    CHECK(std::memcmp(b.data(), esperado, sizeof esperado) == 0);
    Escritor r = cmd::reiniciar(t);
    CHECK(r.cuerpo()[0] == 71);
}

namespace {
// Ítem en el formato de S_InvList / S_AddItem.
void poner_item(std::vector<u8>& p, u32 id, u8 uso, u16 gfx, u32 cantidad, const char* nombre) {
    auto d = [&](u32 v) { for (int i = 0; i < 4; ++i) p.push_back(static_cast<u8>(v >> (8 * i))); };
    d(id);
    p.insert(p.end(), {0, 0, uso, 0, static_cast<u8>(gfx), static_cast<u8>(gfx >> 8), 0});
    d(cantidad);
    p.push_back(1);
    for (const char* c = nombre; *c; ++c) p.push_back(static_cast<u8>(*c));
    p.push_back(0);
    p.insert(p.end(), {2, 0xAA, 0xBB});  // 2 bytes de atributos
    p.insert(p.end(), {10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
}
}  // namespace

TEST_CASE("Ítems: S_InvList (varios eventos), S_AddItem, S_ItemAmount y comandos") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(4096);
    std::vector<Evento> ev;
    std::vector<u8> p = {180, 2};
    poner_item(p, 0x11, 1, 34, 1, "$1 ($9)");
    poner_item(p, 0x22, 59, 80, 5, "$2");
    REQUIRE(decodificar(p, t, arena, ev));
    REQUIRE(ev.size() == 3);
    CHECK(ev[0].tipo == Ev::INVENTARIO_VACIAR);
    CHECK(ev[1].tipo == Ev::ITEM);
    CHECK(ev[1].id == 0x11);
    CHECK(ev[1].v[IT_GFX] == 34);
    CHECK(ev[1].s[0] == "$1 ($9)");
    CHECK(ev[2].id == 0x22);
    CHECK(ev[2].v[IT_USO] == 59);
    CHECK(ev[2].v[IT_CANTIDAD] == 5);

    ev.clear();
    p = {63};
    poner_item(p, 0x33, 0, 5, 1000, "$4");
    REQUIRE(decodificar(p, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::ITEM);
    CHECK(ev[0].v[IT_CANTIDAD] == 1000);

    ev.clear();
    std::vector<u8> c = {127, 0x22, 0, 0, 0, '$', '2', 0, 4, 0, 0, 0, 0};
    REQUIRE(decodificar(c, t, arena, ev));
    CHECK(ev[0].tipo == Ev::ITEM_CANTIDAD);
    CHECK(ev[0].v[IT_CANTIDAD] == 4);

    Escritor u = cmd::usar_item(t, 0x22);
    Bytes bu = u.cuerpo();
    const u8 eu[] = {44, 0x22, 0, 0, 0, 0, 0, 0};
    REQUIRE(bu.size() == sizeof eu);
    CHECK(std::memcmp(bu.data(), eu, sizeof eu) == 0);
    Escritor r = cmd::recoger(t, 1, 2, 0x33, 1000);
    Bytes br = r.cuerpo();
    const u8 er[] = {188, 1, 0, 2, 0, 0x33, 0, 0, 0, 0xE8, 0x03, 0, 0, 0, 0, 0};
    REQUIRE(br.size() == sizeof er);
    CHECK(std::memcmp(br.data(), er, sizeof er) == 0);
    CHECK(cmd::soltar(t, 1, 2, 0x33, 1).cuerpo()[0] == 54);
}

TEST_CASE("Social: preguntas, intercambio, título, emblema, página del clan y comandos") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(4096);
    std::vector<Evento> ev;
    std::vector<u8> p = {155, 0, 0, 7, 0, 0, 0, 0xB9, 0x03, 'C', 'h', 'a', 'r', 'l', 'a', 0};
    REQUIRE(decodificar(p, t, arena, ev));
    CHECK(ev[0].tipo == Ev::PREGUNTA);
    CHECK(ev[0].v[PR_NUMERO] == 7);
    CHECK(ev[0].v[PR_TIPO] == 953);
    CHECK(ev[0].s[0] == "Charla");

    ev.clear();
    p = {77, 'P', 'r', 'u', 'e', 'b', 'a', 0};
    REQUIRE(decodificar(p, t, arena, ev));
    CHECK(ev[0].tipo == Ev::INTERCAMBIO_ABRIR);
    CHECK(ev[0].s[0] == "Prueba");
    ev.clear();
    p = {86, 1, 0x2C, 0x01, '$', '2', '3', '4', 0, 1, 0};
    REQUIRE(decodificar(p, t, arena, ev));
    CHECK(ev[0].tipo == Ev::INTERCAMBIO_ITEM);
    CHECK(ev[0].v[IN_LADO] == 1);
    CHECK(ev[0].v[IN_GFX] == 300);
    CHECK(ev[0].s[0] == "$234");
    CHECK(ev[0].v[IN_BENDICION] == 1);
    ev.clear();
    p = {239, 1};
    REQUIRE(decodificar(p, t, arena, ev));
    CHECK(ev[0].tipo == Ev::INTERCAMBIO_FIN);
    CHECK(ev[0].v[IN_CANCELADO] == 1);

    ev.clear();
    p = {202, 0x10, 0, 0, 0, 'J', 'e', 'f', 'e', 0};
    REQUIRE(decodificar(p, t, arena, ev));
    CHECK(ev[0].tipo == Ev::TITULO);
    CHECK(ev[0].id == 0x10);
    CHECK(ev[0].s[0] == "Jefe");

    ev.clear();
    p = {50, 0x20, 0, 0, 0};
    for (size_t i = 0; i < BYTES_EMBLEMA; ++i) p.push_back(static_cast<u8>(i));
    p.insert(p.end(), {0, 0, 0});
    REQUIRE(decodificar(p, t, arena, ev));
    CHECK(ev[0].tipo == Ev::EMBLEMA);
    CHECK(ev[0].id == 0x20);
    REQUIRE(ev[0].s[0].size() == BYTES_EMBLEMA);
    CHECK(static_cast<u8>(ev[0].s[0][383]) == 383 % 256);

    // S_Pledge: 3 datos que la página lee como 5 (con las cantidades).
    ev.clear();
    p = {119, 1, 0, 0, 0};
    for (const char* s : {"pledge", "\x01", "\x03", "Clan", "", "Ana Beto "}) {
        if (s[0] == '\x01' || s[0] == '\x03') {
            p.insert(p.end(), {static_cast<u8>(s[0]), 0});
            continue;
        }
        for (const char* c = s; *c; ++c) p.push_back(static_cast<u8>(*c));
        p.push_back(0);
    }
    REQUIRE(decodificar(p, t, arena, ev));
    CHECK(ev[0].tipo == Ev::HTML);
    CHECK(ev[0].s[0] == "pledge");
    CHECK(ev[0].v[HT_DATOS] == 5);
    CHECK(ev[0].s[1] == std::string_view("Clan\x01" "0\x01\x01" "2\x01" "Ana Beto "));

    Escritor r = cmd::responder(t, 7, 953, true);
    const u8 er[] = {61, 0, 0, 7, 0, 0, 0, 0xB9, 0x03, 1, 0};
    Bytes br = r.cuerpo();
    REQUIRE(br.size() >= sizeof er);
    CHECK(std::memcmp(br.data(), er, sizeof er) == 0);
    Escritor o = cmd::ofrecer_item(t, 0x22, 3);
    const u8 eo[] = {241, 0x22, 0, 0, 0, 3, 0, 0, 0};
    Bytes bo = o.cuerpo();
    REQUIRE(bo.size() >= sizeof eo);
    CHECK(std::memcmp(bo.data(), eo, sizeof eo) == 0);
    CHECK(cmd::aceptar_intercambio(t).cuerpo()[0] == 110);
    CHECK(cmd::cancelar_intercambio(t).cuerpo()[0] == 167);
    CHECK(cmd::invitar_grupo(t, 1).cuerpo()[0] == 166);
    CHECK(cmd::crear_clan(t, "X").cuerpo()[0] == 154);
    u8 emblema[BYTES_EMBLEMA] = {};
    Escritor em = cmd::subir_emblema(t, emblema);
    CHECK(em.cuerpo()[0] == 107);
    CHECK(em.cuerpo().size() >= 1 + BYTES_EMBLEMA);
}

TEST_CASE("Maestro de magia: S_SkillBuy, C_SkillBuy y C_SkillBuyOK") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(1024);
    std::vector<Evento> ev;
    std::vector<u8> p = {222, 100, 0, 0, 0, 3, 0, 1, 0, 0, 0, 2, 0, 0, 0, 40, 0, 0, 0};
    REQUIRE(decodificar(p, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::MAGIA_EN_VENTA);
    CHECK(static_cast<u32>(ev[0].v[MV_MASCARA]) == 0b110u);
    CHECK(static_cast<u32>(ev[0].v[MV_MASCARA + 1]) == 1u << 8);

    Escritor c = cmd::pedir_magia(t, 0x1003F87F);
    const u8 ec[] = {173, 0x7F, 0xF8, 0x03, 0x10};
    REQUIRE(c.cuerpo().size() >= sizeof ec);
    CHECK(std::memcmp(c.cuerpo().data(), ec, sizeof ec) == 0);
    const u32 indices[] = {1, 2};
    Escritor ok = cmd::aprender_magia(t, indices);
    const u8 eo[] = {207, 2, 0, 1, 0, 0, 0, 2, 0, 0, 0};
    REQUIRE(ok.cuerpo().size() >= sizeof eo);
    CHECK(std::memcmp(ok.cuerpo().data(), eo, sizeof eo) == 0);
}

TEST_CASE("Puertas, estados alterados, cantidad y mascotas: decodificar, aplicar y comandos") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(4096);
    std::vector<Evento> ev;
    auto uno = [&](std::vector<u8> p) {
        ev.clear();
        REQUIRE(decodificar(p, t, arena, ev));
        REQUIRE(ev.size() == 1);
        return ev[0];
    };
    // S_Door de la posada de Talking Island (de la traza): (32597, 32935), cerrada y después abierta.
    Evento e = uno({35, 0x55, 0x7f, 0xa7, 0x80, 0x00, 0x01});
    CHECK(e.tipo == Ev::PUERTA);
    CHECK(e.v[PU_X] == 32597);
    CHECK(e.v[PU_Y] == 32935);
    CHECK(e.v[PU_BLOQUEADA] == 1);
    Mundo m;
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(m.puertas_cerradas.buscar((32597ull << 16) | 32935));
    e = uno({35, 0x55, 0x7f, 0xa7, 0x80, 0x00, 0x00});
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(!m.puertas_cerradas.buscar((32597ull << 16) | 32935));

    // S_Paralysis: sueño (10) y su fin (11); el fin de otro estado no lo quita.
    e = uno({165, 10});
    CHECK(e.tipo == Ev::PARALISIS);
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(m.jugador.inmovil == SUENO);
    e = uno({165, 3});
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(m.jugador.inmovil == SUENO);
    e = uno({165, 11});
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(m.jugador.inmovil == 0);

    // S_Poison sobre un objeto: verde y después gris.
    Evento p{};
    p.tipo = Ev::PONER_OBJETO;
    p.id = 77;
    aplicar_eventos(m, std::span(&p, 1), 0);
    e = uno({93, 77, 0, 0, 0, 1, 0});
    CHECK(e.tipo == Ev::VENENO);
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(m.objetos.col<O_FLAGS>()[m.objetos.buscar(77)] == FLAG_VENENO);
    e = uno({93, 77, 0, 0, 0, 0, 1});
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(m.objetos.col<O_FLAGS>()[m.objetos.buscar(77)] == FLAG_CONGELADO);

    // S_CurseBlind (de la traza: Blinding Potion).
    e = uno({238, 1, 0});
    CHECK(e.tipo == Ev::CEGUERA);
    aplicar_eventos(m, std::span(&e, 1), 0);
    CHECK(m.jugador.ceguera == 1);

    // S_InputAmount: S_HowManyMake devuelve la acción (segundo texto); S_HowManyKey la página.
    std::vector<u8> fab = {253, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0, 0, 0};
    for (char c : std::string_view("request")) fab.push_back(static_cast<u8>(c));
    fab.push_back(0);
    for (char c : std::string_view("make1")) fab.push_back(static_cast<u8>(c));
    fab.push_back(0);
    e = uno(fab);
    CHECK(e.tipo == Ev::PEDIR_CANTIDAD);
    CHECK(e.id == 9);
    CHECK(e.v[PC_MIN] == 1);
    CHECK(e.v[PC_MAX] == 5);
    CHECK(e.s[0] == "make1");
    std::vector<u8> llave = {253, 9, 0, 0, 0, 44, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 0, 0,
                             'i', 'n', 'n', '2', 0, 1, 0, 2, 0, 'S', 'e', 'l', 'e', 'n', 'a', 0, '3', '0', '0', 0};
    e = uno(llave);
    CHECK(e.v[PC_PRECIO] == 300);
    CHECK(e.v[PC_MAX] == 8);
    CHECK(e.s[0] == "inn2");
    CHECK(e.s[1] == "Selena");

    // S_PetCtrlMenu: entra (con nombre) y sale; con otro primer byte (S_CharReset) se ignora.
    e = uno({33, 0x0c, 3, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0x10, 0x7f, 0x20, 0x80, 'D', 'o', 'g', 0});
    CHECK(e.tipo == Ev::MASCOTA);
    CHECK(e.id == 5);
    CHECK(e.v[MA_AGREGAR] == 1);
    CHECK(e.s[0] == "Dog");
    e = uno({33, 0x0c, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0});
    CHECK(e.v[MA_AGREGAR] == 0);
    ev.clear();
    std::vector<u8> reinicio = {33, 0x01, 0, 0};
    REQUIRE(decodificar(reinicio, t, arena, ev));
    CHECK(ev.empty());  // S_CharReset (mismo número): ignorado
    e = uno({177, 5, 0, 0, 0, 0, 0, 2});
    CHECK(e.tipo == Ev::ELEGIR_OBJETIVO_MASCOTA);
    CHECK(e.id == 5);

    // Comandos.
    auto igual = [](Escritor w, std::vector<u8> esperado) {
        REQUIRE(w.cuerpo().size() >= esperado.size());
        CHECK(std::memcmp(w.cuerpo().data(), esperado.data(), esperado.size()) == 0);
    };
    igual(cmd::puerta(t, 32597, 32934, 0x100359d7), {199, 0x55, 0x7f, 0xa6, 0x80, 0xd7, 0x59, 0x03, 0x10});
    igual(cmd::dar_item(t, 5, 1, 2, 7, 3), {244, 5, 0, 0, 0, 1, 0, 2, 0, 7, 0, 0, 0, 3, 0, 0, 0});
    igual(cmd::cantidad(t, 9, 2, "make1"), {109, 9, 0, 0, 0, 2, 0, 0, 0, 0, 'm', 'a', 'k', 'e', '1', 0});
    igual(cmd::inventario_mascota(t, 5), {217, 5, 0, 0, 0});
    igual(cmd::borrar_item(t, 0x33, 2), {209, 0x33, 0, 0, 0, 2, 0, 0, 0});
    igual(cmd::usar_item_mascota(t, 5, 2), {213, 0, 5, 0, 0, 0, 2});
    igual(cmd::objetivo_mascota(t, 5, 77), {155, 5, 0, 0, 0, 0, 77, 0, 0, 0});
}

TEST_CASE("Reparar armas, marcadores y cambiar de personaje: decodificar y comandos") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(4096);
    std::vector<Evento> ev;
    // S_FixWeaponList de la traza: 200 por punto, 1 arma (0x10015307) con daño 3.
    std::vector<u8> fix = {208, 0xc8, 0, 0, 0, 1, 0, 0x07, 0x53, 0x01, 0x10, 3};
    REQUIRE(decodificar(fix, t, arena, ev));
    REQUIRE(ev.size() == 2);
    CHECK(ev[0].tipo == Ev::LISTA_TIENDA);
    CHECK(ev[0].v[LT_TIPO] == LISTA_REPARAR);
    CHECK(ev[1].tipo == Ev::ITEM_TIENDA);
    CHECK(ev[1].id == 0x10015307u);
    CHECK(ev[1].v[IT_CANTIDAD] == 3);
    CHECK(ev[1].v[TI_PRECIO] == 600);
    // S_Bookmarks: s nombre, h mapa, d id, h x, h y.
    ev.clear();
    std::vector<u8> bm = {11, 'B', 'o', 's', 'q', 'u', 'e', 0, 4, 0, 1, 0, 0, 0, 0x56, 0x7f, 0xe8, 0x80};
    REQUIRE(decodificar(bm, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::MARCADOR);
    CHECK(ev[0].s[0] == "Bosque");
    CHECK(ev[0].v[MK_MAPA] == 4);
    CHECK(ev[0].id == 1);
    CHECK(ev[0].v[MK_X] == 32598);
    CHECK(ev[0].v[MK_Y] == 33000);

    auto igual = [](Escritor w, std::vector<u8> esperado) {
        REQUIRE(w.cuerpo().size() >= esperado.size());
        CHECK(std::memcmp(w.cuerpo().data(), esperado.data(), esperado.size()) == 0);
    };
    igual(cmd::cambiar_personaje(t), {237});
    igual(cmd::armas_a_reparar(t), {106});
    igual(cmd::reparar_arma(t, 0x10015307, 0x1003fa8b), {238, 0x07, 0x53, 0x01, 0x10, 0x8b, 0xfa, 0x03, 0x10});
    igual(cmd::marcar_lugar(t, "Plaza"), {134, 'P', 'l', 'a', 'z', 'a', 0});
    igual(cmd::borrar_marcador(t, "Plaza"), {223, 'P', 'l', 'a', 'z', 'a', 0});
    igual(cmd::usar_teleport(t, 9, 4, 1), {44, 9, 0, 0, 0, 4, 0, 1, 0, 0, 0});
    igual(cmd::hechizo_teleport(t, 5, 4, 1), {115, 0, 4, 4, 0, 1, 0, 0, 0});
}
