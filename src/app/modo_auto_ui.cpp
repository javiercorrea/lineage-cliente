// Modo auto programable (docs/analisis/PLAN_MODO_AUTO.md), fase 3: corre el programa en el juego.
//  - "/auto <nombre>" carga planes/<nombre>.txt (junto a settings.ini), lo valida y lo arranca; "/auto"
//    pausa o reanuda; "/auto off" lo termina. Con errores no arranca (una regla de escape rota sería peor
//    que nada): los muestra en el chat con su línea.
//  - Cada frame arma el entorno (vida, maná, monstruos y personajes a la vista, inventario, efectos) y
//    pregunta a `decidir` qué hacer. Las acciones de una regla van a una cola y salen de a una (un hechizo
//    espera a que termine el anterior); una regla de más prioridad la reemplaza.
//  - Moverse: fija el destino en cada frame (el punto al azar de "recorrer", el de "puntos", el personaje
//    que se sigue) y el enrutador sigue la última ruta, recalculando al azar; el paso lo da mover_jugador
//    (a.paso_auto), con la misma lógica de celdas, portales y giros que el teclado.
//  - Atacar usa el objetivo y el ataque sostenido de siempre (mover_jugador camina y ataca).
//  - Mientras corre, los auto-buffs de la barra esperan (los del programa van en su prioridad) y la
//    auto-poción sigue "pociones:". El jugador lo pausa al caminar, hacer clic en el mundo o usar un atajo
//    de la barra; las teclas del programa no lo pausan.
// shortcut: el programa no se recuerda por personaje ni se elige desde Opciones, y "mapa" por nombre no se
// resuelve (solo el número); los auto-hechizos no se muestran en los casilleros de la barra (fase 4).
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include "app/app.h"
#include "mundo/autopocion.h"

namespace lin {

namespace {

const u32 COLOR_AVISO = rgba(160, 220, 255);

void aviso(App& a, std::string_view t) { a.chat.agregar(t, COLOR_AVISO, CHAT_SISTEMA); }

bool igual(std::string_view x, std::string_view y) { return normalizar_nombre(x) == normalizar_nombre(y); }

i32 distancia_a(i32 x0, i32 y0, i32 x1, i32 y1) { return std::max(std::abs(x1 - x0), std::abs(y1 - y0)); }

// El nombre de un ítem del inventario como lo escribe un programa: traducido, sin la cantidad ni lo que
// va entre paréntesis ("Red Potion (10)", "Sword ($9)") ni el encantamiento ("+7 "), normalizado.
std::string nombre_item_normalizado(const App& a, Slot s) {
    char buf[256];
    const size_t n = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), buf, sizeof buf);
    std::string_view v(buf, n);
    for (size_t p; (p = v.rfind(" (")) != std::string_view::npos && v.back() == ')';) v = v.substr(0, p);
    if (!v.empty() && (v[0] == '+' || v[0] == '-')) {
        const size_t esp = v.find(' ');
        if (esp != std::string_view::npos) v.remove_prefix(esp + 1);
    }
    return normalizar_nombre(v);
}

ObjId buscar_item(const App& a, const std::string& nombre) {
    const TablaItems& t = a.mundo.inventario.items;
    for (Slot s = 0; s < t.tamano(); ++s)
        if (t.col<I_CANTIDAD>()[s] && nombre_item_normalizado(a, s) == nombre) return t.ids()[s];
    return 0;
}

u32 hechizo_por_nombre(const App& a, const std::string& nombre) {
    for (const auto& [n, id] : a.modo_auto.hechizos)
        if (n == nombre) return id;
    return 0;
}

u32 pocion_por_nombre(const std::string& nombre) {
    for (u32 i = 0; i < std::size(POCIONES_AUTO); ++i)
        if (normalizar_nombre(POCIONES_AUTO[i].nombre) == nombre) return POCION_AUTO + i;
    return 0;
}

// Un monstruo vivo que se puede atacar: ni mascota ni invocación (propia o ajena).
bool es_monstruo(const App& a, Slot s) {
    const TablaObjetos& o = a.mundo.objetos;
    return o.ids()[s] != a.mundo.jugador.id && o.col<O_ESTADO>()[s] != ESTADO_MUERTO && !o.col<O_MASCOTA>()[s] &&
           tipo_objeto(a, s) == TIPO_MONSTRUO;
}

