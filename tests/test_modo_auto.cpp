// Modo auto: el intérprete del programa y el evaluador (mundo/modo_auto.h).
#include <doctest/doctest.h>
#include <string>
#include "mundo/modo_auto.h"

using namespace lin;

namespace {
Programa leer(std::string_view texto) {
    std::vector<ErrorPrograma> errores;
    Programa p = leer_programa(texto, errores);
    for (const ErrorPrograma& e : errores) FAIL_CHECK("line " << e.linea << ": " << e.mensaje);
    return p;
}

std::vector<ErrorPrograma> errores_de(std::string_view texto) {
    std::vector<ErrorPrograma> errores;
    leer_programa(texto, errores);
    return errores;
}

EntornoAuto entorno(i32 hp = 100, i32 mp = 100) {
    EntornoAuto e;
    e.hp = hp;
    e.hp_max = 100;
    e.mp = mp;
    e.mp_max = 100;
    e.mapa = 151;
    return e;
}

// Lo esencial de un programa, para comparar el español con el inglés.
std::string resumen(const Programa& p) {
    std::string s = p.nombre + "|" + std::to_string(p.mapa) + "|" + p.arma + "|" + std::to_string(static_cast<int>(p.pociones)) +
                    "|" + std::to_string(static_cast<int>(p.movimiento.tipo)) + "|" + p.movimiento.seguir;
    for (const Tecla& k : p.teclas) {
        s += "|K" + k.tecla + ":" + std::to_string(k.acciones.size()) + (k.con_hasta ? "h" : "");
        for (const AccionAuto& a : k.acciones) s += "," + std::to_string(static_cast<int>(a.tipo)) + a.nombre + a.objetivo_nombre;
    }
    for (const Regla& r : p.reglas) {
        s += "|R" + std::to_string(r.es_auto) + std::to_string(r.cada_ms) + (r.con_hasta ? "h" : "");
        for (const auto& g : r.si.o) {
            s += "(";
            for (const Condicion& c : g)
                s += std::to_string(static_cast<int>(c.tipo)) + std::to_string(static_cast<int>(c.comp)) + std::to_string(c.valor) +
                     (c.porcentaje ? "%" : "") + std::to_string(c.radio) + (c.negada ? "!" : "") + c.nombre + ";";
            s += ")";
        }
        for (const AccionAuto& a : r.acciones)
            s += "," + std::to_string(static_cast<int>(a.tipo)) + a.nombre + std::to_string(static_cast<int>(a.objetivo)) + a.objetivo_nombre;
        for (const AutoHechizo& h : r.auto_hechizos) s += "," + h.nombre + std::to_string(h.si.o.size());
    }
    return s;
}
}  // namespace

TEST_CASE("normalizar_nombre: sin mayúsculas, tildes ni espacios") {
    CHECK(normalizar_nombre("Turn Undead") == "turnundead");
    CHECK(normalizar_nombre("TurnUndead") == "turnundead");
    CHECK(normalizar_nombre("Más Cercano") == "mascercano");
    CHECK(normalizar_nombre("Auto-Hechizos") == "autohechizos");
}

TEST_CASE("leer_programa: el español y el inglés dan el mismo programa") {
    const Programa es = leer(R"(
plan "Prueba"
mapa 151
arma "Sword of Red Desire"
pociones: atacando
moverse seguir a "Andromeda" distancia 3
tecla F5: pausar
tecla Ctrl + 1: lanzar "Full Heal" a "Andromeda" hasta "Andromeda" hp >= 90%
si hp < 10% o sin pociones entonces usar "Scroll of Escape"; terminar
auto-hechizos "Haste", "Blue Potion" si no seguro a 3
si mp < 20% y seguro entonces detener; lanzar "Meditate" hasta mp >= 90%
si atacando y mp >= 30% cada 4s entonces lanzar "Sun Burst" a objetivo
si monstruos >= 4 a 2 y no tiene "Immune to Harm" entonces lanzar "Immune to Harm" a mí
si no seguro entonces atacar más cercano
)");
    const Programa en = leer(R"(
plan "Prueba"
map 151
weapon "Sword of Red Desire"
potions: attacking
move follow "Andromeda" distance 3
key F5: pause
key ctrl+1: cast "Full Heal" on "Andromeda" until "Andromeda" hp >= 90%
when hp < 10% or no potions then use "Scroll of Escape"; end
auto-spells "Haste", "Blue Potion" when not safe within 3
when mp < 20% and safe then stop; cast "Meditate" until mp >= 90%
when attacking and mp >= 30% every 4s then cast "Sun Burst" on target
when monsters >= 4 within 2 and not has "Immune to Harm" then cast "Immune to Harm" on me
when not safe then attack nearest
)");
    CHECK(resumen(es) == resumen(en));
    REQUIRE(es.reglas.size() == 6);
    CHECK(es.teclas[1].tecla == "ctrl+1");
    CHECK(es.reglas[0].si.o.size() == 2);  // "o": dos grupos
    CHECK(es.reglas[2].si.o[0].size() == 2);  // "y": un grupo de dos
    CHECK(es.reglas[3].cada_ms == 4000);
    CHECK(es.reglas[4].acciones[0].objetivo == Objetivo::YO);
    CHECK(es.reglas[1].es_auto);
    CHECK(es.reglas[1].auto_hechizos[1].nombre == "bluepotion");
}

