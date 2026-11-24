// Entregable 6.5: uso de hechizos y habilidades.
//  - Ventana de hechizos (Alt+S): los que sabe el personaje (S_AddSkill), por nivel, con su costo.
//    Clic lanza; con el puntero sobre uno, F5–F12 lo asigna a esa tecla (otra vez la quita).
//  - F5–F12 lanzan el hechizo asignado (se guardan por personaje en settings.ini).
//  - Un hechizo con objetivo espera el clic sobre un personaje o monstruo (clic derecho cancela);
//    si está lejos, se camina hasta tenerlo en alcance y se lanza.
//  - Los efectos activos (haste, escudos...) se listan arriba a la izquierda con lo que les queda.
#include <charconv>
#include <cstring>
#include "mundo/ataque.h"
#include "mundo/tabla_buffs.h"
#include "app/app.h"

namespace lin {

namespace {

constexpr int FILA = ALTO_GLIFO + 3;
constexpr int LISTA_X = 14, LISTA_Y = 30, LISTA_FILAS = 14;  // 30 + 14 × 17 px + pie: dentro de los 304 px
constexpr int BARRA_X = ui::VENTANA_ANCHO - 12;

// Hechizos que sabe el personaje, en orden (para la ventana).
int hechizos_sabidos(const App& a, u16* ids, int cap) {
    int n = 0;
    for (u32 id = 1; id <= CANTIDAD_HECHIZOS && n < cap; ++id)
        if (a.mundo.jugador.sabe(id) && a.hechizos[id].existe()) ids[n++] = static_cast<u16>(id);
    return n;
}

// Habilidades sin entrada en el spellicon.idx del cliente (las habilidades de caballero de este juego de recursos):
// una casilla con las primeras letras de las dos primeras palabras del nombre, una palabra por línea.
void dibujar_etiqueta_hechizo(App& a, const Hechizo& h, int x, int y, u64 clave, u32 color) {
    std::string_view n = h.nombre;
    for (int linea = 0; linea < 2 && !n.empty(); ++linea) {
        const size_t fin = n.find(' ');
        const std::string_view palabra = n.substr(0, fin);
        a.g->texto(palabra.substr(0, 4), static_cast<float>(x + 3), static_cast<float>(y + 4 + linea * (ALTO_GLIFO + 2)), clave, color);
        n = fin == std::string_view::npos ? std::string_view{} : n.substr(fin + 1);
    }
}

// settings.ini: atajos=Nombre:4,3,0,0,0,0,0,0,...;Otro:...  Un hechizo es su id; un objeto es "i<gfx>:<id>". 24 entradas: las tres
// páginas de ocho casillas (el original guarda 3 páginas de teclas F, 0x4b2240); a.atajos / a.items_f tienen la página visible.
void cargar_atajos(App& a) {
    a.atajos_de = nombre_personaje(a);
    std::memset(a.atajos_pg, 0, sizeof a.atajos_pg);
    for (auto& pagina : a.items_pg) for (Atajo& b : pagina) b = {};
    std::string_view v = valor_de_personaje(a.cfg.atajos, a.atajos_de);
    for (int i = 0; i < 24 && !v.empty(); ++i) {
        const size_t coma = v.find(',');
        const std::string_view t = v.substr(0, coma);
        const char* fin = t.data() + t.size();
        if (!t.empty() && t[0] == 'i') {
            Atajo& b = a.items_pg[i / 8][i % 8];
            auto r = std::from_chars(t.data() + 1, fin, b.gfx);
            if (r.ptr < fin && *r.ptr == ':' && std::from_chars(r.ptr + 1, fin, b.id).ec == std::errc{}) b.tipo = TipoAtajo::ITEM;
        } else {
            u16 id = 0;
            std::from_chars(t.data(), fin, id);
            a.atajos_pg[i / 8][i % 8] = id;
        }
        v = coma == std::string_view::npos ? std::string_view{} : v.substr(coma + 1);
    }
    a.pag_f = 0;
    std::memcpy(a.atajos, a.atajos_pg[0], sizeof a.atajos);
    std::copy(std::begin(a.items_pg[0]), std::end(a.items_pg[0]), a.items_f);
}

void guardar_atajos(App& a) {
    std::memcpy(a.atajos_pg[a.pag_f], a.atajos, sizeof a.atajos);
    std::copy(std::begin(a.items_f), std::end(a.items_f), a.items_pg[a.pag_f]);
    std::string v;
    for (int i = 0; i < 24; ++i) {
        const Atajo& b = a.items_pg[i / 8][i % 8];
        if (b.tipo == TipoAtajo::ITEM) v += 'i' + std::to_string(b.gfx) + ':' + std::to_string(b.id);
        else v += std::to_string(a.atajos_pg[i / 8][i % 8]);
        if (i < 23) v += ',';
    }
    poner_valor_de_personaje(a.cfg.atajos, a.atajos_de, v);
    guardar_config(a.cfg);
}

// Casilla del inventario de un objeto de casilla F: por id o, si ya no está, otro objeto con el mismo gfx (la casilla
// pasa a seguir ese id).
Slot item_de_tecla_f(App& a, Atajo& b) {
    const TablaItems& t = a.mundo.inventario.items;
    Slot s = t.buscar(b.id);
    if (s != SIN_SLOT) return s;
    for (Slot i = 0; i < t.tamano(); ++i) {
        if (t.col<I_GFX>()[i] == b.gfx) {
            b.id = t.ids()[i];
            guardar_atajos(a);
            return i;
        }
    }
    return SIN_SLOT;
}

void usar_tecla_f(App& a, int k) {
    Atajo& b = a.items_f[k];
    if (b.tipo == TipoAtajo::ITEM) {
        if (Slot s = item_de_tecla_f(a, b); s != SIN_SLOT) usar_item(a, a.mundo.inventario.items.ids()[s]);
    } else if (a.atajos[k]) {
        pedir_hechizo(a, a.atajos[k], 0);
    }
}

}  // namespace

// Flechas de página de teclas F (0x456960): la flecha arriba (1021) muestra la página siguiente, la de abajo (1023) la anterior, de 3.
void cambiar_pagina_f(App& a, int delta) {
    std::memcpy(a.atajos_pg[a.pag_f], a.atajos, sizeof a.atajos);
    std::copy(std::begin(a.items_f), std::end(a.items_f), a.items_pg[a.pag_f]);
    a.pag_f = static_cast<u8>((a.pag_f + delta + 3) % 3);
    std::memcpy(a.atajos, a.atajos_pg[a.pag_f], sizeof a.atajos);
    std::copy(std::begin(a.items_pg[a.pag_f]), std::end(a.items_pg[a.pag_f]), a.items_f);
}

// Primer hechizo sabido cuyo nombre contenga `texto` (sin distinguir mayúsculas); 0 si no hay.
u32 buscar_hechizo(const App& a, std::string_view texto) {
    auto minuscula = [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c); };
    for (u32 id = 1; id <= CANTIDAD_HECHIZOS; ++id) {
        std::string_view n = a.hechizos[id].nombre;
        if (!a.mundo.jugador.sabe(id) || n.size() < texto.size()) continue;
        for (size_t i = 0; i + texto.size() <= n.size(); ++i) {
            size_t k = 0;
            while (k < texto.size() && minuscula(n[i + k]) == minuscula(texto[k])) ++k;
            if (k == texto.size()) return id;
        }
    }
    return 0;
}

namespace {

int distancia(const TablaObjetos& o, Slot a, Slot b) {
    return std::max(std::abs(o.col<O_X>()[a] - o.col<O_X>()[b]), std::abs(o.col<O_Y>()[a] - o.col<O_Y>()[b]));
}

// La ficha del hechizo sin los códigos de color (\fX) y con ^ como salto de línea.
size_t ficha_limpia(std::string_view f, char* buf, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; i < f.size() && n + 1 < cap; ++i) {
        if (f[i] == '\\' && i + 2 < f.size() && f[i + 1] == 'f') {
            i += 2;
            continue;
        }
        buf[n++] = f[i] == '^' ? '\n' : f[i];
    }
    return n;
}