ObjId monstruo_mas_cercano(const App& a, Slot yo) {
    const TablaObjetos& o = a.mundo.objetos;
    ObjId mejor = 0;
    i32 dmin = 1 << 30;
    for (Slot s = 0; s < o.tamano(); ++s) {
        if (!es_monstruo(a, s)) continue;
        const i32 d = distancia_a(o.col<O_X>()[yo], o.col<O_Y>()[yo], o.col<O_X>()[s], o.col<O_Y>()[s]);
        if (d < dmin) {
            dmin = d;
            mejor = o.ids()[s];
        }
    }
    return mejor;
}

// Otro jugador a la vista por su nombre normalizado.
Slot personaje_por_nombre(const App& a, const std::string& nombre) {
    const TablaObjetos& o = a.mundo.objetos;
    for (Slot s = 0; s < o.tamano(); ++s)
        if (o.ids()[s] != a.mundo.jugador.id && tipo_objeto(a, s) == TIPO_PERSONAJE &&
            normalizar_nombre(texto(a.mundo, o.col<O_NOMBRE>()[s])) == nombre)
            return s;
    return SIN_SLOT;
}

EntornoAuto armar_entorno(App& a, Slot yo, u64 ahora) {
    const EstadoJugador& j = a.mundo.jugador;
    const TablaObjetos& o = a.mundo.objetos;
    App::ModoAuto& m = a.modo_auto;
    EntornoAuto en;
    en.hp = j.hp;
    en.hp_max = j.hp_max;
    en.mp = j.mp;
    en.mp_max = j.mp_max;
    en.mapa = a.mundo.mapa;
    const Slot obj = a.objetivo ? o.buscar(a.objetivo) : SIN_SLOT;
    en.atacando = obj != SIN_SLOT && !a.hablar_objetivo && o.col<O_ESTADO>()[obj] != ESTADO_MUERTO;
    if (obj != SIN_SLOT && o.col<O_HP>()[obj] <= 100) en.objetivo_hp_pct = o.col<O_HP>()[obj];
    en.hay_pociones = elegir_pocion(a.mundo.inventario, 1, 100).id != 0;
    const i32 x = o.col<O_X>()[yo], y = o.col<O_Y>()[yo];
    for (Slot s = 0; s < o.tamano(); ++s) {
        if (s == yo) continue;
        const i32 d = distancia_a(x, y, o.col<O_X>()[s], o.col<O_Y>()[s]);
        if (es_monstruo(a, s)) {
            en.monstruos.push_back(d);
        } else if (tipo_objeto(a, s) == TIPO_PERSONAJE && o.col<O_ESTADO>()[s] != ESTADO_MUERTO) {
            const u8 hp = o.col<O_HP>()[s];
            en.personajes.push_back({normalizar_nombre(texto(a.mundo, o.col<O_NOMBRE>()[s])), d, hp <= 100 ? hp : -1});
        }
    }
    const TablaItems& t = a.mundo.inventario.items;
    for (Slot s = 0; s < t.tamano(); ++s)
        if (t.col<I_CANTIDAD>()[s]) en.items.push_back(nombre_item_normalizado(a, s));
    char buf[96];
    for (u32 i = 0; i < j.n_efectos; ++i) {
        if (j.efectos[i].fin <= ahora) continue;
        const size_t n = nombre_efecto_activo(a, j.efectos[i], buf, sizeof buf);
        en.efectos.push_back(normalizar_nombre({buf, n}));
    }
    // Los auto-hechizos que hay que renovar: como un casillero de la barra, y que se pueda (sabido, o que
    // quede la poción).
    for (App::ModoAuto::Auto& au : m.autos) {
        CasilleroAuto& c = au.c;
        if (!c.efecto_visto && resta_efecto_de_hechizo(j, c.hechizo, ahora)) c.efecto_visto = true;
        const bool hay = pocion_auto(c.hechizo) ? buscar_pocion_auto(a.mundo.inventario, c.hechizo) != 0 : j.sabe(c.hechizo);
        if (hay && elegir_autobuff(j, &c, 1, ahora) == 0) en.renovar.push_back(au.nombre);
    }
    return en;
}

// Se puede pedir un hechizo ahora: no hay otro en camino ni esperando su objetivo, y pasó la demora.
bool libre_para_hechizo(const App& a, u64 ahora) {
    return !a.hechizo_pendiente && !a.hechizo_apuntando && ahora >= a.hechizo_listo_ms && ahora >= a.fin_demora_hechizo;
}

