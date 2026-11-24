#include <algorithm>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "app/app.h"
#include "mundo/ataque.h"
#include "recursos/aes.h"
#include "recursos/sprites.h"
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

// Listas de sprites (texto plano, las pone el usuario, nunca se distribuyen). La principal: lista_sprites= de settings.ini, si no
// <carpeta del juego>/list.spz o <carpeta del juego>/spritelist/list.spz. Después cada carpeta juego_extra=, en orden:
// list.spz, spritelist/list.spz y cualquier *.txt de primer nivel que empiece con una cabecera de lista ("12317 0 41211" o
// "S21972 0 41210"; el TW13081901.txt del cliente TW). Un gfx ya conocido se conserva: ganan las fuentes anteriores.
static bool tiene_cabecera_lista(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::string l;
    if (!f || !std::getline(f, l)) return false;
    unsigned a = 0, b = 0, c = 0;
    return std::sscanf(l.c_str(), "S%u %u %u", &a, &b, &c) == 3 || std::sscanf(l.c_str(), "%u %u %u", &a, &b, &c) == 3;
}

static std::vector<std::string> listas_de_extra(const std::string& carpeta) {
    namespace fs = std::filesystem;
    std::vector<std::string> v;
    std::error_code ec;
    for (const char* rel : {"list.spz", "spritelist/list.spz"})
        if (fs::exists(carpeta + rel, ec)) v.push_back(carpeta + rel);
    std::vector<std::string> txt;
    for (const auto& e : fs::directory_iterator(carpeta, ec))
        if (e.is_regular_file(ec) && e.path().extension() == ".txt" && tiene_cabecera_lista(e.path())) txt.push_back(e.path().string());
    std::sort(txt.begin(), txt.end());
    v.insert(v.end(), txt.begin(), txt.end());
    return v;
}

static std::string lista_de_juego(const App& a) {
    namespace fs = std::filesystem;
    if (!a.cfg.lista_sprites.empty()) return a.cfg.lista_sprites;
    const std::string& j = a.cfg.carpeta_juego;
    for (const char* rel : {"list.spz", "spritelist/list.spz"})
        if (const std::string r = ruta_real(j + rel); fs::exists(r)) return r;
    std::error_code ec;
    std::vector<std::string> txt;
    for (const auto& e : fs::directory_iterator(j, ec)) {
        if (!e.is_regular_file(ec) || e.path().extension() != ".txt") continue;
        std::ifstream f(e.path(), std::ios::binary);
        std::string l;
        unsigned x = 0, y = 0, z = 0;
        if (f && std::getline(f, l) &&
            (std::sscanf(l.c_str(), "S%u %u %u", &x, &y, &z) == 3 || std::sscanf(l.c_str(), "%u %u %u", &x, &y, &z) == 3))
            txt.push_back(e.path().string());
    }
    std::sort(txt.begin(), txt.end());
    return txt.empty() ? std::string{} : txt.front();
}

static void cargar_lista_sprites(App& a) {
    LIN_LOG("Sprite table: %u gfx from sprite_table.txt", a.sprites.gfx_de_tabla());
    std::vector<std::string> rutas;
    const std::string p = lista_de_juego(a);
    if (!p.empty()) {
        rutas.push_back(p);
        if (!a.cfg.lista_sprites.empty() && !std::filesystem::exists(p)) LIN_LOG("Sprite table: lista_sprites=%s does not exist", p.c_str());
    }
    for (const std::string& e : a.cfg.carpetas_extra)
        for (std::string& r : listas_de_extra(e)) rutas.push_back(std::move(r));
    // Una lista puede venir de un cliente más nuevo: se revisa cada fila contra los archivos de sprites que sí tenemos.
    const ContadorFrames contador = [&a](GfxId g, u16 sub) {
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%u-%u.spr", g, sub);
        return contar_frames_spr(a.rec.sprite(nombre));
    };
    auto informar = [](bool ok, const ResumenLista& r, const char* origen) {
        if (ok)
            LIN_LOG("Sprite table: %u gfx from %s (%u new with actions, %u attribute-only, %u already known kept, %u skipped; "
                    "%u with a missing/redirected file, %u with frames dropped)", r.nuevos + r.atributos, origen, r.nuevos, r.atributos,
                    r.reemplazados, r.omitidos, r.sin_archivo, r.recortados);
        else
            LIN_LOG("Sprite table: %s has no usable entry", origen);
    };
    // Sin lista en texto plano, la del juego: list.spz de Text.pak, con 5 bytes en claro ("S1354") y el resto en AES
    // (docs/analisis/FORMATOS_PAK.md).
    if (std::vector<u8> t; p.empty() && a.rec.texto("list.spz", t) && t.size() > 5) {
        descifrar_aes_lineage(t.data() + 5, t.size() - 5);
        ResumenLista r;
        informar(a.sprites.fusionar_lista({reinterpret_cast<const char*>(t.data()), t.size()}, r, &contador), r, "Text.pak list.spz");
    }
    for (const std::string& ruta : rutas) {
        ResumenLista r;
        informar(a.sprites.fusionar_lista_archivo(ruta, r, &contador), r, ruta.c_str());
    }
}

