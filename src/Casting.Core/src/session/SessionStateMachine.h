// Mirra — Session State Machine
// States: Idle → AdbSetup → ServerInstall → Tunneling → Streaming → Recovering → Stopped

#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include "SessionState.h"
#include "../ipc/IpcMessages.h"
#include "../ipc/IPipeServer.h"
#include "../adb/AdbManager.h"
#include "../net/AdbSocketClient.h"
#include "../decoder/H264Decoder.h"
#include "../renderer/SdlRenderer.h"
#include "../audio/AudioPlayer.h"

namespace mirra {

class SessionStateMachine {
public:
    SessionStateMachine(const std::string& sessionId,
                        const std::string& deviceSerial,
                        IPipeServer& pipe);
    ~SessionStateMachine();

    void tick();
    void handleCommand(const IpcMessage& msg);
    void stop();
    bool isStopped() const;
    SessionState currentState() const;

private:
    void transitionTo(SessionState next);
    void sendStateEvent(SessionState state) const;
    void sendError(int code, const std::string& message, const std::string& layer) const;
    void sendHealthTick();

    void tickIdle();
    void tickAdbSetup();
    void tickServerInstall();
    void tickTunneling();
    void tickStreaming();
    void tickRecovering();

    std::string    m_sessionId;
    std::string    m_deviceSerial;
    IPipeServer&   m_pipe;
    SessionState   m_state = SessionState::Idle;

    std::unique_ptr<AdbManager>      m_adb;
    std::unique_ptr<AdbSocketClient> m_socketClient;
    std::unique_ptr<H264Decoder>     m_decoder;
    std::unique_ptr<SdlRenderer>     m_renderer;
    std::unique_ptr<AudioPlayer>     m_audioPlayer;

    bool m_firstFrameSent = false;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastHealthTick;

    int  m_recoveryAttempt = 0;
    static constexpr int k_maxRecovery = 5;
    static constexpr int k_videoPort = 27183;
    static constexpr int k_controlPort = 27184;
    static constexpr int k_audioPort = 27185;
};

} // namespace mirra