// Bloque de texto con fondo (varias líneas separadas por \n) en (x, y), dentro de la pantalla.
void cuadro_texto(App& a, std::string_view t, int x, int y) {
    Graficos& g = *a.g;
    int ancho = 0, lineas = 0;
    for (std::string_view r = t; !r.empty(); ++lineas) {
        std::string_view l = r.substr(0, r.find('\n'));
        ancho = std::max(ancho, ancho_texto(l.data(), l.size()));
        r = l.size() < r.size() ? r.substr(l.size() + 1) : std::string_view{};
    }
    x = std::min(x, a.gpu.ancho() - ancho - 8);
    y = std::min(y, a.gpu.alto() - lineas * FILA - 8);
    // Encima de todas las ventanas: dibujado con el nivel de la ventana de magias, su texto (que va
    // sobre cualquier fondo del mismo nivel) se vería a través del cuadro.
    const u32 nivel = a.gpu.nivel_ventana();
    a.gpu.nivel_ventana(NIVEL_SOBRE_VENTANAS);
    g.gpu().rectangulo(x - 4, y - 3, ancho + 8, lineas * FILA + 4, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 240));
    int fy = y;
    for (std::string_view r = t; !r.empty(); fy += FILA) {
        std::string_view l = r.substr(0, r.find('\n'));
        g.texto(l, static_cast<float>(x), static_cast<float>(fy), clave_orden(CAPA_UI_TEXTO, 30), rgba(230, 220, 190));
        r = l.size() < r.size() ? r.substr(l.size() + 1) : std::string_view{};
    }
    a.gpu.nivel_ventana(nivel);
}

}  // namespace

bool sobre_hechizos(const App& a) { return sobre_ventana(a, IdVentana::HECHIZOS); }

namespace {

// El arma o la armadura (cuerpo) equipada; 0 si no hay (el servidor contesta "no pasó nada").
ObjId item_equipado_de(const App& a, u8 uso) {
    const TablaItems& t = a.mundo.inventario.items;
    for (Slot s = 0; s < t.tamano(); ++s)
        if (t.col<I_USO>()[s] == uso && item_equipado(nombre_item(a.mundo.inventario, s))) return t.ids()[s];
    return 0;
}

}  // namespace

// Gráfico de un hechizo de área alrededor del lanzador (ver EF_AREA). El original lo reparte según
// el hechizo; la disposición no está en los datos (va en el ejecutable), así que es aproximada:
// - SpellTornado: 4 copias arriba, abajo, a la izquierda y a la derecha en pantalla, a la mitad del área;
// - SpellBlizzard: una cada 3 celdas en todo el círculo del área (10 celdas: la pantalla original).
static void dibujar_area(App& a, const Evento& e) {
    const GfxId gfx = static_cast<GfxId>(e.v[EF_GFX]);
    const Hechizo* h = nullptr;
    for (u32 id = 1; id <= CANTIDAD_HECHIZOS && !h; ++id)
        if (a.hechizos[id].gfx == gfx && a.hechizos[id].reparto != Hechizo::AREA_UNA) h = &a.hechizos[id];
    EfectoVisual ef;
    ef.gfx = gfx;
    ef.inicio = a.entrada.ahora_ms;
    auto poner = [&](int dx, int dy) {
        ef.x0 = ef.x1 = e.v[EF_X] + dx;
        ef.y0 = ef.y1 = e.v[EF_Y] + dy;
        agregar_efecto(a.mundo, ef);
    };
    if (!h) {
        poner(0, 0);
    } else if (h->reparto == Hechizo::AREA_CUATRO) {
        const int k = std::max(1, h->area / 2);  // en diagonal del mundo = recto en pantalla
        poner(k, k);
        poner(-k, -k);
        poner(k, -k);
        poner(-k, k);
    } else {
        const int r = std::max<int>(h->area, 1);
        for (int dy = -r + 1; dy < r; dy += 3)
            for (int dx = -r + 1; dx < r; dx += 3)
                if (dx * dx + dy * dy <= r * r) poner(dx, dy);
    }
}

void eventos_hechizos(App& a, const Evento& e) {
    // S_CurseBlind (0x409f00): the client itself prints string 147 / 148 when the blind state changes; l1j-en sends no text.
    if (e.tipo == Ev::CEGUERA) {
        const u8 estado = static_cast<u8>(e.v[0]);
        if (estado != a.ceguera_avisada && estado != 0)
            a.chat.agregar(a.mensajes.linea(estado == 2 ? 148 : 147), rgba(255, 255, 255), CHAT_SISTEMA);
        a.ceguera_avisada = estado;
    }
    if (e.tipo == Ev::MENSAJE_SERVIDOR) {
        const u8 k = a.msgs_recientes_n++ % 8;
        a.msgs_recientes[k] = static_cast<u16>(e.v[MS_ID]);
        a.msgs_recientes_ms[k] = a.entrada.ahora_ms;
    }
    if (e.tipo == Ev::ICONO_EFECTO && e.v[IE_CLASE] == IE_CONTEXTO) {  // S_PacketBox buff icons (see icono_de_caja)
        const TablaObjetos& o = a.mundo.objetos;
        const Slot s = o.buscar(a.mundo.jugador.id);
        const i32 extra[4] = {e.v[IE_EXTRA], e.v[IE_EXTRA + 1], e.v[IE_EXTRA + 2], e.v[IE_EXTRA + 3]};
        icono_de_caja(a.mundo.jugador, e.v[IE_TIPO], e.v[IE_SEG], extra, s != SIN_SLOT ? o.col<O_GFX>()[s] : 0, clase_personaje(a),
                      a.entrada.ahora_ms);
        return;
    }
    // Pociones de FUE/DES (Flora...): S_Strup/S_Dexup con el aumento en el lugar del tipo; el
    // ícono es el de la poción recién usada.
    if (e.tipo == Ev::ICONO_EFECTO && (e.v[IE_CLASE] == IE_FUERZA || e.v[IE_CLASE] == IE_DESTREZA) && e.v[IE_SEG] > 0 &&
        e.v[IE_TIPO] != 5 && e.v[IE_TIPO] != 2) {
        const u16 icono = a.entrada.ahora_ms - a.t_item_usado < 3000 ? a.gfx_item_usado : 0;
        (e.v[IE_CLASE] == IE_FUERZA ? a.icono_fuerza : a.icono_destreza) = icono;
        return;
    }
    if (e.tipo == Ev::EFECTO && e.v[EF_AREA]) {
        dibujar_area(a, e);
        return;
    }
    // Los buffs de los que el servidor no manda ícono (Advance Spirit, Light, muchos de elfo,
    // caballero y elfo oscuro): como el original, que trae la duración en spells, el ícono se pone
    // al ver el gráfico del hechizo (S_SkillSound) sobre el propio personaje poco después de lanzarlo.
    // Si el servidor también manda el suyo, se dibuja uno solo (dibujar_efectos_activos).
    // Sin graphic= en spells (Light), el gráfico lo pone el servidor: vale el primero sobre el
    // personaje en el segundo y medio siguiente.
    const u16 gfx = a.buff_propio ? a.hechizos[a.buff_propio].gfx : 0;
    if (e.tipo == Ev::EFECTO && a.buff_propio && e.id == a.mundo.jugador.id && (gfx == 0 || e.v[EF_GFX] == gfx)) {
        if (a.entrada.ahora_ms - a.hechizo_lanzado_ms < (gfx ? 3000u : 1500u))
            poner_efecto_activo(a.mundo.jugador, EA_HECHIZO, static_cast<u8>(a.buff_propio), a.hechizos[a.buff_propio].duracion_s,
                                a.entrada.ahora_ms);
        a.buff_propio = 0;
        return;
    }
    // El servidor no manda ícono de los buffs sobre el arma o la armadura (Enchant Weapon, Blessed
    // Armor, Bless Weapon...), solo el mensaje 161 ("%0 brilla..."): se registra el efecto con la
    // duración de spells.
    if (e.tipo != Ev::MENSAJE_SERVIDOR || e.v[MS_ID] != 161 || !a.buff_de_item) return;
    if (a.entrada.ahora_ms - a.hechizo_lanzado_ms < 3000)
        poner_efecto_activo(a.mundo.jugador, EA_HECHIZO, static_cast<u8>(a.buff_de_item), a.hechizos[a.buff_de_item].duracion_s,
                            a.entrada.ahora_ms);
    a.buff_de_item = 0;
}

