#include "mundo/luz.h"
#include <algorithm>
#include <cstddef>

namespace lin {

namespace {

struct Rango {
    MapId lo, hi;
};

// Anexo A de docs/analisis/PLAN_LUZ_AGUA.md: mapas cerrados del original (0x6ddae8, 297 entradas).
constexpr Rango MAPAS_CERRADOS[] = {
    {1, 3},     {7, 28},    {30, 41},   {43, 56},    {59, 64},    {72, 82},    {85, 87},   {101, 237},
    {240, 243}, {248, 251}, {301, 301}, {304, 310},  {320, 320},  {330, 330},  {400, 400}, {401, 401},
    {410, 410}, {420, 420}, {430, 430}, {441, 444},  {450, 457},  {460, 467},  {470, 478}, {481, 481},
    {490, 496}, {522, 524}, {530, 536}, {541, 543},  {666, 666},  {781, 782},  {1001, 1005},
};

// Niveles fijos de algunos mapas (valor final, ya con el +1 del original). Los de 5140 en adelante
// y el 1000 están fuera de la lista cerrada; el original los trata igual, sin hora.
struct NivelFijo {
    MapId lo, hi;
    u8 nivel;
};
constexpr NivelFijo NIVELES_FIJOS[] = {
    {72, 74, 5},     {310, 310, 5},   {330, 330, 5},   {400, 401, 5},  {522, 524, 5},
    {307, 309, 7},   {320, 320, 7},   {75, 77, 12},    {430, 430, 14},
    {5140, 5142, 5}, {5143, 5145, 15}, {1000, 1000, 7},
};

// Nivel de los mapas cerrados sin nivel fijo. El binario de 2009 dice 3, pero la captura de
// mazmorra real (ss01) cuadra con 1: ver D2 del plan.
constexpr u8 NIVEL_CERRADO = 1;

constexpr int MINUTOS_DIA = 1440;
constexpr int VENTANA_TRANSICION = 40;  // minutos antes del amanecer y después del atardecer

template <typename T, size_t N>
bool en_rangos(MapId m, const T (&rangos)[N]) {
    for (const T& r : rangos)
        if (m >= r.lo && m <= r.hi) return true;
    return false;
}

// Amanecer y atardecer en minutos del día según el día del año (fórmula del binario, sub_4b2cc0).
void horas_de_luz(int dia_anio, int& amanecer, int& atardecer) {
    if (dia_anio <= 172) {
        amanecer = 452 - dia_anio * 192 / 172;
        atardecer = 986 + dia_anio * 197 / 172;
    } else {
        amanecer = 260 + (dia_anio - 173);
        atardecer = 1183 + (173 - dia_anio) * 197 / 192;
    }
}

}  // namespace

bool mapa_oscuro(MapId m) { return m >= 0x4000 || en_rangos(m, MAPAS_CERRADOS); }

int nivel_ambiente(MapId m, int minuto, int dia_anio, bool lluvia) {
    minuto = std::clamp(minuto, 0, MINUTOS_DIA - 1);
    for (const NivelFijo& f : NIVELES_FIJOS)
        if (m >= f.lo && m <= f.hi) return f.nivel;
    if (mapa_oscuro(m)) return NIVEL_CERRADO;

    // Al aire libre: 15 de día, 2 de noche, con rampa de 40 minutos a cada lado.
    int amanecer, atardecer;
    horas_de_luz(std::clamp(dia_anio, 1, 366), amanecer, atardecer);
    int t;
    if (minuto > amanecer - VENTANA_TRANSICION && minuto < amanecer)
        t = (minuto - amanecer + VENTANA_TRANSICION) * 15 / VENTANA_TRANSICION + 1;
    else if (minuto >= amanecer && minuto <= atardecer)
        t = 15;
    else if (minuto > atardecer && minuto < atardecer + VENTANA_TRANSICION)
        t = (atardecer - minuto + VENTANA_TRANSICION) * 15 / VENTANA_TRANSICION + 1;
    else
        t = 2;
    int nivel = std::clamp(t * 2 / 3 + 5, 5, 15);
    if (lluvia && nivel >= 10) --nivel;
    if (nivel < 15) ++nivel;  // el original suma 1 fuera del día pleno
    return nivel;
}

int nivel_con_luz_hechizo(int nivel) { return std::min(nivel + 5, 15); }

int brillo_lut(int nivel, int mascara) {
    // Brillo ambiente de cada nivel (columna 0 de la LUT).
    constexpr int AMBIENTE[16] = {0, 4, 10, 11, 13, 14, 16, 18, 19, 21, 23, 24, 26, 28, 29, 31};
    const int a = AMBIENTE[std::clamp(nivel, 0, 15)];
    const int m = std::clamp(mascara, 0, 255) >> 3;
    return a + (31 - a) * m / 31;
}

bool tinte_mapa(MapId m, u8& r, u8& g, u8& b) {
    // Mapas con tinte propio (sub_467653): 430 y 601.
    if (m == 430) { r = 203; g = 170; b = 218; return true; }
    if (m == 601) { r = 253; g = 216; b = 225; return true; }
    return false;
}

Vision modo_vision(u8 clase, bool opcion_activa, int nivel, bool submarino, bool ceguera) {
    if (!opcion_activa || submarino || ceguera || nivel >= 15) return Vision::NINGUNA;
    if (clase == 2) return Vision::ELFO;
    if (clase == 4) return Vision::ELFO_OSCURO;
    return Vision::NINGUNA;
}

}  // namespace lin
