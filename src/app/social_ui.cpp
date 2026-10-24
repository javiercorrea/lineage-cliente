// Entregable 9: social.
//  - Preguntas de sí o no del servidor (S_MessageYN: intercambio, grupo, entrar al clan...): un
//    cuadro con el mensaje de string-e.tbl y Yes / No (Enter / Escape); la respuesta es C_Attr.
//  - Intercambio: S_Trade abre la ventana. Arriba lo que ofrece uno (arrastrando ítems desde el
//    inventario, o doble clic en el inventario), abajo lo que ofrece el otro; OK (C_TradeOK) y
//    Cancel. S_TradeStatus la cierra (terminado o cancelado).
//  - Ventana Social (botón del apretón de manos o "Social"): grupo, intercambio y clan, y los
//    comandos de chat que hacen lo mismo (/invite, /trade, /pledge...).
//  - Nombres: el título arriba, el emblema del clan a la izquierda y el color según el alineamiento
//    (ver dibujar_personajes).
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr int ANCHO_PREGUNTA = 300, ALTO_PREGUNTA = 92;
constexpr int ANCHO_INTERCAMBIO = ui::VENTANA_ANCHO, ALTO_INTERCAMBIO = 300;
constexpr int CELDA = 36, COLUMNAS = 6, FILAS = 3;
constexpr int ANCHO_SOCIAL = 220, ALTO_SOCIAL = 230;

bool igual_sin_mayusculas(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if ((a[i] | 0x20) != (b[i] | 0x20)) return false;
    return true;
}

// Objeto a la vista con ese nombre (personajes: el nombre tal cual, sin "$N").
ObjId buscar_por_nombre(const App& a, std::string_view nombre) {
    const TablaObjetos& o = a.mundo.objetos;
    for (Slot s = 0; s < o.tamano(); ++s)
        if (igual_sin_mayusculas(texto(a.mundo, o.col<O_NOMBRE>()[s]), nombre)) return o.ids()[s];
    return 0;
}

void aviso(App& a, std::string_view t) { a.chat.agregar(t, rgba(255, 230, 120), CHAT_SISTEMA); }

void responder_pregunta(App& a, bool si) {
    if (a.preguntas.empty()) return;
    const Pregunta& p = a.preguntas.front();
    enviar(a, cmd::responder(a.op, p.numero, p.tipo, si));
    a.preguntas.erase(a.preguntas.begin());
}

// Recuadro con título para las ventanas de este módulo; true al cerrar.
bool marco(App& a, Rect r, std::string_view titulo, bool con_cierre = true) {
    Graficos& g = *a.g;
    const u8 op = opacidad_ventanas(a);
    g.gpu().rectangulo(r.x, r.y, r.w, r.h, clave_orden(CAPA_UI, 10), rgba(10, 8, 6, op));
    g.gpu().rectangulo(r.x + 1, r.y + 1, r.w - 2, ui::ALTO_TITULO - 2, clave_orden(CAPA_UI, 11),
                       rgba(40, 32, 22, static_cast<u8>(std::min(255, op + 60))));
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    g.texto(titulo, static_cast<float>(r.x + 8), static_cast<float>(r.y + 6), kt, rgba(214, 190, 140));
    if (!con_cierre) return false;
    g.texto("[x]", static_cast<float>(r.x + r.w - 26), static_cast<float>(r.y + 6), kt, rgba(255, 255, 254));
    return a.entrada.clic && ui::sobre(a.entrada, r.x + r.w - 28, r.y + 2, 26, 20);
}

// Grilla de ítems ofrecidos en el intercambio; devuelve el índice bajo el puntero o -1.
int grilla(App& a, const std::vector<ItemIntercambio>& items, int x0, int y0) {
    Graficos& g = *a.g;
    int bajo = -1;
    for (int c = 0; c < COLUMNAS * FILAS; ++c) {
        const int x = x0 + (c % COLUMNAS) * CELDA, y = y0 + (c / COLUMNAS) * CELDA;
        const bool encima = ui::sobre(a.entrada, x, y, CELDA - 2, CELDA - 2);
        g.gpu().rectangulo(x, y, CELDA - 2, CELDA - 2, clave_orden(CAPA_UI, 12), encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        if (c >= static_cast<int>(items.size())) continue;
        g.gpu().dibujar(g.icono(items[c].gfx), x + 1, y + 1, clave_orden(CAPA_UI, 13));
        if (encima) bajo = c;
    }
    return bajo;
}

}  // namespace

