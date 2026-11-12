// Reiniciar atributos con la Candle of Reminiscence (49142): se habla con Lolo (Gludio) y el
// servidor lleva al personaje a una sala aparte (mapa 997) y manda S_CharReset 1. El jugador:
//  1. reparte los 75 puntos iniciales, como al crear el personaje (el servidor no los valida:
//     cada atributo entre la base de la clase y base + bonus, y la suma 75);
//  2. sube de nivel hasta el que tenía: de a 1 o de a 10 hasta el 50; desde ahí cada nivel suma un
//     punto en el atributo que elija; el último nivel termina (C_CharReset 2, 8 + atributo);
//  3. si tenía elixires, el servidor pide repartir esos puntos (S_CharReset 3) y se mandan los 6
//     atributos finales.
// Al terminar, el servidor guarda, quita la vela y lo deja en Gludio.
#include <cstdio>
#include "app/app.h"

namespace lin {

namespace {

constexpr int ANCHO = 300, ALTO = 250;
constexpr const char* NOMBRES[6] = {"STR", "INT", "WIS", "DEX", "CON", "CHA"};
// Orden de C_CharReset (fue, int, sab, des, con, car) -> orden de la tabla de creación (fue, des, con, sab, car, int).
constexpr int A_CREAR[6] = {0, 5, 3, 1, 2, 4};

int clase(const App& a) { return a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) ? a.pjs[a.pj_sel].v[PJ_CLASE] : 0; }
int base_de(const App& a, int i) { return stats_base_clase(clase(a))[A_CREAR[i]]; }

int libres_inicial(const App& a) {
    int suma = 0;
    for (int i = 0; i < 6; ++i) suma += base_de(a, i) + a.reinicio.extra[i];
    return 75 - suma;
}

}  // namespace

void eventos_reinicio(App& a, const Evento& e) {
    if (e.tipo != Ev::REINICIO) return;
    ReinicioAtributos& r = a.reinicio;
    if (e.v[RE_TIPO] == 1) {
        r = {};
        r.fase = ReinicioAtributos::INICIAL;
        r.hp = e.v[RE_HP];
        r.mp = e.v[RE_MP];
        r.ac = e.v[RE_AC];
        r.maximo = e.v[RE_MAX];
        for (int i = 0; i < 6; ++i) r.stats[i] = static_cast<u8>(base_de(a, i));
    } else if (e.v[RE_TIPO] == 2) {
        r.fase = ReinicioAtributos::SUBIR;
        r.nivel = e.v[RE_NIVEL];
        r.maximo = e.v[RE_MAX];
        r.hp = e.v[RE_HP];
        r.mp = e.v[RE_MP];
        r.ac = e.v[RE_AC];
        for (int i = 0; i < 6; ++i) r.stats[i] = static_cast<u8>(e.v[RE_FUE + i]);
    } else {
        r.fase = ReinicioAtributos::ELIXIR;
        r.puntos = e.v[RE_PUNTOS];
        for (u8& x : r.extra) x = 0;
    }
}

