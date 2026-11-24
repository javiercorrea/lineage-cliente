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
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "core/mapa_hash.h"
#include "recursos/modo_mezcla.h"

namespace lin {

struct FrameAccion {
    u16 sub;       // sub-sprite base (se suma el heading si la acción tiene dirección)
    u16 frame;     // frame dentro del .spr
    u8 duracion;   // en ticks
    bool impacto;  // sufijo "!": el golpe alcanza al objetivo en este frame
    i16 sonido;    // primer id de sonido, -1 si no hay (sufijos "<N", "[N", "^N"; hasta 3 por cuadro, ver agregar_sonido)
    i16 sonido_mas[2];  // segundo y tercer id de sonido (de los hasta 3 se elige uno al azar cada vez que se muestra el cuadro)
    u8 modo_sonido[3];  // per id: ModoSonido
    u8 n_sonidos;
    u16 efecto;    // sufijo "]N": al mostrarse este frame nace el gráfico N (polvo, chispas...); 0 = ninguno
};

// Sufijos de sonido de los cuadros (S3EP1U): "<N" puede superponerse consigo mismo, "[N" solo empieza si no está sonando, "^N" se
// escala según la distancia a quien escucha.
enum ModoSonido : u8 { SONIDO_SOLAPA = 0, SONIDO_UNICO = 1, SONIDO_DISTANCIA = 2 };

inline void agregar_sonido(FrameAccion& f, ModoSonido modo, int id) {
    if (f.n_sonidos >= 3) return;
    if (f.n_sonidos == 0) f.sonido = static_cast<i16>(id);
    else f.sonido_mas[f.n_sonidos - 1] = static_cast<i16>(id);
    f.modo_sonido[f.n_sonidos++] = modo;
}

struct RangoAccion {
    u32 primer;
    u16 cantidad;
    u8 direccional;
    u8 velocidad;  // "110 N" vigente (24 por defecto): un tick dura 960 / velocidad ms
};

// Tipos de gfx (atributo 102 de la tabla; verificado con los gfx de suelo de ítems de etc_items y
// weapons, los monstruos de npcs y los personajes). Los NPC del pueblo son 6 o 12.
enum TipoGfx : u8 { TIPO_CARTEL = 4, TIPO_PERSONAJE = 5, TIPO_ITEM = 9, TIPO_MONSTRUO = 10 };

// Acciones conocidas (ActionCodes de L1J). Caminar, atacar, recibir daño y quieto tienen una
// variante por arma: acción + código de arma (4 espada, 11 hacha, 20 arco...); morir no.
enum Accion : u8 { ACC_CAMINAR = 0, ACC_ATACAR = 1, ACC_DANADO = 2, ACC_QUIETO = 3, ACC_MORIR = 8 };

// Mueve al final la entrada inicial de una fila de muerte (el último cuadro del archivo del sprite); ver tabla_sprites.cpp.
void poner_cadaver_al_final(std::vector<FrameAccion>& frames, const RangoAccion& r);

// Resultado de fusionar list.spz en la tabla.
struct ResumenLista {
    u32 nuevos = 0;        // gfx con acciones que no tenían fila en sprite_table.txt
    u32 reemplazados = 0;  // gfx que ya estaban en sprite_table.txt: se conservan, las filas de list.spz se ignoran
    u32 omitidos = 0;      // entradas ya sin acciones, o sin ningún archivo, que se dejan como están
    u32 atributos = 0;     // gfx nuevos sin acciones (objetos del suelo, objetos): solo tipo, sombra, alias...
    u32 sin_archivo = 0;   // gfx nuevos cuyo archivo de sprite (alias) falta en nuestros paks: redirigidos o descartados
    u32 recortados = 0;    // gfx nuevos con cuadros (o acciones enteras) descartados porque a nuestros paks les faltan esas imágenes
};

// Rectángulo del atributo 100: x en medias celdas (el exe avanza de a 2), y en celdas, relativo a la celda del objeto.
struct RectTransparencia {
    i16 x1, y1, x2, y2;
};

// Cantidad de imágenes del archivo de sprite "<gfx>-<sub>.spr" de los paks cargados; 0 si el archivo no está.
using ContadorFrames = std::function<u32(GfxId gfx, u16 sub)>;

class TablaSprites {
public:
    bool cargar(const std::vector<u8>& texto);
    bool cargar_archivo(const std::string& ruta);
    // Fusiona un list.spz en texto plano (ver lista_sprites.cpp): sus gfx reemplazan o se suman a los cargados.
    // Llamar después de cargar(). Devuelve false si el texto no tiene ninguna entrada usable.
    // Con `contador`, las filas se revisan contra nuestros paks de sprites (list.spz viene de un cliente más nuevo): los cuadros
    // que apuntan a imágenes que no tenemos se descartan, un alias sin archivos vuelve al archivo propio del gfx, y
    // las entradas sin ningún archivo dibujable se dejan fuera (el gfx conserva el comportamiento sin fila).
    bool fusionar_lista(std::string_view texto, ResumenLista& resumen, const ContadorFrames* contador = nullptr);
    bool fusionar_lista_archivo(const std::string& ruta, ResumenLista& resumen,
                                const ContadorFrames* contador = nullptr);
    // gfx con al menos una fila de acción en sprite_table.txt (antes de cualquier fusión).
    u32 gfx_de_tabla() const { return gfx_de_tabla_; }

