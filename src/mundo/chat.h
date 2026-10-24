// Chat: registro de mensajes, globos sobre las cabezas y lo que escribe el jugador.
//
// - El registro es un anillo de líneas de tamaño fijo: agregar un mensaje no reserva memoria.
// - Los mensajes del servidor (S_ServerMessage) llegan como id + argumentos; el texto sale de
//   string-e.tbl (%0, %1 se reemplazan por los argumentos) y los "$N" de desc-e.tbl.
// - Los códigos de color "\fX" del texto se quitan; el primero decide el color de la línea.
// - Prefijos del original al escribir: ! grito, & global, $ comercio, @ clan, # grupo,
//   % alianza, * grupo de chat y "nombre texto para susurrar.
#pragma once
#include <span>
#include <string_view>
#include "proto/eventos.h"
#include "recursos/textos.h"

namespace lin {

constexpr size_t LARGO_LINEA_CHAT = 190;
constexpr u32 CAPACIDAD_CHAT = 256;
constexpr u32 CANTIDAD_GLOBOS = 16;
constexpr u64 MS_GLOBO = 5000;     // tiempo que el texto queda sobre la cabeza de quien habla
constexpr u8 CHAT_MENSAJE = 200;   // tipo de línea para los mensajes del servidor (no es de cable)

struct LineaChat {
    u32 color;
    u8 tipo;  // TipoChat o CHAT_MENSAJE
    u8 largo;
    char texto[LARGO_LINEA_CHAT];
};

struct GloboChat {
    ObjId id;
    u64 inicio;
    u32 linea;  // número absoluto de línea en el registro
};

struct Chat {
    LineaChat lineas[CAPACIDAD_CHAT];
    u32 total = 0;  // líneas agregadas desde el inicio (el anillo guarda las últimas CAPACIDAD_CHAT)
    GloboChat globos[CANTIDAD_GLOBOS] = {};

    u32 cantidad() const { return total < CAPACIDAD_CHAT ? total : CAPACIDAD_CHAT; }
    // i = 0 es la más reciente; i < cantidad().
    const LineaChat& reciente(u32 i) const { return lineas[(total - 1 - i) % CAPACIDAD_CHAT]; }
    std::string_view texto(const LineaChat& l) const { return {l.texto, l.largo}; }
    void agregar(std::string_view t, u32 color, u8 tipo);
    // Globo vigente del objeto (vacío si no habló en los últimos MS_GLOBO).
    std::string_view globo(ObjId id, u64 ahora) const;
    void limpiar() { total = 0; for (GloboChat& g : globos) g = {}; }
};

void aplicar_chat(Chat& c, const TablaTexto& mensajes, const TablaTexto& desc, std::span<const Evento> eventos, u64 ahora);

// Color de cada tipo de chat (0xAABBGGRR).
u32 color_chat(u8 tipo);

// Reemplaza los "$N" del texto por la entrada N de desc-e.tbl. Devuelve los bytes escritos.
size_t expandir_desc(const TablaTexto& desc, std::string_view t, char* destino, size_t capacidad);
// Arma el texto de un S_ServerMessage. `color` sale del primer código \fX, si lo hay.
size_t formatear_mensaje(const TablaTexto& mensajes, const TablaTexto& desc, const Evento& e, char* destino,
                         size_t capacidad, u32& color);

// Lo que escribió el jugador, ya separado según el prefijo.
struct EntradaChat {
    u8 tipo = CHAT_NORMAL;
    bool susurro = false;
    std::string_view destino;  // solo susurro
    std::string_view texto;
};
// false si no hay nada que enviar (línea vacía, prefijo sin texto, susurro sin destinatario).
bool interpretar_entrada(std::string_view linea, EntradaChat& salida);

}  // namespace lin
