#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace novel {

/// 初始化全局 logger。
/// TUI 应用不能向 stdout/stderr 直接输出，因此默认只写文件（novel.log）。
/// 若 also_stderr = true，则同时向 stderr 输出（调试时可开启，但会破坏 TUI 渲染）。
inline void init_logger(const std::string& log_file = "novel.log",
                        bool also_stderr = false)
{
    std::vector<spdlog::sink_ptr> sinks;

    // 文件 sink（总是启用，方便事后查看日志）
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, /*truncate=*/false);
    file_sink->set_level(spdlog::level::trace);
    sinks.push_back(file_sink);

    if (also_stderr) {
        auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        stderr_sink->set_level(spdlog::level::debug);
        sinks.push_back(stderr_sink);
    }

    auto logger = std::make_shared<spdlog::logger>("fanqie", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    logger->flush_on(spdlog::level::warn); // warn 及以上立即 flush

    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(1));

    spdlog::info("Logger initialized. Log file: {}", log_file);
}

} // namespace novel
