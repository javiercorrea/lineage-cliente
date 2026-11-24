#include <doctest/doctest.h>
#include "plataforma/lanzador.h"
#include "plataforma/rutas.h"
#include <chrono>
#include <random>
#include <thread>
#include <unordered_map>
#include "core/anillo.h"
#include "core/arena.h"
#include "core/tabla_soa.h"

using namespace lin;

TEST_CASE("MapaHash: comparado con unordered_map en operaciones aleatorias") {
    MapaHash m;
    std::unordered_map<u64, u32> ref;
    std::mt19937_64 rng(1);
    for (int i = 0; i < 20000; ++i) {
        u64 k = rng() % 500;
        int op = static_cast<int>(rng() % 3);
        if (op == 0) {
            u32 v = static_cast<u32>(rng());
            m.poner(k, v);
            ref[k] = v;
        } else if (op == 1) {
            CHECK(m.quitar(k) == (ref.erase(k) == 1));
        } else {
            const u32* v = m.buscar(k);
            auto it = ref.find(k);
            REQUIRE((v != nullptr) == (it != ref.end()));
            if (v) CHECK(*v == it->second);
        }
    }
    CHECK(m.tamano() == ref.size());
}

TEST_CASE("TablaSoa: el índice queda consistente tras borrar con intercambio") {
    TablaSoa<u16, u32> t;
    for (ObjId id = 100; id < 200; ++id) {
        Slot s = t.insertar(id);
        t.col<0>()[s] = static_cast<u16>(id);
        t.col<1>()[s] = id * 10;
    }
    for (ObjId id = 100; id < 200; id += 3) CHECK(t.quitar(id));
    CHECK_FALSE(t.quitar(100));
    for (ObjId id = 100; id < 200; ++id) {
        Slot s = t.buscar(id);
        if ((id - 100) % 3 == 0) {
            CHECK(s == SIN_SLOT);
        } else {
            REQUIRE(s != SIN_SLOT);
            CHECK(t.ids()[s] == id);
            CHECK(t.col<0>()[s] == id);
            CHECK(t.col<1>()[s] == id * 10);
        }
    }
    CHECK(t.tamano() == 66);
}

TEST_CASE("TablaSoa: reordenar mueve columnas e índice juntos") {
    TablaSoa<u32> t;
    for (ObjId id = 1; id <= 4; ++id) t.col<0>()[t.insertar(id)] = id * 10;
    const Slot orden[] = {3, 1, 0, 2};
    t.reordenar(orden);
    CHECK(t.ids() == std::vector<ObjId>{4, 2, 1, 3});
    for (ObjId id = 1; id <= 4; ++id) CHECK(t.col<0>()[t.buscar(id)] == id * 10);
}

TEST_CASE("AnilloBytes: mensajes de longitud variable con vuelta del anillo") {
    AnilloBytes a(64);
    std::vector<u8> salida;
    for (int ronda = 0; ronda < 50; ++ronda) {
        u8 m[10];
        for (int i = 0; i < 10; ++i) m[i] = static_cast<u8>(ronda + i);
        REQUIRE(a.empujar({m, static_cast<size_t>(1 + ronda % 10)}));
        REQUIRE(a.sacar(salida));
        CHECK(salida.size() == static_cast<size_t>(1 + ronda % 10));
        CHECK(salida[0] == static_cast<u8>(ronda));
    }
    CHECK_FALSE(a.sacar(salida));
    u8 grande[70] = {};
    CHECK_FALSE(a.empujar({grande, 70}));  // no entra
}

TEST_CASE("AnilloBytes: vaciar descarta lo pendiente (reconexión)") {
    AnilloBytes a(64);
    std::vector<u8> salida;
    const u8 m[3] = {1, 2, 3};
    CHECK(a.vacio());
    REQUIRE(a.empujar({m, 3}));
    CHECK_FALSE(a.vacio());
    a.vaciar();
    CHECK(a.vacio());
    CHECK_FALSE(a.sacar(salida));
    REQUIRE(a.empujar({m, 2}));
    REQUIRE(a.sacar(salida));
    CHECK(salida.size() == 2);
}

TEST_CASE("Arena: alineación, copia y reset") {
    Arena a(256);
    u8* p1 = static_cast<u8*>(a.reservar(3, 1));
    u64* p2 = a.reservar_arreglo<u64>(2);
    REQUIRE(p1);
    REQUIRE(p2);
    CHECK(reinterpret_cast<uintptr_t>(p2) % alignof(u64) == 0);
    CHECK(a.copiar("hola") == "hola");
    CHECK(a.reservar(1000) == nullptr);
    a.resetear();
    CHECK(a.usado() == 0);
}

