#include "audio/audio.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO_DEFAULT_ALLOCATOR_WARNING
#include "dr_mp3.h"

namespace lin {

namespace {

constexpr SDL_AudioSpec SALIDA = {SDL_AUDIO_F32, 2, 44100};
constexpr u64 MS_MISMO_EFECTO = 60;  // el mismo efecto dentro de este tiempo no se repite (golpes en masa)
constexpr u64 MS_EFECTO_TARDIO = 250;  // un efecto cuyo WAV tarda más en cargarse ya no se toca
constexpr u32 MAX_ID_WAV = 20000;

drmp3* mp3(void* p) { return static_cast<drmp3*>(p); }

void liberar_mp3(void* p) {
    if (!p) return;
    drmp3_uninit(mp3(p));
    delete mp3(p);
}

}  // namespace

struct Audio::Wav {
    SDL_AudioSpec spec{};
    u8* datos = nullptr;  // de SDL_LoadWAV
    u32 bytes = 0;
};

Audio::Audio() = default;
Audio::~Audio() { cerrar(); }

bool Audio::abrir(const std::string& carpeta_juego) {
    carpeta_ = carpeta_juego + "sound/";
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "audio: %s\n", SDL_GetError());
        return false;
    }
    dispositivo_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &SALIDA);
    if (!dispositivo_) {
        std::fprintf(stderr, "audio: sin dispositivo (%s)\n", SDL_GetError());
        return false;
    }
    stream_musica_ = SDL_CreateAudioStream(&SALIDA, &SALIDA);
    stream_ambiente_ = SDL_CreateAudioStream(&SALIDA, &SALIDA);
    SDL_SetAudioStreamGetCallback(stream_musica_, alimentar_musica, this);
    SDL_SetAudioStreamGetCallback(stream_ambiente_, alimentar_ambiente, this);
    SDL_BindAudioStream(dispositivo_, stream_musica_);
    SDL_BindAudioStream(dispositivo_, stream_ambiente_);
    for (SDL_AudioStream*& s : streams_) {
        s = SDL_CreateAudioStream(&SALIDA, &SALIDA);
        SDL_BindAudioStream(dispositivo_, s);
    }
    volumenes(static_cast<int>(vol_musica_ * 100), static_cast<int>(vol_efectos_ * 100));
    SDL_ResumeAudioDevice(dispositivo_);
    pedidos_wav_.reserve(64);
    wavs_listos_.reserve(64);
    salir_ = false;
    cargador_ = std::thread(&Audio::bucle_cargador, this);
    return true;
}

void Audio::cerrar() {
    if (cargador_.joinable()) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            salir_ = true;
        }
        cv_.notify_one();
        cargador_.join();
    }
    // Lo que el cargador dejó sin recoger.
    for (auto& [id, w] : wavs_listos_) {
        SDL_free(w->datos);
        delete w;
    }
    wavs_listos_.clear();
    pedidos_wav_.clear();
    if (hay_mp3_listo_) liberar_mp3(mp3_listo_);
    mp3_listo_ = nullptr;
    hay_mp3_listo_ = false;
    mp3_cargando_ = false;
    n_pendientes_ = 0;
    ambiente_pendiente_ = -1;

    if (dispositivo_) SDL_CloseAudioDevice(dispositivo_);  // primero: ya no corren los callbacks
    cerrar_mp3();
    if (dispositivo_) {
        for (SDL_AudioStream*& s : streams_) {
            SDL_DestroyAudioStream(s);
            s = nullptr;
        }
        SDL_DestroyAudioStream(stream_musica_);
        SDL_DestroyAudioStream(stream_ambiente_);
        stream_musica_ = stream_ambiente_ = nullptr;
        dispositivo_ = 0;
    }
    for (Wav*& w : wavs_) {
        if (w) SDL_free(w->datos);
        delete w;
        w = nullptr;
    }
    wav_pedido_.clear();
}

void Audio::volumenes(int musica, int efectos) {
    vol_musica_ = std::clamp(musica, 0, 100) / 100.0f;
    vol_efectos_ = std::clamp(efectos, 0, 100) / 100.0f;
    if (!dispositivo_) return;
    SDL_SetAudioStreamGain(stream_musica_, vol_musica_);
    SDL_SetAudioStreamGain(stream_ambiente_, vol_efectos_ * 0.7f);
}

