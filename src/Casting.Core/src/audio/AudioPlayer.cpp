#include "AudioPlayer.h"
#include "../diag/DiagLogger.h"

namespace mirra {

AudioPlayer::AudioPlayer() {}

AudioPlayer::~AudioPlayer() {
    destroy();
}

bool AudioPlayer::init(int sampleRate, int channels) {
    if (m_initialized) return true;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    SDL_AudioSpec spec;
    spec.freq = sampleRate;
    spec.channels = channels;
    spec.format = SDL_AUDIO_S16LE; // 16-bit PCM

    m_deviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (m_deviceId == 0) {
        DiagLogger::get().error("Failed to open audio device: {}", SDL_GetError());
        return false;
    }

    m_stream = SDL_CreateAudioStream(&spec, &spec);
    if (!m_stream) {
        DiagLogger::get().error("Failed to create audio stream: {}", SDL_GetError());
        SDL_CloseAudioDevice(m_deviceId);
        return false;
    }

    SDL_BindAudioStream(m_deviceId, m_stream);
    SDL_ResumeAudioDevice(m_deviceId);

    m_initialized = true;
    DiagLogger::get().info("AudioPlayer initialized: {} Hz, {} channels", sampleRate, channels);
    return true;
}

void AudioPlayer::destroy() {
    if (!m_initialized) return;
    
    if (m_stream) {
        SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }
    if (m_deviceId != 0) {
        SDL_CloseAudioDevice(m_deviceId);
        m_deviceId = 0;
    }
    
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    m_initialized = false;
}

void AudioPlayer::queueAudio(const uint8_t* data, size_t size) {
    if (!m_initialized || !m_stream) return;
    SDL_PutAudioStreamData(m_stream, data, static_cast<int>(size));
}

} // namespace mirra
