// Estado de la aplicación y pantallas. Cada pantalla es una función que, por frame, lee la
// entrada, emite comandos de red y dibuja (UI inmediata).
#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <array>
#include <string>
#include <vector>
#include "audio/audio.h"
#include "mundo/autobuffs.h"
#include "mundo/autopocion.h"
#include "mundo/chat.h"
#include "mundo/mundo.h"
#include "plataforma/lanzador.h"
#include "plataforma/rutas.h"
#include "proto/comandos.h"
#include "proto/decodificar.h"
#include "proto/hilo_red.h"
#include "recursos/fuente.h"
#include "recursos/hechizos.h"
#include "recursos/html.h"
#include "recursos/sonidos.h"
#include "recursos/tabla_sprites.h"
#include "recursos/textos.h"
#include "recursos/zonas.h"
#include "render/graficos.h"
#include "ui/ui.h"

namespace lin {

struct EstadoAjustes;  // ajustes_ui.cpp

enum class Pantalla { ELEGIR_CARPETA, LOGIN, PERSONAJES, CREAR, CARGANDO, JUEGO };

struct PersonajeLista {
    std::string nombre, clan;
    i32 v[14];  // índices PJ_* de eventos.h
};

// Casillero de la barra de atajos. Un ítem se guarda por su id y su gfx: si el id ya no está (se
// acabó la pila de pociones y se compró otra), se usa otro ítem con el mismo gfx.
enum class TipoAtajo : u8 { VACIO, HECHIZO, ITEM };
struct Atajo {
    TipoAtajo tipo = TipoAtajo::VACIO;
    u32 id = 0;   // skill id o id del ítem
    u16 gfx = 0;  // ítems: gfx de inventario
};
constexpr int CASILLEROS_BARRA = 10;
// Auto-buffs (autobuffs_ui.cpp): a la derecha del selector de páginas de la barra.
constexpr int CELDA_AUTO = 34, ANCHO_AUTO = CASILLEROS_AUTO * (CELDA_AUTO + 2) - 2, SEPARACION_AUTO = 8;
struct AutoBuffs {
    bool encendido = false;
    CasilleroAuto c[CASILLEROS_AUTO];
    std::string de;        // personaje cuyos auto-buffs están cargados
    int arrastrando = -1;  // casillero que se arrastra
    int pidiendo = -1;     // casillero cuyo hechizo se pidió y todavía no salió
    u64 pedido_ms = 0;
    int ultimo = -1;       // el último que salió (a él se le atribuye un "falló")
    int x = 0, y = -1000;  // posición del último frame (la pone dibujar_barra)
    u64 desde_ms = 0;      // no lanza antes (al entrar: que lleguen los íconos de lo que sigue activo)
    bool apretado_interruptor = false;  // el botón se apretó sobre "Auto"
    TurnoAuto turno;       // compartido con la auto-poción
};

// Tienda y almacén (npc_ui.cpp): una fila por ítem; `id` es el índice en la tienda (compra) o el id
// del ítem (venta, retiro, depósito).
// MASCOTA: el inventario de una mascota (S_PetInventory); un clic usa o equipa el ítem.
// REPARAR: las armas dañadas (S_FixWeaponList); un clic repara una (C_SelectList).
enum class TipoTienda : u8 { NINGUNA, COMPRA, VENTA, RETIRO, DEPOSITO, MAGIA, MASCOTA, REPARAR };
struct FilaTienda {
    u32 id = 0;
    u16 gfx = 0;
    u8 bendicion = 0;
    u32 precio = 0, maximo = 0, elegido = 0;
    std::string nombre;  // con "$N" sin resolver
    std::string atributos;  // S_ShopSellList status bytes (L1ItemInstance.getStatusBytes), for the tooltip property lines
};
// Ventanas del juego (ventanas.cpp): se mueven arrastrando la barra de título.
enum class IdVentana : u8 { ESTADO, INVENTARIO, HECHIZOS, NPC, SALIDA, SOCIAL, INTERCAMBIO, VOLUMEN, HPMP, TIENDA_PRIV, LISTA, MASCOTA, DESTINO, CARTEL, COCINA, CANTIDAD };
struct Rect {
    int x, y, w, h;
};
// Tienda de jugadores (tienda_ui.cpp). Vendedores a la vista con su título; la tienda que se está
// mirando (S_PrivateShop); y la propia: lo que se pone a la venta.
struct Vendedor {
    ObjId id = 0;
    std::string titulo;
};
struct LineaTiendaVista {
    u8 orden = 0, bendicion = 0;
    u16 gfx = 0;
    u32 cantidad = 0, precio = 0;
    ObjId mi_item = 0;  // en "lo que compra": el ítem propio que se le vende
    std::string nombre;
};
struct TiendaJugadores {
    std::vector<Vendedor> vendedores;
    ObjId vista = 0;   // la tienda abierta en pantalla (0 = ninguna)
    u8 tipo = 0;       // 0 lo que vende, 1 lo que compra
    std::vector<LineaTiendaVista> lineas;
    bool armando = false, abierta = false;  // la propia
    std::string titulo;
    std::vector<cmd::LineaTienda> venta, compra;  // la tienda propia: grillas "Selling item" / "Buying item" (ventana original)
    std::vector<LineaTiendaVista> grilla[2];      // la tienda que se mira: sus listas de venta (0) y compra (1)
    std::string titulo2;                          // segunda línea del título (ventana original)
    u8 rejilla = 0;                               // grilla a la que suma el próximo clic en el inventario
    int foco = 0;                                 // title edit box with the keyboard
    // Scroll: la tienda que se mira (`lineas`) y la propia (`venta`).
    u32 desplaza_vista = 0, desplaza_venta = 0;
    ui::BarraScroll barra_vista, barra_venta;
};

// Tablón de los pueblos (tablon_ui.cpp): la página de temas, el que se lee y lo que se escribe.
struct TemaTablon {
    u32 id = 0;
    std::string autor, titulo;  // autor: "nombre  fecha"
};
struct Tablon {
    ObjId id = 0;  // el tablón abierto (0 = cerrado)
    std::vector<TemaTablon> temas;
    u32 leyendo = 0;            // tema abierto
    std::string cabecera, texto;  // del que se lee
    bool escribiendo = false;
    std::string nuevo_titulo, nuevo_texto;
    int foco = 0;
};
// Correo (correo_ui.cpp): bandejas 0 normal, 1 clan, 2 guardadas.
struct Carta {
    u32 id = 0, fecha = 0;
    bool leida = false, enviada = false;
    std::string otro, asunto;
};
struct Correo {
    bool abierto = false;
    u8 bandeja = 0;
    std::vector<Carta> cartas;
    u32 leyendo = 0;
    std::string texto;
    bool escribiendo = false, al_clan = false;
    std::string destino, asunto, cuerpo;
    int foco = 0;
    int nuevas = 0;  // sin leer en la bandeja normal: el sobre de aviso
};
// Casas (casas_ui.cpp): el tablón de subastas abierto con sus casas, y el mapa de una.
struct CasaSubasta {
    u32 id = 0;
    std::string nombre;
    u16 area = 0;
    u8 mes = 0, dia = 0;
    u32 precio = 0;
};
struct Subastas {
    ObjId tablon = 0;  // 0 = cerrado
    std::vector<CasaSubasta> casas;
    int desplazamiento = 0;  // filas
    u32 mapa = 0;            // casa cuyo mapa se muestra (0 = ninguno)
};
// World-map item window (map_ui.cpp): number 0 = closed.
struct MapaItem {
    int numero = 0;
    ObjId objeto = 0;
    int x = 0, y = 0;  // posición de la ventana (origen de la superficie de juego al abrirse)
    bool arrastrando = false;
    int ult_x = 0, ult_y = 0;
};
// Castillos (castillos_ui.cpp): señores (S_CastleMaster), asedios en curso y guerras del clan.
struct SenorCastillo {
    ObjId id = 0;
    u8 castillo = 0;
};
struct Castillos {
    std::vector<SenorCastillo> senores;
    bool asedio[9] = {};                // castillos 1–8 en asedio (S_PacketBox 0/2 hasta el 1)
    std::vector<std::string> enemigos;  // clanes en guerra con el propio (S_War)
};

// Grupo (S_PacketBox 104–106; social_ui.cpp): los miembros con su vida (S_HpMeter) y el líder.
struct MiembroGrupo {
    ObjId id = 0;
    std::string nombre;
    u8 hp = 0xFF;  // %; 0xFF desconocida
    bool lider = false;
};

// Mascota o invocación propia (S_PetCtrlMenu; mascotas_ui.cpp).
struct Mascota {
    ObjId id = 0;
    std::string nombre;  // con "$N" sin resolver
};

// Marcador de teleport (S_Bookmarks; marcadores_ui.cpp) y el diálogo para elegir destino.
struct Marcador {
    u32 id = 0;
    u16 mapa = 0, x = 0, y = 0;
    std::string nombre;
};
// Barras de scroll de las ventanas que no tienen un struct propio (ver barra_scroll_ventana).
struct ScrollVentanas {
    ui::BarraScroll hechizos, inventario, tienda, tienda_elegidas, forma;
    u32 tienda_elegidas_pos = 0;  // primera fila visible de la grilla de elegidos de la tienda original
    u32 forma_pos = 0;       // primera sugerencia visible de polymorph
    std::string forma_filtro;  // lo escrito la última vez: al cambiar, la lista va a los niveles más altos
};

struct ElegirDestino {
    bool abierto = false;
    ObjId item = 0;   // pergamino de teleport, o 0 si es el hechizo
    u32 hechizo = 0;  // Teleport o Mass Teleport
    // Ventana: posición (x < 0: centrada), arrastre por el título y scroll de la lista.
    int x = -1, y = 60;
    bool arrastrando = false;
    int dx = 0, dy = 0;
    u32 desplaza = 0;
    ui::BarraScroll barra;
    int sel = -1;          // interfaz original: la fila cliqueada (teñida hasta que ocurre el teleport)
    bool enviado = false;  // interfaz original: la elección se mandó; la ventana sigue hasta el teleport (0x418d6e)
};

// Reiniciar atributos (reinicio_ui.cpp): S_CharReset / C_CharReset, con la Candle of Reminiscence.
struct ReinicioAtributos {
    enum Fase : u8 { CERRADO, INICIAL, SUBIR, ELIXIR } fase = CERRADO;
    int nivel = 1, maximo = 1, hp = 0, mp = 0, ac = 0, puntos = 0;
    u8 stats[6] = {};  // fue, int, sab, des, con, car (el orden de C_CharReset)
    u8 extra[6] = {};  // lo que se va sumando en la fase inicial o con los elixires
};

// Diálogo de cantidad (cantidad_ui.cpp): S_InputAmount del servidor, o cuántos dar de una pila.
constexpr u8 CANTIDAD_FILA_NPC = 4;
struct DialogoCantidad {
    bool abierto = false;
    bool dar = false;         // true: C_GiveItem del `item` a `objeto`; false: C_Amount a `objeto`
    bool borrar = false;      // true: C_DeleteInventoryItem del `item` (el basurero)
    std::vector<ObjId> varios;  // con `borrar`: los ítems de la selección, enteros (si no está vacío, en vez de `item`)
    u8 tienda = 0;            // tienda de jugadores (tienda_ui.cpp): 1 comprar, 2 venderle, 3 precio al armar;
                              // CANTIDAD_FILA_NPC: cuántos de la fila `item` de la tienda o almacén de `objeto` (npc_ui.cpp)
    u8 castillo = 0;          // castillos_ui.cpp: CF_TASA + 1 impuesto, CF_DEPOSITAR + 1 poner, CF_RETIRAR + 1 sacar
    bool escribiendo = false; // ya se escribió un número (el siguiente se agrega en vez de reemplazar)
    ObjId objeto = 0, item = 0;
    i32 valor = 1, min = 1, max = 1, precio = 0;
    std::string accion, titulo;
};

// La caja de ingreso de texto del original (vtable [0x702dd8]+0x64, 0x4451d0): la línea de edición del chat toma la próxima línea escrita (su
// pregunta va al registro del chat) y se la pasa a un callback en vez de mandarla como chat. Enter confirma, Esc cancela.
struct PromptChat {
    bool activo = false;
    std::string accion;       // para qué es el texto escrito (social_ui.cpp: confirmar_prompt)
    i32 x = 0, y = 0;         // recoger: celda
    ObjId id = 0;             // pickup: item
    u32 maximo = 0;           // pickup / drop / give: stack size
    ObjId objetivo = 0;       // dar: el objeto que recibe
};

struct PosVentana {
    int x = 0, y = 0;
    bool movida = false;    // si no, va en su lugar de siempre
    bool colocada = false;  // al abrirla, su lugar de siempre tapaba a otra: va en (x, y) sin guardarse
};

// Hasta dónde se camina para hablarle a un NPC (celdas).
constexpr int DISTANCIA_HABLAR_NPC = 2;

// Social (social_ui.cpp): preguntas de sí o no pendientes y el intercambio en curso.
struct Pregunta {
    u32 numero = 0;
    u16 tipo = 0;
    std::string texto;
    std::string respuesta;  // las que piden un texto (PREGUNTA_NOMBRE_CASA)
};
constexpr u16 PREGUNTA_NOMBRE_CASA = 512;  // el mayordomo: "¿nombre de la casa?" (C_Attr con el texto)
struct ItemIntercambio {
    u16 gfx = 0;
    u8 bendicion = 0;
    std::string nombre;  // con "$N" y la cantidad
};
struct Intercambio {
    bool abierto = false;
    std::string con;
    std::vector<ItemIntercambio> propio, otro;
    bool ok = false;  // ya se mandó C_TradeOK
    ui::BarraScroll barra[2];  // barras de scroll de la interfaz original de las grillas propia / del otro
    u32 desplaza[2] = {0, 0};  // primera fila visible de cada una
};

// Cursor del mouse (cursores.cpp): mano, espada sobre un monstruo, círculo al elegir objetivo.
// Original UI: ESTADO_1..ESTADO_16 are the client's 16 cursor states (S3EP1U table 0x6e0de8, docs/fork/RE_HUD.md).
enum class Cursor : u8 {
    MANO, ESPADA, OBJETIVO, LABIOS, MONEDA,
    ESTADO_1, ESTADO_2, ESTADO_3, ESTADO_4, ESTADO_5, ESTADO_6, ESTADO_7, ESTADO_8,
    ESTADO_9, ESTADO_10, ESTADO_11, ESTADO_12, ESTADO_13, ESTADO_14, ESTADO_15, ESTADO_16,
    CANTIDAD
};
constexpr Cursor cursor_estado(int n) { return static_cast<Cursor>(static_cast<int>(Cursor::ESTADO_1) + n - 1); }

struct App {
    Config cfg;
    SDL_Window* ventana = nullptr;
    bool salir = false;
    ui::Barra ventana_sin_borde;  // ventana sin borde: arrastre, botones y bordes (ventana_ui.cpp)

