// Editor visual de settings.ini: formulario con pestañas (Juego, Interfaz, Audio, Servidores,
// Personajes) dibujado con la misma UI simple del juego (recuadros, cajas de texto y desplegables de
// lin_ui). Se usa antes de jugar; escribe el mismo archivo que lee el cliente. No necesita el juego.
// Los textos salen del catálogo (i18n/textos.h) en el idioma del juego; al cambiar el selector de
// idioma, el editor cambia de idioma en vivo.
//
// Uso: ajustes [--config=RUTA]   (por defecto, el settings.ini del usuario)
// Tab: siguiente campo de texto. Enter: cierra el campo. Esc: sale sin guardar (en un desplegable, lo cierra).
// Desarrollo: --pestana=N (0–4) y --captura=x.bmp (guarda una imagen a los 30 frames y sale).
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "i18n/textos.h"
#include "plataforma/rutas.h"
#include "render/graficos.h"
#include "ui/ui.h"

using namespace lin;

namespace {

constexpr int LOGICO_W = 640, LOGICO_H = 480;  // la ventana mide esto en puntos: lo que se dibuja, sin sobrar
constexpr int X_ETIQUETA = 32, X_CAMPO = 250, ANCHO_CAMPO = 340, Y_PRIMERA = 78, PASO = 28;
constexpr int CAJA_X = 12, CAJA_Y = 62, CAJA_ANCHO = 616, CAJA_ALTO = 340;  // recuadro del contenido
const u32 COLOR_ERROR = rgba(255, 120, 100), COLOR_OK = rgba(170, 230, 150);
const std::vector<std::string> PROTOCOLOS = {"jp363", "tw380"};  // los que entiende el cliente (opcodes.cpp)
// Idiomas del juego: el código que va a settings.ini ("" = el del sistema). El nombre de cada uno se
// escribe en su propio idioma, salvo la primera opción, que sigue el idioma activo.
const std::vector<std::string> IDIOMAS_CODIGOS = {"", "es", "en", "pt", "de", "fr"};

std::vector<std::string> pestanas() {
    return {tr(Tx::AJ_PESTANA_JUEGO), tr(Tx::AJ_PESTANA_INTERFAZ), tr(Tx::AJ_PESTANA_AUDIO),
            tr(Tx::AJ_PESTANA_SERVIDORES), tr(Tx::AJ_PESTANA_PERSONAJES)};
}

std::vector<std::string> idiomas_nombres() {
    return {tr(Tx::AJ_IDIOMA_AUTO), "Español", "English", "Português", "Deutsch", "Français"};
}

// Pone el idioma del editor: el de settings.ini, o el del sistema si está vacío o no lo tenemos.
void aplicar_idioma(const std::string& codigo) {
    Idioma i = Idioma::EN;
    fijar_idioma(idioma_de_codigo(codigo, i) ? i : idioma_del_sistema());
}

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
    if (!entero(f.ancho, 320, 7680, n)) return tr(Tx::AJ_ERR_ANCHO);
    c.ventana_ancho = n;
    if (!entero(f.alto, 240, 4320, n)) return tr(Tx::AJ_ERR_ALTO);
    c.ventana_alto = n;
    if (!entero(f.zoom, 0, 16, n)) return tr(Tx::AJ_ERR_ZOOM);
    c.zoom = n;
    if (!entero(f.opacidad, 0, 255, n)) return tr(Tx::AJ_ERR_OPACIDAD);
    c.opacidad_ventanas = n;
    if (!entero(f.musica, 0, 100, n)) return tr(Tx::AJ_ERR_MUSICA);
    c.volumen_musica = n;
    if (!entero(f.efectos, 0, 100, n)) return tr(Tx::AJ_ERR_EFECTOS);
    c.volumen_efectos = n;
    if (!entero(f.puerto, 1, 65535, n)) return tr(Tx::AJ_ERR_PUERTO);
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
    s.nombre = tr(Tx::AJ_NUEVO_NOMBRE);
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
    // Scroll del contenido de la pestaña: las filas se corren `desplaza` px; lo que queda fuera de la caja no se dibuja.
    int desplaza = 0, desplaza_max = 0;  // el tope sale del alto del frame anterior
    int alto_contenido = 0;              // alto de las filas de este frame, sin desplazar
    bool en_tab = false;                 // dibujando el contenido de la pestaña
};

