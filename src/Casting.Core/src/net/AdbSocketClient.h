// Mirra — ADB Socket Client
// Connects to localhost TCP ports forwarded/reversed by ADB for video and control.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace mirra {

using VideoPacketCallback = std::function<void(const uint8_t* data, size_t size, int64_t ptsUs)>;
using AudioPacketCallback = std::function<void(const uint8_t* data, size_t size)>;
using ClipboardCallback = std::function<void(const std::string& text)>;

class AdbSocketClient {
public:
    AdbSocketClient();
    ~AdbSocketClient();

    // Connect to video socket on localhost:port
    bool connectVideo(int port, int timeoutMs = 5000);

    // Connect to control socket on localhost:port
    bool connectControl(int port, int timeoutMs = 5000);

    // Connect to audio socket on localhost:port
    bool connectAudio(int port, int timeoutMs = 5000);

    // Start video receiving thread
    void startVideoReceiver(VideoPacketCallback callback);

    // Start audio receiving thread
    void startAudioReceiver(AudioPacketCallback callback);

    // Start control receiving thread
    void startControlReceiver(ClipboardCallback callback);

    // Raw control packet send
    bool sendControl(const uint8_t* data, size_t size);

    // High-level control commands (matching AndroidServer Controller protocol)
    bool sendTouchEvent(int action, int pointerId, int x, int y, int screenW, int screenH, float pressure = 1.0f, int buttons = 0);
    bool sendKeyEvent(int action, int keyCode, int metaState = 0);
    bool sendText(const std::string& text);
    bool sendClipboard(const std::string& text);
    bool sendScroll(int x, int y, int screenW, int screenH, float hScroll, float vScroll);
    bool sendScreenPowerMode(int powerMode);

    // Disconnect and stop
    void disconnect();

    bool isConnected() const { return m_videoConnected.load() && m_controlConnected.load(); }
    bool isVideoConnected() const { return m_videoConnected.load(); }
    bool isControlConnected() const { return m_controlConnected.load(); }
    bool isAudioConnected() const { return m_audioConnected.load(); }

    int videoWidth() const { return m_videoWidth; }
    int videoHeight() const { return m_videoHeight; }

private:
    void videoReceiveLoop();
    void audioReceiveLoop();
    void controlReceiveLoop();

#ifdef _WIN32
    SOCKET m_videoSock   = INVALID_SOCKET;
    SOCKET m_controlSock = INVALID_SOCKET;
    SOCKET m_audioSock   = INVALID_SOCKET;
#else
    int m_videoSock   = -1;
    int m_controlSock = -1;
    int m_audioSock   = -1;
#endif

    std::atomic<bool> m_videoConnected{false};
    std::atomic<bool> m_controlConnected{false};
    std::atomic<bool> m_audioConnected{false};
    std::atomic<bool> m_running{false};
    std::mutex        m_controlMutex;

    int m_videoWidth  = 0;
    int m_videoHeight = 0;

    VideoPacketCallback m_videoCallback;
    AudioPacketCallback m_audioCallback;
    ClipboardCallback m_clipboardCallback;

    std::thread         m_videoThread;
    std::thread         m_audioThread;
    std::thread         m_controlThread;
};

} // namespace mirra
