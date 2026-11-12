// Control en vivo para pruebas (solo con LIN_CONTROL; la versión final no lo compila).
//
// Un socket de archivo (Unix domain socket) en $LIN_CONTROL o $TMPDIR/lineage-control-<pid>.sock,
// solo para el usuario. Cada línea es un comando; la respuesta son líneas que terminan con "ok" o
// "error <motivo>". Se revisa una vez por frame en el hilo principal (sin hilos ni locks): los
// comandos llaman a las mismas funciones que la interfaz. Los que esperan algo ("esperar", "login",
// "personaje", "captura") dejan la conexión en espera y responden en el frame en que se cumple o
// vence el plazo, sin frenar el juego. "esperar paquete=" y "esperar chat=" cuentan también lo que
// llegó desde que empezó el comando anterior de esa conexión (la respuesta a un "usar" puede llegar
// antes del "esperar" que lo sigue).
//
// Comandos: "ayuda" (AYUDA) y docs/analisis/PLAN_CONTROL_VIVO.md. Herramienta: herramientas/lin_ctl.py.
// Los textos van en CP949, como los del juego (nombres coreanos incluidos); ASCII es igual.
// MULTIPLATAFORMA: AF_UNIX existe en Windows 10 1803+ (afunix.h, closesocket, ioctlsocket); falta.
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include "app/app.h"

