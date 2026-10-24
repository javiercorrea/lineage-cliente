// Entregable 1: pantalla de login. Posiciones e imágenes de drawmode/draw_login.cpp de
// l1j-client: fondo 814.png, recuadro 59.img, botones 53/65/55/57 (+1 = resaltado).
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
    g.gpu().dibujar(g.png(814), 0, 0, clave_orden(CAPA_FONDO, 0));

    // Texto de bienvenida (intro-e.tbl) sobre el pergamino de la izquierda.
    for (size_t i = 0; i < a.intro.cantidad() && i < 18; ++i)
        g.texto(a.intro.linea(i), 34, 59 + 12 * static_cast<int>(i), clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));

    ui::imagen(g, 59, 0x1a9, 0x138);
    if (e.clic && ui::sobre(e, X_CAJAS, Y_USUARIO - 2, ANCHO_CAJA, 14)) a.foco = 0;
    if (e.clic && ui::sobre(e, X_CAJAS, Y_CLAVE - 2, ANCHO_CAJA, 14)) a.foco = 1;
    if (e.tab) a.foco ^= 1;
    bool bloqueado = a.esperando || a.mostrando_noticias;
    EntradaFrame sin_texto = e;
    sin_texto.ntexto = 0;
    sin_texto.retroceso = false;
    const EntradaFrame& ec = bloqueado ? sin_texto : e;
    if (ui::caja_texto(g, ec, a.usuario, X_CAJAS, Y_USUARIO, ANCHO_CAJA, a.foco == 0, false, 20)) a.foco = 1;
    bool enter_clave = ui::caja_texto(g, ec, a.clave, X_CAJAS, Y_CLAVE, ANCHO_CAJA, a.foco == 1, true, 20);

    bool login = ui::boton(g, e, 53, X_BOTONES, 0x183) || enter_clave;
    ui::boton(g, e, 65, X_BOTONES, 0x195);  // nueva cuenta: L1J las crea solas al primer login
    ui::boton(g, e, 55, X_BOTONES, 0x1a8);  // opciones (entregable 10)
    if (ui::boton(g, e, 57, X_BOTONES, 0x1c2)) a.salir = true;
    if (e.escape && !a.mostrando_noticias) a.salir = true;
    if ((login || (a.aut.activa && a.frames_en_pantalla == 5)) && !bloqueado) iniciar_login(a);

    if (a.esperando && !a.mostrando_noticias) g.texto("Conectando...", 0x1b8, 0x128, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 230, 150));
    if (!a.aviso.empty()) g.texto_sombra(a.aviso, 0x1a9, 0x128, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 120, 100));

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
