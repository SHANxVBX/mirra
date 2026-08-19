#include "SdlRenderer.h"
#include "../diag/DiagLogger.h"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace mirra {

SdlRenderer::SdlRenderer() = default;

SdlRenderer::~SdlRenderer() {
    destroy();
}

bool SdlRenderer::init(const std::string& title, int width, int height, bool headless) {
    auto& log = DiagLogger::get();
    m_headless = headless;

    if (headless) {
        log.info("SdlRenderer: headless mode (no window)");
        return true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log.error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_window) {
        log.error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    // Prefer Direct3D 11 on Windows for best performance
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // Linear filtering
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");         // Disable VSync at hint level too

    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        log.warn("D3D11 renderer unavailable, falling back: {}", SDL_GetError());
        m_renderer = SDL_CreateRenderer(m_window, nullptr);
    }
    if (!m_renderer) {
        log.error("SDL_CreateRenderer failed: {}", SDL_GetError());
        return false;
    }

    // Explicitly disable VSync for lowest latency
    SDL_SetRenderVSync(m_renderer, 0);

    log.info("SdlRenderer initialized: {}x{}", width, height);
    return true;
}

void SdlRenderer::presentFrame(const std::shared_ptr<DecodedFrame>& frame) {
    if (m_headless || !m_renderer || !frame) return;

    // (Re)create texture if dimensions changed
    if (!m_texture || m_texW != frame->width || m_texH != frame->height) {
        if (m_texture) SDL_DestroyTexture(m_texture);
        m_texture = SDL_CreateTexture(m_renderer,
            SDL_PIXELFORMAT_IYUV,           // YUV420P
            SDL_TEXTUREACCESS_STREAMING,
            frame->width, frame->height);
        m_texW = frame->width;
        m_texH = frame->height;
    }

    // Update YUV planes
    SDL_UpdateYUVTexture(m_texture, nullptr,
        frame->dataY.data(), frame->strideY,
        frame->dataU.data(), frame->strideU,
        frame->dataV.data(), frame->strideV);

    SDL_RenderClear(m_renderer);

    // Aspect-ratio-correct scaling
    int winW = 0, winH = 0;
    SDL_GetWindowSize(m_window, &winW, &winH);
    float scaleX = static_cast<float>(winW) / frame->width;
    float scaleY = static_cast<float>(winH) / frame->height;
    float scale  = std::min(scaleX, scaleY);

    SDL_FRect dst{
        (winW - frame->width  * scale) / 2.0f,
        (winH - frame->height * scale) / 2.0f,
        frame->width  * scale,
        frame->height * scale
    };

    SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst);
    SDL_RenderPresent(m_renderer);
}

void SdlRenderer::resize(int width, int height) {
    if (m_window) SDL_SetWindowSize(m_window, width, height);
}

void* SdlRenderer::nativeHwnd() const {
#ifdef _WIN32
    if (!m_window) return nullptr;
    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    return nullptr;
#endif
}

bool SdlRenderer::pollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) return false;
    }
    return true;
}

void SdlRenderer::destroy() {
    if (m_texture)  { SDL_DestroyTexture(m_texture);   m_texture  = nullptr; }
    if (m_renderer) { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    if (m_window)   { SDL_DestroyWindow(m_window);     m_window   = nullptr; }
    SDL_Quit();
}

} // namespace mirra