namespace lin {

namespace {

constexpr int MAX_CONEXIONES = 4;
constexpr u64 PLAZO_MS = 10000;

const char* nombre_pantalla(Pantalla p) {
    switch (p) {
    case Pantalla::ELEGIR_CARPETA: return "carpeta";
    case Pantalla::LOGIN: return "login";
    case Pantalla::PERSONAJES: return "personajes";
    case Pantalla::CREAR: return "crear";
    case Pantalla::CARGANDO: return "cargando";
    case Pantalla::JUEGO: return "juego";
    }
    return "?";
}

// Lo que una conexión espera para responder.
struct Espera {
    enum Tipo : u8 { NADA, PANTALLA, PAQUETE, HTML, LOGIN, PERSONAJE, FRAMES, MS, VENTANA, CHAT, CREAR } tipo = NADA;
    Pantalla pantalla = Pantalla::LOGIN;
    Srv paquete = Srv::DESCONOCIDO;
    std::string texto;
    bool visto = false;  // el paquete llegó
    u32 chat_desde = 0;  // líneas de chat que ya había al empezar a esperar
    u64 vence_ms = 0, frame = 0;
};

struct Conexion {
    int fd = -1;
    std::string entrada, salida;
    Espera espera;
    // Paquetes recibidos y líneas de chat al empezar el comando anterior y el actual: "esperar
    // paquete=" y "esperar chat=" cuentan también lo que llegó desde el comando anterior (la
    // respuesta a un "usar" puede llegar antes de que empiece el "esperar" que la sigue).
    u32 paquetes_antes = 0, paquetes_ahora = 0, chat_antes = 0, chat_ahora = 0;
};

// Un paso de mouse inyectado: se aplica en un frame (después de leer la entrada real).
struct PasoMouse {
    int x, y;
    u8 boton;  // 0 izquierdo, 1 derecho, 2 medio
    bool apretar, soltar;
    int rueda = 0;  // pasos de la rueda en ese frame (+ hacia arriba)
};

struct Control {
    int escucha = -1;
    std::string ruta;
    std::vector<Conexion> conexiones;
    std::deque<PasoMouse> mouse;
    std::deque<std::string> textos;  // un SDL_EVENT_TEXT_INPUT por frame
    std::string texto_actual;        // el que SDL está por leer (tiene que seguir vivo)
    bool ctrl = false;               // "ctrl on": Ctrl mantenido (forzar ataque)
    bool shift = false;              // "shift on": Shift mantenido (Shift+clic gira)
    static constexpr u32 RECIENTES = 256;
    Srv recientes[RECIENTES] = {};   // los últimos paquetes recibidos ("paquetes", "esperar paquete=")
    u32 n_recientes = 0;
};
Control g;

void responder(Conexion& c, std::string_view linea) {
    c.salida += linea;
    c.salida += '\n';
}

// Palabras de la línea; "entre comillas" es una sola.
std::vector<std::string> palabras(std::string_view l) {
    std::vector<std::string> v;
    size_t i = 0;
    while (i < l.size()) {
        while (i < l.size() && l[i] == ' ') ++i;
        if (i >= l.size()) break;
        // Hasta el próximo espacio fuera de comillas; las comillas se quitan ("a b" y clave="a b").
        std::string w;
        bool comillas = false;
        for (; i < l.size() && (comillas || l[i] != ' '); ++i) {
            if (l[i] == '"') comillas = !comillas;
            else w += l[i];
        }
        v.push_back(std::move(w));
    }
    return v;
}

// "clave=valor" de la lista; vacío si no está.
std::string_view opcion(const std::vector<std::string>& p, std::string_view clave) {
    for (const std::string& s : p)
        if (s.size() > clave.size() && s.compare(0, clave.size(), clave) == 0 && s[clave.size()] == '=')
            return std::string_view(s).substr(clave.size() + 1);
    return {};
}

void clic(int x, int y, u8 boton) {
    g.mouse.push_back({x, y, boton, true, false});
    g.mouse.push_back({x, y, boton, false, true});
}

// Una tecla por su nombre de SDL ("Return", "Escape", "F5", "A"...), con "ctrl+", "alt+", "shift+".
bool tecla(std::string_view nombre) {
    SDL_Keymod mod = 0;
    static constexpr struct {
        std::string_view prefijo;
        SDL_Keymod mod;
    } MODS[] = {{"ctrl+", SDL_KMOD_CTRL}, {"alt+", SDL_KMOD_ALT}, {"shift+", SDL_KMOD_SHIFT}};
    for (bool otro = true; otro;) {
        otro = false;
        for (const auto& m : MODS)
            if (nombre.starts_with(m.prefijo)) {
                mod |= m.mod;
                nombre.remove_prefix(m.prefijo.size());
                otro = true;
            }
    }
    const std::string n(nombre == "enter" ? "Return" : nombre == "esc" ? "Escape" : nombre);
    const SDL_Keycode k = SDL_GetKeyFromName(n.c_str());
    if (k == SDLK_UNKNOWN) return false;
    SDL_Event ev{};
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.key = k;
    ev.key.mod = mod;
    SDL_PushEvent(&ev);
    return true;
}

// Objeto a la vista: el id (solo dígitos), "#gfx" o el más cercano cuyo nombre lo contenga.
Slot buscar_objeto(const App& a, std::string_view q, bool solo_npc = false) {
    const TablaObjetos& o = a.mundo.objetos;
    const Slot yo = o.buscar(a.mundo.jugador.id);
    if (yo == SIN_SLOT || q.empty()) return SIN_SLOT;
    if (q.find_first_not_of("0123456789") == std::string_view::npos) return o.buscar(static_cast<ObjId>(std::strtoul(std::string(q).c_str(), nullptr, 10)));
    const int gfx = q[0] == '#' ? std::atoi(std::string(q.substr(1)).c_str()) : -1;
    Slot mejor = SIN_SLOT;
    int mejor_d = 1 << 30;
    for (Slot s = 0; s < o.tamano(); ++s) {
        if (s == yo || (solo_npc && !es_npc(a, s))) continue;
        if (gfx >= 0 ? o.col<O_GFX>()[s] != gfx : a.desc.resolver(texto(a.mundo, o.col<O_NOMBRE>()[s])).find(q) == std::string_view::npos)
            continue;
        const int d = std::max(std::abs(o.col<O_X>()[s] - o.col<O_X>()[yo]), std::abs(o.col<O_Y>()[s] - o.col<O_Y>()[yo]));
        if (d < mejor_d) mejor_d = d, mejor = s;
    }
    return mejor;
}

// Las ventanas que se abren con "abrir"/"cerrar" y se esperan con "esperar ventana=".
bool* ventana(App& a, std::string_view n) {
    if (n == "inventario") return &a.ventana_inventario;
    if (n == "estado") return &a.ventana_estado;
    if (n == "hechizos") return &a.ventana_hechizos;
    if (n == "opciones") return &a.ventana_opciones;
    if (n == "forma") return &a.ventana_forma;
    if (n == "social") return &a.ventana_social;
    if (n == "salida") return &a.ventana_salida;
    if (n == "teclas") return &a.ayuda_teclas;
    return nullptr;
}
// Las que se abren solas (con su nombre para "ventanas" y "esperar ventana=").
bool abierta(const App& a, std::string_view n) {
    if (n == "npc") return a.npc != 0;
    if (n == "tienda") return a.tienda != TipoTienda::NINGUNA;
    if (n == "tablon") return a.tablon.id != 0;
    if (n == "correo") return a.correo.abierto;
    if (n == "subastas") return a.subastas.tablon != 0;
    if (n == "mapa-casa") return a.subastas.mapa != 0;
    if (n == "intercambio") return a.intercambio.abierto;
    if (n == "pregunta") return !a.preguntas.empty();
    if (n == "cantidad") return a.cantidad.abierto;
    if (n == "menu-servidores") return a.menu_servidores;
    if (bool* v = ventana(const_cast<App&>(a), n)) return *v;
    return false;
}
constexpr const char* VENTANAS[] = {"inventario", "estado", "hechizos", "opciones", "social", "salida", "teclas", "npc", "tienda",
                                    "tablon", "correo", "subastas", "mapa-casa", "intercambio", "pregunta", "cantidad",
                                    "menu-servidores"};

void objetos(App& a, Conexion& c, int radio) {
    const TablaObjetos& o = a.mundo.objetos;
    const Slot yo = o.buscar(a.mundo.jugador.id);
    if (yo == SIN_SLOT) return;
    char l[320];
    for (Slot s = 0; s < o.tamano(); ++s) {
        const int x = o.col<O_X>()[s], y = o.col<O_Y>()[s];
        if (std::max(std::abs(x - o.col<O_X>()[yo]), std::abs(y - o.col<O_Y>()[yo])) > radio) continue;
        const ObjId id = o.ids()[s];
        const std::string_view n = a.desc.resolver(texto(a.mundo, o.col<O_NOMBRE>()[s]));
        int px = -1, py = -1;  // centro en pantalla (coordenadas de "clic"), si se dibujó
        const char* sprite = "fuera";
        for (const App::ObjetoEnPantalla& r : a.en_pantalla)
            if (r.id == id) {
                px = static_cast<int>((r.x0 + r.x1) / 2), py = static_cast<int>((r.y0 + r.y1) / 2);
                sprite = r.sprite == 0 ? "ok" : r.sprite == 1 ? "pendiente" : "falta";
            }
        // Para diagnosticar un bloque de color: el archivo que se busca y su acción de estar quieto.
        const GfxId gfx = o.col<O_GFX>()[s], alias = a.sprites.gfx_archivos(gfx);
        char spr0[32];
        std::snprintf(spr0, sizeof spr0, "%u-0.spr", alias);
        std::snprintf(l, sizeof l, "id=%u nombre=\"%.*s\" gfx=%u tipo=%u x=%d y=%d sprite=%s alias=%u tabla=%d archivo=%zu estado=%u pantalla=%d,%d%s", id, static_cast<int>(n.size()), n.data(),
                      static_cast<unsigned>(gfx), static_cast<unsigned>(tipo_objeto(a, s)), x, y, sprite, static_cast<unsigned>(alias),
                      a.sprites.accion(gfx, ACC_QUIETO) || a.sprites.accion(gfx, ACC_CAMINAR) ? 1 : 0, a.rec.sprite(spr0).size(),
                      static_cast<unsigned>(o.col<O_ESTADO>()[s]), px, py,
                      o.col<O_ESTADO>()[s] == ESTADO_MUERTO ? " muerto" : s == yo ? " yo" : "");
        responder(c, l);
    }
}

void inventario(App& a, Conexion& c) {
    const Inventario& inv = a.mundo.inventario;
    const TablaItems& t = inv.items;
    char nombre[256], l[320];
    for (Slot s = 0; s < t.tamano(); ++s) {
        const size_t n = expandir_desc(a.desc, nombre_item(inv, s), nombre, sizeof nombre);
        std::snprintf(l, sizeof l, "id=%u nombre=\"%.*s\" cantidad=%u gfx=%u%s", t.ids()[s], static_cast<int>(n), nombre,
                      t.col<I_CANTIDAD>()[s], static_cast<unsigned>(t.col<I_GFX>()[s]), item_equipado(nombre_item(inv, s)) ? " equipado" : "");
        responder(c, l);
    }
}

void html(App& a, Conexion& c) {
    if (!a.npc) return;
    responder(c, "pagina=" + a.npc_pagina + " npc=" + std::to_string(a.npc));
    for (const ElementoHtml& el : a.npc_html.elementos) {
        const std::string_view v = a.npc_html.vinculo(el);
        if (v.empty()) continue;
        const std::string_view t(a.npc_html.textos.data() + el.texto_ofs, el.texto_len);
        responder(c, std::string(el.es_enlace ? "enlace=" : "accion=") + std::string(v) + " texto=\"" + std::string(t) + "\"");
    }
}

void estado(App& a, Conexion& c) {
    char l[256];
    const Servidor& sv = servidor_elegido(a.cfg);
    std::snprintf(l, sizeof l, "pantalla=%s servidor=\"%s\" frame=%llu vista=%dx%d ui=%s", nombre_pantalla(a.pantalla),
                  sv.nombre.c_str(), static_cast<unsigned long long>(a.frames_en_pantalla), a.gpu.ancho(), a.gpu.alto(),
                  a.cfg.ui_simple ? "simple" : "original");
    responder(c, l);
    if (a.pantalla != Pantalla::JUEGO) {
        if (!a.aviso.empty()) responder(c, "aviso=\"" + a.aviso + "\"");
        return;
    }
    const EstadoJugador& j = a.mundo.jugador;
    const Slot s = a.mundo.objetos.buscar(j.id);
    const int x = s != SIN_SLOT ? a.mundo.objetos.col<O_X>()[s] : 0, y = s != SIN_SLOT ? a.mundo.objetos.col<O_Y>()[s] : 0;
    const std::string_view nombre = nombre_personaje(a);
    std::snprintf(l, sizeof l, "personaje=%.*s mapa=%u x=%d y=%d nivel=%d hp=%d/%d mp=%d/%d muerto=%d estado=%d",
                  static_cast<int>(nombre.size()), nombre.data(), static_cast<unsigned>(a.mundo.mapa), x, y, j.nivel, j.hp,
                  j.hp_max, j.mp, j.mp_max, j.muerto ? 1 : 0, s != SIN_SLOT ? a.mundo.objetos.col<O_ESTADO>()[s] : -1);
    responder(c, l);
    std::string asedios = "asedios=";
    for (int k = 1; k <= 8; ++k)
        if (a.castillos.asedio[k]) (asedios += std::to_string(k)) += ',';
    if (u8 cs = castillo_de(a, j.id)) asedios += " senor=" + std::to_string(cs);
    responder(c, asedios);
}

const char* AYUDA[] = {
    "consultas: estado | objetos [radio] | inventario | html | ventanas | grupo | chat [n] | paquetes [n]",
    "login <cuenta> <clave> [servidor]   (espera la lista de personajes)",
    "personaje <nombre>                  (espera estar en el juego)",
    "crear <nombre> <clase 0-6>          (en la selección; espera la respuesta del servidor)",
    "decir \"texto\"                       (como escrito en el chat; también /comandos)",
    "objeto = id | #gfx | parte del nombre (el más cercano)",
    "hablar <objeto>   atacar <objeto>   parar   clic-objeto <objeto> [der]",
    "accion <vínculo de la página>   responder si|no|\"texto\"   cantidad <n>",
    "usar <ítem> [sobre <ítem>]   soltar <ítem>   dar <ítem> <objeto>   borrar <ítem>   lanzar <hechizo> [objeto]",
    "abrir|cerrar inventario|estado|hechizos|opciones|social|salida|teclas",
    "girar <0-7>   ofrecer <ítem> (intercambio)   aceptar-intercambio   cancelar-intercambio",
    "clic <x> <y> [der|medio]   arrastrar <x1> <y1> <x2> <y2>   mantener <x> <y> <frames>   raton <x> <y> down|up",
    "rueda <x> <y> <pasos>   barra-pagina [n]   (página visible de la barra de atajos; sin n, la dice)",
    "tecla <nombre> (ctrl+ alt+ shift+)   texto \"...\"   ctrl on|off   shift on|off",
    "esperar pantalla=X | paquete=NOMBRE | html=X | ventana=X | dialogo=pregunta|cantidad | chat=texto | frames=n | ms=n  [plazo=ms]",
    "captura <ruta.bmp>   mudo on|off   traza on|off   traza-html on|off   perf on|off   depuracion on|off   salir",
    "forzar-hora <segundos del día 0-86399>|off (luz y sonido, para pruebas; el día del año pasa a ser el 1 de enero)",
};

// Ejecuta una línea. Devuelve false si queda esperando (responde después).
bool ejecutar(App& a, Conexion& c, std::string_view linea) {
    const std::vector<std::string> p = palabras(linea);
    if (p.empty()) return true;
    c.paquetes_antes = c.paquetes_ahora;
    c.chat_antes = c.chat_ahora;
    c.paquetes_ahora = g.n_recientes;
    c.chat_ahora = a.chat.total;
    const std::string& cmd = p[0];
    const u64 ahora = a.entrada.ahora_ms;
    Espera& es = c.espera;
    es = {};
    const std::string_view plazo = opcion(p, "plazo");
    es.vence_ms = ahora + (plazo.empty() ? PLAZO_MS : std::strtoull(std::string(plazo).c_str(), nullptr, 10));
    es.frame = a.frame;
    auto error = [&](std::string_view m) {
        responder(c, "error " + std::string(m));
        return true;
    };
    if (cmd == "ayuda") {
        for (const char* l : AYUDA) responder(c, l);
    } else if (cmd == "estado") {
        estado(a, c);
    } else if (cmd == "login") {
        if (a.pantalla != Pantalla::LOGIN) return error("no está en el login");
        if (p.size() < 3) return error("uso: login <cuenta> <clave> [servidor]");
        if (p.size() > 3) {
            size_t i = 0;
            while (i < a.cfg.servidores.size() && a.cfg.servidores[i].nombre != p[3]) ++i;
            if (i == a.cfg.servidores.size()) return error("no hay un servidor \"" + p[3] + "\"");
            a.cfg.servidor = i;
        }
        a.usuario = p[1];
        a.clave = p[2];
        iniciar_login(a);
        if (!a.esperando) return error(a.aviso.empty() ? "no se pudo iniciar" : a.aviso);
        es.tipo = Espera::LOGIN;
        return false;
    } else if (cmd == "personaje") {
        if (a.pantalla != Pantalla::PERSONAJES) return error("no está en la selección de personajes");
        if (p.size() < 2) return error("uso: personaje <nombre>");
        int i = 0;
        while (i < static_cast<int>(a.pjs.size()) && a.pjs[i].nombre != p[1]) ++i;
        if (i == static_cast<int>(a.pjs.size())) return error("no hay un personaje \"" + p[1] + "\"");
        a.pj_sel = i;
        entrar_juego(a);
        es.tipo = Espera::PERSONAJE;
        return false;
    } else if (cmd == "crear") {
        if (a.pantalla != Pantalla::PERSONAJES) return error("no está en la selección de personajes");
        if (p.size() < 3) return error("uso: crear <nombre> <clase 0-6>");
        crear_personaje_rapido(a, p[1], std::atoi(p[2].c_str()));
        es.tipo = Espera::CREAR;
        es.texto = p[1];
        return false;
    } else if (cmd == "girar") {
        if (a.pantalla != Pantalla::JUEGO || p.size() < 2) return error("uso: girar <0-7> (en el juego)");
        const u8 d = static_cast<u8>(std::atoi(p[1].c_str()) & 7);
        const Slot s = a.mundo.objetos.buscar(a.mundo.jugador.id);
        if (s != SIN_SLOT) a.mundo.objetos.col<O_DIR>()[s] = d;
        enviar(a, cmd::girar(a.op, d));
    } else if (cmd == "ofrecer") {
        if (!a.intercambio.abierto) return error("no hay un intercambio abierto");
        const ObjId item = p.size() > 1 ? buscar_item(a, p[1]) : 0;
        if (!item) return error("no hay ese ítem");
        ofrecer_en_intercambio(a, item);
    } else if (cmd == "cancelar-intercambio") {
        if (!a.intercambio.abierto) return error("no hay un intercambio abierto");
        enviar(a, cmd::cancelar_intercambio(a.op));
    } else if (cmd == "aceptar-intercambio") {
        if (!a.intercambio.abierto) return error("no hay un intercambio abierto");
        enviar(a, cmd::aceptar_intercambio(a.op));
        a.intercambio.ok = true;
    } else if (cmd == "traza-html") {
        g_traza_html = p.size() < 2 || p[1] == "on";
    } else if (cmd == "forzar-hora") {
        // Pruebas de la luz: fija la hora de juego (segundos del día); "off" vuelve a la del servidor.
        if (p.size() < 2) return error("uso: forzar-hora <segundos del día 0-86399> | off");
        if (p[1] == "off") {
            a.hora_forzada = -1;
        } else {
            char* fin = nullptr;
            const long s = std::strtol(p[1].c_str(), &fin, 10);
            if (fin == p[1].c_str() || *fin || s < 0 || s > 86399) return error("la hora va de 0 a 86399, o off");
            a.hora_forzada = static_cast<i32>(s);
        }
    } else if (cmd == "perf") {
        a.perf = p.size() < 2 || p[1] == "on";
    } else if (cmd == "depuracion") {
        a.depuracion = p.size() < 2 || p[1] == "on";
    } else if (cmd == "decir") {
        if (a.pantalla != Pantalla::JUEGO) return error("no está en el juego");
        if (p.size() < 2) return error("uso: decir \"texto\"");
        enviar_chat(a, p[1]);
    } else if (cmd == "objetos") {
        objetos(a, c, p.size() > 1 ? std::atoi(p[1].c_str()) : 20);
    } else if (cmd == "inventario") {
        inventario(a, c);
    } else if (cmd == "html") {
        html(a, c);
    } else if (cmd == "ventanas") {
        std::string l = "abiertas=";
        for (const char* v : VENTANAS)
            if (abierta(a, v)) (l += v) += ',';
        responder(c, l);
        // Dónde están las que se mueven (para hacer clic adentro): nombre=x,y,ancho,alto.
        static constexpr std::pair<const char*, IdVentana> MOVILES[] = {
            {"estado", IdVentana::ESTADO}, {"inventario", IdVentana::INVENTARIO}, {"hechizos", IdVentana::HECHIZOS},
            {"npc", IdVentana::NPC}, {"opciones", IdVentana::OPCIONES}, {"intercambio", IdVentana::INTERCAMBIO}};
        if (a.pantalla == Pantalla::JUEGO)
            for (const auto& [n, id] : MOVILES)
                if (abierta(a, n)) {
                    const Rect rv = rect_ventana(a, id);
                    responder(c, std::string(n) + '=' + std::to_string(rv.x) + ',' + std::to_string(rv.y) + ',' + std::to_string(rv.w) +
                                     ',' + std::to_string(rv.h));
                }
    } else if (cmd == "grupo") {
        for (const MiembroGrupo& m : a.grupo)
            responder(c, "id=" + std::to_string(m.id) + " nombre=" + m.nombre + " hp=" + std::to_string(m.hp) + (m.lider ? " lider" : ""));
    } else if (cmd == "chat") {
        const u32 n = std::min<u32>(p.size() > 1 ? static_cast<u32>(std::atoi(p[1].c_str())) : 10, a.chat.cantidad());
        for (u32 i = n; i-- > 0;) responder(c, std::string(a.chat.texto(a.chat.reciente(i))));
    } else if (cmd == "paquetes") {
        const u32 n = std::min<u32>({p.size() > 1 ? static_cast<u32>(std::atoi(p[1].c_str())) : 20, g.n_recientes, Control::RECIENTES});
        std::string l;
        for (u32 i = n; i-- > 0;) (l += nombre_srv(g.recientes[(g.n_recientes - 1 - i) % Control::RECIENTES])) += ' ';
        responder(c, l);
    } else if (cmd == "hablar" || cmd == "atacar" || cmd == "clic-objeto") {
        if (a.pantalla != Pantalla::JUEGO) return error("no está en el juego");
        if (p.size() < 2) return error("uso: " + cmd + " <objeto>");
        const Slot s = buscar_objeto(a, p[1], cmd == "hablar");
        if (s == SIN_SLOT) return error("no hay un \"" + p[1] + "\" a la vista");
        const ObjId id = a.mundo.objetos.ids()[s];
        if (cmd == "clic-objeto") {
            const App::ObjetoEnPantalla* r = nullptr;
            for (const App::ObjetoEnPantalla& e : a.en_pantalla)
                if (e.id == id) r = &e;
            if (!r) return error("no se está dibujando");
            clic(static_cast<int>((r->x0 + r->x1) / 2), static_cast<int>((r->y0 + r->y1) / 2), p.size() > 2 && p[2] == "der" ? 1 : 0);
        } else {
            // Como un clic sobre él: se camina hasta el NPC y se le habla, o se lo ataca hasta que muera
            // (el ataque sostenido de la tecla A).
            a.objetivo = id;
            a.objetivo_fijo = true;
            a.hablar_objetivo = cmd == "hablar";
            a.ataque_sostenido = cmd == "atacar";
        }
    } else if (cmd == "parar") {
        a.objetivo = 0;
        a.objetivo_fijo = a.hablar_objetivo = a.ataque_sostenido = false;
    } else if (cmd == "accion") {
        if (!a.npc) return error("no hay una página abierta");
        if (p.size() < 2) return error("uso: accion <vínculo>");
        bool enlace = false;
        for (const ElementoHtml& el : a.npc_html.elementos)
            if (a.npc_html.vinculo(el) == p[1]) enlace = el.es_enlace;
        activar(a, p[1], enlace);
    } else if (cmd == "responder") {
        if (a.preguntas.empty()) return error("no hay una pregunta");
        if (p.size() < 2) return error("uso: responder si|no|\"texto\"");
        if (a.preguntas.front().tipo == PREGUNTA_NOMBRE_CASA) a.preguntas.front().respuesta = p[1];
        responder_pregunta(a, p[1] != "no");
    } else if (cmd == "cantidad") {
        if (!a.cantidad.abierto) return error("no hay un diálogo de cantidad");
        if (p.size() < 2) return error("uso: cantidad <n>");
        aceptar_cantidad(a, std::atoi(p[1].c_str()));
    } else if (cmd == "usar" || cmd == "soltar" || cmd == "dar" || cmd == "borrar") {
        if (a.pantalla != Pantalla::JUEGO) return error("no está en el juego");
        if (p.size() < 2) return error("uso: " + cmd + " <ítem>");
        const ObjId item = buscar_item(a, p[1]);
        if (!item) return error("no hay un ítem \"" + p[1] + "\"");
        if (cmd == "soltar") {
            soltar_item(a, item);
        } else if (cmd == "borrar") {
            borrar_item(a, item);  // abre el diálogo: "cantidad N" lo confirma
        } else if (cmd == "dar") {
            const Slot s = p.size() > 2 ? buscar_objeto(a, p[2]) : SIN_SLOT;
            if (s == SIN_SLOT) return error("uso: dar <ítem> <objeto a la vista>");
            dar_item(a, a.mundo.objetos.ids()[s], item);
        } else if (p.size() > 3 && p[2] == "sobre") {
            const ObjId objetivo = buscar_item(a, p[3]);
            if (!objetivo) return error("no hay un ítem \"" + p[3] + "\"");
            enviar(a, cmd::usar_item_sobre(a.op, item, objetivo));
        } else {
            usar_item(a, item);
        }
    } else if (cmd == "lanzar") {
        if (a.pantalla != Pantalla::JUEGO) return error("no está en el juego");
        if (p.size() < 2) return error("uso: lanzar <hechizo> [objeto]");
        const u32 id = buscar_hechizo(a, p[1]);
        if (!id) return error("no sabe \"" + p[1] + "\"");
        const Slot s = p.size() > 2 ? buscar_objeto(a, p[2]) : SIN_SLOT;
        if (p.size() > 2 && s == SIN_SLOT) return error("no hay un \"" + p[2] + "\" a la vista");
        pedir_hechizo(a, id, s == SIN_SLOT ? 0 : a.mundo.objetos.ids()[s]);
    } else if (cmd == "abrir" || cmd == "cerrar") {
        bool* v = p.size() > 1 ? ventana(a, p[1]) : nullptr;
        if (!v) return error("ventana desconocida");
        *v = cmd == "abrir";
    } else if (cmd == "arrastrar") {
        if (p.size() < 5) return error("uso: arrastrar <x1> <y1> <x2> <y2>");
        const int x1 = std::atoi(p[1].c_str()), y1 = std::atoi(p[2].c_str()), x2 = std::atoi(p[3].c_str()), y2 = std::atoi(p[4].c_str());
        constexpr int PASOS = 20;
        g.mouse.push_back({x1, y1, 0, true, false});
        for (int i = 1; i <= PASOS; ++i)
            g.mouse.push_back({x1 + (x2 - x1) * i / PASOS, y1 + (y2 - y1) * i / PASOS, 0, true, false});
        g.mouse.push_back({x2, y2, 0, false, true});
    } else if (cmd == "mantener") {
        if (p.size() < 4) return error("uso: mantener <x> <y> <frames>");
        const int x = std::atoi(p[1].c_str()), y = std::atoi(p[2].c_str());
        for (int i = std::max(1, std::atoi(p[3].c_str())); i-- > 0;) g.mouse.push_back({x, y, 0, true, false});
        g.mouse.push_back({x, y, 0, false, true});
    } else if (cmd == "raton") {
        // Un evento de SDL en puntos de la ventana (no el del control del juego): para probar la barra.
        if (p.size() < 4 || (p[3] != "down" && p[3] != "up")) return error("uso: raton <x> <y> down|up");
        SDL_Event ev{};
        ev.type = p[3] == "down" ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.down = p[3] == "down";
        ev.button.x = static_cast<float>(std::atoi(p[1].c_str()));
        ev.button.y = static_cast<float>(std::atoi(p[2].c_str()));
        ev.button.windowID = SDL_GetWindowID(a.ventana);
        SDL_PushEvent(&ev);
    } else if (cmd == "rueda") {
        if (p.size() < 4) return error("uso: rueda <x> <y> <pasos>");
        g.mouse.push_back({std::atoi(p[1].c_str()), std::atoi(p[2].c_str()), 3, false, false, std::atoi(p[3].c_str())});
    } else if (cmd == "barra-pagina") {
        if (p.size() > 1) cambiar_pagina_barra(a, std::atoi(p[1].c_str()) - 1);
        responder(c, "pagina=" + std::to_string(a.pagina_barra + 1) + " paginas=" + std::to_string(a.cfg.paginas_barra));
    } else if (cmd == "clic") {
        if (p.size() < 3) return error("uso: clic <x> <y> [der|medio]");
        clic(std::atoi(p[1].c_str()), std::atoi(p[2].c_str()), p.size() > 3 ? (p[3] == "der" ? 1 : p[3] == "medio" ? 2 : 0) : 0);
    } else if (cmd == "tecla") {
        if (p.size() < 2 || !tecla(p[1])) return error("tecla desconocida");
    } else if (cmd == "texto") {
        if (p.size() < 2) return error("uso: texto \"...\"");
        g.textos.push_back(p[1]);
    } else if (cmd == "ctrl") {
        g.ctrl = p.size() > 1 && p[1] == "on";
    } else if (cmd == "shift") {
        g.shift = p.size() > 1 && p[1] == "on";
    } else if (cmd == "mudo") {
        a.mudo = p.size() < 2 || p[1] == "on";
        aplicar_volumenes(a);
    } else if (cmd == "traza") {
        g_traza = p.size() < 2 || p[1] == "on";
    } else if (cmd == "captura") {
        if (p.size() < 2) return error("uso: captura <ruta.bmp>");
        a.gpu.pedir_captura(p[1]);
        es.tipo = Espera::FRAMES;
        es.frame = a.frame + 2;  // se escribe al presentar
        return false;
    } else if (cmd == "esperar") {
        if (std::string_view v = opcion(p, "pantalla"); !v.empty()) {
            es.tipo = Espera::PANTALLA;
            es.texto = v;
        } else if (std::string_view v = opcion(p, "paquete"); !v.empty()) {
            es.tipo = Espera::PAQUETE;
            es.texto = v;
            for (u32 i = std::max(c.paquetes_antes, g.n_recientes - std::min(g.n_recientes, Control::RECIENTES)); i < g.n_recientes; ++i)
                es.visto |= es.texto == nombre_srv(g.recientes[i % Control::RECIENTES]);
        } else if (std::string_view v = opcion(p, "html"); !v.empty()) {
            es.tipo = Espera::HTML;
            es.texto = v;
        } else if (std::string_view v = opcion(p, "ventana"); !v.empty()) {
            es.tipo = Espera::VENTANA;
            es.texto = v;
        } else if (std::string_view v = opcion(p, "dialogo"); !v.empty()) {
            es.tipo = Espera::VENTANA;  // "pregunta" o "cantidad": son ventanas que se abren solas
            es.texto = v;
        } else if (std::string_view v = opcion(p, "chat"); !v.empty()) {
            es.tipo = Espera::CHAT;
            es.texto = v;
            es.chat_desde = c.chat_antes;
        } else if (std::string_view v = opcion(p, "ms"); !v.empty()) {
            es.tipo = Espera::MS;  // tiempo real (los frames dependen de los fps)
            es.frame = ahora + std::strtoull(std::string(v).c_str(), nullptr, 10);
            es.vence_ms = std::max(es.vence_ms, es.frame + 1000);
        } else if (std::string_view v = opcion(p, "frames"); !v.empty()) {
            es.tipo = Espera::FRAMES;
            es.frame = a.frame + std::strtoull(std::string(v).c_str(), nullptr, 10);
        } else {
            return error("uso: esperar pantalla=|paquete=|html=|ventana=|dialogo=|chat=|frames=|ms= [plazo=ms]");
        }
        return false;
    } else if (cmd == "salir") {
        responder(c, "ok");
        a.salir = true;
        return true;
    } else {
        return error("comando desconocido (ayuda)");
    }
    responder(c, "ok");
    return true;
}

// Una conexión en espera: ¿se cumplió o venció?
void revisar_espera(App& a, Conexion& c) {
    Espera& es = c.espera;
    bool listo = false;
    std::string falla;
    switch (es.tipo) {
    case Espera::NADA: return;
    case Espera::PANTALLA: listo = es.texto == nombre_pantalla(a.pantalla); break;
    case Espera::PAQUETE: listo = es.visto; break;
    case Espera::HTML: listo = a.npc && a.npc_pagina == es.texto; break;
    case Espera::FRAMES: listo = a.frame >= es.frame; break;
    case Espera::MS: listo = a.entrada.ahora_ms >= es.frame; break;
    case Espera::VENTANA: listo = abierta(a, es.texto); break;
    case Espera::CHAT:  // una línea nueva que contenga el texto
        for (u32 i = 0; !listo && i < std::min(a.chat.total - es.chat_desde, a.chat.cantidad()); ++i)
            listo = a.chat.texto(a.chat.reciente(i)).find(es.texto) != std::string_view::npos;
        break;
    case Espera::LOGIN:
        if (a.mostrando_noticias) {  // las noticias del servidor se aceptan solas
            a.mostrando_noticias = false;
            enviar(a, cmd::lista_personajes(a.op));
        }
        listo = a.pantalla == Pantalla::PERSONAJES;
        if (!a.esperando && a.pantalla == Pantalla::LOGIN) falla = a.aviso.empty() ? "no se conectó" : a.aviso;
        break;
    case Espera::PERSONAJE: listo = a.pantalla == Pantalla::JUEGO && a.mundo.en_juego; break;
    case Espera::CREAR:  // aparece en la lista, o el servidor lo rechaza (el motivo queda en el aviso)
        listo = std::any_of(a.pjs.begin(), a.pjs.end(), [&](const PersonajeLista& pj) { return pj.nombre == es.texto; });
        break;
    }
    if (listo) {
        responder(c, "ok");
    } else if (!falla.empty()) {
        responder(c, "error " + falla);
    } else if (a.entrada.ahora_ms >= es.vence_ms) {
        responder(c, "error plazo");
    } else {
        return;
    }
    es.tipo = Espera::NADA;
}

void cerrar_conexion(Conexion& c) {
    ::close(c.fd);
    c.fd = -1;
}

}  // namespace

void control_iniciar(App&) {
    const char* e = std::getenv("LIN_CONTROL");
    if (e && *e) {
        g.ruta = e;
    } else {
        const char* tmp = std::getenv("TMPDIR");
        std::string dir = tmp && *tmp ? tmp : "/tmp/";
        if (dir.back() != '/') dir += '/';
        g.ruta = dir + "lineage-control-" + std::to_string(::getpid()) + ".sock";
    }
    sockaddr_un dir{};
    dir.sun_family = AF_UNIX;
    if (g.ruta.size() >= sizeof dir.sun_path) {
        LIN_LOG("Control en vivo: la ruta es demasiado larga (%zu bytes): %s", g.ruta.size(), g.ruta.c_str());
        return;
    }
    std::memcpy(dir.sun_path, g.ruta.c_str(), g.ruta.size() + 1);
    ::unlink(g.ruta.c_str());
    g.escucha = ::socket(AF_UNIX, SOCK_STREAM, 0);
    const mode_t antes = ::umask(0077);  // solo el usuario
    const bool ok = g.escucha >= 0 && ::bind(g.escucha, reinterpret_cast<sockaddr*>(&dir), sizeof dir) == 0 &&
                    ::listen(g.escucha, MAX_CONEXIONES) == 0;
    ::umask(antes);
    if (!ok) {
        LIN_LOG("Control en vivo: no se pudo abrir %s (%s)", g.ruta.c_str(), std::strerror(errno));
        if (g.escucha >= 0) ::close(g.escucha);
        g.escucha = -1;
        return;
    }
    ::fcntl(g.escucha, F_SETFL, O_NONBLOCK);
    LIN_LOG("Control en vivo: %s", g.ruta.c_str());
}

void control_frame(App& a) {
    if (g.escucha < 0) return;
    // Conexiones nuevas.
    for (int fd; static_cast<int>(g.conexiones.size()) < MAX_CONEXIONES && (fd = ::accept(g.escucha, nullptr, nullptr)) >= 0;) {
        ::fcntl(fd, F_SETFL, O_NONBLOCK);
        g.conexiones.push_back({fd, {}, {}, {}});
    }
    for (Conexion& c : g.conexiones) {
        // Leer lo que llegó.
        char buf[4096];
        for (ssize_t n; (n = ::recv(c.fd, buf, sizeof buf, 0)) != 0;) {
            if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) cerrar_conexion(c);
                break;
            }
            c.entrada.append(buf, static_cast<size_t>(n));
        }
        if (c.fd < 0) continue;
        // Un comando a la vez: el siguiente espera a que el anterior responda.
        revisar_espera(a, c);
        for (size_t fin; c.espera.tipo == Espera::NADA && (fin = c.entrada.find('\n')) != std::string::npos;) {
            std::string linea = c.entrada.substr(0, fin);
            c.entrada.erase(0, fin + 1);
            if (!linea.empty() && linea.back() == '\r') linea.pop_back();
            if (!ejecutar(a, c, linea)) revisar_espera(a, c);
        }
        // Escribir las respuestas.
        while (!c.salida.empty()) {
            const ssize_t n = ::send(c.fd, c.salida.data(), c.salida.size(), 0);
            if (n <= 0) break;
            c.salida.erase(0, static_cast<size_t>(n));
        }
    }
    std::erase_if(g.conexiones, [](const Conexion& c) { return c.fd < 0; });
    // Texto inyectado: uno por frame (SDL guarda el puntero hasta leer el evento).
    if (!g.textos.empty()) {
        g.texto_actual = std::move(g.textos.front());
        g.textos.pop_front();
        SDL_Event t{};
        t.type = SDL_EVENT_TEXT_INPUT;
        t.text.text = g.texto_actual.c_str();
        SDL_PushEvent(&t);
    }
}

