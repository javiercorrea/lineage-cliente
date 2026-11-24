// Ventana de ajustes: el formulario con pestañas (Juego, Interfaz, Audio, Servidores, Personajes) que antes
// era el programa aparte "Lineage Settings". Se abre desde el login (botón Ajustes / Options del original) y en
// el juego (botón Options). Es modal: mientras está abierta, la pantalla de atrás se dibuja pero no recibe
// entrada (app.cpp). Edita una copia de la configuración; Guardar la aplica en vivo y la escribe en
// settings.ini. La carpeta del juego y la tabla de sprites se leen al arrancar: si cambian, se avisa que
// valen al reiniciar el cliente.
// Tab: siguiente campo de texto. Enter: cierra el campo. Esc: cierra sin guardar (en un desplegable, lo cierra).
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include "app/app.h"
#include "i18n/textos.h"

namespace lin {

namespace {

constexpr int ANCHO = 640, ALTO = 480;  // el formulario mide lo que medía la ventana del programa aparte
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

// El idioma de la interfaz: el de settings.ini, o el del sistema si está vacío o no lo tenemos.
void aplicar_idioma(const std::string& codigo) {
    Idioma i = Idioma::EN;
    fijar_idioma(idioma_de_codigo(codigo, i) ? i : idioma_del_sistema());
}

// "Buscar carpeta...": el diálogo nativo es asíncrono; el callback corre en el hilo principal.
std::string g_carpeta_ajustes;
bool g_carpeta_ajustes_nueva = false;
void SDLCALL al_elegir_carpeta_ajustes(void*, const char* const* lista, int) {
    g_carpeta_ajustes = (lista && lista[0]) ? lista[0] : "";
    g_carpeta_ajustes_nueva = true;
}

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

}  // namespace

// La configuración que se edita, más los campos numéricos como texto (se convierten al guardar), y el estado
// de la ventana (pestaña, foco, desplegables, scroll, aviso).
struct EstadoAjustes {
    Config cfg;
    std::string ancho, alto, zoom, opacidad, musica, efectos, puerto, paginas, ms_mantener;
    std::string extras;  // carpetas_extra (juego_extra=), separadas por ';'
    size_t sv = 0;         // servidor que se edita (pestaña Servidores)
    size_t personaje = 0;  // personaje de la pestaña Personajes
    std::vector<std::string> protocolos = PROTOCOLOS;
    size_t pestana = 0;
    int foco = -1;                     // campo de texto con foco (su número dentro de la pestaña)
    int campos = 0, campos_antes = 0;  // campos de texto de este frame y del anterior (para Tab)
    bool abre_sv = false, abre_proto = false, abre_ini = false, abre_pj = false, abre_idioma = false, abre_umbral = false,
         abre_escalado = false, abre_vista = false, abre_alpha = false;
    std::string aviso;  // lo que pasó con el último botón; sale en la línea de abajo
    bool aviso_error = false;
    // Scroll del contenido de la pestaña: las filas se corren `desplaza` px; lo que queda fuera de la caja no se dibuja.
    int desplaza = 0, desplaza_max = 0;  // el tope sale del alto del frame anterior
    int alto_contenido = 0;              // alto de las filas de este frame, sin desplazar
    bool en_tab = false;                 // dibujando el contenido de la pestaña
};

namespace {

// Lo que se dibuja en un frame: el estado, la app y el origen del formulario (centrado en la vista).
struct Formulario {
    EstadoAjustes& s;
    Graficos& g;
    EntradaFrame& e;
    App& a;
    int ox, oy;
};

void cargar_campos(EstadoAjustes& f) {
    const Config& c = f.cfg;
    f.ancho = std::to_string(c.ventana_ancho);
    f.alto = std::to_string(c.ventana_alto);
    f.zoom = std::to_string(c.zoom);
    f.opacidad = std::to_string(c.opacidad_ventanas);
    f.paginas = std::to_string(c.paginas_barra);
    f.ms_mantener = std::to_string(c.ms_mantener_tecla);
    f.extras.clear();
    for (const std::string& e : c.carpetas_extra) (f.extras += f.extras.empty() ? "" : ";") += e;
    f.musica = std::to_string(c.volumen_musica);
    f.efectos = std::to_string(c.volumen_efectos);
    f.sv = std::min(f.sv, c.servidores.size() - 1);  // leer_config siempre deja al menos un servidor
    f.puerto = std::to_string(c.servidores[f.sv].puerto);
    for (const Servidor& s : c.servidores)  // un protocolo desconocido se muestra igual, para no perderlo
        if (std::find(f.protocolos.begin(), f.protocolos.end(), s.protocolo) == f.protocolos.end())
            f.protocolos.push_back(s.protocolo);
}

// Pasa los campos a la configuración. Devuelve el primer error ("" si todo está bien).
std::string volcar(EstadoAjustes& f) {
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
    if (!entero(f.paginas, 1, PAGINAS_BARRA_MAX, n)) return tr(Tx::AJ_ERR_PAGINAS);
    c.paginas_barra = n;
    if (!entero(f.ms_mantener, MS_MANTENER_MIN, MS_MANTENER_MAX, n)) return tr(Tx::AJ_ERR_MS_MANTENER);
    c.ms_mantener_tecla = n;
    c.carpetas_extra.clear();  // una carpeta por ';', con su '/' final como al leer settings.ini
    for (std::string_view v = f.extras; !v.empty();) {
        std::string_view e = v.substr(0, v.find(';'));
        v.remove_prefix(std::min(e.size() + 1, v.size()));
        while (!e.empty() && e.front() == ' ') e.remove_prefix(1);
        while (!e.empty() && e.back() == ' ') e.remove_suffix(1);
        if (e.empty()) continue;
        c.carpetas_extra.emplace_back(e);
        if (c.carpetas_extra.back().back() != '/') c.carpetas_extra.back() += '/';
    }
    if (!entero(f.musica, 0, 100, n)) return tr(Tx::AJ_ERR_MUSICA);
    c.volumen_musica = n;
    if (!entero(f.efectos, 0, 100, n)) return tr(Tx::AJ_ERR_EFECTOS);
    c.volumen_efectos = n;
    // Con lanzador el puerto solo nombra al servidor: ahí vale 0.
    if (!entero(f.puerto, c.servidores[f.sv].lanzador.empty() ? 1 : 0, 65535, n)) return tr(Tx::AJ_ERR_PUERTO);
    c.servidores[f.sv].puerto = static_cast<u16>(n);
    return "";
}

// Guarda el puerto del servidor que se editaba y pasa a otro.
void cambiar_servidor(EstadoAjustes& f, size_t nuevo) {
    int n = 0;
    // El mismo mínimo que volcar: un servidor sin lanzador no se queda con puerto 0 al cambiar a otro antes de guardar.
    if (entero(f.puerto, f.cfg.servidores[f.sv].lanzador.empty() ? 1 : 0, 65535, n)) f.cfg.servidores[f.sv].puerto = static_cast<u16>(n);
    f.sv = nuevo;
    f.puerto = std::to_string(f.cfg.servidores[f.sv].puerto);
}

void agregar_servidor(EstadoAjustes& f) {
    Servidor s;
    s.nombre = tr(Tx::AJ_NUEVO_NOMBRE);
    s.host = "127.0.0.1";
    f.cfg.servidores.push_back(s);
    cambiar_servidor(f, f.cfg.servidores.size() - 1);
}

void quitar_servidor(EstadoAjustes& f) {
    if (f.cfg.servidores.size() < 2) return;  // siempre queda uno
    const size_t quitado = f.sv;
    f.cfg.servidores.erase(f.cfg.servidores.begin() + static_cast<std::ptrdiff_t>(quitado));
    if (f.cfg.servidor == quitado) f.cfg.servidor = 0;
    else if (f.cfg.servidor > quitado) --f.cfg.servidor;
    f.sv = std::min(quitado, f.cfg.servidores.size() - 1);
    f.puerto = std::to_string(f.cfg.servidores[f.sv].puerto);
}

void estado(Formulario& p, std::string t, bool error) {
    p.s.aviso = std::move(t);
    p.s.aviso_error = error;
}

// Una fila que cae fuera de la caja (o de la parte que se ve) no se dibuja ni recibe clics.
bool oculto(const Formulario& p, int y, int alto) {
    return p.s.en_tab && (y < p.oy + CAJA_Y + 4 || y + alto > p.oy + CAJA_Y + CAJA_ALTO - 4);
}

void etiqueta(Formulario& p, int y, std::string_view t, u32 color = ui::COLOR_TEXTO) {
    if (oculto(p, y, 20)) return;
    p.g.texto(t, p.ox + X_ETIQUETA, y + 4, clave_orden(CAPA_UI_TEXTO, 0), color);
}

void campo(Formulario& p, int y, std::string_view nombre, std::string& valor, size_t maximo) {
    const int id = p.s.campos++;
    etiqueta(p, y, nombre);
    if (oculto(p, y, 24)) return;
    const int x = p.ox + X_CAMPO;
    if (p.e.clic && ui::sobre(p.e, x - 4, y - 4, ANCHO_CAMPO + 8, 24)) p.s.foco = id;
    if (ui::campo(p.g, p.e, valor, x, y, ANCHO_CAMPO, p.s.foco == id, false, maximo)) p.s.foco = -1;  // Enter
}

void si_no(Formulario& p, int y, std::string_view nombre, bool& valor) {
    etiqueta(p, y, nombre);
    if (oculto(p, y, 20)) return;
    if (ui::boton_texto(p.g, p.e, valor ? tr(Tx::AJ_SI) : tr(Tx::AJ_NO), p.ox + X_CAMPO, y, 90, 20)) valor = !valor;
}

// Desplegable con etiqueta. Devuelve true si cambió el elegido.
bool lista(Formulario& p, int y, std::string_view nombre, const std::vector<std::string>& opciones, size_t& elegido,
           bool& abierto) {
    etiqueta(p, y, nombre);
    if (oculto(p, y, 24)) return false;
    return ui::desplegable(p.g, p.e, opciones, elegido, abierto, p.ox + X_CAMPO, y, ANCHO_CAMPO);
}

// Botón en x relativo al formulario.
bool boton(Formulario& p, int x, int y, int w, std::string_view t) {
    if (oculto(p, y, 22)) return false;
    return ui::boton_texto(p.g, p.e, t, p.ox + x, y, w, 22);
}

// Fila i de la pestaña: marca hasta dónde llega el contenido (para el tope del scroll).
int fila(Formulario& p, int i) {
    p.s.alto_contenido = std::max(p.s.alto_contenido, Y_PRIMERA + (i + 1) * PASO);
    return p.oy + Y_PRIMERA + i * PASO - p.s.desplaza;
}

// --- Pestañas ----------------------------------------------------------------------------------

void pestana_juego(Formulario& p) {
    Config& c = p.s.cfg;
    campo(p, fila(p, 0), tr(Tx::AJ_CARPETA), c.carpeta_juego, 300);
    if (boton(p, X_CAMPO, fila(p, 1), 160, tr(Tx::AJ_BUSCAR)))
        SDL_ShowOpenFolderDialog(al_elegir_carpeta_ajustes, nullptr, p.a.ventana, nullptr, false);
    if (carpeta_juego_valida(c.carpeta_juego)) etiqueta(p, fila(p, 2), tr(Tx::AJ_CARPETA_OK), COLOR_OK);
    else etiqueta(p, fila(p, 2), tr(Tx::AJ_CARPETA_MAL), COLOR_ERROR);
    campo(p, fila(p, 3), tr(Tx::AJ_CARPETAS_EXTRA), p.s.extras, 600);
    campo(p, fila(p, 4), tr(Tx::AJ_LISTA_SPRITES), c.lista_sprites, 300);
    campo(p, fila(p, 5), tr(Tx::AJ_PACK_HD), c.carpeta_hd, 300);
    si_no(p, fila(p, 6), tr(Tx::AJ_CAMARA), c.camara_a_saltos);
    si_no(p, fila(p, 7), tr(Tx::AJ_CHAT_ENTER), c.chat_con_enter);
    si_no(p, fila(p, 8), tr(Tx::AJ_SOMBRA), c.sombra_personajes);
    si_no(p, fila(p, 9), tr(Tx::AJ_COMPENSAR), c.compensar_ataque);
    si_no(p, fila(p, 10), tr(Tx::AJ_TEXTO_DANO), c.texto_dano);
    si_no(p, fila(p, 11), tr(Tx::AJ_BARRA_PJ), c.barra_personaje);
    si_no(p, fila(p, 12), tr(Tx::AJ_EFECTO_LUZ), c.efecto_luz);
    si_no(p, fila(p, 13), tr(Tx::AJ_EFECTO_AGUA), c.efecto_agua_tinte);
    si_no(p, fila(p, 14), tr(Tx::AJ_EFECTO_OLA), c.efecto_agua_ola);
    si_no(p, fila(p, 15), tr(Tx::AJ_EFECTO_INFRA), c.efecto_infravision);
}

void pestana_interfaz(Formulario& p) {
    EstadoAjustes& f = p.s;
    Config& c = f.cfg;
    si_no(p, fila(p, 0), tr(Tx::AJ_UI_SIMPLE), c.ui_simple);
    si_no(p, fila(p, 1), tr(Tx::AJ_FPS), c.mostrar_fps);
    si_no(p, fila(p, 2), tr(Tx::AJ_TEXTO_HD), c.texto_hd);
    si_no(p, fila(p, 3), tr(Tx::AJ_GLOBOS), c.globos_comic);
    campo(p, fila(p, 4), tr(Tx::AJ_OPACIDAD), f.opacidad, 3);
    campo(p, fila(p, 5), tr(Tx::AJ_LOGO), c.logo, 200);
    campo(p, fila(p, 6), tr(Tx::AJ_VENTANA_ANCHO), f.ancho, 4);
    campo(p, fila(p, 7), tr(Tx::AJ_VENTANA_ALTO), f.alto, 4);
    campo(p, fila(p, 8), tr(Tx::AJ_ZOOM), f.zoom, 2);
    // Idioma: la ventana ya cambia al elegirlo; se guarda con Guardar (cerrar sin guardar vuelve al anterior).
    const std::vector<std::string> nombres = idiomas_nombres();
    size_t idx = 0;
    for (size_t i = 0; i < IDIOMAS_CODIGOS.size(); ++i)
        if (IDIOMAS_CODIGOS[i] == c.idioma) idx = i;
    if (lista(p, fila(p, 9), tr(Tx::AJ_IDIOMA), nombres, idx, f.abre_idioma)) {
        c.idioma = IDIOMAS_CODIGOS[idx];
        aplicar_idioma(c.idioma);
    }
    si_no(p, fila(p, 10), tr(Tx::AJ_PANTALLA_COMPLETA), c.pantalla_completa);
    campo(p, fila(p, 11), tr(Tx::AJ_PAGINAS_BARRA), f.paginas, 1);
    campo(p, fila(p, 12), tr(Tx::AJ_MS_MANTENER), f.ms_mantener, 4);
    si_no(p, fila(p, 13), tr(Tx::AJ_USAR_AUTOBUFFS), c.usar_autobuffs);
    si_no(p, fila(p, 14), tr(Tx::AJ_USAR_AUTOPOCION), c.usar_autopocion);
    si_no(p, fila(p, 15), tr(Tx::AJ_FUENTE_ORIGINAL), c.fuente_original);
    // Filtro final de escalado (Shift+F5) y vista: fija de 640x480 escalada a la ventana, o amplia.
    static const std::vector<std::string> ESCALADOS = [] {
        std::vector<std::string> v;
        for (int i = 0; i < N_ESCALADOS; ++i) v.emplace_back(nombre_escalado(static_cast<Escalado>(i)));
        return v;
    }();
    size_t esc = static_cast<size_t>(escalado_desde(c.escalado));
    if (lista(p, fila(p, 16), tr(Tx::AJ_ESCALADO), ESCALADOS, esc, f.abre_escalado)) c.escalado = ESCALADOS[esc];
    static const std::vector<std::string> VISTAS = {"fija", "amplia"};
    size_t vista = c.vista == "amplia" ? 1 : 0;
    if (lista(p, fila(p, 17), tr(Tx::AJ_VISTA), VISTAS, vista, f.abre_vista)) c.vista = VISTAS[vista];
    // Transparencia de las ventanas de la interfaz original: los 4 pasos del cliente (opaca, 20/31, 13/31, 7/31).
    static const std::vector<std::string> ALPHAS = {"0", "1", "2", "3"};
    size_t alpha = static_cast<size_t>(std::clamp(c.alpha_orig, 0, 3));
    if (lista(p, fila(p, 18), tr(Tx::AJ_ALPHA_ORIG), ALPHAS, alpha, f.abre_alpha)) c.alpha_orig = static_cast<int>(alpha);
}

void pestana_audio(Formulario& p) {
    campo(p, fila(p, 0), tr(Tx::AJ_MUSICA), p.s.musica, 3);
    campo(p, fila(p, 1), tr(Tx::AJ_EFECTOS), p.s.efectos, 3);
}

void pestana_servidores(Formulario& p) {
    EstadoAjustes& f = p.s;
    Config& c = f.cfg;
    std::vector<std::string> nombres;
    for (const Servidor& s : c.servidores) nombres.push_back(s.nombre);
    size_t a_editar = f.sv;
    if (lista(p, fila(p, 0), tr(Tx::AJ_SERVIDOR_EDITAR), nombres, a_editar, f.abre_sv)) cambiar_servidor(f, a_editar);
    if (boton(p, X_CAMPO, fila(p, 1), 100, tr(Tx::AJ_AGREGAR))) agregar_servidor(f);
    if (boton(p, X_CAMPO + 110, fila(p, 1), 100, tr(Tx::AJ_QUITAR))) quitar_servidor(f);
    Servidor& sv = c.servidores[f.sv];
    campo(p, fila(p, 2), tr(Tx::AJ_NOMBRE), sv.nombre, 32);
    campo(p, fila(p, 3), tr(Tx::AJ_HOST), sv.host, 64);
    campo(p, fila(p, 4), tr(Tx::AJ_PUERTO), f.puerto, 5);
    size_t proto = posicion(f.protocolos, sv.protocolo);
    if (lista(p, fila(p, 5), tr(Tx::AJ_PROTOCOLO), f.protocolos, proto, f.abre_proto)) sv.protocolo = f.protocolos[proto];
    campo(p, fila(p, 6), tr(Tx::AJ_CUENTA), sv.usuario, 32);
    campo(p, fila(p, 7), tr(Tx::AJ_LANZADOR), sv.lanzador, 256);  // comando que abre un proxy local (Servidor::lanzador)
    size_t entrar = c.servidor;
    if (lista(p, fila(p, 8), tr(Tx::AJ_SERVIDOR_ENTRAR), nombres, entrar, f.abre_ini)) c.servidor = entrar;
}

void pestana_personajes(Formulario& p) {
    EstadoAjustes& f = p.s;
    Config& c = f.cfg;
    const std::vector<std::string> nombres = personajes_con_datos(c);
    char buf[160];
    if (nombres.empty()) {
        etiqueta(p, fila(p, 0), tr(Tx::AJ_SIN_DATOS));
        etiqueta(p, fila(p, 1), tr(Tx::AJ_SIN_DATOS_2));
    } else {
        f.personaje = std::min(f.personaje, nombres.size() - 1);
        size_t elegido = f.personaje;
        if (lista(p, fila(p, 0), tr(Tx::AJ_PERSONAJE), nombres, elegido, f.abre_pj)) f.personaje = elegido;
        const std::string& nombre = nombres[f.personaje];
        etiqueta(p, fila(p, 1), valor_de_personaje(c.atajos, nombre).empty() ? tr(Tx::AJ_ATAJOS_NO) : tr(Tx::AJ_ATAJOS_SI));
        etiqueta(p, fila(p, 2), valor_de_personaje(c.barra, nombre).empty() ? tr(Tx::AJ_BARRA_NO) : tr(Tx::AJ_BARRA_SI));
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
        etiqueta(p, fila(p, 4), valor_de_personaje(c.autobuffs, nombre).empty() ? tr(Tx::AJ_AUTOBUFFS_NO) : tr(Tx::AJ_AUTOBUFFS_SI));
        if (boton(p, X_CAMPO + 170, fila(p, 4), 160, tr(Tx::AJ_QUITAR_AUTOBUFFS))) {
            quitar_valor_de_personaje(c.autobuffs, nombre);
            std::snprintf(buf, sizeof buf, tr(Tx::AJ_AUTOBUFFS_QUITADOS), nombre.c_str());
            estado(p, buf, false);
        }
        // Orden del inventario (Sort o arrastrar a otra celda): sin él, vuelve el del servidor al entrar.
        etiqueta(p, fila(p, 5), valor_de_personaje(c.inventario_orden, nombre).empty() ? tr(Tx::AJ_ORDEN_NO) : tr(Tx::AJ_ORDEN_SI));
        if (boton(p, X_CAMPO + 170, fila(p, 5), 160, tr(Tx::AJ_QUITAR_ORDEN))) {
            quitar_valor_de_personaje(c.inventario_orden, nombre);
            std::snprintf(buf, sizeof buf, tr(Tx::AJ_ORDEN_QUITADO), nombre.c_str());
            estado(p, buf, false);
        }
        // Auto-poción del personaje (sin entrada: apagada con 60 %).
        OpcionPocion o = autopocion_de(c, nombre);
        bool encendida = o.encendida;
        si_no(p, fila(p, 6), tr(Tx::AJ_AUTOPOCION), encendida);
        static const std::vector<std::string> UMBRALES = [] {
            std::vector<std::string> v;
            for (int u = UMBRAL_POCION_MIN; u <= UMBRAL_POCION_MAX; u += 5) v.push_back(std::to_string(u) + " %");
            return v;
        }();
        size_t umbral = static_cast<size_t>((o.umbral - UMBRAL_POCION_MIN) / 5);
        const bool otro_umbral = lista(p, fila(p, 7), tr(Tx::AJ_UMBRAL_POCION), UMBRALES, umbral, f.abre_umbral);
        if (encendida != o.encendida || otro_umbral) {
            o.encendida = encendida;
            o.umbral = UMBRAL_POCION_MIN + static_cast<int>(umbral) * 5;
            poner_autopocion(c, nombre, o);
        }
    }
    etiqueta(p, fila(p, 8), c.ventanas.empty() ? tr(Tx::AJ_VENTANAS_DEF) : tr(Tx::AJ_VENTANAS_GUARD));
    if (boton(p, X_CAMPO, fila(p, 9), 220, tr(Tx::AJ_RESTABLECER))) {
        c.ventanas.clear();
        estado(p, tr(Tx::AJ_VENTANAS_RESET), false);
    }
}

// Pasa la configuración editada al juego: lo que se puede, en vivo. Devuelve true si cambió algo que solo
// se lee al arrancar (las carpetas del juego, el pack HD y la tabla de sprites: los recursos ya están cargados).
bool aplicar(App& a, const Config& nueva) {
    const Config vieja = a.cfg;
    a.cfg = nueva;
    if (a.ventana && nueva.pantalla_completa != vieja.pantalla_completa) SDL_SetWindowFullscreen(a.ventana, nueva.pantalla_completa);
    if (a.ventana && !nueva.pantalla_completa &&
        (nueva.ventana_ancho != vieja.ventana_ancho || nueva.ventana_alto != vieja.ventana_alto))
        SDL_SetWindowSize(a.ventana, nueva.ventana_ancho, nueva.ventana_alto);
    aplicar_idioma(nueva.idioma);
    aplicar_volumenes(a);
    usar_fuente_original(nueva.fuente_original);  // el escalado, la vista y alpha_orig se leen de a.cfg en cada frame
    if (nueva.chat_con_enter != vieja.chat_con_enter) a.escribiendo_chat = false;
    if (nueva.ventanas.empty() && !vieja.ventanas.empty())
        for (PosVentana& p : a.pos_ventanas) p = {};
    // Los datos por personaje (barra, atajos, auto-buffs, auto-poción) se vuelven a leer de la configuración
    // nueva la próxima vez que se usan: el personaje en juego toma lo que se quitó o cambió aquí.
    a.barra_de.clear();
    a.atajos_de.clear();
    a.autobuffs.de.clear();
    a.autopocion.de.clear();
    return nueva.carpeta_juego != vieja.carpeta_juego || nueva.lista_sprites != vieja.lista_sprites ||
           nueva.carpetas_extra != vieja.carpetas_extra || nueva.carpeta_hd != vieja.carpeta_hd;
}

void cerrar(App& a) {
    aplicar_idioma(a.cfg.idioma);  // un idioma elegido sin guardar no queda
    a.ventana_ajustes = false;
    a.ajustes.reset();
}

// Botones de abajo: guardar (y aplicar), volver a lo guardado y cerrar.
void barra_inferior(Formulario& p) {
    EstadoAjustes& f = p.s;
    // Tres botones centrados, cada uno tan ancho como su texto (mínimo 110 px): "Annuler les modifications" entra.
    auto ancho_de = [](Tx clave) {
        const std::string_view t = tr(clave);
        return std::max(110, ancho_texto(t.data(), t.size()) + 24);
    };
    const int w1 = ancho_de(Tx::AJ_GUARDAR), w2 = ancho_de(Tx::AJ_REVERTIR), w3 = ancho_de(Tx::AJ_CERRAR);
    const int x1 = (ANCHO - (w1 + w2 + w3 + 20)) / 2, x2 = x1 + w1 + 10, x3 = x2 + w2 + 10;
    const int y = p.oy + 440;
    if (boton(p, x1, y, w1, tr(Tx::AJ_GUARDAR))) {
        const std::string error = volcar(f);
        if (!error.empty()) {
            estado(p, error, true);
        } else {
            const bool reiniciar = aplicar(p.a, f.cfg);
            if (!guardar_config(p.a.cfg)) estado(p, tr(Tx::AJ_NO_ESCRIBE), true);
            else estado(p, tr(reiniciar ? Tx::AJ_GUARDADO_REINICIO : Tx::AJ_GUARDADO), false);
        }
    }
    if (boton(p, x2, y, w2, tr(Tx::AJ_REVERTIR))) {
        f.cfg = p.a.cfg;
        cargar_campos(f);
        aplicar_idioma(f.cfg.idioma);
        estado(p, tr(Tx::AJ_RELEIDO), false);
    }
    if (boton(p, x3, y, w3, tr(Tx::AJ_CERRAR))) cerrar(p.a);
}

}  // namespace

void dibujar_ajustes(App& a) {
    if (!a.ventana_ajustes || !a.g) return;
    if (!a.ajustes) {  // al abrir: una copia de la configuración en uso
        a.ajustes = std::make_shared<EstadoAjustes>();
        a.ajustes->cfg = a.cfg;
        cargar_campos(*a.ajustes);
    }
    EstadoAjustes& f = *a.ajustes;
    EntradaFrame& e = a.entrada;
    Graficos& g = *a.g;
    Formulario p{f, g, e, a, std::max(0, (a.gpu.ancho() - ANCHO) / 2), std::max(0, (a.gpu.alto() - ALTO) / 2)};
    if (g_carpeta_ajustes_nueva) {  // resultado de "Buscar carpeta..."
        g_carpeta_ajustes_nueva = false;
        const std::string nueva = g_carpeta_ajustes + "/";
        if (!g_carpeta_ajustes.empty() && carpeta_juego_valida(nueva)) {
            f.cfg.carpeta_juego = nueva;
            estado(p, tr(Tx::AJ_CARPETA_CAMBIADA), false);
        } else if (!g_carpeta_ajustes.empty()) {
            estado(p, tr(Tx::AJ_CARPETA_NO), true);
        }
    }

    g.gpu().nivel_ventana(NIVEL_SOBRE_VENTANAS);  // encima de todo, también de las ventanas del juego
    // Lo de atrás, oscurecido: la ventana es modal.
    g.gpu().rectangulo(-a.gpu.origen_x(), -a.gpu.origen_y(), a.gpu.grid_ancho(), a.gpu.grid_alto(), clave_orden(CAPA_UI, 0),
                       rgba(0, 0, 0, 150));
    ui::recuadro(g, p.ox, p.oy, ANCHO, ALTO, 250);
    ui::texto_centrado(g, tr(Tx::AJ_TITULO), p.ox, p.oy + 10, ANCHO, rgba(214, 190, 140));

    f.desplaza = std::clamp(f.desplaza - e.rueda * PASO, 0, f.desplaza_max);
    f.alto_contenido = 0;
    f.campos_antes = f.campos;
    f.campos = 0;
    if (e.clic) f.foco = -1;  // un clic fuera de un campo le quita el foco
    if (e.tab && f.campos_antes > 0) f.foco = (f.foco + 1) % f.campos_antes;

    const std::vector<std::string> nombres = pestanas();
    for (size_t i = 0; i < nombres.size(); ++i) {
        const std::string t = i == f.pestana ? "> " + nombres[i] + " <" : nombres[i];
        if (ui::boton_texto(g, e, t, p.ox + 16 + static_cast<int>(i) * 122, p.oy + 34, 118, 22)) {
            f.pestana = i;
            f.foco = -1;
            f.desplaza = 0;
        }
    }
    ui::recuadro(g, p.ox + CAJA_X, p.oy + CAJA_Y, CAJA_ANCHO, CAJA_ALTO);
    f.en_tab = true;
    switch (f.pestana) {
    case 0: pestana_juego(p); break;
    case 1: pestana_interfaz(p); break;
    case 2: pestana_audio(p); break;
    case 3: pestana_servidores(p); break;
    default: pestana_personajes(p); break;
    }
    f.en_tab = false;
    // Tope del scroll para el próximo frame: lo que sobra de la caja (el último renglón deja su margen).
    f.desplaza_max = std::max(0, f.alto_contenido - (CAJA_Y + CAJA_ALTO - 4 - Y_PRIMERA));
    if (f.desplaza_max > 0) {
        const int x_flechas = p.ox + CAJA_X + CAJA_ANCHO - 22;
        if (ui::boton_flecha(g, e, true, x_flechas, p.oy + CAJA_Y + 4, 18, 18)) f.desplaza -= PASO;
        if (ui::boton_flecha(g, e, false, x_flechas, p.oy + CAJA_Y + CAJA_ALTO - 22, 18, 18)) f.desplaza += PASO;
        f.desplaza = std::clamp(f.desplaza, 0, f.desplaza_max);
    }
    if (!f.aviso.empty()) ui::texto_centrado(g, f.aviso, p.ox, p.oy + 412, ANCHO, f.aviso_error ? COLOR_ERROR : COLOR_OK);
    const bool escape = e.escape;  // un desplegable abierto ya consumió su Esc
    barra_inferior(p);
    g.gpu().nivel_ventana(0);
    if (escape && a.ventana_ajustes) cerrar(a);
}

}  // namespace lin