// Hilo cargador: abre los WAV pedidos y el MP3 pedido, y deja el resultado para recoger_cargados.
void Audio::bucle_cargador() {
    std::vector<u32> lote;
    for (;;) {
        i32 tema = -1;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return salir_ || !pedidos_wav_.empty() || pedido_mp3_ >= 0; });
            if (salir_) return;
            lote.swap(pedidos_wav_);
            tema = pedido_mp3_;
            pedido_mp3_ = -1;
        }
        if (tema >= 0) {
            char nombre[32];
            std::snprintf(nombre, sizeof nombre, "music%d.mp3", tema);
            auto* m = new drmp3;
            // Algunos archivos empiezan con mayúscula (Music1012.mp3): en macOS el sistema de archivos no
            // las distingue. MULTIPLATAFORMA: en Linux, buscar sin distinguir mayúsculas.
            if (!drmp3_init_file(m, (carpeta_ + nombre).c_str(), nullptr)) {
                delete m;
                m = nullptr;
            }
            std::lock_guard<std::mutex> lk(mu_);
            if (hay_mp3_listo_) liberar_mp3(mp3_listo_);  // uno anterior sin recoger: ya no se quiere
            mp3_listo_ = m;
            mp3_listo_n_ = tema;
            hay_mp3_listo_ = true;
        }
        for (u32 id : lote) {
            Wav* w = new Wav;
            char ruta[64];
            std::snprintf(ruta, sizeof ruta, "%u.wav", id);
            if (!SDL_LoadWAV((carpeta_ + ruta).c_str(), &w->spec, &w->datos, &w->bytes)) w->bytes = 0;
            std::lock_guard<std::mutex> lk(mu_);
            wavs_listos_.emplace_back(id, w);
        }
        lote.clear();
    }
}

const Audio::Wav* Audio::wav(u32 id) {
    if (id >= MAX_ID_WAV || !cargador_.joinable()) return nullptr;
    if (id < wavs_.size() && wavs_[id]) return wavs_[id]->bytes ? wavs_[id] : nullptr;
    if (id >= wav_pedido_.size()) wav_pedido_.resize(id + 1, 0);
    if (!wav_pedido_[id]) {
        wav_pedido_[id] = 1;
        {
            std::lock_guard<std::mutex> lk(mu_);
            pedidos_wav_.push_back(id);
        }
        cv_.notify_one();
    }
    return nullptr;
}

void Audio::recoger_cargados(u64 ahora_ms) {
    void* m = nullptr;
    i32 n = -1;
    bool hay_mp3 = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [id, w] : wavs_listos_) {
            if (id >= wavs_.size()) wavs_.resize(id + 1, nullptr);
            wavs_[id] = w;
            ++wavs_cargados_;
        }
        wavs_listos_.clear();
        if (hay_mp3_listo_) {
            m = mp3_listo_;
            n = mp3_listo_n_;
            hay_mp3 = true;
            mp3_listo_ = nullptr;
            hay_mp3_listo_ = false;
        }
    }
    // Efectos que esperaban su WAV: los que llegaron a tiempo suenan; los viejos se descartan.
    int k = 0;
    for (int i = 0; i < n_pendientes_; ++i) {
        const EfectoPendiente& p = pendientes_[i];
        if (ahora_ms - p.pedido_ms > MS_EFECTO_TARDIO) continue;
        const bool llego = p.id < wavs_.size() && wavs_[p.id];
        if (!llego) {
            pendientes_[k++] = p;
        } else if (const Wav* w = wavs_[p.id]; w->bytes) {
            tocar(p.id, w, p.ganancia, p.pedido_ms);
        }
    }
    n_pendientes_ = k;
    if (ambiente_pendiente_ >= 0 && ambiente_pendiente_ == ambiente_id_) {
        const u32 id = static_cast<u32>(ambiente_pendiente_);
        if (id < wavs_.size() && wavs_[id]) {
            poner_ambiente(wavs_[id]->bytes ? wavs_[id] : nullptr);
            ambiente_pendiente_ = -1;
        }
    }
    if (hay_mp3) {
        if (n != musica_n_ || !mp3_cargando_) {
            liberar_mp3(m);  // otro tema ya pedido: este no se usa
        } else {
            mp3_cargando_ = false;
            if (!m) {
                reanudar_ms_ = ahora_ms + 60000;  // no existe: no se reintenta en cada frame
            } else {
                const SDL_AudioSpec spec = {SDL_AUDIO_F32, static_cast<int>(mp3(m)->channels), static_cast<int>(mp3(m)->sampleRate)};
                SDL_LockAudioStream(stream_musica_);
                SDL_SetAudioStreamFormat(stream_musica_, &spec, nullptr);
                mp3_termino_ = false;
                mp3_ = m;
                SDL_UnlockAudioStream(stream_musica_);            }
        }
    }
}

void Audio::tocar(u32 id, const Wav* w, float ganancia, u64 ahora_ms) {
    int libre = -1, viejo = 0;
    for (int i = 0; i < N_EFECTOS; ++i) {
        const bool ocupado = SDL_GetAudioStreamQueued(streams_[i]) > 0;
        if (ocupado && id_stream_[i] == id && ahora_ms - inicio_stream_[i] < MS_MISMO_EFECTO) return;
        if (!ocupado && libre < 0) libre = i;
        if (inicio_stream_[i] < inicio_stream_[viejo]) viejo = i;
    }
    const int i = libre >= 0 ? libre : viejo;
    SDL_ClearAudioStream(streams_[i]);
    SDL_SetAudioStreamFormat(streams_[i], &w->spec, nullptr);
    SDL_SetAudioStreamGain(streams_[i], std::min(1.0f, ganancia) * vol_efectos_);
    SDL_PutAudioStreamData(streams_[i], w->datos, static_cast<int>(w->bytes));
    inicio_stream_[i] = ahora_ms;
    id_stream_[i] = id;
}

