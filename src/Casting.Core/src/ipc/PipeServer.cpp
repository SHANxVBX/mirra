#include "PipeServer.h"
#include "MessageFramer.h"
#include "../diag/DiagLogger.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace mirra {

PipeServer::PipeServer(const std::string& pipeName, const std::string& sessionId)
    : m_pipeName(pipeName), m_sessionId(sessionId) {}

PipeServer::~PipeServer() {
    stop();
}

void PipeServer::onMessage(MessageCallback cb) {
    m_callback = std::move(cb);
}

bool PipeServer::start() {
#ifdef _WIN32
    auto& log = DiagLogger::get();
    const std::string fullPipe = R"(\\.\pipe\)" + m_pipeName;
    const std::wstring widePipe(fullPipe.begin(), fullPipe.end());

    m_hPipe = CreateNamedPipeW(
        widePipe.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,          // max instances
        65536,      // out buffer
        65536,      // in buffer
        5000,       // timeout ms
        nullptr
    );

    if (m_hPipe == INVALID_HANDLE_VALUE) {
        log.error("CreateNamedPipe failed: {}", GetLastError());
        m_hPipe = nullptr;
        return false;
    }

    log.info("Named pipe created: {}. Waiting for Shell to connect...", fullPipe);

    // Block until the Shell connects
    BOOL connected = ConnectNamedPipe(m_hPipe, nullptr)
        ? TRUE
        : (GetLastError() == ERROR_PIPE_CONNECTED);

    if (!connected) {
        log.error("ConnectNamedPipe failed: {}", GetLastError());
        CloseHandle(m_hPipe);
        m_hPipe = nullptr;
        return false;
    }

    log.info("Shell connected to named pipe.");
    m_running.store(true);
    m_readThread = std::thread(&PipeServer::readLoop, this);
    return true;
#else
    return false;
#endif
}

void PipeServer::stop() {
    m_running.store(false);
#ifdef _WIN32
    if (m_hPipe) {
        DisconnectNamedPipe(m_hPipe);
        CloseHandle(m_hPipe);
        m_hPipe = nullptr;
    }
#endif
    if (m_readThread.joinable()) {
        m_readThread.join();
    }
}

void PipeServer::readLoop() {
#ifdef _WIN32
    auto& log = DiagLogger::get();
    MessageFramer framer;
    std::vector<uint8_t> buf(4096);

    while (m_running.load()) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(m_hPipe, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, nullptr);

        if (!ok || bytesRead == 0) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED || err == ERROR_HANDLE_EOF) {
                log.warn("Pipe disconnected by Shell.");
            } else if (m_running.load()) {
                log.error("ReadFile on pipe failed: {}", err);
            }
            m_running.store(false);
            break;
        }

        framer.feed(std::span<const uint8_t>{buf.data(), bytesRead});

        while (auto jsonStr = framer.extractMessage()) {
            try {
                auto j = nlohmann::json::parse(*jsonStr);
                IpcMessage msg = IpcMessage::fromJson(j);
                if (m_callback) m_callback(msg);
            } catch (const std::exception& ex) {
                log.error("Failed to parse IPC message: {}", ex.what());
                // Per PRD: malformed IPC must fail the session safely, not crash.
            }
        }
    }
#endif
}

void PipeServer::send(const IpcMessage& msg) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(m_writeMutex);
    auto& log = DiagLogger::get();
    if (!m_hPipe || !m_running.load()) return;

    std::string json = msg.toJson().dump();
    auto frame = MessageFramer::encode(json);

    DWORD written = 0;
    BOOL ok = WriteFile(m_hPipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr);
    if (!ok && m_running.load()) {
        log.error("WriteFile on pipe failed: {}", GetLastError());
    }
#endif
}

} // namespace mirra
