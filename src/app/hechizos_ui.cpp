// Entregable 6.5: uso de hechizos y habilidades.
//  - Ventana de hechizos (Alt+S): los que sabe el personaje (S_AddSkill), por nivel, con su costo.
//    Clic lanza; con el puntero sobre uno, F5–F12 lo asigna a esa tecla (otra vez la quita).
//  - F5–F12 lanzan el hechizo asignado (se guardan por personaje en settings.ini).
//  - Un hechizo con objetivo espera el clic sobre un personaje o monstruo (clic derecho cancela);
//    si está lejos, se camina hasta tenerlo en alcance y se lanza.
//  - Los efectos activos (haste, escudos...) se listan arriba a la izquierda con lo que les queda.
#include <charconv>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr int FILA = ALTO_GLIFO + 3;
constexpr int LISTA_X = 14, LISTA_Y = 30, LISTA_FILAS = 17;
constexpr u64 MS_ENTRE_AUTOMATICOS = 3000;

// Hechizos que sabe el personaje, en orden (para la ventana).
int hechizos_sabidos(const App& a, u16* ids, int cap) {
    int n = 0;
    for (u32 id = 1; id <= CANTIDAD_HECHIZOS && n < cap; ++id)
        if (a.mundo.jugador.sabe(id) && a.hechizos[id].existe()) ids[n++] = static_cast<u16>(id);
    return n;
}

// settings.ini: atajos=Nombre:4,3,0,0,0,0,0,0;Otro:...
void cargar_atajos(App& a) {
    a.atajos_de = nombre_personaje(a);
    std::memset(a.atajos, 0, sizeof a.atajos);
    std::string_view v = valor_de_personaje(a.cfg.atajos, a.atajos_de);
    for (int i = 0; i < 8 && !v.empty(); ++i) {
        u16 id = 0;
        std::from_chars(v.data(), v.data() + v.size(), id);
        a.atajos[i] = id;
        size_t coma = v.find(',');
        v = coma == std::string_view::npos ? std::string_view{} : v.substr(coma + 1);
    }
}

