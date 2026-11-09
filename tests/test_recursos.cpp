// Tests contra la instalación real del juego (LINEAGE_DIR). Se saltan si no está definida.
#include <doctest/doctest.h>
#include <fstream>
#include <sstream>
#include "comun.h"
#include "recursos/fuente.h"
#include "recursos/mapa.h"
#include "recursos/pak.h"
#include "recursos/sprites.h"
#include "recursos/tabla_sprites.h"
#include "recursos/textos.h"
#include "recursos/tiles.h"

using namespace lin;

static u32 rgb_gris(u32 v) { return v | (v << 8) | (v << 16) | 0xFF000000u; }

TEST_CASE("Recursos del juego: .pak, imágenes, textos, tiles y sprites" * doctest::skip(carpeta_juego().empty())) {
    Recursos r;
    REQUIRE(r.abrir(carpeta_juego()));
    CHECK(r.pak_sprites_base().cantidad() > 1000);
    CHECK(r.pak_textos().cantidad() > 1000);

    // 814.png está en Sprite00.pak (hash del nombre) y viene ofuscado ("PNX").
    ImagenRGBA img;
    Bytes png = r.sprite("814.png");
    REQUIRE_FALSE(png.empty());
    CHECK(png[3] == 'X');
    REQUIRE(decodificar_png(png, img));
    CHECK(img.ancho == 640);
    CHECK(img.alto == 480);

    REQUIRE(decodificar_img(r.sprite("59.img"), img));
    CHECK(img.ancho > 0);

    TablaTexto intro;
    REQUIRE(intro.cargar(r, "intro-e.tbl"));
    CHECK(intro.cantidad() > 5);
    TablaTexto desc;
    REQUIRE(desc.cargar(r, "desc-e.tbl"));
    CHECK(desc.linea(0) == "Long Sword");
    CHECK(desc.resolver("$0") == "Long Sword");
    CHECK(desc.resolver("Pepe") == "Pepe");

    JuegoTiles til;
    REQUIRE(til.abrir(r.tile("0.til")));
    CHECK(til.cantidad > 0);
    REQUIRE(til.decodificar(0, false, img));
    CHECK(img.ancho == 24);

    ArchivoSpr spr;
    REQUIRE(spr.abrir(r.sprite("3225-8.spr")));  // príncipe, quieto, dirección 0
    REQUIRE_FALSE(spr.frames.empty());
    REQUIRE(spr.componer_frame(0, img));
    CHECK(img.ancho > 10);
    CHECK(img.alto > 20);

    FuenteBitmap f;
    REQUIRE(f.cargar(carpeta_juego()));
    REQUIRE(f.rasterizar('A' - 0x20, img));
    CHECK(img.ancho == 6);
}

TEST_CASE("Tabla de sprites (datos/sprite_table.txt)") {
    TablaSprites t;
    if (!t.cargar_archivo("datos/sprite_table.txt") && !t.cargar_archivo("../../datos/sprite_table.txt")) return;
    // Filas agregadas a mano: los dark elf 75 y 80 usan los archivos de los otros dark elf.
    CHECK(t.gfx_archivos(9225) == t.gfx_archivos(6160));
    CHECK(t.gfx_archivos(9226) == t.gfx_archivos(6160));
    CHECK(t.accion(9225, ACC_CAMINAR) != nullptr);
    CHECK(t.accion(9226, 8) != nullptr);
    CHECK(t.gfx_archivos(9224) == 9224);
    CHECK(t.cantidad_gfx() > 1000);
    CHECK(t.gfx_archivos(0) == 3225);  // "312=3225"
    const RangoAccion* caminar = t.accion(0, ACC_CAMINAR);
    REQUIRE(caminar);
    CHECK(caminar->cantidad == 4);
    CHECK(caminar->direccional == 1);
    CHECK(t.frame(caminar->primer).sub == 0);
    const RangoAccion* quieto = t.accion(0, ACC_QUIETO);
    REQUIRE(quieto);
    CHECK(t.frame(quieto->primer).sub == 8);
}

