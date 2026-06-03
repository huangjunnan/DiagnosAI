#pragma once
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace diagnostic_logger
{
    void init(const std::string &config_path = "config/proxy.yaml");
    std::shared_ptr<spdlog::logger> get_logger(const std::string &name = "default");
}

// ======================== 文件级便捷宏 ========================
// 用法：在每个 .cpp 文件顶部放置 DEFINE_MODULE_LOG(模块名)，之后即可使用 L_INFO 等宏
// 示例：
//   #include "logger.h"
//   DEFINE_MODULE_LOG(server)
//   void func() { L_INFO("收到请求"); }

#define DEFINE_MODULE_LOG(module_name)                                                                      \
    static auto _module_logger = diagnostic_logger::get_logger(#module_name);                               \
    template <typename... Args>                                                                             \
    inline void _log_helper(spdlog::level::level_enum lvl, fmt::format_string<Args...> fmt, Args &&...args) \
    {                                                                                                       \
        if (_module_logger->should_log(lvl))                                                                \
            _module_logger->log(lvl, fmt, std::forward<Args>(args)...);                                     \
    }

#define L_TRACE(...) _log_helper(spdlog::level::trace, __VA_ARGS__)
#define L_DEBUG(...) _log_helper(spdlog::level::debug, __VA_ARGS__)
#define L_INFO(...) _log_helper(spdlog::level::info, __VA_ARGS__)
#define L_WARN(...) _log_helper(spdlog::level::warn, __VA_ARGS__)
#define L_ERROR(...) _log_helper(spdlog::level::err, __VA_ARGS__)
#define L_CRITICAL(...) _log_helper(spdlog::level::critical, __VA_ARGS__)
