#include "logger.h"
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using json = nlohmann::json;

namespace diagnostic_logger
{

    static spdlog::level::level_enum str_to_level(const std::string &s)
    {
        if (s == "trace")
            return spdlog::level::trace;
        if (s == "debug")
            return spdlog::level::debug;
        if (s == "info")
            return spdlog::level::info;
        if (s == "warn")
            return spdlog::level::warn;
        if (s == "err" || s == "error")
            return spdlog::level::err;
        if (s == "critical")
            return spdlog::level::critical;
        if (s == "off")
            return spdlog::level::off;
        return spdlog::level::info; // 默认
    }

    void init(const std::string &config_path)
    {
        std::ifstream ifs(config_path);
        if (!ifs)
        {
            throw std::runtime_error("Cannot open log config file: " + config_path);
        }

        json config = json::parse(ifs);
        const auto &proxy = config.at("proxy");
        const auto &log_cfg = proxy.at("log");

        // 默认级别
        spdlog::level::level_enum default_level = str_to_level(log_cfg.value("default_level", "info"));

        // 各 logger 独立级别
        std::unordered_map<std::string, spdlog::level::level_enum> logger_levels;
        if (log_cfg.contains("loggers"))
        {
            for (const auto &[name, level_str] : log_cfg["loggers"].items())
            {
                logger_levels[name] = str_to_level(level_str.get<std::string>());
            }
        }

        // 创建 sinks
        std::vector<spdlog::sink_ptr> sinks;
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        sinks.push_back(console_sink);

        std::string log_path = "logs/diagnostic-proxy.log";
        size_t max_size = 5 * 1024 * 1024;
        size_t max_files = 5;
        if (log_cfg.contains("sinks") && log_cfg["sinks"].contains("file"))
        {
            const auto &file_sink_cfg = log_cfg["sinks"]["file"];
            if (file_sink_cfg.value("enabled", true))
            {
                log_path = file_sink_cfg.value("path", log_path);
                max_size = file_sink_cfg.value("max_size", max_size);
                max_files = file_sink_cfg.value("max_files", max_files);
            }
        }
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_path, max_size, max_files);
        file_sink->set_level(spdlog::level::trace);
        sinks.push_back(file_sink);

        // 初始化异步线程池
        spdlog::init_thread_pool(8192, 1);

        // 创建预定义的 logger
        std::vector<std::string> names = {"main", "server", "heaptrack_tool", "perf_tool"};
        for (const auto &name : names)
        {
            auto logger = std::make_shared<spdlog::async_logger>(
                name, sinks.begin(), sinks.end(),
                spdlog::thread_pool(), spdlog::async_overflow_policy::block);
            auto it = logger_levels.find(name);
            logger->set_level(it != logger_levels.end() ? it->second : default_level);
            logger->flush_on(spdlog::level::warn);
            spdlog::register_logger(logger);
        }

        auto main_logger = spdlog::get("main");
        if (main_logger)
        {
            spdlog::set_default_logger(main_logger);
        }
        else
        {
            auto tmp = std::make_shared<spdlog::async_logger>(
                "default", sinks.begin(), sinks.end(),
                spdlog::thread_pool(), spdlog::async_overflow_policy::block);
            tmp->set_level(default_level);
            spdlog::register_logger(tmp);
            spdlog::set_default_logger(tmp);
        }
    }

    std::shared_ptr<spdlog::logger> get_logger(const std::string &name)
    {
        auto logger = spdlog::get(name);
        return logger ? logger : spdlog::default_logger();
    }

} // namespace diagnostic_logger
