// Mirra — Casting.Core entry point
// Spawned by Mirra.Shell with: --session <id> --pipe <pipeName> --device <serial>

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>

#include "diag/DiagLogger.h"
#include "ipc/PipeServer.h"
#include "session/SessionStateMachine.h"

namespace {
    std::atomic<bool> g_shutdown{false};
}

void signalHandler(int /*signal*/) {
    g_shutdown.store(true, std::memory_order_relaxed);
}

struct Args {
    std::string sessionId;
    std::string pipeName;
    std::string deviceSerial;
    bool headless = false;
};

Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--session" && i + 1 < argc)   a.sessionId    = argv[++i];
        else if (arg == "--pipe" && i + 1 < argc)  a.pipeName     = argv[++i];
        else if (arg == "--device" && i + 1 < argc) a.deviceSerial = argv[++i];
        else if (arg == "--headless")               a.headless     = true;
    }
    return a;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    Args args = parseArgs(argc, argv);

    if (args.sessionId.empty() || args.pipeName.empty() || args.deviceSerial.empty()) {
        std::cerr << "[CastingCore] Usage: CastingCore --session <id> --pipe <name> --device <serial>\n";
        return 1;
    }

    // ── Diagnostics ─────────────────────────────────────────────────────────
    mirra::DiagLogger::init(args.sessionId);
    auto& log = mirra::DiagLogger::get();
    log.info("CastingCore starting. session={} device={} pipe={}", 
             args.sessionId, args.deviceSerial, args.pipeName);

    // ── IPC Pipe Server ──────────────────────────────────────────────────────
    mirra::PipeServer pipeServer(args.pipeName, args.sessionId);

    // ── Session State Machine ────────────────────────────────────────────────
    mirra::SessionStateMachine session(args.sessionId, args.deviceSerial, pipeServer);

    // Wire up pipe commands → session
    pipeServer.onMessage([&](const mirra::IpcMessage& msg) {
        session.handleCommand(msg);
    });

    // Start pipe server (blocks until connected, then serves on background thread)
    if (!pipeServer.start()) {
        log.error("Failed to start named pipe server on {}", args.pipeName);
        return 2;
    }

    log.info("Named pipe connected. Waiting for StartSession command.");

    // ── Main loop ────────────────────────────────────────────────────────────
    while (!g_shutdown.load(std::memory_order_relaxed) && !session.isStopped()) {
        session.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    session.stop();
    pipeServer.stop();
    log.info("CastingCore shut down cleanly. session={}", args.sessionId);

    return 0;
}
