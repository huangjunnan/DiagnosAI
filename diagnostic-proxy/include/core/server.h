#pragma once
#include <memory>

#include "interfaces/idiagnostic_tool.h"

class LocalProxyServer
{
public:
    explicit LocalProxyServer(std::shared_ptr<ToolRegistry> registry);
    void start(int port);

private:
    std::shared_ptr<ToolRegistry> registry_;
};