TEST_CASE("Tabla de sprites: sufijos sueltos y pegados") {
    // Caminar de la rana (gfx 55): "4>" es un sufijo sin número y "4<41" un sonido; ninguno debe
    // desfasar la lectura de los frames siguientes.
    const char reg[] = "?16 0 1 9 0 0 4 0 1 4> 0 2 4<41 0 3 4 0 4 4> 0 5 4 0 6 4 0 7 4> 0 8 4 3 1 2 8 1 4 8 0 8<43 102 10";
    std::vector<u8> t(reg, reg + sizeof reg - 1);
    TablaSprites s;
    REQUIRE(s.cargar(t));
    const RangoAccion* r = s.accion(0, ACC_CAMINAR);
    REQUIRE(r);
    CHECK(r->cantidad == 9);
    for (u32 i = 0; i < r->cantidad; ++i) {
        CHECK(s.frame(r->primer + i).sub == 0);
        CHECK(s.frame(r->primer + i).frame == i);
        CHECK(s.frame(r->primer + i).duracion == 4);
    }
    CHECK(s.frame(r->primer + 2).sonido == 41);
    CHECK(s.duracion_ms(0, ACC_CAMINAR) == 1440);
    const RangoAccion* q = s.accion(0, ACC_QUIETO);
    REQUIRE(q);
    CHECK(q->cantidad == 2);
    CHECK(s.frame(q->primer + 1).sonido == 43);
}

TEST_CASE("Tabla de sprites: caminar dura lo mismo que move_speed del servidor") {
    TablaSprites t;
    if (!t.cargar_archivo("datos/sprite_table.txt") && !t.cargar_archivo("../../datos/sprite_table.txt")) return;
    // npcs.move_speed / spr_actions de L1J-JP 3.63c (gfx: ms por celda).
    const struct { GfxId gfx; u32 ms; } casos[] = {
        {0, 640}, {61, 640}, {55, 1440}, {1022, 760}, {56, 800}, {998, 960}, {1003, 1280}, {1059, 1280},
    };
    for (const auto& c : casos) {
        CAPTURE(c.gfx);
        CHECK(t.duracion_ms(c.gfx, ACC_CAMINAR) == c.ms);
    }
}

// Orientación de los .s32 (y exterior, x interior) y significado del bit 0 del atributo,
// comparando con el mapa en texto del servidor L1J (filas = y, columnas = x).
TEST_CASE("Mapa 2005: atributos del .s32 coinciden con el mapa del servidor" * doctest::skip(carpeta_juego().empty())) {
    std::ifstream f("servidor_local/l1j-jp/maps/2005.txt");
    if (!f) f.open("../../servidor_local/l1j-jp/maps/2005.txt");
    if (!f) return;  // servidor local no instalado
    std::vector<std::vector<int>> srv;
    for (std::string linea; std::getline(f, linea);) {
        std::vector<int> fila;
        std::stringstream ss(linea);
        for (std::string v; std::getline(ss, v, ',');)
            if (!v.empty()) fila.push_back(std::stoi(v));
        if (!fila.empty()) srv.push_back(fila);
    }
    const int X0 = 32512, Y0 = 32704;  // map_ids 2005
    std::vector<u8> datos;
    REQUIRE(leer_archivo(carpeta_juego() + nombre_segmento(2005, 32691, 32864), datos));
    SegmentoMapa s;
    REQUIRE(decodificar_s32(datos, 32691 & ~63, 32864 & ~63, s));
    int coinciden = 0, total = 0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            int t = srv[s.base_y + y - Y0][s.base_x + x - X0];
            bool bloqueada_cliente = (s.atributo_en(x, y, 0) & 1) && (s.atributo_en(x, y, 1) & 1);
            bool bloqueada_servidor = (t & 3) == 0;
            coinciden += bloqueada_cliente == bloqueada_servidor;
            ++total;
        }
    }
    CHECK(coinciden * 100 / total >= 95);
}

