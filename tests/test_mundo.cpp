#include <doctest/doctest.h>
#include <chrono>
#include <thread>
#include "comun.h"
#include "plataforma/archivo_mapeado.h"
#include "mundo/mundo.h"

using namespace lin;

static Evento poner(ObjId id, int x, int y, std::string_view nombre) {
    Evento e{};
    e.tipo = Ev::PONER_OBJETO;
    e.id = id;
    e.v[OB_X] = x;
    e.v[OB_Y] = y;
    e.v[OB_GFX] = 0;
    e.s[0] = nombre;
    return e;
}

TEST_CASE("Mundo: poner, mover (con interpolación) y quitar objetos en lote") {
    Mundo m;
    Evento mov{};
    mov.tipo = Ev::MOVER_OBJETO;
    mov.id = 7;
    mov.v[0] = 11;
    mov.v[1] = 20;
    mov.v[2] = 2;
    Evento ev[] = {poner(7, 10, 20, "Guardia"), poner(8, 5, 5, "Otro"), mov};
    aplicar_eventos(m, ev, 1000);
    REQUIRE(m.objetos.tamano() == 2);
    Slot s = m.objetos.buscar(7);
    REQUIRE(s != SIN_SLOT);
    CHECK(texto(m, m.objetos.col<O_NOMBRE>()[s]) == "Guardia");
    CHECK(m.objetos.col<O_X>()[s] == 11);
    CHECK(moviendose(m, s, 1000 + DURACION_PASO_MS / 2));
    double x, y;
    posicion_interpolada(m, s, 1000 + DURACION_PASO_MS / 2, x, y);
    CHECK(x == doctest::Approx(10.5f));
    posicion_interpolada(m, s, 1000 + DURACION_PASO_MS, x, y);
    CHECK(x == doctest::Approx(11.0f));
    CHECK_FALSE(moviendose(m, s, 1000 + DURACION_PASO_MS));

    Evento q{};
    q.tipo = Ev::QUITAR_OBJETO;
    q.id = 8;
    Evento ev2[] = {q};
    aplicar_eventos(m, ev2, 2000);
    CHECK(m.objetos.tamano() == 1);
    CHECK(m.objetos.buscar(8) == SIN_SLOT);
    CHECK(m.objetos.buscar(7) != SIN_SLOT);
}

TEST_CASE("Mundo: cambio de mapa descarta los objetos salvo el jugador") {
    Mundo m;
    Evento st{};
    st.tipo = Ev::ESTADO_PROPIO;
    st.id = 1;
    Evento ev[] = {st, poner(1, 3, 3, "Yo"), poner(2, 4, 4, "NPC")};
    aplicar_eventos(m, ev, 0);
    Evento mapa{};
    mapa.tipo = Ev::MAPA;
    mapa.v[0] = 4;
    Evento ev2[] = {mapa};
    aplicar_eventos(m, ev2, 0);
    CHECK(m.mapa == 4);
    CHECK(m.objetos.tamano() == 1);
    CHECK(m.objetos.buscar(1) != SIN_SLOT);
}

TEST_CASE("Mundo: el paso dura lo que la animación de caminar del sprite") {
    // gfx 0: caminar = 9 frames × 4 ticks (1440 ms, la rana); gfx 1: "110 36" -> 24 ticks = 640 ms.
    const char reg[] = "?16 0 1 9 0 0 4 0 1 4> 0 2 4<41 0 3 4 0 4 4> 0 5 4 0 6 4 0 7 4> 0 8 4" "\0"
                       "48 110 36 0 1 12 0 0 2 0 1 2 0 2 2 0 3 2 0 4 2 0 5 2 0 6 2 0 7 2 0 8 2 0 9 2 0 10 2 0 11 2";
    TablaSprites sp;
    REQUIRE(sp.cargar(std::vector<u8>(reg, reg + sizeof reg - 1)));
    Mundo m;
    m.sprites = &sp;
    CHECK(duracion_paso(m, 0, 0, 0) == 1440);
    CHECK(duracion_paso(m, 1, 0, 0) == 640);
    CHECK(duracion_paso(m, 0, 0, 1) == 1080);  // haste
    CHECK(duracion_paso(m, 99, 0, 0) == DURACION_PASO_MS);

    Evento ev[] = {poner(5, 10, 10, "Rana")};
    aplicar_eventos(m, ev, 0);
    Slot s = m.objetos.buscar(5);
    CHECK(m.objetos.col<O_DURMOV>()[s] == 1440);
}

