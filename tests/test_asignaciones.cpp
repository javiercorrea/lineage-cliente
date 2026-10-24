// "Cero allocations en régimen" (PLAN_CLIENTE.md, principio 5), con operator new instrumentado
// (core/asignaciones.cpp está enlazado en los tests). Se ejercita lo que corre en cada frame sin
// GPU: decodificar paquetes, aplicarlos al mundo y al chat, interpolar posiciones, la precarga y el
// empaquetado del atlas. Primero una vuelta de calentamiento (las tablas y buffers crecen hasta
// su tamaño de trabajo) y después la misma vuelta no puede reservar memoria.
#include <doctest/doctest.h>
#include <chrono>
#include <thread>
#include "core/arena.h"
#include "core/asignaciones.h"
#include "mundo/chat.h"
#include "mundo/mundo.h"
#include "proto/decodificar.h"
#include "recursos/precarga.h"
#include "render/atlas.h"

using namespace lin;

TEST_CASE("Sin allocations en régimen: red, mundo, chat, precarga y atlas") {
    TablaOpcodes t;
    REQUIRE(tabla_opcodes("jp363", t));
    Arena arena(1 << 16);
    std::vector<Evento> ev;
    ev.reserve(256);
    static Mundo m;
    static Chat chat;
    TablaTexto vacia;
    Precarga precarga;
    EmpaquetadoAtlas atlas(2048, 4);
    static const u8 datos[64] = {};

    // 50 objetos que caminan y hablan.
    for (u32 id = 1; id <= 50; ++id) {
        Evento e{};
        e.tipo = Ev::PONER_OBJETO;
        e.id = id;
        e.v[OB_X] = 32700;
        e.v[OB_Y] = 32800;
        e.s[0] = "Guardia";
        Evento lote[] = {e};
        aplicar_eventos(m, lote, 0);
    }
    REQUIRE(precarga.listo(1, {datos, sizeof datos}) == false);
    for (int i = 0; i < 1000 && !precarga.listo(1, {datos, sizeof datos}); ++i) {
        precarga.actualizar();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(precarga.listo(1, {datos, sizeof datos}));

    auto vuelta = [&](u64 ahora) {
        for (u32 id = 1; id <= 50; ++id) {
            // S_MoveCharPacket: id, x, y, dirección.
            const u8 p[] = {122, static_cast<u8>(id), 0, 0, 0, 0xBC, 0x7F, 0x20, 0x80, 3, 129, 0, 0, 0, 0};
            arena.resetear();
            ev.clear();
            REQUIRE(decodificar({p, sizeof p}, t, arena, ev));
            aplicar_eventos(m, ev, ahora);
            Evento c{};
            c.tipo = Ev::CHAT;
            c.v[0] = CHAT_NORMAL;
            c.id = id;
            c.s[0] = "Guardia: hola";
            Evento lote[] = {c};
            aplicar_chat(chat, vacia, vacia, lote, ahora);
        }
        double x, y;
        for (Slot s = 0; s < m.objetos.tamano(); ++s) posicion_interpolada(m, s, ahora + 100, x, y);
        precarga.actualizar();
        CHECK(precarga.listo(1, {datos, sizeof datos}));
        RegionAtlas r;
        atlas.usar(0, ahora);
        CHECK(atlas.reservar(24, 24, ahora, r));
    };
    vuelta(1000);  // calentamiento
    u64 antes = asignaciones_hilo();
    for (u64 i = 0; i < 10; ++i) vuelta(2000 + i * 700);
    CHECK(asignaciones_hilo() - antes == 0);

    // El contador funciona (si asignaciones.cpp no estuviera enlazado, el test pasaría en falso).
    antes = asignaciones_hilo();
    std::vector<int> v(1000, 1);
    CHECK(v[999] == 1);
    CHECK(asignaciones_hilo() - antes == 1);
}
