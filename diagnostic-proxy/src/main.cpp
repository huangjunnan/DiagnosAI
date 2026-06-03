#include <filesystem>
#include <memory>

#include "core/server.h"
#include "logger.h"
#include "plugins/gperf_tool.h"
#include "plugins/heaptrack_tool.h"
#include "plugins/perf_tool.h"

int main()
{
    diagnostic_logger::init("config/proxy.json");

    auto log = diagnostic_logger::get_logger("main");
    log->info("Diagnostic Proxy 启动成功");
    log->debug("日志系统已初始化，配置文件: config/proxy.json");

    auto registry = std::make_shared<ToolRegistry>();
    registry->registerTool(std::make_unique<HeaptrackTool>());
    // registry->registerTool(std::make_unique<PerfTool>());
    registry->registerTool(std::make_unique<GperfTool>());
    log->info("已注册工具: heaptrack, gperf");

    LocalProxyServer server(registry);
    server.start(8080);

    log->info("代理已正常退出");
    return 0;
}
