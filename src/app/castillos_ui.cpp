// Castillos y guerras de clanes.
//  - Señor del castillo (S_CastleMaster): una corona junto al nombre (dibujo propio: el original no
//    se encontró en Sprite.pak).
//  - Guerras (S_War): el aviso en el chat y la lista de clanes enemigos. "/war clan" declara (si el
//    clan es dueño de un castillo, es un asedio: dentro del horario y desde fuera del castillo),
//    "/surrender clan" se rinde y "/ceasefire clan" termina la guerra (C_War 0, 2 y 3).
//  - Asedio (S_PacketBox 0–4): avisos en el chat y un cartel arriba mientras dura.
//  - Chambelán (todo HTML): "tax" responde S_TaxRate, "cdeposit" S_Deposit y "withdrawal" S_Drawal;
//    los tres abren el diálogo de cantidad, que responde C_TaxRate, C_Deposit o C_Drawal.
//  - "/wartime" (C_ChangeWarTime): el servidor responde S_WarTime con la fecha del próximo asedio
//    (solo la muestra: no hay paquete para elegir otra).
#include <cstdio>
#include <cstring>
#include <ctime>
#include "app/app.h"

namespace lin {

namespace {

void aviso(App& a, std::string_view t) { a.chat.agregar(t, rgba(255, 200, 120), CHAT_SISTEMA); }

std::string_view clan_propio(const App& a) {
    return a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) ? std::string_view(a.pjs[a.pj_sel].clan) : std::string_view{};
}

void enemigo(App& a, std::string_view clan, bool agregar) {
    std::erase(a.castillos.enemigos, std::string(clan));
    if (agregar && !clan.empty()) a.castillos.enemigos.emplace_back(clan);
}

}  // namespace

const char* nombre_castillo(int castillo) {
    static const char* NOMBRES[] = {"?", "Kent Castle", "Orcish Forest Fortress", "Windawood Castle", "Giran Castle",
                                    "Heine Castle", "Dwarf Castle", "Aden Castle", "Diad Fortress"};
    return castillo >= 1 && castillo <= 8 ? NOMBRES[castillo] : NOMBRES[0];
}

u8 castillo_de(const App& a, ObjId id) {
    for (const SenorCastillo& s : a.castillos.senores)
        if (s.id == id) return s.castillo;
    return 0;
}

void eventos_castillos(App& a, const Evento& e) {
    Castillos& c = a.castillos;
    char txt[200];
    switch (e.tipo) {
    case Ev::SENOR_CASTILLO:
        std::erase_if(c.senores, [&](const SenorCastillo& s) { return s.id == e.id; });
        if (e.v[0]) c.senores.push_back({e.id, static_cast<u8>(e.v[0])});
        break;
    case Ev::GUERRA: {
        const int n0 = static_cast<int>(e.s[0].size()), n1 = static_cast<int>(e.s[1].size());
        const char* a0 = e.s[0].data();
        const char* a1 = e.s[1].data();
        const std::string_view propio = clan_propio(a);
        const std::string_view otro = e.s[0] == propio ? e.s[1] : e.s[0];
        const bool nos_toca = !propio.empty() && (e.s[0] == propio || e.s[1] == propio);
        switch (e.v[GU_TIPO]) {
        case 1: std::snprintf(txt, sizeof txt, "The %.*s pledge declared war on the %.*s pledge.", n0, a0, n1, a1); break;
        case 2: std::snprintf(txt, sizeof txt, "The %.*s pledge surrendered to the %.*s pledge.", n0, a0, n1, a1); break;
        case 3: std::snprintf(txt, sizeof txt, "The war between the %.*s and %.*s pledges is over.", n0, a0, n1, a1); break;
        case 4: std::snprintf(txt, sizeof txt, "The %.*s pledge won the war against the %.*s pledge.", n0, a0, n1, a1); break;
        case 6: std::snprintf(txt, sizeof txt, "The %.*s and %.*s pledges formed an alliance.", n0, a0, n1, a1); break;
        case 7: std::snprintf(txt, sizeof txt, "The alliance between the %.*s and %.*s pledges was broken.", n0, a0, n1, a1); break;
        case 8: std::snprintf(txt, sizeof txt, "Your pledge is at war with the %.*s pledge.", n1, a1); break;
        default: std::snprintf(txt, sizeof txt, "War (%d): %.*s, %.*s", e.v[GU_TIPO], n0, a0, n1, a1); break;
        }
        aviso(a, txt);
        if (e.v[GU_TIPO] == 8) enemigo(a, e.s[1], true);
        else if (nos_toca && e.v[GU_TIPO] == 1) enemigo(a, otro, true);
        else if (nos_toca && e.v[GU_TIPO] >= 2 && e.v[GU_TIPO] <= 4) enemigo(a, otro, false);
        break;
    }
    case Ev::ASEDIO: {
        const int castillo = e.v[GU_CASTILLO];
        switch (e.v[GU_TIPO]) {
        case 0: std::snprintf(txt, sizeof txt, "The siege of %s has begun.", nombre_castillo(castillo)); break;
        case 1: std::snprintf(txt, sizeof txt, "The siege of %s is over.", nombre_castillo(castillo)); break;
        case 2: std::snprintf(txt, sizeof txt, "The siege of %s is in progress.", nombre_castillo(castillo)); break;
        case 3: std::snprintf(txt, sizeof txt, "Your pledge has taken the initiative."); break;
        default: std::snprintf(txt, sizeof txt, "Your pledge has occupied the castle."); break;
        }
        aviso(a, txt);
        if (castillo >= 1 && castillo <= 8 && e.v[GU_TIPO] <= 2) c.asedio[castillo] = e.v[GU_TIPO] != 1;
        break;
    }
    case Ev::CASTILLO_FONDOS:
        if (e.v[CF_TIPO] == CF_TASA) {
            abrir_cantidad_castillo(a, CF_TASA, e.id, e.v[CF_A], e.v[CF_B], e.v[CF_A]);
        } else if (e.v[CF_TIPO] == CF_RETIRAR) {
            abrir_cantidad_castillo(a, CF_RETIRAR, e.id, 0, e.v[CF_B], e.v[CF_B]);
        } else {
            u32 adena = 0;
            const TablaItems& t = a.mundo.inventario.items;
            for (Slot s = 0; s < t.tamano(); ++s)
                if (nombre_item(a.mundo.inventario, s).starts_with("$4")) adena = t.col<I_CANTIDAD>()[s];  // $4 = Adena
            abrir_cantidad_castillo(a, CF_DEPOSITAR, e.id, 0, static_cast<i32>(std::min<u32>(adena, 0x7FFFFFFF)), 0);
        }
        break;
    case Ev::FECHA_ASEDIO:
        aviso(a, "Next siege (the server does not take a choice):");
        for (int i = 0; i < e.v[FA_N]; ++i) {
            const std::time_t t = static_cast<std::time_t>(e.v[FA_MINUTOS + i]) * 60;
            char fecha[40] = "";
            if (const std::tm* tm = std::gmtime(&t)) std::strftime(fecha, sizeof fecha, "%Y-%m-%d %H:%M", tm);
            std::snprintf(txt, sizeof txt, "  %s (%.*s)", fecha, static_cast<int>(e.s[0].size()), e.s[0].data());
            aviso(a, txt);
        }
        break;
    default: break;
    }
}