void guardar_atajos(App& a) {
    std::string v;
    for (int i = 0; i < 8; ++i) v += std::to_string(a.atajos[i]) + (i < 7 ? "," : "");
    poner_valor_de_personaje(a.cfg.atajos, a.atajos_de, v);
    guardar_config(a.cfg);
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

int distancia(const TablaObjetos& o, Slot a, Slot b) {
    return std::max(std::abs(o.col<O_X>()[a] - o.col<O_X>()[b]), std::abs(o.col<O_Y>()[a] - o.col<O_Y>()[b]));
}

// Prueba automática (--lanzar): uno cada 3 s.
void hechizos_automaticos(App& a, u64 ahora) {
    Automatizacion& au = a.aut;
    if (au.lanzar.empty() || a.hechizo_pendiente || a.frames_en_pantalla < 240 ||
        ahora < a.fin_demora_hechizo + MS_ENTRE_AUTOMATICOS)
        return;
    const std::string& pedido = au.lanzar[au.lanzados++ % au.lanzar.size()];
    size_t arroba = pedido.find('@');
    u32 id = buscar_hechizo(a, std::string_view(pedido).substr(0, arroba));
    if (!id) {
        LIN_LOG("Prueba automática: el personaje no sabe \"%s\"", pedido.c_str());
        a.fin_demora_hechizo = ahora;
        return;
    }
    if (arroba == std::string::npos) {
        pedir_hechizo(a, id, 0);
        return;
    }
    std::string_view quien = std::string_view(pedido).substr(arroba + 1);
    const TablaObjetos& o = a.mundo.objetos;
    Slot yo = o.buscar(a.mundo.jugador.id), mejor = SIN_SLOT;
    for (Slot s = 0; yo != SIN_SLOT && s < o.tamano(); ++s) {
        if (o.col<O_ESTADO>()[s] == ESTADO_MUERTO) continue;
        if (std::any_of(a.mascotas.begin(), a.mascotas.end(), [&](const Mascota& m) { return m.id == o.ids()[s]; })) continue;
        if (a.desc.resolver(texto(a.mundo, o.col<O_NOMBRE>()[s])).find(quien) == std::string_view::npos) continue;
        if (mejor == SIN_SLOT || distancia(o, yo, s) < distancia(o, yo, mejor)) mejor = s;
    }
    if (mejor == SIN_SLOT) {
        LIN_LOG("Prueba automática: no hay un \"%.*s\" a la vista", static_cast<int>(quien.size()), quien.data());
        a.fin_demora_hechizo = ahora;
        return;
    }
    pedir_hechizo(a, id, o.ids()[mejor]);
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
    g.gpu().rectangulo(x - 4, y - 3, ancho + 8, lineas * FILA + 4, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 225));
    int fy = y;
    for (std::string_view r = t; !r.empty(); fy += FILA) {
        std::string_view l = r.substr(0, r.find('\n'));
        g.texto(l, static_cast<float>(x), static_cast<float>(fy), clave_orden(CAPA_UI_TEXTO, 30), rgba(230, 220, 190));
        r = l.size() < r.size() ? r.substr(l.size() + 1) : std::string_view{};
    }
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
    a.buff_de_item = h.sobre_item != Hechizo::SIN_ITEM || h.sobre_arma_actual ? id : 0;
    const bool sobre_si = obj == SIN_SLOT || obj == yo;
    a.buff_propio = !a.buff_de_item && !h.de_ataque && h.duracion_s > 0 && sobre_si ? id : 0;
    // Animación propia ya (el eco del servidor no la reinicia) y sin otra acción hasta que termine:
    // el servidor mide el intervalo entre hechizos con la duración de esa animación.
    const u8 accion = h.de_ataque ? ACC_HECHIZO_ATAQUE : ACC_HECHIZO;
    o.col<O_ACCION>()[yo] = accion;
    o.col<O_TACCION>()[yo] = ahora;
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
    if (e.alt_s) a.ventana_hechizos = !a.ventana_hechizos;
    if (a.aut.hechizos) {
        a.ventana_hechizos = true;
        a.aut.hechizos = false;
    }
    if (!e.presionado) a.clic_consumido = false;

    // F5–F12: asignar (con el puntero sobre un hechizo de la ventana) o lanzar.
    if (e.tecla_f >= 5 && e.tecla_f <= 12) {
        int i = e.tecla_f - 5;
        u32 bajo = hechizo_bajo_puntero(a);
        if (bajo) {
            a.atajos[i] = a.atajos[i] == bajo ? 0 : static_cast<u16>(bajo);
            guardar_atajos(a);
        } else if (a.atajos[i]) {
            pedir_hechizo(a, a.atajos[i], 0);
        }
    }

    // Eligiendo el objetivo: clic sobre un personaje o monstruo (o sobre uno mismo); clic
    // derecho, Escape o un clic en el suelo cancelan.
    if (a.hechizo_apuntando) {
        if (e.clic_derecho || e.escape) {
            a.hechizo_apuntando = 0;
        } else if (e.pulsado && !sobre_hechizos(a) && !sobre_inventario(a) && !sobre_barra(a)) {
            ObjId obj = objeto_bajo_puntero(a, true);
            if (obj) pedir_hechizo(a, a.hechizo_apuntando, obj);
            a.hechizo_apuntando = 0;
            a.clic_consumido = true;
        }
    }
    hechizos_automaticos(a, ahora);
}

void dibujar_arrastre(App& a);

// Casilleros F5–F12 del panel original (1019.img): 2 filas de 4, de 34 px, desde (17, 23).
constexpr int TECLAS_X = 17, TECLAS_Y = 23, TECLA = 34;

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
    for (int k = 0; k < 8; ++k) {
        if (!a.atajos[k]) continue;
        const Hechizo& h = a.hechizos[a.atajos[k]];
        const int cx = x + TECLAS_X + (k % 4) * TECLA, cy = y + TECLAS_Y + (k / 4) * TECLA;
        g.gpu().dibujar(g.icono(h.icono), cx, cy, clave_orden(CAPA_UI, 3),
                        a.mundo.jugador.mp >= h.mp ? BLANCO : rgba(110, 110, 110));
        dibujar_espera_hechizo(a, cx, cy, 32, 32);
    }
    if (bajo >= 0 && a.atajos[bajo]) {
        if (e.clic && !a.arrastrando_hechizo && a.arrastrando_atajo < 0) pedir_hechizo(a, a.atajos[bajo], 0);
        if (e.clic_derecho) {
            a.atajos[bajo] = 0;
            guardar_atajos(a);
        }
    }
}

