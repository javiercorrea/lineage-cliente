// Correo (C_Mail / S_Mail). "/mail" abre la bandeja (0 normal, 1 clan, 2 guardadas); un clic en una
// carta la lee (y queda leída en el servidor). Desde la carta: borrar o guardarla. "Write" escribe
// una a un personaje (50 adena) o a todo el clan (1000 adena por miembro). El asunto y el texto van
// en UTF-16LE; el cliente solo escribe y muestra ASCII (lo demás queda como '?').
// Una carta nueva llega como S_Mail 0x50: aviso en el chat (y se agrega si su bandeja está abierta).
#include <algorithm>
#include <cstdio>
#include <ctime>
#include "app/app.h"

namespace lin {

int texto_ajustado(Graficos& g, std::string_view t, int x, int y, int ancho, u64 clave, u32 color, int max_lineas);
std::string_view sin_espacios(std::string_view s);

namespace {

constexpr int ANCHO = 440, ALTO = 340, FILA = 30;
constexpr const char* BANDEJAS[3] = {"Inbox", "Clan", "Saved"};

int x0(const App& a) { return (a.gpu.ancho() - ANCHO) / 2; }
int y0() { return 50; }

void abrir_bandeja(App& a, u8 b) {
    Correo& c = a.correo;
    c.abierto = true;
    c.bandeja = b;
    c.cartas.clear();  // el servidor no manda nada si está vacía
    c.leyendo = 0;
    enviar(a, cmd::correo(a.op, b));
}

}  // namespace

bool comando_correo(App& a, std::string_view orden) {
    if (orden != "mail" && orden != "Mail") return false;
    abrir_bandeja(a, 0);
    return true;
}

void eventos_correo(App& a, const Evento& e) {
    if (e.tipo != Ev::CORREO) return;
    Correo& c = a.correo;
    const int t = e.v[CO_TIPO];
    if (t <= 2) {
        if (!e.v[CO_LISTA]) {
            c.bandeja = static_cast<u8>(t);
            c.cartas.clear();
            return;
        }
        c.cartas.push_back({e.id, static_cast<u32>(e.v[CO_FECHA]), e.v[CO_LEIDA] != 0, e.v[CO_ENVIADA] != 0,
                            std::string(e.s[0]), std::string(e.s[1])});
    } else if (t >= 0x10 && t <= 0x12) {
        c.leyendo = e.id;
        c.texto.assign(e.s[1]);
        for (Carta& x : c.cartas)
            if (x.id == e.id) x.leida = true;
    } else if (t == 0x20 || t == 0x21) {
        a.chat.agregar(e.v[CO_OK] ? "The mail was sent." : "The mail could not be sent (the mailbox is full).",
                       rgba(255, 230, 120), CHAT_SISTEMA);
    } else if ((t >= 0x30 && t <= 0x32) || t == 0x40 || t == 0x41) {
        std::erase_if(c.cartas, [&](const Carta& x) { return x.id == e.id; });  // borrada o pasada a guardadas
        if (c.leyendo == e.id) c.leyendo = 0;
    } else if (t == 0x50 && !e.v[CO_ENVIADA]) {
        char aviso[160];
        std::snprintf(aviso, sizeof aviso, "New mail from %.*s: %.*s (/mail)", static_cast<int>(e.s[0].size()), e.s[0].data(),
                      static_cast<int>(e.s[1].size()), e.s[1].data());
        a.chat.agregar(aviso, rgba(255, 230, 120), CHAT_SISTEMA);
        if (c.abierto && c.bandeja == 0) c.cartas.push_back({e.id, static_cast<u32>(std::time(nullptr)), false, false,
                                                               std::string(e.s[0]), std::string(e.s[1])});
    }
}

bool sobre_correo(const App& a) { return a.correo.abierto && ui::sobre(a.entrada, x0(a), y0(), ANCHO, ALTO); }

void dibujar_correo(App& a) {
    Correo& c = a.correo;
    if (!c.abierto) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = x0(a), y = y0(), by = y + ALTO - 26;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 72), kf = clave_orden(CAPA_UI, 72);
    ui::recuadro(g, x, y, ANCHO, ALTO, 235);
    g.texto("Mail", static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    char txt[200];

    if (c.escribiendo) {
        g.texto(c.al_clan ? "To the whole pledge (1000 adena each)" : "To a character (50 adena)", static_cast<float>(x + 60),
                static_cast<float>(y + 8), kt, rgba(170, 160, 140));
        const char* rotulos[3] = {c.al_clan ? "Pledge" : "To", "Subject", "Text"};
        std::string* campos[3] = {&c.destino, &c.asunto, &c.cuerpo};
        const size_t maximos[3] = {16, 40, 400};
        for (int i = 0; i < 3; ++i) {
            const int fy = y + 34 + i * 28;
            g.texto(rotulos[i], static_cast<float>(x + 10), static_cast<float>(fy + 3), kt, ui::COLOR_TEXTO);
            if (e.clic && ui::sobre(e, x + 70, fy, ANCHO - 80, 20)) c.foco = i;
            g.gpu().rectangulo(x + 70, fy, ANCHO - 80, 20, kf, rgba(40, 32, 25, 255));
            ui::caja_texto(g, e, *campos[i], x + 74, fy + 3, ANCHO - 88, c.foco == i, false, maximos[i]);
        }
        if (e.tab) c.foco = (c.foco + 1) % 3;
        texto_ajustado(g, c.cuerpo, x + 10, y + 124, ANCHO - 20, kt, rgba(200, 190, 160), 10);
        if (ui::boton_texto(g, e, c.al_clan ? "To a character" : "To the pledge", x + 10, by, 110, 20)) c.al_clan = !c.al_clan;
        if (!c.destino.empty() && !c.asunto.empty() && ui::boton_texto(g, e, "Send", x + ANCHO - 170, by, 76, 20)) {
            enviar(a, cmd::escribir_correo(a.op, c.al_clan, sin_espacios(c.destino), sin_espacios(c.asunto), sin_espacios(c.cuerpo)));
            c.escribiendo = false;
            c.asunto.clear();
            c.cuerpo.clear();
        }
        if (ui::boton_texto(g, e, "Cancel", x + ANCHO - 86, by, 76, 20) || e.escape) {
            c.escribiendo = false;
            e.escape = false;
        }
        e.ntexto = 0;
        e.enter = false;
        return;
    }

    // Pestañas de las bandejas.
    for (u8 b = 0; b < 3; ++b)
        if (ui::boton_texto(g, e, BANDEJAS[b], x + 60 + b * 70, y + 6, 64, 18)) abrir_bandeja(a, b);

    if (c.leyendo) {
        const auto it = std::find_if(c.cartas.begin(), c.cartas.end(), [&](const Carta& x) { return x.id == c.leyendo; });
        if (it != c.cartas.end()) {
            std::snprintf(txt, sizeof txt, "%s %s: %s", it->enviada ? "To" : "From", it->otro.c_str(), it->asunto.c_str());
            g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 34), kt, rgba(255, 220, 130));
        }
        texto_ajustado(g, c.texto, x + 10, y + 58, ANCHO - 20, kt, ui::COLOR_TEXTO, 14);
        if (ui::boton_texto(g, e, "Delete", x + 10, by, 76, 20)) enviar(a, cmd::correo(a.op, static_cast<u8>(0x30 + c.bandeja), c.leyendo));
        if (c.bandeja == 0 && ui::boton_texto(g, e, "Keep", x + 92, by, 76, 20)) enviar(a, cmd::correo(a.op, 0x40, c.leyendo));
        if (ui::boton_texto(g, e, "Back", x + ANCHO - 86, by, 76, 20) || e.escape) {
            c.leyendo = 0;
            e.escape = false;
        }
        return;
    }

    if (c.cartas.empty()) g.texto("No mail.", static_cast<float>(x + 12), static_cast<float>(y + 36), kt, ui::COLOR_TEXTO);
    for (size_t i = 0; i < c.cartas.size() && i < 9; ++i) {
        const Carta& m = c.cartas[i];
        const int fy = y + 32 + static_cast<int>(i) * FILA;
        const bool encima = ui::sobre(e, x + 8, fy, ANCHO - 16, FILA - 2);
        g.gpu().rectangulo(x + 8, fy, ANCHO - 16, FILA - 2, kf, encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        g.texto(m.asunto, static_cast<float>(x + 12), static_cast<float>(fy + 3), kt, m.leida ? rgba(170, 160, 140) : ui::COLOR_TEXTO);
        const std::time_t f = static_cast<std::time_t>(m.fecha);
        char fecha[32] = "";
        if (const std::tm* tm = std::localtime(&f)) std::strftime(fecha, sizeof fecha, "%Y-%m-%d %H:%M", tm);
        std::snprintf(txt, sizeof txt, "%s %s  %s", m.enviada ? "to" : "from", m.otro.c_str(), fecha);
        g.texto(txt, static_cast<float>(x + 12), static_cast<float>(fy + 15), kt, rgba(150, 140, 120));
        if (encima && e.clic) enviar(a, cmd::correo(a.op, static_cast<u8>(0x10 + c.bandeja), m.id));
    }
    if (ui::boton_texto(g, e, "Write", x + ANCHO - 170, by, 76, 20)) {
        c.escribiendo = true;
        c.foco = c.destino.empty() ? 0 : 1;
    }
    if (ui::boton_texto(g, e, "Close", x + ANCHO - 86, by, 76, 20) || e.escape) {
        c.abierto = false;
        e.escape = false;
    }
}

}  // namespace lin
