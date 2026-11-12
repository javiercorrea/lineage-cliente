// Audio sobre SDL3: un dispositivo y varios SDL_AudioStream ligados a él (SDL los mezcla).
//  - Música: sound/music<n>.mp3, decodificada de a poco con dr_mp3 en el hilo de audio de SDL (el
//    callback del stream pide lo que falta), con una pausa opcional entre repeticiones (INTERVAL de
//    music.def). Alimentarla desde el frame cortaba el sonido cuando el bucle se detiene, como al
//    cambiar de aplicación (más de 1 s en macOS).
//  - Efectos: sound/<id>.wav, en un caché que se carga al primer uso; se tocan en un grupo fijo de
//    streams (si están todos ocupados, se corta el más viejo).
//  - Ambiente: un sonido en bucle (MAIN_SOUND) en su propio stream, también desde su callback.
//  - Los archivos (WAV y MP3) se abren en un hilo cargador: leerlos del disco en el frame daba tirones
//    (el primer golpe de cada sonido, el cambio de zona). Un efecto que todavía no está suena al
//    llegar si tarda menos de MS_EFECTO_TARDIO; si no, se pierde (ya no corresponde a la acción).
// Sin dispositivo de audio todo sigue andando en silencio.
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
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

    // Una vez por frame: vuelve a empezar la música después de la pausa entre repeticiones.
    void actualizar(u64 ahora_ms);

    u32 efectos_tocados() const { return efectos_tocados_; }  // pedidos (suenen o no): pruebas y perf
    u32 wavs_cargados() const { return wavs_cargados_; }      // los que ya trajo el cargador (perf)

private:
    struct Wav;
    // El WAV si ya está cargado; si no, lo pide al cargador y devuelve nullptr.
    const Wav* wav(u32 id);
    void abrir_mp3(u64 ahora_ms);
    void cerrar_mp3();
    void tocar(u32 id, const Wav* w, float ganancia, u64 ahora_ms);
    void poner_ambiente(const Wav* w);
    void bucle_cargador();
    void recoger_cargados(u64 ahora_ms);
    // Callbacks de los streams (hilo de audio, con el stream bloqueado).
    static void alimentar_musica(void* yo, SDL_AudioStream* s, int falta, int total);
    static void alimentar_ambiente(void* yo, SDL_AudioStream* s, int falta, int total);

    std::string carpeta_;
    u32 dispositivo_ = 0;
    SDL_AudioStream* stream_musica_ = nullptr;
    SDL_AudioStream* stream_ambiente_ = nullptr;
    static constexpr int N_EFECTOS = 16;
    SDL_AudioStream* streams_[N_EFECTOS] = {};
    u64 inicio_stream_[N_EFECTOS] = {};
    u32 id_stream_[N_EFECTOS] = {};
    std::vector<Wav*> wavs_;  // por id; nullptr sin cargar, Wav vacío si no existe (solo el hilo principal)
    std::vector<u8> wav_pedido_;  // por id: ya se pidió al cargador

    // Hilo cargador: pedidos y resultados, con mu_. El MP3 pedido es el número de tema (-1 ninguno).
    std::thread cargador_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool salir_ = false;
    std::vector<u32> pedidos_wav_;
    std::vector<std::pair<u32, Wav*>> wavs_listos_;
    i32 pedido_mp3_ = -1;
    i32 mp3_listo_n_ = -1;
    void* mp3_listo_ = nullptr;  // drmp3 abierto (o nullptr si no existe) para el tema mp3_listo_n_
    bool hay_mp3_listo_ = false;
    bool mp3_cargando_ = false;  // solo el hilo principal

    // Efectos pedidos cuyo WAV todavía no llegó.
    struct EfectoPendiente {
        u32 id;
        float ganancia;
        u64 pedido_ms;
    };
    static constexpr int N_PENDIENTES = 8;
    EfectoPendiente pendientes_[N_PENDIENTES] = {};
    int n_pendientes_ = 0;
    i32 ambiente_pendiente_ = -1;  // ambiente pedido cuyo WAV todavía no llegó

    void* mp3_ = nullptr;  // drmp3; se cambia con stream_musica_ bloqueado
    std::atomic<bool> mp3_termino_{false};  // lo marca el callback: el hilo principal lo cierra
    i32 musica_n_ = -1;
    u32 intervalo_s_ = 0;
    u64 reanudar_ms_ = 0;  // terminada: vuelve a empezar en este momento (0 = sonando)
    float pcm_[4096 * 2];

    i32 ambiente_id_ = -1;
    const Wav* ambiente_wav_ = nullptr;  // el del bucle; se cambia con stream_ambiente_ bloqueado
    float vol_musica_ = 0.6f, vol_efectos_ = 0.8f;
    u32 efectos_tocados_ = 0;
    u32 wavs_cargados_ = 0;
};

}  // namespace lin