TEST_CASE("Mundo: un movimiento que llega antes de terminar el paso sigue desde donde se ve") {
    Mundo m;
    Evento ev[] = {poner(7, 10, 10, "Orco")};
    aplicar_eventos(m, ev, 0);
    Evento mov{};
    mov.tipo = Ev::MOVER_OBJETO;
    mov.id = 7;
    mov.v[0] = 11;
    mov.v[1] = 10;
    mov.v[2] = 2;
    Evento lote1[] = {mov};
    aplicar_eventos(m, lote1, 1000);
    Slot s = m.objetos.buscar(7);
    const u64 medio = 1000 + DURACION_PASO_MS / 2;
    double x0, y0;
    posicion_interpolada(m, s, medio, x0, y0);
    CHECK(x0 == doctest::Approx(10.5));
    // Llega el siguiente a mitad del paso: la posición en ese instante no salta.
    mov.v[0] = 12;
    Evento lote2[] = {mov};
    aplicar_eventos(m, lote2, medio);
    double x1, y1;
    posicion_interpolada(m, s, medio, x1, y1);
    CHECK(x1 == doctest::Approx(x0));
    posicion_interpolada(m, s, medio + DURACION_PASO_MS, x1, y1);
    CHECK(x1 == doctest::Approx(12.0));
    // La animación de caminar sigue un cuarto de paso más (el próximo paquete suele llegar tarde).
    CHECK(caminando(m, s, medio + DURACION_PASO_MS + DURACION_PASO_MS / 8));
    CHECK_FALSE(moviendose(m, s, medio + DURACION_PASO_MS + DURACION_PASO_MS / 8));
    CHECK_FALSE(caminando(m, s, medio + DURACION_PASO_MS * 2));
}

TEST_CASE("Mundo: el sprite se dibuja desde la celda de origen y el frame aporta el avance") {
    // Sin tabla de sprites: 4 frames iguales, que avanzan 0, ¼, ½, ¾ de celda.
    Mundo m;
    Evento ev[] = {poner(3, 10, 10, "Yo")};
    aplicar_eventos(m, ev, 0);
    Evento mov{};
    mov.tipo = Ev::MOVER_OBJETO;
    mov.id = 3;
    mov.v[0] = 11;  // dirección 2: x + 1, y igual
    mov.v[1] = 10;
    mov.v[2] = 2;
    Evento lote[] = {mov};
    aplicar_eventos(m, lote, 1000);
    Slot s = m.objetos.buscar(3);
    const u64 d = DURACION_PASO_MS;
    i32 x, y;
    origen_paso(m, s, 1000 + d / 2, x, y);
    CHECK(x == 10);
    CHECK(y == 10);
    CHECK(fraccion_frame(m, s, 1000) == doctest::Approx(0.0));
    CHECK(fraccion_frame(m, s, 1000 + d / 4) == doctest::Approx(0.25));
    CHECK(fraccion_frame(m, s, 1000 + d / 2 + 1) == doctest::Approx(0.5));
    CHECK(fraccion_frame(m, s, 1000 + d - 1) == doctest::Approx(0.75));
    // Terminado el paso: se dibuja en la celda nueva, sin avance del frame.
    origen_paso(m, s, 1000 + d, x, y);
    CHECK(x == 11);
    CHECK(fraccion_frame(m, s, 1000 + d) == doctest::Approx(0.0));
}

