// Audio sobre SDL3: un dispositivo y varios SDL_AudioStream ligados a él (SDL los mezcla).
//  - Música: sound/music<n>.mp3, decodificada de a poco con dr_mp3 (se mantiene ~0,5 s en cola),
//    con una pausa opcional entre repeticiones (INTERVAL de music.def).
//  - Efectos: sound/<id>.wav, en un caché que se carga al primer uso; se tocan en un grupo fijo de
//    streams (si están todos ocupados, se corta el más viejo).
//  - Ambiente: un sonido en bucle (MAIN_SOUND) en su propio stream.
// Sin dispositivo de audio todo sigue andando en silencio.
#pragma once
#include <string>
#include <vector>
#include "core/tipos.h"

struct SDL_AudioStream;

namespace lin {

class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // `carpeta_juego` termina en "/"; los archivos están en <carpeta>sound/.
    bool abrir(const std::string& carpeta_juego);
    void cerrar();
    bool abierto() const { return dispositivo_ != 0; }

    // Volúmenes de 0 a 100.
    void volumenes(int musica, int efectos);

    // Tema a tocar (-1 = silencio). Si es el que ya suena, no hace nada. `intervalo_s`: pausa
    // entre repeticiones.
    void musica(i32 n, u32 intervalo_s, u64 ahora_ms);
    i32 musica_actual() const { return musica_n_; }
    // Un efecto, con `ganancia` de 0 a 1 (además del volumen de efectos).
    void efecto(u32 id, float ganancia, u64 ahora_ms);
    // Sonido de ambiente en bucle (-1 = ninguno).
    void ambiente(i32 id);

    // Una vez por frame: alimenta la música y el bucle de ambiente.
    void actualizar(u64 ahora_ms);

    u32 efectos_tocados() const { return efectos_tocados_; }  // para pruebas y --perf

private:
    struct Wav;
    const Wav* wav(u32 id);
    void abrir_mp3(u64 ahora_ms);
    void cerrar_mp3();

    std::string carpeta_;
    u32 dispositivo_ = 0;
    SDL_AudioStream* stream_musica_ = nullptr;
    SDL_AudioStream* stream_ambiente_ = nullptr;
    static constexpr int N_EFECTOS = 16;
    SDL_AudioStream* streams_[N_EFECTOS] = {};
    u64 inicio_stream_[N_EFECTOS] = {};
    u32 id_stream_[N_EFECTOS] = {};
    std::vector<Wav*> wavs_;  // por id; nullptr sin cargar, Wav vacío si no existe

    void* mp3_ = nullptr;  // drmp3
    i32 musica_n_ = -1;
    u32 intervalo_s_ = 0;
    u64 reanudar_ms_ = 0;  // terminada: vuelve a empezar en este momento (0 = sonando)
    float pcm_[4096 * 2];

    i32 ambiente_id_ = -1;
    float vol_musica_ = 0.6f, vol_efectos_ = 0.8f;
    u32 efectos_tocados_ = 0;
};

}  // namespace lin
