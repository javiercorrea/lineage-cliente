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

drmp3* mp3(void* p) { return static_cast<drmp3*>(p); }

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
    return true;
}

void Audio::cerrar() {
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
}

void Audio::volumenes(int musica, int efectos) {
    vol_musica_ = std::clamp(musica, 0, 100) / 100.0f;
    vol_efectos_ = std::clamp(efectos, 0, 100) / 100.0f;
    if (!dispositivo_) return;
    SDL_SetAudioStreamGain(stream_musica_, vol_musica_);
    SDL_SetAudioStreamGain(stream_ambiente_, vol_efectos_ * 0.7f);
}

const Audio::Wav* Audio::wav(u32 id) {
    if (id >= 20000) return nullptr;
    if (id >= wavs_.size()) wavs_.resize(id + 1, nullptr);
    if (!wavs_[id]) {
        Wav* w = new Wav;
        char ruta[64];
        std::snprintf(ruta, sizeof ruta, "%u.wav", id);
        if (!SDL_LoadWAV((carpeta_ + ruta).c_str(), &w->spec, &w->datos, &w->bytes)) w->bytes = 0;
        wavs_[id] = w;
    }
    return wavs_[id]->bytes ? wavs_[id] : nullptr;
}

void Audio::efecto(u32 id, float ganancia, u64 ahora_ms) {
    if (!dispositivo_ || ganancia <= 0.01f || vol_efectos_ <= 0) {
        ++efectos_tocados_;  // cuenta igual: las pruebas corren en silencio
        return;
    }
    const Wav* w = wav(id);
    if (!w) return;
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
    ++efectos_tocados_;
}

void Audio::ambiente(i32 id) {
    if (id == ambiente_id_) return;
    ambiente_id_ = id;
    if (!dispositivo_) return;
    const Wav* w = id >= 0 ? wav(static_cast<u32>(id)) : nullptr;  // fuera del bloqueo: puede leer el disco
    SDL_LockAudioStream(stream_ambiente_);
    SDL_ClearAudioStream(stream_ambiente_);
    ambiente_wav_ = w;
    if (w) SDL_SetAudioStreamFormat(stream_ambiente_, &w->spec, nullptr);
    SDL_UnlockAudioStream(stream_ambiente_);
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
    drmp3_uninit(mp3(viejo));
    delete mp3(viejo);
}

void Audio::abrir_mp3(u64 ahora_ms) {
    cerrar_mp3();
    reanudar_ms_ = 0;
    if (musica_n_ < 0) return;
    char nombre[32];
    std::snprintf(nombre, sizeof nombre, "music%d.mp3", musica_n_);
    auto* m = new drmp3;
    // Algunos archivos empiezan con mayúscula (Music1012.mp3): en macOS el sistema de archivos no
    // las distingue. MULTIPLATAFORMA: en Linux, buscar sin distinguir mayúsculas.
    if (!drmp3_init_file(m, (carpeta_ + nombre).c_str(), nullptr)) {
        delete m;
        reanudar_ms_ = ahora_ms + 60000;  // no existe: no se reintenta en cada frame
        return;
    }
    const SDL_AudioSpec spec = {SDL_AUDIO_F32, static_cast<int>(m->channels), static_cast<int>(m->sampleRate)};
    SDL_LockAudioStream(stream_musica_);
    SDL_SetAudioStreamFormat(stream_musica_, &spec, nullptr);
    mp3_termino_ = false;
    mp3_ = m;
    SDL_UnlockAudioStream(stream_musica_);
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
    // La música y el ambiente los alimentan sus callbacks; aquí, el fin del tema y su vuelta.
    if (mp3_ && mp3_termino_) {
        cerrar_mp3();
        reanudar_ms_ = ahora_ms + static_cast<u64>(intervalo_s_) * 1000;
    } else if (!mp3_ && musica_n_ >= 0 && reanudar_ms_ && ahora_ms >= reanudar_ms_) {
        abrir_mp3(ahora_ms);
    }
}

}  // namespace lin