bool en_reintento(const App& a, u32 id, u64 ahora) {
    for (const auto& [h, hasta] : a.modo_auto.reintentar)
        if (h == id && ahora < hasta) return true;
    return false;
}

// El objeto al que va un hechizo o un ítem; 0 si no hay (o, sin objetivo pedido, el que corresponde).
ObjId resolver_objetivo(App& a, const AccionAuto& ac, const Hechizo* h, Slot yo) {
    const TablaObjetos& o = a.mundo.objetos;
    switch (ac.objetivo) {
    case Objetivo::YO: return a.mundo.jugador.id;
    case Objetivo::OBJETIVO: return a.objetivo && !a.hablar_objetivo ? a.objetivo : monstruo_mas_cercano(a, yo);
    case Objetivo::MAS_CERCANO: return monstruo_mas_cercano(a, yo);
    case Objetivo::NOMBRE: {
        const Slot s = personaje_por_nombre(a, ac.objetivo_nombre);
        return s != SIN_SLOT ? o.ids()[s] : 0;
    }
    case Objetivo::ALIADO_MAS_HERIDO: {
        ObjId mejor = 0;
        int peor = 101;
        for (Slot s = 0; s < o.tamano(); ++s)
            if (o.ids()[s] != a.mundo.jugador.id && tipo_objeto(a, s) == TIPO_PERSONAJE && o.col<O_HP>()[s] < peor) {
                peor = o.col<O_HP>()[s];
                mejor = o.ids()[s];
            }
        return mejor;
    }
    case Objetivo::NINGUNO:
        if (!h || !h->con_objetivo) return 0;
        // Sin objetivo pedido: el de ataque a lo que se ataca o al más cercano; el que no es de ataque, a sí mismo.
        if (h->de_ataque) return a.objetivo && !a.hablar_objetivo ? a.objetivo : monstruo_mas_cercano(a, yo);
        return a.mundo.jugador.id;
    }
    return 0;
}

void detener(App& a) {
    a.objetivo = 0;
    a.ataque_sostenido = a.hablar_objetivo = a.objetivo_fijo = false;
    a.paso_auto = -1;
    soltar_destino(a.modo_auto.ruta);
}

void terminar(App& a, std::string_view por) {
    App::ModoAuto& m = a.modo_auto;
    if (!m.cargado) return;
    detener(a);
    m.cargado = false;
    m.cola.clear();
    m.estado.clear();
    aviso(a, por);
}

void pausar(App& a, std::string_view por) {
    App::ModoAuto& m = a.modo_auto;
    if (!m.cargado || m.est.pausado) return;
    m.est.pausado = true;
    m.cola.clear();
    detener(a);
    aviso(a, por);
}

PuedePasar puede_pasar(App& a) {
    // Las celdas con alguien parado (monstruos, personajes, NPCs) tampoco: el servidor no deja pisarlas.
    std::vector<u64> ocupadas;
    const TablaObjetos& o = a.mundo.objetos;
    for (Slot s = 0; s < o.tamano(); ++s)
        if (o.ids()[s] != a.mundo.jugador.id && o.col<O_ESTADO>()[s] != ESTADO_MUERTO && tipo_objeto(a, s) != TIPO_ITEM)
            ocupadas.push_back(static_cast<u64>(o.col<O_X>()[s]) << 16 | o.col<O_Y>()[s]);
    std::sort(ocupadas.begin(), ocupadas.end());
    return paso_por_celdas([&a, ocupadas = std::move(ocupadas)](i32 x, i32 y) {
        if (x < 0 || y < 0 || x > 0xFFFF || y > 0xFFFF) return false;
        if (std::binary_search(ocupadas.begin(), ocupadas.end(), static_cast<u64>(x) << 16 | static_cast<u64>(y))) return false;
        return celda_pasable(a.mundo, a.rec, x, y);
    });
}

// Un paso hacia el destino fijado (o ninguno).
void pedir_paso(App& a, Slot yo, u64 ahora) {
    const TablaObjetos& o = a.mundo.objetos;
    a.paso_auto = siguiente_paso(a.modo_auto.ruta, o.col<O_X>()[yo], o.col<O_Y>()[yo], puede_pasar(a), ahora);
}

