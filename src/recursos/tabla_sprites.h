// Tabla de acciones de sprites (equivalente a list.spr del cliente original).
//
// Esta instalación solo trae list.enx (cifrado, formato desconocido), así que se usa
// datos/sprite_table.txt de l1j-client (GPLv3). Registros separados por \0, uno por gfx:
//   "<subsprites>[=<gfx de archivos>]" y luego entradas "<n> ...":
//   - 100–114: atributos (101 sombra, 102 tipo, 104 atributo, ...). "110 N" fija la velocidad de
//     las acciones que siguen en el registro: un tick dura 40 × 24 / N ms (N = 24 por defecto,
//     40 ms), la misma fórmula que spr_actions del servidor (frame_count × 40 × 24 / frame_rate).
//   - otro n: acción n: "<n> <dir> <frames>" y frames × "<sub> <frame> <duración>[sufijo]"
//     con sufijos ! (impacto), [n / <n (sonido), ] ^ > { n (otros).
// Un frame se dibuja con el archivo "<gfx de archivos>-<sub + heading>.spr", frame <frame>.
//
// Diseño orientado a datos: todos los frames de todas las acciones van en un solo arreglo y
// una tabla hash (gfx, acción) -> rango.
#pragma once
#include <string>
#include <vector>
#include "core/mapa_hash.h"

namespace lin {

struct FrameAccion {
    u16 sub;       // sub-sprite base (se suma el heading si la acción tiene dirección)
    u16 frame;     // frame dentro del .spr
    u8 duracion;   // en ticks
    bool impacto;  // sufijo "!": el golpe alcanza al objetivo en este frame
    i16 sonido;    // -1 si no hay
};

struct RangoAccion {
    u32 primer;
    u16 cantidad;
    u8 direccional;
    u8 velocidad;  // "110 N" vigente (24 por defecto): un tick dura 960 / velocidad ms
};

// Tipos de gfx (atributo 102 de la tabla; verificado con los gfx de suelo de ítems de etc_items y
// weapons, los monstruos de npcs y los personajes). Los NPC del pueblo son 6 o 12.
enum TipoGfx : u8 { TIPO_PERSONAJE = 5, TIPO_ITEM = 9, TIPO_MONSTRUO = 10 };

// Acciones conocidas (ActionCodes de L1J). Caminar, atacar, recibir daño y quieto tienen una
// variante por arma: acción + código de arma (4 espada, 11 hacha, 20 arco...); morir no.
enum Accion : u8 { ACC_CAMINAR = 0, ACC_ATACAR = 1, ACC_DANADO = 2, ACC_QUIETO = 3, ACC_MORIR = 8 };

class TablaSprites {
public:
    bool cargar(const std::vector<u8>& texto);
    bool cargar_archivo(const std::string& ruta);

    const RangoAccion* accion(GfxId gfx, u8 accion) const;
    const FrameAccion& frame(u32 i) const { return frames_[i]; }
    GfxId gfx_archivos(GfxId gfx) const { return gfx < alias_.size() ? alias_[gfx] : gfx; }
    i16 sombra(GfxId gfx) const { return gfx < sombra_.size() ? sombra_[gfx] : -1; }
    // Tipo de gfx (atributo 102): TIPO_ITEM para los ítems del suelo, TIPO_MONSTRUO... 0 si no tiene.
    u8 tipo(GfxId gfx) const { return gfx < tipo_.size() ? tipo_[gfx] : 0; }
    // Atributo 104, bit 8: se dibuja sumando la luz al fondo (los 1921 gfx que lo tienen son
    // efectos de hechizos; su "fondo" oscuro no tapa).
    bool aditivo(GfxId gfx) const { return gfx < atributos_.size() && (atributos_[gfx] & 8); }
    // Atributo 109: gráfico que sigue a este al terminar (la explosión de Energy Bolt al llegar); 0 si no hay.
    GfxId siguiente(GfxId gfx) const { return gfx < siguiente_.size() ? siguiente_[gfx] : 0; }
    // Duración total de una acción en ms (suma de los frames × ms por tick); 0 si no existe. Para
    // caminar coincide con la velocidad de movimiento que usa el servidor (spr_actions de L1J).
    u32 duracion_ms(GfxId gfx, u8 accion) const;
    u32 ticks(const RangoAccion& r) const;  // suma de las duraciones de los frames (como frame_count del servidor)
    // Frame de la acción `transcurrido_ms` después de empezar (`termino`: ya pasó un ciclo). En
    // bucle, o detenida en el último frame si !bucle (morir). Con `ciclo_ms` > 0 la acción se
    // estira o acorta para durar eso (caminar = un paso).
    const FrameAccion& frame_en(const RangoAccion& r, u64 transcurrido_ms, bool& termino, u64 ciclo_ms = 0,
                                bool bucle = true) const;
    // ms desde el comienzo de la acción hasta su primer frame de impacto ("!"); si no tiene, la
    // mitad de la acción.
    u32 ms_hasta_impacto(GfxId gfx, u8 accion) const;
    size_t cantidad_gfx() const { return alias_.size(); }

private:
    std::vector<FrameAccion> frames_;
    std::vector<RangoAccion> rangos_;
    MapaHash indice_;              // (gfx << 8 | accion) -> índice en rangos_
    std::vector<GfxId> alias_;     // por gfx: gfx cuyos archivos .spr se usan
    std::vector<i16> sombra_;
    std::vector<u8> tipo_;
    std::vector<u16> atributos_;
    std::vector<GfxId> siguiente_;
};

}  // namespace lin