void estado(Pantalla& p, std::string t, bool error) {
    p.aviso = std::move(t);
    p.aviso_error = error;
}

// Una fila que cae fuera de la caja (o de la parte que se ve) no se dibuja ni recibe clics.
bool oculto(const Pantalla& p, int y, int alto) {
    return p.en_tab && (y < CAJA_Y + 4 || y + alto > CAJA_Y + CAJA_ALTO - 4);
}

void etiqueta(Pantalla& p, int y, std::string_view t) {
    if (oculto(p, y, 20)) return;
    p.g.texto(t, X_ETIQUETA, y + 4, clave_orden(CAPA_UI_TEXTO, 0), ui::COLOR_TEXTO);
}

void texto(Pantalla& p, int y, std::string_view t, u32 color = ui::COLOR_TEXTO) {
    if (oculto(p, y, 20)) return;
    p.g.texto(t, X_ETIQUETA, y + 4, clave_orden(CAPA_UI_TEXTO, 0), color);
}

void campo(Pantalla& p, int y, std::string_view nombre, std::string& valor, size_t maximo) {
    const int id = p.campos++;
    etiqueta(p, y, nombre);
    if (oculto(p, y, 24)) return;
    if (p.e.clic && ui::sobre(p.e, X_CAMPO - 4, y - 4, ANCHO_CAMPO + 8, 24)) p.foco = id;
    if (ui::campo(p.g, p.e, valor, X_CAMPO, y, ANCHO_CAMPO, p.foco == id, false, maximo)) p.foco = -1;  // Enter
}

void si_no(Pantalla& p, int y, std::string_view nombre, bool& valor) {
    etiqueta(p, y, nombre);
    if (oculto(p, y, 20)) return;
    if (ui::boton_texto(p.g, p.e, valor ? tr(Tx::AJ_SI) : tr(Tx::AJ_NO), X_CAMPO, y, 90, 20)) valor = !valor;
}

// Desplegable con etiqueta. Devuelve true si cambió el elegido.
bool lista(Pantalla& p, int y, std::string_view nombre, const std::vector<std::string>& opciones, size_t& elegido,
           bool& abierto) {
    etiqueta(p, y, nombre);
    if (oculto(p, y, 24)) return false;
    return ui::desplegable(p.g, p.e, opciones, elegido, abierto, X_CAMPO, y, ANCHO_CAMPO);
}

bool boton(Pantalla& p, int x, int y, int w, std::string_view t) {
    if (oculto(p, y, 22)) return false;
    return ui::boton_texto(p.g, p.e, t, x, y, w, 22);
}

// Fila i de la pestaña: marca hasta dónde llega el contenido (para el tope del scroll).
int fila(Pantalla& p, int i) {
    p.alto_contenido = std::max(p.alto_contenido, Y_PRIMERA + (i + 1) * PASO);
    return Y_PRIMERA + i * PASO - p.desplaza;
}

// --- Pestañas ----------------------------------------------------------------------------------

void pestana_juego(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    campo(p, fila(p, 0), tr(Tx::AJ_CARPETA), c.carpeta_juego, 300);
    if (boton(p, X_CAMPO, fila(p, 1), 160, tr(Tx::AJ_BUSCAR))) SDL_ShowOpenFolderDialog(al_elegir_carpeta, nullptr, p.ventana, nullptr, false);
    if (carpeta_juego_valida(c.carpeta_juego)) texto(p, fila(p, 2), tr(Tx::AJ_CARPETA_OK), COLOR_OK);
    else texto(p, fila(p, 2), tr(Tx::AJ_CARPETA_MAL), COLOR_ERROR);
    si_no(p, fila(p, 3), tr(Tx::AJ_CAMARA), c.camara_a_saltos);
    si_no(p, fila(p, 4), tr(Tx::AJ_CHAT_ENTER), c.chat_con_enter);
    si_no(p, fila(p, 5), tr(Tx::AJ_SOMBRA), c.sombra_personajes);
    si_no(p, fila(p, 6), tr(Tx::AJ_COMPENSAR), c.compensar_ataque);
    si_no(p, fila(p, 7), tr(Tx::AJ_TEXTO_DANO), c.texto_dano);
    si_no(p, fila(p, 8), tr(Tx::AJ_BARRA_PJ), c.barra_personaje);
    si_no(p, fila(p, 9), tr(Tx::AJ_EFECTO_LUZ), c.efecto_luz);
    si_no(p, fila(p, 10), tr(Tx::AJ_EFECTO_AGUA), c.efecto_agua_tinte);
    si_no(p, fila(p, 11), tr(Tx::AJ_EFECTO_OLA), c.efecto_agua_ola);
    si_no(p, fila(p, 12), tr(Tx::AJ_EFECTO_INFRA), c.efecto_infravision);
}