void mover(App& a, Slot yo, u64 ahora) {
    App::ModoAuto& m = a.modo_auto;
    const TablaObjetos& o = a.mundo.objetos;
    const i32 x = o.col<O_X>()[yo], y = o.col<O_Y>()[yo];
    const Movimiento& mv = m.p.movimiento;
    switch (mv.tipo) {
    case TipoMovimiento::QUIETO: soltar_destino(m.ruta); return;
    case TipoMovimiento::RECORRER:
        // Un punto al azar nuevo al llegar o si al anterior no hay camino.
        if (!m.recorriendo || distancia_a(x, y, m.destino.first, m.destino.second) <= 1 || m.ruta.sin_camino) {
            auto pisable = [&a](i32 px, i32 py) { return px >= 0 && py >= 0 && celda_pasable(a.mundo, a.rec, px, py); };
            m.recorriendo = punto_al_azar(x, y, 6, 16, pisable, m.semilla, m.destino);
            soltar_destino(m.ruta);
            if (!m.recorriendo) return;
        }
        fijar_destino(m.ruta, m.destino.first, m.destino.second, 0);
        break;
    case TipoMovimiento::PUNTOS: {
        const auto* p = punto_actual(mv, m.est, x, y);
        if (!p) {
            soltar_destino(m.ruta);
            return;
        }
        fijar_destino(m.ruta, p->first, p->second, 1);
        break;
    }
    case TipoMovimiento::SEGUIR: {
        const Slot s = personaje_por_nombre(a, mv.seguir);
        if (s == SIN_SLOT) {  // no se lo ve: espera
            soltar_destino(m.ruta);
            return;
        }
        fijar_destino(m.ruta, o.col<O_X>()[s], o.col<O_Y>()[s], mv.distancia);
        break;
    }
    }
    pedir_paso(a, yo, ahora);
}

// Una acción de la cola. true si ya terminó (sale de la cola); false si espera (un hechizo hasta poder
// pedirlo, "ir a" hasta llegar).
bool ejecutar(App& a, const AccionAuto& ac, Slot yo, u64 ahora, bool repetida) {
    App::ModoAuto& m = a.modo_auto;
    const TablaObjetos& o = a.mundo.objetos;
    switch (ac.tipo) {
    case TipoAccion::ATACAR: {
        const Slot obj = a.objetivo ? o.buscar(a.objetivo) : SIN_SLOT;
        if (obj == SIN_SLOT || a.hablar_objetivo || !es_monstruo(a, obj)) {
            const ObjId t = monstruo_mas_cercano(a, yo);
            if (!t) return true;
            a.objetivo = t;
            a.hablar_objetivo = false;
        }
        a.ataque_sostenido = true;
        soltar_destino(m.ruta);
        return true;
    }
    case TipoAccion::LANZAR: {
        const u32 id = hechizo_por_nombre(a, ac.nombre);
        const Hechizo& h = a.hechizos[id];
        if (!id || !a.mundo.jugador.sabe(id) || en_reintento(a, id, ahora)) return true;
        // Sostenida: un buff que ya está puesto no se relanza (Meditate espera; Fire Storm se repite). Si su
        // efecto no se ve (sin ícono), tampoco antes de su duración desde la última vez.
        if (repetida && !h.de_ataque) {
            if (resta_efecto_de_hechizo(a.mundo.jugador, id, ahora)) return true;
            for (const auto& [hid, cuando] : m.lanzados)
                if (hid == id && ahora < cuando + std::max<u64>(h.duracion_s * 1000ull, ESPERA_RESPUESTA_MS)) return true;
        }
        if (a.mundo.jugador.mp < h.mp) return true;
        if (!libre_para_hechizo(a, ahora)) return false;
        const ObjId t = resolver_objetivo(a, ac, &h, yo);
        if (h.con_objetivo && !t) return true;  // sin a quién: pedir_hechizo esperaría un clic
        pedir_hechizo(a, id, t);
        m.ultimo_hechizo = id;
        m.ultimo_auto = -1;
        m.ultimo_ms = ahora;
        std::erase_if(m.lanzados, [id](const auto& l) { return l.first == id; });
        m.lanzados.emplace_back(id, ahora);
        return true;
    }
    case TipoAccion::USAR: {
        if (const ObjId it = buscar_item(a, ac.nombre)) usar_item(a, it);
        return true;
    }
    case TipoAccion::EQUIPAR: {
        const ObjId it = buscar_item(a, ac.nombre);
        const Slot s = it ? a.mundo.inventario.items.buscar(it) : SIN_SLOT;
        if (s != SIN_SLOT && !item_equipado(nombre_item(a.mundo.inventario, s))) usar_item(a, it);
        return true;
    }
    case TipoAccion::DETENER: detener(a); return true;
    case TipoAccion::TERMINAR: m.est.terminado = true; return true;
    case TipoAccion::IR_A:
        fijar_destino(m.ruta, ac.x, ac.y, 0);
        pedir_paso(a, yo, ahora);
        return distancia_a(o.col<O_X>()[yo], o.col<O_Y>()[yo], ac.x, ac.y) == 0 || a.paso_auto < 0;
    default: return true;  // esperar y las de control (pulsar_tecla ya las aplicó)
    }
}

