#include "SessionStateMachine.h"
#include "../ipc/IPipeServer.h"
#include "../diag/DiagLogger.h"
#include <chrono>
#include <filesystem>

namespace mirra {

SessionStateMachine::SessionStateMachine(
    const std::string& sessionId,
    const std::string& deviceSerial,
    IPipeServer& pipe)
    : m_sessionId(sessionId)
    , m_deviceSerial(deviceSerial)
    , m_pipe(pipe)
{
    // Find bundled adb path
    std::string adbPath = "platform-tools/adb.exe";
    if (!std::filesystem::exists(adbPath)) {
        adbPath = "adb.exe";
    }

    m_adb = std::make_unique<AdbManager>(adbPath);
    m_socketClient = std::make_unique<AdbSocketClient>();
    m_decoder = std::make_unique<H264Decoder>();
    m_renderer = std::make_unique<SdlRenderer>();
    m_audioPlayer = std::make_unique<AudioPlayer>();

    m_lastHealthTick = std::chrono::steady_clock::now();
}

SessionStateMachine::~SessionStateMachine() {
    if (m_state != SessionState::Stopped) stop();
}

bool SessionStateMachine::isStopped() const {
    return m_state == SessionState::Stopped;
}

SessionState SessionStateMachine::currentState() const {
    return m_state;
}

void SessionStateMachine::transitionTo(SessionState next) {
    auto& log = DiagLogger::get();
    log.info("Session state: {} -> {}", sessionStateToString(m_state), sessionStateToString(next));
    m_state = next;
    sendStateEvent(next);
}

void SessionStateMachine::sendStateEvent(SessionState state) const {
    PayloadStateChanged payload;
    payload.state        = sessionStateToString(state);
    payload.deviceSerial = m_deviceSerial;

    IpcMessage msg;
    msg.version = 1;
    msg.session = m_sessionId;
    msg.type    = MSG_STATE_CHANGED;
    msg.ts      = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count();
    msg.payload = payload.toJson();

    m_pipe.send(msg);
}

void SessionStateMachine::sendError(int code, const std::string& message, const std::string& layer) const {
    PayloadError p;
    p.code    = code;
    p.message = message;
    p.layer   = layer;

    IpcMessage msg;
    msg.version = 1;
    msg.session = m_sessionId;
    msg.type    = MSG_ERROR;
    msg.ts      = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count();
    msg.payload = p.toJson();
    m_pipe.send(msg);
}

void SessionStateMachine::sendHealthTick() {
    PayloadHealthTick p;
    p.fps = 60.0f; // reported from renderer/decoder
    p.decodeMs = static_cast<int>(m_decoder->avgDecodeMs());
    p.frameDropCount = m_decoder->frameDropCount();
    p.audioLatencyMs = 0;

    IpcMessage msg;
    msg.version = 1;
    msg.session = m_sessionId;
    msg.type    = MSG_HEALTH_TICK;
    msg.ts      = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count();
    msg.payload = p.toJson();
    m_pipe.send(msg);
}

void SessionStateMachine::handleCommand(const IpcMessage& msg) {
    auto& log = DiagLogger::get();
    log.info("Received command: {}", msg.type);

    if (msg.type == CMD_START_SESSION && m_state == SessionState::Idle) {
        m_startTime = std::chrono::steady_clock::now();
        transitionTo(SessionState::AdbSetup);
    } else if (msg.type == CMD_STOP_SESSION) {
        stop();
    } else if (msg.type == CMD_SEND_INPUT && m_state == SessionState::Streaming) {
        // Forward input event to server control socket
        std::string eventType = msg.payload.value("eventType", "");
        if (eventType == "touch_down" || eventType == "touch_up" || eventType == "touch_move") {
            int action = (eventType == "touch_down") ? 0 : ((eventType == "touch_up") ? 1 : 2);
            float x = msg.payload.value("x", 0.0f);
            float y = msg.payload.value("y", 0.0f);
            int w = m_socketClient->videoWidth() > 0 ? m_socketClient->videoWidth() : 1080;
            int h = m_socketClient->videoHeight() > 0 ? m_socketClient->videoHeight() : 1920;
            int px = static_cast<int>(x * w);
            int py = static_cast<int>(y * h);
            m_socketClient->sendTouchEvent(action, 0, px, py, w, h);
        } else if (eventType == "key") {
            int action = msg.payload.value("action", 0);
            int keyCode = msg.payload.value("keyCode", 0);
            int metaState = msg.payload.value("metaState", 0);
            m_socketClient->sendKeyEvent(action, keyCode, metaState);
        } else if (eventType == "scroll") {
            float x = msg.payload.value("x", 0.0f);
            float y = msg.payload.value("y", 0.0f);
            float sx = msg.payload.value("scrollX", 0.0f);
            float sy = msg.payload.value("scrollY", 0.0f);
            int w = m_socketClient->videoWidth() > 0 ? m_socketClient->videoWidth() : 1080;
            int h = m_socketClient->videoHeight() > 0 ? m_socketClient->videoHeight() : 1920;
            m_socketClient->sendScroll(static_cast<int>(x * w), static_cast<int>(y * h), w, h, sx, sy);
        }
    } else if (msg.type == CMD_SEND_CLIPBOARD && m_state == SessionState::Streaming) {
        std::string text = msg.payload.value("text", "");
        m_socketClient->sendClipboard(text);
    } else if (msg.type == CMD_SET_WINDOW_SIZE) {
        int w = msg.payload.value("widthPx", 1080);
        int h = msg.payload.value("heightPx", 1920);
        if (m_renderer) m_renderer->resize(w, h);
    }
}

void SessionStateMachine::tick() {
    switch (m_state) {
        case SessionState::Idle:          tickIdle();          break;
        case SessionState::AdbSetup:      tickAdbSetup();      break;
        case SessionState::ServerInstall: tickServerInstall(); break;
        case SessionState::Tunneling:     tickTunneling();     break;
        case SessionState::Streaming:     tickStreaming();     break;
        case SessionState::Recovering:    tickRecovering();    break;
        case SessionState::Stopped:       break;
    }
}

void SessionStateMachine::stop() {
    auto& log = DiagLogger::get();
    log.info("Stopping session {}", m_sessionId);

    if (m_socketClient) m_socketClient->disconnect();
    if (m_decoder) m_decoder->flush();
    if (m_renderer) m_renderer->destroy();
    if (m_audioPlayer) m_audioPlayer->destroy();
    if (m_adb) {
        m_adb->removeForwards(m_deviceSerial);
        m_adb->stopAsyncWorkers();
    }

    transitionTo(SessionState::Stopped);
}

void SessionStateMachine::tickIdle() {
    // Waiting for command
}

void SessionStateMachine::tickAdbSetup() {
    auto& log = DiagLogger::get();
    log.info("Setting up ADB tunnels for device {}", m_deviceSerial);

    // Set up reverse / forward port tunnels
    bool fwdVideo = m_adb->forward(m_deviceSerial, k_videoPort, k_videoPort);
    bool fwdControl = m_adb->forward(m_deviceSerial, k_controlPort, k_controlPort);
    bool fwdAudio = m_adb->forward(m_deviceSerial, k_audioPort, k_audioPort);

    if (!fwdVideo || !fwdControl || !fwdAudio) {
        log.warn("ADB port forward failed, attempting reverse tunnel");
        m_adb->reverse(m_deviceSerial, k_videoPort, k_videoPort);
        m_adb->reverse(m_deviceSerial, k_controlPort, k_controlPort);
        m_adb->reverse(m_deviceSerial, k_audioPort, k_audioPort);
    }

    transitionTo(SessionState::ServerInstall);
}

void SessionStateMachine::tickServerInstall() {
    auto& log = DiagLogger::get();
    log.info("Pushing server JAR to device {}", m_deviceSerial);

    // Check for server JAR
    std::string serverJar = "server/mirra-server.jar";
    m_adb->push(m_deviceSerial, serverJar, "/data/local/tmp/mirra-server.jar");

    // Spawn server process asynchronously via adb shell
    std::string cmd = "CLASSPATH=/data/local/tmp/mirra-server.jar app_process / com.mirra.server.Server";
    log.info("Starting MirraServer on device asynchronously: {}", cmd);
    m_adb->shellAsync(m_deviceSerial, cmd);

    transitionTo(SessionState::Tunneling);
}

void SessionStateMachine::tickTunneling() {
    auto& log = DiagLogger::get();
    log.info("Connecting to video and control sockets...");

    // Allow server a moment to start, with retries for tunneling resilience
    bool vConn = false;
    bool cConn = false;
    bool aConn = false;
    for (int i = 0; i < 15; ++i) { // Try for up to ~3 seconds total
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        vConn = m_socketClient->connectVideo(k_videoPort, 500);
        cConn = m_socketClient->connectControl(k_controlPort, 500);
        aConn = m_socketClient->connectAudio(k_audioPort, 500);
        
        if (vConn && cConn && aConn) {
            break;
        }
        
        log.warn("Socket connection attempt {} failed, retrying...", i + 1);
    }

    if (!vConn || !cConn || !aConn) {
        log.error("Socket connection failed after retries. Entering recovery.");
        transitionTo(SessionState::Recovering);
        return;
    }

    // Initialize decoder and renderer
    if (!m_decoder->init()) {
        sendError(2001, "Failed to initialize H.264 decoder", "decoder");
        transitionTo(SessionState::Stopped);
        return;
    }

    int w = m_socketClient->videoWidth() > 0 ? m_socketClient->videoWidth() : 1080;
    int h = m_socketClient->videoHeight() > 0 ? m_socketClient->videoHeight() : 1920;

    if (!m_renderer->init("Mirra Native Video Surface", w, h, false)) {
        log.warn("SDL renderer init fallback");
    }

    // Report CoreHwnd to Shell for HwndHost embedding
    void* hwnd = m_renderer->nativeHwnd();
    if (hwnd) {
        PayloadCoreHwnd p;
        p.hwnd = reinterpret_cast<int64_t>(hwnd);
        IpcMessage msg;
        msg.version = 1;
        msg.session = m_sessionId;
        msg.type = MSG_CORE_HWND;
        msg.payload = p.toJson();
        m_pipe.send(msg);
        log.info("Sent CoreHwnd to shell: {}", p.hwnd);
    }

    // Wire decoder output -> renderer
    m_decoder->onFrame([this](std::shared_ptr<DecodedFrame> frame) {
        if (!m_firstFrameSent) {
            m_firstFrameSent = true;
            auto now = std::chrono::steady_clock::now();
            int latency = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count());

            PayloadFirstFrame p;
            p.widthPx = frame->width;
            p.heightPx = frame->height;
            p.latencyMs = latency;

            IpcMessage msg;
            msg.version = 1;
            msg.session = m_sessionId;
            msg.type = MSG_FIRST_FRAME;
            msg.payload = p.toJson();
            m_pipe.send(msg);
        }
        m_renderer->presentFrame(frame);
    });

