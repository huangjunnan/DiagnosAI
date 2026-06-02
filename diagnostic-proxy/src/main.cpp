#include "core/server.h"
#include "plugins/heaptrack_tool.h"
#include "plugins/perf_tool.h"
#include <memory>
#include <filesystem> // 新增头文件

int main()
{
    // 固定工作目录，避免 getcwd() 失败
    std::filesystem::current_path("/tmp");

    auto registry = std::make_shared<ToolRegistry>();
    registry->registerTool(std::make_unique<HeaptrackTool>());
    registry->registerTool(std::make_unique<PerfTool>());

    LocalProxyServer server(registry);
    server.start(8080);
    return 0;
}