void renovar_auto(App& a, const Decision& d, Slot yo, u64 ahora) {
    App::ModoAuto& m = a.modo_auto;
    for (size_t k = 0; k < m.autos.size(); ++k) {
        App::ModoAuto::Auto& au = m.autos[k];
        if (au.regla != d.indice || au.nombre != d.auto_hechizo) continue;
        if (pocion_auto(au.c.hechizo)) {
            if (const ObjId it = buscar_pocion_auto(a.mundo.inventario, au.c.hechizo)) {
                usar_item(a, it);
                au.c.lanzado_ms = ahora;
            }
            return;
        }
        if (!libre_para_hechizo(a, ahora)) return;
        const Hechizo& h = a.hechizos[au.c.hechizo];
        pedir_hechizo(a, au.c.hechizo, h.con_objetivo ? a.mundo.jugador.id : 0);
        au.c.lanzado_ms = ahora;
        m.ultimo_hechizo = au.c.hechizo;
        m.ultimo_auto = static_cast<int>(k);
        m.ultimo_ms = ahora;
        return;
    }
}

void describir(App& a, const Decision& d) {
    App::ModoAuto& m = a.modo_auto;
    char t[160];
    switch (d.origen) {
    case OrigenDecision::REGLA:
        std::snprintf(t, sizeof t, "rule %d (line %d)%s", d.indice + 1, m.p.reglas[static_cast<size_t>(d.indice)].linea,
                      d.continua ? " ..." : "");
        break;
    case OrigenDecision::TECLA: std::snprintf(t, sizeof t, "key %s%s", m.p.teclas[static_cast<size_t>(d.indice)].tecla.c_str(), d.continua ? " ..." : ""); break;
    case OrigenDecision::AUTO_HECHIZO: std::snprintf(t, sizeof t, "auto-spell %s", d.auto_hechizo.c_str()); break;
    case OrigenDecision::MOVER: std::snprintf(t, sizeof t, "moving"); break;
    case OrigenDecision::NADA:
        std::snprintf(t, sizeof t, "%s", m.p.mapa >= 0 && a.mundo.mapa != m.p.mapa ? "waiting (other map)" : "idle");
        break;
    }
    m.estado = t;
}