    Recursos rec;
    FuenteBitmap fuente;
    TablaSprites sprites;
    TablaZonas zonas;  // zone3-e.tbl: nombre de la zona en la que se está
    std::vector<ItemMision> item_quest;  // itemquest.tbl: (gfx, desc-e.tbl entry), original UI "Quest" tab
    TablaTexto desc, intro, mensajes;  // mensajes = string-e.tbl (S_ServerMessage)
    TablaHechizos hechizos;
    Gpu gpu;
    std::unique_ptr<Graficos> g;
    // Audio (sonido.cpp): definiciones de music.def/envsound.def (los textos viven aquí) y el
    // ambiente vigente con el próximo sonido suelto de cada grupo.
    Audio audio;
    DefSonidos sonidos;
    std::vector<u8> texto_music_def, texto_envsound_def;
    const AmbienteZona* ambiente = nullptr;
    u64 proximo_ambiente[8] = {};
    u64 t_zona_sonido = 0;
    // Música de combate (0x4c53a0 fija la hora, el tick de 500 ms 0x468113 la termina 15 s después).
    u64 t_combate = 0;
    i32 pista_combate = -1;
    u64 t_emblema_subido = 0;     // última subida de emblema (el original permite una cada 59876 ms)
    std::string emblemas_ruta;    // archivo de caché en uso (Emblem.dat en la carpeta de ajustes)
    u64 t_emblema_sucio = 0;
    std::vector<u8> emblema_vista;  // vista previa de checkbmp (RGB555, 384 bytes) mientras t_vista_emblema está en el futuro
    u64 t_vista_emblema = 0;

