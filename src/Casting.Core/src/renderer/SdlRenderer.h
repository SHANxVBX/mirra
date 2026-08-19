// Mirra — SDL3 Renderer
// Creates an SDL3 window + D3D11 renderer, presents decoded YUV frames.

#pragma once

#include <memory>
#include <string>
#include "../decoder/H264Decoder.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace mirra {

class SdlRenderer {
public:
    SdlRenderer();
    ~SdlRenderer();

    // Initialize SDL3 window and renderer
    bool init(const std::string& title, int width, int height, bool headless = false);

    // Update and present a decoded frame (called from decoder callback)
    void presentFrame(const std::shared_ptr<DecodedFrame>& frame);

    // Resize the window/surface
    void resize(int width, int height);

    // Returns the native HWND for WPF HwndHost embedding (Windows only)
    void* nativeHwnd() const;

    // Poll SDL events; returns false if window close was requested
    bool pollEvents();

    void destroy();

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture*  m_texture  = nullptr;
    int           m_texW     = 0;
    int           m_texH     = 0;
    bool          m_headless = false;
};

} // namespace mirra
