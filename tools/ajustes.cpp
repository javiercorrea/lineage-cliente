// Editor visual de settings.ini: formulario con pestañas (Juego, Interfaz, Audio, Servidores,
// Personajes) dibujado con la misma UI simple del juego (recuadros, cajas de texto y desplegables de
// lin_ui). Se usa antes de jugar; escribe el mismo archivo que lee el cliente. No necesita el juego.
//
// Uso: ajustes [--config=RUTA]   (por defecto, el settings.ini del usuario)
// Tab: siguiente campo de texto. Enter: cierra el campo. Esc: sale sin guardar (en un desplegable, lo cierra).
// Desarrollo: --pestana=N (0–4) y --captura=x.bmp (guarda una imagen a los 30 frames y sale).
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "plataforma/rutas.h"
#include "render/graficos.h"
#include "ui/ui.h"

using namespace lin;

namespace {

constexpr int VENTANA_W = 1280, VENTANA_H = 960;  // la lógica es 640×480, como el juego
constexpr int LOGICO_W = 640, LOGICO_H = 480;
constexpr int X_ETIQUETA = 32, X_CAMPO = 250, ANCHO_CAMPO = 340, Y_PRIMERA = 78, PASO = 28;
const u32 COLOR_ERROR = rgba(255, 120, 100), COLOR_OK = rgba(170, 230, 150);
const std::vector<std::string> PESTANAS = {"Juego", "Interfaz", "Audio", "Servidores", "Personajes"};
const std::vector<std::string> PROTOCOLOS = {"jp363", "tw380"};  // los que entiende el cliente (opcodes.cpp)
// Idiomas del juego: nombre en su propio idioma, y el código que va a settings.ini ("" = el del sistema).
const std::vector<std::string> IDIOMAS_NOMBRES = {"Automático (sistema)", "Español", "English", "Português", "Deutsch", "Français"};
const std::vector<std::string> IDIOMAS_CODIGOS = {"", "es", "en", "pt", "de", "fr"};

// --- Carpeta del juego: diálogo nativo. Es asíncrono; el callback corre en el hilo principal. ---
std::string g_carpeta_elegida;
bool g_carpeta_nueva = false;  // hay un resultado del diálogo sin mirar
void SDLCALL al_elegir_carpeta(void*, const char* const* lista, int) {
    g_carpeta_elegida = (lista && lista[0]) ? lista[0] : "";
    g_carpeta_nueva = true;
}

// Sin carpeta del juego válida no hay fuente de texto: se pide, como hace el juego en el primer arranque.
bool elegir_carpeta_inicial(SDL_Window* v, std::string& carpeta) {
    g_carpeta_nueva = false;
    SDL_ShowOpenFolderDialog(al_elegir_carpeta, nullptr, v, nullptr, false);
    while (!g_carpeta_nueva) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_EVENT_QUIT) return false;
        SDL_Delay(16);
    }
    carpeta = g_carpeta_elegida;
    return !carpeta.empty();
}

// --- Datos del formulario ----------------------------------------------------------------------

// La configuración, más los campos numéricos como texto (se convierten al guardar).
struct Formulario {
    Config cfg;
    std::string ancho, alto, zoom, opacidad, musica, efectos, puerto;
    size_t sv = 0;         // servidor que se edita (pestaña Servidores)
    size_t personaje = 0;  // personaje de la pestaña Personajes
    std::vector<std::string> protocolos = PROTOCOLOS;
};

size_t posicion(const std::vector<std::string>& v, const std::string& x) {
    const auto it = std::find(v.begin(), v.end(), x);
    return it == v.end() ? 0 : static_cast<size_t>(it - v.begin());
}

// Número entero de la cadena, dentro de [min, max]. Solo dígitos: sin signos ni espacios.
bool entero(const std::string& t, int min, int max, int& out) {
    if (t.empty() || t.size() > 6) return false;
    if (!std::all_of(t.begin(), t.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }))
        return false;
    out = std::stoi(t);
    return out >= min && out <= max;
}

void cargar_campos(Formulario& f) {
    const Config& c = f.cfg;
    f.ancho = std::to_string(c.ventana_ancho);
    f.alto = std::to_string(c.ventana_alto);
    f.zoom = std::to_string(c.zoom);
    f.opacidad = std::to_string(c.opacidad_ventanas);
    f.musica = std::to_string(c.volumen_musica);
    f.efectos = std::to_string(c.volumen_efectos);
    f.sv = std::min(f.sv, c.servidores.size() - 1);  // leer_config siempre deja al menos un servidor
    f.puerto = std::to_string(c.servidores[f.sv].puerto);
    for (const Servidor& s : c.servidores)  // un protocolo desconocido se muestra igual, para no perderlo
        if (std::find(f.protocolos.begin(), f.protocolos.end(), s.protocolo) == f.protocolos.end())
            f.protocolos.push_back(s.protocolo);
}

