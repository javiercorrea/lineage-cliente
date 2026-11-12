// Vista del equipo (pestaña "Equipment" de la ventana de estado): la imagen del original (981 de
// Sprite.pak, 179×275: la silueta del personaje con un recuadro por parte del cuerpo y "Armor
// Class") con el ícono de lo equipado en cada recuadro. La parte de cada ítem sale de su tipo de uso
// (S_AddItem; ItemTable del servidor) y "equipado" del sufijo del nombre ($9 / $117).
// Los recuadros se midieron sobre la imagen; qué parte va en cada uno es una deducción por dónde
// caen sobre la silueta (casco en la cabeza, aro al costado, botas en los pies...). Los anillos no
// tienen recuadro: van más chicos en el espacio libre de abajo a la derecha.
// Con la UI simple no se usa la imagen: una lista de las partes (de la cabeza a los pies) con el
// ícono y el nombre de lo equipado, con el estilo de los demás paneles de texto.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr u32 IMG_EQUIPO = 981;  // 179×275

struct Recuadro {
    u8 uso;  // tipo de uso del ítem (ItemTable)
    int x, y, w, h;
    const char* nombre;
};
// Tipos de uso: 1 arma, 2 armadura, 18 camiseta, 19 capa, 20 guantes, 21 botas, 22 casco, 24 collar,
// 25 escudo, 37 cinturón, 40 aro.
constexpr Recuadro RECUADROS[] = {
    {22, 55, 36, 37, 33, "Helm"},     {40, 93, 45, 36, 33, "Earring"}, {19, 35, 80, 35, 33, "Cloak"},
    {24, 70, 80, 35, 33, "Amulet"},   {18, 105, 80, 35, 33, "T-shirt"}, {2, 71, 122, 36, 33, "Armor"},
    {20, 25, 116, 36, 33, "Gloves"},  {1, 25, 149, 36, 63, "Weapon"},   {25, 113, 116, 36, 33, "Shield"},
    {37, 113, 149, 36, 32, "Belt"},   {21, 71, 180, 36, 33, "Boots"},
};
constexpr u8 USO_ANILLO = 23;
constexpr int ANILLOS_X = 113, ANILLOS_Y = 184, LADO_ANILLO = 17;

}  // namespace