bool cargar_recursos(App& a) {
    if (!a.rec.abrir(a.cfg.carpeta_juego, a.cfg.carpetas_extra)) {
        LIN_LOG("No se pudieron abrir los .pak en %s", a.cfg.carpeta_juego.c_str());
        return false;
    }
    usar_fuente_original(a.cfg.fuente_original);
    if (!a.fuente.cargar(a.cfg.carpeta_juego)) LIN_LOG("Aviso: no se encontró font/eng.fnt");
    a.desc.cargar(a.rec, "desc-e.tbl");
    a.zonas.cargar(a.rec);
    {
        TablaTexto q;
        if (q.cargar(a.rec, "itemquest.tbl")) cargar_item_quest(q, a.item_quest);
    }
    a.intro.cargar(a.rec, "intro-e.tbl");
    if (!a.mensajes.cargar(a.rec, "string-e.tbl")) LIN_LOG("Aviso: no se encontró string-e.tbl (mensajes del servidor)");
    // datos/sprite_table.txt junto al ejecutable o en el directorio actual.
    const char* base = SDL_GetBasePath();
    std::string ruta = std::string(base ? base : "") + "datos/sprite_table.txt";
    if (!a.sprites.cargar_archivo(ruta) && !a.sprites.cargar_archivo("datos/sprite_table.txt"))
        LIN_LOG("Aviso: no se encontró datos/sprite_table.txt (los personajes no se animarán)");
    cargar_lista_sprites(a);
    a.mundo.sprites = &a.sprites;
    if (!cargar_formas(a)) LIN_LOG("Aviso: no se encontró datos/formas.txt ni polymorph.tbl (sugerencias de polymorph)");
    if (!a.hechizos.cargar(a.rec)) LIN_LOG("Aviso: no se encontró spelldesc-e.tbl (nombres de los hechizos)");
    a.g = std::make_unique<Graficos>(a.gpu, a.rec, a.fuente);
    if (!a.cfg.carpeta_hd.empty() && !a.g->abrir_hd(a.cfg.carpeta_hd))
        LIN_LOG("HD: no manifest.tsv in %s: using the original images", a.cfg.carpeta_hd.c_str());
    cargar_sonidos(a);
    preparar_luz(a);
    return true;
}

