// Mirra — Diagnostic Logger (spdlog wrapper)

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <string>
#include <memory>

namespace mirra {

class DiagLogger {
public:
    static void init(const std::string& sessionId);
    static spdlog::logger& get();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace mirra