// Fila de la ventana bajo el puntero -> hechizo (0 si no hay).
// Ventana original (con la UI original, F2): el marco 934 con una grilla de 4 × 6 íconos y, a la
// derecha, la tira de pestañas 936 + pestaña (11 pestañas de 19 px: niveles 1 a 10 y las
// habilidades de clase, filas 11 a 28 de S_AddSkill). Posiciones medidas en las imágenes.
constexpr u32 IMG_MAGIA = 934, IMG_PESTANAS = 936;
constexpr int GRILLA_X = 37, GRILLA_Y = 43, CELDA = 34, COLUMNAS = 4, FILAS_GRILLA = 6;
constexpr int PESTANAS = 11, ALTO_PESTANA = 19, PESTANAS_X = 207, PESTANAS_Y = 40;

static bool ventana_original(const App& a) { return !a.cfg.ui_simple && a.g->img(IMG_MAGIA).valida(); }

// Hechizos sabidos de la pestaña actual.
static int hechizos_pestana(const App& a, u16* ids, int cap) {
    int n = 0;
    for (u32 id = 1; id <= CANTIDAD_HECHIZOS && n < cap; ++id) {
        if (!a.mundo.jugador.sabe(id) || !a.hechizos[id].existe()) continue;
        int fila = static_cast<int>((id - 1) / 8);
        if (a.hechizos_pestana < PESTANAS - 1 ? fila == a.hechizos_pestana : fila >= PESTANAS - 1) ids[n++] = static_cast<u16>(id);
    }
    return n;
}

u32 hechizo_bajo_puntero(const App& a) {
    if (!sobre_hechizos(a)) return 0;
    if (ventana_original(a)) {
        const Rect rv = rect_ventana(a, IdVentana::HECHIZOS);
        int cx = a.entrada.mx - (rv.x + GRILLA_X), cy = a.entrada.my - (rv.y + GRILLA_Y);
        if (cx < 0 || cy < 0 || cx >= COLUMNAS * CELDA || cy >= FILAS_GRILLA * CELDA) return 0;
        u16 ids[COLUMNAS * FILAS_GRILLA];
        int n = hechizos_pestana(a, ids, COLUMNAS * FILAS_GRILLA);
        int i = (cy / CELDA) * COLUMNAS + cx / CELDA;
        return i < n ? ids[i] : 0;
    }
    u16 ids[CANTIDAD_HECHIZOS];
    int n = hechizos_sabidos(a, ids, CANTIDAD_HECHIZOS);
    const int y0 = rect_ventana(a, IdVentana::HECHIZOS).y;
    int fila = (a.entrada.my - (y0 + LISTA_Y)) / FILA;
    if (a.entrada.my < y0 + LISTA_Y || fila >= LISTA_FILAS) return 0;
    int i = static_cast<int>(a.hechizos_desplazamiento) + fila;
    return i < n ? ids[i] : 0;
}