void leer_entrada(App& a) {
    EntradaFrame& e = a.entrada;
    e.clic = e.clic_derecho = e.doble_derecho = e.doble_izquierdo = e.clic_medio = e.enter = e.tab = e.escape = e.retroceso = e.suprimir = false;
    e.pagina_arriba = e.pagina_abajo = e.alt_c = e.alt_i = e.alt_s = e.pulsado = false;
    e.atajo_ctrl = 0;
    e.tecla_f = 0;
    e.alt_numero = 0;
    e.numero = 0;
    e.tecla_a = false;
    e.solto_fuera = false;
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
            if (ev.button.button == SDL_BUTTON_LEFT)
                e.presionado = e.pulsado = true, e.salio_con_boton = false, e.doble_izquierdo = ev.button.clicks >= 2;
            if (ev.button.button == SDL_BUTTON_RIGHT) e.derecho_presionado = true, e.doble_derecho = ev.button.clicks >= 2;
            if (ev.button.button == SDL_BUTTON_MIDDLE) e.medio_presionado = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            a.gpu.a_logico(ev.button.x, ev.button.y, e.mx, e.my);
            if (ev.button.button == SDL_BUTTON_LEFT) {
                // SDL captura el mouse mientras hay un botón apretado (SDL_HINT_MOUSE_AUTO_CAPTURE, por defecto),
                // así que este evento llega aunque se suelte fuera de la ventana, con coordenadas fuera de ella.
                int vw = 0, vh = 0;
                SDL_GetWindowSize(a.ventana, &vw, &vh);
                e.solto_fuera = e.salio_con_boton || punto_fuera_de_ventana(ev.button.x, ev.button.y, vw, vh);
                e.salio_con_boton = false;
                e.presionado = false;
                e.clic = true;
            }
            if (ev.button.button == SDL_BUTTON_RIGHT) { e.derecho_presionado = false; e.clic_derecho = true; }
            if (ev.button.button == SDL_BUTTON_MIDDLE) { e.medio_presionado = false; e.clic_medio = true; }
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: e.salio_con_boton = e.presionado; break;
        case SDL_EVENT_MOUSE_WHEEL: e.rueda += ev.wheel.y > 0 ? 1 : (ev.wheel.y < 0 ? -1 : 0); break;
        case SDL_EVENT_TEXT_INPUT:
            if (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) break;  // atajos (zoom), no texto
            for (const char* p = ev.text.text; *p && e.ntexto < 63; ++p) e.texto[e.ntexto++] = *p;
            break;
        case SDL_EVENT_KEY_DOWN:
            switch (ev.key.key) {
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (ev.key.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI)) {  // Alt+Enter (Cmd+Enter en Mac): pantalla completa o ventana
                    if (!ev.key.repeat) {
                        alternar_pantalla_completa(a);
                        guardar_config(a.cfg);
                    }
                } else e.enter = true;
                break;
            case SDLK_F:  // Ctrl+Cmd+F: el atajo de pantalla completa de macOS
                if ((ev.key.mod & SDL_KMOD_GUI) && (ev.key.mod & SDL_KMOD_CTRL) && !ev.key.repeat) {
                    alternar_pantalla_completa(a);
                    guardar_config(a.cfg);
                }
                break;
            case SDLK_TAB: e.tab = true; break;
            case SDLK_ESCAPE: e.escape = true; break;
            case SDLK_BACKSPACE: e.retroceso = true; break;
            case SDLK_DELETE: e.suprimir = true; break;
            case SDLK_PAGEUP: e.pagina_arriba = true; break;
            case SDLK_PAGEDOWN: e.pagina_abajo = true; break;
            // El cliente 2009 abre sus ventanas con Ctrl+letra (WM_CHAR 1..26). Alt+I/C/S y A del original solo
            // valen en la interfaz simple; la interfaz original conserva solo las teclas originales.
            case SDLK_C: case SDLK_I: case SDLK_S: case SDLK_A: case SDLK_Q: case SDLK_H: case SDLK_V: case SDLK_P:
            case SDLK_T: case SDLK_O: case SDLK_X: case SDLK_Y: case SDLK_Z: {
                const SDL_Keymod m = ev.key.mod;
                if ((m & SDL_KMOD_ALT) && !(m & SDL_KMOD_CTRL)) {
                    if (!a.cfg.ui_simple) break;
                    if (ev.key.key == SDLK_C) e.alt_c = true;
                    else if (ev.key.key == SDLK_I) e.alt_i = true;
                    else if (ev.key.key == SDLK_S) e.alt_s = true;
                } else if ((m & SDL_KMOD_CTRL) && !(m & (SDL_KMOD_ALT | SDL_KMOD_SHIFT))) {
                    if (ev.key.repeat) break;
                    e.atajo_ctrl = static_cast<char>(ev.key.key - SDLK_A + 'A');
                } else if (a.cfg.ui_simple && ev.key.key == SDLK_A && !(m & (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_SHIFT))) {
                    e.tecla_a = true;
                }
                break;
            }
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
                if ((ev.key.mod & SDL_KMOD_CTRL) && !(ev.key.mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT)) && ev.key.key <= SDLK_8) {
                    // Cliente original: Ctrl+1..8 usan las mismas ocho casillas que F5..F12.
                    if (!ev.key.repeat) e.tecla_f = 5 + static_cast<int>(ev.key.key - SDLK_1);
                } else if ((ev.key.mod & SDL_KMOD_ALT) && !(ev.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)))
                    e.alt_numero = 1 + static_cast<int>(ev.key.key - SDLK_1);  // SDLK_1..SDLK_9 son consecutivos
                else if (!(ev.key.mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_GUI)))
                    e.numero = 1 + static_cast<int>(ev.key.key - SDLK_1);
                texto_antes = e.ntexto;
                break;
            case SDLK_F1:
                if (!ev.key.repeat && a.cfg.ui_simple) a.ayuda_teclas = !a.ayuda_teclas;  // original: F1 no es la lista de teclas
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
                if (!a.cfg.ui_simple && !(ev.key.mod & SDL_KMOD_SHIFT)) {  // original: recoge un objeto cercano
                    if (a.pantalla == Pantalla::JUEGO) pick_up_nearby(a);
                    break;
                }
                if (ev.key.repeat) break;
                // Cámara a saltos (como el original) o suave; con Shift, personajes oscurecidos en
                // la sombra o no.
                if (ev.key.mod & SDL_KMOD_SHIFT) a.cfg.sombra_personajes = !a.cfg.sombra_personajes;
                else a.cfg.camara_a_saltos = !a.cfg.camara_a_saltos;
                guardar_config(a.cfg);
                break;
            case SDLK_F12:
                if (ev.key.mod & SDL_KMOD_SHIFT) {
                    tomar_captura(a);
                    break;
                }
                e.tecla_f = 12;
                break;
            case SDLK_F5:
                if (ev.key.mod & SDL_KMOD_SHIFT) {  // Shift+F5: rota el filtro de escalado de la presentación final
                    if (ev.key.repeat) break;
                    const int sig = (static_cast<int>(escalado_desde(a.cfg.escalado)) + 1) % N_ESCALADOS;
                    a.cfg.escalado = nombre_escalado(static_cast<Escalado>(sig));
                    guardar_config(a.cfg);
                    {  // avisa al jugador qué filtro quedó activo
                        char t[48];
                        std::snprintf(t, sizeof t, "Upscaling: %s (Shift+F5)", a.cfg.escalado.c_str());
                        a.chat.agregar(t, rgba(255, 200, 120), CHAT_SISTEMA);
                    }
                    break;
                }
                e.tecla_f = 5;
                break;
            case SDLK_F6: case SDLK_F7: case SDLK_F8: case SDLK_F9: case SDLK_F10: case SDLK_F11:
                // Alt+F10 es WM_SYSKEYDOWN en el original y hace lo mismo que F10 (casilla 6).
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
    // El número apretado ahora (por su tecla, no su posición: igual que `numero`).
    e.numeros_mantenidos = 0;
    for (int i = 0; i < 10; ++i) {
        const SDL_Scancode s = SDL_GetScancodeFromKey(i < 9 ? SDLK_1 + static_cast<SDL_Keycode>(i) : SDLK_0, nullptr);
        if (s != SDL_SCANCODE_UNKNOWN && teclas[s]) e.numeros_mantenidos |= static_cast<u16>(1u << i);
    }
    e.derecha = teclas[SDL_SCANCODE_RIGHT];
    e.ctrl = (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
    e.shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
}

