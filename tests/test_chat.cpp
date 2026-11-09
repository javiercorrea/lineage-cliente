#include <string>
#include <vector>
#include <doctest/doctest.h>
#include <string>
#include "comun.h"
#include "mundo/chat.h"
#include "mundo/mundo.h"
#include "proto/comandos.h"
#include "recursos/fuente.h"
#include "recursos/fuente_ttf.h"
#include "proto/decodificar.h"
#include "recursos/fuente.h"

using namespace lin;

namespace {

std::vector<Evento> decodificar_uno(std::vector<u8> p, Arena& arena) {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    std::vector<Evento> ev;
    REQUIRE(decodificar(p, t, arena, ev));
    return ev;
}

void poner_s(std::vector<u8>& p, std::string_view s) {
    p.insert(p.end(), s.begin(), s.end());
    p.push_back(0);
}

}  // namespace

TEST_CASE("Chat: prefijos del original") {
    EntradaChat c;
    REQUIRE(interpretar_entrada("hola", c));
    CHECK(c.tipo == CHAT_NORMAL);
    CHECK(c.texto == "hola");
    REQUIRE(interpretar_entrada("!a todos", c));
    CHECK(c.tipo == CHAT_GRITO);
    CHECK(c.texto == "a todos");
    REQUIRE(interpretar_entrada("&global", c));
    CHECK(c.tipo == CHAT_GLOBAL);
    REQUIRE(interpretar_entrada("$vendo", c));
    CHECK(c.tipo == CHAT_COMERCIO);
    REQUIRE(interpretar_entrada("@clan", c));
    CHECK(c.tipo == CHAT_CLAN);
    REQUIRE(interpretar_entrada("#grupo", c));
    CHECK(c.tipo == CHAT_GRUPO);
    REQUIRE(interpretar_entrada("%alianza", c));
    CHECK(c.tipo == CHAT_ALIANZA);
    REQUIRE(interpretar_entrada("*charla", c));
    CHECK(c.tipo == CHAT_GRUPO_CHAT);
    REQUIRE(interpretar_entrada("\"Exploradora  hola che ", c));
    CHECK(c.susurro);
    CHECK(c.destino == "Exploradora");
    CHECK(c.texto == "hola che");
    CHECK_FALSE(interpretar_entrada("", c));
    CHECK_FALSE(interpretar_entrada("   ", c));
    CHECK_FALSE(interpretar_entrada("!", c));
    CHECK_FALSE(interpretar_entrada("\"solo", c));
}

TEST_CASE("Chat: C_Chat y C_ChatWhisper") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Escritor ec = cmd::chat(t, CHAT_GRITO, "hi");  // cuerpo() es una vista al buffer del Escritor
    Bytes b = ec.cuerpo();
    const u8 esperado[] = {190, 2, 'h', 'i', 0, 0, 0, 0};
    REQUIRE(b.size() == sizeof esperado);
    CHECK(std::memcmp(b.data(), esperado, sizeof esperado) == 0);
    Escritor ew = cmd::susurrar(t, "Ana", "yo");
    Bytes w = ew.cuerpo();
    const u8 esperado_w[] = {122, 'A', 'n', 'a', 0, 'y', 'o', 0};
    REQUIRE(w.size() == sizeof esperado_w);
    CHECK(std::memcmp(w.data(), esperado_w, sizeof esperado_w) == 0);
}

TEST_CASE("Decodificar: paquetes de chat") {
    Arena arena(4096);
    // Grito (NORMALCHAT tipo 2): c tipo, d id, s texto, h x, h y
    std::vector<u8> p = {76, 2, 0x22, 0, 0, 0};
    poner_s(p, "<Ana> hola");
    p.insert(p.end(), {0x10, 0x80, 0x20, 0x80});
    auto ev = decodificar_uno(p, arena);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::CHAT);
    CHECK(ev[0].v[0] == CHAT_GRITO);
    CHECK(ev[0].id == 0x22);
    CHECK(ev[0].s[0] == "<Ana> hola");
    // Sistema (GLOBALCHAT tipo 9): sin id
    p = {10, 9};
    poner_s(p, "aviso");
    ev = decodificar_uno(p, arena);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[0] == CHAT_SISTEMA);
    CHECK(ev[0].id == 0);
    CHECK(ev[0].s[0] == "aviso");
    // Susurro recibido (WHISPERCHAT): s remitente, s texto
    p = {255};
    poner_s(p, "Ana");
    poner_s(p, "psst");
    ev = decodificar_uno(p, arena);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[0] == CHAT_SUSURRO);
    CHECK(ev[0].s[1] == "Ana");
    CHECK(ev[0].s[0] == "psst");
    // NPC (NPCSHOUT): mismo formato que el chat normal
    p = {133, 0, 0x33, 0, 0, 0};
    poner_s(p, "$1422: hola");
    ev = decodificar_uno(p, arena);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].id == 0x33);
}