void eventos_social(App& a, const Evento& e) {
    switch (e.tipo) {
    case Ev::PREGUNTA: {
        Pregunta p;
        p.numero = static_cast<u32>(e.v[PR_NUMERO]);
        p.tipo = static_cast<u16>(e.v[PR_TIPO]);
        // El texto, ya armado: string-e.tbl con los argumentos (como S_ServerMessage).
        Evento m = e;
        m.v[MS_ID] = e.v[PR_TIPO];
        m.v[MS_NARGS] = !e.s[0].empty() + !e.s[1].empty();
        char buf[256];
        u32 color = 0;
        size_t n = formatear_mensaje(a.mensajes, a.desc, m, buf, sizeof buf, color);
        p.texto.assign(buf, n);
        a.preguntas.push_back(std::move(p));
        if (a.aut.aceptar) responder_pregunta(a, true);  // prueba automática
        break;
    }
    case Ev::INTERCAMBIO_ABRIR:
        a.intercambio.abierto = true;
        a.intercambio.con = e.s[0];
        a.intercambio.propio.clear();
        a.intercambio.otro.clear();
        a.intercambio.ok = false;
        a.ventana_inventario = true;  // de ahí se arrastran los ítems
        break;
    case Ev::INTERCAMBIO_ITEM:
        (e.v[IN_LADO] == 0 ? a.intercambio.propio : a.intercambio.otro)
            .push_back({static_cast<u16>(e.v[IN_GFX]), static_cast<u8>(e.v[IN_BENDICION]), std::string(e.s[0])});
        break;
    case Ev::EMBLEMA: a.g->poner_emblema(e.id, {reinterpret_cast<const u8*>(e.s[0].data()), e.s[0].size()}); break;
    case Ev::INTERCAMBIO_FIN:
        aviso(a, e.v[IN_CANCELADO] ? "The trade was cancelled." : "The trade is complete.");
        a.intercambio = {};
        break;
    default: break;
    }
}

void ofrecer_en_intercambio(App& a, ObjId item) {
    if (!a.intercambio.abierto || a.intercambio.ok) return;
    Slot s = a.mundo.inventario.items.buscar(item);
    if (s == SIN_SLOT) return;
    enviar(a, cmd::ofrecer_item(a.op, item, a.mundo.inventario.items.col<I_CANTIDAD>()[s]));
}

bool comando_social(App& a, std::string_view linea) {
    if (linea.empty() || linea[0] != '/') return false;
    std::string_view resto = linea.substr(1);
    std::string_view orden = resto.substr(0, resto.find(' '));
    std::string_view arg = orden.size() < resto.size() ? resto.substr(orden.size() + 1) : std::string_view{};
    while (!arg.empty() && arg.front() == ' ') arg.remove_prefix(1);
    if (comando_marcador(a, orden, arg)) return true;
    if (igual_sin_mayusculas(orden, "invite")) {
        ObjId id = buscar_por_nombre(a, arg);
        if (id) enviar(a, cmd::invitar_grupo(a.op, id));
        else aviso(a, "/invite: that character is not in sight.");
    } else if (igual_sin_mayusculas(orden, "party")) {
        enviar(a, cmd::ver_grupo(a.op));
    } else if (igual_sin_mayusculas(orden, "leave")) {
        enviar(a, cmd::dejar_grupo(a.op));
    } else if (igual_sin_mayusculas(orden, "kick")) {
        enviar(a, cmd::echar_del_grupo(a.op, arg));
    } else if (igual_sin_mayusculas(orden, "trade")) {
        enviar(a, cmd::pedir_intercambio(a.op));
    } else if (igual_sin_mayusculas(orden, "pledge")) {
        enviar(a, cmd::ver_clan(a.op));
    } else if (igual_sin_mayusculas(orden, "join")) {
        enviar(a, cmd::entrar_clan(a.op));
    } else if (igual_sin_mayusculas(orden, "leaveclan")) {
        enviar(a, cmd::dejar_clan(a.op));
    } else if (igual_sin_mayusculas(orden, "clan")) {
        if (arg.empty()) aviso(a, "/clan <name>: create a blood pledge (princes; 30000 adena).");
        else enviar(a, cmd::crear_clan(a.op, arg));
    } else if (igual_sin_mayusculas(orden, "banclan")) {
        enviar(a, cmd::echar_del_clan(a.op, arg));
    } else if (igual_sin_mayusculas(orden, "title")) {
        // /title <título> (el propio) o /title <personaje> <título> (príncipe a su clan).
        std::string_view primero = arg.substr(0, arg.find(' '));
        bool otro = primero.size() < arg.size() && buscar_por_nombre(a, primero);
        std::string_view quien = otro ? primero : nombre_personaje(a);
        std::string_view titulo = otro ? arg.substr(primero.size() + 1) : arg;
        enviar(a, cmd::poner_titulo(a.op, quien, titulo));
    } else if (igual_sin_mayusculas(orden, "emblem")) {
        // El original lo carga de un archivo; aquí, uno de muestra: una cruz dorada sobre azul.
        u8 datos[BYTES_EMBLEMA];
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < 16; ++x) {
                bool cruz = (x >= 7 && x <= 8) || (y >= 5 && y <= 6);
                u16 c = cruz ? static_cast<u16>((31 << 10) | (25 << 5) | 8) : static_cast<u16>((4 << 10) | (8 << 5) | 24);
                datos[(y * 16 + x) * 2] = static_cast<u8>(c);
                datos[(y * 16 + x) * 2 + 1] = static_cast<u8>(c >> 8);
            }
        enviar(a, cmd::subir_emblema(a.op, datos));
        a.emblemas_pedidos.clear();  // se vuelven a pedir: el del propio clan cambió
    } else if (igual_sin_mayusculas(orden, "who")) {
        enviar(a, cmd::quien(a.op, arg));
    } else {
        return false;  // no es un comando del cliente: se manda como chat
    }
    return true;
}

