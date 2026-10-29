#include <atomic>
#include <cstdlib>
#include "app/app.h"
#include "core/asignaciones.h"

namespace lin {

// LIN_TRAZA=1 registra cada paquete enviado y recibido (depuración del protocolo).
static const bool TRAZA = std::getenv("LIN_TRAZA") != nullptr;

namespace {

// Carpeta elegida en el diálogo nativo (el callback llega desde otro hilo).
std::atomic<int> g_dialogo{0};  // 0 abierto, 1 elegida, 2 cancelada
std::string g_carpeta_elegida;

void SDLCALL al_elegir_carpeta(void*, const char* const* lista, int) {
    if (lista && lista[0]) {
        g_carpeta_elegida = lista[0];
        g_dialogo = 1;
    } else {
        g_dialogo = 2;
    }
}

Pantalla pantalla_de(const std::string& s) {
    if (s == "login") return Pantalla::LOGIN;
    if (s == "personajes") return Pantalla::PERSONAJES;
    if (s == "crear") return Pantalla::CREAR;
    return Pantalla::JUEGO;
}

bool cargar_recursos(App& a) {
    if (!a.rec.abrir(a.cfg.carpeta_juego)) {
        LIN_LOG("No se pudieron abrir los .pak en %s", a.cfg.carpeta_juego.c_str());
        return false;
    }
    if (!a.fuente.cargar(a.cfg.carpeta_juego)) LIN_LOG("Aviso: no se encontró font/eng.fnt");
    a.desc.cargar(a.rec, "desc-e.tbl");
    a.intro.cargar(a.rec, "intro-e.tbl");
    if (!a.mensajes.cargar(a.rec, "string-e.tbl")) LIN_LOG("Aviso: no se encontró string-e.tbl (mensajes del servidor)");
    // datos/sprite_table.txt junto al ejecutable o en el directorio actual.
    const char* base = SDL_GetBasePath();
    std::string ruta = std::string(base ? base : "") + "datos/sprite_table.txt";
    if (!a.sprites.cargar_archivo(ruta) && !a.sprites.cargar_archivo("datos/sprite_table.txt"))
        LIN_LOG("Aviso: no se encontró datos/sprite_table.txt (los personajes no se animarán)");
    a.mundo.sprites = &a.sprites;
    if (!cargar_formas(a)) LIN_LOG("Aviso: no se encontró polymorph.tbl (sugerencias de polymorph)");
    if (!a.hechizos.cargar(a.rec)) LIN_LOG("Aviso: no se encontró spelldesc-e.tbl (nombres de los hechizos)");
    a.g = std::make_unique<Graficos>(a.gpu, a.rec, a.fuente);
    cargar_sonidos(a);
    return true;
}

void leer_entrada(App& a) {
    EntradaFrame& e = a.entrada;
    e.clic = e.clic_derecho = e.clic_medio = e.enter = e.tab = e.escape = e.retroceso = false;
    e.pagina_arriba = e.pagina_abajo = e.alt_c = e.alt_i = e.alt_s = e.pulsado = false;
    e.tecla_f = 0;
    e.alt_numero = 0;
    e.numero = 0;
    e.tecla_a = false;
    int texto_antes = 0;  // para distinguir Alt+número de un carácter escrito con Option (ver abajo)
    e.rueda = 0;
    e.zoom = 0;
    e.ntexto = 0;
    e.ahora_ms = SDL_GetTicks();
    // Prueba automática (--pulsar): una tecla cada 10 frames desde el frame 300 del juego (o del
    // login, para probarlo sin --auto), como
    // eventos de SDL (tecla y, si se escribe, texto): pasan por el mismo camino que el teclado.
    Automatizacion& au = a.aut;
    if (au.pulsadas < au.pulsar.size() && (a.pantalla == Pantalla::JUEGO || a.pantalla == Pantalla::LOGIN) &&
        a.frames_en_pantalla >= 300 &&
        (a.frames_en_pantalla - 300) % 10 == 0) {
        const char c = au.pulsar[au.pulsadas++];
        SDL_Event k{};
        k.type = SDL_EVENT_KEY_DOWN;
        k.key.key = c == '#' ? SDLK_RETURN : c == '~' ? SDLK_ESCAPE : static_cast<SDL_Keycode>(c);
        SDL_PushEvent(&k);
        if (c != '#' && c != '~') {
            static char texto[2];
            texto[0] = c;
            SDL_Event t{};
            t.type = SDL_EVENT_TEXT_INPUT;
            t.text.text = texto;
            SDL_PushEvent(&t);
        }
    }
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT: a.salir = true; break;
        case SDL_EVENT_MOUSE_MOTION: a.gpu.a_logico(ev.motion.x, ev.motion.y, e.mx, e.my); break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            a.gpu.a_logico(ev.button.x, ev.button.y, e.mx, e.my);
            if (ev.button.button == SDL_BUTTON_LEFT) e.presionado = e.pulsado = true;
            if (ev.button.button == SDL_BUTTON_RIGHT) e.derecho_presionado = true;
            if (ev.button.button == SDL_BUTTON_MIDDLE) e.medio_presionado = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            a.gpu.a_logico(ev.button.x, ev.button.y, e.mx, e.my);
            if (ev.button.button == SDL_BUTTON_LEFT) { e.presionado = false; e.clic = true; }
            if (ev.button.button == SDL_BUTTON_RIGHT) { e.derecho_presionado = false; e.clic_derecho = true; }
            if (ev.button.button == SDL_BUTTON_MIDDLE) { e.medio_presionado = false; e.clic_medio = true; }
            break;
        case SDL_EVENT_MOUSE_WHEEL: e.rueda += ev.wheel.y > 0 ? 1 : (ev.wheel.y < 0 ? -1 : 0); break;
        case SDL_EVENT_TEXT_INPUT:
            if (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) break;  // atajos (zoom), no texto
            for (const char* p = ev.text.text; *p && e.ntexto < 63; ++p) e.texto[e.ntexto++] = *p;
            break;
        case SDL_EVENT_KEY_DOWN:
            switch (ev.key.key) {
            case SDLK_RETURN: case SDLK_KP_ENTER: e.enter = true; break;
            case SDLK_TAB: e.tab = true; break;
            case SDLK_ESCAPE: e.escape = true; break;
            case SDLK_BACKSPACE: e.retroceso = true; break;
            case SDLK_PAGEUP: e.pagina_arriba = true; break;
            case SDLK_PAGEDOWN: e.pagina_abajo = true; break;
            case SDLK_C:
                if (ev.key.mod & SDL_KMOD_ALT) e.alt_c = true;
                break;
            case SDLK_I:
                if (ev.key.mod & SDL_KMOD_ALT) e.alt_i = true;
                break;
            case SDLK_S:
                if (ev.key.mod & SDL_KMOD_ALT) e.alt_s = true;
                break;
            case SDLK_A:
                if (!(ev.key.mod & (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_SHIFT))) e.tecla_a = true;
                break;
            case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS:
                if (ev.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) e.zoom = 1;
                break;
            case SDLK_MINUS: case SDLK_KP_MINUS:
                if (ev.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) e.zoom = -1;
                break;
            case SDLK_0: case SDLK_KP_0:
                if (ev.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) e.zoom = 2;
                else if ((ev.key.mod & SDL_KMOD_ALT) && ev.key.key == SDLK_0) e.alt_numero = 10;
                else if (ev.key.key == SDLK_0 && !(ev.key.mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT))) e.numero = 10;
                texto_antes = e.ntexto;
                break;
            case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8:
            case SDLK_9:
                if ((ev.key.mod & SDL_KMOD_ALT) && !(ev.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)))
                    e.alt_numero = 1 + static_cast<int>(ev.key.key - SDLK_1);  // SDLK_1..SDLK_9 son consecutivos
                else if (!(ev.key.mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_GUI)))
                    e.numero = 1 + static_cast<int>(ev.key.key - SDLK_1);
                texto_antes = e.ntexto;
                break;
            case SDLK_F1: a.ayuda_teclas = !a.ayuda_teclas; break;
            case SDLK_F3: a.depuracion = !a.depuracion; break;
            case SDLK_F2:  // UI simple (texto) o panel original
                a.cfg.ui_simple = !a.cfg.ui_simple;
                guardar_config(a.cfg);
                break;
            case SDLK_F4:
                // Cámara a saltos (como el original) o suave; con Shift, personajes oscurecidos en
                // la sombra o no.
                if (ev.key.mod & SDL_KMOD_SHIFT) a.cfg.sombra_personajes = !a.cfg.sombra_personajes;
                else a.cfg.camara_a_saltos = !a.cfg.camara_a_saltos;
                guardar_config(a.cfg);
                break;
            case SDLK_F12:
                if (ev.key.mod & SDL_KMOD_SHIFT) {
                    a.gpu.pedir_captura("captura.bmp");
                    break;
                }
                e.tecla_f = 12;
                break;
            case SDLK_F5: case SDLK_F6: case SDLK_F7: case SDLK_F8: case SDLK_F9: case SDLK_F10: case SDLK_F11:
                e.tecla_f = 5 + static_cast<int>(ev.key.key - SDLK_F5);  // los códigos F5..F11 son consecutivos
                break;
            default: break;
            }
            break;
        default: break;
        }
    }
    // Alt+número: en algunos teclados (español en Mac: Option+2 = "@") Alt con un número escribe
    // un carácter. Si salió un carácter ASCII se lo toma como texto; si no (Option+1 = "¡" en
    // inglés), es el atajo y el carácter se descarta.
    if (e.alt_numero && e.ntexto > texto_antes) {
        bool ascii = true;
        for (int i = texto_antes; i < e.ntexto; ++i) ascii &= (static_cast<u8>(e.texto[i]) < 0x80);
        if (ascii) e.alt_numero = 0;
        else e.ntexto = texto_antes;
    }
    const bool* teclas = SDL_GetKeyboardState(nullptr);
    e.arriba = teclas[SDL_SCANCODE_UP];
    e.abajo = teclas[SDL_SCANCODE_DOWN];
    e.izquierda = teclas[SDL_SCANCODE_LEFT];
    e.derecha = teclas[SDL_SCANCODE_RIGHT];
    e.ctrl = (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0 || a.aut.ctrl;
    e.shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    // Prueba automática (--arrastrar, repetible): el n-ésimo aprieta en (x1, y1) en el frame
    // 300 + 180·n del juego, se mueve en 30 frames hasta (x2, y2) y suelta.
    const u64 desde = a.frames_en_pantalla >= 300 ? a.frames_en_pantalla - 300 : ~0ull;
    if (a.pantalla == Pantalla::JUEGO && desde / 180 < a.aut.arrastres.size() && desde % 180 <= 31) {
        const int* ar = a.aut.arrastres[desde / 180].data();
        int k = static_cast<int>(desde % 180);
        e.mx = ar[0] + (ar[2] - ar[0]) * std::min(k, 30) / 30;
        e.my = ar[1] + (ar[3] - ar[1]) * std::min(k, 30) / 30;
        e.pulsado = k == 0;
        e.presionado = k < 31;
        e.clic = k == 31;
    }
    // Prueba automática (--mantener=x,y,N): mantener el botón izquierdo en (x, y) desde el frame
    // 300 durante N frames.
    if (!a.aut.mantener_sobre.empty() && a.pantalla == Pantalla::JUEGO && a.frames_en_pantalla == 300) {
        // El punto de pantalla (fijo desde ahí) del más cercano con ese nombre.
        const std::string& v = a.aut.mantener_sobre;
        const size_t coma = v.rfind(',');
        const std::string_view nombre = std::string_view(v).substr(0, coma);
        for (const App::ObjetoEnPantalla& r : a.en_pantalla) {
            const Slot so = a.mundo.objetos.buscar(r.id);
            if (so == SIN_SLOT || a.desc.resolver(texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[so])).find(nombre) == std::string_view::npos) continue;
            a.aut.mantener[0] = static_cast<int>((r.x0 + r.x1) / 2);
            a.aut.mantener[1] = static_cast<int>((r.y0 + r.y1) / 2);
            a.aut.mantener[2] = coma == std::string::npos ? 120 : std::atoi(v.c_str() + coma + 1);
            break;
        }
    }
    const int* ma = a.aut.mantener;
    if (ma[0] >= 0 && a.pantalla == Pantalla::JUEGO && a.frames_en_pantalla >= 300 && a.frames_en_pantalla <= 300u + ma[2]) {
        e.mx = ma[0];
        e.my = ma[1];
        e.pulsado = a.frames_en_pantalla == 300;
        e.presionado = a.frames_en_pantalla < 300u + ma[2];
        e.clic = a.frames_en_pantalla == 300u + ma[2];
    }
    // Prueba automática (--medio=x,y,N): mantener el botón del medio en (x, y) desde el frame 300
    // durante N frames.
    const int* me = a.aut.medio;
    if (me[0] >= 0 && a.pantalla == Pantalla::JUEGO && a.frames_en_pantalla >= 300 && a.frames_en_pantalla <= 300u + me[2]) {
        e.mx = me[0];
        e.my = me[1];
        e.medio_presionado = a.frames_en_pantalla < 300u + me[2];
        e.clic_medio = a.frames_en_pantalla == 300u + me[2];
    }
}