TEST_CASE("Decodificar: mensajes y estado") {
    Arena arena(4096);
    std::vector<u8> p = {14, 73, 0, 1};
    poner_s(p, "Ana");
    auto ev = decodificar_uno(p, arena);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::MENSAJE_SERVIDOR);
    CHECK(ev[0].v[MS_ID] == 73);
    CHECK(ev[0].v[MS_NARGS] == 1);
    CHECK(ev[0].v[MS_AZUL] == 0);
    CHECK(ev[0].s[0] == "Ana");
    p = {59, 5, 0, 0};
    CHECK(decodificar_uno(p, arena)[0].v[MS_AZUL] == 1);

    ev = decodificar_uno({121, 7, 0x10, 0x27, 0, 0}, arena);  // S_Exp: nivel 7, exp 10000
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::EXPERIENCIA);
    CHECK(ev[0].v[0] == 7);
    CHECK(ev[0].v[1] == 10000);
    ev = decodificar_uno({140, 1, 0, 0, 0, 0x18, 0xFC, 0, 0, 0, 0}, arena);  // S_Lawful: -1000
    CHECK(ev[0].v[0] == -1000);
    ev = decodificar_uno({15, 0xFE, 10, 0, 0, 0xFB}, arena);  // S_OwnCharAttrDef: CA -2, fuego 10, tierra -5
    CHECK(ev[0].v[DF_CA] == -2);
    CHECK(ev[0].v[DF_FUEGO] == 10);
    CHECK(ev[0].v[DF_TIERRA] == -5);
    ev = decodificar_uno({40, 11, 200}, arena);  // S_PacketBox FOOD
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::COMIDA);
    CHECK(ev[0].v[0] == 200);
    CHECK(decodificar_uno({40, 20, 1, 2, 3}, arena).empty());  // otros subtipos: se ignoran sin error

    Mundo m;
    m.jugador.id = 1;
    ev = decodificar_uno({216, 18, 9, 12, 11, 16, 13, 120}, arena);  // S_OwnCharStatus2
    Evento lote[] = {ev[0], decodificar_uno({174, 2, 5}, arena)[0], decodificar_uno({140, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0}, arena)[0]};
    aplicar_eventos(m, lote, 0);
    CHECK(m.jugador.fue == 18);
    CHECK(m.jugador.car == 13);
    CHECK(m.jugador.peso == 120);
    CHECK(m.jugador.sp == 2);
    CHECK(m.jugador.mr == 5);
    CHECK(m.jugador.legal == 100);
}

TEST_CASE("Chat: anillo de tamaño fijo, recorte CP949 y globos") {
    static Chat c;  // 50 KB: fuera de la pila
    c.limpiar();
    char t[16];
    for (u32 i = 0; i < CAPACIDAD_CHAT + 10; ++i) {
        std::snprintf(t, sizeof t, "linea %u", i);
        c.agregar(t, 0, CHAT_NORMAL);
    }
    CHECK(c.cantidad() == CAPACIDAD_CHAT);
    CHECK(c.texto(c.reciente(0)) == "linea 265");
    CHECK(c.texto(c.reciente(CAPACIDAD_CHAT - 1)) == "linea 10");

    // Una línea larga que termina en un carácter coreano no se corta a la mitad.
    std::string largo(LARGO_LINEA_CHAT - 1, 'a');
    largo += "\xB0\xA1";
    c.agregar(largo, 0, CHAT_NORMAL);
    CHECK(c.reciente(0).largo == LARGO_LINEA_CHAT - 1);

    TablaTexto vacia;
    Evento e{};
    e.tipo = Ev::CHAT;
    e.v[0] = CHAT_NORMAL;
    e.id = 42;
    e.s[0] = "Ana: hola";
    Evento lote[] = {e};
    aplicar_chat(c, vacia, vacia, lote, 1000);
    CHECK(c.globo(42, 1000 + MS_GLOBO - 1) == "Ana: hola");
    CHECK(c.globo(42, 1000 + MS_GLOBO).empty());
    CHECK(c.globo(7, 1000).empty());
}

