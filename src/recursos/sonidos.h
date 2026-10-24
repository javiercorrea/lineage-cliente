// Definiciones de sonido del cliente original (Text.pak, texto en CP949 con comentarios ";"):
//  - music.def: música por zona. Primero "System Setting Begin ... SystemMenu Exit" con temas
//    fijos ("Login=0;", "Death=120;"...) y después zonas:
//      LOC  <mapa> [x1 y1 x2 y2] \ <mapa> ...   (varios rectángulos; sin coordenadas, todo el mapa)
//      MUSIC <n>                                  (sound/music<n>.mp3)
//      INTERVAL [segundos]                        (pausa entre repeticiones; vacío = sin pausa)
//    Vale la primera zona que contiene la posición, en el orden del archivo (los pueblos van antes
//    que el mapa que los rodea).
//  - envsound.def: sonidos de ambiente. Grupos "[NOMBRE:segundos] id id ... [/NOMBRE]" (un sonido
//    al azar del grupo cada tantos segundos) y zonas:
//      <LOC>...</LOC> (igual que en music.def), y por cada <TIME>DAY|NIGHT</TIME> (o sin TIME,
//      siempre) un <MAIN_SOUND>id</MAIN_SOUND> en bucle y <ONE_SOUND>GRUPO GRUPO</ONE_SOUND>.
// Los sonidos son sound/<id>.wav.
#pragma once
#include <string_view>
#include <vector>
#include "core/tipos.h"

namespace lin {

struct RectZona {
    u16 mapa;
    bool todo_el_mapa;
    i32 x1, y1, x2, y2;
};

struct ZonaMusica {
    u32 primer_rect, n_rects;  // en DefSonidos::rects
    i16 musica = -1;
    u16 intervalo_s = 0;
};

struct GrupoSonido {
    std::string_view nombre;  // apunta al texto cargado
    u16 intervalo_s = 0;
    u32 primero, cantidad;    // en DefSonidos::ids_grupos
};

struct AmbienteZona {
    u32 primer_rect, n_rects;
    u8 momento;  // 0 siempre, 1 de día, 2 de noche
    i32 principal = -1;           // sonido en bucle; -1 ninguno
    u32 primer_grupo, n_grupos;   // en DefSonidos::grupos_ambiente (índices de grupos)
};

struct DefSonidos {
    std::vector<RectZona> rects;
    std::vector<ZonaMusica> musica;
    std::vector<GrupoSonido> grupos;
    std::vector<u16> ids_grupos;
    std::vector<u32> grupos_ambiente;
    std::vector<AmbienteZona> ambiente;
    i16 musica_login = 0, musica_muerte = -1;

    // `music_def` y `envsound_def` deben vivir mientras se use la tabla (los nombres apuntan ahí).
    void cargar(std::string_view music_def, std::string_view envsound_def);
    // Zona de música en esa posición; nullptr si ninguna.
    const ZonaMusica* musica_en(u16 mapa, i32 x, i32 y) const;
    // Ambiente en esa posición y momento del día; nullptr si ninguno.
    const AmbienteZona* ambiente_en(u16 mapa, i32 x, i32 y, bool noche) const;

private:
    bool contiene(u32 primer, u32 n, u16 mapa, i32 x, i32 y) const;
};

}  // namespace lin