// Pasa los campos a la configuración. Devuelve el primer error ("" si todo está bien).
std::string volcar(Formulario& f) {
    Config& c = f.cfg;
    int n = 0;
    if (!entero(f.ancho, 320, 7680, n)) return "Ventana: el ancho debe estar entre 320 y 7680";
    c.ventana_ancho = n;
    if (!entero(f.alto, 240, 4320, n)) return "Ventana: el alto debe estar entre 240 y 4320";
    c.ventana_alto = n;
    if (!entero(f.zoom, 0, 16, n)) return "Zoom: entre 0 (automatico) y 16";
    c.zoom = n;
    if (!entero(f.opacidad, 0, 255, n)) return "Opacidad de ventanas: entre 0 y 255";
    c.opacidad_ventanas = n;
    if (!entero(f.musica, 0, 100, n)) return "Volumen de musica: entre 0 y 100";
    c.volumen_musica = n;
    if (!entero(f.efectos, 0, 100, n)) return "Volumen de efectos: entre 0 y 100";
    c.volumen_efectos = n;
    if (!entero(f.puerto, 1, 65535, n)) return "Servidor: el puerto debe estar entre 1 y 65535";
    c.servidores[f.sv].puerto = static_cast<u16>(n);
    return "";
}

// Guarda el puerto del servidor que se editaba y pasa a otro.
void cambiar_servidor(Formulario& f, size_t nuevo) {
    int n = 0;
    if (entero(f.puerto, 1, 65535, n)) f.cfg.servidores[f.sv].puerto = static_cast<u16>(n);
    f.sv = nuevo;
    f.puerto = std::to_string(f.cfg.servidores[f.sv].puerto);
}

void agregar_servidor(Formulario& f) {
    Servidor s;
    s.nombre = "Nuevo";
    s.host = "127.0.0.1";
    f.cfg.servidores.push_back(s);
    cambiar_servidor(f, f.cfg.servidores.size() - 1);
}

void quitar_servidor(Formulario& f) {
    if (f.cfg.servidores.size() < 2) return;  // siempre queda uno
    const size_t quitado = f.sv;
    f.cfg.servidores.erase(f.cfg.servidores.begin() + static_cast<std::ptrdiff_t>(quitado));
    if (f.cfg.servidor == quitado) f.cfg.servidor = 0;
    else if (f.cfg.servidor > quitado) --f.cfg.servidor;
    f.sv = std::min(quitado, f.cfg.servidores.size() - 1);
    f.puerto = std::to_string(f.cfg.servidores[f.sv].puerto);
}

// --- Pantalla: dibujo y entrada de un frame ----------------------------------------------------

struct Pantalla {
    Graficos& g;
    EntradaFrame& e;
    SDL_Window* ventana;
    Pantalla(Graficos& g_, EntradaFrame& e_, SDL_Window* v) : g(g_), e(e_), ventana(v) {}
    int foco = -1;                    // campo de texto con foco (su número dentro de la pestaña)
    int campos = 0, campos_antes = 0;  // campos de texto de este frame y del anterior (para Tab)
    bool abre_sv = false, abre_proto = false, abre_ini = false, abre_pj = false, abre_idioma = false;  // desplegables abiertos
    std::string aviso;  // lo que pasó con el último botón; sale en la línea de abajo
    bool aviso_error = false;
};

void estado(Pantalla& p, std::string t, bool error) {
    p.aviso = std::move(t);
    p.aviso_error = error;
}

void etiqueta(Pantalla& p, int y, std::string_view t) {
    p.g.texto(t, X_ETIQUETA, y + 4, clave_orden(CAPA_UI_TEXTO, 0), ui::COLOR_TEXTO);
}

void texto(Pantalla& p, int y, std::string_view t, u32 color = ui::COLOR_TEXTO) {
    p.g.texto(t, X_ETIQUETA, y + 4, clave_orden(CAPA_UI_TEXTO, 0), color);
}