void dibujar_pregunta(App& a) {
    if (a.preguntas.empty()) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const Rect r{(a.gpu.ancho() - ANCHO_PREGUNTA) / 2, a.gpu.alto() / 4, ANCHO_PREGUNTA, ALTO_PREGUNTA};
    marco(a, r, "Question", false);
    // El mensaje en hasta 2 líneas.
    const std::string& t = a.preguntas.front().texto;
    char lineas[2][128];
    size_t n[2] = {0, 0};
    int linea = 0;
    for (size_t i = 0; i < t.size() && linea < 2;) {
        size_t fin = t.find(' ', i);
        std::string_view palabra(t.data() + i, (fin == std::string::npos ? t.size() : fin) - i);
        if (n[linea] && ancho_texto(lineas[linea], n[linea]) + ancho_texto(palabra.data(), palabra.size()) + 4 > r.w - 20) ++linea;
        if (linea >= 2) break;
        if (n[linea] && n[linea] < 126) lineas[linea][n[linea]++] = ' ';
        size_t m = std::min(palabra.size(), sizeof lineas[0] - 1 - n[linea]);
        std::memcpy(lineas[linea] + n[linea], palabra.data(), m);
        n[linea] += m;
        i = fin == std::string::npos ? t.size() : fin + 1;
    }
    for (int k = 0; k < 2; ++k)
        g.texto({lineas[k], n[k]}, static_cast<float>(r.x + 10), static_cast<float>(r.y + 28 + k * 15),
                clave_orden(CAPA_UI_TEXTO, 10), rgba(255, 255, 254));
    bool si = ui::boton_texto(g, e, "Yes", r.x + r.w / 2 - 90, r.y + r.h - 26, 80, 18) || e.enter;
    bool no = ui::boton_texto(g, e, "No", r.x + r.w / 2 + 10, r.y + r.h - 26, 80, 18) || e.escape;
    if (si || no) {
        responder_pregunta(a, si);
        e.enter = e.escape = false;  // no van al chat
    }
}