void pedir_hechizo(App& a, u32 id, ObjId objetivo) {
    const Hechizo& h = a.hechizos[id];
    if (!a.mundo.jugador.sabe(id) || !h.existe() || a.mundo.jugador.muerto) return;
    a.hechizo_apuntando = 0;
    // Teleport con marcadores: primero el destino (marcadores_ui.cpp lo vuelve a pedir).
    if (es_hechizo_teleport(id) && !a.destino_elegido && elegir_destino(a, 0, id)) return;
    if (h.con_objetivo && objetivo == 0) {
        a.hechizo_apuntando = id;  // se elige con el próximo clic
        return;
    }
    a.hechizo_pendiente = id;
    a.objetivo_hechizo = h.con_objetivo ? objetivo : 0;
}

int paso_hechizo_pendiente(App& a, Slot yo, u64 ahora) {
    if (!a.hechizo_pendiente) return -1;
    TablaObjetos& o = a.mundo.objetos;
    const u32 id = a.hechizo_pendiente;
    const Hechizo& h = a.hechizos[id];
    Slot obj = a.objetivo_hechizo ? o.buscar(a.objetivo_hechizo) : SIN_SLOT;
    if (a.objetivo_hechizo && (obj == SIN_SLOT || o.col<O_ESTADO>()[obj] == ESTADO_MUERTO)) {
        a.hechizo_pendiente = 0;  // el objetivo desapareció o murió
        return -1;
    }
    if (obj != SIN_SLOT && h.alcance > 0 && distancia(o, yo, obj) > h.alcance) {
        // Lejos: un paso hacia el objetivo.
        return direccion_de(o.col<O_X>()[obj] - o.col<O_X>()[yo], o.col<O_Y>()[obj] - o.col<O_Y>()[yo]);
    }
    if (ahora < a.fin_demora_hechizo) return -2;  // esperar sin moverse
    a.hechizo_pendiente = 0;
    if (obj != SIN_SLOT && obj != yo) {
        int dir = direccion_de(o.col<O_X>()[obj] - o.col<O_X>()[yo], o.col<O_Y>()[obj] - o.col<O_Y>()[yo]);
        if (dir >= 0) o.col<O_DIR>()[yo] = static_cast<u8>(dir);
    }
    if (es_hechizo_teleport(id)) {
        enviar(a, cmd::hechizo_teleport(a.op, id, a.destino_mapa, a.destino_marcador));
        a.destino_elegido = false;
        a.destino_mapa = 0;
        a.destino_marcador = 0;
    } else if (h.sobre_item != Hechizo::SIN_ITEM) {
        // Encantar arma o armadura: el servidor lee el ítem en el lugar del objetivo (_itemobjid).
        enviar(a, cmd::usar_hechizo(a.op, id, item_equipado_de(a, h.sobre_item == Hechizo::SOBRE_ARMA ? USO_ARMA : USO_ARMADURA), 0, 0));
    } else {
        enviar(a, cmd::usar_hechizo(a.op, id, obj != SIN_SLOT ? o.ids()[obj] : 0, obj != SIN_SLOT ? o.col<O_X>()[obj] : 0,
                                    obj != SIN_SLOT ? o.col<O_Y>()[obj] : 0));
    }
    if (obj != SIN_SLOT && obj != yo) combate_musica(a, obj, a.entrada.ahora_ms);  // a spell at someone starts the combat music
    // Overweight: the server refuses (message 316) and sends no echo: no local animation, no pending buff.
    if (sobrecargado(a.mundo.jugador.peso)) return -2;
    a.buff_de_item = h.sobre_item != Hechizo::SIN_ITEM || h.sobre_arma_actual ? id : 0;
    const bool sobre_si = obj == SIN_SLOT || obj == yo;
    a.buff_propio = !a.buff_de_item && !h.de_ataque && h.duracion_s > 0 && sobre_si ? id : 0;
    // Animación propia ya (el eco del servidor no la reinicia) y sin otra acción hasta que termine:
    // el servidor mide el intervalo entre hechizos con la duración de esa animación.
    const u8 accion = h.de_ataque ? ACC_HECHIZO_ATAQUE : ACC_HECHIZO;
    o.col<O_ACCION>()[yo] = accion;
    o.col<O_TACCION>()[yo] = ahora;
    fijar_ritmo(a.mundo, yo);
    u32 ms = a.sprites.duracion_ms(o.col<O_GFX>()[yo], accion);
    a.fin_ataque = ahora + intervalo_propio(a.mundo.jugador, Intervalo::HECHIZO, ms ? ms : 1000);
    a.fin_demora_hechizo = ahora + h.demora_ms;
    a.hechizo_lanzado_ms = ahora;
    a.hechizo_listo_ms = std::max(a.fin_ataque, a.fin_demora_hechizo);
    return -2;
}

void procesar_hechizos(App& a, u64 ahora) {
    EntradaFrame& e = a.entrada;
    if (a.atajos_de != nombre_personaje(a)) cargar_atajos(a);
    vencer_efectos(a.mundo.jugador, ahora);
    // Mensajes de fin de efecto de la tabla de buffs del cliente original, salvo que el servidor ya haya impreso el mismo texto.
    for (u32 i = 0; i < a.mundo.jugador.n_fin_pendientes;) {
        EstadoJugador& j = a.mundo.jugador;
        if (j.fin_pendientes[i].due > ahora) {
            ++i;
            continue;
        }
        const u16 msg = j.fin_pendientes[i].msg;
        j.fin_pendientes[i] = j.fin_pendientes[--j.n_fin_pendientes];
        if (a.cfg.ui_simple) continue;
        const std::string_view texto = a.mensajes.linea(msg);
        bool repetido = texto.empty();
        for (int k = 0; k < 8 && !repetido; ++k)
            repetido = a.msgs_recientes[k] && ahora - a.msgs_recientes_ms[k] < 3000 && a.mensajes.linea(a.msgs_recientes[k]) == texto;
        if (repetido) continue;
        Evento m{};
        m.tipo = Ev::MENSAJE_SERVIDOR;
        m.v[MS_ID] = msg;
        char buf[256];
        u32 color = 0;
        const size_t n = formatear_mensaje(a.mensajes, a.desc, m, buf, sizeof buf, color);
        a.chat.agregar({buf, n}, color, CHAT_MENSAJE);
    }
    if (e.alt_s) a.ventana_hechizos = !a.ventana_hechizos;
    if (!e.presionado) a.clic_consumido = false;

    // F5–F12: asignar (con el puntero sobre un hechizo de la ventana) o lanzar.
    if (e.tecla_f >= 5 && e.tecla_f <= 12) {
        int i = e.tecla_f - 5;
        u32 bajo = hechizo_bajo_puntero(a);
        if (bajo) {
            poner_tecla_f(a, i, a.atajos[i] == bajo ? Atajo{} : Atajo{TipoAtajo::HECHIZO, bajo, 0});
        } else {
            usar_tecla_f(a, i);
        }
    }

    // Eligiendo el objetivo: clic sobre un personaje o monstruo (o sobre uno mismo); clic
    // derecho, Escape o un clic en el suelo cancelan.
    if (a.hechizo_apuntando) {
        if (e.clic_derecho || e.escape) {
            a.hechizo_apuntando = 0;
            e.escape = false;  // Esc cancela solo esto
        } else if (e.pulsado && !sobre_hechizos(a) && !sobre_inventario(a) && !sobre_barra(a)) {
            ObjId obj = objeto_bajo_puntero(a, true);
            if (obj) pedir_hechizo(a, a.hechizo_apuntando, obj);
            a.hechizo_apuntando = 0;
            a.clic_consumido = true;
        }
    }
}

void dibujar_arrastre(App& a);