void pestana_interfaz(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    si_no(p, fila(p, 0), tr(Tx::AJ_UI_SIMPLE), c.ui_simple);
    si_no(p, fila(p, 1), tr(Tx::AJ_FPS), c.mostrar_fps);
    campo(p, fila(p, 2), tr(Tx::AJ_OPACIDAD), f.opacidad, 3);
    campo(p, fila(p, 3), tr(Tx::AJ_LOGO), c.logo, 200);
    campo(p, fila(p, 4), tr(Tx::AJ_VENTANA_ANCHO), f.ancho, 4);
    campo(p, fila(p, 5), tr(Tx::AJ_VENTANA_ALTO), f.alto, 4);
    campo(p, fila(p, 6), tr(Tx::AJ_ZOOM), f.zoom, 2);
    // Idioma del juego. Se guarda con Guardar; el editor ya cambia de idioma al elegirlo.
    const std::vector<std::string> nombres = idiomas_nombres();
    size_t idx = 0;
    for (size_t i = 0; i < IDIOMAS_CODIGOS.size(); ++i)
        if (IDIOMAS_CODIGOS[i] == c.idioma) idx = i;
    if (lista(p, fila(p, 7), tr(Tx::AJ_IDIOMA), nombres, idx, p.abre_idioma)) {
        c.idioma = IDIOMAS_CODIGOS[idx];
        aplicar_idioma(c.idioma);
    }
    si_no(p, fila(p, 8), tr(Tx::AJ_PANTALLA_COMPLETA), c.pantalla_completa);
}

void pestana_audio(Pantalla& p, Formulario& f) {
    campo(p, fila(p, 0), tr(Tx::AJ_MUSICA), f.musica, 3);
    campo(p, fila(p, 1), tr(Tx::AJ_EFECTOS), f.efectos, 3);
}

void pestana_servidores(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    std::vector<std::string> nombres;
    for (const Servidor& s : c.servidores) nombres.push_back(s.nombre);
    size_t a_editar = f.sv;
    if (lista(p, fila(p, 0), tr(Tx::AJ_SERVIDOR_EDITAR), nombres, a_editar, p.abre_sv)) cambiar_servidor(f, a_editar);
    if (boton(p, X_CAMPO, fila(p, 1), 100, tr(Tx::AJ_AGREGAR))) agregar_servidor(f);
    if (boton(p, X_CAMPO + 110, fila(p, 1), 100, tr(Tx::AJ_QUITAR))) quitar_servidor(f);
    Servidor& sv = c.servidores[f.sv];
    campo(p, fila(p, 2), tr(Tx::AJ_NOMBRE), sv.nombre, 32);
    campo(p, fila(p, 3), tr(Tx::AJ_HOST), sv.host, 64);
    campo(p, fila(p, 4), tr(Tx::AJ_PUERTO), f.puerto, 5);
    size_t proto = posicion(f.protocolos, sv.protocolo);
    if (lista(p, fila(p, 5), tr(Tx::AJ_PROTOCOLO), f.protocolos, proto, p.abre_proto)) sv.protocolo = f.protocolos[proto];
    campo(p, fila(p, 6), tr(Tx::AJ_CUENTA), sv.usuario, 32);
    size_t entrar = c.servidor;
    if (lista(p, fila(p, 7), tr(Tx::AJ_SERVIDOR_ENTRAR), nombres, entrar, p.abre_ini)) c.servidor = entrar;
}

