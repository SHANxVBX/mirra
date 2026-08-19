// Mirra — ADB Manager
// Wraps the bundled adb.exe subprocess to manage device communication.

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

namespace mirra {

enum class AdbDeviceState {
    Unknown,
    Unauthorized,
    Offline,
    Ready
};

struct AdbDevice {
    std::string     serial;
    AdbDeviceState  state = AdbDeviceState::Unknown;
    std::string     model;
};

class AdbManager {
public:
    explicit AdbManager(const std::string& adbPath);
    ~AdbManager();

    // Enumerate connected devices
    std::vector<AdbDevice> listDevices();

    // Static parser to parse `adb devices -l` output into AdbDevice list
    static std::vector<AdbDevice> parseDeviceList(const std::string& rawOutput);

    // Push a file to the device
    bool push(const std::string& serial, const std::string& localPath, const std::string& remotePath);

    // Execute a shell command synchronously and return stdout
    std::string shell(const std::string& serial, const std::string& command);

    // Execute a long-running shell command in a background thread
    void shellAsync(const std::string& serial, const std::string& command, std::function<void(int exitCode, const std::string& output)> onExit = nullptr);

    // Set up a forward tunnel: local TCP port → device port
    bool forward(const std::string& serial, int localPort, int devicePort);

    // Set up a reverse tunnel: device TCP port → local port
    bool reverse(const std::string& serial, int devicePort, int localPort);

    // Remove all forwarding rules for a device (cleanup)
    void removeForwards(const std::string& serial);

    // Stop all asynchronous shell workers
    void stopAsyncWorkers();

    // Get the path to the bundled adb binary
    const std::string& adbPath() const { return m_adbPath; }

private:
    // Run adb with given args; returns stdout, throws on non-zero exit
    std::string run(const std::vector<std::string>& args);

    std::string m_adbPath;
    std::vector<std::thread> m_asyncWorkers;
    std::atomic<bool> m_stopping{false};
};

} // namespace mirra