// Casilleros F5–F12 del panel original (1019.img): 2 filas de 4, de 34 px, desde (17, 23).
constexpr int TECLAS_X = 17, TECLAS_Y = 23, TECLA = 34;

void poner_tecla_f(App& a, int k, Atajo atajo) {
    if (k < 0 || k >= 8) return;
    a.atajos[k] = atajo.tipo == TipoAtajo::HECHIZO ? static_cast<u16>(atajo.id) : 0;
    a.items_f[k] = atajo.tipo == TipoAtajo::ITEM ? atajo : Atajo{};
    guardar_atajos(a);
}

int tecla_f_bajo_puntero(const App& a) {
    if (a.teclas_f_y < 0) return -1;
    int cx = a.entrada.mx - (a.teclas_f_x + TECLAS_X), cy = a.entrada.my - (a.teclas_f_y + TECLAS_Y);
    if (cx < 0 || cy < 0 || cx >= 4 * TECLA || cy >= 2 * TECLA) return -1;
    return (cy / TECLA) * 4 + cx / TECLA;
}

void dibujar_espera_hechizo(App& a, int x, int y, int w, int h) {
    const u64 ahora = a.entrada.ahora_ms;
    if (ahora >= a.hechizo_listo_ms || a.hechizo_listo_ms <= a.hechizo_lanzado_ms) return;
    // Lo que falta, como una sombra que baja desde arriba hasta vaciarse.
    const u64 total = a.hechizo_listo_ms - a.hechizo_lanzado_ms, falta = a.hechizo_listo_ms - ahora;
    const int alto = static_cast<int>(h * falta / total);
    a.g->gpu().rectangulo(x, y + h - alto, w, alto, clave_orden(CAPA_UI, 7), rgba(0, 0, 0, 160));
    // Y los segundos que faltan, con centésimas: el número corre mientras se espera.
    {
        char t[12];
        std::snprintf(t, sizeof t, "%.2f", static_cast<double>(falta) / 1000.0);
        a.g->texto_sombra(t, static_cast<float>(x + (w - ancho_texto(t, std::strlen(t))) / 2),
                          static_cast<float>(y + (h - ALTO_GLIFO) / 2), clave_orden(CAPA_UI_TEXTO, 7), rgba(255, 255, 254));
    }
}

void dibujar_teclas_f(App& a, int x, int y) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    a.teclas_f_x = x;
    a.teclas_f_y = y;
    const int bajo = tecla_f_bajo_puntero(a);
    if (bajo >= 0 && e.pulsado && !a.cfg.ui_simple) a.tecla_f_sel = static_cast<u8>(bajo);  // 0x4b24d0: la última casilla cliqueada
    for (int k = 0; k < 8; ++k) {
        if (a.items_f[k].tipo == TipoAtajo::ITEM) {
            Atajo& b = a.items_f[k];
            const int cx = x + TECLAS_X + (k % 4) * TECLA, cy = y + TECLAS_Y + (k / 4) * TECLA;
            const Slot s = item_de_tecla_f(a, b);
            const TablaItems& t = a.mundo.inventario.items;
            g.gpu().dibujar(g.icono(b.gfx), cx, cy, clave_orden(CAPA_UI, 3), s != SIN_SLOT ? BLANCO : rgba(110, 110, 110));
            if (s != SIN_SLOT && t.col<I_CANTIDAD>()[s] > 1) {
                char c[16];
                std::snprintf(c, sizeof c, "%u", t.col<I_CANTIDAD>()[s]);
                g.texto_sombra(c, static_cast<float>(cx + TECLA - 4 - ancho_texto(c, std::strlen(c))),
                               static_cast<float>(cy + TECLA - 3 - ALTO_GLIFO), clave_orden(CAPA_UI_TEXTO, 3), rgba(255, 255, 254));
            }
            continue;
        }
        if (!a.atajos[k]) continue;
        const Hechizo& h = a.hechizos[a.atajos[k]];
        const int cx = x + TECLAS_X + (k % 4) * TECLA, cy = y + TECLAS_Y + (k / 4) * TECLA;
        const u32 tinte = a.mundo.jugador.mp >= h.mp ? BLANCO : rgba(110, 110, 110);
        if (h.icono) g.gpu().dibujar(g.icono(h.icono), cx, cy, clave_orden(CAPA_UI, 3), tinte);
        else dibujar_etiqueta_hechizo(a, h, cx, cy, clave_orden(CAPA_UI_TEXTO, 3), tinte);
        dibujar_espera_hechizo(a, cx, cy, 32, 32);
    }
    if (!a.cfg.ui_simple) {
        // Aspecto original: la casilla elegida lleva el marco de la imagen 1020 (0x4b24d0..0x4b2549) y una casilla llena bajo el puntero
        // muestra su nombre, blanco con contorno negro, en (-2,+11) de la casilla y dentro de la pantalla (0x4b2549..0x4b264a).
        const int sx = x + TECLAS_X + (a.tecla_f_sel % 4) * TECLA, sy = y + TECLAS_Y - 1 + (a.tecla_f_sel / 4) * TECLA;
        g.gpu().dibujar(g.img(1020), sx, sy, clave_orden(CAPA_UI, 8));
        const bool hay = bajo >= 0 && (a.atajos[bajo] || a.items_f[bajo].tipo == TipoAtajo::ITEM);
        if (hay && a.tecla_f_arrastrada < 0) {
            char nombre[256];
            std::string_view nom = a.hechizos[a.atajos[bajo]].nombre;
            if (a.items_f[bajo].tipo == TipoAtajo::ITEM) {
                const Slot s = item_de_tecla_f(a, a.items_f[bajo]);
                nom = s != SIN_SLOT ? nombre_item(a.mundo.inventario, s) : std::string_view();
                nom = std::string_view(nombre, expandir_desc(a.desc, nom, nombre, sizeof nombre));
            }
            const int w = ancho_texto(nom.data(), nom.size());
            const int tx = std::min(x + TECLAS_X - 2 + (bajo % 4) * TECLA, a.gpu.ancho() - w);
            g.texto_contorno(nom, static_cast<float>(tx), static_cast<float>(y + TECLAS_Y + 11 + (bajo / 4) * TECLA), clave_orden(CAPA_UI_TEXTO, 9), rgb555(0x7fff));
        }
    }
    // Arrastre desde una casilla: sobre otra casilla las intercambia, fuera de toda ventana la vacía; un clic simple la usa.
    auto lleno = [&](int k) { return a.atajos[k] || a.items_f[k].tipo == TipoAtajo::ITEM; };
    if (bajo >= 0 && lleno(bajo)) {
        if (e.pulsado && !a.arrastrando && !a.arrastrando_hechizo && a.arrastrando_atajo < 0) {
            a.tecla_f_arrastrada = bajo;
            a.arrastre_x0 = e.mx;
            a.arrastre_y0 = e.my;
        }
        if (e.clic_derecho) poner_tecla_f(a, bajo, {});
    }
    const int desde = a.tecla_f_arrastrada;
    if (desde >= 0 && e.presionado && arrastre_movido(a)) {
        const u16 icono = a.items_f[desde].tipo == TipoAtajo::ITEM ? a.items_f[desde].gfx : a.hechizos[a.atajos[desde]].icono;
        if (icono || a.items_f[desde].tipo == TipoAtajo::ITEM) g.gpu().dibujar(g.icono(icono), e.mx - 16, e.my - 16, clave_orden(CAPA_ARRASTRE, 0));
        else dibujar_etiqueta_hechizo(a, a.hechizos[a.atajos[desde]], e.mx - 16, e.my - 16, clave_orden(CAPA_ARRASTRE, 1), rgba(255, 230, 120));
    }
    if (desde >= 0 && e.clic) {
        a.tecla_f_arrastrada = -1;
        if (!arrastre_movido(a)) {
            if (bajo == desde) usar_tecla_f(a, desde);
        } else if (bajo >= 0) {
            std::swap(a.atajos[desde], a.atajos[bajo]);
            std::swap(a.items_f[desde], a.items_f[bajo]);
            guardar_atajos(a);
        } else if (!sobre_alguna_ventana(a) && !sobre_barra(a)) {
            poner_tecla_f(a, desde, {});
        }
    }
}