void pestana_personajes(Pantalla& p, Formulario& f) {
    Config& c = f.cfg;
    const std::vector<std::string> nombres = personajes_con_datos(c);
    char buf[160];
    if (nombres.empty()) {
        texto(p, fila(p, 0), tr(Tx::AJ_SIN_DATOS));
        texto(p, fila(p, 1), tr(Tx::AJ_SIN_DATOS_2));
    } else {
        f.personaje = std::min(f.personaje, nombres.size() - 1);
        size_t elegido = f.personaje;
        if (lista(p, fila(p, 0), tr(Tx::AJ_PERSONAJE), nombres, elegido, p.abre_pj)) f.personaje = elegido;
        const std::string& nombre = nombres[f.personaje];
        texto(p, fila(p, 1), valor_de_personaje(c.atajos, nombre).empty() ? tr(Tx::AJ_ATAJOS_NO) : tr(Tx::AJ_ATAJOS_SI));
        texto(p, fila(p, 2), valor_de_personaje(c.barra, nombre).empty() ? tr(Tx::AJ_BARRA_NO) : tr(Tx::AJ_BARRA_SI));
        if (boton(p, X_CAMPO, fila(p, 3), 160, tr(Tx::AJ_QUITAR_ATAJOS))) {
            quitar_valor_de_personaje(c.atajos, nombre);
            std::snprintf(buf, sizeof buf, tr(Tx::AJ_ATAJOS_QUITADOS), nombre.c_str());
            estado(p, buf, false);
        }
        if (boton(p, X_CAMPO + 170, fila(p, 3), 160, tr(Tx::AJ_QUITAR_BARRA))) {
            quitar_valor_de_personaje(c.barra, nombre);
            std::snprintf(buf, sizeof buf, tr(Tx::AJ_BARRA_QUITADA), nombre.c_str());
            estado(p, buf, false);
        }
    }
    texto(p, fila(p, 5), c.ventanas.empty() ? tr(Tx::AJ_VENTANAS_DEF) : tr(Tx::AJ_VENTANAS_GUARD));
    if (boton(p, X_CAMPO, fila(p, 6), 220, tr(Tx::AJ_RESTABLECER))) {
        c.ventanas.clear();
        estado(p, tr(Tx::AJ_VENTANAS_RESET), false);
    }
}

// Barra de botones de abajo: guardar, recargar el archivo y salir.
void barra_inferior(Pantalla& p, Formulario& f, const std::string& ruta, bool& salir) {
    // Tres botones centrados, cada uno tan ancho como su texto (mínimo 110 px): "Annuler les modifications" entra.
    auto ancho_de = [](Tx clave) {
        const std::string_view t = tr(clave);
        return std::max(110, ancho_texto(t.data(), t.size()) + 24);
    };
    const int w1 = ancho_de(Tx::AJ_GUARDAR), w2 = ancho_de(Tx::AJ_REVERTIR), w3 = ancho_de(Tx::AJ_SALIR);
    const int x1 = (LOGICO_W - (w1 + w2 + w3 + 20)) / 2, x2 = x1 + w1 + 10, x3 = x2 + w2 + 10;
    if (boton(p, x1, 440, w1, tr(Tx::AJ_GUARDAR))) {
        const std::string error = volcar(f);
        if (!error.empty()) estado(p, error, true);
        else if (guardar_config_en(ruta, f.cfg)) estado(p, tr(Tx::AJ_GUARDADO), false);
        else estado(p, tr(Tx::AJ_NO_ESCRIBE), true);
    }
    if (boton(p, x2, 440, w2, tr(Tx::AJ_REVERTIR))) {
        f.cfg = cargar_config_de(ruta);
        cargar_campos(f);
        aplicar_idioma(f.cfg.idioma);
        estado(p, tr(Tx::AJ_RELEIDO), false);
    }
    if (boton(p, x3, 440, w3, tr(Tx::AJ_SALIR))) salir = true;
}

// --- Entrada y bucle ---------------------------------------------------------------------------

// La barra de arriba arrastra la ventana, salvo sus botones (ui::prueba_barra). No cambia de tamaño.
SDL_HitTestResult zona_ajustes(SDL_Window* w, const SDL_Point* p, void* datos) {
    return ui::prueba_barra(w, *static_cast<const Gpu*>(datos), p, false);
}