    TablaOpcodes op{};
    HiloRed red;
    Lanzador lanzador;  // el lanzador del servidor elegido, si tiene (Servidor::lanzador)
    EstadoRed estado_red_previo = EstadoRed::INACTIVA;
    std::vector<u8> paquete;
    std::vector<Evento> eventos;
    Arena arena{4u << 20};
    Mundo mundo;
    Chat chat;

    Pantalla pantalla = Pantalla::LOGIN;
    u64 frames_en_pantalla = 0;
    EntradaFrame entrada;
    u64 frame = 0;
    bool depuracion = false;

    // Login
    std::string usuario, clave, aviso, noticias;
    int foco = 0;
    bool confirmar_borrado = false;  // selección de personaje (UI simple): el próximo "Delete" borra
    bool esperando = false, mostrando_noticias = false;
    int scroll_texto = 0;  // renglón del texto de bienvenida o de las noticias que se ve arriba (rueda, Re/Av Pág)
    bool menu_servidores = false;  // el desplegable de servidores del login, abierto
    bool mudo = false;             // el audio corre (y cuenta) pero sin volumen (pruebas: --mudo o "mudo on")

    // Selección de personaje
    int cantidad_pj = 0, max_pj = 0;
    std::vector<PersonajeLista> pjs;
    int pj_sel = -1, pagina = 0;
    u64 t_anim = 0;

    // Creación
    std::string nombre_nuevo;
    int clase_nueva = 0, sexo_nuevo = 0;
    int extra[6] = {};  // puntos agregados a fue, des, con, sab, car, int