TEST_CASE("Mapa generado .mg: paleta de piso, colisión y estampa de pared por clase") {
    // Bytes fijos, sin instalación: MG03 + paleta de 1 piso + 4 estampas (clase 0 con 2 piezas) +
    // grilla 64×64 todo piso (índice 0) salvo (2,3) que es pared de clase 0.
    auto u16le = [](std::vector<u8>& b, u16 v) { b.push_back(v & 0xFF); b.push_back(v >> 8); };
    auto u32le = [](std::vector<u8>& b, u32 v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
    std::vector<u8> b = {'M', 'G', '0', '3'};
    u16le(b, 0x8000);        // base_x del archivo
    u16le(b, 0x8000);        // base_y
    u16le(b, 1);             // paleta de piso: 1 par
    u32le(b, 0x0009c8aeu); u32le(b, 0x0009c8afu);
    u16le(b, 2);             // clase 0 (horizontal): 2 piezas
    b.insert(b.end(), {0, 0, 16}); u32le(b, 0x0000101eu);
    b.insert(b.end(), {1, 0, 16}); u32le(b, 0x0000101fu);
    for (int c = 1; c < 4; ++c) u16le(b, 0);  // clases 1-3 vacías
    for (int ly = 0; ly < 64; ++ly)
        for (int lx = 0; lx < 64; ++lx)
            b.push_back(lx == 2 && ly == 3 ? 0x80 : 0);  // (2,3) pared clase 0; resto piso índice 0

    SegmentoMapa s;
    REQUIRE(decodificar_mg(b, 0x8000, 0x8000, s));
    CHECK(s.base_x == 0x8000);
    CHECK(s.base_y == 0x8000);
    // Piso de la paleta (índice 0) en una celda de piso.
    CHECK(s.suelo_en(10, 10, 0) == 0x0009c8aeu);
    CHECK(s.suelo_en(10, 10, 1) == 0x0009c8afu);
    // Colisión: la celda (2,3) bloquea; una celda de piso no.
    CHECK(((s.atributo_en(2, 3, 0) & 1) && (s.atributo_en(2, 3, 1) & 1)));
    CHECK_FALSE(((s.atributo_en(10, 10, 0) & 1) && (s.atributo_en(10, 10, 1) & 1)));
    // La estampa de la clase 0 (2 piezas) se expande en la celda-pared (2,3).
    REQUIRE(s.tiles_objetos.size() == 2);
    REQUIRE(s.objetos.size() == 1);
    CHECK(s.objetos[0].cantidad == 2);
    CHECK(s.tiles_objetos[0].x == 4);   // lx*2 + dx(0)
    CHECK(s.tiles_objetos[0].y == 3);   // ly + dy(0)
    CHECK(s.tiles_objetos[0].tile == 0x0000101eu);
    CHECK(s.tiles_objetos[1].x == 5);   // lx*2 + dx(1)
    CHECK(s.tiles_objetos[1].tile == 0x0000101fu);
}

TEST_CASE("Mapa generado .mg: cabecera inválida") {
    std::vector<u8> malo = {'X', 'X', 'X', 'X', 0, 0, 0, 0};
    SegmentoMapa s;
    CHECK_FALSE(decodificar_mg(malo, 0, 0, s));  // magic equivocado
}

TEST_CASE("Mapa 2005: la copa de un árbol se ordena con el pie del tronco" * doctest::skip(carpeta_juego().empty())) {
    // Árbol junto al punto de inicio (32691, 32864): su pieza más alta está en (110, 32) del segmento.
    std::vector<u8> d;
    REQUIRE(leer_archivo(carpeta_juego() + nombre_segmento(2005, 32691, 32864), d));
    SegmentoMapa s;
    REQUIRE(decodificar_s32(d, 32640, 32832, s));
    const TileObjetoMapa* copa = nullptr;
    for (const TileObjetoMapa& t : s.tiles_objetos)
        if (t.x == 110 && t.y == 32 && (t.tile >> 8) == 3883) copa = &t;
    REQUIRE(copa);
    // Su pie es el del tronco, 9 filas más abajo (la pieza (100, 36) de capa 17), no su propia
    // fila ni la sombra del árbol (capa 8, pegada al suelo, que llega más abajo).
    CHECK(copa->base == 9);
    for (const TileObjetoMapa& t : s.tiles_objetos) {
        CHECK(t.base >= 0);
        if (pieza_de_suelo(t)) CHECK(t.base == 0);
    }
}

TEST_CASE("Mapa 2005: las sombras son tiles translúcidos" * doctest::skip(carpeta_juego().empty())) {
    Recursos r;
    REQUIRE(r.abrir(carpeta_juego()));
    std::vector<u8> d;
    REQUIRE(leer_archivo(carpeta_juego() + nombre_segmento(2005, 32691, 32864), d));
    SegmentoMapa s;
    REQUIRE(decodificar_s32(d, 32640, 32832, s));
    std::vector<JuegoTiles> juegos(4096);
    int sombras = 0, altas_translucidas = 0, altas = 0;
    for (const TileObjetoMapa& t : s.tiles_objetos) {
        JuegoTiles& j = juegos[t.tile >> 8];
        if (!j.cantidad) {
            char n[32];
            std::snprintf(n, sizeof n, "%u.til", t.tile >> 8);
            REQUIRE(j.abrir(r.tile(n)));
        }
        bool translucido = j.translucido(t.tile & 0xFF);
        if (t.capa == 8) {
            // Las sombras de los árboles: todas translúcidas, con el alfa fijo.
            CHECK(translucido);
            ImagenRGBA img;
            REQUIRE(j.decodificar(t.tile & 0xFF, t.x & 1, img));
            for (u32 p : img.px) {
                const u32 alfa = p >> 24;
                CHECK((alfa == 0 || alfa == ALFA_TRANSLUCIDO));
            }
            ++sombras;
        } else if (!pieza_de_suelo(t)) {
            altas_translucidas += translucido;
            ++altas;
        }
    }
    CHECK(sombras > 100);
    CHECK(altas_translucidas * 1000 < altas);  // casi ninguna pieza que se levanta
}

TEST_CASE("PNG: el fondo negro se va y el resplandor unido a él queda translúcido") {
    // Fila: negro, resplandor gris 0x20, figura clara, oscuro 0x10 dentro de la figura, figura clara.
    ImagenRGBA img;
    img.preparar(5, 1);
    const u32 claro = rgb_gris(0xC0), resplandor = rgb_gris(0x20), oscuro = rgb_gris(0x10);
    img.px = {rgb_gris(0), resplandor, claro, oscuro, claro};
    quitar_fondo_negro(img);
    CHECK(img.px[0] == 0);
    const u32 alfa = img.px[1] >> 24, aporte = (img.px[1] & 0xFF) * alfa / 255;
    CHECK(alfa == 0x20 * 255 / 0x40);  // alfa proporcional al brillo
    CHECK(aporte == 0x1F);             // al dibujarlo suma ~su color
    CHECK(img.px[2] == claro);
    CHECK(img.px[3] == oscuro);  // no toca el fondo: parte de la figura
}

TEST_CASE("Mapa 4: portales al final del .s32 (los de DungeonTable del servidor)" * doctest::skip(carpeta_juego().empty())) {
    // Segmento 7ffe8009: "WWin" en (32686, 33376) y "entWWinB" en (32678, 33392), al castillo de
    // Windawood (mapa 29), como en la tabla dungeons del servidor; "crossWWa" es un cruce del mismo mapa.
    std::vector<u8> d;
    REQUIRE(leer_archivo(carpeta_juego() + nombre_segmento(4, 32686, 33376), d));
    SegmentoMapa s;
    REQUIRE(decodificar_s32(d, 32686 & ~63, 33376 & ~63, s));
    REQUIRE(s.portales.size() == 4);
    auto hay = [&](int x, int y, u16 mapa) {
        for (const PortalMapa& p : s.portales)
            if (s.base_x + p.x == x && s.base_y + p.y == y && p.mapa == mapa) return true;
        return false;
    };
    CHECK(hay(32686, 33376, 29));
    CHECK(hay(32678, 33392, 29));
    CHECK(hay(32682, 33387, 4));
}
