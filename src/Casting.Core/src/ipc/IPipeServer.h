// Mirra — IPC Pipe Server Interface
// Abstract interface for the Named Pipe Server to facilitate modularity and unit testing.

#pragma once

#include <functional>
#include "IpcMessages.h"

namespace mirra {

class IPipeServer {
public:
    using MessageCallback = std::function<void(const IpcMessage&)>;

    virtual ~IPipeServer() = default;

    // Start listening for incoming IPC connections
    virtual bool start() = 0;

    // Stop listening and close the pipe connection
    virtual void stop() = 0;

    // Register callback for incoming messages from the shell
    virtual void onMessage(MessageCallback cb) = 0;

    // Send an IPC message to the shell
    virtual void send(const IpcMessage& msg) = 0;

    // Returns whether the pipe is actively running
    virtual bool isRunning() const = 0;
};

} // namespace mirra