    // Juego
    bool ayuda_teclas = false;     // F1
    u64 t_intento_portal = 0;      // último paso hacia una celda bloqueada (pantalla_juego.cpp: intento_portal)
    u32 celda_intento_portal = 0;
    // Combate: lo que se dibujó en el frame anterior (para saber qué hay bajo el puntero), el
    // objetivo del ataque y hasta cuándo dura el ataque en curso.
    struct ObjetoEnPantalla {
        ObjId id;
        float x0, y0, x1, y1;
        u32 profundidad;
        u8 sprite;  // 0 dibujado, 1 pendiente (bloque verde), 2 faltante (bloque magenta)
    };
    std::vector<ObjetoEnPantalla> en_pantalla;
    // Último cuadro de sprite dibujado por casilla de objeto. Mientras un cuadro nuevo todavía se carga (acción nueva como
    // recibir un golpe), se mantiene el anterior en pantalla en vez de que el sprite desaparezca.
    struct LastFrame {
        ObjId id = 0;
        RegionAtlas region;
    };
    std::vector<LastFrame> last_frames = std::vector<LastFrame>(4096);
    // Fuentes de luz del frame, en los pies. Aparte de en_pantalla, que deja afuera a los muertos.
    struct LuzEnPantalla {
        int radio;
        float x, y;
    };
    std::vector<LuzEnPantalla> luces;
    std::vector<u8> luz_mascara, luz_brillo;  // aplicar_luz: buffers del frame, se reusan
    bool orbe_anim = false;                   // game-time orb transition running (0x8f0809)
    u64 orbe_t0 = 0;                          // su tick de inicio (0x8f080c)
    u32 orbe_hora_n = 0;                      // último mundo.hora_n que vio el orbe
    int hint_boton = -1;                      // ayuda de la franja inferior: botón bajo el puntero (-1 ninguno)
    u64 hint_t0 = 0;                          // tick en que el puntero entró / último clic (0x4bbb50)
    i32 hora_forzada = -1;                    // comando forzar-hora (pruebas): segundos del día, -1 = la del servidor
    bool ambiente_agua = false;               // el ambiente que suena es el del agua (sonido 330)
    std::vector<u32> agua_filas;              // color por fila del agua (1416.img), cargado al primer uso
    // Roof groups of the local player's cell fading (original UI; see actualizar_techos): level 0..31 per group.
    struct TechoFade {
        u32 segmento;  // base_x << 16 | base_y del segmento dueño del id de grupo
        u16 gid;
        u8 tipo, nivel;
        bool activo;
    };
    static constexpr u32 MAX_TECHOS_FADE = 16;
    TechoFade techos_fade[MAX_TECHOS_FADE] = {};
    u32 n_techos_fade = 0;
    u64 techos_proximo_ms = 0;
    bool jugador_tapado = false;  // un objeto del mapa tapa al propio personaje (se ve translúcido)
    ObjId objetivo = 0;
    u64 fin_ataque = 0;
    u64 next_pickup = 0;  // F4 pickup cooldown
    u64 t_muerte = 0;  // cuándo se vio el aviso de muerte (0 = vivo)
    std::string chat_entrada;
    u32 chat_desplazamiento = 0;  // líneas hacia atrás desde la más reciente
    int chat_barra_agarre = -1;   // original chat scrollbar: grab offset inside the thumb while dragging, -1 = idle
    bool ventana_estado = false;
    u8 pestana_estado = 0;  // 0 atributos, 1 equipo (equipo_ui.cpp)
    // Hechizos (Alt+S, F5–F12). Un hechizo con objetivo espera el clic en el objetivo
    // (hechizo_apuntando) y después se camina hasta tenerlo en alcance (hechizo_pendiente).
    bool ventana_hechizos = false;
    u32 hechizos_desplazamiento = 0;  // filas
    int teclas_f_x = 0, teclas_f_y = -1;  // panel original: casilleros F5–F12 (-1 = no se dibujan)         // ventana original: nivel 1..10 (0..9) o habilidades de clase (10)
    u32 hechizo_apuntando = 0;
    u32 hechizo_pendiente = 0;
    ObjId objetivo_hechizo = 0;
    u16 msgs_recientes[8] = {};       // últimos ids de S_ServerMessage (anillo) y sus horas: evita repetir textos de fin de efecto
    u64 msgs_recientes_ms[8] = {};
    u8 msgs_recientes_n = 0;
    u64 fin_demora_hechizo = 0;       // useDelay del último hechizo
    // Espera hasta el próximo hechizo (la animación del lanzado o su useDelay, la mayor): se
    // muestra sobre los casilleros de hechizos.
    u64 hechizo_lanzado_ms = 0, hechizo_listo_ms = 0;
    u32 buff_de_item = 0;  // buff sobre el arma o la armadura recién lanzado: su ícono llega con el mensaje 161
    u32 buff_propio = 0;   // buff recién lanzado sobre sí mismo: su ícono, al ver su gráfico sobre el personaje
    u16 gfx_item_usado = 0;  // ícono del último ítem usado y cuándo: el de las pociones de FUE/DES
    u64 t_item_usado = 0;
    u16 icono_fuerza = 0, icono_destreza = 0;  // el de la poción que dio el efecto activo
    u64 t_repetir_medio = 0;          // botón del medio mantenido: último uso de un ítem
    bool medio_sin_objetivo = false;  // esta pulsación del medio ya dejó un hechizo esperando el clic
    AutoBuffs autobuffs;
    // Auto-poción (autopocion_ui.cpp): la opción del personaje cargado y la demora de la última.
    struct AutoPocion {
        std::string de;
        OpcionPocion op;
        u64 proxima_ms = 0, bebida_ms = 0;
        bool apretado_boton = false;  // el botón se apretó sobre el de la barra
        int x = 0, y = -1000;         // el botón de la barra en el último frame (lo pone dibujar_barra)
    } autopocion;
    // Tecla 1–0 mantenida sobre un hechizo con objetivo que no es de ataque (barra_ui.cpp).
    struct TeclaMantenida {
        int numero = 0;      // 1..10; 0 = ninguna
        u32 hechizo = 0;
        u64 desde_ms = 0;
        bool sobre_si = false;  // pasó el tiempo: se lanza sobre uno mismo
    } tecla_mantenida;
    bool clic_consumido = false;      // el clic que eligió el objetivo no camina hasta soltarlo
    u16 atajos[8] = {};               // F5..F12: spell ids
    u8 tecla_f_sel = 0;               // casilla F elegida 0..7 ([0x703100]): la última cliqueada, la mueve la rueda (shortcutscroll), la dispara el botón del medio
    bool medio_antes = false;         // estado del botón del medio en el cuadro anterior
    u64 medio_proximo_ms = 0;         // próxima repetición de la casilla F elegida mientras se mantiene el botón del medio
    int tecla_f_arrastrada = -1;      // casilla F que se arrastra
    u16 atajos_pg[3][8] = {};         // las tres páginas de teclas F (a.atajos / items_f son la que se muestra, pag_f)
    Atajo items_pg[3][8];
    u8 pag_f = 0;
    Atajo items_f[8];                 // F5..F12: objeto (cuando la casilla tiene un objeto; su entrada en atajos[] es 0)
    std::string atajos_de;            // personaje cuyos atajos están cargados
    // Barra de atajos (Alt+1..Alt+0; barra_ui.cpp): hechizos o ítems arrastrados a sus casilleros.
    // Páginas (cfg.paginas_barra a la vista; las de más se conservan) y la visible.
    Atajo barra[PAGINAS_BARRA_MAX][CASILLEROS_BARRA];
    int pagina_barra = 0;
    std::string barra_de;             // personaje cuya barra está cargada
    int barra_x = 0, barra_y = -1000; // posición del último frame (para el puntero)
    u32 arrastrando_hechizo = 0;      // desde la ventana de hechizos
    int arrastrando_atajo = -1;       // desde un casillero de la barra
    int casillero_elegido = -1;       // el que usa el clic del medio sobre el mundo: página * CASILLEROS_BARRA + casillero (-1 ninguno)
    int arrastre_x0 = 0, arrastre_y0 = 0;
    // Polymorph (forma_ui.cpp): diálogo para escribir el monstruo.
    bool ventana_forma = false;
    u8 npc_poly = 0;                  // original polymorph page "monlist" open: 1 = Shape Change, 2 = scroll (forma_item)
    ObjId forma_item = 0;             // pergamino usado; 0 = Shape Change (se responde con C_NPCAction)
    std::string forma_texto;
    // Sugerencias de polymorph: datos/formas.txt (nivel y nombre, por nivel; herramientas/generar_formas.py).
    struct Forma {
        int nivel = 0;  // 0 si vino de polymorph.tbl, que no trae nivel
        std::string nombre;
    };
    std::vector<Forma> formas;
    // NPC (npc_ui.cpp): diálogo con la página HTML, y tienda o almacén en la misma ventana.
    ObjId npc = 0;
    bool npc_local = false;  // la página es del cliente (option2, ...): sin NPC, sin cierre por distancia, las acciones quedan locales
    std::string npc_pagina, npc_datos;
    u32 npc_casa = 0;  // página "agsel" del tablón de subastas: la casa ("map" y "apply" la llevan)
    PaginaHtml npc_html;
    int npc_desplazamiento = 0;  // px
    TipoTienda tienda = TipoTienda::NINGUNA;
    ObjId tienda_npc = 0;
    std::vector<FilaTienda> tienda_filas;
    u32 tienda_desplazamiento = 0;
    u8 tienda_tipo = 0;       // tipo de ventana de lista original (S3EP1U 0x4d1000): 0 compra, 1 venta, 2/3 depósito, 4/5 clan, 8/9 elfo, ...
    int tienda_marcada = -1;  // fila marcada por un clic (imagen 536)
    u32 tienda_digitos = 0;
    // Pesca (pesca_ui.cpp): el corcho propio lanzado por S_Fishing.
    struct Pesca {
        bool activa = false, picada = false;
        i32 x = 0, y = 0;
        u64 inicio = 0;
    } pesca;
    int cursor_html = 0;           // interfaz original: cursor que pidió la página del NPC el cuadro anterior (1 texto, 3 enlace), 0 ninguno
    int cursor_estado_orig = 3;    // interfaz original: último estado de cursor fijado (0x477c90 es persistente: las zonas sin setter lo conservan)   // cantidad escrita en la fila marcada (panel +0x98), la reinicia cualquier clic en la lista
    int tienda_flecha = -1;   // quantity arrow held down: row * 2 + (1 = increase), -1 none
    bool hablar_objetivo = false;  // a.objetivo es un NPC al que se camina para hablarle
    bool objetivo_fijo = false;    // elegido al apretar el botón: se mantiene aunque el puntero ya no esté encima
    bool ataque_sostenido = false; // tecla A: se sigue atacando al objetivo sin mantener el botón
    bool modo_pk = false;          // "/pk": el clic sobre otro jugador lo ataca sin Ctrl (original: the "pk" option [0x6f1dac])
    bool modo_ataque = false;      // original UI Ctrl+X attack mode ([0x703105]); clicks treat it like modo_pk
    // Ventanas: posiciones, la que se está arrastrando y la de opciones.
    PosVentana pos_ventanas[static_cast<int>(IdVentana::CANTIDAD)];
    bool ventanas_cargadas = false;
    // De atrás hacia adelante (IdVentana); la de más arriba abierta bajo el puntero, o -1.
    u8 orden_ventanas[static_cast<int>(IdVentana::CANTIDAD)] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    bool ventana_abierta_antes[static_cast<int>(IdVentana::CANTIDAD)] = {};
    int ventana_arriba = -1;
    bool ventana_sobre_otra = false;
    bool escribiendo_chat = false;
    DialogoCantidad cantidad;
    ReinicioAtributos reinicio;
    std::vector<Mascota> mascotas;
    std::vector<MiembroGrupo> grupo;
    TiendaJugadores tienda_pj;
    Tablon tablon;
    Correo correo;
    Subastas subastas;
    Castillos castillos;
    MapaItem mapa_item;
    std::vector<std::string> bloqueados;  // C_Exclude: el servidor no le pasa sus mensajes
    std::vector<Marcador> marcadores;
    ElegirDestino destino;
    // Destino elegido para el próximo Teleport (se usa al lanzarlo; 0, 0 = al azar).
    bool destino_elegido = false;
    u16 destino_mapa = 0;
    u32 destino_marcador = 0;
    ObjId mascota_eligiendo = 0;      // S_SelectTarget: el próximo clic elige a quién ataca esta mascota
    ObjId marca_objetivo = 0;         // S_TrueTarget: objetivo marcado hasta fin_marca
    u64 fin_marca = 0;
    std::string marca_texto;  // con chat_con_enter: la línea del chat está abierta (Enter la abrió)  // la que se está dibujando tapa a otra abierta (fondo opaco)
    int ventana_arrastrada = -1;
    int arrastre_dx = 0, arrastre_dy = 0;
    // Ajustes (ajustes_ui.cpp): ventana modal, también desde el login. El estado del formulario existe solo
    // mientras está abierta.
    bool ventana_ajustes = false;
    std::shared_ptr<EstadoAjustes> ajustes;
    ScrollVentanas scroll;
    bool ventana_salida = false;  // Restart / Quit
    // Texto de combate flotante sobre el personaje (daño recibido / "Miss"), si cfg.texto_dano.
    struct TextoCombate { char t[12]; u64 t0; u8 r, g, b; };
    TextoCombate textos_combate[6];
    u8 n_textos_combate = 0;
    i32 hp_anterior = 0;  // detectar daño recibido: caída de HP tras aplicar los eventos del frame
    // Render no interactivo a PNG (LIN_RENDER=id,x,y,salida.bmp): dibuja el mapa y sale. Ver app.cpp.
    struct { bool activo = false; MapId mapa = 0; int x = 0, y = 0; std::string salida; } render;
    PromptChat chat_prompt;
    bool telcon = false;               // /telcon: los elfos de nivel 50+ tienen el selector de marcadores sin el anillo (0x409690)
    bool invitando = false;            // invitar al grupo: el próximo clic en el mundo elige el objetivo (0x402490, mensaje 0xb3)
    bool ventana_social = false;
    // Original party / pledge / buddy list window (lista_ui.cpp; S_ShowHTML pages "party", "pledge", "buddy").
    struct ListaOrig {
        bool abierta = false;
        u8 tipo = 0;  // 0 party, 1 pledge, 2 buddy
        std::string titulo;
        std::vector<std::pair<std::string, bool>> filas;  // name, online
        u32 pos = 0;
        ui::BarraScroll barra;
    } lista;
    bool ventana_mascota = true;       // ventana original de mascota (mascotas_ui.cpp), cerrada con su X hasta la próxima invocación
    size_t mascota_activa = 0;
    u8 hpmp_estilo = 0;                // floating HP/MP window style 0..2 ([0x8169b0], float_hpmp_ui.cpp)
    bool ventana_volumen = false;      // original volume window (volume_ui.cpp)
    // Ventana original de cocina (cocina_ui.cpp; S_PacketBox 52 la abre para el libro de cocina usado).
    struct Cocina {
        bool abierta = false;
        u8 modo = 0;        // etapa del libro de cocina 1..5 (se muestran 8 recetas por etapa, hasta 24)
        ObjId pan = 0;      // el último libro de cocina usado ([0x8f2fb8]); 0 = ninguno
        int sel = -1;       // receta elegida (+8 de la fila)
        u32 desplaza = 0;   // scroll, en filas de 4 recetas
        ui::BarraScroll barra;
        std::vector<std::string> nombres;  // recipe.tbl, en el orden de la lista: el campo del nombre del plato ("$N")
    } cocina;
    struct Cartel { bool abierto = false; u8 rumbo = 0; u32 desc = 0; } cartel;  // ficha de un cartel (cartel_ui.cpp)
    int volumen_tercero = 100;         // tercer deslizador de esa ventana (el fork no tiene ese canal de audio)
    int volumen_arrastre = -1;         // slider being dragged

