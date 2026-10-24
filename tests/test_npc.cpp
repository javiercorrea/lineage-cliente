// Entregable 7: NPCs y comercio. Paquetes de L1J-JP 3.63c (S_*.java) y maquetación de las
// páginas HTML de los NPC.
#include <doctest/doctest.h>
#include "comun.h"
#include "proto/comandos.h"
#include "proto/decodificar.h"
#include "recursos/html.h"
#include "recursos/pak.h"

using namespace lin;

namespace {
std::vector<Evento> decodificar_bytes(std::initializer_list<u8> p, Arena& arena) {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    std::vector<u8> b(p);
    std::vector<Evento> ev;
    REQUIRE(decodificar({b.data(), b.size()}, t, arena, ev));
    return ev;
}
bool medir(void*, u32 n, int& w, int& h) {
    if (n != 312 && n != 314) return false;
    w = 60;
    h = 20;
    return true;
}
}  // namespace

TEST_CASE("NPC: S_NPCTalkReturn con datos, S_Teleport y comandos") {
    Arena arena(4096);
    // Página "arena6" con dos datos.
    auto ev = decodificar_bytes({119, 0x10, 0, 0, 0, 'a', 'r', 'e', 'n', 'a', '6', 0, 1, 0, 2, 0, 'B', 'o', 'b', 0, '1', '5', 0}, arena);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::HTML);
    CHECK(ev[0].id == 0x10);
    CHECK(ev[0].s[0] == "arena6");
    CHECK(ev[0].v[HT_DATOS] == 2);
    CHECK(ev[0].s[1] == std::string_view("Bob\x01" "15"));
    // Sin datos (h 0, h 0).
    auto ev2 = decodificar_bytes({119, 0x10, 0, 0, 0, 'f', 0, 0, 0, 0, 0}, arena);
    CHECK(ev2[0].s[0] == "f");
    CHECK(ev2[0].v[HT_DATOS] == 0);
    // S_Teleport.
    auto ev3 = decodificar_bytes({4, 0, 0x40, 0x06, 0x53, 0x01, 0x10}, arena);
    CHECK(ev3[0].tipo == Ev::TELETRANSPORTE);
    CHECK(ev3[0].id == 0x10015306);

    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Escritor h = cmd::hablar_npc(t, 0x22);
    CHECK(h.cuerpo()[0] == 58);
    LineaResultado compra[] = {{3, 2}, {7, 1}};
    Escritor r = cmd::resultado(t, 0x22, RESULTADO_COMPRA, compra);
    Bytes b = r.cuerpo();
    const u8 esperado[] = {40, 0x22, 0, 0, 0, 0, 2, 0, 3, 0, 0, 0, 2, 0, 0, 0, 7, 0, 0, 0, 1, 0, 0, 0};
    REQUIRE(b.size() == sizeof esperado);
    CHECK(std::memcmp(b.data(), esperado, sizeof esperado) == 0);
    Escritor sa = cmd::subir_atributo(t, "str");
    Bytes bsa = sa.cuerpo();
    const u8 esperado_attr[] = {61, 0xDF, 0x01, 1, 's', 't', 'r', 0};
    REQUIRE(bsa.size() == sizeof esperado_attr);
    CHECK(std::memcmp(bsa.data(), esperado_attr, sizeof esperado_attr) == 0);
    Escritor tp = cmd::teletransporte(t);
    CHECK(tp.cuerpo()[0] == 242);
}