TEST_CASE("leer_programa: los cuatro ejemplos de PLAN_MODO_AUTO.md cargan sin errores") {
    const Programa a = leer(R"(
plan "Piso 51 - Turn Undead"
mapa 151
pociones: siempre
moverse recorrer

si hp < 10% o sin pociones entonces usar "Scroll of Escape"; terminar
si mp < 10% entonces detener; lanzar "Meditate" hasta mp >= 90%
si no seguro entonces lanzar "Turn Undead" a más cercano
)");
    CHECK(a.reglas.size() == 3);
    CHECK(a.movimiento.tipo == TipoMovimiento::RECORRER);
    const Programa b = leer(R"(
plan "Elf 51F"
map 151
weapon "Sword of Red Desire"
potions: attacking
move wander
key F5: pause
key F6: use "Scroll of Escape"; end

when hp < 10% or no potions then use "Scroll of Escape"; end
auto-spells "Haste Potion", "Elven Wafer"
when mp < 20% and safe then cast "Blood to Soul"; cast "Greater Heal" on me
when attacking and mp >= 30% every 4s then cast "Sun Burst" on target
when not safe then attack nearest
)");
    CHECK(b.reglas.size() == 5);
    CHECK(b.teclas.size() == 2);
    const Programa c = leer(R"(
plan "Mago 11F"
mapa 111
moverse recorrer
tecla F5: pausar
tecla ctrl+1: lanzar "Fire Storm"

si hp < 15% entonces usar "Scroll of Escape"; terminar
auto-hechizos "Blue Potion", "Immune to Harm" si monstruos >= 4 a 2
si monstruos >= 4 a 2 entonces detener; lanzar "Fire Storm" hasta seguro a 3
si mp < 20% y seguro entonces lanzar "Meditate" hasta mp >= 90%
)");
    CHECK(c.reglas.size() == 4);
    const Programa d = leer(R"(
plan "Sanador"
moverse seguir a "Andromeda" distancia 2

si "Andromeda" hp < 40% entonces lanzar "Full Heal" a "Andromeda"
si "Andromeda" hp < 70% entonces lanzar "Greater Heal" a "Andromeda"
si sin jugadores a 30 entonces esperar
)");
    CHECK(d.movimiento.seguir == "andromeda");
    CHECK(d.reglas[0].si.o[0][0].tipo == TipoCond::HP_DE);
    CHECK(d.reglas[2].si.o[0][0].radio == 30);
}

TEST_CASE("leer_programa: errores con su línea; el resto se carga") {
    auto e = errores_de("plan \"X\"\nvolar alto\nsi hp < 10% usar \"Scroll of Escape\"\nsi mp < 5% entonces esperar\n");
    REQUIRE(e.size() == 2);
    CHECK(e[0].linea == 2);  // instrucción desconocida
    CHECK(e[1].linea == 3);  // falta "entonces"
    std::vector<ErrorPrograma> errores;
    const Programa p = leer_programa("si mp < 5% entonces esperar\nsi hp < 10% then\n", errores);
    CHECK(p.reglas.size() == 1);
    CHECK(errores.size() == 1);

    CHECK(errores_de("tecla F1: pausar").size() == 1);         // reservada
    CHECK(errores_de("key shift+F12: pause").size() == 1);     // reservada
    CHECK(errores_de("key F12: pause").empty());               // F12 sola no
    CHECK(errores_de("tecla F5: pausar\ntecla f5: terminar").size() == 1);  // repetida
    CHECK(errores_de("si hp < 10% entonces pausar").size() == 1);           // de control: solo en teclas
    CHECK(errores_de("si hp < 10% entonces usar \"Scroll").size() == 1);    // comillas sin cerrar
    CHECK(errores_de("auto-hechizos \"A\", \"B\", \"C\", \"D\", \"E\", \"F\", \"G\"").size() == 1);  // más de 6
    CHECK(errores_de("moverse puntos").size() == 1);
    CHECK(errores_de("# solo un comentario\n\n   \n").empty());
    CHECK(errores_de("si hp < 10% entonces usar \"Scroll # no es comentario\"").empty());
}

