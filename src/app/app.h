// Estado de la aplicación y pantallas. Cada pantalla es una función que, por frame, lee la
// entrada, emite comandos de red y dibuja (UI inmediata).
#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <array>
#include <string>
#include <vector>
#include "audio/audio.h"
#include "mundo/chat.h"
#include "mundo/mundo.h"
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
#include "render/graficos.h"
#include "ui/ui.h"

namespace lin {

enum class Pantalla { ELEGIR_CARPETA, LOGIN, PERSONAJES, CREAR, CARGANDO, JUEGO };

struct PersonajeLista {
    std::string nombre, clan;
    i32 v[13];  // índices PJ_* de eventos.h
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
};
// Ventanas del juego (ventanas.cpp): se mueven arrastrando la barra de título.
enum class IdVentana : u8 { ESTADO, INVENTARIO, HECHIZOS, NPC, OPCIONES, SALIDA, SOCIAL, INTERCAMBIO, CANTIDAD };
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
    std::vector<cmd::LineaTienda> venta;
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
struct ElegirDestino {
    bool abierto = false;
    ObjId item = 0;   // pergamino de teleport, o 0 si es el hechizo
    u32 hechizo = 0;  // Teleport o Mass Teleport
};

// Reiniciar atributos (reinicio_ui.cpp): S_CharReset / C_CharReset, con la Candle of Reminiscence.
struct ReinicioAtributos {
    enum Fase : u8 { CERRADO, INICIAL, SUBIR, ELIXIR } fase = CERRADO;
    int nivel = 1, maximo = 1, hp = 0, mp = 0, ac = 0, puntos = 0;
    u8 stats[6] = {};  // fue, int, sab, des, con, car (el orden de C_CharReset)
    u8 extra[6] = {};  // lo que se va sumando en la fase inicial o con los elixires
};

// Diálogo de cantidad (cantidad_ui.cpp): S_InputAmount del servidor, o cuántos dar de una pila.
struct DialogoCantidad {
    bool abierto = false;
    bool dar = false;         // true: C_GiveItem del `item` a `objeto`; false: C_Amount a `objeto`
    bool borrar = false;      // true: C_DeleteInventoryItem del `item` (el basurero)
    u8 tienda = 0;            // tienda de jugadores (tienda_ui.cpp): 1 comprar, 2 venderle, 3 precio al armar
    u8 castillo = 0;          // castillos_ui.cpp: CF_TASA + 1 impuesto, CF_DEPOSITAR + 1 poner, CF_RETIRAR + 1 sacar
    bool escribiendo = false; // ya se escribió un número (el siguiente se agrega en vez de reemplazar)
    ObjId objeto = 0, item = 0;
    i32 valor = 1, min = 1, max = 1, precio = 0;
    std::string accion, titulo;
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
};

// Cursor del mouse (cursores.cpp): mano, espada sobre un monstruo, círculo al elegir objetivo.
enum class Cursor : u8 { MANO, ESPADA, OBJETIVO, LABIOS, CANTIDAD };

struct App {
    Config cfg;
    SDL_Window* ventana = nullptr;
    bool salir = false;

    Recursos rec;
    FuenteBitmap fuente;
    TablaSprites sprites;
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