    // Start video socket receiver thread
    m_socketClient->startVideoReceiver([this](const uint8_t* data, size_t size, int64_t ptsUs) {
        m_decoder->feed(data, size, ptsUs);
    });

    // Initialize and start audio
    m_audioPlayer->init(48000, 2);
    m_socketClient->startAudioReceiver([this](const uint8_t* data, size_t size) {
        m_audioPlayer->queueAudio(data, size);
    });

    // Start control socket receiver
    m_socketClient->startControlReceiver([this](const std::string& text) {
        nlohmann::json payload;
        payload["text"] = text;
        
        IpcMessage msg;
        msg.version = 1;
        msg.session = m_sessionId;
        msg.type = CMD_SET_CLIPBOARD;
        msg.ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count();
        msg.payload = payload;
        m_pipe.send(msg);
    });

    transitionTo(SessionState::Streaming);
}

void SessionStateMachine::tickStreaming() {
    if (!m_socketClient->isConnected()) {
        transitionTo(SessionState::Recovering);
        return;
    }

    // Poll SDL events
    if (!m_renderer->pollEvents()) {
        stop();
        return;
    }

    // 1 Hz health tick
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - m_lastHealthTick).count() >= 1) {
        m_lastHealthTick = now;
        sendHealthTick();
    }
}

void SessionStateMachine::tickRecovering() {
    auto& log = DiagLogger::get();
    ++m_recoveryAttempt;
    log.info("Recovery attempt {}/{}", m_recoveryAttempt, k_maxRecovery);

    if (m_recoveryAttempt > k_maxRecovery) {
        sendError(1001, "Max recovery attempts exceeded", "session");
        transitionTo(SessionState::Stopped);
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(500 * m_recoveryAttempt));
        transitionTo(SessionState::AdbSetup);
    }
}

} // namespace mirra