// Red: transiciones de estado y paquetes -> eventos -> pantallas y mundo.
void procesar_red(App& a) {
    EstadoRed st = a.red.estado();
    if (st != a.estado_red_previo) {
        if (st == EstadoRed::CONECTADA) {
            enviar(a, cmd::version(a.op));
            enviar(a, cmd::login(a.op, a.usuario, a.clave));
        } else if (st == EstadoRed::ERROR || st == EstadoRed::CERRADA) {
            a.aviso = a.red.error();
            a.esperando = false;
            if (a.pantalla != Pantalla::LOGIN) {
                LIN_LOG("Conexión perdida: %s", a.aviso.c_str());
                a.mundo.limpiar();
                a.chat.limpiar();
                cambiar_pantalla(a, Pantalla::LOGIN);
            }
        }
        a.estado_red_previo = st;
    }
    a.eventos.clear();
    while (a.red.sacar(a.paquete)) {
        if (TRAZA) {
            char hex[3 * 16 + 1] = {};
            for (size_t i = 1; i < a.paquete.size() && i <= 16; ++i) std::snprintf(hex + 3 * (i - 1), 4, "%02x ", a.paquete[i]);
            LIN_LOG("[%llu] <- %3u %-16s %s", static_cast<unsigned long long>(SDL_GetTicks()), a.paquete[0],
                    nombre_srv(a.op.srv(a.paquete[0])), hex);
        }
        decodificar(a.paquete, a.op, a.arena, a.eventos);
    }
    for (const Evento& e : a.eventos) {
        eventos_login(a, e);
        eventos_personajes(a, e);
        eventos_forma(a, e);
        eventos_npc(a, e);
        eventos_social(a, e);
        eventos_cantidad(a, e);
        eventos_mascotas(a, e);
        eventos_marcadores(a, e);
        if (e.tipo == Ev::MAPA && (a.pantalla == Pantalla::JUEGO || a.pantalla == Pantalla::CARGANDO)) avisar_mapa(a, static_cast<MapId>(e.v[0]));
    }
    aplicar_eventos(a.mundo, a.eventos, a.entrada.ahora_ms);
    aplicar_chat(a.chat, a.mensajes, a.desc, a.eventos, a.entrada.ahora_ms);
}