static void dibujar_original(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    // El marco tiene el interior transparente: se rellena detrás.
    g.gpu().rectangulo(x0 + 8, y0 + 14, 191, 266, clave_orden(CAPA_UI, 9), rgba(14, 12, 10, opacidad_ventanas(a)));
    g.gpu().dibujar(g.img(IMG_MAGIA), x0, y0, clave_orden(CAPA_UI, 10),
                    rgba(255, 255, 255, static_cast<u8>(std::min(255, opacidad_ventanas(a) + 60))));
    g.gpu().dibujar(g.img(IMG_PESTANAS + static_cast<u32>(a.hechizos_pestana)), x0 + PESTANAS_X, y0 + PESTANAS_Y,
                    clave_orden(CAPA_UI, 10));
    for (int p = 0; p < PESTANAS; ++p)
        if (e.clic && ui::sobre(e, x0 + PESTANAS_X, y0 + PESTANAS_Y + p * ALTO_PESTANA, 19, ALTO_PESTANA)) a.hechizos_pestana = p;
    u16 ids[COLUMNAS * FILAS_GRILLA];
    int n = hechizos_pestana(a, ids, COLUMNAS * FILAS_GRILLA);
    const u32 bajo = hechizo_bajo_puntero(a);
    const u64 ki = clave_orden(CAPA_UI, 12), kt = clave_orden(CAPA_UI_TEXTO, 12);
    for (int i = 0; i < n; ++i) {
        const int x = x0 + GRILLA_X + (i % COLUMNAS) * CELDA, y = y0 + GRILLA_Y + (i / COLUMNAS) * CELDA;
        const Hechizo& h = a.hechizos[ids[i]];
        if (ids[i] == bajo) g.gpu().rectangulo(x, y, CELDA - 1, CELDA - 1, ki - (1ull << 24), rgba(90, 75, 30, 200));
        // Sin MP suficiente, oscurecido.
        g.gpu().dibujar(g.icono(h.icono), x, y, ki, a.mundo.jugador.mp >= h.mp ? BLANCO : rgba(110, 110, 110));
        for (int k = 0; k < 8; ++k)
            if (a.atajos[k] == ids[i]) {
                char t[4];
                std::snprintf(t, sizeof t, "F%d", k + 5);
                g.texto_sombra(t, static_cast<float>(x + 1), static_cast<float>(y + CELDA - 1 - ALTO_GLIFO), kt, rgba(255, 230, 120));
            }
    }
    // Abajo: poder mágico (SP) y resistencia mágica (MR).
    char t[16];
    std::snprintf(t, sizeof t, "%d", a.mundo.jugador.sp);
    g.texto(t, static_cast<float>(x0 + 78), static_cast<float>(y0 + 264), kt, rgba(255, 255, 254));
    std::snprintf(t, sizeof t, "%d", a.mundo.jugador.mr);
    g.texto(t, static_cast<float>(x0 + 149), static_cast<float>(y0 + 264), kt, rgba(255, 255, 254));
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
    u32 max_desp = n > LISTA_FILAS ? static_cast<u32>(n - LISTA_FILAS) : 0;
    if (sobre_hechizos(a) && e.rueda) {
        i64 d = static_cast<i64>(a.hechizos_desplazamiento) - e.rueda;
        a.hechizos_desplazamiento = static_cast<u32>(std::clamp<i64>(d, 0, max_desp));
    }
    a.hechizos_desplazamiento = std::min(a.hechizos_desplazamiento, max_desp);
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
        } else if (int f = tecla_f_bajo_puntero(a); f >= 0) {  // a una tecla F del panel original
            a.atajos[f] = static_cast<u16>(id);
            guardar_atajos(a);
        }
    }
}

// Hechizo que da el efecto (para el nombre y el ícono), o 0 si viene de un ítem.
// S_SkillIconShield y S_SkillBrave: tipo -> hechizo según L1SkillUse.sendIcon del servidor.
static u32 hechizo_de_efecto(const EfectoActivo& ef) {
    switch (ef.clase) {
    case EA_HASTE: return ef.tipo == 2 ? 29 : 43;  // Slow; Haste (también la poción)
    case EA_BRAVE: return ef.tipo == 4 ? 52 : ef.tipo == 6 ? 186 : 0;  // Holy Walk; Bloodlust
    case EA_ESCUDO:
        return ef.tipo == 5 ? 3 : ef.tipo == 3 ? 99 : ef.tipo == 7 ? 159 : ef.tipo == 10 ? 168 : ef.tipo == 6 ? 151 : 0;
    case EA_AURA: return ef.tipo + 1u;  // el índice del hechizo
    case EA_ICONO: return ef.tipo == 40 ? 68 : 0;  // Immune to Harm
    case EA_FUERZA: return ef.tipo == 5 ? 42 : ef.tipo == 2 ? 109 : 0;  // Enchant Mighty; Dress Mighty
    case EA_DESTREZA: return ef.tipo == 5 ? 26 : ef.tipo == 2 ? 110 : 0;  // Enchant Dexterity; Dress Dexterity
    case EA_HECHIZO: return ef.tipo;
    default: return 0;
    }
}

