#include "core/server.h"
#include "plugins/heaptrack_tool.h"
#include <memory>

int main()
{
    auto registry = std::make_shared<ToolRegistry>();
    registry->registerTool(std::make_unique<HeaptrackTool>());
    // 未来扩展：registry->registerTool(std::make_unique<PerfTool>());

    LocalProxyServer server(registry);
    server.start(8080);
    return 0;
}