// Fila de la ventana bajo el puntero -> hechizo (0 si no hay).
// Libro de hechizos de la interfaz original (cliente 2009, 0x4af3a0): fondo 933 en (8,10), marco 934, los íconos
// de hechizos en una grilla de 4 x 6 celdas de 34 px, adornos 935 en (2,10) encima y los valores S.power / M.resist
// en las cajas impresas en 935. La franja de pestañas (0x47e860, en (16,38) a la izquierda de la grilla) tiene una imagen por
// pestaña elegida, según la clase: elfo 936 (11 pestañas), príncipe 947 (7), elfo oscuro 954 (5), el resto 959 (10); cada pestaña es
// una casilla de 19 px. Los hechizos van en tablas de casillas por clase (spellbook_skill_id). La grilla está en (37,43), la barra de scroll en (175,45) (arriba 974, riel 971, abajo 972, botón 976).
constexpr u32 IMG_SPELL_BG = 933, IMG_SPELL_FRAME = 934, IMG_SPELL_ORNAMENT = 935;
constexpr int SPELL_BG_X = 8, SPELL_BG_Y = 10;
constexpr int GRILLA_X = 37, GRILLA_Y = 43, CELDA = 34, COLUMNAS = 4, FILAS_GRILLA = 6;
constexpr int ALTO_PESTANA = 19, ANCHO_PESTANA = 19, PESTANAS_X = 16, PESTANAS_Y = 38;
constexpr int SPELL_CLOSE_X = 181, SPELL_CLOSE_Y = 18;
constexpr int SPELL_SCROLL_X = 175, SPELL_SCROLL_Y = 45, SPELL_SCROLL_H = 199;  // arriba 45, riel desde 53, abajo en 232 + 12

// Franja de pestañas de una clase: primera imagen y cantidad de pestañas (0x4aebe0). Clases: 0 príncipe, 1 caballero, 2 elfo, 3 mago,
// 4 elfo oscuro, 5 caballero dragón, 6 ilusionista. Los hechizos van en casillas fijas (spellbook_skill_id, 8 por pestaña en 2 filas de 4).
struct TabStrip {
    u32 base;
    int count;
};
static TabStrip tab_strip(int cls) {
    switch (cls) {
    case 0: return {947, 7};
    case 2: return {936, 11};
    case 4: return {954, 5};
    default: return {959, 10};
    }
}
static int spellbook_class(const App& a) {
    return a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) ? a.pjs[static_cast<size_t>(a.pj_sel)].v[PJ_CLASE] : -1;
}
constexpr int SP_BOX_X = 76, SP_BOX_W = 19, MR_BOX_X = 147, MR_BOX_W = 22, STAT_BOX_Y = 260, STAT_BOX_H = 12;

static bool ventana_original(const App& a) { return uses_original_frame(a, IdVentana::HECHIZOS); }

// Hechizo que se muestra en la celda (fila, col) del libro: solo los hechizos que el personaje conoce.
static u32 hechizo_en_celda(const App& a, int row, int col) {
    const u32 id = spellbook_skill_id(spellbook_class(a), row * COLUMNAS + col);
    return id && id < CANTIDAD_HECHIZOS && a.mundo.jugador.sabe(id) && a.hechizos[id].existe() ? id : 0;
}

static int primera_fila(const App& a) {
    return std::clamp(static_cast<int>(a.hechizos_desplazamiento), 0, spellbook_max_scroll(spellbook_class(a)));
}

u32 hechizo_bajo_puntero(const App& a) {
    if (!sobre_hechizos(a)) return 0;
    if (ventana_original(a)) {
        const Rect rv = rect_ventana(a, IdVentana::HECHIZOS);
        int cx = a.entrada.mx - (rv.x + GRILLA_X), cy = a.entrada.my - (rv.y + GRILLA_Y);
        if (cx < 0 || cy < 0 || cx >= COLUMNAS * CELDA || cy >= FILAS_GRILLA * CELDA) return 0;
        return hechizo_en_celda(a, primera_fila(a) + cy / CELDA, cx / CELDA);
    }
    u16 ids[CANTIDAD_HECHIZOS];
    int n = hechizos_sabidos(a, ids, CANTIDAD_HECHIZOS);
    const Rect rv = rect_ventana(a, IdVentana::HECHIZOS);
    const int y0 = rv.y;
    if (a.entrada.mx >= rv.x + BARRA_X) return 0;  // sobre la barra de scroll: ni lanza ni arrastra el hechizo
    int fila = (a.entrada.my - (y0 + LISTA_Y)) / FILA;
    if (a.entrada.my < y0 + LISTA_Y || fila >= LISTA_FILAS) return 0;
    int i = static_cast<int>(a.hechizos_desplazamiento) + fila;
    return i < n ? ids[i] : 0;
}

static void dibujar_original(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    if (draw_original_frame(a, x0, y0, IMG_SPELL_BG, SPELL_BG_X, SPELL_BG_Y, IMG_SPELL_FRAME, SPELL_CLOSE_X, SPELL_CLOSE_Y))
        a.ventana_hechizos = false;
    // Pestañas (0x47e9d0): la resaltada sigue a la primera fila visible / 2; un clic desplaza a la fila 2 * pestaña.
    const TabStrip ts = tab_strip(spellbook_class(a));
    const int shown = std::clamp(primera_fila(a) / 2, 0, ts.count - 1);
    g.gpu().dibujar(g.img(ts.base + static_cast<u32>(shown)), x0 + PESTANAS_X, y0 + PESTANAS_Y, clave_orden(CAPA_UI, 16));
    for (int p = 0; p < ts.count; ++p)
        if (e.clic && !a.arrastrando_hechizo &&
            ui::sobre(e, x0 + PESTANAS_X, y0 + PESTANAS_Y + p * ALTO_PESTANA, ANCHO_PESTANA, ALTO_PESTANA))
            a.hechizos_desplazamiento = static_cast<u32>(2 * p);
    {
        // Barra de scroll (0x47cd80, arte 974 / 971 / 972 / 976); también toma la rueda sobre la ventana.
        u32 pos = static_cast<u32>(primera_fila(a));
        original_scrollbar(a, a.scroll.hechizos, x0 + SPELL_SCROLL_X, y0 + SPELL_SCROLL_Y, SPELL_SCROLL_H,
                           static_cast<u32>(spellbook_max_scroll(spellbook_class(a)) + FILAS_GRILLA), FILAS_GRILLA, pos,
                           sobre_hechizos(a), 0, -2);
        a.hechizos_desplazamiento = pos;
    }
    const int first = primera_fila(a);
    const u32 bajo = hechizo_bajo_puntero(a);
    const u64 ki = clave_orden(CAPA_UI, 12), kt = clave_orden(CAPA_UI_TEXTO, 12);
    for (int r = 0; r < FILAS_GRILLA; ++r)
        for (int c = 0; c < COLUMNAS; ++c) {
            const u32 id = hechizo_en_celda(a, first + r, c);
            if (!id) continue;
            const int x = x0 + GRILLA_X + c * CELDA, y = y0 + GRILLA_Y + r * CELDA;
            const Hechizo& h = a.hechizos[id];
            if (id == bajo) g.gpu().rectangulo(x, y, CELDA - 1, CELDA - 1, ki - (1ull << 24), rgba(90, 75, 30, 200));
            // Sin MP suficiente el ícono se oscurece.
            const u32 tinte = a.mundo.jugador.mp >= h.mp ? BLANCO : rgba(110, 110, 110);
            if (h.icono) g.gpu().dibujar(g.icono(h.icono), x, y, ki, tinte);
            else dibujar_etiqueta_hechizo(a, h, x, y, kt, a.mundo.jugador.mp >= h.mp ? rgba(255, 230, 120) : tinte);
            for (int k = 0; k < 8; ++k)
                if (a.atajos[k] == id) {
                    char t[4];
                    std::snprintf(t, sizeof t, "F%d", k + 5);
                    g.texto_sombra(t, static_cast<float>(x + 1), static_cast<float>(y + CELDA - 1 - ALTO_GLIFO), kt,
                                   rgba(255, 230, 120));
                }
        }
    g.gpu().dibujar(g.img(IMG_SPELL_ORNAMENT), x0 + 2, y0 + 10, clave_orden(CAPA_UI, 15));
    // Poder mágico (SP) y resistencia mágica (MR) en las cajas impresas en los adornos.
    auto boxed = [&](int value, int bx, int bw) {
        char t[16];
        std::snprintf(t, sizeof t, "%d", value);
        const int w = ancho_texto(t, std::strlen(t));
        g.texto(t, static_cast<float>(x0 + bx + (bw - w) / 2), static_cast<float>(y0 + STAT_BOX_Y + (STAT_BOX_H - ALTO_GLIFO) / 2 + 1),
                clave_orden(CAPA_UI_TEXTO, 16), rgba(255, 255, 254));
    };
    boxed(a.mundo.jugador.sp, SP_BOX_X, SP_BOX_W);
    boxed(a.mundo.jugador.mr, MR_BOX_X, MR_BOX_W);
}

