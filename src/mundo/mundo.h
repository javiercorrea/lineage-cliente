// Estado del juego en tablas columnares y los sistemas que lo transforman.
//
// Los eventos de red se aplican en lote (aplicar_eventos) al inicio del frame. La posición
// interpolada y el frame de animación son funciones puras del estado y del tiempo: no hay
// estado de animación por objeto que actualizar cada frame.
#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "core/tabla_soa.h"
#include "mundo/cargador_segmentos.h"
#include "mundo/inventario.h"
#include "proto/eventos.h"
#include "recursos/hechizos.h"
#include "recursos/mapa.h"
#include "recursos/pak.h"
#include "recursos/tabla_sprites.h"

namespace lin {

// Columnas de la tabla de objetos (personajes, NPCs, monstruos, puertas...).
enum ColObjeto : size_t {
    O_X, O_Y,            // celda actual (destino si se está moviendo)
    O_XANT, O_YANT,      // posición (en celdas, fraccionaria) desde la que empezó el último paso
    O_TMOV,              // inicio del último paso (ms)
    O_DURMOV,            // duración del paso (ms): la animación de caminar de su sprite
    O_DIR, O_GFX, O_ESTADO,  // estado = arma actual (se suma a la acción base) o ESTADO_MUERTO
    O_ACCION, O_TACCION, // acción puntual (ataque, daño...) y su inicio (puede ser futuro); 0xFF = ninguna
    O_LUZ, O_HP, O_FLAGS,  // HP en % (S_CharPack, S_HpMeter; 0xFF = desconocido)
    O_VEL,               // velocidad de S_CharPack: 0 normal, 1 rápido (haste), 2 lento
    O_CANTIDAD,          // ítems del suelo: cantidad (para recogerlos todos)
    O_CLAN,              // id del clan (0 sin clan): para su emblema
    O_LEGAL,             // alineamiento (color del nombre)
    O_SONIDO,            // último frame de animación dibujado (índice + 1; 0 = ninguno): su sonido suena una vez
    O_NOMBRE, O_TITULO,
};
using TablaObjetos =
    TablaSoa<u16, u16, double, double, u64, u16, u8, u16, u8, u8, u64, u8, u8, u8, u8, u32, u32, i16, u32, TextoRef, TextoRef>;

// Efecto con duración sobre el propio personaje (módulo "Buffs" del plan): haste, brave, escudos...
// Clase EA_* y tipo según el paquete que lo informó (ver ICONO_EFECTO y VELOCIDAD en eventos.h).
enum ClaseEfecto : u8 { EA_HASTE, EA_BRAVE, EA_ESCUDO, EA_AURA, EA_ICONO };
struct EfectoActivo {
    u8 clase, tipo;
    u64 fin;  // ms
};
constexpr u32 MAX_EFECTOS_ACTIVOS = 24;

// Estado del propio personaje (módulo "Player" del plan). Lo actualizan S_OwnCharStatus y los
// paquetes parciales (HP, MP, EXP, alineamiento, SP/MR, defensa, atributos, peso, comida).
struct EstadoJugador {
    ObjId id = 0;
    i32 nivel = 0, exp = 0, fue = 0, int_ = 0, sab = 0, des = 0, con = 0, car = 0;
    i32 hp = 0, hp_max = 0, mp = 0, mp_max = 0, ca = 0, legal = 0;
    i32 comida = 0;  // 0–225 (225 = lleno)
    i32 peso = 0;    // 0–240 (240 = 100 % de la carga máxima)
    i32 sp = 0, mr = 0;  // agregados por equipo y magia (S_SpMr)
    bool muerto = false;  // recibió su acción de morir; vuelve con C_Restart (reiniciar_jugador)
    i32 fuego = 0, agua = 0, viento = 0, tierra = 0;
    u8 hechizos[FILAS_HECHIZOS] = {};  // S_AddSkill: fila = nivel, bit c = hechizo fila*8+c+1
    u8 haste = 0;  // 0 normal, 1 rápido, 2 lento (S_SkillHaste)
    u8 brave = 0;  // 0 nada, 1 poción, 3 waffle, 4 caminar rápido, 6 bloodlust (S_SkillBrave)
    u8 inmovil = 0;   // S_Paralysis: el que empezó (PARALISIS_*), 0 = puede moverse y actuar
    u8 ceguera = 0;   // S_CurseBlind: 0 ve, 1 ciego, 2 ciego con Floating Eye
    EfectoActivo efectos[MAX_EFECTOS_ACTIVOS];
    u32 n_efectos = 0;

