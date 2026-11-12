// Auto-buffs (PLAN_JUGABILIDAD.md, punto 4): seis casilleros a la derecha de la barra de atajos con
// buffs propios que se relanzan solos al terminarse, y un interruptor "Auto" encima (apagado por
// defecto). Se llenan arrastrando un hechizo desde la ventana de magia o desde la barra; entre ellos
// se arrastra para ordenar y clic derecho vacía. Es una macro del lado del cliente: manda el mismo
// C_UseSkill que el jugador a mano, con la misma espera entre hechizos, y no corta lo que el
// personaje está haciendo (espera a que se detenga). No se pausa en pueblos. Se guarda por personaje
// en settings.ini: "autobuffs=Nombre:1|43,42,0,0,0,0" (encendido y los hechizos).
#include <charconv>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr int CELDA = CELDA_AUTO, PASO = CELDA + 2;
const u32 VERDE = rgba(90, 170, 90);

void poner(App& a, int i, u32 id) {
    CasilleroAuto& c = a.autobuffs.c[i];
    c = {};
    if (!id) return;
    c.hechizo = id;
    c.mp = a.hechizos[id].mp;
    c.duracion_ms = a.hechizos[id].duracion_s * 1000ull;
}

void cargar(App& a) {
    AutoBuffs& ab = a.autobuffs;
    ab.de = nombre_personaje(a);
    ab.encendido = false;
    ab.arrastrando = ab.pidiendo = ab.ultimo = -1;
    for (int i = 0; i < CASILLEROS_AUTO; ++i) poner(a, i, 0);
    std::string_view v = valor_de_personaje(a.cfg.autobuffs, ab.de);
    const size_t barra = v.find('|');
    if (barra == std::string_view::npos) return;
    ab.encendido = v.substr(0, barra) == "1";
    v = v.substr(barra + 1);
    for (int i = 0; i < CASILLEROS_AUTO && !v.empty(); ++i) {
        std::string_view t = v.substr(0, v.find(','));
        v = t.size() < v.size() ? v.substr(t.size() + 1) : std::string_view{};
        u32 id = 0;
        std::from_chars(t.data(), t.data() + t.size(), id);
        if (id && acepta_autobuff(a.hechizos[id])) poner(a, i, id);
    }
}

void guardar(App& a) {
    std::string v = a.autobuffs.encendido ? "1|" : "0|";
    for (int i = 0; i < CASILLEROS_AUTO; ++i) {
        v += std::to_string(a.autobuffs.c[i].hechizo);
        if (i + 1 < CASILLEROS_AUTO) v += ',';
    }
    poner_valor_de_personaje(a.cfg.autobuffs, a.autobuffs.de, v);
    guardar_config(a.cfg);
}

// Algo que un hechizo cortaría o que no deja lanzarlo: caminar, atacar, otro hechizo en camino o
// esperando, un intercambio, el modo fantasma.
bool ocupado(const App& a, u64 ahora) {
    const EntradaFrame& e = a.entrada;
    const TablaObjetos& o = a.mundo.objetos;
    const Slot s = o.buscar(a.mundo.jugador.id);
    return s == SIN_SLOT || e.presionado || e.medio_presionado || e.arriba || e.abajo || e.izquierda || e.derecha ||
           a.objetivo || a.ataque_sostenido || a.hechizo_pendiente || a.hechizo_apuntando || ahora < a.hechizo_listo_ms ||
           ahora < a.fin_ataque || moviendose(a.mundo, s, ahora) || a.intercambio.abierto ||
           (o.col<O_FLAGS>()[s] & FLAG_FANTASMA);
}

bool movio(const App& a) {
    return std::abs(a.entrada.mx - a.arrastre_x0) > 4 || std::abs(a.entrada.my - a.arrastre_y0) > 4;
}

int y_interruptor(const App& a) { return a.autobuffs.y - ALTO_GLIFO - 5; }

}  // namespace

int autobuff_bajo_puntero(const App& a) {
    const EntradaFrame& e = a.entrada;
    const int dx = e.mx - a.autobuffs.x, dy = e.my - a.autobuffs.y;
    if (dx < 0 || dy < 0 || dx >= ANCHO_AUTO || dy >= CELDA || dx % PASO >= CELDA) return -1;
    return dx / PASO;
}

bool sobre_autobuffs(const App& a) {
    const EntradaFrame& e = a.entrada;
    const int x = a.autobuffs.x, y = y_interruptor(a);
    return e.mx >= x - 4 && e.mx < x + ANCHO_AUTO + 4 && e.my >= y && e.my < a.autobuffs.y + CELDA + 4;
}