bool comando_castillos(App& a, std::string_view orden, std::string_view resto) {
    if (orden == "wartime") {
        enviar(a, cmd::fecha_asedio(a.op));
        return true;
    }
    cmd::TipoGuerra tipo;
    if (orden == "war") tipo = cmd::GUERRA_DECLARAR;
    else if (orden == "surrender") tipo = cmd::GUERRA_RENDIRSE;
    else if (orden == "ceasefire") tipo = cmd::GUERRA_TERMINAR;
    else return false;
    while (!resto.empty() && resto.front() == ' ') resto.remove_prefix(1);
    while (!resto.empty() && resto.back() == ' ') resto.remove_suffix(1);
    if (resto.empty()) aviso(a, "Usage: /war, /surrender or /ceasefire <pledge>");
    else enviar(a, cmd::guerra(a.op, tipo, resto));
    return true;
}

// Corona del señor de un castillo (12 × 9), a la izquierda de (x, y).
void dibujar_corona(Graficos& g, float x, float y, u64 k) {
    const int X = static_cast<int>(x), Y = static_cast<int>(y);
    const u32 oro = rgba(240, 200, 60), borde = rgba(90, 60, 10);
    g.gpu().rectangulo(X - 1, Y + 3, 14, 7, k, borde);
    g.gpu().rectangulo(X, Y + 4, 12, 5, k + 1, oro);
    for (int i = 0; i < 3; ++i) {  // las tres puntas
        g.gpu().rectangulo(X + i * 5 - 1, Y - 1, 4, 5, k, borde);
        g.gpu().rectangulo(X + i * 5, Y, 2, 4, k + 1, oro);
    }
    g.gpu().rectangulo(X + 5, Y + 5, 2, 2, k + 2, rgba(200, 40, 40));  // la piedra
}

// Cartel arriba mientras dura un asedio.
void dibujar_castillos(App& a) {
    Graficos& g = *a.g;
    int y = 40;
    for (int c = 1; c <= 8; ++c) {
        if (!a.castillos.asedio[c]) continue;
        char txt[96];
        std::snprintf(txt, sizeof txt, "Siege of %s in progress", nombre_castillo(c));
        const int w = ancho_texto(txt, std::strlen(txt));
        const int x = (a.gpu.ancho() - w) / 2;
        g.gpu().rectangulo(x - 8, y - 3, w + 16, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 20), rgba(90, 20, 15, 210));
        g.texto_sombra(txt, static_cast<float>(x), static_cast<float>(y), clave_orden(CAPA_UI_TEXTO, 20),
                       (a.entrada.ahora_ms / 600) % 2 ? rgba(255, 220, 130) : rgba(255, 255, 254));
        y += ALTO_GLIFO + 8;
    }
}

}  // namespace lin
