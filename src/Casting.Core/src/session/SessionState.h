// Mirra — Session State Definition
// Lifecycle: Idle → AdbSetup → ServerInstall → Tunneling → Streaming → Recovering → Stopped

#pragma once

namespace mirra {

enum class SessionState {
    Idle,
    AdbSetup,
    ServerInstall,
    Tunneling,
    Streaming,
    Recovering,
    Stopped
};

constexpr const char* sessionStateToString(SessionState s) {
    switch (s) {
        case SessionState::Idle:          return "Idle";
        case SessionState::AdbSetup:      return "AdbSetup";
        case SessionState::ServerInstall: return "ServerInstall";
        case SessionState::Tunneling:     return "Tunneling";
        case SessionState::Streaming:     return "Streaming";
        case SessionState::Recovering:    return "Recovering";
        case SessionState::Stopped:       return "Stopped";
        default:                          return "Unknown";
    }
}

} // namespace mirra
