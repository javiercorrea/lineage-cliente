// Entregable 1: pantalla de login. Posiciones e imágenes de drawmode/draw_login.cpp de
// l1j-client: fondo 814.png, recuadro 59.img, botones 53/65/55/57 (+1 = resaltado). Con la UI
// simple (F2), un recuadro de texto bajo el logo propio (opción "logo" de settings.ini).
#include <algorithm>
#include "app/app.h"

namespace lin {

namespace {

// Las cajas del 59.img de esta instalación están unos píxeles más abajo que en l1j-client.
constexpr int X_CAJAS = 520, Y_USUARIO = 340, Y_CLAVE = 362, ANCHO_CAJA = 100;
constexpr int X_BOTONES = 0x213;

const char* motivo_login(int codigo) {
    switch (codigo) {
    case 7: return "La cuenta ya existe.";
    case 8: return "Usuario o clave incorrectos.";
    case 22: return "La cuenta ya está en uso.";
    default: return "El servidor rechazó el ingreso.";
    }
}

void iniciar_login(App& a) {
    if (a.usuario.empty() || a.clave.empty() || a.esperando) return;
    a.aviso.clear();
    a.esperando = true;
    a.red.iniciar(a.cfg.host, a.cfg.puerto, a.op);  // al conectar se envían versión y login
    a.estado_red_previo = EstadoRed::CONECTANDO;
}

// Logo de la UI simple: el PNG de la opción "logo" achicado para entrar en el recuadro, o el
// título en texto.
void dibujar_logo(App& a, int x, int y, int w, int h) {
    Graficos& g = *a.g;
    if (!a.cfg.logo.empty()) {
        static const std::string carpeta_config = ruta_configuracion().substr(0, ruta_configuracion().rfind('/') + 1);
        const std::string ruta = a.cfg.logo[0] == '/' ? a.cfg.logo : carpeta_config + a.cfg.logo;
        const RegionAtlas& r = g.png_archivo(ruta);
        if (r.valida()) {
            const float esc = std::min({1.0f, static_cast<float>(w) / r.w, static_cast<float>(h) / r.h});
            const float lw = r.w * esc, lh = r.h * esc;
            g.gpu().dibujar_escalado(r, x + (w - lw) / 2, y + (h - lh) / 2, lw, lh, clave_orden(CAPA_UI, 1));
            return;
        }
    }
    ui::texto_centrado(g, "L I N E A G E", x, y + h / 2 - 8, w, ui::COLOR_TITULO);
}

}  // namespace

void eventos_login(App& a, const Evento& e) {
    switch (e.tipo) {
    case Ev::RESULTADO_LOGIN:
        if (e.v[0] != 0) {
            a.aviso = motivo_login(e.v[0]);
            a.esperando = false;
            a.red.cerrar();
            a.estado_red_previo = EstadoRed::CERRADA;
        }
        break;
    case Ev::NOTICIAS:
        a.noticias.assign(e.s[0]);
        a.mostrando_noticias = true;
        if (a.aut.activa) {
            a.mostrando_noticias = false;
            enviar(a, cmd::lista_personajes(a.op));
        }
        break;
    default: break;
    }
}

void frame_login(App& a) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const bool simple = a.cfg.ui_simple;
    // Cajas de usuario y clave: las del recuadro 59.img o las de la UI simple.
    const int x_cajas = simple ? 290 : X_CAJAS, y_usuario = simple ? 266 : Y_USUARIO, y_clave = simple ? 294 : Y_CLAVE;
    const int ancho_caja = simple ? 130 : ANCHO_CAJA;
    if (simple) {
        g.gpu().rectangulo(0, 0, g.gpu().ancho(), g.gpu().alto(), clave_orden(CAPA_FONDO, 0), ui::COLOR_FONDO_SIMPLE);
        dibujar_logo(a, 40, 24, 560, 190);
        ui::recuadro(g, 190, 240, 260, 120);
        g.texto("Account", 206, y_usuario, clave_orden(CAPA_UI_TEXTO, 0), ui::COLOR_TITULO);
        g.texto("Password", 206, y_clave, clave_orden(CAPA_UI_TEXTO, 0), ui::COLOR_TITULO);
        char pie[96];
        std::snprintf(pie, sizeof pie, "%s:%u  -  F2: original interface", a.cfg.host.c_str(), a.cfg.puerto);
        ui::texto_centrado(g, pie, 0, 456, 640, rgba(120, 110, 90));
    } else {
        g.gpu().dibujar(g.png(814), 0, 0, clave_orden(CAPA_FONDO, 0));
        // Texto de bienvenida (intro-e.tbl) sobre el pergamino de la izquierda.
        for (size_t i = 0; i < a.intro.cantidad() && i < 18; ++i)
            g.texto(a.intro.linea(i), 34, 59 + 12 * static_cast<int>(i), clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
        ui::imagen(g, 59, 0x1a9, 0x138);
    }

    if (e.clic && ui::sobre(e, x_cajas - 4, y_usuario - 4, ancho_caja + 8, 20)) a.foco = 0;
    if (e.clic && ui::sobre(e, x_cajas - 4, y_clave - 4, ancho_caja + 8, 20)) a.foco = 1;
    if (e.tab) a.foco ^= 1;
    bool bloqueado = a.esperando || a.mostrando_noticias;
    EntradaFrame sin_texto = e;
    sin_texto.ntexto = 0;
    sin_texto.retroceso = false;
    const EntradaFrame& ec = bloqueado ? sin_texto : e;
    auto caja = simple ? ui::campo : ui::caja_texto;
    if (caja(g, ec, a.usuario, x_cajas, y_usuario, ancho_caja, a.foco == 0, false, 20)) a.foco = 1;
    bool enter_clave = caja(g, ec, a.clave, x_cajas, y_clave, ancho_caja, a.foco == 1, true, 20);

    bool login = enter_clave;
    if (simple) {
        login |= ui::boton_texto(g, e, "Login", 206, 326, 108, 20);
        if (ui::boton_texto(g, e, "Quit", 326, 326, 108, 20)) a.salir = true;
    } else {
        login |= ui::boton(g, e, 53, X_BOTONES, 0x183);
        ui::boton(g, e, 65, X_BOTONES, 0x195);  // nueva cuenta: L1J las crea solas al primer login
        ui::boton(g, e, 55, X_BOTONES, 0x1a8);  // opciones (entregable 10)
        if (ui::boton(g, e, 57, X_BOTONES, 0x1c2)) a.salir = true;
    }
    if (e.escape && !a.mostrando_noticias) a.salir = true;
    if ((login || (a.aut.activa && a.frames_en_pantalla == 5)) && !bloqueado) iniciar_login(a);

    if (simple) {
        if (a.esperando && !a.mostrando_noticias) ui::texto_centrado(g, "Connecting...", 0, 372, 640, rgba(255, 230, 150));
        if (!a.aviso.empty()) ui::texto_centrado(g, a.aviso, 0, 372, 640, rgba(255, 120, 100));
    } else {
        if (a.esperando && !a.mostrando_noticias) g.texto("Conectando...", 0x1b8, 0x128, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 230, 150));
        if (!a.aviso.empty()) g.texto_sombra(a.aviso, 0x1a9, 0x128, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 120, 100));
    }

    // Noticias del servidor (S_CommonNews): se aceptan para pedir la lista de personajes.
    if (a.mostrando_noticias) {
        const int x = 150, y = 120, w = 340, h = 220;
        g.gpu().rectangulo(x, y, w, h, clave_orden(CAPA_UI, 10), rgba(20, 15, 10, 235));
        g.gpu().rectangulo(x, y, w, 1, clave_orden(CAPA_UI, 11), rgba(160, 130, 80));
        g.gpu().rectangulo(x, y + h - 1, w, 1, clave_orden(CAPA_UI, 11), rgba(160, 130, 80));
        std::string_view t = a.noticias;
        int fila = 0;
        while (!t.empty() && fila < 14) {
            size_t fin = t.find('\n');
            std::string_view l = t.substr(0, std::min<size_t>(fin, 54));
            g.texto(l, x + 10, y + 10 + fila * 13, clave_orden(CAPA_UI_TEXTO, 10), rgba(255, 255, 254));
            t = (fin == std::string_view::npos) ? std::string_view{} : t.substr(fin + 1);
            ++fila;
        }
        if (ui::boton_texto(g, e, "Aceptar", x + w / 2 - 40, y + h - 26, 80, 18) || e.enter) {
            a.mostrando_noticias = false;
            enviar(a, cmd::lista_personajes(a.op));
        }
    }
}

}  // namespace lin