    bool sabe(u32 id) const {
        return id >= 1 && id <= FILAS_HECHIZOS * 8u && ((hechizos[(id - 1) / 8] >> ((id - 1) % 8)) & 1);
    }
};

// S_Paralysis: subtipos que empiezan cada estado (el siguiente número lo termina). El 7 (fin de la
// espera tras un teleport) no cambia nada aquí.
enum Paralisis : u8 {
    PARALISIS = 2, PARALISIS_2 = 4, SUENO = 10, CONGELADO = 12, ATURDIDO = 22, ATADO = 24,
};
// Flags de S_CharPack (y S_Poison): veneno (verde), invisible, congelado (gris).
constexpr u8 FLAG_VENENO = 1, FLAG_INVISIBLE = 2, FLAG_CONGELADO = 8;
// Tipo de gfx de las puertas (atributo 102): sus acciones son abrir (28) y cerrar (29).
constexpr u8 TIPO_PUERTA = 8, ACC_ABRIR = 28, ACC_CERRAR = 29;

// Gráfico de un hechizo en el mundo: sobre un objeto (lo sigue), en una celda, o un proyectil que
// vuela de (x0, y0) a (x1, y1) durante vuelo_ms. Se dibuja la acción 0 del gfx una vez (el
// proyectil, en bucle mientras vuela). Anillo de tamaño fijo: el más viejo se pisa.
struct EfectoVisual {
    GfxId gfx = 0;
    u8 dir = 0;
    ObjId objeto = 0;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    u64 inicio = 0;
    u32 vuelo_ms = 0;
    u32 sonido = 0;  // último frame dibujado (como O_SONIDO)
};
constexpr u32 MAX_EFECTOS_VISUALES = 128;
// Velocidad de los proyectiles (hechizos y flechas). No está en los datos: estimada mirando el
// original en video (una celda cada ~40 ms).
constexpr u32 MS_VUELO_CELDA = 40;

// Porcentaje de experiencia dentro del nivel actual (tabla de ExpTable.java de L1J).
float porcentaje_exp(i32 nivel, i32 exp);

// Paso normal de los personajes (spr_actions de L1J: 640 ms por celda); también el de los objetos
// cuyo sprite no está en la tabla de sprites.
constexpr u16 DURACION_PASO_MS = 640;
// Paso propio: el anti-acelerador de L1J (AcceleratorChecker, CheckStrictness=102) multiplica el
// intervalo entre C_MoveChar por 0,97 y exige que no baje de lo que dura caminar en el sprite; con
// 10 pasos seguidos por debajo desconecta ("Overspeed detected"). El cliente original camina ~5 %
// más lento que el valor teórico, y así se hace aquí (con 640 ms, 672).
constexpr u16 DURACION_PASO_PROPIO_MS = DURACION_PASO_MS * 105 / 100;
static_assert(DURACION_PASO_PROPIO_MS * 97 / 100 >= DURACION_PASO_MS, "el servidor lo tomaría como acelerador");

// Alcance del ataque (celdas) según el arma: el estado visual de S_CharPack / S_CharVisualUpdate,
// que es el tipo de arma de L1J. Arco (20) y guantelete (62) disparan a todo lo que esté "en
// pantalla" para el servidor (L1Attack.calcHit -> Point.isInScreen: 18 celdas; además pide línea de
// vista, que el cliente no comprueba: con un obstáculo, el servidor lo cuenta como fallo); la lanza
// (24) llega a 2 celdas (range de weapons); el resto, cuerpo a cuerpo.
constexpr u8 ARMA_ARCO = 20, ARMA_GUANTELETE = 62, ARMA_LANZA = 24;
constexpr int ALCANCE_DISTANCIA = 18;
constexpr int alcance_ataque(u8 arma) {
    return arma == ARMA_ARCO || arma == ARMA_GUANTELETE ? ALCANCE_DISTANCIA : arma == ARMA_LANZA ? 2 : 1;
}

// Lo que mide el anti-acelerador, y el factor que le aplica según haste, lentitud y brave del
// propio personaje (AcceleratorChecker.getRightInterval: × 0,75 haste, ÷ 0,75 lento, × 0,75
// poción de brave, × 0,87 waffle; × 0,75 al caminar con Holy Walk y parecidos; × 0,65 al atacar
// con Bloodlust).
enum class Intervalo : u8 { CAMINAR, ATACAR, HECHIZO };
double factor_intervalo(const EstadoJugador& j, Intervalo t);
// Duración de un intervalo propio de `base` ms (lo que dura la animación en el sprite): con el
// factor de arriba y el 5 % de margen. Al caminar, además, alcanza para encadenar pasos
// (MS_ENCADENAR_PASO en pantalla_juego.cpp) sin bajar del mínimo.
u32 intervalo_propio(const EstadoJugador& j, Intervalo t, u32 base, u32 ms_encadenar = 0);

struct Mundo {
    TablaObjetos objetos;
    Inventario inventario;
    const TablaSprites* sprites = nullptr;  // para la duración de los pasos (caminar de cada sprite)
    EstadoJugador jugador;
    MapId mapa = 0;
    i32 hora = 0;
    u8 clima = 0;
    bool en_juego = false;
    EfectoVisual efectos[MAX_EFECTOS_VISUALES];
    u32 siguiente_efecto = 0;