void campo(Pantalla& p, int y, std::string_view nombre, std::string& valor, size_t maximo) {
    const int id = p.campos++;
    etiqueta(p, y, nombre);
    if (p.e.clic && ui::sobre(p.e, X_CAMPO - 4, y - 4, ANCHO_CAMPO + 8, 24)) p.foco = id;
    if (ui::campo(p.g, p.e, valor, X_CAMPO, y, ANCHO_CAMPO, p.foco == id, false, maximo)) p.foco = -1;  // Enter
}

void si_no(Pantalla& p, int y, std::string_view nombre, bool& valor) {
    etiqueta(p, y, nombre);
    if (ui::boton_texto(p.g, p.e, valor ? "Si" : "No", X_CAMPO, y, 90, 20)) valor = !valor;
}

// Desplegable con etiqueta. Devuelve true si cambió el elegido.
bool lista(Pantalla& p, int y, std::string_view nombre, const std::vector<std::string>& opciones, size_t& elegido,
           bool& abierto) {
    etiqueta(p, y, nombre);
    return ui::desplegable(p.g, p.e, opciones, elegido, abierto, X_CAMPO, y, ANCHO_CAMPO);
}

bool boton(Pantalla& p, int x, int y, int w, std::string_view t) { return ui::boton_texto(p.g, p.e, t, x, y, w, 22); }

int fila(int i) { return Y_PRIMERA + i * PASO; }

// --- Pestañas ----------------------------------------------------------------------------------

void pestana_juego(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    campo(p, fila(0), "Carpeta del juego", c.carpeta_juego, 300);
    if (boton(p, X_CAMPO, fila(1), 160, "Buscar carpeta...")) SDL_ShowOpenFolderDialog(al_elegir_carpeta, nullptr, p.ventana, nullptr, false);
    if (carpeta_juego_valida(c.carpeta_juego)) texto(p, fila(2), "Carpeta correcta: estan los .pak del juego.", COLOR_OK);
    else texto(p, fila(2), "Aqui no estan Sprite.pak, Text.pak y Tile.pak.", COLOR_ERROR);
    si_no(p, fila(3), "Camara a saltos (F4)", c.camara_a_saltos);
    si_no(p, fila(4), "Enter abre el chat", c.chat_con_enter);
    si_no(p, fila(5), "Sombra en personajes", c.sombra_personajes);
    si_no(p, fila(6), "Compensar ataque", c.compensar_ataque);
    si_no(p, fila(7), "Texto de dano", c.texto_dano);
    si_no(p, fila(8), "Barra HP/MP sobre el personaje", c.barra_personaje);
}

void pestana_interfaz(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    si_no(p, fila(0), "Interfaz simple (F2)", c.ui_simple);
    campo(p, fila(1), "Opacidad de ventanas (0-255)", f.opacidad, 3);
    campo(p, fila(2), "Logo PNG del login (vacio = texto)", c.logo, 200);
    campo(p, fila(3), "Ventana: ancho", f.ancho, 4);
    campo(p, fila(4), "Ventana: alto", f.alto, 4);
    campo(p, fila(5), "Zoom (0 = automatico)", f.zoom, 2);
    // Idioma del juego: "" = el del sistema. Se aplica al abrir el juego, no en vivo.
    size_t idx = 0;
    for (size_t i = 0; i < IDIOMAS_CODIGOS.size(); ++i)
        if (IDIOMAS_CODIGOS[i] == c.idioma) idx = i;
    if (lista(p, fila(6), "Idioma del juego", IDIOMAS_NOMBRES, idx, p.abre_idioma)) c.idioma = IDIOMAS_CODIGOS[idx];
}

void pestana_audio(Pantalla& p, Formulario& f) {
    campo(p, fila(0), "Musica (0-100)", f.musica, 3);
    campo(p, fila(1), "Efectos (0-100)", f.efectos, 3);
}

void pestana_servidores(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    std::vector<std::string> nombres;
    for (const Servidor& s : c.servidores) nombres.push_back(s.nombre);
    size_t a_editar = f.sv;
    if (lista(p, fila(0), "Servidor a editar", nombres, a_editar, p.abre_sv)) cambiar_servidor(f, a_editar);
    if (boton(p, X_CAMPO, fila(1), 100, "Agregar")) agregar_servidor(f);
    if (boton(p, X_CAMPO + 110, fila(1), 100, "Quitar")) quitar_servidor(f);
    Servidor& sv = c.servidores[f.sv];
    campo(p, fila(2), "Nombre", sv.nombre, 32);
    campo(p, fila(3), "Host (IP o nombre)", sv.host, 64);
    campo(p, fila(4), "Puerto", f.puerto, 5);
    size_t proto = posicion(f.protocolos, sv.protocolo);
    if (lista(p, fila(5), "Protocolo", f.protocolos, proto, p.abre_proto)) sv.protocolo = f.protocolos[proto];
    campo(p, fila(6), "Cuenta (la ultima usada)", sv.usuario, 32);
    size_t entrar = c.servidor;
    if (lista(p, fila(7), "Servidor al entrar", nombres, entrar, p.abre_ini)) c.servidor = entrar;
}

