// Entregable 10: música por zona, sonidos de ambiente y sonidos de las animaciones.
//  - Música: la del login (music.def "Login") fuera del juego; dentro, la de la zona donde está
//    el personaje (se recalcula cada medio segundo), y la de "Death" mientras está muerto.
//  - Ambiente: el MAIN_SOUND de la zona en bucle y, de cada grupo de ONE_SOUND, un sonido al azar
//    cada tantos segundos (entre la mitad y una vez y media del intervalo del grupo). De día o de
//    noche según la hora del juego (S_GameTime; noche fuera de 6:00–17:59, como L1GameTime).
//  - Animaciones: los frames de la tabla de sprites con sonido ("[n" o "<n") suenan al empezar a
//    dibujarse; más bajo cuanto más lejos del personaje (nada a 20 celdas).
#include <cstdlib>
#include "app/app.h"

namespace lin {

namespace {

constexpr i32 SONIDO_AGUA = 330;  // el ambiente bajo el agua (0x14a en el cliente original)

constexpr u64 MS_ZONA = 500;
constexpr double CELDAS_SILENCIO = 20;
// El valle escondido de esta versión (2005) es una copia del mapa 69, que es el que nombran
// music.def y envsound.def.
MapId mapa_de_sonido(MapId m) { return m == 2005 ? 69 : m; }

u64 al_azar(u64 intervalo_ms) { return intervalo_ms / 2 + static_cast<u64>(std::rand()) % (intervalo_ms + 1); }

}  // namespace

void cargar_sonidos(App& a) {
    a.rec.texto("music.def", a.texto_music_def);
    a.rec.texto("envsound.def", a.texto_envsound_def);
    a.sonidos.cargar({reinterpret_cast<const char*>(a.texto_music_def.data()), a.texto_music_def.size()},
                     {reinterpret_cast<const char*>(a.texto_envsound_def.data()), a.texto_envsound_def.size()});
    if (a.sonidos.musica.empty()) LIN_LOG("Aviso: no se encontró music.def (música por zona)");
    if (!a.audio.abrir(a.cfg.carpeta_juego)) LIN_LOG("Aviso: sin audio");
    aplicar_volumenes(a);
}

void aplicar_volumenes(App& a) {
    if (a.mudo) a.audio.volumenes(0, 0);
    else a.audio.volumenes(a.cfg.volumen_musica, a.cfg.volumen_efectos);
}

void actualizar_sonido(App& a, u64 ahora) {
    if (a.pantalla == Pantalla::LOGIN || a.pantalla == Pantalla::PERSONAJES || a.pantalla == Pantalla::CREAR) {
        a.audio.musica(a.sonidos.musica_login, 0, ahora);
        a.audio.ambiente(-1);
        a.ambiente = nullptr;
    } else if (a.pantalla == Pantalla::JUEGO && ahora >= a.t_zona_sonido + MS_ZONA) {
        a.t_zona_sonido = ahora;
        const TablaObjetos& o = a.mundo.objetos;
        Slot yo = o.buscar(a.mundo.jugador.id);
        if (yo != SIN_SLOT) {
            const MapId mapa = mapa_de_sonido(a.mundo.mapa);
            const i32 x = o.col<O_X>()[yo], y = o.col<O_Y>()[yo];
            if (o.col<O_ESTADO>()[yo] == ESTADO_MUERTO && a.sonidos.musica_muerte >= 0) {
                a.audio.musica(a.sonidos.musica_muerte, 0, ahora);
            } else if (const ZonaMusica* z = a.sonidos.musica_en(mapa, x, y)) {
                a.audio.musica(z->musica, z->intervalo_s, ahora);
            } else {
                a.audio.musica(-1, 0, ahora);
            }
            // Bajo el agua suena el agua en bucle (sonido 330, de sub_4891e0) en lugar del ambiente de la zona.
            const u32 hora = static_cast<u32>(hora_de_juego(a)) % 86400 / 3600;
            const bool agua = a.mundo.submarino;
            const AmbienteZona* amb = agua ? nullptr : a.sonidos.ambiente_en(mapa, x, y, hora < 6 || hora >= 18);
            if (amb != a.ambiente || agua != a.ambiente_agua) {
                a.ambiente = amb;
                a.ambiente_agua = agua;
                a.audio.ambiente(agua ? SONIDO_AGUA : amb ? amb->principal : -1);
                for (u32 i = 0; amb && i < amb->n_grupos && i < 8; ++i) {
                    const GrupoSonido& g = a.sonidos.grupos[a.sonidos.grupos_ambiente[amb->primer_grupo + i]];
                    a.proximo_ambiente[i] = ahora + al_azar(g.intervalo_s * 1000ull);
                }
            }
        }
    }
    // Sonidos sueltos del ambiente.
    if (a.pantalla == Pantalla::JUEGO && a.ambiente) {
        for (u32 i = 0; i < a.ambiente->n_grupos && i < 8; ++i) {
            if (ahora < a.proximo_ambiente[i]) continue;
            const GrupoSonido& g = a.sonidos.grupos[a.sonidos.grupos_ambiente[a.ambiente->primer_grupo + i]];
            if (g.cantidad) a.audio.efecto(a.sonidos.ids_grupos[g.primero + static_cast<u32>(std::rand()) % g.cantidad], 0.6f, ahora);
            a.proximo_ambiente[i] = ahora + al_azar(std::max<u64>(g.intervalo_s, 5) * 1000ull);
        }
    }
    a.audio.actualizar(ahora);
}

void sonido_de_frame(App& a, u32& ultimo, const FrameAccion& f, double x, double y, u64 ahora, bool sonar_primero) {
    const u32 indice = static_cast<u32>(&f - &a.sprites.frame(0)) + 1;
    if (indice == ultimo) return;
    const bool primero = ultimo == 0 && !sonar_primero;
    ultimo = indice;
    if (primero || f.sonido < 0) return;
    double d = 0;
    const TablaObjetos& o = a.mundo.objetos;
    if (Slot yo = o.buscar(a.mundo.jugador.id); yo != SIN_SLOT)
        d = std::max(std::abs(x - o.col<O_X>()[yo]), std::abs(y - o.col<O_Y>()[yo]));
    const float ganancia = static_cast<float>(1.0 - d / CELDAS_SILENCIO);
    a.audio.efecto(static_cast<u32>(f.sonido), ganancia, ahora);
}

}  // namespace lin