    const RangoAccion* accion(GfxId gfx, u8 accion) const;
    const FrameAccion& frame(u32 i) const { return frames_[i]; }
    // Subsprite para un gfx sin filas en la tabla: los archivos van de a 8 rumbos por grupo, el grupo 0
    // primero; un rumbo fuera de 0-7 vuelve al sub 0.
    static u16 sub_sin_fila(u8 dir) { return dir < 8 ? dir : 0; }
    GfxId gfx_archivos(GfxId gfx) const { return gfx < alias_.size() ? alias_[gfx] : gfx; }
    i16 sombra(GfxId gfx) const { return gfx < sombra_.size() ? sombra_[gfx] : -1; }
    // Tipo de gfx (atributo 102): TIPO_ITEM para los ítems del suelo, TIPO_MONSTRUO... 0 si no tiene.
    u8 tipo(GfxId gfx) const { return gfx < tipo_.size() ? tipo_[gfx] : 0; }
    // Atributo 104 -> modo de mezcla (screen para los efectos de hechizos y las llamas, multiplicar para los discos de las antorchas...).
    ModoMezcla modo(GfxId gfx) const { return gfx < atributos_.size() ? modo_de_atributo(atributos_[gfx]) : ModoMezcla::NORMAL; }
    // Atributo 111 (zancada): la caminata son dos medias zancadas, la acción 0 y la acción 4 (cada una empieza en la
    // pose de la otra), reproducidas en pasos alternados. Valakas, Antharas, los bugbears de carrera, los perros...
    bool zancada(GfxId gfx) const { return gfx < zancada_.size() && zancada_[gfx]; }
    // Acción de caminata para un paso de un sprite con zancada (S3EP1U 0x4c6ff0: un contador de pasos por objeto elige entre
    // {0, 4}, sea cual sea el estado del arma): 0 en los pasos pares, 4 en los impares si el sprite la tiene.
    u8 accion_caminar(GfxId gfx, u8 paso) const { return paso & 1 && accion(gfx, 4) ? 4 : 0; }
    // Atributo 105 (ropa): gfx dibujados sobre el cuerpo, después de él y en el orden de la lista, con el subsprite y el
    // cuadro del cuerpo y su propio atributo 104 (S3EP1U 0x4cbc50 / 0x4cbdb0). Valakas -> 2003.
    std::span<const GfxId> ropa(GfxId gfx) const {
        return gfx < ropa_.size() ? std::span<const GfxId>(ropa_[gfx]) : std::span<const GfxId>{};
    }
    // Atributo 109 tipo 2 ("109.effect(2 x)"): un efecto creado por S_UseAttackSkill que mantiene la luz al máximo desde su 3.er cuadro.
    bool flash_de_luz(GfxId gfx) const { return flash_.buscar(gfx) != nullptr; }
    // Atributo 113 (efecto de transformación, S3EP1U 0x488f60): gfx del efecto que aparece en un objeto que se transforma en este gfx; 0 si no hay.
    GfxId morfo(GfxId gfx) const {
        const u32* i = morfo_.buscar(gfx);
        return i ? static_cast<GfxId>(*i) : 0;
    }
    // Atributo 100 (disparador de transparencia, S3EP1U): mientras el jugador local está dentro de cualquier rectángulo del objeto
    // (dx, dy = celda del jugador menos celda del objeto) el objeto se dibuja al 50%. True si el gfx no tiene ninguno registrado.
    bool dentro_transparencia(GfxId gfx, int dx, int dy) const {
        const u32* i = transp_.buscar(gfx);
        if (!i) return false;
        for (const RectTransparencia& r : rects_transp_[*i])
            if (dx >= (r.x1 >> 1) && dx <= (r.x2 >> 1) && dy >= r.y1 && dy <= r.y2) return true;
        return false;
    }
    bool tiene_transparencia(GfxId gfx) const { return transp_.buscar(gfx) != nullptr; }
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
    // ms from the action start to its "!" frame (frame length n * 1000 / rate, exe 0x440ba8); SIN_IMPACTO if the action
    // is missing or has no "!" (the exe then never flinches the target).
    static constexpr u32 SIN_IMPACTO = 0xFFFFFFFFu;
    u32 ms_hasta_impacto(GfxId gfx, u8 accion) const;
    size_t cantidad_gfx() const { return alias_.size(); }

private:
    u32 gfx_de_tabla_ = 0;
    std::vector<FrameAccion> frames_;
    std::vector<bool> zancada_;
    std::vector<std::vector<GfxId>> ropa_;
    std::vector<RangoAccion> rangos_;
    MapaHash indice_;              // (gfx << 8 | accion) -> índice en rangos_
    std::vector<GfxId> alias_;     // por gfx: gfx cuyos archivos .spr se usan
    std::vector<i16> sombra_;
    std::vector<u8> tipo_;
    std::vector<u16> atributos_;
    std::vector<GfxId> siguiente_;
    MapaHash transp_;  // gfx -> índice en rects_transp_ (atributo 100), solo los gfx que tienen rectángulos
    std::vector<std::vector<RectTransparencia>> rects_transp_;
    void poner_transparencia(GfxId gfx, std::vector<RectTransparencia> r) {
        if (r.empty()) return;
        transp_.poner(gfx, static_cast<u32>(rects_transp_.size()));
        rects_transp_.push_back(std::move(r));
    }
    MapaHash flash_;  // gfx -> 1 para los sprites cuyo atributo 109 es de tipo 2 (efectos de destello, RE_ROUND4 sección 4)
    MapaHash morfo_;  // gfx -> gfx del efecto de transformación (atributo 113), solo los gfx que tienen uno
};

}  // namespace lin