void dibujar_hechizos(App& a) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::HECHIZOS);
    const Rect rv = rect_ventana(a, IdVentana::HECHIZOS);
    const int x0 = rv.x, y0 = rv.y;
    if (ventana_original(a)) {
        dibujar_original(a, x0, y0);
        if (e.escape) a.ventana_hechizos = false;
        dibujar_arrastre(a);
        return;
    }
    if (ui::ventana(g, e, a.cfg.ui_simple, x0, y0, "Magic", opacidad_ventanas(a))) a.ventana_hechizos = false;
    u16 ids[CANTIDAD_HECHIZOS];
    int n = hechizos_sabidos(a, ids, CANTIDAD_HECHIZOS);
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12), kf = clave_orden(CAPA_UI, 12);
    if (n == 0) {
        g.texto("(no spells)", static_cast<float>(x0 + LISTA_X), static_cast<float>(y0 + LISTA_Y), kt, rgba(160, 150, 130));
        return;
    }
    barra_scroll_ventana(a, a.scroll.hechizos, x0 + BARRA_X, y0 + LISTA_Y, LISTA_FILAS * FILA, static_cast<u32>(n),
                         LISTA_FILAS, a.hechizos_desplazamiento, clave_orden(CAPA_UI, 14), sobre_hechizos(a));
    const u32 bajo = hechizo_bajo_puntero(a);
    for (int f = 0; f < LISTA_FILAS; ++f) {
        int i = static_cast<int>(a.hechizos_desplazamiento) + f;
        if (i >= n) break;
        const u32 id = ids[i];
        const Hechizo& h = a.hechizos[id];
        const int y = y0 + LISTA_Y + f * FILA;
        if (id == bajo) g.gpu().rectangulo(x0 + LISTA_X - 3, y - 1, ui::VENTANA_ANCHO - 2 * LISTA_X + 6, FILA, kf, rgba(60, 50, 35, 230));
        // Nivel (fila de S_AddSkill), nombre, costo y tecla asignada.
        char t[32];
        std::snprintf(t, sizeof t, "%u", (id - 1) / 8 + 1);
        g.texto(t, static_cast<float>(x0 + LISTA_X), static_cast<float>(y), kt, rgba(160, 150, 130));
        bool alcanza = a.mundo.jugador.mp >= h.mp;
        g.texto(h.nombre, static_cast<float>(x0 + LISTA_X + 18), static_cast<float>(y), kt,
                alcanza ? rgba(255, 255, 254) : rgba(150, 150, 150));
        std::snprintf(t, sizeof t, "%u", h.mp);
        int tecla = -1;
        for (int k = 0; k < 8; ++k)
            if (a.atajos[k] == id) tecla = k;
        if (tecla >= 0) std::snprintf(t + std::strlen(t), sizeof t - std::strlen(t), " F%d", tecla + 5);
        g.texto(t, static_cast<float>(x0 + ui::VENTANA_ANCHO - LISTA_X - ancho_texto(t, std::strlen(t))), static_cast<float>(y),
                kt, rgba(140, 180, 255));
    }
    char pie[48];
    std::snprintf(pie, sizeof pie, "MP %d/%d   F5-F12: assign", a.mundo.jugador.mp, a.mundo.jugador.mp_max);
    g.texto(pie, static_cast<float>(x0 + LISTA_X), static_cast<float>(y0 + LISTA_Y + LISTA_FILAS * FILA + 6), kt,
            rgba(214, 190, 140));
    dibujar_arrastre(a);
}

// Común a las dos ventanas: la ficha del hechizo bajo el puntero, arrastrarlo a la barra de
// atajos y, sin arrastrarlo, lanzarlo con un clic.
void dibujar_arrastre(App& a) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u32 bajo = hechizo_bajo_puntero(a);
    if (bajo && !a.arrastrando_hechizo) {
        char buf[512];
        size_t m = ficha_limpia(a.hechizos[bajo].ficha, buf, sizeof buf);
        if (m) cuadro_texto(a, {buf, m}, e.mx + 14, e.my + 16);
    }
    // Arrastrar un hechizo a la barra de atajos; sin arrastrarlo, clic lo lanza.
    if (e.pulsado && bajo) {
        a.arrastrando_hechizo = bajo;
        a.arrastre_x0 = e.mx;
        a.arrastre_y0 = e.my;
    }
    const bool movio = std::abs(e.mx - a.arrastre_x0) > 4 || std::abs(e.my - a.arrastre_y0) > 4;
    if (a.arrastrando_hechizo && e.presionado && movio) {
        const Hechizo& h = a.hechizos[a.arrastrando_hechizo];
        if (h.icono) g.gpu().dibujar(g.icono(h.icono), e.mx - 16, e.my - 16, clave_orden(CAPA_ARRASTRE, 0));
        else g.texto_sombra(h.nombre, static_cast<float>(e.mx), static_cast<float>(e.my), clave_orden(CAPA_ARRASTRE, 1), rgba(255, 230, 120));
    }
    if (e.clic && a.arrastrando_hechizo) {
        const u32 id = a.arrastrando_hechizo;
        a.arrastrando_hechizo = 0;
        if (!movio) {
            if (bajo == id) pedir_hechizo(a, id, 0);
        } else if (int c = casillero_bajo_puntero(a); c >= 0) {
            poner_atajo(a, c, {TipoAtajo::HECHIZO, id, 0});
        } else if (int ab = autobuff_bajo_puntero(a); ab >= 0) {
            poner_autobuff(a, ab, id);
        } else if (int f = tecla_f_bajo_puntero(a); f >= 0) {  // a una tecla F del panel original
            poner_tecla_f(a, f, {TipoAtajo::HECHIZO, id, 0});
        }
    }
}

// Hechizo que da el efecto: hechizo_de_efecto (mundo/autobuffs.cpp).

// Los que vienen de un ítem: su ícono de inventario (gfx de L1J: poción de brave 79, waffle de
// elfo 130, poción azul 75) y su nombre.
struct EfectoDeItem {
    u16 icono;
    std::string_view nombre;
};
// Casillas de la tabla de buffs del original (0x6e1020) que usan los íconos de S_PacketBox: id del ícono clásico (campo 0), el
// gemelo N.tbt de marco fino cuando se conoce (0 = ninguno) y un nombre para el tooltip.
struct RanuraBuff {
    u8 ranura;
    u16 clasico, gemelo;
    std::string_view nombre;
};
static constexpr RanuraBuff RANURAS[] = {
    {0x27, 1477, 0, "Polymorph"},       {0x4b, 2430, 0, "Polymorph"},    {0x29, 1562, 5286, "Chat ban"},
    {0x4a, 2382, 0, "Effect"},          {0x52, 2588, 0, "Effect"},       {0x5c, 2708, 0, "Effect"},
    {0x87, 3080, 0, "Effect"},          {0x0e, 813, 0, "Class skill"},   {0x11, 745, 0, "Class skill"},
    {0x0f, 804, 0, "Class skill"},      {0x10, 807, 0, "Class skill"},   {0x1f, 1122, 0, "Class skill"},
    {0x1d, 1101, 0, "Class skill"},     {0x1e, 1113, 0, "Class skill"},  {0x20, 1125, 0, "Class skill"},
    {0x74, 3123, 0, "Class skill"},
};
static const RanuraBuff* ranura_de_efecto(const EfectoActivo& ef) {
    if (ef.clase != EA_ICONO || ef.tipo < EFECTO_RANURA) return nullptr;
    for (const RanuraBuff& r : RANURAS)
        if (r.ranura == ef.tipo - EFECTO_RANURA) return &r;
    return nullptr;
}