void dibujar_reinicio(App& a) {
    ReinicioAtributos& r = a.reinicio;
    if (r.fase == ReinicioAtributos::CERRADO) return;
    if (r.fase == ReinicioAtributos::CERRADO) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = (a.gpu.ancho() - ANCHO) / 2, y = (a.gpu.alto() - ALTO) / 3;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 90);
    ui::recuadro(g, x, y, ANCHO, ALTO, 240);
    char t[96];
    g.texto("Reset Attributes", static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);

    const bool inicial = r.fase == ReinicioAtributos::INICIAL, elixir = r.fase == ReinicioAtributos::ELIXIR;
    const int libres = inicial ? libres_inicial(a) : elixir ? r.puntos - [&] {
        int s = 0;
        for (u8 v : r.extra) s += v;
        return s;
    }() : 0;
    if (inicial) std::snprintf(t, sizeof t, "Initial stats: %d points left", libres);
    else if (elixir) std::snprintf(t, sizeof t, "Elixir points: %d left", libres);
    else std::snprintf(t, sizeof t, "Level %d / %d   HP %d  MP %d  AC %d", r.nivel, r.maximo, r.hp, r.mp, r.ac);
    g.texto(t, static_cast<float>(x + 10), static_cast<float>(y + 30), kt, ui::COLOR_TEXTO);

    // Los seis atributos; en la fase inicial y con elixires, con − y +.
    for (int i = 0; i < 6; ++i) {
        const int fy = y + 54 + i * 22;
        const int valor = (inicial ? base_de(a, i) : r.stats[i]) + ((inicial || elixir) ? r.extra[i] : 0);
        std::snprintf(t, sizeof t, "%s  %2d", NOMBRES[i], valor);
        g.texto(t, static_cast<float>(x + 16), static_cast<float>(fy + 3), kt, ui::COLOR_TEXTO);
        if (inicial || elixir) {
            const int tope = inicial ? bonus_clase(clase(a)) : 255;
            if (ui::boton_texto(g, e, "-", x + 100, fy, 22, 18) && r.extra[i] > 0) --r.extra[i];
            if (ui::boton_texto(g, e, "+", x + 126, fy, 22, 18) && libres > 0 && r.extra[i] < tope) ++r.extra[i];
        }
    }

    const int by = y + ALTO - 28;
    if (inicial) {
        if (libres == 0 && ui::boton_texto(g, e, "OK", x + ANCHO - 86, by, 76, 20)) {
            u8 v[6];
            for (int i = 0; i < 6; ++i) v[i] = static_cast<u8>(base_de(a, i) + r.extra[i]);
            enviar(a, cmd::reinicio(a.op, 1, {v[0], v[1], v[2], v[3], v[4], v[5]}));
        }
    } else if (elixir) {
        if (libres == 0 && ui::boton_texto(g, e, "Finish", x + ANCHO - 86, by, 76, 20)) {
            u8 v[6];
            for (int i = 0; i < 6; ++i) v[i] = static_cast<u8>(r.stats[i] + r.extra[i]);
            enviar(a, cmd::reinicio(a.op, 3, {v[0], v[1], v[2], v[3], v[4], v[5]}));
            r.fase = ReinicioAtributos::CERRADO;
        }
    } else if (r.nivel < std::min(r.maximo, 50)) {
        // Hasta el 50, sin puntos: de a 1 o de a 10 (sin pasar el 50, que perdería los puntos).
        if (ui::boton_texto(g, e, "+1 level", x + 120, by, 80, 20)) enviar(a, cmd::reinicio(a.op, 2, {0}));
        if (r.nivel + 10 <= std::min(r.maximo, 50) && ui::boton_texto(g, e, "+10", x + ANCHO - 86, by, 76, 20))
            enviar(a, cmd::reinicio(a.op, 2, {7}));
    } else {
        // Desde el 50, cada nivel con su punto; el último nivel (o el máximo si es ≤ 50) termina.
        const bool ultimo = r.maximo <= 50 || r.nivel >= r.maximo - 1;
        g.texto(r.maximo <= 50 ? "Done." : ultimo ? "Last level: pick a stat" : "Next level: pick a stat",
                static_cast<float>(x + 160), static_cast<float>(y + 57), kt, rgba(255, 230, 150));
        if (r.maximo <= 50) {
            if (ui::boton_texto(g, e, "Finish", x + ANCHO - 86, by, 76, 20)) {
                enviar(a, cmd::reinicio(a.op, 2, {8, 0}));
                r.fase = ReinicioAtributos::CERRADO;  // si faltan elixires, S_CharReset 3 la vuelve a abrir
            }
        } else {
            for (int i = 0; i < 6; ++i)
                if (ui::boton_texto(g, e, "+1", x + 100, y + 54 + i * 22, 30, 18)) {
                    enviar(a, ultimo ? cmd::reinicio(a.op, 2, {8, static_cast<u8>(i + 1)}) : cmd::reinicio(a.op, 2, {static_cast<u8>(i + 1)}));
                    if (ultimo) r.fase = ReinicioAtributos::CERRADO;
                }
        }
    }
    // El teclado es del diálogo mientras está abierto.
    e.ntexto = 0;
    e.enter = e.escape = false;
}

}  // namespace lin
