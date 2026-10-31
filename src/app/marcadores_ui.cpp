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
constexpr int ANCHO = 240, FILA = ALTO_GLIFO + 10;

int alto(const App& a) { return 34 + static_cast<int>(a.marcadores.size() + 1) * FILA + 8; }

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
    a.destino = {true, item, hechizo};
    return true;
}

bool sobre_destino(const App& a) {
    return a.destino.abierto && ui::sobre(a.entrada, (a.gpu.ancho() - ANCHO) / 2, 60, ANCHO, alto(a));
}

void dibujar_destino(App& a) {
    ElegirDestino& d = a.destino;
    if (!d.abierto) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = (a.gpu.ancho() - ANCHO) / 2, y = 60;
    const u64 kf = clave_orden(CAPA_UI, 3), kt = clave_orden(CAPA_UI_TEXTO, 3);
    ui::recuadro(g, x, y, ANCHO, alto(a), 240);
    g.texto("Teleport to...", static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    const Marcador* elegido = nullptr;
    bool azar = false, borrado = false;
    for (size_t i = 0; i <= a.marcadores.size(); ++i) {
        const int fy = y + 30 + static_cast<int>(i) * FILA, w = ANCHO - 16;
        const bool encima = ui::sobre(e, x + 8, fy, w, FILA - 2);
        g.gpu().rectangulo(x + 8, fy, w, FILA - 2, kf, encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
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
    const bool cancelar = e.escape || (e.clic_derecho && !borrado);
    e.escape = false;
    if (elegido || azar) ir(a, elegido);
    else if (cancelar || a.marcadores.empty()) d.abierto = false;
}

}  // namespace lin