void Audio::efecto(u32 id, float ganancia, u64 ahora_ms) {
    if (!dispositivo_ || ganancia <= 0.01f || vol_efectos_ <= 0) {
        ++efectos_tocados_;  // cuenta igual: las pruebas corren en silencio
        return;
    }
    ++efectos_tocados_;
    if (const Wav* w = wav(id)) {
        tocar(id, w, ganancia, ahora_ms);
        return;
    }
    // Todavía cargándose (o no existe: el cargador lo dirá). Se toca al llegar, si llega a tiempo.
    if (id >= MAX_ID_WAV || (id < wavs_.size() && wavs_[id])) return;  // ya se sabe que no existe
    for (int i = 0; i < n_pendientes_; ++i)
        if (pendientes_[i].id == id) return;  // el mismo golpe en masa: uno alcanza
    if (n_pendientes_ < N_PENDIENTES) pendientes_[n_pendientes_++] = {id, ganancia, ahora_ms};
}

void Audio::poner_ambiente(const Wav* w) {
    SDL_LockAudioStream(stream_ambiente_);
    SDL_ClearAudioStream(stream_ambiente_);
    ambiente_wav_ = w;
    if (w) SDL_SetAudioStreamFormat(stream_ambiente_, &w->spec, nullptr);
    SDL_UnlockAudioStream(stream_ambiente_);
}

void Audio::ambiente(i32 id) {
    if (id == ambiente_id_) return;
    ambiente_id_ = id;
    if (!dispositivo_) return;
    const Wav* w = id >= 0 ? wav(static_cast<u32>(id)) : nullptr;
    // Si todavía se está cargando, calla el anterior y empieza al llegar (recoger_cargados).
    ambiente_pendiente_ = id >= 0 && !w ? id : -1;
    poner_ambiente(w);
}

void Audio::alimentar_ambiente(void* yo, SDL_AudioStream* s, int, int) {
    // El bucle se vuelve a encolar cuando queda menos de la mitad.
    const Wav* w = static_cast<Audio*>(yo)->ambiente_wav_;
    if (w && SDL_GetAudioStreamQueued(s) < static_cast<int>(w->bytes / 2)) SDL_PutAudioStreamData(s, w->datos, static_cast<int>(w->bytes));
}

void Audio::alimentar_musica(void* yo, SDL_AudioStream* s, int falta, int) {
    Audio& a = *static_cast<Audio*>(yo);
    if (!a.mp3_ || a.mp3_termino_) return;
    drmp3* m = mp3(a.mp3_);
    // Lo que pide SDL más un cuarto de segundo de margen.
    const int bytes_seg = static_cast<int>(m->sampleRate * m->channels * sizeof(float));
    const int meta = std::max(falta, 0) + bytes_seg / 4;
    while (SDL_GetAudioStreamQueued(s) < meta) {
        const u64 cap = sizeof a.pcm_ / sizeof a.pcm_[0] / m->channels;
        const u64 n = drmp3_read_pcm_frames_f32(m, cap, a.pcm_);
        if (n == 0) {
            a.mp3_termino_ = true;
            return;
        }
        SDL_PutAudioStreamData(s, a.pcm_, static_cast<int>(n * m->channels * sizeof(float)));
    }
}

void Audio::cerrar_mp3() {
    if (!mp3_) return;
    if (stream_musica_) SDL_LockAudioStream(stream_musica_);
    void* viejo = mp3_;
    mp3_ = nullptr;
    mp3_termino_ = false;
    if (stream_musica_) SDL_UnlockAudioStream(stream_musica_);
    liberar_mp3(viejo);
}

// Pide el tema actual al cargador; recoger_cargados lo conecta al stream cuando está abierto.
void Audio::abrir_mp3(u64) {
    cerrar_mp3();
    reanudar_ms_ = 0;
    mp3_cargando_ = musica_n_ >= 0 && cargador_.joinable();
    if (!mp3_cargando_) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        pedido_mp3_ = musica_n_;
    }
    cv_.notify_one();
}

void Audio::musica(i32 n, u32 intervalo_s, u64 ahora_ms) {
    if (n == musica_n_) return;
    musica_n_ = n;
    intervalo_s_ = intervalo_s;
    if (!dispositivo_) return;
    cerrar_mp3();  // antes de vaciar: si no, el callback alcanza a encolar del tema anterior
    SDL_ClearAudioStream(stream_musica_);
    abrir_mp3(ahora_ms);
}

void Audio::actualizar(u64 ahora_ms) {
    if (!dispositivo_) return;
    recoger_cargados(ahora_ms);
    // La música y el ambiente los alimentan sus callbacks; aquí, el fin del tema y su vuelta.
    if (mp3_ && mp3_termino_) {
        cerrar_mp3();
        reanudar_ms_ = ahora_ms + static_cast<u64>(intervalo_s_) * 1000;
    } else if (!mp3_ && !mp3_cargando_ && musica_n_ >= 0 && reanudar_ms_ && ahora_ms >= reanudar_ms_) {
        abrir_mp3(ahora_ms);
    }
}

}  // namespace lin
