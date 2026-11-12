// Marcadores de teleport (bookmarks).
//  - "/bookmark nombre" en el chat recuerda el lugar donde se está (C_AddBookmark); el servidor lo
//    confirma con S_Bookmarks, que también manda todos al entrar al juego.
//  - Al usar un pergamino de teleport (usos 6 y 29) o el hechizo Teleport / Mass Teleport con
//    marcadores guardados, se elige el destino: uno de ellos o "Random". Clic derecho sobre un
//    marcador lo borra (C_DeleteBookmark); Esc cancela.
#include "app/app.h"

namespace lin {

namespace {

constexpr u32 HECHIZO_TELEPORT = 5, HECHIZO_MASS_TELEPORT = 69;
constexpr int ANCHO = 240, FILA = ALTO_GLIFO + 10, TITULO = 22, FILAS_VISIBLES = 6;

// Alto fijo: las filas que no caben se recorren con la barra o la rueda.
int alto_ventana() { return TITULO + 4 + FILAS_VISIBLES * FILA + 6; }

// Posición de la ventana: la elegida al arrastrarla, o centrada al abrirla.
void posicion(const App& a, int& x, int& y) {
    x = a.destino.x < 0 ? (a.gpu.ancho() - ANCHO) / 2 : a.destino.x;
    y = a.destino.y;
}

bool igual_sin_mayusculas(std::string_view x, std::string_view y) {
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i)
        if ((x[i] | 0x20) != (y[i] | 0x20)) return false;
    return true;
}

// Ir al destino elegido (marcador o, con nullptr, al azar).
void ir(App& a, const Marcador* m) {
    ElegirDestino& d = a.destino;
    d.abierto = false;
    const u16 mapa = m ? m->mapa : 0;
    const u32 id = m ? m->id : 0;
    if (d.item) {
        enviar(a, cmd::usar_teleport(a.op, d.item, mapa, id));
    } else {
        a.destino_mapa = mapa;
        a.destino_marcador = id;
        a.destino_elegido = true;
        pedir_hechizo(a, d.hechizo, 0);
    }
}

}  // namespace

bool es_hechizo_teleport(u32 id) { return id == HECHIZO_TELEPORT || id == HECHIZO_MASS_TELEPORT; }

void eventos_marcadores(App& a, const Evento& e) {
    if (e.tipo != Ev::MARCADOR) return;
    auto it = std::find_if(a.marcadores.begin(), a.marcadores.end(), [&](const Marcador& m) { return m.id == e.id; });
    if (it == a.marcadores.end()) it = a.marcadores.insert(a.marcadores.end(), Marcador{});
    it->id = e.id;
    it->mapa = static_cast<u16>(e.v[MK_MAPA]);
    it->x = static_cast<u16>(e.v[MK_X]);
    it->y = static_cast<u16>(e.v[MK_Y]);
    it->nombre.assign(e.s[0]);
}

bool comando_marcador(App& a, std::string_view orden, std::string_view nombre) {
    if (igual_sin_mayusculas(orden, "bookmark")) {
        if (nombre.empty()) a.chat.agregar("/bookmark <name>: remembers this place for teleporting.", rgba(255, 230, 120), CHAT_SISTEMA);
        else enviar(a, cmd::marcar_lugar(a.op, nombre));
        return true;
    }
    if (igual_sin_mayusculas(orden, "delbookmark")) {
        auto it = std::find_if(a.marcadores.begin(), a.marcadores.end(), [&](const Marcador& m) { return m.nombre == nombre; });
        if (it == a.marcadores.end()) {
            a.chat.agregar("/delbookmark: there is no bookmark with that name.", rgba(255, 230, 120), CHAT_SISTEMA);
        } else {
            enviar(a, cmd::borrar_marcador(a.op, nombre));
            a.marcadores.erase(it);
        }
        return true;
    }
    return false;
}

bool elegir_destino(App& a, ObjId item, u32 hechizo) {
    if (a.marcadores.empty()) return false;
    a.destino = ElegirDestino{};
    a.destino.abierto = true;
    a.destino.item = item;
    a.destino.hechizo = hechizo;
    return true;
}