TEST_CASE("Mundo: ataque, daño en el frame de impacto, muerte, cadáver y reinicio") {
    // gfx 0 (atacante): atacar = 4 frames de 4 ticks, impacto ("!") en el tercero -> 320 ms.
    const char reg[] = "?16 1 1 4 24 0 4 24 1 4 24 2 4! 24 3 4";
    TablaSprites sp;
    REQUIRE(sp.cargar(std::vector<u8>(reg, reg + sizeof reg - 1)));
    CHECK(sp.ms_hasta_impacto(0, ACC_ATACAR) == 320);
    Mundo m;
    m.sprites = &sp;
    m.jugador.id = 1;
    Evento ev[] = {poner(1, 10, 10, "Yo"), poner(2, 11, 10, "Orco")};
    aplicar_eventos(m, ev, 0);
    Slot yo = m.objetos.buscar(1), orco = m.objetos.buscar(2);

    Evento at{};
    at.tipo = Ev::ATAQUE;
    at.id = 1;
    at.v[AT_OBJETIVO] = 2;
    at.v[AT_ACCION] = ACC_ATACAR;
    at.v[AT_GOLPE] = 1;
    at.v[AT_DIR] = 2;
    Evento l1[] = {at};
    aplicar_eventos(m, l1, 1000);
    CHECK(m.objetos.col<O_ACCION>()[yo] == ACC_ATACAR);
    CHECK(m.objetos.col<O_DIR>()[yo] == 2);
    CHECK(m.objetos.col<O_ACCION>()[orco] == ACC_DANADO);
    CHECK(m.objetos.col<O_TACCION>()[orco] == 1320);  // cuando lo alcanza el golpe

    // Fallo: el objetivo no reacciona.
    m.objetos.col<O_ACCION>()[orco] = 0xFF;
    at.v[AT_GOLPE] = 0;
    Evento l2[] = {at};
    aplicar_eventos(m, l2, 3000);
    CHECK(m.objetos.col<O_ACCION>()[orco] == 0xFF);

    // Muere el orco: queda como cadáver y ya no reacciona.
    Evento muere{};
    muere.tipo = Ev::ACCION_OBJETO;
    muere.id = 2;
    muere.v[0] = ACC_MORIR;
    Evento l3[] = {muere};
    aplicar_eventos(m, l3, 4000);
    CHECK(m.objetos.col<O_ESTADO>()[orco] == ESTADO_MUERTO);
    Evento dano{};
    dano.tipo = Ev::ACCION_OBJETO;
    dano.id = 2;
    dano.v[0] = ACC_DANADO;
    Evento l4[] = {dano};
    aplicar_eventos(m, l4, 5000);
    CHECK(m.objetos.col<O_ACCION>()[orco] == ACC_MORIR);

    // Un cadáver que aparece ya muerto (estado 8 en S_CharPack).
    Evento cad = poner(3, 12, 12, "Cadáver");
    cad.v[OB_ESTADO] = ESTADO_MUERTO;
    Evento l5[] = {cad};
    aplicar_eventos(m, l5, 6000);
    CHECK(m.objetos.col<O_ACCION>()[m.objetos.buscar(3)] == ACC_MORIR);

    // Muere el jugador y reinicia: se descarta todo salvo él.
    muere.id = 1;
    Evento l6[] = {muere};
    aplicar_eventos(m, l6, 7000);
    CHECK(m.jugador.muerto);
    reiniciar_jugador(m);
    CHECK_FALSE(m.jugador.muerto);
    CHECK(m.objetos.tamano() == 1);
    CHECK(m.objetos.buscar(1) != SIN_SLOT);
}

TEST_CASE("Inventario: lista, agregar, cantidad, nombre (equipado) y quitar") {
    Mundo m;
    auto item = [](ObjId id, u16 gfx, u32 cant, std::string_view nombre) {
        Evento e{};
        e.tipo = Ev::ITEM;
        e.id = id;
        e.v[IT_GFX] = gfx;
        e.v[IT_CANTIDAD] = static_cast<i32>(cant);
        e.v[IT_USO] = USO_ARMA;
        e.s[0] = nombre;
        return e;
    };
    Evento vaciar{};
    vaciar.tipo = Ev::INVENTARIO_VACIAR;
    Evento l1[] = {vaciar, item(1, 34, 1, "$1"), item(2, 80, 5, "$2 (5)")};
    aplicar_eventos(m, l1, 0);
    const TablaItems& t = m.inventario.items;
    REQUIRE(t.tamano() == 2);
    CHECK(t.col<I_GFX>()[t.buscar(1)] == 34);

    Evento cant{};
    cant.tipo = Ev::ITEM_CANTIDAD;
    cant.id = 2;
    cant.v[IT_CANTIDAD] = 4;
    cant.s[0] = "$2 (4)";
    Evento nom{};
    nom.tipo = Ev::ITEM_NOMBRE;
    nom.id = 1;
    nom.s[0] = "$1 ($9)";
    Evento l2[] = {cant, nom};
    aplicar_eventos(m, l2, 0);
    CHECK(t.col<I_CANTIDAD>()[t.buscar(2)] == 4);
    CHECK(nombre_item(m.inventario, t.buscar(2)) == "$2 (4)");
    CHECK(item_equipado(nombre_item(m.inventario, t.buscar(1))));
    CHECK_FALSE(item_equipado(nombre_item(m.inventario, t.buscar(2))));

    Evento quitar{};
    quitar.tipo = Ev::ITEM_QUITAR;
    quitar.id = 2;
    Evento l3[] = {quitar};
    aplicar_eventos(m, l3, 0);
    CHECK(t.tamano() == 1);
    Evento l4[] = {vaciar};
    aplicar_eventos(m, l4, 0);
    CHECK(t.tamano() == 0);
}

