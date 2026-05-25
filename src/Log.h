#pragma once
#include <spdlog/spdlog.h>

namespace Log {
void Init();
}

// clang-format off
#define LOG_INFO(...)  spdlog::log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info,  __VA_ARGS__)
#define LOG_WARN(...)  spdlog::log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn,  __VA_ARGS__)
#define LOG_ERROR(...) spdlog::log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err,   __VA_ARGS__)
#define LOG_TRACE(...) spdlog::log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__)
// clang-format on
