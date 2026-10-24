// Barra de atajos (como el original: Alt+1..Alt+0). Diez casilleros abajo al centro, sobre el
// panel. Se llenan arrastrando un hechizo desde su ventana (Alt+S) o un ítem desde el inventario
// (Alt+I); entre casilleros se arrastra para mover, y clic derecho o soltar fuera los vacía. Clic
// o Alt+número usa: el hechizo (si pide objetivo, espera el clic) o el ítem (como el doble clic en
// el inventario). Clic del medio sobre un casillero lo elige (marco dorado; otra vez lo suelta) y
// clic del medio sobre el mundo usa el elegido sobre lo que haya bajo el puntero, como en el
// original. Se guarda por personaje en settings.ini ("barra="; "*" tras el elegido).
#include <charconv>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr int CELDA = 36, SEPARACION = 2, PASO = CELDA + SEPARACION;
constexpr int ANCHO_BARRA = CASILLEROS_BARRA * PASO - SEPARACION;
constexpr int MOVIMIENTO_ARRASTRE = 4;  // px antes de que un clic se tome como arrastre

void cargar_barra(App& a) {
    a.barra_de = nombre_personaje(a);
    for (Atajo& b : a.barra) b = {};
    a.casillero_elegido = -1;
    std::string_view v = valor_de_personaje(a.cfg.barra, a.barra_de);
    for (int i = 0; i < CASILLEROS_BARRA && !v.empty(); ++i) {
        std::string_view t = v.substr(0, v.find(','));
        v = t.size() < v.size() ? v.substr(t.size() + 1) : std::string_view{};
        if (t.size() < 2) continue;
        if (t.back() == '*') a.casillero_elegido = i;
        const char* p = t.data() + 1;
        const char* fin = t.data() + t.size();
        Atajo& b = a.barra[i];
        if (t[0] == 'h') {
            b.tipo = TipoAtajo::HECHIZO;
            std::from_chars(p, fin, b.id);
        } else if (t[0] == 'i') {
            b.tipo = TipoAtajo::ITEM;
            auto r = std::from_chars(p, fin, b.gfx);
            if (r.ptr < fin && *r.ptr == ':') std::from_chars(r.ptr + 1, fin, b.id);
        }
    }
}

void guardar_barra(App& a) {
    std::string v;
    for (int i = 0; i < CASILLEROS_BARRA; ++i) {
        const Atajo& b = a.barra[i];
        if (b.tipo == TipoAtajo::HECHIZO) v += 'h' + std::to_string(b.id);
        else if (b.tipo == TipoAtajo::ITEM) v += 'i' + std::to_string(b.gfx) + ':' + std::to_string(b.id);
        else v += '0';
        if (i == a.casillero_elegido) v += '*';
        if (i + 1 < CASILLEROS_BARRA) v += ',';
    }
    poner_valor_de_personaje(a.cfg.barra, a.barra_de, v);
    guardar_config(a.cfg);
}

// Slot del ítem del casillero en el inventario: por id o, si ya no está, otro con el mismo gfx
// (y el casillero pasa a ese id).
Slot item_de(App& a, Atajo& b) {
    const TablaItems& t = a.mundo.inventario.items;
    Slot s = t.buscar(b.id);
    if (s != SIN_SLOT) return s;
    for (Slot i = 0; i < t.tamano(); ++i) {
        if (t.col<I_GFX>()[i] == b.gfx) {
            b.id = t.ids()[i];
            guardar_barra(a);
            return i;
        }
    }
    return SIN_SLOT;
}

void usar_casillero(App& a, int i) {
    Atajo& b = a.barra[i];
    if (b.tipo == TipoAtajo::HECHIZO) {
        pedir_hechizo(a, b.id, 0);
    } else if (b.tipo == TipoAtajo::ITEM) {
        Slot s = item_de(a, b);
        if (s != SIN_SLOT) usar_item(a, a.mundo.inventario.items.ids()[s]);
    }
}

// Clic del medio: sobre un casillero lo elige; sobre el mundo usa el elegido. Un hechizo con
// objetivo va a lo que haya bajo el puntero (uno mismo incluido) y sin nada ahí no se lanza.
void clic_medio(App& a) {
    if (int c = casillero_bajo_puntero(a); c >= 0) {
        a.casillero_elegido = a.casillero_elegido == c || a.barra[c].tipo == TipoAtajo::VACIO ? -1 : c;
        guardar_barra(a);
        return;
    }
    if (a.casillero_elegido < 0 || sobre_barra(a) || sobre_alguna_ventana(a)) return;
    const Atajo& b = a.barra[a.casillero_elegido];
    if (b.tipo == TipoAtajo::HECHIZO && a.hechizos[b.id].con_objetivo) {
        if (ObjId obj = objeto_bajo_puntero(a, true)) pedir_hechizo(a, b.id, obj);
    } else {
        usar_casillero(a, a.casillero_elegido);
    }
}