TEST_CASE("Chat: S_ServerMessage con string-e.tbl y desc-e.tbl reales") {
    std::string dir = carpeta_juego();
    if (dir.empty()) return;
    Recursos rec;
    REQUIRE(rec.abrir(dir));
    TablaTexto mensajes, desc;
    REQUIRE(mensajes.cargar(rec, "string-e.tbl"));
    REQUIRE(desc.cargar(rec, "desc-e.tbl"));
    Evento e{};
    e.tipo = Ev::MENSAJE_SERVIDOR;
    e.v[MS_ID] = 73;  // "%0 is not playing now."
    e.v[MS_NARGS] = 1;
    e.s[0] = "Ana";
    char buf[256];
    u32 color = 0;
    size_t n = formatear_mensaje(mensajes, desc, e, buf, sizeof buf, color);
    CHECK(std::string_view(buf, n) == "Ana is not playing now.");
    e.v[MS_ID] = 195;
    e.v[MS_NARGS] = 0;
    n = formatear_mensaje(mensajes, desc, e, buf, sizeof buf, color);
    CHECK(std::string_view(buf, n) == "You do not have the ability to use global chat.");
    // Nombres "$N" dentro del texto de los NPC.
    n = expandir_desc(desc, "$1422: hola", buf, sizeof buf);
    CHECK(std::string_view(buf, n) == std::string(desc.linea(1422)) + ": hola");
    CHECK(desc.linea(1422).size() > 0);
}

TEST_CASE("Estado: porcentaje de experiencia y ajuste de línea") {
    CHECK(porcentaje_exp(1, 0) == doctest::Approx(0.0f));
    CHECK(porcentaje_exp(1, 62) == doctest::Approx(49.6f));
    CHECK(porcentaje_exp(2, 300) == doctest::Approx(100.0f));
    CHECK(porcentaje_exp(0, 10) == 0.0f);
    const char* t = "uno dos tres";
    const int a = ancho_ttf('a');  // avance de una letra en la fuente de la interfaz (ahora 7 px)
    CHECK(bytes_que_caben(t, 12, a * 12) == 12);
    CHECK(bytes_que_caben(t, 12, a * 9) == 8);   // corta después de "dos "
    CHECK(bytes_que_caben("abcdef", 6, a * 3) == 3);  // sin espacios: corta en el ancho
}

TEST_CASE("Chat: códigos de color y miles") {
    auto partes = [](std::string_view t, bool doble = false) {
        std::vector<std::pair<std::string, u32>> r;
        recorrer_codigos(t, 7, doble, [&](std::string_view s, u32 c) { r.emplace_back(std::string(s), c); });
        return r;
    };
    CHECK(partes("hola") == std::vector<std::pair<std::string, u32>>{{"hola", 7}});
    const auto rojo = partes("a\\f3b");
    REQUIRE(rojo.size() == 2);
    CHECK(rojo[0] == std::pair<std::string, u32>{"a", 7});
    CHECK(rojo[1].first == "b");
    CHECK(rojo[1].second == color_codigo('3', 7));
    CHECK(rojo[1].second != 7);
    CHECK(partes("\\f3x\\f>y")[1] == std::pair<std::string, u32>{"y", 7});
    CHECK(partes("\\bAz")[0] == std::pair<std::string, u32>{"z", 7});
    CHECK(partes("\\\\f3a")[0].first == "f3a");     // \\ se descarta, como en el original
    CHECK(partes("c:\\x")[0].first == "c:\\x");     // otra barra se muestra
    CHECK(partes("\\f~")[0].first == "\\f~");       // fuera de rango: no es un código
    CHECK(partes("\x95\\f3", true)[0].first == "\x95\\f3");  // Shift-JIS: la barra es del carácter

    char b[32];
    CHECK(std::string(con_miles(0, b, sizeof b)) == "0");
    CHECK(std::string(con_miles(999, b, sizeof b)) == "999");
    CHECK(std::string(con_miles(10000, b, sizeof b)) == "10.000");
    CHECK(std::string(con_miles(1234567, b, sizeof b)) == "1.234.567");
}