void cargar(App& a, std::string_view nombre) {
    App::ModoAuto& m = a.modo_auto;
    std::string carpeta = ruta_configuracion();
    const size_t barra = carpeta.find_last_of("/\\");
    carpeta = barra == std::string::npos ? std::string() : carpeta.substr(0, barra + 1);
    const std::string ruta = carpeta + "planes/" + std::string(nombre) + ".txt";
    std::ifstream f(ruta, std::ios::binary);
    if (!f) {
        aviso(a, "Auto: can't open " + ruta);
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string texto = ss.str();

    if (m.hechizos.empty())
        for (u32 id = 1; id <= CANTIDAD_HECHIZOS; ++id)
            if (a.hechizos[id].existe()) m.hechizos.emplace_back(normalizar_nombre(a.hechizos[id].nombre), id);
    std::vector<ErrorPrograma> errores;
    Programa p = leer_programa(texto, errores);
    validar_programa(p, [&a](std::string_view n) { return hechizo_por_nombre(a, std::string(n)) != 0; }, errores);
    if (!p.mapa_nombre.empty()) errores.push_back({0, "map by name is not supported yet: use the map number"});
    if (!errores.empty()) {
        aviso(a, "Auto: " + std::string(nombre) + " has errors, not started:");
        for (size_t i = 0; i < errores.size() && i < 8; ++i)
            aviso(a, "  line " + std::to_string(errores[i].linea) + ": " + errores[i].mensaje);
        return;
    }
    terminar(a, "Auto: previous program ended.");
    m = {};  // también olvida los nombres de hechizos: se vuelven a leer (por si cambió la tabla)
    m.hechizos.clear();
    for (u32 id = 1; id <= CANTIDAD_HECHIZOS; ++id)
        if (a.hechizos[id].existe()) m.hechizos.emplace_back(normalizar_nombre(a.hechizos[id].nombre), id);
    m.p = std::move(p);
    m.archivo = nombre;
    m.de = nombre_personaje(a);
    m.semilla ^= static_cast<u32>(a.entrada.ahora_ms);
    for (size_t i = 0; i < m.p.reglas.size(); ++i)
        for (const AutoHechizo& h : m.p.reglas[i].auto_hechizos) {
            App::ModoAuto::Auto au{static_cast<int>(i), h.nombre, {}};
            if (const u32 pid = pocion_por_nombre(h.nombre)) {
                au.c.hechizo = pid;
                au.c.duracion_ms = pocion_auto(pid)->duracion_s * 1000ull;
            } else {
                au.c.hechizo = hechizo_por_nombre(a, h.nombre);
                au.c.mp = a.hechizos[au.c.hechizo].mp;
                au.c.duracion_ms = a.hechizos[au.c.hechizo].duracion_s * 1000ull;
            }
            m.autos.push_back(std::move(au));
        }
    // El arma del programa, si no está puesta.
    if (!m.p.arma.empty()) {
        AccionAuto eq;
        eq.tipo = TipoAccion::EQUIPAR;
        eq.nombre = m.p.arma;
        m.cola.push_back(std::move(eq));
    }
    m.cargado = true;
    aviso(a, "Auto: " + (m.p.nombre.empty() ? m.archivo : m.p.nombre) + " started. /auto pauses, /auto off ends.");
}

}  // namespace

bool comando_modo_auto(App& a, std::string_view orden, std::string_view arg) {
    if (!igual(orden, "auto")) return false;
    App::ModoAuto& m = a.modo_auto;
    while (!arg.empty() && arg.back() == ' ') arg.remove_suffix(1);
    if (igual(arg, "off") || igual(arg, "fin") || igual(arg, "end")) {
        if (m.cargado) terminar(a, "Auto: ended.");
        else aviso(a, "Auto: not running.");
    } else if (!arg.empty()) {
        cargar(a, arg);
    } else if (!m.cargado) {
        aviso(a, "Auto: /auto <program> runs planes/<program>.txt (next to settings.ini).");
    } else if (m.est.pausado) {
        m.est.pausado = false;
        aviso(a, "Auto: resumed.");
    } else {
        pausar(a, "Auto: paused. /auto resumes.");
    }
    return true;
}

bool modo_auto_activo(const App& a) { return a.modo_auto.cargado && !a.modo_auto.est.pausado; }

bool modo_auto_deja_pociones(const App& a) {
    if (!modo_auto_activo(a)) return true;
    switch (a.modo_auto.p.pociones) {
    case ModoPociones::SIEMPRE: return true;
    case ModoPociones::NUNCA: return false;
    case ModoPociones::ATACANDO: return a.objetivo != 0 && !a.hablar_objetivo;
    }
    return true;
}

void modo_auto_intervencion(App& a) {
    if (modo_auto_activo(a)) pausar(a, "Auto: paused (you took control). /auto resumes.");
}