TEST_CASE("Mundo: la arena de textos se compacta y los nombres siguen iguales") {
    Mundo m;
    Evento e = poner(1, 10, 10, "Primero");
    aplicar_eventos(m, std::span(&e, 1), 0);
    // Cambios de título repetidos: cada uno agrega texto hasta pasar el límite.
    std::string titulo(200, 'x');
    for (int i = 0; i * titulo.size() < MAX_ARENA_TEXTOS + 1000; ++i) {
        Evento t{};
        t.tipo = Ev::PONER_OBJETO;
        t.id = 2;
        t.v[OB_X] = 11;
        t.v[OB_Y] = 11;
        t.s[0] = "Segundo";
        t.s[1] = titulo;
        aplicar_eventos(m, std::span(&t, 1), 0);
    }
    CHECK(m.textos.size() < MAX_ARENA_TEXTOS);
    const TablaObjetos& o = m.objetos;
    CHECK(texto(m, o.col<O_NOMBRE>()[o.buscar(1)]) == "Primero");
    CHECK(texto(m, o.col<O_NOMBRE>()[o.buscar(2)]) == "Segundo");
    CHECK(texto(m, o.col<O_TITULO>()[o.buscar(2)]) == titulo);
}

TEST_CASE("Mundo: segmentos en el hilo de carga y liberación de los lejanos" * doctest::skip(carpeta_juego().empty())) {
    Recursos rec;
    REQUIRE(rec.abrir(carpeta_juego()));
    Mundo m;
    m.mapa = 2005;
    const int x = 32691, y = 32864;
    CHECK(segmento_cargado(m, rec, x, y) == nullptr);  // pedido al hilo
    const SegmentoMapa* s = nullptr;
    for (int i = 0; i < 500 && !s; ++i) {
        actualizar_segmentos(m, x, y, 2);
        s = segmento_cargado(m, rec, x, y);
        if (!s) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(s);
    // Igual al que se carga en el momento.
    std::vector<u8> datos;
    SegmentoMapa directo;
    REQUIRE(leer_archivo(carpeta_juego() + nombre_segmento(2005, x, y), datos));
    REQUIRE(decodificar_s32(datos, x & ~63, y & ~63, directo));
    CHECK(s->suelo == directo.suelo);
    CHECK(s->tiles_objetos.size() == directo.tiles_objetos.size());
    // Los objetos cubren todas las piezas, seguidas, y su rectángulo las contiene.
    u32 siguiente = 0;
    for (const ObjetoMapa& o : s->objetos) {
        CHECK(o.primera == siguiente);
        siguiente += o.cantidad;
        for (u32 k = o.primera; k < o.primera + o.cantidad; ++k) {
            const TileObjetoMapa& t = s->tiles_objetos[k];
            CHECK((t.x / 2 >= o.x0 && t.x / 2 <= o.x1 && t.y >= o.y0 && t.y <= o.y1));
        }
    }
    CHECK(siguiente == s->tiles_objetos.size());
    // Uno lejano se carga en el momento; al actualizar con radio 2 alrededor del primero se libera
    // y el primero sigue encontrándose por su clave.
    REQUIRE(segmento(m, rec, x + 64 * 3, y));
    CHECK(m.segmentos.size() == 2);
    actualizar_segmentos(m, x, y, 2);
    CHECK(m.segmentos.size() == 1);
    CHECK(segmento_cargado(m, rec, x, y) == &m.segmentos[0]);
    CHECK(segmento_cargado(m, rec, x + 64 * 3, y) == nullptr);  // pedido de nuevo
}

TEST_CASE("Inventario: atributos de un ítem identificado (getStatusBytes) en texto") {
    char buf[512];
    // Arma: daño 6/8, hierro, peso 30, +3, dos manos, caballero, FUE +1.
    const u8 arma[] = {1, 6, 8, 11, 30, 0, 0, 0, 2, 3, 4, 7, 2, 8, 1};
    size_t n = describir_atributos({reinterpret_cast<const char*>(arma), sizeof arma}, buf, sizeof buf);
    CHECK(std::string_view(buf, n) ==
          "Damage 6 (small) / 8 (large)\nMaterial: iron\nWeight 30\nEnchant +3\nTwo-handed\nUsable by: Knight\nSTR +1");
    // Armadura: AC 4, cuero, sin grado, peso 15, todas las clases, MR 10, resistencia al sueño 5.
    const u8 armadura[] = {19, 4, 7, 0xFF, 15, 0, 0, 0, 7, 0x7F, 15, 10, 0, 15, 5, 0, 33, 3};
    n = describir_atributos({reinterpret_cast<const char*>(armadura), sizeof armadura}, buf, sizeof buf);
    CHECK(std::string_view(buf, n) ==
          "AC 4\nMaterial: leather\nWeight 15\nUsable by: all classes\nMR +10\nSleep resistance +5");
    // Sin identificar (vacío) y buffer chico: sin desbordar.
    CHECK(describir_atributos({}, buf, sizeof buf) == 0);
    n = describir_atributos({reinterpret_cast<const char*>(arma), sizeof arma}, buf, 8);
    CHECK(n < 8);
}