void leer_entrada(Gpu& gpu, EntradaFrame& e, bool& salir, int& rueda) {
    e.clic = e.enter = e.tab = e.escape = e.retroceso = false;
    e.ntexto = 0;
    rueda = 0;
    e.ahora_ms = SDL_GetTicks();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT: salir = true; break;
        case SDL_EVENT_MOUSE_WHEEL: rueda += static_cast<int>(ev.wheel.y); break;  // positivo: hacia arriba
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
    const size_t cantidad_pestanas = pestanas().size();
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a.rfind("--captura=", 0) == 0) captura = a.substr(10);
        if (a.rfind("--pestana=", 0) == 0) pestana = std::min<size_t>(std::atoi(a.c_str() + 10), cantidad_pestanas - 1);
    }

    Formulario f;
    f.cfg = cargar_config_de(ruta);
    cargar_campos(f);
    aplicar_idioma(f.cfg.idioma);  // antes de crear la ventana: el título también sale en el idioma

    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window* v = SDL_CreateWindow(tr(Tx::AJ_TITULO_VENTANA), LOGICO_W, LOGICO_H,
                                     SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    Gpu gpu;
    if (!v || !gpu.iniciar(v, LOGICO_W, LOGICO_H)) {
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowHitTest(v, zona_ajustes, &gpu);
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
        int rueda = 0;
        leer_entrada(gpu, entrada, salir, rueda);
        if (g_carpeta_nueva) {  // resultado de "Buscar carpeta..."
            g_carpeta_nueva = false;
            const std::string nueva = g_carpeta_elegida + "/";
            if (!g_carpeta_elegida.empty() && carpeta_juego_valida(nueva)) estado(p, tr(Tx::AJ_CARPETA_CAMBIADA), false);
            else if (!g_carpeta_elegida.empty()) estado(p, tr(Tx::AJ_CARPETA_NO), true);
            if (carpeta_juego_valida(nueva)) f.cfg.carpeta_juego = nueva;
        }

        p.desplaza = std::clamp(p.desplaza - rueda * PASO, 0, p.desplaza_max);
        p.alto_contenido = 0;
        p.campos_antes = p.campos;
        p.campos = 0;
        if (entrada.clic) p.foco = -1;  // un clic fuera de un campo le quita el foco
        if (entrada.tab && p.campos_antes > 0) p.foco = (p.foco + 1) % p.campos_antes;

        const std::vector<std::string> nombres_pestanas = pestanas();
        gpu.rectangulo(0, 0, LOGICO_W, LOGICO_H, clave_orden(CAPA_FONDO, 0), rgba(24, 20, 16));
        bool cerrar = false, minimizar = false;
        ui::barra_ventana(*g, entrada, tr(Tx::AJ_TITULO), 0, 0, LOGICO_W, cerrar, minimizar);
        if (minimizar) SDL_MinimizeWindow(v);
        if (cerrar) salir = true;
        for (size_t i = 0; i < nombres_pestanas.size(); ++i) {
            const std::string t = i == pestana ? "> " + nombres_pestanas[i] + " <" : nombres_pestanas[i];
            if (ui::boton_texto(*g, entrada, t, 16 + static_cast<int>(i) * 122, 34, 118, 22)) {
                pestana = i;
                p.foco = -1;
                p.desplaza = 0;
            }
        }
        ui::recuadro(*g, CAJA_X, CAJA_Y, CAJA_ANCHO, CAJA_ALTO);
        p.en_tab = true;
        switch (pestana) {
        case 0: pestana_juego(p, f); break;
        case 1: pestana_interfaz(p, f); break;
        case 2: pestana_audio(p, f); break;
        case 3: pestana_servidores(p, f); break;
        default: pestana_personajes(p, f); break;
        }
        p.en_tab = false;
        // Tope del scroll para el próximo frame: lo que sobra de la caja (el último renglón deja su margen).
        p.desplaza_max = std::max(0, p.alto_contenido - (CAJA_Y + CAJA_ALTO - 4 - Y_PRIMERA));
        if (p.desplaza_max > 0) {
            const int x_flechas = CAJA_X + CAJA_ANCHO - 22;
            if (ui::boton_texto(*g, entrada, "^", x_flechas, CAJA_Y + 4, 18, 18)) p.desplaza -= PASO;
            if (ui::boton_texto(*g, entrada, "v", x_flechas, CAJA_Y + CAJA_ALTO - 22, 18, 18)) p.desplaza += PASO;
            p.desplaza = std::clamp(p.desplaza, 0, p.desplaza_max);
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