    std::vector<Pregunta> preguntas;
    Intercambio intercambio;
    std::vector<u32> emblemas_pedidos;  // clanes cuyo emblema ya se pidió (C_Clan)
    // Cursor: el que pide el frame (cada frame empieza en la mano) y el que está puesto.
    SDL_Cursor* cursores[static_cast<int>(Cursor::CANTIDAD)] = {};
    Cursor cursor = Cursor::MANO;
    Cursor cursor_aplicado = Cursor::CANTIDAD;
    // Inventario (Alt+I).
    bool ventana_inventario = false;
    u32 inventario_desplazamiento = 0;  // filas
    u8 inventory_tab = 0;               // interfaz original: 0 todo, 1 equipo, 2 misión, 3 varios
    ObjId arrastrando = 0;              // ítem que se arrastra (para soltarlo al suelo)
    bool es_gm = false;                 // the chosen character was flagged administrator in the character list
    u8 gm_apuntando = 0;                // GM Ctrl+Y (1) / Ctrl+Z (2): el próximo clic en el mundo elige el objetivo
    ObjId gm_objetivo = 0;              // objetivo de Ctrl+Z que se reenvía
    u8 gm_repeticiones = 0;
    u64 gm_proximo_ms = 0;
    u64 flash_luz_frame = 0;            // último cuadro en que un efecto de destello pasó su cuadro de disparo
    u8 ceguera_avisada = 0;             // último estado de S_CurseBlind cuyo texto se imprimió
    bool infracon = true;               // "/infracon" (0x6f1e4b): elfos / elfos oscuros usan su filtro de infravisión
    u32 capturas = 0;                   // contador de capturas Ctrl+P de la interfaz original ([0x6f1e54], empieza en 0 cada vez)
    u8 chat_tam = 0;                    // original UI Ctrl+T: chat window size 0..3 (+3 rows per step)
    bool paleta_chat = false;           // original UI Ctrl+O: chat colour palette
    ObjId item_apuntando = 0;           // varita u objeto apuntado que espera el clic en su objetivo (inventario_ui.cpp)
    std::vector<ObjId> inventario_seleccion;  // marcados con Shift+clic: se arrastran juntos
    ObjId usar_sobre = 0;               // pergamino (identificar, encantar) esperando el clic en su objetivo
    bool usar_sobre_armado = false;     // a fresh press happened after usar_sobre was set (the double click's release doesn't pick)
    ObjId ultimo_clic_item = 0;         // para el doble clic
    ObjId inv_press = 0;                // original inventory: item pressed, drag not started yet
    int inv_press_x = 0, inv_press_y = 0;
    u64 t_ultimo_clic_item = 0;
    float fps = 0;
    // Medición por segundo (overlay F3 y, con --perf, una línea en el registro).
    float ms_cpu = 0;      // CPU del frame (entrada, red, pantalla, armado y orden de la lista), promedio
    float ms_cpu_max = 0;  // el peor frame
    float ms_decod = 0;    // decodificando gráficos nuevos (en el hilo principal)
    u32 decodificados = 0;
    u64 asign_frame_max = 0, asign_total = 0;  // allocations del hilo principal (sin las del driver)
    double acum_cpu = 0, max_cpu = 0;
    u64 acum_asign = 0, max_asign = 0;
    u64 t_fps = 0, frames_fps = 0;
    bool perf = false;
};

bool iniciar_app(App& a);
void bucle_app(App& a);
void cerrar_app(App& a);
void cambiar_pantalla(App& a, Pantalla p);
void enviar(App& a, Escritor e);

void frame_login(App& a);
void frame_personajes(App& a);
void frame_crear(App& a);
void frame_juego(App& a);
void pick_up_nearby(App& a);  // F4 in the original UI
// Modo no interactivo (LIN_RENDER): dibuja un mapa centrado en una celda y, tras unos frames,
// guarda un PNG y sale. Sin red ni login. Reusa el dibujo de mapa de frame_juego.
void dibujar_captura(App& a);

const char* nombre_clase(int clase);

// Ventanas (ventanas.cpp).
Rect rect_ventana(const App& a, IdVentana v);
// Nivel de ventana de los diálogos y avisos: encima de todas las ventanas (Gpu::nivel_ventana).
constexpr u32 NIVEL_SOBRE_VENTANAS = 31;

// ui::barra_scroll y, mientras se la usa, sin caminar aunque el puntero salga de la ventana.
void barra_scroll_ventana(App& a, ui::BarraScroll& st, int x, int y, int alto, u32 total, u32 visibles, u32& pos,
                          u64 clave, bool sobre_lista);
bool sobre_ventana(const App& a, IdVentana v);  // abierta, con el puntero encima y sin otra ventana encima
bool sobre_alguna_ventana(const App& a);
// Al dibujar cada ventana, antes que nada: arrastrarla por la barra de título.
void arrastrar_ventana(App& a, IdVentana v);
bool alguna_ventana_abierta(const App& a);  // alguna ventana IdVentana abierta (salvo la barra flotante de HP/MP)
void close_listed_windows(App& a);  // Esc: las cuatro ventanas de lista y un intercambio pendiente (interfaz original)
void guardar_posiciones_ventanas(App& a);  // after a custom drag
// Una vez por frame, antes de procesar la entrada: las que se abren pasan adelante (y a un lugar
// libre), y la que se toca con el clic también.
void actualizar_ventanas(App& a);
bool tapa_a_otra(const App& a, IdVentana v);  // se solapa con otra abierta que está detrás
u8 opacidad_ventanas(const App& a);
// Ventanas con marco del cliente original (original_window.cpp): tamaño, marco con botón de cerrar (devuelve true
// cuando se cliquea), adornos dibujados sobre el contenido y la barra de scroll con imágenes.
constexpr int ORIGINAL_WINDOW_W = 190, ORIGINAL_WINDOW_H = 288;  // inventario y estado
constexpr int SHOP_WINDOW_W = 258, SHOP_WINDOW_H = 335;  // imagen de marco 1100 (tienda, depósito y otras listas)
constexpr int NPC_WINDOW_W = 258, NPC_WINDOW_H = 335;    // imagen de marco 1356 (diálogo de NPC)
void original_npc_size(const App& a, int& w, int& h);    // tamaño de la ventana del NPC en la interfaz original
constexpr int SPELLBOOK_WINDOW_W = 207, SPELLBOOK_WINDOW_H = 289;  // tamaño de la imagen de marco 934
bool uses_original_frame(const App& a, IdVentana v);
void draw_original_status(App& a, int x, int y);  // ventana de estado: pestañas de equipo y atributos
// close_x < 0: el botón de cerrar de las ventanas del tamaño del inventario. Devuelve true si se cliqueó el botón de cerrar.
// La caja de hover de las listas e inventario originales (0x49a440 oscurece a la mitad los píxeles debajo, 0x498720 dibuja el bisel).
// Color del nombre de un objeto según su estado en el protocolo (0 bendito, 1 normal, 2 maldito, 3 sin identificar; tabla 0x8f0f34).
u32 original_item_color(u8 state);
void draw_original_hover_box(App& a, int x, int y, int w, int h);
bool draw_original_frame(App& a, int x, int y, u32 background, int bg_x, int bg_y, u32 frame, int close_x = -1, int close_y = -1);
void draw_original_overlay(App& a, int x, int y, u32 image);  // imagen de adorno tal cual en la posición de la ventana
// Un botón hecho de dos imágenes (normal, puntero encima); true al cliquearlo.
bool draw_original_button(App& a, int x, int y, u32 image, u32 hover_image);
void dibujar_volumen(App& a);  // volume_ui.cpp
void dibujar_cartel(App& a);   // cartel_ui.cpp: ficha de un cartel (exe 0x4a6c90)
Rect rect_cartel(const App& a);
void abrir_cartel(App& a, Slot s);  // lee el nombre `$` y el rumbo del cartel; no hace nada si el nombre no es un id de desc
Rect rect_volumen(const App& a);
void dibujar_hpmp_flotante(App& a);  // float_hpmp_ui.cpp
Rect rect_hpmp_flotante(const App& a);
void abrir_lista_orig(App& a, std::string_view pagina, std::string_view datos);  // lista_ui.cpp; sin resultado: solo para las 3 páginas
bool es_pagina_lista(std::string_view pagina);
void dibujar_lista_orig(App& a);
void dibujar_destino_orig(App& a);  // marcadores_ui.cpp: la lista de marcadores de teleport (interfaz original)
Rect rect_destino_orig(const App& a);
bool destino_orig_abierto(const App& a);
Rect rect_lista_orig(const App& a);
void eventos_cocina(App& a, const Evento& e);  // cocina_ui.cpp
void dibujar_cocina(App& a);
Rect rect_cocina(const App& a);
bool cocina_abierta(const App& a);
void dibujar_mascota_orig(App& a);  // mascotas_ui.cpp
Rect rect_mascota_orig(const App& a);
bool mascota_orig_abierta(const App& a);
bool hpmp_flotante_abierta(const App& a);
void draw_original_ornament(App& a, int x, int y, u32 ornament);
void original_scrollbar(App& a, ui::BarraScroll& st, int x, int y, int height, u32 total, u32 visible, u32& pos,
                        bool over_list, u32 track_image = 0, int track_dy = 0);
// Botones para abrir las ventanas: los del panel original (esquina del panel derecho) o de texto
// (UI simple; devuelve dónde terminan a la izquierda).
void dibujar_botones_panel(App& a, int x, int y);
int dibujar_botones_texto(App& a, int x_der, int y_top);
int ancho_botones_texto();  // ancho del bloque de botones de la UI simple (2 columnas)
// Ajustes (ajustes_ui.cpp): con a.ventana_ajustes, la ventana modal encima de cualquier pantalla. app.cpp le
// deja la entrada del frame y a la pantalla de atrás le pasa una vacía.
void dibujar_ajustes(App& a);
void dibujar_ayuda_teclas(App& a);  // F1
// Clima (clima.cpp): lluvia o nieve sobre el mundo (hasta la fila `h`, donde empieza el panel).
void dibujar_clima(App& a, int w, int h, u64 ahora);
// Oscuridad del mundo (noche, mazmorras, luces de objetos y tinte de mapa): multiplica lo que va
// por debajo de CAPA_EFECTOS. Sin efecto en el agua (ahí va el tinte azul, F4).
void aplicar_luz(App& a, u64 ahora);
void preparar_luz(App& a);  // al arrancar, fuera del frame: color del agua y búferes

// Ventana del juego: en ventana, sin borde y con barra propia (ventana_ui.cpp); a pantalla completa, sin barra.
void dibujar_barra_ventana(App& a);
void alternar_pantalla_completa(App& a);
int alto_barra(const App& a);  // píxeles lógicos que tapa la barra (0 a pantalla completa)
// Sonido (sonido.cpp).
void cargar_sonidos(App& a);
void aplicar_volumenes(App& a);
void actualizar_sonido(App& a, u64 ahora);  // una vez por frame: música, ambiente y streams
// Segundos de la hora de juego: la del servidor, o la forzada con forzar-hora (ver luz_ui.cpp).
i64 hora_de_juego(const App& a);  // game clock as the exe reads it (0x4bad30)
// Ctrl+P: screenshot request (original UI: <settings folder>/Capture/LinC%04d.bmp + chat line, simple UI: captura.bmp).
void tomar_captura(App& a);
// Al dibujar un frame de animación: si cambió desde el anterior y tiene sonido, suena (con el
// volumen según la distancia al personaje). `ultimo`: O_SONIDO o EfectoVisual::sonido.
// `sonar_primero`: el primer frame también suena (los efectos de hechizos; un personaje recién
// aparecido no, porque puede estar a mitad de una acción).
void sonido_de_frame(App& a, u32& ultimo, const FrameAccion& f, double x, double y, u64 ahora, bool sonar_primero, u8 dir = 0);
// Cursores (cursores.cpp).
void crear_cursores(App& a);
void aplicar_cursor(App& a);  // al final del frame, si cambió
void destruir_cursores(App& a);
// Salida: "Restart" (C_Restart: vuelve al lugar de retorno de la zona, vivo o muerto) y "Quit".
void dibujar_salida(App& a);
void reiniciar(App& a);  // de pantalla_juego.cpp
// Si el servidor manda a un mapa que esta instalación no tiene (map/<id>/), se avisa en el chat.
void avisar_mapa(App& a, MapId mapa);

// Inventario (inventario_ui.cpp).
bool sobre_inventario(const App& a);
void dibujar_inventario(App& a);
void usar_item(App& a, ObjId item);
void soltar_item(App& a, ObjId item, bool preguntar = true);  // original UI asks how many of a stack in the chat line
// Al basurero: pregunta cuántos (o confirma, si es uno) y lo elimina (cantidad_ui.cpp).
void borrar_item(App& a, ObjId item);
// Varios a la vez (la selección con Shift): una sola confirmación y la pila entera de cada uno.
void borrar_items(App& a, std::span<const ObjId> items);

// Hechizos (hechizos_ui.cpp).
bool sobre_hechizos(const App& a);
// Lanzar un hechizo: sin objetivo se lanza en cuanto se pueda; con objetivo y `objetivo` = 0,
// espera el clic en el objetivo.
void pedir_hechizo(App& a, u32 id, ObjId objetivo);
// Al comienzo del frame del juego: F5–F12, Alt+S, elección del objetivo y prueba automática.
void procesar_hechizos(App& a, u64 ahora);
// Hechizo pendiente (desde mover_jugador): dirección de un paso hacia el objetivo si está lejos;
// -2 si lo lanzó o espera la demora del anterior (no moverse); -1 si no hay.
int paso_hechizo_pendiente(App& a, Slot yo, u64 ahora);
u32 hechizo_bajo_puntero(const App& a);
// Sobre el casillero de un hechizo: la espera que falta hasta poder lanzar otro (todos comparten
// la misma: el servidor no acepta otro hechizo hasta que termina).
void dibujar_espera_hechizo(App& a, int x, int y, int w, int h);
// Panel original: los casilleros F5–F12 con sus hechizos (`x`, `y`: esquina del panel 1019).
void dibujar_teclas_f(App& a, int x, int y);
int tecla_f_bajo_puntero(const App& a);
// Pone un hechizo/objeto (o nada) en la casilla k de F5..F12 y lo guarda por personaje.
void poner_tecla_f(App& a, int k, Atajo atajo);
void dibujar_hechizos(App& a);
// Efectos activos (haste, escudos...) arriba a la izquierda, y el aviso de elegir objetivo.
void dibujar_efectos_activos(App& a, u64 ahora);
size_t nombre_efecto_activo(const App& a, const EfectoActivo& ef, char* buf, size_t cap);

// Barra de atajos (barra_ui.cpp), abajo al centro sobre el panel.
std::string_view nombre_personaje(const App& a);
// Acciones de la interfaz que también usa el control en vivo (control.cpp).
void iniciar_login(App& a);                        // pantalla_login.cpp: conectar con cuenta y clave
void atender_lanzador(App& a);                     // pantalla_login.cpp: conectar cuando el lanzador da su puerto
void entrar_juego(App& a);                         // pantalla_personajes.cpp: con a.pj_sel
void enviar_chat(App& a, std::string_view linea);  // pantalla_juego.cpp: como escrita en el chat (comandos "/" incluidos)
ObjId buscar_item(const App& a, std::string_view texto);    // inventario_ui.cpp: el primero cuyo nombre lo contenga
u32 buscar_hechizo(const App& a, std::string_view texto);   // hechizos_ui.cpp: uno que sepa, por nombre
void alternar_comunidad(App& a);  // Ctrl+C / botón de las manos: página option2p (príncipe) u option2, alterna; interfaz simple: ventana Social
void accion_comunidad(App& a, std::string_view accion);
void cambiar_pagina_f(App& a, int delta);  // hechizos_ui.cpp
void rueda_teclas_f(App& a);                // rueda (shortcutscroll) y botón del medio sobre la casilla F elegida
void combate_musica(App& a, Slot objetivo, u64 ahora);  // sonido.cpp: empezó un combate (ataque, hechizo, recibir un ataque)
void emblemas_abrir(App& a);                            // social_ui.cpp: elige la caché de emblemas de este servidor
void emblemas_guardar(App& a, u64 ahora, bool ya);      // social_ui.cpp: la escribe si cambió (ya = ahora mismo)
void confirmar_prompt(App& a, std::string_view texto);  // social_ui.cpp: Enter en la línea de pregunta (Esc: cancelar_prompt)
void cancelar_prompt(App& a);
void pedir_soltar(App& a, ObjId item, u32 cantidad);  // interfaz original: soltar una pila preguntando cuántos en la línea del chat (texto 119)
void pedir_dar(App& a, ObjId objetivo, ObjId item, u32 cantidad);  // lo mismo para dar (texto 140)
void pedir_recoger(App& a, i32 x, i32 y, ObjId id, u32 cantidad);  // recoger; pregunta cuántos de una pila salvo con `getall`
void elegir_invitado(App& a);  // clic en el mundo mientras `invitando`
std::string datos_opciones(const App& a);  // social_ui.cpp: valores `<var src="#N">` de las páginas de opciones
bool accion_opciones(App& a, std::string_view accion);  // social_ui.cpp: acciones de option11/12/13 (true = atendida)
void alternar_opciones(App& a);  // botón Opciones / Ctrl+V: página option1 (interfaz simple: la ventana del fork)
void alternar_ayuda(App& a);     // botón Ayuda / Ctrl+H: página de ayuda (interfaz simple: la lista de teclas)
void abrir_pagina_local(App& a, std::string_view id);  // npc_ui.cpp: client-side HTML page in the NPC window (Ctrl+C: option2)
void activar(App& a, std::string_view vinculo, bool es_enlace);  // npc_ui.cpp: un vínculo de la página del NPC
void responder_pregunta(App& a, bool si);                   // social_ui.cpp: la primera pregunta pendiente
void aceptar_cantidad(App& a, i32 valor);                   // cantidad_ui.cpp: el diálogo abierto, con ese número
extern bool g_traza, g_traza_html;                 // app.cpp: registrar los paquetes y las páginas HTML
void crear_personaje_rapido(App& a, std::string_view nombre, int clase);  // pantalla_crear.cpp: puntos en orden
#if LIN_CONTROL
// Control en vivo para pruebas (control.cpp; solo en las versiones de desarrollo).
void control_iniciar(App& a);
void control_frame(App& a);    // antes de leer la entrada
void control_entrada(App& a);  // después: el mouse inyectado
void control_paquete(Srv s);   // cada paquete recibido
void control_cerrar();
#endif
// Casillero bajo el puntero, -1 si no hay.
int casillero_bajo_puntero(const App& a);
bool sobre_barra(const App& a);  // también el selector de página
// Un arrastre que empezó en arrastre_x0/y0 ya se movió lo suficiente para no ser un clic.
bool arrastre_movido(const App& a);
// Página visible de la barra (con vuelta: de la última a la primera).
void cambiar_pagina_barra(App& a, int pagina);
void poner_atajo(App& a, int casillero, Atajo atajo);
// Al comienzo del frame del juego: Alt+número y carga de la barra del personaje.
void procesar_barra(App& a);
// `y`: fila de arriba de la barra.
void dibujar_barra(App& a, int y);

// Auto-buffs (autobuffs_ui.cpp).
int autobuff_bajo_puntero(const App& a);  // -1 si no hay
bool sobre_autobuffs(const App& a);       // casilleros e interruptor
void poner_autobuff(App& a, int casillero, u32 hechizo);  // con aviso en el chat si no es un buff propio
void poner_autobuff_item(App& a, int casillero, ObjId item);  // un ítem: solo las pociones de POCIONES_AUTO
void procesar_autobuffs(App& a);          // después de procesar_barra: lanza el que toque
void eventos_autobuffs(App& a, const Evento& e);
void dibujar_autobuffs(App& a);           // después de dibujar_barra

// Auto-poción (autopocion_ui.cpp).
void procesar_autopocion(App& a);
void eventos_autopocion(App& a, const Evento& e);
void guardar_autopocion(App& a);          // a.autopocion.op en settings.ini (lo usan las opciones)
// Encendida, "Pot<60%" en (x, y) (rojo sin pociones de curación); devuelve el ancho (0 apagada).
int dibujar_indicador_pocion(App& a, float x, float y, u64 clave);
// Botón a la derecha de los auto-buffs: clic gira Off -> 30 -> 60 -> 80 -> 90 -> Off.
bool sobre_boton_pocion(const App& a);
void dibujar_boton_pocion(App& a);  // después de dibujar_autobuffs

// Polymorph (forma_ui.cpp).
bool cargar_formas(App& a);
// Abre el diálogo: con el pergamino usado, o 0 para Shape Change.
void abrir_forma(App& a, ObjId pergamino);
void abrir_polimorfia(App& a, ObjId pergamino);  // original page (npc_ui.cpp)
void eventos_forma(App& a, const Evento& e);
// Antes del chat: el diálogo se queda con el teclado.
void dibujar_forma(App& a);
bool sobre_forma(const App& a);

// NPCs y comercio (npc_ui.cpp). El diálogo va arriba a la izquierda.
// Tipo de gfx del objeto (TipoGfx); un gfx que no está en la tabla de sprites cuenta como monstruo.
u8 tipo_objeto(const App& a, Slot s);
u8 clase_objeto(const App& a, Slot s);  // clase de objeto del cliente original (0x4cdd30)
// Lista de NPC original: dígitos / Backspace escritos en la cantidad de la fila marcada (0x4be860); llamar antes de la barra y del chat.
void teclas_lista_tienda(App& a);
bool es_npc(const App& a, Slot s);
// Encola un texto de combate (daño/"Miss") sobre el personaje; descarta el más viejo si no entra.
void empujar_texto_combate(App& a, const char* t, u8 r, u8 g, u8 b);
void eventos_npc(App& a, const Evento& e);
bool sobre_npc(const App& a);
// Al comienzo del frame del juego: prueba automática y cierre al alejarse.
void procesar_npc(App& a, u64 ahora);
void dibujar_npc(App& a);

// Social (social_ui.cpp).
void eventos_social(App& a, const Evento& e);

// Diálogo de cantidad y dar ítems (cantidad_ui.cpp).
void eventos_cantidad(App& a, const Evento& e);
// Reiniciar atributos (reinicio_ui.cpp).
void eventos_reinicio(App& a, const Evento& e);
void dibujar_reinicio(App& a);
// Atributos base de cada clase (pantalla_crear.cpp): fue, des, con, sab, car, int; y sus puntos libres.
const u8* stats_base_clase(int clase);
int bonus_clase(int clase);
// Dar un ítem del inventario a un NPC o una mascota; de una pila, pregunta cuántos.
void dar_item(App& a, ObjId objetivo, ObjId item);
// Varios a la vez: la pila entera de cada uno, sin preguntar.
void dar_items(App& a, ObjId objetivo, std::span<const ObjId> items);
void dibujar_cantidad(App& a);
bool sobre_cantidad(const App& a);

// Mascotas e invocaciones (mascotas_ui.cpp).
void eventos_mascotas(App& a, const Evento& e);
void procesar_gm_apuntado(App& a, u64 ahora);  // GM Ctrl+Y / Ctrl+Z: elige un objetivo y manda el opcode 83
void procesar_item_apuntado(App& a);  // clic en el objetivo de un objeto apuntado (varita de polymorph)
void procesar_mascotas(App& a);  // antes del movimiento: elegir el objetivo de una mascota
void dibujar_mascotas(App& a);
bool sobre_mascotas(const App& a);

// Marcadores de teleport (marcadores_ui.cpp).
void eventos_marcadores(App& a, const Evento& e);
// Confirmación de un buff sobre el arma o la armadura (hechizos_ui.cpp).
void eventos_hechizos(App& a, const Evento& e);
// "/bookmark nombre" y "/delbookmark nombre"; true si era uno de esos.
bool comando_marcador(App& a, std::string_view orden, std::string_view nombre);
void abrir_prompt_borrar_marcador(App& a, ObjId marcador);  // Supr en una fila de marcador: pregunta sí/no (texto 0x34e)
void abrir_prompt_marcador(App& a);                        // social_ui.cpp: /bookmark sin nombre (texto 0x246)
void marcar_lugar_aqui(App& a, std::string_view nombre);  // revisa el largo del nombre / el mapa, después C_AddBookmark
int clase_personaje(const App& a);                          // byte de clase del personaje elegido (0 príncipe, 2 elfo)
bool es_hechizo_teleport(u32 id);
// Abre el diálogo de destino (si hay marcadores) para un pergamino o un hechizo de teleport;
// false si no hay ninguno (se usa como siempre).
bool elegir_destino(App& a, ObjId item, u32 hechizo);
void dibujar_destino(App& a);
bool sobre_destino(const App& a);
// Comandos del cliente que empiezan con "/" (/invite, /trade, /pledge...); false si no es uno.
bool comando_social(App& a, std::string_view linea);
void ofrecer_en_intercambio(App& a, ObjId item);  // todo el montón
void dibujar_pregunta(App& a);  // antes del chat: Enter y Escape responden
void dibujar_intercambio(App& a);
void dibujar_social(App& a);
void dibujar_grupo(App& a);
// Pestaña "Equipment" de la ventana de estado (equipo_ui.cpp), en la ventana de esquina (x, y).
void dibujar_equipo(App& a, int x, int y);
// Tienda de jugadores (tienda_ui.cpp).
void eventos_tienda(App& a, const Evento& e);
void eventos_pesca(App& a, const Evento& e);
bool clic_pesca(App& a);  // un clic en el mundo con el corcho afuera: C_FishClick; true si tomó el clic
void dibujar_pesca(App& a, u64 ahora, double ox, double oy);
void dibujar_tienda(App& a);       // solo interfaz simple
void dibujar_tienda_original(App& a);  // ventana original, en la pila de ventanas
Rect rect_tienda_original(const App& a);
bool tienda_original_abierta(const App& a);
bool sobre_tienda(const App& a);
const Vendedor* vendedor(const App& a, ObjId id);
void abrir_tienda_de(App& a, ObjId vendedor);           // al llegar junto al vendedor
bool comando_tienda(App& a, std::string_view orden, std::string_view arg);
bool clic_item_tienda(App& a, ObjId item);              // inventario: agregar a la propia mientras se arma
void aceptar_cantidad_tienda(App& a);
// Una ventana con un campo de texto tiene el teclado: los atajos (1–0, A, Enter del chat) no actúan.
inline bool escribiendo_en_ventana(const App& a) {
    return a.ventana_forma || a.tablon.escribiendo || (a.tienda_pj.armando && !a.cfg.ui_simple) || a.correo.escribiendo ||
           (!a.preguntas.empty() && (a.preguntas.front().tipo == PREGUNTA_NOMBRE_CASA));
}
// Tablón de los pueblos (tablon_ui.cpp) y correo (correo_ui.cpp).
bool es_tablon(GfxId gfx);
void eventos_tablon(App& a, const Evento& e);
void dibujar_tablon(App& a);
bool sobre_tablon(const App& a);
void eventos_correo(App& a, const Evento& e);
void dibujar_correo(App& a);
bool sobre_correo(const App& a);
bool comando_correo(App& a, std::string_view orden);
// Casas (casas_ui.cpp) y castillos (castillos_ui.cpp).
void eventos_casas(App& a, const Evento& e);
void dibujar_casas(App& a);
bool sobre_casas(const App& a);
// World-map items (map_ui.cpp).
void eventos_mapa(App& a, const Evento& e);
void dibujar_mapa_item(App& a);
bool sobre_mapa(const App& a);
void eventos_castillos(App& a, const Evento& e);
void dibujar_castillos(App& a);
bool comando_castillos(App& a, std::string_view orden, std::string_view resto);
u8 castillo_de(const App& a, ObjId id);
void dibujar_corona(Graficos& g, float x, float y, u64 k);  // el señor del castillo, junto al nombre  // el castillo del que es señor (0 = ninguno)
u32 adena_inventario(const App& a);  // npc_ui.cpp
void abrir_cantidad_castillo(App& a, int tipo, ObjId personaje, i32 min, i32 max, i32 inicial);
const char* nombre_castillo(int castillo);
Rect rect_social(const App& a);
Rect rect_intercambio(const App& a);

// De pantalla_juego.cpp: objeto vivo bajo el puntero (0 si no hay; el propio personaje solo con
// `incluir_propio`) y dirección (0–7) de un paso hacia (dx, dy), -1 si es (0, 0).
ObjId objeto_bajo_puntero(const App& a, bool incluir_propio = false, bool incluir_muertos = false);
int direccion_de(int dx, int dy);

// Eventos de la red que afectan a las pantallas previas al juego.
void eventos_login(App& a, const Evento& e);
void eventos_personajes(App& a, const Evento& e);

}  // namespace lin