TEST_CASE("NPC: listas de tienda y almacén") {
    Arena arena(4096);
    // S_ShopSellList (lo que vende el NPC): 1 ítem, índice 0, gfx 80, 50 adena, "Potion", sin atributos.
    auto c = decodificar_bytes({254, 0x22, 0, 0, 0, 1, 0, 0, 0, 0, 0, 80, 0, 50, 0, 0, 0, 'P', 'o', 't', 'i', 'o', 'n', 0, 0, 7, 0}, arena);
    REQUIRE(c.size() == 2);
    CHECK(c[0].tipo == Ev::LISTA_TIENDA);
    CHECK(c[0].v[LT_TIPO] == LISTA_COMPRA);
    CHECK(c[1].tipo == Ev::ITEM_TIENDA);
    CHECK(c[1].v[IT_GFX] == 80);
    CHECK(c[1].v[TI_PRECIO] == 50);
    CHECK(c[1].s[0] == "Potion");
    // S_ShopBuyList (lo que compra el NPC): ítem 0x99 a 12 adena.
    auto v = decodificar_bytes({170, 0x22, 0, 0, 0, 1, 0, 0x99, 0, 0, 0, 12, 0, 0, 0, 7, 0}, arena);
    REQUIRE(v.size() == 2);
    CHECK(v[0].v[LT_TIPO] == LISTA_VENTA);
    CHECK(v[1].id == 0x99);
    CHECK(v[1].v[TI_PRECIO] == 12);
    // S_RetrieveList: 1 ítem 0x55, gfx 80, 3 unidades.
    auto r = decodificar_bytes({250, 0x22, 0, 0, 0, 1, 0, 3, 0x55, 0, 0, 0, 0, 80, 0, 0, 3, 0, 0, 0, 1, 'P', 0, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0}, arena);
    REQUIRE(r.size() == 2);
    CHECK(r[0].v[LT_TIPO] == LISTA_RETIRO);
    CHECK(r[1].id == 0x55);
    CHECK(r[1].v[IT_CANTIDAD] == 3);
    CHECK(r[1].s[0] == "P");
}

TEST_CASE("NPC: maquetación del HTML") {
    // Como fraoun-e.html: título en blanco, un enlace, una acción y los botones de comprar y vender centrados.
    const char* html =
        "<body><font fg=ffffff><p align=left>Merchant Fraoun:</p></font> <br>"
        "<p>Feel free to look around, <username>. <a link=\"fraoun8\">Weapons</a> <a action=\"buy\">Buy</a></p>"
        "<p align=center><img src=\"#312\" action=\"buy\"></img>&nbsp;&nbsp;<img src=\"#314\" action=\"sell\"></img></p>"
        "<var src=\"#1\"></body>";
    PaginaHtml p;
    maquetar_html(html, std::string_view("cero\x01uno"), '\x01', "Charla", 200, medir, nullptr, p);
    REQUIRE(!p.elementos.empty());
    CHECK(p.texto(p.elementos[0]) == "Merchant Fraoun:");
    CHECK(p.elementos[0].color == 0xFFFFFFFFu);
    std::string todo;
    for (const ElementoHtml& e : p.elementos)
        if (e.tipo == ElementoHtml::Tipo::TEXTO) (todo += p.texto(e)) += '|';
    CHECK(todo.find("Charla.") != std::string::npos);
    CHECK(todo.find("uno") != std::string::npos);
    int imagenes = 0;
    for (const ElementoHtml& e : p.elementos) {
        if (p.texto(e) == "Weapons") {
            CHECK(e.es_enlace);
            CHECK(p.vinculo(e) == "fraoun8");
            CHECK(e.color == COLOR_VINCULO_HTML);
        }
        if (p.texto(e) == "Buy") {
            CHECK_FALSE(e.es_enlace);
            CHECK(p.vinculo(e) == "buy");
        }
        if (e.tipo == ElementoHtml::Tipo::IMAGEN) {
            ++imagenes;
            CHECK(e.x + e.w <= 200);
            CHECK(e.x > 0);  // centradas
        }
        CHECK(e.x + e.w <= 200 + 1);
    }
    CHECK(imagenes == 2);
    CHECK(p.alto > 3 * ALTO_LINEA_HTML);
}

TEST_CASE("NPC: todas las páginas en inglés se maquetan" * doctest::skip(carpeta_juego().empty())) {
    Recursos r;
    REQUIRE(r.abrir(carpeta_juego()));
    const Pak& t = r.pak_textos();
    int paginas = 0, con_vinculos = 0;
    PaginaHtml p;
    std::vector<u8> d;
    for (size_t i = 0; i < t.cantidad(); ++i) {
        std::string_view n = t.nombre(i);
        if (n.size() < 7 || n.substr(n.size() - 7) != "-e.html" || !r.texto(n, d)) continue;
        maquetar_html({reinterpret_cast<const char*>(d.data()), d.size()}, {}, '\x01', "X", 260, medir, nullptr, p);
        ++paginas;
        bool vinculo = false;
        for (const ElementoHtml& e : p.elementos) vinculo |= e.vinculo_len > 0;
        con_vinculos += vinculo;
    }
    CHECK(paginas > 6000);
    CHECK(con_vinculos > 3000);
}
