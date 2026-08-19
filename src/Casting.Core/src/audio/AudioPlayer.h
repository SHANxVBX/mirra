#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <vector>

namespace mirra {

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    bool init(int sampleRate = 48000, int channels = 2);
    void destroy();

    void queueAudio(const uint8_t* data, size_t size);

private:
    SDL_AudioStream* m_stream = nullptr;
    SDL_AudioDeviceID m_deviceId = 0;
    bool m_initialized = false;
};

} // namespace mirra