// Red: transiciones de estado y paquetes -> eventos -> pantallas y mundo.
void procesar_red(App& a) {
    atender_lanzador(a);
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
        eventos_pesca(a, e);
        eventos_tablon(a, e);
        eventos_correo(a, e);
        eventos_casas(a, e);
        eventos_mapa(a, e);
        eventos_castillos(a, e);
        if (e.tipo == Ev::SONIDO) a.audio.efecto(static_cast<u32>(e.v[0]), 1.0f, a.entrada.ahora_ms);  // S_Sound
        eventos_cantidad(a, e);
        eventos_reinicio(a, e);
        eventos_mascotas(a, e);
        eventos_cocina(a, e);
        eventos_marcadores(a, e);
        eventos_hechizos(a, e);
        eventos_autobuffs(a, e);
        eventos_autopocion(a, e);
        if (e.tipo == Ev::MAPA && (a.pantalla == Pantalla::JUEGO || a.pantalla == Pantalla::CARGANDO)) avisar_mapa(a, static_cast<MapId>(e.v[0]));
        // "Miss" sobre el personaje: un ataque que lo tiene de objetivo y no acierta (el daño recibido
        // se detecta por la caída de HP tras aplicar los eventos, abajo).
        // Someone attacks the player: combat music (0x4c654f, attacker = the object).
        if (e.tipo == Ev::ATAQUE && static_cast<ObjId>(e.v[AT_OBJETIVO]) == a.mundo.jugador.id && e.id != a.mundo.jugador.id)
            combate_musica(a, a.mundo.objetos.buscar(e.id), a.entrada.ahora_ms);
        if (a.cfg.texto_dano && e.tipo == Ev::ATAQUE && static_cast<ObjId>(e.v[AT_OBJETIVO]) == a.mundo.jugador.id &&
            !e.v[AT_GOLPE])
            empujar_texto_combate(a, "Miss", 230, 230, 230);
    }
    a.mundo.jugador.clase = clase_personaje(a);
    aplicar_eventos(a.mundo, a.eventos, a.entrada.ahora_ms);
    avanzar_colas(a.mundo, a.entrada.ahora_ms);
    a.red.en_juego(a.mundo.en_juego && a.pantalla == Pantalla::JUEGO);  // el keepalive lo manda el hilo de red
    if (a.cfg.texto_dano && a.mundo.jugador.hp_max > 0 && a.mundo.jugador.hp < a.hp_anterior) {
        char t[12];
        std::snprintf(t, sizeof t, "-%d", a.hp_anterior - a.mundo.jugador.hp);
        empujar_texto_combate(a, t, 255, 80, 60);
    }
    a.hp_anterior = a.mundo.jugador.hp;
    a.chat.original_ui = !a.cfg.ui_simple;
    a.chat.globo_comercio = a.cfg.opciones_orig >> OPC_MARKETMSG & 1;
    const Slot yo_chat = a.mundo.objetos.buscar(a.mundo.jugador.id);
    aplicar_chat(a.chat, a.mensajes, a.desc, a.eventos, a.entrada.ahora_ms,
                 yo_chat == SIN_SLOT ? -1 : a.mundo.objetos.col<O_X>()[yo_chat], yo_chat == SIN_SLOT ? -1 : a.mundo.objetos.col<O_Y>()[yo_chat]);
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
    if (!e.definido()) return;  // el comando no existe en esta versión del protocolo (C_Ship fuera de s3ep1...)
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
    // Automated relaunches (tests) must not steal the user's focus.
    if (SDL_getenv("LIN_NO_FOCUS")) SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
    a.ventana = SDL_CreateWindow("Lineage", a.cfg.ventana_ancho, a.cfg.ventana_alto,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!a.ventana || !a.gpu.iniciar(a.ventana, 640, 480)) return false;
    ui::poner_icono(a.ventana, "lineage.png");
    if (a.cfg.pantalla_completa) SDL_SetWindowFullscreen(a.ventana, true);
    a.ventana_sin_borde.redimensionable = true;
    activar_hit_test(a.ventana, a.gpu, a.ventana_sin_borde);
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
// Viajes en barco (s3ep1): el original manda C_Ship desde el reloj de su escena del barco, una vez por ciclo de 180 s reales
// (0x4a6630 -> 0x4a6750..0x4a6bc8): en los mapas 6 / 83 / 446 en el segundo 90 del ciclo, en los mapas 5 / 84 / 447 en el 0.
// El mapa de destino y una celda junto al muelle son fijos (el original suma un desvío al azar de 0-9 celdas; el servidor
// acepta +-20 celdas respecto de su propia tabla).
static void barco_llegada(App& a) {
    if (a.pantalla != Pantalla::JUEGO) return;
    struct Llegada { MapId desde; u16 destino, x, y; u32 segundo; };
    static constexpr Llegada L[] = {
        {5, 4, 32564, 32726, 0},      {6, 0, 32631, 32972, 90},      {83, 70, 32936, 33052, 90},
        {84, 4, 33426, 33489, 0},     {446, 445, 32748, 32855, 90},  {447, 440, 32296, 33066, 0},
    };
    static i64 ultimo = 0;
    const i64 t = static_cast<i64>(std::time(nullptr));
    if (t == ultimo) return;
    ultimo = t;
    for (const Llegada& l : L)
        if (a.mundo.mapa == l.desde && static_cast<u32>(t % 180) == l.segundo) {
            enviar(a, cmd::barco(a.op, l.destino, l.x, l.y));
            return;
        }
}

