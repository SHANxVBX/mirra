#include "AdbSocketClient.h"
#include "../diag/DiagLogger.h"
#include <chrono>
#include <cstring>

namespace mirra {

namespace {

inline void putUint8(std::vector<uint8_t>& buf, uint8_t v) {
    buf.push_back(v);
}

inline void putUint16BE(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void putUint32BE(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void putFloatBE(std::vector<uint8_t>& buf, float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(float));
    putUint32BE(buf, u);
}

} // anonymous namespace

AdbSocketClient::AdbSocketClient() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

AdbSocketClient::~AdbSocketClient() {
    disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

static SOCKET connectLocal(int port, int timeoutMs) {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    // Set non-blocking for connect with timeout
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);

    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(s, &writeSet);

    timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(0, nullptr, &writeSet, nullptr, &tv);
    if (sel <= 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    // Set back to blocking
    nonBlocking = 0;
    ioctlsocket(s, FIONBIO, &nonBlocking);

    // Disable Nagle's algorithm for lowest latency
    int nodelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    return s;
#else
    return -1;
#endif
}

bool AdbSocketClient::connectVideo(int port, int timeoutMs) {
    m_videoSock = connectLocal(port, timeoutMs);
    if (m_videoSock == INVALID_SOCKET) {
        DiagLogger::get().warn("Failed to connect to video port {}", port);
        return false;
    }

    // Read initial 4-byte header: width (2 bytes) + height (2 bytes) in big-endian
    uint8_t header[4];
    int r = recv(m_videoSock, reinterpret_cast<char*>(header), 4, MSG_WAITALL);
    if (r == 4) {
        m_videoWidth  = (static_cast<int>(header[0]) << 8) | header[1];
        m_videoHeight = (static_cast<int>(header[2]) << 8) | header[3];
        DiagLogger::get().info("Connected to video socket: device size {}x{}", m_videoWidth, m_videoHeight);
    } else {
        m_videoWidth  = 1080;
        m_videoHeight = 2400;
    }

    m_videoConnected.store(true);
    return true;
}

bool AdbSocketClient::connectControl(int port, int timeoutMs) {
    m_controlSock = connectLocal(port, timeoutMs);
    if (m_controlSock == INVALID_SOCKET) {
        DiagLogger::get().warn("Failed to connect to control port {}", port);
        return false;
    }

    m_controlConnected.store(true);
    DiagLogger::get().info("Connected to control socket on port {}", port);
    return true;
}

bool AdbSocketClient::connectAudio(int port, int timeoutMs) {
    m_audioSock = connectLocal(port, timeoutMs);
    if (m_audioSock == INVALID_SOCKET) {
        DiagLogger::get().warn("Failed to connect to audio port {}", port);
        return false;
    }
    
    m_audioConnected.store(true);
    DiagLogger::get().info("Connected to audio socket on port {}", port);
    return true;
}

void AdbSocketClient::startVideoReceiver(VideoPacketCallback callback) {
    m_videoCallback = std::move(callback);
    m_running.store(true);
    m_videoThread = std::thread(&AdbSocketClient::videoReceiveLoop, this);
}

void AdbSocketClient::startAudioReceiver(AudioPacketCallback callback) {
    m_audioCallback = std::move(callback);
    m_running.store(true);
    m_audioThread = std::thread(&AdbSocketClient::audioReceiveLoop, this);
}

void AdbSocketClient::startControlReceiver(ClipboardCallback callback) {
    m_clipboardCallback = std::move(callback);
    m_running.store(true);
    m_controlThread = std::thread(&AdbSocketClient::controlReceiveLoop, this);
}

void AdbSocketClient::videoReceiveLoop() {
    auto& log = DiagLogger::get();
    log.info("Starting video receiver loop");

    uint8_t header[12]; // 4 bytes size + 8 bytes PTS (us)
    std::vector<uint8_t> payload;

    while (m_running.load() && m_videoSock != INVALID_SOCKET) {
        int r = recv(m_videoSock, reinterpret_cast<char*>(header), 12, MSG_WAITALL);
        if (r <= 0) {
            log.warn("Video socket disconnected");
            break;
        }

        uint32_t size = (static_cast<uint32_t>(header[0]) << 24) |
                        (static_cast<uint32_t>(header[1]) << 16) |
                        (static_cast<uint32_t>(header[2]) << 8)  |
                        static_cast<uint32_t>(header[3]);

        int64_t ptsUs = 0;
        for (int i = 0; i < 8; ++i) {
            ptsUs = (ptsUs << 8) | header[4 + i];
        }

        if (size > 10 * 1024 * 1024) {
            log.error("Corrupt video packet size: {}", size);
            break;
        }

        payload.resize(size);
        int received = 0;
        while (received < static_cast<int>(size) && m_running.load()) {
            int chunk = recv(m_videoSock, reinterpret_cast<char*>(payload.data() + received), static_cast<int>(size) - received, 0);
            if (chunk <= 0) break;
            received += chunk;
        }

        if (received == static_cast<int>(size) && m_videoCallback) {
            m_videoCallback(payload.data(), payload.size(), ptsUs);
        }
    }

    m_videoConnected.store(false);
    log.info("Video receiver loop stopped");
}

void AdbSocketClient::audioReceiveLoop() {
    auto& log = DiagLogger::get();
    log.info("Starting audio receiver loop");
    
    // Expect raw PCM chunks
    const int CHUNK_SIZE = 4096;
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    while (m_running.load() && m_audioSock != INVALID_SOCKET) {
        int r = recv(m_audioSock, reinterpret_cast<char*>(buffer.data()), CHUNK_SIZE, 0);
        if (r <= 0) {
            log.warn("Audio socket disconnected");
            break;
        }

        if (m_audioCallback) {
            m_audioCallback(buffer.data(), r);
        }
    }
    
    m_audioConnected.store(false);
    log.info("Audio receiver loop stopped");
}

void AdbSocketClient::controlReceiveLoop() {
    auto& log = DiagLogger::get();
    log.info("Starting control receiver loop");

    // TYPE_SET_CLIPBOARD packets: 1 byte type (4), 4 bytes length, N bytes UTF-8 text
    uint8_t header[5];
    
    while (m_running.load() && m_controlSock != INVALID_SOCKET) {
        int r = recv(m_controlSock, reinterpret_cast<char*>(header), 5, MSG_WAITALL);
        if (r <= 0) {
            log.warn("Control socket disconnected");
            break;
        }
        
        if (header[0] == 4) {
            uint32_t len = (static_cast<uint32_t>(header[1]) << 24) |
                           (static_cast<uint32_t>(header[2]) << 16) |
                           (static_cast<uint32_t>(header[3]) << 8)  |
                           static_cast<uint32_t>(header[4]);

            if (len > 10 * 1024 * 1024) {
                log.error("Corrupt clipboard packet size: {}", len);
                continue;
            }
            
            std::string text(len, '\0');
            int received = 0;
            while (received < static_cast<int>(len) && m_running.load()) {
                int chunk = recv(m_controlSock, &text[received], static_cast<int>(len) - received, 0);
                if (chunk <= 0) break;
                received += chunk;
            }
            
            if (received == static_cast<int>(len) && m_clipboardCallback) {
                m_clipboardCallback(text);
            }
        } else {
            // Other packets not expected from AndroidServer currently, just drop them
            log.warn("Unexpected control packet type: {}", header[0]);
        }
    }

    m_controlConnected.store(false);
    log.info("Control receiver loop stopped");
}

bool AdbSocketClient::sendControl(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_controlMutex);
    if (m_controlSock == INVALID_SOCKET || !m_controlConnected.load()) return false;
    int sent = send(m_controlSock, reinterpret_cast<const char*>(data), static_cast<int>(size), 0);
    return sent == static_cast<int>(size);
}

// ── Control Message Protocol Implementation (matching Controller.java) ──────────

bool AdbSocketClient::sendTouchEvent(int action, int pointerId, int x, int y, int screenW, int screenH, float pressure, int buttons) {
    std::vector<uint8_t> buf;
    buf.reserve(26);
    putUint8(buf, 0); // TYPE_INJECT_TOUCH_EVENT
    putUint8(buf, static_cast<uint8_t>(action));
    putUint32BE(buf, static_cast<uint32_t>(pointerId));
    putUint32BE(buf, static_cast<uint32_t>(x));
    putUint32BE(buf, static_cast<uint32_t>(y));
    putUint16BE(buf, static_cast<uint16_t>(screenW));
    putUint16BE(buf, static_cast<uint16_t>(screenH));
    putFloatBE(buf, pressure);
    putUint32BE(buf, static_cast<uint32_t>(buttons));

    return sendControl(buf.data(), buf.size());
}

bool AdbSocketClient::sendKeyEvent(int action, int keyCode, int metaState) {
    std::vector<uint8_t> buf;
    buf.reserve(10);
    putUint8(buf, 1); // TYPE_INJECT_KEY_EVENT
    putUint8(buf, static_cast<uint8_t>(action));
    putUint32BE(buf, static_cast<uint32_t>(keyCode));
    putUint32BE(buf, static_cast<uint32_t>(metaState));

    return sendControl(buf.data(), buf.size());
}

bool AdbSocketClient::sendText(const std::string& text) {
    std::vector<uint8_t> buf;
    buf.reserve(5 + text.size());
    putUint8(buf, 2); // TYPE_INJECT_TEXT
    putUint32BE(buf, static_cast<uint32_t>(text.size()));
    buf.insert(buf.end(), text.begin(), text.end());

    return sendControl(buf.data(), buf.size());
}

bool AdbSocketClient::sendClipboard(const std::string& text) {
    std::vector<uint8_t> buf;
    buf.reserve(5 + text.size());
    putUint8(buf, 4); // TYPE_SET_CLIPBOARD
    putUint32BE(buf, static_cast<uint32_t>(text.size()));
    buf.insert(buf.end(), text.begin(), text.end());

    return sendControl(buf.data(), buf.size());
}

bool AdbSocketClient::sendScroll(int x, int y, int screenW, int screenH, float hScroll, float vScroll) {
    std::vector<uint8_t> buf;
    buf.reserve(21);
    putUint8(buf, 3); // TYPE_INJECT_SCROLL_EVENT
    putUint32BE(buf, static_cast<uint32_t>(x));
    putUint32BE(buf, static_cast<uint32_t>(y));
    putUint16BE(buf, static_cast<uint16_t>(screenW));
    putUint16BE(buf, static_cast<uint16_t>(screenH));
    putFloatBE(buf, hScroll);
    putFloatBE(buf, vScroll);

    return sendControl(buf.data(), buf.size());
}

bool AdbSocketClient::sendScreenPowerMode(int powerMode) {
    std::vector<uint8_t> buf;
    buf.reserve(2);
    putUint8(buf, 6); // TYPE_SET_SCREEN_POWER_MODE
    putUint8(buf, static_cast<uint8_t>(powerMode));

    return sendControl(buf.data(), buf.size());
}

void AdbSocketClient::disconnect() {
    m_running.store(false);
#ifdef _WIN32
    if (m_videoSock != INVALID_SOCKET) {
        closesocket(m_videoSock);
        m_videoSock = INVALID_SOCKET;
    }
    if (m_controlSock != INVALID_SOCKET) {
        closesocket(m_controlSock);
        m_controlSock = INVALID_SOCKET;
    }
    if (m_audioSock != INVALID_SOCKET) {
        closesocket(m_audioSock);
        m_audioSock = INVALID_SOCKET;
    }
#endif
    m_videoConnected.store(false);
    m_controlConnected.store(false);
    m_audioConnected.store(false);

    if (m_videoThread.joinable()) {
        m_videoThread.join();
    }
    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }
    if (m_controlThread.joinable()) {
        m_controlThread.join();
    }
}

} // namespace mirra