void frame_elegir_carpeta(App& a) {
    if (g_dialogo == 1) {
        a.cfg.carpeta_juego = g_carpeta_elegida + "/";
        if (carpeta_juego_valida(a.cfg.carpeta_juego) && cargar_recursos(a)) {
            guardar_config(a.cfg);
            cambiar_pantalla(a, Pantalla::LOGIN);
        } else {
            g_dialogo = 0;
            SDL_ShowOpenFolderDialog(al_elegir_carpeta, nullptr, a.ventana, nullptr, false);
        }
    } else if (g_dialogo == 2) {
        a.salir = true;
    }
}

}  // namespace

void enviar(App& a, Escritor e) {
    Bytes b = e.cuerpo();
    if (TRAZA) {
        char hex[3 * 16 + 1] = {};
        for (size_t i = 0; i < b.size() && i < 16; ++i) std::snprintf(hex + 3 * i, 4, "%02x ", b[i]);
        LIN_LOG("[%llu] -> %3u %s", static_cast<unsigned long long>(SDL_GetTicks()), b[0], hex);
    }
    a.red.enviar(b);
}

void cambiar_pantalla(App& a, Pantalla p) {
    a.pantalla = p;
    a.frames_en_pantalla = 0;
    a.foco = 0;
}

bool iniciar_app(App& a, int argc, char** argv) {
    a.cfg = cargar_config(argc, argv);
    int vw = 1280, vh = 960;  // tamaño inicial de la ventana en puntos (en Retina, el doble de píxeles)
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto valor = [&](const char* p) { return s.substr(std::strlen(p)); };
        if (s == "--auto") a.aut.activa = true;
        else if (s.rfind("--personaje=", 0) == 0) a.aut.personaje = valor("--personaje=");
        else if (s == "--crear") a.aut.crear = true;
        else if (s.rfind("--clase=", 0) == 0) a.aut.clase = std::atoi(valor("--clase=").c_str());
        else if (s.rfind("--captura=", 0) == 0) a.aut.captura = valor("--captura=");
        else if (s.rfind("--captura-pantalla=", 0) == 0) a.aut.pantalla_captura = pantalla_de(valor("--captura-pantalla="));
        else if (s.rfind("--captura-espera=", 0) == 0) a.aut.espera_frames = std::atoi(valor("--captura-espera=").c_str());
        else if (s == "--salir") a.aut.salir_tras_captura = true;
        else if (s == "--depuracion") a.depuracion = true;
        else if (s == "--perf") a.perf = true;
        else if (s == "--mudo") a.aut.mudo = true;
        else if (s.rfind("--pasos=", 0) == 0) a.aut.pasos = std::atoi(valor("--pasos=").c_str());
        else if (s.rfind("--decir=", 0) == 0) a.aut.decir.push_back(valor("--decir="));
        else if (s == "--estado") a.aut.estado = true;
        else if (s.rfind("--ruta=", 0) == 0) a.aut.ruta = valor("--ruta=");
        else if (s.rfind("--atacar=", 0) == 0) a.aut.atacar = valor("--atacar=");
        else if (s == "--reiniciar") a.aut.reiniciar = true;
        else if (s.rfind("--usar=", 0) == 0) a.aut.items.push_back({false, valor("--usar="), ""});
        else if (s.rfind("--soltar=", 0) == 0) a.aut.items.push_back({true, valor("--soltar="), ""});
        else if (s.rfind("--borrar=", 0) == 0) a.aut.items.push_back({false, valor("--borrar="), "", true});
        else if (s.rfind("--usar-sobre=", 0) == 0) {
            std::string v = valor("--usar-sobre=");
            const size_t arroba = v.find('@');
            a.aut.items.push_back({false, v.substr(0, arroba), "", false, arroba == std::string::npos ? "" : v.substr(arroba + 1)});
        }
        else if (s.rfind("--dar=", 0) == 0) {
            std::string v = valor("--dar=");
            const size_t arroba = v.find('@');
            a.aut.items.push_back({false, v.substr(0, arroba), arroba == std::string::npos ? "" : v.substr(arroba + 1)});
        }
        else if (s == "--recoger") a.aut.recoger = true;
        else if (s == "--inventario") a.ventana_inventario = true;
        else if (s.rfind("--lanzar=", 0) == 0) a.aut.lanzar.push_back(valor("--lanzar="));
        else if (s == "--hechizos") a.aut.hechizos = true;
        else if (s.rfind("--forma=", 0) == 0) a.aut.forma = valor("--forma=");
        else if (s.rfind("--hablar=", 0) == 0) a.aut.hablar = valor("--hablar=");
        else if (s.rfind("--npc-accion=", 0) == 0) a.aut.npc_acciones.push_back(valor("--npc-accion="));
        else if (s.rfind("--tienda=", 0) == 0) a.aut.tienda.push_back(valor("--tienda="));
        else if (s.rfind("--arrastrar=", 0) == 0)
        {
            std::array<int, 4> ar{};
            std::sscanf(s.c_str() + 12, "%d,%d,%d,%d", &ar[0], &ar[1], &ar[2], &ar[3]);
            a.aut.arrastres.push_back(ar);
        }
        else if (s.rfind("--pulsar=", 0) == 0) a.aut.pulsar = valor("--pulsar=");
        else if (s.rfind("--cantidad=", 0) == 0) a.aut.cantidad = std::atoi(s.c_str() + 11);
        else if (s.rfind("--destino=", 0) == 0) a.aut.destino = valor("--destino=");
        else if (s == "--ctrl") a.aut.ctrl = true;
        else if (s.rfind("--mantener-sobre=", 0) == 0) a.aut.mantener_sobre = valor("--mantener-sobre=");
        else if (s.rfind("--mantener=", 0) == 0)
            std::sscanf(s.c_str() + 11, "%d,%d,%d", &a.aut.mantener[0], &a.aut.mantener[1], &a.aut.mantener[2]);
        else if (s.rfind("--medio=", 0) == 0)
            std::sscanf(s.c_str() + 8, "%d,%d,%d", &a.aut.medio[0], &a.aut.medio[1], &a.aut.medio[2]);
        else if (s == "--opciones") a.ventana_opciones = true;
        else if (s == "--teclas") a.ayuda_teclas = true;
        else if (s == "--salida") a.ventana_salida = true;
        else if (s == "--aceptar") a.aut.aceptar = true;
        else if (s.rfind("--girar=", 0) == 0) a.aut.girar = std::atoi(valor("--girar=").c_str());
        else if (s.rfind("--ofrecer=", 0) == 0) a.aut.ofrecer = valor("--ofrecer=");
        else if (s == "--social") a.ventana_social = true;
        else if (s.rfind("--subir=", 0) == 0) a.aut.subir = valor("--subir=");
        else if (s.rfind("--captura-proyectil=", 0) == 0) a.aut.captura_proyectil = valor("--captura-proyectil=");
        else if (s.rfind("--alt=", 0) == 0) a.aut.alt.push_back(std::atoi(valor("--alt=").c_str()));
        else if (s.rfind("--ventana=", 0) == 0) std::sscanf(s.c_str() + 10, "%dx%d", &vw, &vh);
    }
    a.usuario = a.cfg.usuario;
    a.clave = a.cfg.clave;
    if (!tabla_opcodes(a.cfg.protocolo, a.op)) {
        LIN_LOG("Versión de protocolo desconocida: %s", a.cfg.protocolo.c_str());
        return false;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LIN_LOG("SDL_Init: %s", SDL_GetError());
        return false;
    }
    a.ventana = SDL_CreateWindow("Lineage", vw, vh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!a.ventana || !a.gpu.iniciar(a.ventana, 640, 480)) return false;
    SDL_StartTextInput(a.ventana);
    crear_cursores(a);
    if (carpeta_juego_valida(a.cfg.carpeta_juego) && cargar_recursos(a)) {
        cambiar_pantalla(a, Pantalla::LOGIN);
    } else {
        // Primer arranque: el usuario elige la carpeta del juego con el diálogo nativo.
        cambiar_pantalla(a, Pantalla::ELEGIR_CARPETA);
        SDL_ShowOpenFolderDialog(al_elegir_carpeta, nullptr, a.ventana, nullptr, false);
    }
    return true;
}

