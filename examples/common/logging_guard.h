#pragma once
/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ----------------------------------------------------------------------
 */

#include <vertexnova/logging/logging.h>

CREATE_VNE_LOGGER_CATEGORY("vnesc.examples")

namespace vne::sc::examples {

/**
 * @brief RAII guard - configures console logging for vnesc examples.
 *
 * Place at the start of `main()` so library log output (pipeline, glslang, etc.)
 * and example messages share the same sink.
 */
class LoggingGuard {
   public:
    LoggingGuard() {
        vne::log::LoggerConfig config;
        config.name = vne::log::kDefaultLoggerName;
        config.sink = vne::log::LogSinkType::eConsole;
        config.console_pattern = "[%l] [%n] %v";
        config.log_level = vne::log::LogLevel::eInfo;
        config.async = false;
        vne::log::Logging::configureLogger(config);
    }

    ~LoggingGuard() { vne::log::Logging::shutdown(); }

    LoggingGuard(const LoggingGuard&) = delete;
    LoggingGuard& operator=(const LoggingGuard&) = delete;
};

}  // namespace vne::sc::examples