static EfectoDeItem item_de_efecto(const App& a, const EfectoActivo& ef) {
    if (const RanuraBuff* r = ranura_de_efecto(ef)) return {r->clasico, r->nombre};
    if (ef.clase == EA_ICONO && ef.tipo >= EFECTO_RANURA && ef.tipo - EFECTO_RANURA < RANURAS_BUFF)
        return {BUFF_ICONO[ef.tipo - EFECTO_RANURA], "Effect"};
    if (ef.clase == EA_BRAVE && ef.tipo == 1) return {79, "Brave Potion"};
    if (ef.clase == EA_BRAVE && ef.tipo == 3) return {130, "Elven Waffle"};
    if (ef.clase == EA_ICONO && ef.tipo == ICONO_POCION_AZUL) return {75, "Blue Potion"};
    if (ef.clase == EA_ICONO && ef.tipo == ICONO_SABIDURIA) return {1529, "Wisdom Potion"};
    if (ef.clase == EA_FUERZA && ef.tipo != 5 && ef.tipo != 2) return {a.icono_fuerza, "STR potion"};  // pociones
    if (ef.clase == EA_DESTREZA && ef.tipo != 5 && ef.tipo != 2) return {a.icono_destreza, "DEX potion"};
    if (ef.clase == EA_TERCERA) return {0, "Third Speed"};
    if (ef.clase == EA_EVA) return {0, "Bless of Eva"};
    return {0, {}};
}

size_t nombre_efecto_activo(const App& a, const EfectoActivo& ef, char* buf, size_t cap) {
    std::string_view n = item_de_efecto(a, ef).nombre;
    if (n.empty()) n = a.hechizos[hechizo_de_efecto(ef)].nombre;
    if (n.empty()) n = "?";
    size_t m = std::min(n.size(), cap - 1);
    std::memcpy(buf, n.data(), m);
    buf[m] = 0;
    return m;
}

// Efectos activos: una fila de íconos arriba al centro (sobre las ventanas), con lo que les queda
// debajo ("29m", "45s"); con el puntero encima, el nombre y el tiempo exacto.
// Buff-bar icons of the original client: N.tbt 5240..5325 and 5392..5400 (32x32, thin light frame
// with corner dots; teal-framed variants are 5241, 5393, 5395, 5397, 5399). Found by matching the
// original's screenshot; they are NOT the spellbook icons from spellicon.idx. 0 = no known icon.
static u32 icono_de_buff_original(u32 hechizo) {
    switch (hechizo) {
    case 43: return 5240;  // Haste: ">>" con contorno
    case 54: return 5240;  // Greater Haste: misma casilla de buff que Haste en el original (S_SkillHaste tipo 1)
    // Deducido: el mismo glifo que el ícono del libro de la habilidad, comparado con la fila de buff original.
    case 8: return 5248;   // Holy Weapon: doble cruz
    case 14: return 5291;  // Decrease Weight: bell
    case 26: return 5247;  // Enchant Dexterity: teal pinwheel
    case 42: return 5246;  // Enchant Strength: estallido de varita
    case 52: return 5243;  // Holy Walk: ">>>" (original: S_SkillBrave tipo 4 -> casilla con el ícono clásico 1110)
    default: return 0;
    }
}

// Ícono de la barra de buffs para un efecto que el servidor informa con su propio paquete, recuperado del cliente
// original (S3EP1U.bin): cada manejador de paquete llama a sub_4647a0(duración, casilla) y la fila dibuja
// el ícono clásico table[casilla] (tabla en 0x6e1020, entradas de 20 bytes, campo 0 = id de ícono clásico). Los
// N.tbt de marco fino que se usan aquí son los rediseños de esos íconos clásicos (coincidencia de píxeles >= 0.95).
// Varios manejadores comparten casilla, así que poción y hechizo se ven igual. 0 = no se conoce gemelo de marco fino.
static u32 icono_de_buff_efecto(const EfectoActivo& ef) {
    switch (ef.clase) {
    case EA_HASTE: return ef.tipo == 1 ? 5240 : 0;                      // slot 0 (481); type 2 slow = slot 1 (480): no twin
    case EA_BRAVE:
        switch (ef.tipo & 15) {
        case 1: case 3: case 5: return 5396;                            // slot 2 (486): brave potion, elven wafer
        case 4: return (ef.tipo >> 4) == 1 ? 1561 : 5243;               // Holy Walk: slot 24/37 (1110), wizard slot 42 (1561)
        case 6: return 5245;                                            // Bloodlust: slot 110 (3075)
        default: return 0;
        }
    case EA_TERCERA: return 5396;                                       // S_Liquor 8: slot 73 (486, same glyph as brave)
    case EA_EVA: return BUFF_ICONO[0xb];                                // opcode 0x11: slot 0xb (classic icon 552, no twin known)
    case EA_ESCUDO:
        switch (ef.tipo) {
        case 5: case 3: return 5271;                                    // Shield, Shadow Armor: slots 4/5 (485)
        case 6: return 5273;                                            // Earth Skin: slot 6 (763)
        case 7: return 5274;                                            // Earth Bless: slot 7 (725)
        case 10: return 5275;                                           // Iron Skin: slot 8 (783)
        default: return 0;
        }
    case EA_FUERZA: return ef.tipo == 2 ? 5258 : 5246;                  // Dress Mighty: casilla 34 (1558); si no casilla 9 (482), también pociones
    case EA_DESTREZA: return ef.tipo == 2 ? 5259 : 5247;                // Dress Dexterity: casilla 35 (1559); si no casilla 10 (483), también pociones
    case EA_ICONO:
        if (const RanuraBuff* r = ranura_de_efecto(ef)) return r->gemelo;  // S_PacketBox icons: classic icon when no twin
        if (ef.tipo >= EFECTO_RANURA && ef.tipo - EFECTO_RANURA < RANURAS_BUFF) return BUFF_ICONO[ef.tipo - EFECTO_RANURA];
        if (ef.tipo == 34) return 5400;                                 // poción azul: casilla 38 (1478)
        if (ef.tipo == 40) return 5286;                                 // Immune to Harm: casilla 41 (1562)
        return 0;
    case EA_AURA:                                                       // PACKETBOX 0x16: tipo = el id de aura del servidor
        switch (ef.tipo) {
        case 113: return 5280;                                          // Glowing Aura: slot 25 (880)
        case 114: return 5281;                                          // Shining Aura: slot 26 (1567)
        case 116: return 5282;                                          // Brave Aura: slot 27 (1557)
        case 147: return 5249;                                          // Fire Weapon: slot 18 (774)
        case 148: return 5252;                                          // Wind Shot: slot 21 (855)
        case 154: return 5250;                                          // Fire Bless: slot 19 (735)
        case 155: return 5253;                                          // Storm Eye: slot 22 (771)
        case 162: return 5251;                                          // Burning Weapon: slot 20 (738)
        case 165: return 5254;                                          // Storm Shot: casilla 23 (827)
        // Sin gemelo de marco fino conocido: el ícono clásico de la casilla (switch S3EP1U 0x4903b0, tabla 0x6e1020).
        case 119: return 890;                                           // casilla 0x1c
        case 221: return 2433;                                          // extra 2, Yahee curse: slot 0x4d
        case 222: return 2432;                                          // extra 1, Barlog curse: slot 0x4e
        case 223: return 2566;                                          // extra 6: slot 0x84
        case 227: return 2382;                                          // slot 0x4a
        default: return 0;
        }
    default: return 0;
    }
}