// Cierra el segundo de medición: promedios y máximos para el overlay F3 y --perf.
static void cerrar_medicion(App& a) {
    a.fps = a.frames_fps * 1000.0f / static_cast<float>(a.entrada.ahora_ms - a.t_fps);
    a.ms_cpu = static_cast<float>(a.acum_cpu / a.frames_fps);
    a.ms_cpu_max = static_cast<float>(a.max_cpu);
    a.ms_decod = a.g ? static_cast<float>(a.g->tomar_ms_decodificacion()) : 0;
    a.decodificados = a.g ? a.g->tomar_decodificados() : 0;
    a.asign_frame_max = a.max_asign;
    a.asign_total = a.acum_asign;
    if (a.perf)
        LIN_LOG("perf: %.0f fps  cpu %.2f ms (max %.2f)  decod %.2f ms (%u)  asign %llu (max/frame %llu)  inst %u  pag %u (reemplazos %u)  precarga %u  efectos %u  musica %d",
                a.fps, a.ms_cpu, a.ms_cpu_max, a.ms_decod, a.decodificados, static_cast<unsigned long long>(a.asign_total),
                static_cast<unsigned long long>(a.asign_frame_max), a.gpu.instancias_ultimo_frame(), a.gpu.paginas_usadas(), a.gpu.reemplazos_atlas(),
                a.g ? a.g->precargas_en_curso() : 0u, a.audio.efectos_tocados(), a.audio.musica_actual());
    a.acum_cpu = a.max_cpu = 0;
    a.acum_asign = a.max_asign = 0;
    a.frames_fps = 0;
    a.t_fps = a.entrada.ahora_ms;
}

