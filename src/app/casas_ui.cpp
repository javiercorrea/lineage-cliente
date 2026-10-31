// Casas de clan (agit).
//  - Tablón de subastas (L1AuctionBoard): tiene el gfx de los tablones de los pueblos (1544), así
//    que el clic ya manda C_Board; el servidor responde S_HouseList en vez de S_Board. La lista
//    muestra cada casa con su área, el cierre de la subasta y la puja actual. Un clic manda
//    C_NPCAction "select" + casa y el servidor responde la página "agsel" (S_AuctionBoardRead) en el
//    diálogo del NPC; sus acciones "map" y "apply" llevan la casa (npc_ui.cpp). "apply" responde
//    S_InputAmount "agapply" (el diálogo de cantidad).
//  - S_HouseMap: el original abre un mapa con la casa marcada; esa imagen no se encontró en
//    Sprite.pak, así que el mapa es un dibujo propio: los terrenos de las casas del pueblo (de
//    L1HouseLocation.java del servidor) en isométrico, la elegida resaltada y el personaje.
//  - El mayordomo (L1Housekeeper) es HTML; el nombre de la casa (pregunta 512) va en social_ui.cpp.
#include <algorithm>
#include <cstdio>
#include "app/app.h"

namespace lin {

namespace {

constexpr int ANCHO = 420, ALTO = 330, FILA = 18, FILAS = 13;
constexpr int ANCHO_MAPA = 340, ALTO_MAPA = 300;

// Terreno de cada casa en el mapa 4 (el rectángulo que cubre lo que el servidor cuenta como suyo).
// Giran 262145–262189, Heine 327681–327691, Gludin 524289–524294; las de Aden (458753–) no
// tienen coordenadas en el servidor.
struct Terreno {
    u32 id;
    int x, y, w, h;
};
constexpr Terreno TERRENOS[] = {
    {262145, 33368, 32651, 8, 7}, {262146, 33381, 32653, 7, 4}, {262147, 33392, 32650, 13, 7},
    {262148, 33427, 32656, 4, 7}, {262149, 33439, 32665, 7, 8}, {262150, 33454, 32648, 13, 7},
    {262151, 33476, 32665, 4, 7}, {262152, 33471, 32678, 7, 8}, {262153, 33453, 32694, 8, 7},
    {262154, 33421, 32685, 13, 7}, {262155, 33409, 32674, 7, 8}, {262156, 33414, 32703, 8, 7},
    {262157, 33372, 32692, 13, 7}, {262158, 33362, 32681, 4, 7}, {262159, 33360, 32669, 7, 8},
    {262160, 33341, 32660, 7, 8}, {262161, 33345, 32672, 4, 7}, {262162, 33338, 32704, 13, 8},
    {262163, 33349, 32728, 8, 7}, {262164, 33366, 32713, 7, 8}, {262165, 33380, 32712, 4, 7},
    {262166, 33401, 32733, 13, 7}, {262167, 33424, 32717, 7, 8}, {262168, 33448, 32729, 4, 7},
    {262169, 33404, 32754, 4, 7}, {262170, 33363, 32755, 13, 7}, {262171, 33351, 32774, 7, 8},
    {262172, 33355, 32787, 7, 4}, {262173, 33366, 32786, 8, 7}, {262174, 33383, 32773, 4, 7},
    {262175, 33397, 32788, 8, 7}, {262176, 33479, 32788, 8, 7}, {262177, 33498, 32801, 4, 7},
    {262178, 33379, 32802, 7, 4}, {262179, 33373, 32822, 13, 8}, {262180, 33398, 32810, 4, 7},
    {262181, 33397, 32821, 7, 8}, {262182, 33431, 32838, 8, 7}, {262183, 33456, 32838, 7, 4},
    {262184, 33385, 32845, 8, 7}, {262185, 33399, 32859, 7, 8}, {262186, 33414, 32850, 4, 7},
    {262187, 33372, 32867, 13, 7}, {262188, 33425, 32865, 13, 7}, {262189, 33443, 32869, 7, 8},
    {327681, 33599, 33213, 12, 6}, {327682, 33627, 33206, 6, 4}, {327683, 33626, 33221, 7, 10},
    {327684, 33628, 33241, 9, 12}, {327685, 33616, 33262, 6, 4}, {327686, 33570, 33228, 11, 7},
    {327687, 33583, 33305, 6, 12}, {327688, 33577, 33335, 12, 9}, {327689, 33615, 33374, 9, 12},
    {327690, 33624, 33393, 7, 11}, {327691, 33621, 33442, 12, 9}, {524289, 32559, 32669, 8, 8},
    {524290, 32547, 32705, 10, 12}, {524291, 32537, 32781, 8, 11}, {524292, 32550, 32780, 11, 8},
    {524293, 32534, 32807, 10, 12}, {524294, 32553, 32814, 8, 8},
};

const char* pueblo(u32 casa) {
    switch (casa >> 16) {
    case 4: return "Giran";
    case 5: return "Heine";
    case 7: return "Aden";
    case 8: return "Gludin";
    default: return "?";
    }
}

// A la derecha mientras se lee una casa (la página va en el diálogo del NPC, a la izquierda).
int x0(const App& a) { return a.npc && a.npc == a.subastas.tablon ? a.gpu.ancho() - ANCHO - 10 : (a.gpu.ancho() - ANCHO) / 2; }
int y0() { return 50; }
int xm(const App& a) { return (a.gpu.ancho() - ANCHO_MAPA) / 2; }

const CasaSubasta* casa(const App& a, u32 id) {
    for (const CasaSubasta& c : a.subastas.casas)
        if (c.id == id) return &c;
    return nullptr;
}

void dibujar_mapa(App& a) {
    Subastas& s = a.subastas;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = xm(a), y = y0() + 10;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 75), kf = clave_orden(CAPA_UI, 75);
    ui::recuadro(g, x, y, ANCHO_MAPA, ALTO_MAPA, 245);
    char txt[160];
    const CasaSubasta* c = casa(a, s.mapa);
    std::snprintf(txt, sizeof txt, "%s house %u", pueblo(s.mapa), s.mapa & 0xFFFF);
    g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    if (c) {
        std::snprintf(txt, sizeof txt, "Size %u, bid %u adena, ends %u/%u", c->area, c->precio, c->mes, c->dia);
        g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 24), kt, ui::COLOR_TEXTO);
    }
    g.texto("[x]", static_cast<float>(x + ANCHO_MAPA - 26), static_cast<float>(y + 6), kt, ui::COLOR_TEXTO);
    if ((e.clic && ui::sobre(e, x + ANCHO_MAPA - 28, y + 2, 26, 20)) || e.escape) {
        s.mapa = 0;
        e.escape = false;
        return;
    }
    // Las casas del mismo pueblo, en isométrico como el mundo: (x - y, (x + y) / 2).
    int mx0 = 1 << 30, my0 = 1 << 30, mx1 = -(1 << 30), my1 = -(1 << 30);
    auto iso = [](int wx, int wy, int& px, int& py) { px = wx - wy; py = (wx + wy) / 2; };
    const Terreno* elegido = nullptr;
    for (const Terreno& t : TERRENOS) {
        if (t.id >> 16 != s.mapa >> 16) continue;
        if (t.id == s.mapa) elegido = &t;
        const int esq[4][2] = {{t.x, t.y}, {t.x + t.w, t.y}, {t.x, t.y + t.h}, {t.x + t.w, t.y + t.h}};
        for (const auto& q : esq) {
            int px, py;
            iso(q[0], q[1], px, py);
            mx0 = std::min(mx0, px), my0 = std::min(my0, py), mx1 = std::max(mx1, px), my1 = std::max(my1, py);
        }
    }
    if (!elegido) {
        g.texto("The server has no location for this house.", static_cast<float>(x + 10), static_cast<float>(y + 50), kt,
                rgba(170, 160, 140));
        return;
    }
    const int ax = x + 12, ay = y + 44, aw = ANCHO_MAPA - 24, ah = ALTO_MAPA - 70;
    const float esc = std::min(static_cast<float>(aw) / std::max(1, mx1 - mx0), static_cast<float>(ah) / std::max(1, my1 - my0));
    auto a_pantalla = [&](int wx, int wy, float& sx, float& sy) {
        int px, py;
        iso(wx, wy, px, py);
        sx = ax + (px - mx0) * esc + (aw - (mx1 - mx0) * esc) / 2;
        sy = ay + (py - my0) * esc + (ah - (my1 - my0) * esc) / 2;
    };
    g.gpu().rectangulo(ax, ay, aw, ah, kf, rgba(28, 24, 20, 255));
    // Cada terreno como un rombo (su caja en isométrico), relleno con líneas horizontales.
    for (const Terreno& t : TERRENOS) {
        if (t.id >> 16 != s.mapa >> 16) continue;
        float sx[4], sy[4];
        a_pantalla(t.x, t.y, sx[0], sy[0]);
        a_pantalla(t.x + t.w, t.y, sx[1], sy[1]);
        a_pantalla(t.x + t.w, t.y + t.h, sx[2], sy[2]);
        a_pantalla(t.x, t.y + t.h, sx[3], sy[3]);
        const u32 color = &t == elegido ? rgba(230, 180, 60) : rgba(110, 95, 70);
        const int ymin = static_cast<int>(sy[0]), ymax = static_cast<int>(sy[2]);
        for (int fy = ymin; fy <= ymax; ++fy) {
            // Ancho del rombo en esta fila: entre los lados izquierdo (0-3-2) y derecho (0-1-2).
            auto borde = [&](int i, int j) {
                const float t0 = (sy[j] - sy[i]) != 0 ? (fy - sy[i]) / (sy[j] - sy[i]) : 0;
                return sx[i] + (sx[j] - sx[i]) * std::clamp(t0, 0.f, 1.f);
            };
            const float izq = fy < sy[3] ? borde(0, 3) : borde(3, 2), der = fy < sy[1] ? borde(0, 1) : borde(1, 2);
            g.gpu().rectangulo(static_cast<int>(izq), fy, std::max(1, static_cast<int>(der - izq)), 1, kf + 1, color);
        }
    }
    // El personaje, si está en el pueblo (mapa 4 y dentro del área dibujada).
    const TablaObjetos& o = a.mundo.objetos;
    const Slot sj = o.buscar(a.mundo.jugador.id);
    if (sj != SIN_SLOT && a.mundo.mapa == 4) {
        float px, py;
        a_pantalla(o.col<O_X>()[sj], o.col<O_Y>()[sj], px, py);
        if (px >= ax && px < ax + aw && py >= ay && py < ay + ah) {
            g.gpu().rectangulo(static_cast<int>(px) - 2, static_cast<int>(py) - 2, 5, 5, kf + 2, rgba(120, 200, 255));
            g.texto("You", px + 5, py - 6, kt, rgba(120, 200, 255));
        }
    }
    std::snprintf(txt, sizeof txt, "Lot around %d, %d (map 4)", elegido->x + elegido->w / 2, elegido->y + elegido->h / 2);
    g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + ALTO_MAPA - 20), kt, rgba(170, 160, 140));
}

}  // namespace