static void cerrar_medicion(App& a) {
    a.fps = a.frames_fps * 1000.0f / static_cast<float>(a.entrada.ahora_ms - a.t_fps);
    a.ms_cpu = static_cast<float>(a.acum_cpu / a.frames_fps);
    a.ms_cpu_max = static_cast<float>(a.max_cpu);
    a.ms_decod = a.g ? static_cast<float>(a.g->tomar_ms_decodificacion()) : 0;
    a.decodificados = a.g ? a.g->tomar_decodificados() : 0;
    a.asign_frame_max = a.max_asign;
    a.asign_total = a.acum_asign;
    if (a.perf)
        LIN_LOG("perf: %.0f fps  gpu %.2f ms  cpu %.2f ms (max %.2f)  decod %.2f ms (%u)  asign %llu (max/frame %llu)  inst %u  pag %u (reemplazos %u)  precarga %u  efectos %u (wav %u)  musica %d",
                a.fps, a.gpu.tomar_ms_gpu(), a.ms_cpu, a.ms_cpu_max, a.ms_decod, a.decodificados, static_cast<unsigned long long>(a.asign_total),
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
        // Pantallas originales de login / personajes: la flecha (1, 0x474695 / 0x474c00), el cursor de espera (2) mientras hay
        // un pedido en curso (0x470476..0x472680).
        if (!a.cfg.ui_simple && a.pantalla != Pantalla::JUEGO) {
            const Cursor c = cursor_estado(a.esperando ? 2 : 1);
            if (a.cursores[static_cast<int>(c)]) a.cursor = c;
        }
        if (a.g) a.g->nuevo_frame();
        // Dentro del juego la vista se amplía con la ventana (y el zoom); el resto, 640×480. Con la interfaz simple el
        // zoom es solo del mapa: la interfaz conserva su tamaño.
        a.gpu.fijar_escalado(escalado_desde(a.cfg.escalado));
        a.gpu.fijar_texto_hd(a.cfg.texto_hd);
        a.gpu.medir_gpu(a.perf);
        a.gpu.fijar_vista_fija(a.cfg.vista != "amplia");
        a.gpu.ajustar_vista(a.pantalla == Pantalla::JUEGO || a.pantalla == Pantalla::CARGANDO, a.cfg.zoom, alto_barra(a),
                            a.cfg.ui_simple);
#if LIN_CONTROL
        control_frame(a);  // comandos del socket: antes de leer la entrada (las teclas van como eventos de SDL)
#endif
        leer_entrada(a);
#if LIN_CONTROL
        control_entrada(a);  // el mouse inyectado
#endif
        procesar_red(a);
        // Ajustes abiertos (modales): la pantalla de atrás sigue (red, mundo, dibujo) con una entrada vacía, y
        // la del frame queda para los ajustes, que se dibujan encima.
        const bool ajustes = a.ventana_ajustes && a.pantalla != Pantalla::ELEGIR_CARPETA;
        const EntradaFrame entrada = a.entrada;
        if (ajustes) {
            EntradaFrame vacia;
            vacia.ahora_ms = entrada.ahora_ms;
            vacia.mx = vacia.my = -10000;
            a.entrada = vacia;
        }
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
        barco_llegada(a);
        if (ajustes) {
            a.entrada = entrada;
            dibujar_ajustes(a);
        }
        actualizar_sonido(a, a.entrada.ahora_ms);
        emblemas_guardar(a, a.entrada.ahora_ms, false);
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
    a.lanzador.parar();  // el proxy del lanzador ya no hace falta (espera a lo sumo Lanzador::PLAZO_STOP_MS)
    a.audio.cerrar();
    emblemas_guardar(a, a.entrada.ahora_ms, true);
    a.g.reset();
    destruir_cursores(a);
    a.gpu.cerrar();
    if (a.ventana) SDL_DestroyWindow(a.ventana);
    SDL_Quit();
}

}  // namespace lin