TEST_CASE("validar_programa: hechizos desconocidos; los auto-hechizos también aceptan las pociones") {
    const Programa p = leer("auto-hechizos \"Haste\", \"Blue Potion\", \"Bogus\"\nsi hp < 5% entonces lanzar \"Fake Spell\"\ntecla F5: regla 9");
    std::vector<ErrorPrograma> errores;
    validar_programa(p, [](std::string_view n) { return n == "haste"; }, errores);
    REQUIRE(errores.size() == 3);  // Bogus, Fake Spell y la regla 9
    CHECK(errores[0].linea == 1);
    CHECK(errores[1].linea == 2);
    CHECK(errores[2].linea == 3);
}

TEST_CASE("cumple: cada condición") {
    auto cond = [](const char* texto) { return leer(std::string("si ") + texto + " entonces esperar").reglas.at(0).si; };
    EntornoAuto e = entorno(9, 50);
    CHECK(cumple(cond("hp < 10%"), e));
    CHECK_FALSE(cumple(cond("hp < 9%"), e));
    CHECK(cumple(cond("hp <= 9"), e));
    CHECK(cumple(cond("mp >= 50%"), e));
    e.hp_max = 0;
    CHECK_FALSE(cumple(cond("hp < 10%"), e));  // sin máxima: falsa
    e.hp_max = 100;

    CHECK(cumple(cond("seguro"), e));
    e.monstruos = {5, 2, 12};
    CHECK_FALSE(cumple(cond("seguro"), e));    // 8 por defecto
    CHECK(cumple(cond("seguro a 1"), e));
    CHECK(cumple(cond("no seguro"), e));
    CHECK(cumple(cond("monstruos >= 2"), e));
    CHECK_FALSE(cumple(cond("monstruos >= 2 a 2"), e));
    CHECK(cumple(cond("monstruos = 1 a 2"), e));

    CHECK(cumple(cond("sin jugadores"), e));
    e.personajes = {{"andromeda", 20, 35}};
    CHECK_FALSE(cumple(cond("sin jugadores"), e));
    CHECK(cumple(cond("sin jugadores a 15"), e));
    CHECK(cumple(cond("\"Andromeda\" hp < 40%"), e));
    CHECK_FALSE(cumple(cond("\"Otro\" hp < 40%"), e));  // no está: falsa
    e.personajes[0].hp_pct = -1;
    CHECK_FALSE(cumple(cond("\"Andromeda\" hp < 40%"), e));  // fuera del grupo: desconocida

    CHECK_FALSE(cumple(cond("atacando"), e));
    e.atacando = true;
    e.objetivo_hp_pct = 20;
    CHECK(cumple(cond("atacando y objetivo hp < 30%"), e));
    e.hay_pociones = false;
    CHECK(cumple(cond("sin pociones"), e));
    e.items = {"scrollofescape"};
    CHECK_FALSE(cumple(cond("sin \"Scroll of Escape\""), e));
    CHECK(cumple(cond("sin \"Magic Gem\""), e));
    e.efectos = {"immunetoharm"};
    CHECK(cumple(cond("tiene \"Immune to Harm\""), e));
    CHECK(cumple(cond("en mapa 151"), e));
    // "y" liga más que "o": falso y falso o verdadero.
    CHECK(cumple(cond("hp > 50% y mp > 90% o en mapa 151"), e));
    CHECK_FALSE(cumple(cond("hp > 50% y en mapa 151"), e));
}

TEST_CASE("decidir: la primera regla que se cumple; sin ninguna, moverse") {
    const Programa p = leer(R"(
moverse recorrer
si hp < 10% entonces usar "Scroll of Escape"; terminar
si no seguro entonces atacar
)");
    EstadoAuto est;
    EntornoAuto e = entorno();
    CHECK(decidir(p, est, e, 1000).origen == OrigenDecision::MOVER);
    e.monstruos = {3};
    Decision d = decidir(p, est, e, 1000);
    CHECK(d.origen == OrigenDecision::REGLA);
    CHECK(d.indice == 1);
    e.hp = 5;
    d = decidir(p, est, e, 1000);
    CHECK(d.indice == 0);
    REQUIRE(d.acciones);
    CHECK(d.acciones->size() == 2);

    // Quieto y sin reglas que se cumplan: nada. Pausado, terminado o fuera del mapa: nada.
    const Programa q = leer("mapa 151\nsi hp < 10% entonces esperar");
    e.hp = 100;
    CHECK(decidir(q, est, e, 1000).origen == OrigenDecision::NADA);
    e.hp = 5;
    CHECK(decidir(q, est, e, 1000).origen == OrigenDecision::REGLA);
    e.mapa = 4;
    CHECK(decidir(q, est, e, 1000).origen == OrigenDecision::NADA);
    e.mapa = 151;
    est.pausado = true;
    CHECK(decidir(q, est, e, 1000).origen == OrigenDecision::NADA);
}