void eventos_casas(App& a, const Evento& e) {
    Subastas& s = a.subastas;
    switch (e.tipo) {
    case Ev::CASAS:
        s.tablon = e.id;
        s.casas.clear();
        s.desplazamiento = 0;
        s.mapa = 0;
        break;
    case Ev::CASA:
        s.casas.push_back({e.id, std::string(e.s[0]), static_cast<u16>(e.v[CS_AREA]), static_cast<u8>(e.v[CS_MES]),
                           static_cast<u8>(e.v[CS_DIA]), static_cast<u32>(e.v[CS_PRECIO])});
        break;
    case Ev::CASA_MAPA: s.mapa = static_cast<u32>(e.v[0]); break;
    default: break;
    }
}

bool sobre_casas(const App& a) {
    return (a.subastas.tablon && ui::sobre(a.entrada, x0(a), y0(), ANCHO, ALTO)) ||
           (a.subastas.mapa && ui::sobre(a.entrada, xm(a), y0() + 10, ANCHO_MAPA, ALTO_MAPA));
}

void dibujar_casas(App& a) {
    Subastas& s = a.subastas;
    if (s.mapa) return dibujar_mapa(a);  // encima de la lista
    if (!s.tablon) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = x0(a), y = y0();
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 70), kf = clave_orden(CAPA_UI, 70);
    ui::recuadro(g, x, y, ANCHO, ALTO, 235);
    g.texto("House Auction", static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    const u32 suave = rgba(170, 160, 140);
    g.texto("House", static_cast<float>(x + 12), static_cast<float>(y + 28), kt, suave);
    g.texto("Size", static_cast<float>(x + 200), static_cast<float>(y + 28), kt, suave);
    g.texto("Ends", static_cast<float>(x + 250), static_cast<float>(y + 28), kt, suave);
    g.texto("Bid", static_cast<float>(x + 310), static_cast<float>(y + 28), kt, suave);
    const int n = static_cast<int>(s.casas.size());
    if (ui::sobre(e, x, y, ANCHO, ALTO)) s.desplazamiento -= e.rueda;
    s.desplazamiento = std::clamp(s.desplazamiento, 0, std::max(0, n - FILAS));
    if (s.casas.empty()) g.texto("No houses on sale.", static_cast<float>(x + 12), static_cast<float>(y + 46), kt, ui::COLOR_TEXTO);
    char txt[64];
    for (int i = 0; i < FILAS && s.desplazamiento + i < n; ++i) {
        const CasaSubasta& c = s.casas[static_cast<size_t>(s.desplazamiento + i)];
        const int fy = y + 44 + i * FILA;
        const bool encima = ui::sobre(e, x + 8, fy, ANCHO - 16, FILA - 2);
        g.gpu().rectangulo(x + 8, fy, ANCHO - 16, FILA - 2, kf, encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        // El nombre del servidor viene en coreano (la base es la de Corea): "<pueblo> house N".
        std::snprintf(txt, sizeof txt, "%s house %u", pueblo(c.id), c.id & 0xFFFF);
        g.texto(txt, static_cast<float>(x + 12), static_cast<float>(fy + 2), kt, ui::COLOR_TEXTO);
        std::snprintf(txt, sizeof txt, "%u", c.area);
        g.texto(txt, static_cast<float>(x + 200), static_cast<float>(fy + 2), kt, ui::COLOR_TEXTO);
        std::snprintf(txt, sizeof txt, "%u/%u", c.mes, c.dia);
        g.texto(txt, static_cast<float>(x + 250), static_cast<float>(fy + 2), kt, ui::COLOR_TEXTO);
        std::snprintf(txt, sizeof txt, "%u", c.precio);
        g.texto(txt, static_cast<float>(x + 310), static_cast<float>(fy + 2), kt, rgba(255, 220, 130));
        if (encima && e.clic) enviar(a, cmd::accion_npc(a.op, s.tablon, "select", std::to_string(c.id)));
    }
    if (n > FILAS) {
        std::snprintf(txt, sizeof txt, "%d-%d of %d (wheel)", s.desplazamiento + 1, std::min(n, s.desplazamiento + FILAS), n);
        g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + ALTO - 22), kt, suave);
    }
    if (ui::boton_texto(g, e, "Close", x + ANCHO - 86, y + ALTO - 26, 76, 20) || e.escape) {
        s.tablon = 0;
        e.escape = false;
    }
}

}  // namespace lin
