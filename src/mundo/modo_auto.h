// Modo auto programable (docs/analisis/PLAN_MODO_AUTO.md), fase 1: el intérprete del programa (texto en
// español o inglés -> Programa) y el evaluador (qué regla, auto-hechizo o tecla corre ahora). Sin estado
// de la app, para los tests: el entorno lo arma quien lo usa (app/), con los nombres ya normalizados.
#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "core/tipos.h"

namespace lin {

// Minúsculas, sin tildes (á é í ó ú ü ñ) y sin espacios ni guiones: "Turn Undead" = "turnundead".
std::string normalizar_nombre(std::string_view s);

// ---- Condiciones ----
enum class TipoCond : u8 {
    HP, MP,              // propios: valor en % o absoluto
    SEGURO,              // ningún monstruo a `radio` o menos
    SIN_JUGADORES,       // ningún otro personaje a `radio` o menos
    MONSTRUOS,           // cantidad de monstruos a `radio` o menos, comparada con `valor`
    ATACANDO,            // hay un objetivo de ataque
    SIN_POCIONES,        // no queda ninguna poción de curación
    SIN_ITEM,            // no queda el ítem `nombre`
    TIENE,               // el efecto `nombre` está activo
    HP_DE,               // la vida (en %) del personaje `nombre`
    HP_OBJETIVO,         // la vida (en %) del objetivo
    EN_MAPA,             // el mapa actual es `valor`
};
enum class Comparacion : u8 { MENOR, MENOR_IGUAL, MAYOR, MAYOR_IGUAL, IGUAL };
struct Condicion {
    TipoCond tipo = TipoCond::HP;
    Comparacion comp = Comparacion::MENOR;
    i32 valor = 0;
    bool porcentaje = false;
    i32 radio = 0;
    bool negada = false;
    std::string nombre;  // normalizado
};
// Forma normal disyuntiva: se cumple si se cumple algún grupo, y un grupo si se cumplen todas sus
// condiciones ("y" liga más que "o"). Vacía: siempre verdadera.
struct Expresion {
    std::vector<std::vector<Condicion>> o;
    bool vacia() const { return o.empty(); }
};

// ---- Acciones ----
enum class TipoAccion : u8 {
    ATACAR, LANZAR, USAR, EQUIPAR, DETENER, ESPERAR, TERMINAR, IR_A,
    PAUSAR, SIGUIENTE_PUNTO, REGLA,  // de control: solo en las teclas
};
enum class Objetivo : u8 { NINGUNO, YO, OBJETIVO, MAS_CERCANO, NOMBRE, ALIADO_MAS_HERIDO };
struct AccionAuto {
    TipoAccion tipo = TipoAccion::DETENER;
    std::string nombre;          // hechizo, ítem o arma (normalizado)
    Objetivo objetivo = Objetivo::NINGUNO;
    std::string objetivo_nombre; // con Objetivo::NOMBRE (normalizado)
    i32 x = 0, y = 0;            // IR_A
    i32 n = 0;                   // REGLA: el número (desde 1)
};

// ---- El programa ----
enum class TipoMovimiento : u8 { QUIETO, RECORRER, PUNTOS, SEGUIR };
struct Movimiento {
    TipoMovimiento tipo = TipoMovimiento::QUIETO;
    std::vector<std::pair<i32, i32>> puntos;
    bool una_vez = false;
    std::string seguir;  // normalizado
    i32 distancia = 2;
};
enum class ModoPociones : u8 { SIEMPRE, ATACANDO, NUNCA };
struct AutoHechizo {
    std::string nombre;  // normalizado: hechizo o poción de POCIONES_AUTO
    Expresion si;
};
struct Regla {
    bool es_auto = false;              // la línea de auto-hechizos, en su lugar de la prioridad
    std::vector<AutoHechizo> auto_hechizos;
    Expresion si;
    std::vector<AccionAuto> acciones;
    Expresion hasta;
    bool con_hasta = false;
    u64 cada_ms = 0;
    int linea = 0;
    bool termina() const;  // alguna acción es TERMINAR: puede interrumpir una tecla sostenida
};
struct Tecla {
    std::string tecla;  // normalizada: "ctrl+1", "f5", "shift+a"
    std::vector<AccionAuto> acciones;
    Expresion hasta;
    bool con_hasta = false;
    int linea = 0;
};
struct Programa {
    std::string nombre;
    i32 mapa = -1;               // -1: cualquiera
    std::string mapa_nombre;     // "mapa "Tower of Insolence 51F"": lo resuelve la app
    std::string arma;            // normalizado; vacío: ninguna
    ModoPociones pociones = ModoPociones::SIEMPRE;
    Movimiento movimiento;
    std::vector<Tecla> teclas;
    std::vector<Regla> reglas;
};
struct ErrorPrograma {
    int linea = 0;
    std::string mensaje;  // en inglés, como el resto de la interfaz
};
// Lee el programa entero; las líneas con error se saltean (el resto se carga igual).
Programa leer_programa(std::string_view texto, std::vector<ErrorPrograma>& errores);
// Hechizos y auto-hechizos por nombre: lo que la tabla del cliente no conoce es un error. Un auto-hechizo
// también puede ser una poción de POCIONES_AUTO.
void validar_programa(const Programa& p, const std::function<bool(std::string_view)>& existe_hechizo,
                      std::vector<ErrorPrograma>& errores);

// ---- El entorno (lo arma la app cada frame; nombres normalizados) ----
struct PersonajeVisto {
    std::string nombre;
    i32 distancia = 0;  // en celdas (la mayor de |dx| y |dy|)
    i32 hp_pct = -1;    // -1: desconocida (fuera del grupo)
};
struct EntornoAuto {
    i32 hp = 0, hp_max = 0, mp = 0, mp_max = 0;
    i32 mapa = 0;
    bool atacando = false;
    bool hay_pociones = true;          // de curación
    i32 objetivo_hp_pct = -1;
    std::vector<i32> monstruos;        // distancia de cada monstruo a la vista
    std::vector<PersonajeVisto> personajes;  // otros jugadores a la vista
    std::vector<std::string> items;    // los que hay en el inventario
    std::vector<std::string> efectos;  // los activos
    std::vector<std::string> renovar;  // auto-hechizos cuyo efecto falta o termina (elegir_autobuff)
};
bool cumple(const Expresion& e, const EntornoAuto& en);

// ---- El evaluador ----
enum class OrigenDecision : u8 { NADA, REGLA, AUTO_HECHIZO, TECLA, MOVER };
struct Decision {
    OrigenDecision origen = OrigenDecision::NADA;
    int indice = -1;                       // la regla o la tecla
    const std::vector<AccionAuto>* acciones = nullptr;
    std::string auto_hechizo;              // AUTO_HECHIZO: cuál renovar
    bool continua = false;                 // la misma regla con "hasta", que sigue sostenida
};
struct EstadoAuto {
    bool pausado = false, terminado = false;
    int sostenida = -1;          // regla con "hasta" que tiene el control
    int tecla_sostenida = -1;    // tecla con "hasta" que tiene el control
    std::vector<u64> ultima_ms;  // por regla: cuándo corrió por última vez (para "cada")
    size_t punto = 0;            // el punto de "moverse puntos" al que se va
};
// Una vez por frame. Pausado, terminado o fuera del mapa del programa: NADA. Si no, la primera regla que se
// cumple (de arriba hacia abajo), salvo una sostenida con "hasta": sigue mientras no se cumpla su salida y
// ninguna de más arriba corra. Sin reglas: MOVER (o NADA si el movimiento es quieto).
Decision decidir(const Programa& p, EstadoAuto& est, const EntornoAuto& en, u64 ahora);
// "moverse puntos": el punto al que hay que ir desde (x, y). Al llegar (a 1 celda o menos) pasa al
// siguiente, en bucle (o, con "una vez", se acaba). nullptr si no hay puntos o ya se acabaron.
const std::pair<i32, i32>* punto_actual(const Movimiento& m, EstadoAuto& est, i32 x, i32 y);
// Una tecla (normalizada). Las de control (pausar, terminar, siguiente punto) cambian el estado y devuelven
// NADA; "regla N" devuelve esa regla. Una tecla que el programa no define: NADA, con `usada` en false.
Decision pulsar_tecla(const Programa& p, EstadoAuto& est, std::string_view tecla, bool& usada);

}  // namespace lin