void dibujar_efectos_activos(App& a, u64 ahora) {
    Graficos& g = *a.g;
    const EstadoJugador& j = a.mundo.jugador;
    // Panel original (0x47df20): ícono n-ésimo (n desde 1) en x = W-2-35n, y = 5; si x < 0 pasa a y = 40, x += W-10.
    const bool original = !a.cfg.ui_simple;
    const int PASO = original ? 35 : 36, Y0 = original ? 5 : 2;
    int Y = Y0;
    // Un efecto registrado por el cliente (EA_HECHIZO) no se dibuja si el servidor ya informó el
    // mismo hechizo con su propio paquete (Holy Walk, Shield...).
    auto repetido = [&](const EfectoActivo& ef) {
        if (ef.clase != EA_HECHIZO) return false;
        for (u32 k = 0; k < j.n_efectos; ++k)
            if (j.efectos[k].clase != EA_HECHIZO && hechizo_de_efecto(j.efectos[k]) == ef.tipo) return true;
        return false;
    };
    u32 visibles = 0;
    for (u32 i = 0; i < j.n_efectos; ++i) visibles += !repetido(j.efectos[i]);
    // Panel original: pegados a la esquina superior derecha y sin el tiempo restante (como el original).
    const int x0 = original ? a.gpu.ancho() - 4 - 32 - (static_cast<int>(visibles) - 1) * PASO
                            : (a.gpu.ancho() - static_cast<int>(visibles) * PASO) / 2;
    const u64 ki = clave_orden(CAPA_UI, 2), kt = clave_orden(CAPA_UI_TEXTO, 2);
    for (u32 i = 0, n = 0; i < j.n_efectos; ++i) {
        const EfectoActivo& ef = j.efectos[i];
        if (repetido(ef)) continue;
        int x = x0 + static_cast<int>(n++) * PASO;
        if (original) {
            x = a.gpu.ancho() - 2 - PASO * static_cast<int>(n);
            Y = Y0;
            if (x < 0) x += a.gpu.ancho() - 10, Y = 40;
        }
        const u64 resta = ef.fin > ahora ? (ef.fin - ahora + 999) / 1000 : 0;
        u32 icono = item_de_efecto(a, ef).icono;
        if (!icono) icono = a.hechizos[hechizo_de_efecto(ef)].icono;
        // El ícono de la barra de buffs del original (marco claro con puntos en las esquinas, glifo de líneas) cuando se
        // conoce, en las dos interfaces; si no, el del objeto o el del hechizo.
        if (const u32 hi = icono_de_buff_efecto(ef)) icono = hi;
        else if (const u32 hs = icono_de_buff_original(hechizo_de_efecto(ef))) icono = hs;
        const RegionAtlas& r = icono ? g.icono(icono) : RegionAtlas{};
        // A punto de terminar (menos de 10 s), parpadea.
        const bool visible = original || resta >= 10 || (ahora / 250) % 2 == 0;  // el dibujo original no parpadea
        if (r.valida() && visible) {
            // El cliente original dibuja los íconos de buff con mezcla screen (cliente 0x47e6f0 -> blit 0x49d420 con
            // las tablas de 5 bits armadas en 0x4a03b0: out = max + min * (31 - max) / 31 por canal): el fondo cuadrado
            // oscuro desaparece, el marco claro brilla y se ve el suelo a través. Va igual en la interfaz simple.
            g.gpu().dibujar_en(r, static_cast<float>(x), static_cast<float>(Y), ki, BLANCO, Mezcla::PANTALLA);
        } else if (visible) {
            char t[8];
            size_t m = nombre_efecto_activo(a, ef, t, 4);
            g.gpu().rectangulo(x, Y, 32, 32, ki, rgba(30, 40, 60, 220));
            g.texto({t, m}, static_cast<float>(x + 3), static_cast<float>(Y + 10), kt, rgba(170, 230, 255));
        }
        if (!original) {
            char d[24];  // 20 dígitos de un u64, la unidad y el NUL
            // Minutos redondeados hacia arriba: una poción de 5 minutos recién tomada dice "5m".
            if (resta >= 60) std::snprintf(d, sizeof d, "%llum", static_cast<unsigned long long>((resta + 59) / 60));
            else std::snprintf(d, sizeof d, "%llus", static_cast<unsigned long long>(resta));
            g.texto_sombra(d, static_cast<float>(x + 16 - ancho_texto(d, std::strlen(d)) / 2), static_cast<float>(Y + 33), kt,
                           rgba(255, 255, 254));
        }
        if (ui::sobre(a.entrada, x, Y, 32, 32 + ALTO_GLIFO)) {
            char t[96];
            size_t m = nombre_efecto_activo(a, ef, t, 64);
            if (original) {  // texto de la tabla de mensajes de la casilla de buff (+0xc) cuando se conoce la casilla
                const int rs = ranura_de_efecto_activo(ef.clase, ef.tipo);
                const std::string_view tt = rs >= 0 ? a.mensajes.linea(BUFF_MSG[rs]) : std::string_view();
                if (!tt.empty()) m = std::min(tt.size(), sizeof t - 1), std::memcpy(t, tt.data(), m), t[m] = 0;
            }
            if (!original)  // original: solo el texto de la tabla de mensajes, sin temporizador
                std::snprintf(t + m, sizeof t - m, "  %llu:%02llu", static_cast<unsigned long long>(resta / 60),
                              static_cast<unsigned long long>(resta % 60));
            int w = ancho_texto(t, std::strlen(t));
            int tx = std::max(4, std::min(a.entrada.mx - w / 2, a.gpu.ancho() - w - 4));  // clamp podría invertirse
            g.gpu().rectangulo(tx - 4, Y + 47, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
            g.texto(t, static_cast<float>(tx), static_cast<float>(Y + 50), clave_orden(CAPA_UI_TEXTO, 30),
                    original ? rgba(189, 222, 255) : rgba(170, 230, 255));
        }
    }
    // Eligiendo objetivo: aviso junto al puntero.
    if (a.hechizo_apuntando) {
        char t[80];
        std::string_view n = a.hechizos[a.hechizo_apuntando].nombre;
        std::snprintf(t, sizeof t, "%.*s: choose a target", static_cast<int>(n.size()), n.data());
        a.g->texto_sombra(t, static_cast<float>(a.entrada.mx + 14), static_cast<float>(a.entrada.my + 4),
                          clave_orden(CAPA_ARRASTRE, 1), rgba(255, 230, 120));
    }
}

}  // namespace lin

namespace lin {

// Rueda del mouse sin ventana debajo (0x45aa70): con la opción `shortcutscroll` mueve la casilla F elegida, un paso arriba una casilla atrás
// (0 pasa a 7), abajo una adelante (7 pasa a 0). El botón del medio (0x45ba6a) dispara la casilla elegida, otra vez a los 500 ms y después
// cada 180 ms mientras se mantiene (temporizador 0x45ab60). Solo interfaz original.
void rueda_teclas_f(App& a) {
    EntradaFrame& e = a.entrada;
    if (a.cfg.ui_simple || a.teclas_f_y < 0) return;
    const bool libre = !sobre_alguna_ventana(a) && !a.arrastrando && a.tecla_f_arrastrada < 0;
    if (libre && e.rueda != 0 && (a.cfg.opciones_orig >> OPC_SHORTCUTSCROLL & 1)) {
        a.tecla_f_sel = static_cast<u8>(((a.tecla_f_sel - e.rueda) % 8 + 8) % 8);
        e.rueda = 0;
    }
    if (e.medio_presionado && libre) {
        if (!a.medio_antes) {
            usar_tecla_f(a, a.tecla_f_sel);
            a.medio_proximo_ms = e.ahora_ms + 500;
        } else if (e.ahora_ms >= a.medio_proximo_ms) {
            usar_tecla_f(a, a.tecla_f_sel);
            a.medio_proximo_ms = e.ahora_ms + 180;
        }
    }
    a.medio_antes = e.medio_presionado;
}

}  // namespace lin
