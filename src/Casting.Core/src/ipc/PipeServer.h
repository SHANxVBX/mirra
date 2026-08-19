// Mirra — Named Pipe Server (Windows)
// Listens on \\.\pipe\mirra-core-{sessionId} for the WPF shell to connect.

#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "IPipeServer.h"
#include "IpcMessages.h"

namespace mirra {

class PipeServer : public IPipeServer {
public:
    explicit PipeServer(const std::string& pipeName, const std::string& sessionId);
    ~PipeServer() override;

    // Start listening (blocks until client connects, then serves async)
    bool start() override;

    // Stop and close pipe
    void stop() override;

    // Register callback for incoming messages from the shell
    void onMessage(MessageCallback cb) override;

    // Send a message to the shell (thread-safe)
    void send(const IpcMessage& msg) override;

    bool isRunning() const override { return m_running.load(); }

private:
    void readLoop();

    std::string       m_pipeName;
    std::string       m_sessionId;
    MessageCallback   m_callback;
    std::atomic<bool> m_running{false};
    std::mutex        m_writeMutex;

#ifdef _WIN32
    void* m_hPipe = nullptr;  // HANDLE
#endif

    std::thread m_readThread;
};

} // namespace mirra
