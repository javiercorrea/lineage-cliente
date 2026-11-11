#include <atomic>
#include <cstdio>
#include <cstdlib>
#include "app/app.h"
#include "i18n/textos.h"
#include "core/asignaciones.h"

namespace lin {

// Depuración (comandos "traza" y "traza-html" del control en vivo): cada paquete enviado y
// recibido, y cada página HTML con sus datos.
bool g_traza = false, g_traza_html = false;

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
    if (!cargar_formas(a)) LIN_LOG("Aviso: no se encontró datos/formas.txt ni polymorph.tbl (sugerencias de polymorph)");
    if (!a.hechizos.cargar(a.rec)) LIN_LOG("Aviso: no se encontró spelldesc-e.tbl (nombres de los hechizos)");
    a.g = std::make_unique<Graficos>(a.gpu, a.rec, a.fuente);
    cargar_sonidos(a);
    preparar_luz(a);
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
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        // La barra de la ventana (arrastre, botones y bordes) es de la ventana: el juego no ve esos eventos.
        if ((ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP ||
             ev.type == SDL_EVENT_MOUSE_MOTION) &&
            ui::barra_evento(a.gpu, a.ventana, a.ventana_sin_borde, ev, alto_barra(a) > 0))
            continue;
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
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (ev.key.mod & SDL_KMOD_ALT) {  // Alt+Enter: pantalla completa o ventana (sin repetir al mantenerla)
                    if (!ev.key.repeat) {
                        alternar_pantalla_completa(a);
                        guardar_config(a.cfg);
                    }
                } else e.enter = true;
                break;
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
            case SDLK_F1:
                if (!ev.key.repeat) a.ayuda_teclas = !a.ayuda_teclas;
                break;
            case SDLK_F3:  // con Shift, el contador de fps
                if (ev.key.repeat) break;  // mantenida, alternaría y escribiría settings.ini en cada repetición
                if (ev.key.mod & SDL_KMOD_SHIFT) {
                    a.cfg.mostrar_fps = !a.cfg.mostrar_fps;
                    guardar_config(a.cfg);
                } else a.depuracion = !a.depuracion;
                break;
            case SDLK_F2:  // UI simple (texto) o panel original
                if (ev.key.repeat) break;
                a.cfg.ui_simple = !a.cfg.ui_simple;
                guardar_config(a.cfg);
                break;
            case SDLK_F4:
                if (ev.key.repeat) break;
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
    e.ctrl = (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
    e.shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
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
#if LIN_CONTROL
        control_paquete(a.op.srv(a.paquete[0]));
#endif
        if (g_traza) {
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
        eventos_tienda(a, e);
        eventos_tablon(a, e);
        eventos_correo(a, e);
        eventos_casas(a, e);
        eventos_castillos(a, e);
        if (e.tipo == Ev::SONIDO) a.audio.efecto(static_cast<u32>(e.v[0]), 1.0f, a.entrada.ahora_ms);  // S_Sound
        eventos_cantidad(a, e);
        eventos_reinicio(a, e);
        eventos_mascotas(a, e);
        eventos_marcadores(a, e);
        eventos_hechizos(a, e);
        if (e.tipo == Ev::MAPA && (a.pantalla == Pantalla::JUEGO || a.pantalla == Pantalla::CARGANDO)) avisar_mapa(a, static_cast<MapId>(e.v[0]));
        // "Miss" sobre el personaje: un ataque que lo tiene de objetivo y no acierta (el daño recibido
        // se detecta por la caída de HP tras aplicar los eventos, abajo).
        if (a.cfg.texto_dano && e.tipo == Ev::ATAQUE && static_cast<ObjId>(e.v[AT_OBJETIVO]) == a.mundo.jugador.id &&
            !e.v[AT_GOLPE])
            empujar_texto_combate(a, "Miss", 230, 230, 230);
    }
    aplicar_eventos(a.mundo, a.eventos, a.entrada.ahora_ms);
    a.red.en_juego(a.mundo.en_juego && a.pantalla == Pantalla::JUEGO);  // el keepalive lo manda el hilo de red
    if (a.cfg.texto_dano && a.mundo.jugador.hp_max > 0 && a.mundo.jugador.hp < a.hp_anterior) {
        char t[12];
        std::snprintf(t, sizeof t, "-%d", a.hp_anterior - a.mundo.jugador.hp);
        empujar_texto_combate(a, t, 255, 80, 60);
    }
    a.hp_anterior = a.mundo.jugador.hp;
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
    if (g_traza) {
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

bool iniciar_app(App& a) {
    a.cfg = cargar_config();
    a.usuario = servidor_elegido(a.cfg).usuario;
    Idioma idioma;  // settings.ini "idioma="; vacío o desconocido = el del sistema
    fijar_idioma(idioma_de_codigo(a.cfg.idioma, idioma) ? idioma : idioma_del_sistema());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LIN_LOG("SDL_Init: %s", SDL_GetError());
        return false;
    }
    // Tamaño en puntos (en Retina, el doble de píxeles): el de la última vez (settings.ini).
    // Sin borde del sistema: la barra la dibuja el juego (ventana_ui.cpp).
    a.ventana = SDL_CreateWindow("Lineage", a.cfg.ventana_ancho, a.cfg.ventana_alto,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!a.ventana || !a.gpu.iniciar(a.ventana, 640, 480)) return false;
    ui::poner_icono(a.ventana, "lineage.png");
    if (a.cfg.pantalla_completa) SDL_SetWindowFullscreen(a.ventana, true);
    a.ventana_sin_borde.redimensionable = true;
    SDL_StartTextInput(a.ventana);
    crear_cursores(a);
    if (carpeta_juego_valida(a.cfg.carpeta_juego) && cargar_recursos(a)) {
        cambiar_pantalla(a, Pantalla::LOGIN);
        // LIN_RENDER=id,x,y,salida.bmp: entrar directo a dibujar el mapa (sin red/login) y capturarlo.
        if (const char* r = std::getenv("LIN_RENDER"); r && *r) {
            int id = 0, x = 0, y = 0;
            char sal[256] = {0};
            if (std::sscanf(r, "%d,%d,%d,%255s", &id, &x, &y, sal) == 4) {
                a.render = {true, static_cast<MapId>(id), x, y, sal};
                a.mundo.mapa = static_cast<MapId>(id);
                cambiar_pantalla(a, Pantalla::JUEGO);
            } else {
                LIN_LOG("LIN_RENDER mal formado (esperaba id,x,y,salida.bmp): %s", r);
            }
        }
    } else {
        // Primer arranque: el usuario elige la carpeta del juego con el diálogo nativo.
        cambiar_pantalla(a, Pantalla::ELEGIR_CARPETA);
        SDL_ShowOpenFolderDialog(al_elegir_carpeta, nullptr, a.ventana, nullptr, false);
    }
#if LIN_CONTROL
    control_iniciar(a);
#endif
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
        LIN_LOG("perf: %.0f fps  cpu %.2f ms (max %.2f)  decod %.2f ms (%u)  asign %llu (max/frame %llu)  inst %u  pag %u (reemplazos %u)  precarga %u  efectos %u (wav %u)  musica %d",
                a.fps, a.ms_cpu, a.ms_cpu_max, a.ms_decod, a.decodificados, static_cast<unsigned long long>(a.asign_total),
                static_cast<unsigned long long>(a.asign_frame_max), a.gpu.instancias_ultimo_frame(), a.gpu.paginas_usadas(), a.gpu.reemplazos_atlas(),
                a.g ? a.g->precargas_en_curso() : 0u, a.audio.efectos_tocados(), a.audio.wavs_cargados(), a.audio.musica_actual());
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
        a.gpu.ajustar_vista(a.pantalla == Pantalla::JUEGO || a.pantalla == Pantalla::CARGANDO, a.cfg.zoom, alto_barra(a));
#if LIN_CONTROL
        control_frame(a);  // comandos del socket: antes de leer la entrada (las teclas van como eventos de SDL)
#endif
        leer_entrada(a);
#if LIN_CONTROL
        control_entrada(a);  // el mouse inyectado
#endif
        procesar_red(a);
        if (a.render.activo) {
            dibujar_captura(a);
        } else switch (a.pantalla) {
        case Pantalla::ELEGIR_CARPETA: frame_elegir_carpeta(a); break;
        case Pantalla::LOGIN: frame_login(a); break;
        case Pantalla::PERSONAJES: frame_personajes(a); break;
        case Pantalla::CREAR: frame_crear(a); break;
        case Pantalla::CARGANDO:
        case Pantalla::JUEGO: frame_juego(a); break;
        }
        actualizar_sonido(a, a.entrada.ahora_ms);
        ++a.frames_en_pantalla;
        dibujar_barra_ventana(a);
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
    // El tamaño de la ventana queda para la próxima vez.
    // A pantalla completa, el tamaño es el del monitor: no se guarda.
    if (a.ventana && !(SDL_GetWindowFlags(a.ventana) & SDL_WINDOW_FULLSCREEN) &&
        SDL_GetWindowSize(a.ventana, &a.cfg.ventana_ancho, &a.cfg.ventana_alto))
        guardar_config(a.cfg);
#if LIN_CONTROL
    control_cerrar();
#endif
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
