#include "core/server.h"
#include "plugins/heaptrack_tool.h"
#include "plugins/perf_tool.h"
#include <memory>
#include <filesystem>
#include "logger.h"

int main()
{
    diagnostic_logger::init("config/proxy.json");

    auto log = diagnostic_logger::get_logger("main");
    log->info("Diagnostic Proxy 启动成功");
    log->debug("日志系统已初始化，配置文件: config/proxy.json");

    // 固定工作目录，避免 getcwd() 失败
    // std::filesystem::current_path("/tmp");
    // log->debug("工作目录已切换至 /tmp");

    auto registry = std::make_shared<ToolRegistry>();
    registry->registerTool(std::make_unique<HeaptrackTool>());
    registry->registerTool(std::make_unique<PerfTool>());
    log->info("已注册工具: heaptrack, perf");

    LocalProxyServer server(registry);
    server.start(8080);

    log->info("代理已正常退出");
    return 0;
}