void pestana_personajes(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    const std::vector<std::string> nombres = personajes_con_datos(c);
    if (nombres.empty()) {
        texto(p, fila(0), "Todavia no hay datos guardados por personaje.");
        texto(p, fila(1), "Se crean al asignar hechizos a F5-F12 o a la barra Alt+1..0.");
    } else {
        f.personaje = std::min(f.personaje, nombres.size() - 1);
        size_t elegido = f.personaje;
        if (lista(p, fila(0), "Personaje", nombres, elegido, p.abre_pj)) f.personaje = elegido;
        const std::string& nombre = nombres[f.personaje];
        texto(p, fila(1), valor_de_personaje(c.atajos, nombre).empty() ? "Atajos F5-F12: ninguno" : "Atajos F5-F12: guardados");
        texto(p, fila(2), valor_de_personaje(c.barra, nombre).empty() ? "Barra Alt+1..0: ninguna" : "Barra Alt+1..0: guardada");
        if (boton(p, X_CAMPO, fila(3), 160, "Quitar atajos F5-F12")) {
            quitar_valor_de_personaje(c.atajos, nombre);
            estado(p, nombre + ": atajos quitados (al pulsar Guardar)", false);
        }
        if (boton(p, X_CAMPO + 170, fila(3), 160, "Quitar barra")) {
            quitar_valor_de_personaje(c.barra, nombre);
            estado(p, nombre + ": barra quitada (al pulsar Guardar)", false);
        }
    }
    texto(p, fila(5), c.ventanas.empty() ? "Posiciones de ventanas: por defecto" : "Posiciones de ventanas: guardadas");
    if (boton(p, X_CAMPO, fila(6), 220, "Restablecer posiciones")) {
        c.ventanas.clear();
        estado(p, "Posiciones de ventanas restablecidas (al pulsar Guardar)", false);
    }
}

// Barra de botones de abajo: guardar, recargar el archivo y salir.
void barra_inferior(Pantalla& p, Formulario& f, const std::string& ruta, bool& salir) {
    if (boton(p, 180, 440, 110, "Guardar")) {
        const std::string error = volcar(f);
        if (!error.empty()) estado(p, error, true);
        else if (guardar_config_en(ruta, f.cfg)) estado(p, "Guardado. El juego lo usa al arrancar.", false);
        else estado(p, "No se pudo escribir el archivo de ajustes.", true);
    }
    if (boton(p, 300, 440, 110, "Revertir")) {
        f.cfg = cargar_config_de(ruta);
        cargar_campos(f);
        estado(p, "Se volvieron a leer los ajustes guardados.", false);
    }
    if (boton(p, 420, 440, 110, "Salir")) salir = true;
}

// --- Entrada y bucle ---------------------------------------------------------------------------

void leer_entrada(Gpu& gpu, EntradaFrame& e, bool& salir) {
    e.clic = e.enter = e.tab = e.escape = e.retroceso = false;
    e.ntexto = 0;
    e.ahora_ms = SDL_GetTicks();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT: salir = true; break;
        case SDL_EVENT_MOUSE_MOTION: gpu.a_logico(ev.motion.x, ev.motion.y, e.mx, e.my); break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            gpu.a_logico(ev.button.x, ev.button.y, e.mx, e.my);
            if (ev.button.button == SDL_BUTTON_LEFT) e.presionado = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            gpu.a_logico(ev.button.x, ev.button.y, e.mx, e.my);
            if (ev.button.button == SDL_BUTTON_LEFT) {
                e.presionado = false;
                e.clic = true;
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) break;  // atajos, no texto
            for (const char* p = ev.text.text; *p && e.ntexto < 63; ++p) e.texto[e.ntexto++] = *p;
            break;
        case SDL_EVENT_KEY_DOWN:
            switch (ev.key.key) {
            case SDLK_RETURN: case SDLK_KP_ENTER: e.enter = true; break;
            case SDLK_TAB: e.tab = true; break;
            case SDLK_ESCAPE: e.escape = true; break;
            case SDLK_BACKSPACE: e.retroceso = true; break;
            default: break;
            }
            break;
        default: break;
        }
    }
}