void poner_autobuff(App& a, int casillero, u32 id) {
    if (casillero < 0 || casillero >= CASILLEROS_AUTO) return;
    if (!acepta_autobuff(a.hechizos[id])) {
        a.chat.agregar("Auto-buff: only buffs you can cast on yourself.", rgba(255, 200, 120), CHAT_SISTEMA);
        return;
    }
    poner(a, casillero, id);
    guardar(a);
}

void procesar_autobuffs(App& a) {
    AutoBuffs& ab = a.autobuffs;
    if (ab.de != nombre_personaje(a)) cargar(a);
    const u64 ahora = a.entrada.ahora_ms;
    // Lo pedido: salió (paso_hechizo_pendiente marca hechizo_lanzado_ms) o se canceló (un clic).
    if (ab.pidiendo >= 0) {
        CasilleroAuto& c = ab.c[ab.pidiendo];
        if (a.hechizo_lanzado_ms >= ab.pedido_ms) {
            c.lanzado_ms = a.hechizo_lanzado_ms;
            ab.ultimo = ab.pidiendo;
            ab.pidiendo = -1;
        } else if (a.hechizo_pendiente != c.hechizo) {
            ab.pidiendo = -1;
        }
    }
    if (!ab.encendido || ab.pidiendo >= 0 || ocupado(a, ahora)) return;
    const int i = elegir_autobuff(a.mundo.jugador, ab.c, CASILLEROS_AUTO, ahora);
    if (i < 0) return;
    const u32 id = ab.c[i].hechizo;
    pedir_hechizo(a, id, a.hechizos[id].con_objetivo ? a.mundo.jugador.id : 0);
    if (a.hechizo_pendiente == id) {
        ab.pidiendo = i;
        ab.pedido_ms = ahora;
    } else {
        ab.c[i].reintentar_ms = ahora + ESPERA_FALLO_MS;  // no lo sabe: no insistir en cada frame
    }
}

// "MP insuficiente" (278) o "el hechizo falló" (280) poco después de lanzar uno: ese casillero
// espera ESPERA_FALLO_MS, y deja de contar como lanzado (el efecto no está).
void eventos_autobuffs(App& a, const Evento& e) {
    AutoBuffs& ab = a.autobuffs;
    if (e.tipo != Ev::MENSAJE_SERVIDOR || (e.v[MS_ID] != 278 && e.v[MS_ID] != 280) || ab.ultimo < 0) return;
    CasilleroAuto& c = ab.c[ab.ultimo];
    const u64 ahora = a.entrada.ahora_ms;
    if (ahora - c.lanzado_ms < ESPERA_RESPUESTA_MS) {
        c.lanzado_ms = 0;
        c.reintentar_ms = ahora + ESPERA_FALLO_MS;
    }
    ab.ultimo = -1;
}