    TablaOpcodes op{};
    HiloRed red;
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
    u64 t_mantener_vivo = 0;
    bool ayuda_teclas = false;     // F1
    u64 t_intento_portal = 0;      // último paso hacia una celda bloqueada (pantalla_juego.cpp: intento_portal)
    u32 celda_intento_portal = 0;
    // Combate: lo que se dibujó en el frame anterior (para saber qué hay bajo el puntero), el
    // objetivo del ataque y hasta cuándo dura el ataque en curso.
    struct ObjetoEnPantalla {
        ObjId id;
        float x0, y0, x1, y1;
        u32 profundidad;
    };
    std::vector<ObjetoEnPantalla> en_pantalla;
    bool jugador_tapado = false;  // un objeto del mapa tapa al propio personaje (se ve translúcido)
    ObjId objetivo = 0;
    u64 fin_ataque = 0;
    u64 t_muerte = 0;  // cuándo se vio el aviso de muerte (0 = vivo)
    std::string chat_entrada;
    u32 chat_desplazamiento = 0;  // líneas hacia atrás desde la más reciente
    bool ventana_estado = false;
    u8 pestana_estado = 0;  // 0 atributos, 1 equipo (equipo_ui.cpp)
    // Hechizos (Alt+S, F5–F12). Un hechizo con objetivo espera el clic en el objetivo
    // (hechizo_apuntando) y después se camina hasta tenerlo en alcance (hechizo_pendiente).
    bool ventana_hechizos = false;
    u32 hechizos_desplazamiento = 0;  // filas
    int hechizos_pestana = 0;
    int teclas_f_x = 0, teclas_f_y = -1;  // panel original: casilleros F5–F12 (-1 = no se dibujan)         // ventana original: nivel 1..10 (0..9) o habilidades de clase (10)
    u32 hechizo_apuntando = 0;
    u32 hechizo_pendiente = 0;
    ObjId objetivo_hechizo = 0;
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
    bool clic_consumido = false;      // el clic que eligió el objetivo no camina hasta soltarlo
    u16 atajos[8] = {};               // F5..F12
    std::string atajos_de;            // personaje cuyos atajos están cargados
    // Barra de atajos (Alt+1..Alt+0; barra_ui.cpp): hechizos o ítems arrastrados a sus casilleros.
    Atajo barra[CASILLEROS_BARRA];
    std::string barra_de;             // personaje cuya barra está cargada
    int barra_x = 0, barra_y = -1000; // posición del último frame (para el puntero)
    u32 arrastrando_hechizo = 0;      // desde la ventana de hechizos
    int arrastrando_atajo = -1;       // desde un casillero de la barra
    int casillero_elegido = -1;       // el que usa el clic del medio sobre el mundo (-1 ninguno)
    int arrastre_x0 = 0, arrastre_y0 = 0;
    // Polymorph (forma_ui.cpp): diálogo para escribir el monstruo.
    bool ventana_forma = false;
    ObjId forma_item = 0;             // pergamino usado; 0 = Shape Change (se responde con C_NPCAction)
    std::string forma_texto;
    std::vector<std::string> formas;  // nombres de polymorph.tbl (sugerencias)
    // NPC (npc_ui.cpp): diálogo con la página HTML, y tienda o almacén en la misma ventana.
    ObjId npc = 0;
    std::string npc_pagina, npc_datos;
    u32 npc_casa = 0;  // página "agsel" del tablón de subastas: la casa ("map" y "apply" la llevan)
    PaginaHtml npc_html;
    int npc_desplazamiento = 0;  // px
    TipoTienda tienda = TipoTienda::NINGUNA;
    ObjId tienda_npc = 0;
    std::vector<FilaTienda> tienda_filas;
    u32 tienda_desplazamiento = 0;
    bool hablar_objetivo = false;  // a.objetivo es un NPC al que se camina para hablarle
    bool objetivo_fijo = false;    // elegido al apretar el botón: se mantiene aunque el puntero ya no esté encima
    bool ataque_sostenido = false; // tecla A: se sigue atacando al objetivo sin mantener el botón
    bool modo_pk = false;          // "/pk": el clic sobre otro jugador lo ataca sin Ctrl
    // Ventanas: posiciones, la que se está arrastrando y la de opciones.
    PosVentana pos_ventanas[static_cast<int>(IdVentana::CANTIDAD)];
    bool ventanas_cargadas = false;
    // De atrás hacia adelante (IdVentana); la de más arriba abierta bajo el puntero, o -1.
    u8 orden_ventanas[static_cast<int>(IdVentana::CANTIDAD)] = {0, 1, 2, 3, 4, 5, 6, 7};
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
    bool ventana_opciones = false;
    bool ventana_salida = false;  // Restart / Quit
    // Texto de combate flotante sobre el personaje (daño recibido / "Miss"), si cfg.texto_dano.
    struct TextoCombate { char t[12]; u64 t0; u8 r, g, b; };
    TextoCombate textos_combate[6];
    u8 n_textos_combate = 0;
    i32 hp_anterior = 0;  // detectar daño recibido: caída de HP tras aplicar los eventos del frame
    // Render no interactivo a PNG (LIN_RENDER=id,x,y,salida.bmp): dibuja el mapa y sale. Ver app.cpp.
    struct { bool activo = false; MapId mapa = 0; int x = 0, y = 0; std::string salida; } render;
    bool ventana_social = false;
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
    ObjId arrastrando = 0;              // ítem que se arrastra (para soltarlo al suelo)
    ObjId usar_sobre = 0;               // pergamino (identificar, encantar) esperando el clic en su objetivo
    ObjId ultimo_clic_item = 0;         // para el doble clic
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
// Modo no interactivo (LIN_RENDER): dibuja un mapa centrado en una celda y, tras unos frames,
// guarda un PNG y sale. Sin red ni login. Reusa el dibujo de mapa de frame_juego.
void dibujar_captura(App& a);

const char* nombre_clase(int clase);

// Ventanas (ventanas.cpp).
Rect rect_ventana(const App& a, IdVentana v);
bool sobre_ventana(const App& a, IdVentana v);  // abierta, con el puntero encima y sin otra ventana encima
bool sobre_alguna_ventana(const App& a);
// Al dibujar cada ventana, antes que nada: arrastrarla por la barra de título.
void arrastrar_ventana(App& a, IdVentana v);
// Una vez por frame, antes de procesar la entrada: las que se abren pasan adelante (y a un lugar
// libre), y la que se toca con el clic también.
void actualizar_ventanas(App& a);
bool tapa_a_otra(const App& a, IdVentana v);  // se solapa con otra abierta que está detrás
u8 opacidad_ventanas(const App& a);
// Botones para abrir las ventanas: los del panel original (esquina del panel derecho) o de texto
// (UI simple; devuelve dónde terminan a la izquierda).
void dibujar_botones_panel(App& a, int x, int y);
int dibujar_botones_texto(App& a, int x_der, int y_top);
int ancho_botones_texto();  // ancho del bloque de botones de la UI simple (2 columnas)
void dibujar_opciones(App& a);
void dibujar_ayuda_teclas(App& a);  // F1
// Clima (clima.cpp): lluvia o nieve sobre el mundo (hasta la fila `h`, donde empieza el panel).
void dibujar_clima(App& a, int w, int h, u64 ahora);
// Sonido (sonido.cpp).
void cargar_sonidos(App& a);
void aplicar_volumenes(App& a);
void actualizar_sonido(App& a, u64 ahora);  // una vez por frame: música, ambiente y streams
// Al dibujar un frame de animación: si cambió desde el anterior y tiene sonido, suena (con el
// volumen según la distancia al personaje). `ultimo`: O_SONIDO o EfectoVisual::sonido.
// `sonar_primero`: el primer frame también suena (los efectos de hechizos; un personaje recién
// aparecido no, porque puede estar a mitad de una acción).
void sonido_de_frame(App& a, u32& ultimo, const FrameAccion& f, double x, double y, u64 ahora, bool sonar_primero);
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
void soltar_item(App& a, ObjId item);
// Al basurero: pregunta cuántos (o confirma, si es uno) y lo elimina (cantidad_ui.cpp).
void borrar_item(App& a, ObjId item);

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
void dibujar_hechizos(App& a);
// Efectos activos (haste, escudos...) arriba a la izquierda, y el aviso de elegir objetivo.
void dibujar_efectos_activos(App& a, u64 ahora);
size_t nombre_efecto_activo(const App& a, const EfectoActivo& ef, char* buf, size_t cap);

// Barra de atajos (barra_ui.cpp), abajo al centro sobre el panel.
std::string_view nombre_personaje(const App& a);
// Acciones de la interfaz que también usa el control en vivo (control.cpp).
void iniciar_login(App& a);                        // pantalla_login.cpp: conectar con cuenta y clave
void entrar_juego(App& a);                         // pantalla_personajes.cpp: con a.pj_sel
void enviar_chat(App& a, std::string_view linea);  // pantalla_juego.cpp: como escrita en el chat (comandos "/" incluidos)
ObjId buscar_item(const App& a, std::string_view texto);    // inventario_ui.cpp: el primero cuyo nombre lo contenga
u32 buscar_hechizo(const App& a, std::string_view texto);   // hechizos_ui.cpp: uno que sepa, por nombre
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
bool sobre_barra(const App& a);
void poner_atajo(App& a, int casillero, Atajo atajo);
// Al comienzo del frame del juego: Alt+número y carga de la barra del personaje.
void procesar_barra(App& a);
// `y`: fila de arriba de la barra.
void dibujar_barra(App& a, int y);

// Polymorph (forma_ui.cpp).
bool cargar_formas(App& a);
// Abre el diálogo: con el pergamino usado, o 0 para Shape Change.
void abrir_forma(App& a, ObjId pergamino);
void eventos_forma(App& a, const Evento& e);
// Antes del chat: el diálogo se queda con el teclado.
void dibujar_forma(App& a);
bool sobre_forma(const App& a);

// NPCs y comercio (npc_ui.cpp). El diálogo va arriba a la izquierda.
// Tipo de gfx del objeto (TipoGfx); un gfx que no está en la tabla de sprites cuenta como monstruo.
u8 tipo_objeto(const App& a, Slot s);
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
void dibujar_cantidad(App& a);
bool sobre_cantidad(const App& a);

// Mascotas e invocaciones (mascotas_ui.cpp).
void eventos_mascotas(App& a, const Evento& e);
void procesar_mascotas(App& a);  // antes del movimiento: elegir el objetivo de una mascota
void dibujar_mascotas(App& a);
bool sobre_mascotas(const App& a);

// Marcadores de teleport (marcadores_ui.cpp).
void eventos_marcadores(App& a, const Evento& e);
// Confirmación de un buff sobre el arma o la armadura (hechizos_ui.cpp).
void eventos_hechizos(App& a, const Evento& e);
// "/bookmark nombre" y "/delbookmark nombre"; true si era uno de esos.
bool comando_marcador(App& a, std::string_view orden, std::string_view nombre);
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
void dibujar_tienda(App& a);
bool sobre_tienda(const App& a);
const Vendedor* vendedor(const App& a, ObjId id);
void abrir_tienda_de(App& a, ObjId vendedor);           // al llegar junto al vendedor
bool comando_tienda(App& a, std::string_view orden, std::string_view arg);
bool clic_item_tienda(App& a, ObjId item);              // inventario: agregar a la propia mientras se arma
void aceptar_cantidad_tienda(App& a);
// Una ventana con un campo de texto tiene el teclado: los atajos (1–0, A, Enter del chat) no actúan.
inline bool escribiendo_en_ventana(const App& a) {
    return a.ventana_forma || a.tablon.escribiendo || a.correo.escribiendo ||
           (!a.preguntas.empty() && a.preguntas.front().tipo == PREGUNTA_NOMBRE_CASA);
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
ObjId objeto_bajo_puntero(const App& a, bool incluir_propio = false);
int direccion_de(int dx, int dy);

// Eventos de la red que afectan a las pantallas previas al juego.
void eventos_login(App& a, const Evento& e);
void eventos_personajes(App& a, const Evento& e);

}  // namespace lin