void dibujar_equipo(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const Inventario& inv = a.mundo.inventario;
    const TablaItems& t = inv.items;
    const u64 kf = clave_orden(CAPA_UI, 3), ki = clave_orden(CAPA_UI, 5), kt = clave_orden(CAPA_UI_TEXTO, 5);
    // Qué hay equipado en cada parte.
    constexpr size_t N = std::size(RECUADROS);
    Slot en[N], anillos[4];
    std::fill(std::begin(en), std::end(en), SIN_SLOT);
    int nanillos = 0;
    for (Slot s = 0; s < t.tamano(); ++s) {
        if (!item_equipado(nombre_item(inv, s))) continue;
        const u8 uso = t.col<I_USO>()[s];
        if (uso == USO_ANILLO) {
            if (nanillos < 4) anillos[nanillos++] = s;
            continue;
        }
        for (size_t i = 0; i < N; ++i)
            if (RECUADROS[i].uso == uso) {
                en[i] = s;
                break;
            }
    }

    Slot bajo = SIN_SLOT;
    auto poner = [&](Slot s, int rx, int ry, int rw, int rh) {
        const int lado = std::min({32, rw, rh});
        const RegionAtlas& ic = g.icono(t.col<I_GFX>()[s]);
        g.gpu().dibujar_escalado(ic, static_cast<float>(rx + (rw - lado) / 2), static_cast<float>(ry + (rh - lado) / 2),
                                 static_cast<float>(lado), static_cast<float>(lado), ki);
        if (ui::sobre(e, rx, ry, rw, rh)) bajo = s;
    };
    char txt[160];
    const RegionAtlas& fondo = g.img(IMG_EQUIPO);
    if (a.cfg.ui_simple || !fondo.valida()) {
        // Una fila por parte: rótulo, ícono (16×16) y nombre; los anillos, en la última fila.
        constexpr int FILA = 18, ROTULO = 58, LADO = 16;
        const int x = x0 + 14, w = ui::VENTANA_ANCHO - 28;
        int y = y0 + 28;
        const u32 rotulo = rgba(214, 190, 140), vacio = rgba(110, 100, 80);
        auto fila = [&](const char* nombre, const Slot* ss, int n) {
            g.gpu().rectangulo(x, y, w, FILA - 2, kf, n && ss[0] != SIN_SLOT ? rgba(40, 32, 22, 200) : rgba(22, 18, 14, 200));
            g.texto(nombre, static_cast<float>(x + 4), static_cast<float>(y + 2), kt, rotulo);
            const int ix = x + ROTULO;
            if (!n || ss[0] == SIN_SLOT) {
                g.texto("-", static_cast<float>(ix + 2), static_cast<float>(y + 2), kt, vacio);
            } else if (n == 1) {
                poner(ss[0], ix, y - 1, LADO, LADO + 2);
                const size_t largo = expandir_desc(a.desc, nombre_item(inv, ss[0]), txt, sizeof txt);
                std::string_view nom(txt, largo);
                if (const size_t p = nom.rfind(" ("); p != std::string_view::npos && nom.ends_with(")")) nom = nom.substr(0, p);
                const int ancho = w - ROTULO - LADO - 10;
                if (ancho_texto(nom.data(), nom.size()) > ancho) nom = nom.substr(0, bytes_que_caben(nom.data(), nom.size(), ancho));
                g.texto(nom, static_cast<float>(ix + LADO + 4), static_cast<float>(y + 2), kt, ui::COLOR_TEXTO);
                if (ui::sobre(e, x, y, w, FILA - 2)) bajo = ss[0];
            } else {
                for (int i = 0; i < n; ++i) poner(ss[i], ix + i * (LADO + 4), y - 1, LADO, LADO + 2);
            }
            y += FILA;
        };
        for (size_t i : {0, 1, 3, 2, 4, 5, 6, 7, 8, 9, 10}) fila(RECUADROS[i].nombre, &en[i], 1);  // de la cabeza a los pies
        fila("Rings", anillos, nanillos);
        std::snprintf(txt, sizeof txt, "%d", a.mundo.jugador.ca);
        g.texto("Armor Class", static_cast<float>(x + 4), static_cast<float>(y + 6), kt, rotulo);
        g.texto(txt, static_cast<float>(x + ROTULO + 30), static_cast<float>(y + 6), kt, ui::COLOR_TEXTO);
    } else {
        const int x = x0 + 34, y = y0 + 24;  // la imagen, centrada en la ventana
        g.gpu().dibujar(fondo, x, y, kf);
        for (size_t i = 0; i < N; ++i)
            if (en[i] != SIN_SLOT) poner(en[i], x + RECUADROS[i].x, y + RECUADROS[i].y, RECUADROS[i].w, RECUADROS[i].h);
        for (int i = 0; i < nanillos; ++i)
            poner(anillos[i], x + ANILLOS_X + i % 2 * (LADO_ANILLO + 2), y + ANILLOS_Y + i / 2 * (LADO_ANILLO + 2), LADO_ANILLO, LADO_ANILLO);
        // La AC en su campo.
        std::snprintf(txt, sizeof txt, "%d", a.mundo.jugador.ca);
        g.texto(txt, static_cast<float>(x + 112 - ancho_texto(txt, std::strlen(txt)) / 2), static_cast<float>(y + 223), kt,
                ui::COLOR_TEXTO);
    }

    // Con el puntero encima: el nombre y los atributos del ítem.
    if (bajo != SIN_SLOT) {
        char nombre[256], det[1024];
        const size_t n = expandir_desc(a.desc, nombre_item(inv, bajo), nombre, sizeof nombre);
        const size_t nd = describir_atributos(atributos_item(inv, bajo), det, sizeof det);
        int w = ancho_texto(nombre, n), lineas = 0;
        for (std::string_view resto(det, nd); !resto.empty(); ++lineas) {
            const size_t fin = std::min(resto.find('\n'), resto.size());
            w = std::max(w, ancho_texto(resto.data(), fin));
            resto.remove_prefix(std::min(fin + 1, resto.size()));
        }
        const int alto = ALTO_GLIFO + 3, tx = std::min(e.mx + 12, a.gpu.ancho() - w - 8),
                  ty = std::min(e.my + 16, a.gpu.alto() - (lineas + 1) * alto - 8);
        const u64 kc = clave_orden(CAPA_UI_TEXTO, 29), kn = clave_orden(CAPA_UI_TEXTO, 30);
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, (lineas + 1) * alto + 4, kc, rgba(0, 0, 0, 230));
        g.texto({nombre, n}, static_cast<float>(tx), static_cast<float>(ty), kn, rgba(255, 255, 254));
        int ly = ty + alto;
        for (std::string_view resto(det, nd); !resto.empty(); ly += alto) {
            const size_t fin = std::min(resto.find('\n'), resto.size());
            g.texto(resto.substr(0, fin), static_cast<float>(tx), static_cast<float>(ly), kn, rgba(200, 190, 160));
            resto.remove_prefix(std::min(fin + 1, resto.size()));
        }
    }
}

}  // namespace lin