// Los que vienen de un ítem: su ícono de inventario (gfx de L1J: poción de brave 79, waffle de
// elfo 130, poción azul 75) y su nombre.
struct EfectoDeItem {
    u16 icono;
    std::string_view nombre;
};
static EfectoDeItem item_de_efecto(const App& a, const EfectoActivo& ef) {
    if (ef.clase == EA_BRAVE && ef.tipo == 1) return {79, "Brave Potion"};
    if (ef.clase == EA_BRAVE && ef.tipo == 3) return {130, "Elven Waffle"};
    if (ef.clase == EA_ICONO && ef.tipo == 34) return {75, "Blue Potion"};
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
void dibujar_efectos_activos(App& a, u64 ahora) {
    Graficos& g = *a.g;
    const EstadoJugador& j = a.mundo.jugador;
    constexpr int PASO = 36, Y = 2;
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
    const int x0 = (a.gpu.ancho() - static_cast<int>(visibles) * PASO) / 2;
    const u64 ki = clave_orden(CAPA_UI, 2), kt = clave_orden(CAPA_UI_TEXTO, 2);
    for (u32 i = 0, n = 0; i < j.n_efectos; ++i) {
        const EfectoActivo& ef = j.efectos[i];
        if (repetido(ef)) continue;
        const int x = x0 + static_cast<int>(n++) * PASO;
        const u64 resta = ef.fin > ahora ? (ef.fin - ahora + 999) / 1000 : 0;
        u32 icono = item_de_efecto(a, ef).icono;
        if (!icono) icono = a.hechizos[hechizo_de_efecto(ef)].icono;
        const RegionAtlas& r = icono ? g.icono(icono) : RegionAtlas{};
        // A punto de terminar (menos de 10 s), parpadea.
        const bool visible = resta >= 10 || (ahora / 250) % 2 == 0;
        if (r.valida() && visible) {
            g.gpu().dibujar(r, x, Y, ki);
        } else if (visible) {
            char t[8];
            size_t m = nombre_efecto_activo(a, ef, t, 4);
            g.gpu().rectangulo(x, Y, 32, 32, ki, rgba(30, 40, 60, 220));
            g.texto({t, m}, static_cast<float>(x + 3), static_cast<float>(Y + 10), kt, rgba(170, 230, 255));
        }
        char d[16];
        // Minutos redondeados hacia arriba: una poción de 5 minutos recién tomada dice "5m".
        if (resta >= 60) std::snprintf(d, sizeof d, "%llum", static_cast<unsigned long long>((resta + 59) / 60));
        else std::snprintf(d, sizeof d, "%llus", static_cast<unsigned long long>(resta));
        g.texto_sombra(d, static_cast<float>(x + 16 - ancho_texto(d, std::strlen(d)) / 2), static_cast<float>(Y + 33), kt,
                       rgba(255, 255, 254));
        if (ui::sobre(a.entrada, x, Y, 32, 32 + ALTO_GLIFO)) {
            char t[96];
            size_t m = nombre_efecto_activo(a, ef, t, 64);
            std::snprintf(t + m, sizeof t - m, "  %llu:%02llu", static_cast<unsigned long long>(resta / 60),
                          static_cast<unsigned long long>(resta % 60));
            int w = ancho_texto(t, std::strlen(t));
            int tx = std::clamp(a.entrada.mx - w / 2, 4, a.gpu.ancho() - w - 4);
            g.gpu().rectangulo(tx - 4, Y + 47, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
            g.texto(t, static_cast<float>(tx), static_cast<float>(Y + 50), clave_orden(CAPA_UI_TEXTO, 30), rgba(170, 230, 255));
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
