#pragma once
#include "interfaces/idiagnostic_tool.h"
#include <memory>

class LocalProxyServer {
public:
    explicit LocalProxyServer(std::shared_ptr<ToolRegistry> registry);
    void start(int port);

private:
    std::shared_ptr<ToolRegistry> registry_;
};
