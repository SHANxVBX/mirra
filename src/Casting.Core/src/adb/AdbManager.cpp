#include "AdbManager.h"
#include "../diag/DiagLogger.h"
#include <sstream>
#include <stdexcept>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace mirra {

AdbManager::AdbManager(const std::string& adbPath) : m_adbPath(adbPath) {
#ifdef _WIN32
    // Ensure ADB daemon does not spawn a console window if it auto-starts
    _putenv("ADB_SERVER_NO_WINDOW=1");
#else
    setenv("ADB_SERVER_NO_WINDOW", "1", 1);
#endif
}

AdbManager::~AdbManager() {
    stopAsyncWorkers();
}

void AdbManager::stopAsyncWorkers() {
    m_stopping.store(true);
    for (auto& t : m_asyncWorkers) {
        if (t.joinable()) {
            t.detach(); // Subprocesses will exit when adb connection breaks or process terminates
        }
    }
    m_asyncWorkers.clear();
}

// ── run() — execute adb subprocess and capture stdout ─────────────────────────
std::string AdbManager::run(const std::vector<std::string>& args) {
    auto& log = DiagLogger::get();

    // Build command line
    std::string cmdLine = "\"" + m_adbPath + "\"";
    for (const auto& a : args) cmdLine += " " + a;

    log.debug("adb: {}", cmdLine);

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hReadOut = nullptr, hWriteOut = nullptr;
    if (!CreatePipe(&hReadOut, &hWriteOut, &sa, 0)) {
        throw std::runtime_error("Failed to create pipe for adb process");
    }
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdOutput  = hWriteOut;
    si.hStdError   = hWriteOut;

    PROCESS_INFORMATION pi{};
    std::string cmdLineMut = cmdLine;
    BOOL ok = CreateProcessA(
        nullptr, cmdLineMut.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(hWriteOut);

    if (!ok) {
        CloseHandle(hReadOut);
        throw std::runtime_error("Failed to start adb: " + cmdLine);
    }

    // Read output
    std::string output;
    char buf[4096];
    DWORD read = 0;
    while (ReadFile(hReadOut, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
        buf[read] = '\0';
        output += buf;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadOut);

    if (exitCode != 0) {
        log.warn("adb exited with code {}: {}", exitCode, output);
    }
    return output;
#else
    return {};
#endif
}

// ── parseDeviceList ───────────────────────────────────────────────────────────
std::vector<AdbDevice> AdbManager::parseDeviceList(const std::string& rawOutput) {
    std::vector<AdbDevice> devices;
    std::istringstream ss(rawOutput);
    std::string line;
    std::getline(ss, line); // skip header "List of devices attached"

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        AdbDevice dev;
        std::string stateStr;
        ls >> dev.serial >> stateStr;

        if (dev.serial.empty()) continue;

        if (stateStr == "device")            dev.state = AdbDeviceState::Ready;
        else if (stateStr == "unauthorized") dev.state = AdbDeviceState::Unauthorized;
        else if (stateStr == "offline")      dev.state = AdbDeviceState::Offline;
        else                                 dev.state = AdbDeviceState::Unknown;

        // Extract model from "model:XYZ"
        std::string token;
        while (ls >> token) {
            if (token.rfind("model:", 0) == 0) {
                dev.model = token.substr(6);
            }
        }

        devices.push_back(dev);
    }
    return devices;
}

// ── listDevices ───────────────────────────────────────────────────────────────
std::vector<AdbDevice> AdbManager::listDevices() {
    std::string out = run({"devices", "-l"});
    return parseDeviceList(out);
}

// ── push ──────────────────────────────────────────────────────────────────────
bool AdbManager::push(const std::string& serial, const std::string& localPath, const std::string& remotePath) {
    try {
        run({"-s", serial, "push", localPath, remotePath});
        return true;
    } catch (const std::exception& ex) {
        DiagLogger::get().error("adb push failed: {}", ex.what());
        return false;
    }
}

// ── shell ─────────────────────────────────────────────────────────────────────
std::string AdbManager::shell(const std::string& serial, const std::string& command) {
    return run({"-s", serial, "shell", command});
}

// ── shellAsync ────────────────────────────────────────────────────────────────
void AdbManager::shellAsync(const std::string& serial, const std::string& command, std::function<void(int, const std::string&)> onExit) {
    m_asyncWorkers.emplace_back([this, serial, command, onExit]() {
        try {
            std::string out = run({"-s", serial, "shell", command});
            if (onExit) onExit(0, out);
        } catch (const std::exception& ex) {
            if (onExit) onExit(-1, ex.what());
        }
    });
}

// ── forward ───────────────────────────────────────────────────────────────────
bool AdbManager::forward(const std::string& serial, int localPort, int devicePort) {
    try {
        run({"-s", serial, "forward",
             "tcp:" + std::to_string(localPort),
             "tcp:" + std::to_string(devicePort)});
        return true;
    } catch (...) { return false; }
}

// ── reverse ───────────────────────────────────────────────────────────────────
bool AdbManager::reverse(const std::string& serial, int devicePort, int localPort) {
    try {
        run({"-s", serial, "reverse",
             "tcp:" + std::to_string(devicePort),
             "tcp:" + std::to_string(localPort)});
        return true;
    } catch (...) { return false; }
}

// ── removeForwards ────────────────────────────────────────────────────────────
void AdbManager::removeForwards(const std::string& serial) {
    try { run({"-s", serial, "forward", "--remove-all"}); } catch (...) {}
    try { run({"-s", serial, "reverse", "--remove-all"}); } catch (...) {}
}

} // namespace mirra