bool sobre_destino(const App& a) {
    int x = 0, y = 0;
    posicion(a, x, y);
    return a.destino.abierto && ui::sobre(a.entrada, x, y, ANCHO, alto_ventana());
}

void dibujar_destino(App& a) {
    ElegirDestino& d = a.destino;
    if (!d.abierto) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const u64 kf = clave_orden(CAPA_UI, 3), kt = clave_orden(CAPA_UI_TEXTO, 3);
    int x = 0, y = 0;
    posicion(a, x, y);
    // Arrastrar por el título (el botón de cerrar queda fuera).
    const int alto = alto_ventana();
    if (e.pulsado && ui::sobre(e, x, y, ANCHO - 22, TITULO)) {
        d.arrastrando = true;
        d.dx = e.mx - x;
        d.dy = e.my - y;
    }
    if (d.arrastrando) {
        a.clic_consumido = true;  // no camina aunque el puntero salga de la ventana
        if (!e.presionado) {
            d.arrastrando = false;
            e.clic = false;  // pegada al borde de la pantalla, soltar podría caer sobre una fila
        } else {
            x = std::clamp(e.mx - d.dx, 0, std::max(0, a.gpu.ancho() - ANCHO));
            y = std::clamp(e.my - d.dy, 0, std::max(0, a.gpu.alto() - alto));
        }
    }
    d.x = x;
    d.y = y;

    ui::recuadro(g, x, y, ANCHO, alto, 240);
    g.gpu().rectangulo(x, y, ANCHO, TITULO, kf + (1u << 24), rgba(44, 34, 24, 240));
    g.texto("Teleport to...", static_cast<float>(x + 8), static_cast<float>(y + 5), kt, ui::COLOR_TITULO);
    const bool cerrar_boton = ui::boton_texto(g, e, "x", x + ANCHO - 20, y + 3, 16, 16);

    // Filas: el marcador o "Random" al final. La rueda sobre la ventana las recorre.
    const u32 total = static_cast<u32>(a.marcadores.size()) + 1;
    const int ry = y + TITULO + 4, rancho = ANCHO - 8 - 24;
    barra_scroll_ventana(a, d.barra, x + ANCHO - 14, ry, FILAS_VISIBLES * FILA, total, FILAS_VISIBLES, d.desplaza,
                         clave_orden(CAPA_UI, 4), sobre_destino(a));
    const Marcador* elegido = nullptr;
    bool azar = false, borrado = false;
    for (int f = 0; f < FILAS_VISIBLES; ++f) {
        const size_t i = d.desplaza + static_cast<size_t>(f);
        if (i >= total) break;
        const int fy = ry + f * FILA;
        const bool encima = ui::sobre(e, x + 8, fy, rancho, FILA - 2);
        g.gpu().rectangulo(x + 8, fy, rancho, FILA - 2, kf, encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        if (i == a.marcadores.size()) {
            g.texto("Random", static_cast<float>(x + 14), static_cast<float>(fy + 5), kt, rgba(170, 160, 140));
            azar |= encima && e.clic;
            continue;
        }
        const Marcador& m = a.marcadores[i];
        g.texto(m.nombre, static_cast<float>(x + 14), static_cast<float>(fy + 5), kt, ui::COLOR_TEXTO);
        if (encima && e.clic) elegido = &m;
        if (encima && e.clic_derecho) {  // borrarlo
            enviar(a, cmd::borrar_marcador(a.op, m.nombre));
            a.marcadores.erase(a.marcadores.begin() + static_cast<std::ptrdiff_t>(i));
            borrado = true;
            break;
        }
    }

    const bool cancelar = e.escape || (e.clic_derecho && !borrado) || cerrar_boton;
    e.escape = false;
    if (elegido || azar) ir(a, elegido);
    else if (cancelar || a.marcadores.empty()) d.abierto = false;
}

}  // namespace lin
