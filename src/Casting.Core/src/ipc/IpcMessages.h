// Mirra — IPC Message definitions (shared contract between Casting.Core and Mirra.Shell)
// Wire format: [4-byte LE uint32 length][UTF-8 JSON payload]
// This file defines the canonical type strings and payload structures.

#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace mirra {

// ── Message type constants ───────────────────────────────────────────────────

// Core → Shell
constexpr const char* MSG_STATE_CHANGED       = "StateChanged";
constexpr const char* MSG_FIRST_FRAME         = "FirstFrame";
constexpr const char* MSG_HEALTH_TICK         = "HealthTick";
constexpr const char* MSG_ERROR               = "Error";
constexpr const char* MSG_RECOVERY_ATTEMPT    = "RecoveryAttempt";
constexpr const char* MSG_CORE_HWND           = "CoreHwnd";
constexpr const char* MSG_RECORDING_STARTED   = "RecordingStarted";
constexpr const char* MSG_RECORDING_STOPPED   = "RecordingStopped";
constexpr const char* CMD_SET_CLIPBOARD       = "SetClipboard";

// Shell → Core
constexpr const char* CMD_START_SESSION       = "StartSession";
constexpr const char* CMD_STOP_SESSION        = "StopSession";
constexpr const char* CMD_SEND_INPUT          = "SendInput";
constexpr const char* CMD_SEND_CLIPBOARD      = "SendClipboard";
constexpr const char* CMD_SET_QUALITY         = "SetQuality";
constexpr const char* CMD_TAKE_SCREENSHOT     = "TakeScreenshot";
constexpr const char* CMD_START_RECORDING     = "StartRecording";
constexpr const char* CMD_STOP_RECORDING      = "StopRecording";
constexpr const char* CMD_SET_WINDOW_SIZE     = "SetWindowSize";

// ── Envelope ─────────────────────────────────────────────────────────────────

struct IpcMessage {
    int         version   = 1;
    std::string id;         // UUID per request
    std::string session;    // session UUID
    std::string type;       // message type string
    int64_t     ts = 0;     // monotonic timestamp (ms)
    nlohmann::json payload; // typed payload object

    static IpcMessage fromJson(const nlohmann::json& j) {
        IpcMessage m;
        m.version = j.value("v", 1);
        m.id      = j.value("id", "");
        m.session = j.value("session", "");
        m.type    = j.value("type", "");
        m.ts      = j.value("ts", int64_t{0});
        m.payload = j.contains("payload") ? j["payload"] : nlohmann::json{};
        return m;
    }

    nlohmann::json toJson() const {
        return {
            {"v",       version},
            {"id",      id},
            {"session", session},
            {"type",    type},
            {"ts",      ts},
            {"payload", payload}
        };
    }
};

// ── Payload helpers ───────────────────────────────────────────────────────────

struct PayloadStateChanged {
    std::string state;
    std::string deviceSerial;
    nlohmann::json toJson() const {
        return {{"state", state}, {"deviceSerial", deviceSerial}};
    }
};

struct PayloadFirstFrame {
    int widthPx   = 0;
    int heightPx  = 0;
    int latencyMs = 0;
    nlohmann::json toJson() const {
        return {{"widthPx", widthPx}, {"heightPx", heightPx}, {"latencyMs", latencyMs}};
    }
};

struct PayloadHealthTick {
    float fps              = 0.0f;
    int   decodeMs         = 0;
    int   frameDropCount   = 0;
    int   audioLatencyMs   = 0;
    nlohmann::json toJson() const {
        return {{"fps", fps}, {"decodeMs", decodeMs}, 
                {"frameDropCount", frameDropCount}, {"audioLatencyMs", audioLatencyMs}};
    }
};

struct PayloadError {
    int         code    = 0;
    std::string message;
    std::string layer;  // "adb" | "server" | "decoder" | "renderer" | "ipc"
    nlohmann::json toJson() const {
        return {{"code", code}, {"message", message}, {"layer", layer}};
    }
};

struct PayloadCoreHwnd {
    int64_t hwnd = 0;
    nlohmann::json toJson() const {
        return {{"hwnd", hwnd}};
    }
};

struct PayloadStartSession {
    std::string deviceSerial;
    std::string quality;      // "low" | "medium" | "high"
    std::string audioSource;  // "output" | "microphone" | "none"
};

struct PayloadSendInput {
    std::string eventType;  // "touch_down" | "touch_up" | "touch_move" | "key" | "scroll"
    float x         = 0.0f;
    float y         = 0.0f;
    int   keyCode   = 0;
    int   metaState = 0;
    float scrollX   = 0.0f;
    float scrollY   = 0.0f;
};

struct PayloadSetWindowSize {
    int widthPx  = 0;
    int heightPx = 0;
};

} // namespace mirra