TEST_CASE("decidir: 'hasta' sostiene la regla; las de más arriba la interrumpen") {
    const Programa p = leer(R"(
si hp < 10% entonces usar "Scroll of Escape"; terminar
si mp < 10% entonces detener; lanzar "Meditate" hasta mp >= 90%
si no seguro entonces atacar
)");
    EstadoAuto est;
    EntornoAuto e = entorno(100, 5);
    Decision d = decidir(p, est, e, 1000);
    CHECK(d.indice == 1);
    CHECK_FALSE(d.continua);
    e.mp = 50;
    e.monstruos = {1};  // la de abajo se cumpliría, pero la sostenida sigue
    d = decidir(p, est, e, 1100);
    CHECK(d.indice == 1);
    CHECK(d.continua);
    e.mp = 95;  // salida: suelta y sigue con las demás
    d = decidir(p, est, e, 1200);
    CHECK(d.indice == 2);
    CHECK(est.sostenida == -1);

    e.mp = 5;
    decidir(p, est, e, 1300);
    CHECK(est.sostenida == 1);
    e.hp = 5;  // la de arriba interrumpe
    d = decidir(p, est, e, 1400);
    CHECK(d.indice == 0);
    CHECK(est.sostenida == -1);
}

TEST_CASE("decidir: 'cada' limita una regla a una vez por período") {
    const Programa p = leer("si atacando cada 4s entonces lanzar \"Sun Burst\" a objetivo\nsi atacando entonces atacar");
    EstadoAuto est;
    EntornoAuto e = entorno();
    e.atacando = true;
    CHECK(decidir(p, est, e, 0).indice == 0);  // la primera vez, aunque sea en 0
    CHECK(decidir(p, est, e, 3999).indice == 1);
    CHECK(decidir(p, est, e, 4000).indice == 0);
}

TEST_CASE("decidir: los auto-hechizos en su lugar de la prioridad, con su condición") {
    const Programa p = leer(R"(
si hp < 10% entonces usar "Scroll of Escape"; terminar
auto-hechizos "Haste", "Immune to Harm" si monstruos >= 3 a 3
si no seguro entonces atacar
)");
    EstadoAuto est;
    EntornoAuto e = entorno();
    e.monstruos = {1};
    e.renovar = {"immunetoharm"};
    CHECK(decidir(p, est, e, 0).indice == 2);  // Immune solo rodeado
    e.monstruos = {1, 1, 2};
    Decision d = decidir(p, est, e, 0);
    CHECK(d.origen == OrigenDecision::AUTO_HECHIZO);
    CHECK(d.auto_hechizo == "immunetoharm");
    e.renovar = {"immunetoharm", "haste"};
    CHECK(decidir(p, est, e, 0).auto_hechizo == "haste");  // el primero de la lista
    e.hp = 5;
    CHECK(decidir(p, est, e, 0).indice == 0);  // el escape va antes
}

TEST_CASE("pulsar_tecla: control, acciones sostenidas y reglas por número") {
    const Programa p = leer(R"(
moverse puntos (1,1) (2,2)
tecla F5: pausar
tecla F7: siguiente punto
tecla ctrl+2: lanzar "Meditate" hasta mp >= 90%
tecla ctrl+3: regla 2
si hp < 10% entonces usar "Scroll of Escape"; terminar
si no seguro entonces atacar
)");
    EstadoAuto est;
    EntornoAuto e = entorno(100, 10);
    bool usada = false;
    CHECK(pulsar_tecla(p, est, "f9", usada).origen == OrigenDecision::NADA);
    CHECK_FALSE(usada);
    pulsar_tecla(p, est, "f5", usada);
    CHECK(usada);
    CHECK(est.pausado);
    CHECK(decidir(p, est, e, 0).origen == OrigenDecision::NADA);
    pulsar_tecla(p, est, "f5", usada);
    CHECK_FALSE(est.pausado);
    pulsar_tecla(p, est, "f7", usada);
    CHECK(est.punto == 1);

    Decision d = pulsar_tecla(p, est, "ctrl+3", usada);
    CHECK(d.origen == OrigenDecision::REGLA);
    CHECK(d.indice == 1);

    d = pulsar_tecla(p, est, "ctrl+2", usada);
    CHECK(d.origen == OrigenDecision::TECLA);
    CHECK(est.tecla_sostenida == 2);
    e.monstruos = {1};  // una regla normal no la interrumpe
    d = decidir(p, est, e, 100);
    CHECK(d.origen == OrigenDecision::TECLA);
    CHECK(d.continua);
    e.hp = 5;  // la que termina el programa, sí
    d = decidir(p, est, e, 200);
    CHECK(d.origen == OrigenDecision::REGLA);
    CHECK(d.indice == 0);
    CHECK(est.tecla_sostenida == -1);
}