void control_entrada(App& a) {
    EntradaFrame& e = a.entrada;
    if (g.ctrl) e.ctrl = true;
    if (g.shift) e.shift = true;
    if (g.mouse.empty()) return;
    const PasoMouse m = g.mouse.front();
    g.mouse.pop_front();
    e.mx = m.x;
    e.my = m.y;
    if (m.boton == 0) {
        e.pulsado = m.apretar;
        e.presionado = m.apretar;
        e.clic = m.soltar;
    } else if (m.boton == 1) {
        e.derecho_presionado = m.apretar;
        e.clic_derecho = m.soltar;
    } else if (m.boton == 2) {
        e.medio_presionado = m.apretar;
        e.clic_medio = m.soltar;
    } else {
        e.rueda = m.rueda;
    }
}

void control_paquete(Srv s) {
    g.recientes[g.n_recientes++ % Control::RECIENTES] = s;
    for (Conexion& c : g.conexiones)
        if (c.espera.tipo == Espera::PAQUETE && c.espera.texto == nombre_srv(s)) c.espera.visto = true;
}

void control_cerrar() {
    for (Conexion& c : g.conexiones) ::close(c.fd);
    g.conexiones.clear();
    if (g.escucha >= 0) {
        ::close(g.escucha);
        ::unlink(g.ruta.c_str());
    }
    g.escucha = -1;
}

}  // namespace lin