void dibujar_autobuffs(App& a) {
    AutoBuffs& ab = a.autobuffs;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u64 ahora = e.ahora_ms;
    const int x0 = ab.x, y = ab.y;
    const u64 kf = clave_orden(CAPA_UI, 5), ki = clave_orden(CAPA_UI, 6), kt = clave_orden(CAPA_UI_TEXTO, 5);
    const bool arrastrando_algo = a.arrastrando || a.arrastrando_hechizo || a.arrastrando_atajo >= 0 || ab.arrastrando >= 0;

    // Interruptor encima del primer casillero: verde encendido.
    const int ty = y_interruptor(a), th = ALTO_GLIFO + 3;
    const bool sobre_int = ui::sobre(e, x0, ty, CELDA, th);
    g.gpu().rectangulo(x0, ty, CELDA, th, kf, ab.encendido ? rgba(40, 110, 40, 230) : rgba(20, 18, 15, sobre_int ? 230 : 200));
    g.texto("Auto", static_cast<float>(x0 + (CELDA - ancho_texto("Auto", 4)) / 2), static_cast<float>(ty + 1), kt,
            ab.encendido ? rgba(220, 255, 220) : rgba(160, 150, 130));
    if (e.clic && sobre_int && !arrastrando_algo) {
        ab.encendido = !ab.encendido;
        guardar(a);
    }

    const int bajo = autobuff_bajo_puntero(a);
    const bool trayendo = (a.arrastrando_hechizo || a.arrastrando_atajo >= 0 || ab.arrastrando >= 0) && bajo >= 0;
    for (int i = 0; i < CASILLEROS_AUTO; ++i) {
        const int x = x0 + i * PASO;
        const CasilleroAuto& c = ab.c[i];
        g.gpu().rectangulo(x, y, CELDA, CELDA, kf, i == bajo ? (trayendo ? rgba(60, 90, 40, 230) : rgba(45, 55, 35, 230)) : rgba(20, 18, 15, 200));
        g.gpu().rectangulo(x, y, CELDA, 1, kf + 1, VERDE);
        g.gpu().rectangulo(x, y + CELDA - 1, CELDA, 1, kf + 1, VERDE);
        g.gpu().rectangulo(x, y, 1, CELDA, kf + 1, VERDE);
        g.gpu().rectangulo(x + CELDA - 1, y, 1, CELDA, kf + 1, VERDE);
        if (!c.hechizo || (i == ab.arrastrando && e.presionado && movio(a))) continue;
        const Hechizo& h = a.hechizos[c.hechizo];
        const bool disponible = a.mundo.jugador.sabe(c.hechizo) && a.mundo.jugador.mp >= c.mp;
        const u32 color = disponible ? BLANCO : rgba(110, 110, 110);
        if (h.icono) g.gpu().dibujar(g.icono(h.icono), x + 1, y + 1, ki, color);
        else g.texto(h.nombre.substr(0, 4), static_cast<float>(x + 2), static_cast<float>(y + 11), kt, color);
        // Lo que le queda al efecto, abajo dentro del casillero ("29m", "45s").
        if (const u64 resta = (resta_efecto_de_hechizo(a.mundo.jugador, c.hechizo, ahora) + 999) / 1000; resta > 1) {
            char d[16];
            if (resta >= 60) std::snprintf(d, sizeof d, "%llum", static_cast<unsigned long long>((resta + 59) / 60));
            else std::snprintf(d, sizeof d, "%llus", static_cast<unsigned long long>(resta));
            g.texto_sombra(d, static_cast<float>(x + CELDA - 2 - ancho_texto(d, std::strlen(d))),
                           static_cast<float>(y + CELDA - 1 - ALTO_GLIFO), kt + (2ull << 24), rgba(220, 255, 220));
        }
    }

    // Nombre bajo el puntero.
    if (bajo >= 0 && !trayendo && ab.c[bajo].hechizo) {
        const std::string_view n = a.hechizos[ab.c[bajo].hechizo].nombre;
        const int w = ancho_texto(n.data(), n.size());
        const int tx = std::max(4, std::min(e.mx - w / 2, a.gpu.ancho() - w - 4)), tyy = ty - ALTO_GLIFO - 6;
        g.gpu().rectangulo(tx - 4, tyy - 3, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto(n, static_cast<float>(tx), static_cast<float>(tyy), clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
    }

    // Arrastrar entre casilleros (ordenar) o fuera (vaciar); clic derecho vacía.
    if (e.pulsado && bajo >= 0 && ab.c[bajo].hechizo && !arrastrando_algo) {
        ab.arrastrando = bajo;
        a.arrastre_x0 = e.mx;
        a.arrastre_y0 = e.my;
    }
    if (ab.arrastrando >= 0 && e.presionado && movio(a)) {
        const Hechizo& h = a.hechizos[ab.c[ab.arrastrando].hechizo];
        if (h.icono) g.gpu().dibujar(g.icono(h.icono), e.mx - 16, e.my - 16, clave_orden(CAPA_ARRASTRE, 0));
    }
    if (e.clic && ab.arrastrando >= 0) {
        const int desde = ab.arrastrando;
        ab.arrastrando = -1;
        if (movio(a) && bajo >= 0 && bajo != desde) {
            std::swap(ab.c[desde], ab.c[bajo]);
            if (ab.ultimo == desde) ab.ultimo = bajo;
            else if (ab.ultimo == bajo) ab.ultimo = desde;
            ab.pidiendo = -1;
            guardar(a);
        } else if (movio(a) && !sobre_autobuffs(a)) {
            poner(a, desde, 0);
            guardar(a);
        }
    }
    if (e.clic_derecho && bajo >= 0 && ab.c[bajo].hechizo) {
        poner(a, bajo, 0);
        if (ab.pidiendo == bajo) ab.pidiendo = -1;
        if (ab.ultimo == bajo) ab.ultimo = -1;
        guardar(a);
    }
}

}  // namespace lin