    // Segmentos de mapa cargados (clave: mapa << 32 | sx << 16 | sy). El índice también marca los
    // que no existen y los que se están cargando en el hilo (cargador, que se crea al primer uso).
    std::vector<SegmentoMapa> segmentos;
    MapaHash indice_segmentos;
    std::unique_ptr<CargadorSegmentos> cargador;
    std::vector<CargadorSegmentos::Hecho> segmentos_hechos;  // reutilizado por actualizar_segmentos
    // Celdas de puertas cerradas (S_Door; clave x << 16 | y): no se pasa aunque el mapa diga que sí.
    MapaHash puertas_cerradas{64};

    // Textos de los objetos (nombres, títulos): arena que crece y se compacta (compactar_textos).
    std::vector<char> textos;

    void limpiar();
};

TextoRef guardar_texto(Mundo& m, std::string_view t);
inline std::string_view texto(const Mundo& m, TextoRef r) { return {m.textos.data() + r.ofs, r.len}; }

// Estado de un objeto muerto: el que manda el servidor (ActionCodes.ACTION_Die) en S_CharPack y
// el que queda tras su S_DoActionGFX de morir. Se dibuja el último frame de morir (el cadáver)
// hasta que el servidor lo quita.
constexpr u8 ESTADO_MUERTO = ACC_MORIR;

void aplicar_eventos(Mundo& m, std::span<const Evento> eventos, u64 ahora_ms);
void agregar_efecto(Mundo& m, const EfectoVisual& e);
// Quita los efectos activos que ya terminaron.
void vencer_efectos(EstadoJugador& j, u64 ahora);
// Al pedir el reinicio (C_Restart) el servidor reubica al personaje y vuelve a mandar lo que ve:
// se descarta todo lo demás y el estado de muerte.
void reiniciar_jugador(Mundo& m);

// Segmento que contiene la celda; lo carga la primera vez (en el momento: para la lógica del
// juego, como si una celda es pasable). nullptr si no existe en el disco.
const SegmentoMapa* segmento(Mundo& m, const Recursos& rec, int x, int y);
// Igual, sin esperar (para el render): si no está, lo pide al hilo de carga y devuelve nullptr.
const SegmentoMapa* segmento_cargado(Mundo& m, const Recursos& rec, int x, int y);
// Una vez por frame, antes de dibujar: guarda los segmentos que terminó el hilo y libera los que
// quedaron a más de `radio` segmentos de (x, y). Los punteros a segmentos valen hasta la próxima
// llamada.
void actualizar_segmentos(Mundo& m, int x, int y, int radio);
bool celda_pasable(Mundo& m, const Recursos& rec, int x, int y);

// Proyección isométrica (map_coord.cpp de l1j-client): celda -> píxel del mundo. En double:
// las coordenadas rondan 32 000 (≈1,5 millones de píxeles) y en float la resta de la cámara
// perdería ~0,2 px, más que el paso de la cámara suave (1/escala de píxel).
inline double mundo_px(double x, double y) { return (x + y) * 24.0; }
inline double mundo_py(double x, double y) { return (y - x) * 12.0; }

// Los frames de caminar de los .spr ya traen el desplazamiento: el cuerpo avanza 0, ¼, ½, ¾ de
// celda respecto de la celda de ORIGEN (verificado en personajes y monstruos). Por eso:
//  - el sprite se dibuja en origen_paso() y la animación hace el resto (como el original);
//  - posicion_interpolada() es la posición continua, para la cámara suave, nombres y orden;
//  - fraccion_frame() es lo que el frame de caminar visible ya avanzó.
//
// Celda interpolada (continua) del objeto en el instante `ahora` (en double por lo mismo).
void posicion_interpolada(const Mundo& m, Slot s, u64 ahora, double& x, double& y);
bool moviendose(const Mundo& m, Slot s, u64 ahora);
// Celda desde la que se dibuja el sprite: la de origen mientras dura el paso, si no la actual.
void origen_paso(const Mundo& m, Slot s, u64 ahora, i32& x, i32& y);
// Fracción de celda que ya avanzó el frame de caminar visible (inicio del frame: 0, ¼, ½, ¾ con
// 4 frames iguales); 0 si no se mueve. Sin la acción en la tabla, 4 frames iguales.
double fraccion_frame(const Mundo& m, Slot s, u64 ahora);
// Dónde se ve el cuerpo: origen del paso + lo que avanzó el frame visible (a saltos de frame).
void posicion_visible(const Mundo& m, Slot s, u64 ahora, double& x, double& y);
// Para la animación: sigue "caminando" un cuarto de paso más, porque el próximo S_MoveCharPacket
// suele llegar unos ms tarde y la pose de reposo parpadearía entre pasos.
bool caminando(const Mundo& m, Slot s, u64 ahora);
// Duración de un paso: la animación de caminar del sprite (con el arma), como el servidor
// (spr_actions); × 3/4 rápido (haste), × 4/3 lento.
u16 duracion_paso(const Mundo& m, GfxId gfx, u8 arma, u8 velocidad);

}  // namespace lin