// Ícono del casillero (región vacía si no tiene).
const RegionAtlas& icono_atajo(App& a, const Atajo& b) {
    static const RegionAtlas vacia;
    if (b.tipo == TipoAtajo::HECHIZO && a.hechizos[b.id].icono) return a.g->icono(a.hechizos[b.id].icono);
    if (b.tipo == TipoAtajo::ITEM) return a.g->icono(b.gfx);
    return vacia;
}

bool movio(const App& a) {
    return std::abs(a.entrada.mx - a.arrastre_x0) > MOVIMIENTO_ARRASTRE || std::abs(a.entrada.my - a.arrastre_y0) > MOVIMIENTO_ARRASTRE;
}

}  // namespace

std::string_view nombre_personaje(const App& a) {
    return a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) ? std::string_view(a.pjs[a.pj_sel].nombre)
                                                                        : std::string_view{};
}

int casillero_bajo_puntero(const App& a) {
    const EntradaFrame& e = a.entrada;
    int dx = e.mx - a.barra_x, dy = e.my - a.barra_y;
    if (dx < 0 || dy < 0 || dx >= ANCHO_BARRA || dy >= CELDA || dx % PASO >= CELDA) return -1;
    return dx / PASO;
}

bool sobre_barra(const App& a) {
    const EntradaFrame& e = a.entrada;
    return e.mx >= a.barra_x - 4 && e.mx < a.barra_x + ANCHO_BARRA + 4 && e.my >= a.barra_y - 4 && e.my < a.barra_y + CELDA + 4;
}

void poner_atajo(App& a, int casillero, Atajo atajo) {
    if (casillero < 0 || casillero >= CASILLEROS_BARRA) return;
    a.barra[casillero] = atajo;
    if (atajo.tipo == TipoAtajo::VACIO && a.casillero_elegido == casillero) a.casillero_elegido = -1;
    guardar_barra(a);
}

void procesar_barra(App& a) {
    if (a.barra_de != nombre_personaje(a)) cargar_barra(a);
    if (a.entrada.alt_numero) usar_casillero(a, a.entrada.alt_numero - 1);
    if (a.entrada.clic_medio) clic_medio(a);
    // Prueba automática (--alt=N): como apretar Alt+N, uno cada 3 s.
    Automatizacion& au = a.aut;
    if (au.alt_hechos < au.alt.size() && a.frames_en_pantalla >= 300 + 360 * au.alt_hechos) {
        int n = au.alt[au.alt_hechos++];
        if (n >= 1 && n <= CASILLEROS_BARRA) usar_casillero(a, n - 1);
    }
}

