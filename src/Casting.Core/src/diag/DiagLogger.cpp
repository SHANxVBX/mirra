#include "DiagLogger.h"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>
#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace mirra {

std::shared_ptr<spdlog::logger> DiagLogger::s_logger;

static std::string getLogDir() {
#ifdef _WIN32
    char appData[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData);
    return std::string(appData) + "\\Mirra\\logs";
#else
    return "/tmp/mirra/logs";
#endif
}

void DiagLogger::init(const std::string& sessionId) {
    std::string logDir = getLogDir();
    std::filesystem::create_directories(logDir);

    std::string logPath = logDir + "\\core-" + sessionId + ".log";

    auto fileSink   = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                          logPath, 10 * 1024 * 1024 /* 10MB */, 3);
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    s_logger = std::make_shared<spdlog::logger>(
        "mirra-core",
        spdlog::sinks_init_list{fileSink, consoleSink}
    );

    s_logger->set_level(spdlog::level::debug);
    s_logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e] [%l] [%n] %v");
    spdlog::register_logger(s_logger);
    spdlog::flush_every(std::chrono::seconds(1));
}

spdlog::logger& DiagLogger::get() {
    if (!s_logger) {
        // Fallback logger if init() wasn't called (e.g. in unit tests)
        s_logger = spdlog::stdout_color_mt("mirra-core-fallback");
    }
    return *s_logger;
}

} // namespace mirra