TEST_CASE("settings.ini: servidores, el elegido, migración del formato anterior y la clave") {
    // Formato anterior: un solo servidor con host/puerto/protocolo/usuario; la clave no se lee.
    Config v = leer_config("juego=/j/\nhost=10.0.0.5\npuerto=2001\nprotocolo=jp363\nusuario=ana\nclave=secreta\n");
    REQUIRE(v.servidores.size() == 1);
    CHECK(v.servidores[0].nombre == "Servidor");
    CHECK(v.servidores[0].host == "10.0.0.5");
    CHECK(v.servidores[0].puerto == 2001);
    CHECK(v.servidores[0].usuario == "ana");
    // Sin nada: el local de pruebas.
    Config vacio = leer_config("");
    REQUIRE(vacio.servidores.size() == 1);
    CHECK(vacio.servidores[0].host == "127.0.0.1");
    CHECK(vacio.servidores[0].puerto == 2000);
    // Varios, el elegido por nombre, y la vuelta por escribir_config.
    Config c = leer_config("servidor=Local|127.0.0.1|2000|jp363|\nservidor=Otro de prueba|h.ejemplo|2100|s3ep1|bob\n"
                           "servidor_elegido=Otro de prueba\nui_simple=0\nventana=1024x768\n");
    REQUIRE(c.servidores.size() == 2);
    CHECK(c.servidor == 1);
    CHECK(servidor_elegido(c).protocolo == "s3ep1");
    CHECK(servidor_elegido(c).usuario == "bob");
    CHECK_FALSE(c.ui_simple);
    const std::string texto = escribir_config(c);
    CHECK(texto.find("host=") == std::string::npos);
    CHECK(texto.find("clave") == std::string::npos);
    Config d = leer_config(texto);
    REQUIRE(d.servidores.size() == 2);
    CHECK(d.servidor == 1);
    CHECK(d.servidores[1].nombre == "Otro de prueba");
    CHECK(d.servidores[1].puerto == 2100);
    CHECK(d.servidores[1].lanzador.empty());
    CHECK(texto.find("bob|") == std::string::npos);  // sin lanzador no hay campo vacío: queda el formato de 5 campos
    CHECK_FALSE(d.ui_simple);
    CHECK(d.ventana_ancho == 1024);
    CHECK(d.ventana_alto == 768);
}

TEST_CASE("Lanzador de servidor: el campo 6 se lleva el resto de la línea; el puerto sale de su salida") {
    Config c = leer_config("servidor=P|127.0.0.1|0|jp363|ana|/opt/l/proxy.sh --a|b\nservidor_elegido=P\n");
    REQUIRE(c.servidores.size() == 1);
    CHECK(c.servidores[0].lanzador == "/opt/l/proxy.sh --a|b");
    CHECK(c.servidores[0].puerto == 0);
    Config d = leer_config(escribir_config(c));
    CHECK(d.servidores[0].lanzador == "/opt/l/proxy.sh --a|b");
    CHECK(puerto_en_salida("starting\nport 25134\n") == 25134);
    CHECK(puerto_en_salida("pid 123456 port 7001") == 7001);  // los números fuera de rango no cuentan
    CHECK(puerto_en_salida("no port") == 0);
    CHECK(puerto_en_salida("listening on 127.0.0.1:7001 (pid 4242)") == 7001);  // no el pid del final
    CHECK(puerto_en_salida("12:30:45 proxy ready on localhost:7002") == 7002);   // una hora no es un puerto
    CHECK(puerto_en_salida("report 3 port: 7003 v2") == 7003);
    CHECK(puerto_marcado("12:30:45 starting") == 0);  // sin forma marcada hay que esperar al final de la salida
    CHECK(puerto_en_salida("12:30:45 starting\n25134\n") == 25134);
}

#ifndef _WIN32
TEST_CASE("Lanzador: da el puerto aunque el proxy en segundo plano deje la salida abierta") {
    Lanzador l;
    // El sleep hereda la salida: leer hasta el final esperaría 5 s; con "port N" se corta antes.
    l.iniciar("sleep 5 & echo port 25134");
    u16 puerto = 0;
    bool listo = false;
    for (int i = 0; i < 200 && !(listo = l.terminado(puerto)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(listo);
    CHECK(puerto == 25134);
}
#endif
