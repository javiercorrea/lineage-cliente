// Control en vivo para pruebas (solo con LIN_CONTROL; la versión final no lo compila).
//
// Un socket de archivo (Unix domain socket) en $LIN_CONTROL o $TMPDIR/lineage-control-<pid>.sock,
// solo para el usuario. Cada línea es un comando; la respuesta son líneas que terminan con "ok" o
// "error <motivo>". Se revisa una vez por frame en el hilo principal (sin hilos ni locks): los
// comandos llaman a las mismas funciones que la interfaz. Los que esperan algo ("esperar", "login",
// "personaje", "captura") dejan la conexión en espera y responden en el frame en que se cumple o
// vence el plazo, sin frenar el juego.
//
// Comandos: ver ayuda() y docs/analisis/PLAN_CONTROL_VIVO.md. Herramienta: herramientas/lin_ctl.py.
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
    enum Tipo : u8 { NADA, PANTALLA, PAQUETE, HTML, LOGIN, PERSONAJE, FRAMES } tipo = NADA;
    Pantalla pantalla = Pantalla::LOGIN;
    Srv paquete = Srv::DESCONOCIDO;
    std::string texto;
    bool visto = false;  // el paquete llegó
    u64 vence_ms = 0, frame = 0;
};

struct Conexion {
    int fd = -1;
    std::string entrada, salida;
    Espera espera;
};

// Un paso de mouse inyectado: se aplica en un frame (después de leer la entrada real).
struct PasoMouse {
    int x, y;
    u8 boton;  // 0 izquierdo, 1 derecho, 2 medio
    bool apretar, soltar;
};

struct Control {
    int escucha = -1;
    std::string ruta;
    std::vector<Conexion> conexiones;
    std::deque<PasoMouse> mouse;
    std::deque<std::string> textos;  // un SDL_EVENT_TEXT_INPUT por frame
    std::string texto_actual;        // el que SDL está por leer (tiene que seguir vivo)
    bool ctrl = false;               // "ctrl on": Ctrl mantenido (forzar ataque)
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
        if (l[i] == '"') {
            const size_t fin = l.find('"', i + 1);
            v.emplace_back(l.substr(i + 1, (fin == std::string_view::npos ? l.size() : fin) - i - 1));
            i = fin == std::string_view::npos ? l.size() : fin + 1;
        } else {
            const size_t fin = std::min(l.find(' ', i), l.size());
            v.emplace_back(l.substr(i, fin - i));
            i = fin;
        }
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

void estado(App& a, Conexion& c) {
    char l[256];
    const Servidor& sv = servidor_elegido(a.cfg);
    std::snprintf(l, sizeof l, "pantalla=%s servidor=\"%s\" frame=%llu", nombre_pantalla(a.pantalla), sv.nombre.c_str(),
                  static_cast<unsigned long long>(a.frames_en_pantalla));
    responder(c, l);
    if (a.pantalla != Pantalla::JUEGO) {
        if (!a.aviso.empty()) responder(c, "aviso=\"" + a.aviso + "\"");
        return;
    }
    const EstadoJugador& j = a.mundo.jugador;
    const Slot s = a.mundo.objetos.buscar(j.id);
    const int x = s != SIN_SLOT ? a.mundo.objetos.col<O_X>()[s] : 0, y = s != SIN_SLOT ? a.mundo.objetos.col<O_Y>()[s] : 0;
    const std::string_view nombre = nombre_personaje(a);
    std::snprintf(l, sizeof l, "personaje=%.*s mapa=%u x=%d y=%d nivel=%d hp=%d/%d mp=%d/%d", static_cast<int>(nombre.size()),
                  nombre.data(), static_cast<unsigned>(a.mundo.mapa), x, y, j.nivel, j.hp, j.hp_max, j.mp, j.mp_max);
    responder(c, l);
}

const char* AYUDA[] = {
    "estado",
    "login <cuenta> <clave> [servidor]   (espera la lista de personajes)",
    "personaje <nombre>                  (espera estar en el juego)",
    "decir \"texto\"                       (como escrito en el chat; también /comandos)",
    "clic <x> <y> [der|medio]   tecla <nombre>   texto \"...\"   ctrl on|off",
    "esperar pantalla=<login|personajes|juego> | paquete=<NOMBRE> | html=<página> | frames=<n>  [plazo=ms]",
    "captura <ruta.bmp>   mudo on|off   traza on|off   salir",
};

// Ejecuta una línea. Devuelve false si queda esperando (responde después).
bool ejecutar(App& a, Conexion& c, std::string_view linea) {
    const std::vector<std::string> p = palabras(linea);
    if (p.empty()) return true;
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
    } else if (cmd == "decir") {
        if (a.pantalla != Pantalla::JUEGO) return error("no está en el juego");
        if (p.size() < 2) return error("uso: decir \"texto\"");
        enviar_chat(a, p[1]);
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
        } else if (std::string_view v = opcion(p, "html"); !v.empty()) {
            es.tipo = Espera::HTML;
            es.texto = v;
        } else if (std::string_view v = opcion(p, "frames"); !v.empty()) {
            es.tipo = Espera::FRAMES;
            es.frame = a.frame + std::strtoull(std::string(v).c_str(), nullptr, 10);
        } else {
            return error("uso: esperar pantalla=|paquete=|html=|frames= [plazo=ms]");
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
    case Espera::LOGIN:
        if (a.mostrando_noticias) {  // las noticias del servidor se aceptan solas
            a.mostrando_noticias = false;
            enviar(a, cmd::lista_personajes(a.op));
        }
        listo = a.pantalla == Pantalla::PERSONAJES;
        if (!a.esperando && a.pantalla == Pantalla::LOGIN) falla = a.aviso.empty() ? "no se conectó" : a.aviso;
        break;
    case Espera::PERSONAJE: listo = a.pantalla == Pantalla::JUEGO && a.mundo.en_juego; break;
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
    } else {
        e.medio_presionado = m.apretar;
        e.clic_medio = m.soltar;
    }
}

void control_paquete(Srv s) {
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