void bucle_app(App& a) {
    while (!a.salir) {
        u64 t0 = SDL_GetPerformanceCounter();
        u64 asign0 = asignaciones_hilo();
        a.cursor = Cursor::MANO;
        if (a.g) a.g->nuevo_frame();
        // Dentro del juego la vista se amplía con la ventana (y el zoom); el resto, 640×480.
        a.gpu.ajustar_vista(a.pantalla == Pantalla::JUEGO || a.pantalla == Pantalla::CARGANDO, a.cfg.zoom);
        leer_entrada(a);
        procesar_red(a);
        switch (a.pantalla) {
        case Pantalla::ELEGIR_CARPETA: frame_elegir_carpeta(a); break;
        case Pantalla::LOGIN: frame_login(a); break;
        case Pantalla::PERSONAJES: frame_personajes(a); break;
        case Pantalla::CREAR: frame_crear(a); break;
        case Pantalla::CARGANDO:
        case Pantalla::JUEGO: frame_juego(a); break;
        }
        actualizar_sonido(a, a.entrada.ahora_ms);
        ++a.frames_en_pantalla;
        if (!a.aut.captura.empty() && a.pantalla == a.aut.pantalla_captura &&
            a.frames_en_pantalla == static_cast<u64>(a.aut.espera_frames)) {
            a.gpu.pedir_captura(a.aut.captura);
            if (a.aut.salir_tras_captura) a.salir = true;
        }
        // CPU y allocations: todo menos presentar, que espera el vsync y llama al driver (el de
        // Metal hace sus propias allocations al subir y presentar; no dependen del cliente).
        a.gpu.preparar_frame();
        double ms = static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency();
        a.acum_cpu += ms;
        a.max_cpu = std::max(a.max_cpu, ms);
        u64 asign = asignaciones_hilo() - asign0;
        a.acum_asign += asign;
        a.max_asign = std::max(a.max_asign, asign);
        ++a.frames_fps;
        aplicar_cursor(a);
        a.gpu.presentar();
        a.arena.resetear();
        if (a.entrada.ahora_ms - a.t_fps >= 1000) cerrar_medicion(a);
        ++a.frame;
    }
}

void cerrar_app(App& a) {
    // Como el cliente original, avisar la salida: sin C_QuitGame el servidor no guarda la posición.
    if (a.mundo.en_juego && a.red.estado() == EstadoRed::CONECTADA) {
        enviar(a, Escritor(a.op, Cli::QUITGAME));
        SDL_Delay(200);  // tiempo para que el hilo escritor lo envíe
    }
    a.red.cerrar();
    a.audio.cerrar();
    a.g.reset();
    destruir_cursores(a);
    a.gpu.cerrar();
    if (a.ventana) SDL_DestroyWindow(a.ventana);
    SDL_Quit();
}

}  // namespace lin