std::string ruta_de_argumentos(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a.rfind("--config=", 0) == 0) return a.substr(9);
    }
    return ruta_configuracion();
}

}  // namespace

int main(int argc, char** argv) {
    const std::string ruta = ruta_de_argumentos(argc, argv);
    std::string captura;
    size_t pestana = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a.rfind("--captura=", 0) == 0) captura = a.substr(10);
        if (a.rfind("--pestana=", 0) == 0) pestana = std::min<size_t>(std::atoi(a.c_str() + 10), PESTANAS.size() - 1);
    }

    Formulario f;
    f.cfg = cargar_config_de(ruta);
    cargar_campos(f);

    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window* v = SDL_CreateWindow("Ajustes del cliente Lineage", VENTANA_W, VENTANA_H,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    Gpu gpu;
    if (!v || !gpu.iniciar(v, LOGICO_W, LOGICO_H)) {
        SDL_Quit();
        return 1;
    }
    SDL_StartTextInput(v);

    while (!carpeta_juego_valida(f.cfg.carpeta_juego)) {
        std::string elegida;
        if (!elegir_carpeta_inicial(v, elegida)) {
            gpu.cerrar();
            SDL_DestroyWindow(v);
            SDL_Quit();
            return 1;
        }
        f.cfg.carpeta_juego = elegida + "/";
    }

    Recursos rec;
    FuenteBitmap fuente;
    if (!rec.abrir(f.cfg.carpeta_juego) || !fuente.cargar(f.cfg.carpeta_juego)) {
        LIN_LOG("No se pudo abrir la fuente en %s", f.cfg.carpeta_juego.c_str());
        gpu.cerrar();
        SDL_DestroyWindow(v);
        SDL_Quit();
        return 1;
    }
    auto g = std::make_unique<Graficos>(gpu, rec, fuente);
    EntradaFrame entrada;
    Pantalla p(*g, entrada, v);
    bool salir = false;

    for (int frame = 0; !salir; ++frame) {
        g->nuevo_frame();
        leer_entrada(gpu, entrada, salir);
        if (g_carpeta_nueva) {  // resultado de "Buscar carpeta..."
            g_carpeta_nueva = false;
            const std::string nueva = g_carpeta_elegida + "/";
            if (!g_carpeta_elegida.empty() && carpeta_juego_valida(nueva)) estado(p, "Carpeta del juego cambiada.", false);
            else if (!g_carpeta_elegida.empty()) estado(p, "Esa carpeta no tiene los .pak del juego.", true);
            if (carpeta_juego_valida(nueva)) f.cfg.carpeta_juego = nueva;
        }

        p.campos_antes = p.campos;
        p.campos = 0;
        if (entrada.clic) p.foco = -1;  // un clic fuera de un campo le quita el foco
        if (entrada.tab && p.campos_antes > 0) p.foco = (p.foco + 1) % p.campos_antes;

        gpu.rectangulo(0, 0, LOGICO_W, LOGICO_H, clave_orden(CAPA_FONDO, 0), rgba(24, 20, 16));
        ui::texto_centrado(*g, "AJUSTES DEL CLIENTE", 0, 10, LOGICO_W, ui::COLOR_TITULO);
        for (size_t i = 0; i < PESTANAS.size(); ++i) {
            const std::string t = i == pestana ? "> " + PESTANAS[i] + " <" : PESTANAS[i];
            if (ui::boton_texto(*g, entrada, t, 16 + static_cast<int>(i) * 122, 34, 118, 22)) {
                pestana = i;
                p.foco = -1;
            }
        }
        ui::recuadro(*g, 12, 62, 616, 340);
        switch (pestana) {
        case 0: pestana_juego(p, f); break;
        case 1: pestana_interfaz(p, f); break;
        case 2: pestana_audio(p, f); break;
        case 3: pestana_servidores(p, f); break;
        default: pestana_personajes(p, f); break;
        }
        barra_inferior(p, f, ruta, salir);
        if (!p.aviso.empty()) ui::texto_centrado(*g, p.aviso, 0, 412, LOGICO_W, p.aviso_error ? COLOR_ERROR : COLOR_OK);

        if (entrada.escape) salir = true;  // un desplegable abierto ya consumió su Esc
        if (!captura.empty() && frame == 30) {
            gpu.pedir_captura(captura);
            salir = true;
        }
        gpu.presentar();
    }

    g.reset();
    gpu.cerrar();
    SDL_DestroyWindow(v);
    SDL_Quit();
    return 0;
}