void procesar_modo_auto(App& a, u64 ahora) {
    a.paso_auto = -1;
    App::ModoAuto& m = a.modo_auto;
    if (!m.cargado) return;
    if (m.de != nombre_personaje(a)) {
        terminar(a, "Auto: ended (another character).");
        return;
    }
    if (a.mundo.jugador.muerto) {
        terminar(a, "Auto: ended (dead).");
        return;
    }
    EntradaFrame& e = a.entrada;
    const Slot yo = a.mundo.objetos.buscar(a.mundo.jugador.id);
    if (yo == SIN_SLOT) return;
    // Las teclas del programa, antes que la barra y las F5–F12 (que no las ven).
    if (e.tecla[0] && !a.escribiendo_chat && !escribiendo_en_ventana(a) && !a.cantidad.abierto) {
        bool usada = false;
        const bool pausado_antes = m.est.pausado;
        const Decision d = pulsar_tecla(m.p, m.est, e.tecla, usada);
        if (usada) {
            e.numero = e.alt_numero = e.tecla_f = 0;
            e.tecla_a = false;
            if (m.est.pausado != pausado_antes) {
                if (m.est.pausado) {
                    m.est.pausado = false;  // pausar() lo vuelve a poner con el aviso y deteniendo todo
                    pausar(a, "Auto: paused.");
                } else {
                    aviso(a, "Auto: resumed.");
                }
            }
            if (d.acciones) {
                m.cola = *d.acciones;
                m.cola_origen = d.origen;
                m.cola_indice = d.indice;
            }
        }
    }
    // Un atajo de la barra (que no es del programa): el jugador interviene. Escribiendo, un número es texto.
    if ((e.numero || e.alt_numero || e.tecla_f) && !a.escribiendo_chat && !escribiendo_en_ventana(a) && !a.cantidad.abierto)
        modo_auto_intervencion(a);
    if (m.est.terminado) {
        terminar(a, "Auto: ended.");
        return;
    }
    if (m.est.pausado) return;

    const EntornoAuto en = armar_entorno(a, yo, ahora);
    const Decision d = decidir(m.p, m.est, en, ahora);
    describir(a, d);
    switch (d.origen) {
    case OrigenDecision::REGLA:
    case OrigenDecision::TECLA:
        // Otra regla (o la misma de nuevo, si la cola ya se vació): sus acciones a la cola.
        if (m.cola.empty() || d.origen != m.cola_origen || d.indice != m.cola_indice) {
            m.cola = *d.acciones;
            m.cola_origen = d.origen;
            m.cola_indice = d.indice;
        }
        break;
    case OrigenDecision::AUTO_HECHIZO:
        if (m.cola.empty()) renovar_auto(a, d, yo, ahora);
        break;
    case OrigenDecision::MOVER:
        if (m.cola.empty()) mover(a, yo, ahora);
        break;
    case OrigenDecision::NADA:
        if (m.cola.empty()) soltar_destino(m.ruta);
        break;
    }
    // La cola: las acciones que terminan en el acto salen todas en este frame; la primera que espera, queda.
    while (!m.cola.empty()) {
        const bool repetida = d.continua && (d.origen == OrigenDecision::REGLA || d.origen == OrigenDecision::TECLA);
        if (!ejecutar(a, m.cola.front(), yo, ahora, repetida)) break;
        m.cola.erase(m.cola.begin());
    }
    if (m.cola.empty()) m.cola_origen = OrigenDecision::NADA;
    if (m.est.terminado) terminar(a, "Auto: ended.");
}

// "MP insuficiente" (278) o "el hechizo falló" (280) poco después de pedir uno: ese hechizo espera
// ESPERA_FALLO_MS (y el auto-hechizo deja de contar como lanzado).
void eventos_modo_auto(App& a, const Evento& e) {
    App::ModoAuto& m = a.modo_auto;
    if (!m.cargado || e.tipo != Ev::MENSAJE_SERVIDOR || (e.v[MS_ID] != 278 && e.v[MS_ID] != 280) || !m.ultimo_hechizo) return;
    const u64 ahora = a.entrada.ahora_ms;
    if (ahora - m.ultimo_ms > ESPERA_RESPUESTA_MS) return;
    if (m.ultimo_auto >= 0 && static_cast<size_t>(m.ultimo_auto) < m.autos.size()) {
        CasilleroAuto& c = m.autos[static_cast<size_t>(m.ultimo_auto)].c;
        c.lanzado_ms = 0;
        c.reintentar_ms = ahora + ESPERA_FALLO_MS;
    } else {
        std::erase_if(m.reintentar, [&](const auto& r) { return r.first == m.ultimo_hechizo; });
        m.reintentar.emplace_back(m.ultimo_hechizo, ahora + ESPERA_FALLO_MS);
    }
    m.ultimo_hechizo = 0;
}

void dibujar_modo_auto(App& a) {
    const App::ModoAuto& m = a.modo_auto;
    if (!m.cargado) return;
    char t[256];
    std::snprintf(t, sizeof t, "Auto: %s - %s", m.p.nombre.empty() ? m.archivo.c_str() : m.p.nombre.c_str(),
                  m.est.pausado ? "paused (/auto resumes)" : m.estado.c_str());
    const int w = ancho_texto(t, std::strlen(t));
    a.g->texto_sombra(t, static_cast<float>((a.gpu.ancho() - w) / 2), 28.0f, clave_orden(CAPA_UI_TEXTO, 40),
                      m.est.pausado ? rgba(255, 210, 120) : COLOR_AVISO);
}

}  // namespace lin