void dibujar_barra(App& a, int y) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    a.barra_x = (a.gpu.ancho() - ANCHO_BARRA) / 2;
    a.barra_y = y;
    const u64 kf = clave_orden(CAPA_UI, 5), ki = clave_orden(CAPA_UI, 6), kt = clave_orden(CAPA_UI_TEXTO, 5);
    const int bajo = casillero_bajo_puntero(a);
    // Algo que se está arrastrando hacia la barra: se resalta el casillero de destino.
    const bool trayendo = (a.arrastrando && e.presionado) || a.arrastrando_hechizo || a.arrastrando_atajo >= 0;
    for (int i = 0; i < CASILLEROS_BARRA; ++i) {
        const int x = a.barra_x + i * PASO;
        const Atajo& b = a.barra[i];
        u32 fondo = i == bajo ? (trayendo ? rgba(90, 75, 30, 230) : rgba(60, 50, 35, 230)) : rgba(20, 18, 15, 200);
        g.gpu().rectangulo(x, y, CELDA, CELDA, kf, fondo);
        if (i == a.casillero_elegido) {
            const u32 oro = rgba(240, 200, 80);
            g.gpu().rectangulo(x, y, CELDA, 2, kf + 1, oro);
            g.gpu().rectangulo(x, y + CELDA - 2, CELDA, 2, kf + 1, oro);
            g.gpu().rectangulo(x, y, 2, CELDA, kf + 1, oro);
            g.gpu().rectangulo(x + CELDA - 2, y, 2, CELDA, kf + 1, oro);
        }
        if (i != a.arrastrando_atajo || !movio(a)) {
            const RegionAtlas& ic = icono_atajo(a, b);
            // Hechizo que no se sabe o sin MP suficiente, o ítem que ya no está: oscurecido.
            bool disponible = true;
            if (b.tipo == TipoAtajo::HECHIZO)
                disponible = a.mundo.jugador.sabe(b.id) && a.mundo.jugador.mp >= a.hechizos[b.id].mp;
            u32 cantidad = 0;
            if (b.tipo == TipoAtajo::ITEM) {
                Slot s = a.mundo.inventario.items.buscar(b.id);
                if (s == SIN_SLOT) {
                    for (Slot k = 0; k < a.mundo.inventario.items.tamano() && s == SIN_SLOT; ++k)
                        if (a.mundo.inventario.items.col<I_GFX>()[k] == b.gfx) s = k;
                }
                disponible = s != SIN_SLOT;
                if (s != SIN_SLOT) cantidad = a.mundo.inventario.items.col<I_CANTIDAD>()[s];
            }
            const u32 color = disponible ? BLANCO : rgba(110, 110, 110);
            if (ic.valida()) {
                g.gpu().dibujar(ic, x + 2, y + 2, ki, color);
            } else if (b.tipo == TipoAtajo::HECHIZO) {  // sin ícono: las primeras letras del nombre
                std::string_view n = a.hechizos[b.id].nombre.substr(0, 5);
                g.texto(n, static_cast<float>(x + 3), static_cast<float>(y + 12), kt, color);
            }
            if (cantidad > 1) {
                char c[16];
                std::snprintf(c, sizeof c, "%u", cantidad);
                g.texto_sombra(c, static_cast<float>(x + CELDA - 2 - ancho_texto(c, std::strlen(c))),
                               static_cast<float>(y + CELDA - 1 - ALTO_GLIFO), kt + (2ull << 24), rgba(255, 255, 254));
            }
        }
        char n[4];
        std::snprintf(n, sizeof n, "%d", (i + 1) % 10);
        g.texto_sombra(n, static_cast<float>(x + 2), static_cast<float>(y + 1), kt + (2ull << 24), rgba(255, 230, 120));
    }

    // Nombre bajo el puntero.
    if (bajo >= 0 && !trayendo && a.barra[bajo].tipo != TipoAtajo::VACIO) {
        Atajo& b = a.barra[bajo];
        char buf[256];
        size_t m = 0;
        if (b.tipo == TipoAtajo::HECHIZO) {
            std::string_view nh = a.hechizos[b.id].nombre;
            m = std::min(nh.size(), sizeof buf);
            std::memcpy(buf, nh.data(), m);
        } else if (Slot s = item_de(a, b); s != SIN_SLOT) {
            m = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), buf, sizeof buf);
        }
        int w = ancho_texto(buf, m);
        int tx = std::clamp(e.mx - w / 2, 4, a.gpu.ancho() - w - 4), ty = y - ALTO_GLIFO - 6;
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto({buf, m}, static_cast<float>(tx), static_cast<float>(ty), clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
    }

    // Arrastrar desde un casillero: mover a otro, o vaciarlo soltando fuera. Sin arrastre, clic usa.
    if (e.pulsado && bajo >= 0 && a.barra[bajo].tipo != TipoAtajo::VACIO) {
        a.arrastrando_atajo = bajo;
        a.arrastre_x0 = e.mx;
        a.arrastre_y0 = e.my;
    }
    if (a.arrastrando_atajo >= 0 && e.presionado && movio(a)) {
        const RegionAtlas& ic = icono_atajo(a, a.barra[a.arrastrando_atajo]);
        g.gpu().dibujar(ic, e.mx - 16, e.my - 16, clave_orden(CAPA_ARRASTRE, 0));
    }
    if (e.clic && a.arrastrando_atajo >= 0) {
        int desde = a.arrastrando_atajo;
        a.arrastrando_atajo = -1;
        if (!movio(a)) {
            if (bajo == desde) usar_casillero(a, desde);
        } else if (bajo >= 0) {
            std::swap(a.barra[desde], a.barra[bajo]);
            if (a.casillero_elegido == desde) a.casillero_elegido = bajo;
            else if (a.casillero_elegido == bajo) a.casillero_elegido = desde;
            guardar_barra(a);
        } else if (!sobre_barra(a)) {
            poner_atajo(a, desde, {});
        }
    }
    if (e.clic_derecho && bajo >= 0) poner_atajo(a, bajo, {});
}

}  // namespace lin