void dibujar_intercambio(App& a) {
    if (!a.intercambio.abierto) return;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::INTERCAMBIO);
    const Rect r = rect_ventana(a, IdVentana::INTERCAMBIO);
    char titulo[96];
    std::snprintf(titulo, sizeof titulo, "Trade with %s", a.intercambio.con.c_str());
    if (marco(a, r, titulo)) enviar(a, cmd::cancelar_intercambio(a.op));
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12);
    const int x0 = r.x + (r.w - COLUMNAS * CELDA) / 2;
    g.texto("You offer (drag items here)", static_cast<float>(x0), static_cast<float>(r.y + 28), kt, rgba(214, 190, 140));
    int bajo_propio = grilla(a, a.intercambio.propio, x0, r.y + 42);
    std::snprintf(titulo, sizeof titulo, "%s offers", a.intercambio.con.c_str());
    g.texto(titulo, static_cast<float>(x0), static_cast<float>(r.y + 42 + FILAS * CELDA + 6), kt, rgba(214, 190, 140));
    int bajo_otro = grilla(a, a.intercambio.otro, x0, r.y + 56 + FILAS * CELDA + 6);
    // Nombre del ítem bajo el puntero.
    const ItemIntercambio* it = bajo_propio >= 0 ? &a.intercambio.propio[bajo_propio]
                                : bajo_otro >= 0 ? &a.intercambio.otro[bajo_otro] : nullptr;
    if (it) {
        char buf[256];
        size_t m = expandir_desc(a.desc, it->nombre, buf, sizeof buf);
        int w = ancho_texto(buf, m);
        g.gpu().rectangulo(e.mx + 10, e.my + 14, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto({buf, m}, static_cast<float>(e.mx + 14), static_cast<float>(e.my + 17), clave_orden(CAPA_UI_TEXTO, 30));
    }
    const int yb = r.y + r.h - 26;
    if (a.intercambio.ok) {
        g.texto("Waiting for the other side...", static_cast<float>(r.x + 12), static_cast<float>(yb + 3), kt, rgba(170, 230, 255));
    } else if (ui::boton_texto(g, e, "OK", r.x + 12, yb, 90, 18)) {
        enviar(a, cmd::aceptar_intercambio(a.op));
        a.intercambio.ok = true;
    }
    if (ui::boton_texto(g, e, "Cancel", r.x + r.w - 102, yb, 90, 18)) enviar(a, cmd::cancelar_intercambio(a.op));
}

void dibujar_social(App& a) {
    if (!a.ventana_social) return;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::SOCIAL);
    const Rect r = rect_ventana(a, IdVentana::SOCIAL);
    if (marco(a, r, "Social") || e.escape) {
        a.ventana_social = false;
        return;
    }
    struct Boton {
        std::string_view texto;
        std::string_view comando;
    } botones[] = {
        {"Party members", "/party"},       {"Leave party", "/leave"},
        {"Trade (face to face)", "/trade"}, {"Clan members", "/pledge"},
        {"Join clan (face prince)", "/join"}, {"Leave clan", "/leaveclan"},
    };
    int y = r.y + ui::ALTO_TITULO + 6;
    for (const Boton& b : botones) {
        if (ui::boton_texto(g, e, b.texto, r.x + 10, y, r.w - 20, 18)) comando_social(a, b.comando);
        y += 22;
    }
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    const char* ayuda[] = {"/invite <name>  /kick <name>", "/clan <name>  /title <text>", "/who <name>"};
    for (const char* t : ayuda) {
        g.texto(t, static_cast<float>(r.x + 10), static_cast<float>(y + 4), kt, rgba(170, 160, 140));
        y += 15;
    }
}

void procesar_social(App& a, u64 ahora) {
    Automatizacion& au = a.aut;
    // Prueba automática: mirar hacia un lado (para quedar frente a frente con otro personaje).
    if (au.girar >= 0 && a.frames_en_pantalla == 200) {
        Slot s = a.mundo.objetos.buscar(a.mundo.jugador.id);
        if (s != SIN_SLOT) a.mundo.objetos.col<O_DIR>()[s] = static_cast<u8>(au.girar);
        enviar(a, cmd::girar(a.op, static_cast<u8>(au.girar)));
    }
    if (!a.intercambio.abierto) return;
    if (!au.ofrecer.empty() && !au.ofrecido) {  // ofrecer y, un poco después, aceptar
        const TablaItems& t = a.mundo.inventario.items;
        char buf[256];
        for (Slot s = 0; s < t.tamano(); ++s) {
            size_t n = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), buf, sizeof buf);
            if (std::string_view(buf, n).find(au.ofrecer) == std::string_view::npos) continue;
            ofrecer_en_intercambio(a, t.ids()[s]);
            break;
        }
        au.ofrecido = true;
        au.t_intercambio = ahora;
    }
    // Quien ofrece acepta 1,5 s después; quien solo acepta (--aceptar), cuando ya ve lo ofrecido.
    bool listo = au.ofrecido ? ahora > au.t_intercambio + 1500 : (au.aceptar && !a.intercambio.otro.empty());
    if (listo && !a.intercambio.ok) {
        enviar(a, cmd::aceptar_intercambio(a.op));
        a.intercambio.ok = true;
    }
}

Rect rect_social(const App& a) { return {(a.gpu.ancho() - ANCHO_SOCIAL) / 2 - 140, ui::VENTANA_Y + 60, ANCHO_SOCIAL, ALTO_SOCIAL}; }
Rect rect_intercambio(const App& a) {
    return {(a.gpu.ancho() - ANCHO_INTERCAMBIO) / 2, ui::VENTANA_Y + 20, ANCHO_INTERCAMBIO, ALTO_INTERCAMBIO};
}

}  // namespace lin
