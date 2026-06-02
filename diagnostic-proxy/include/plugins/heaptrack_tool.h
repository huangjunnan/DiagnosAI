#pragma once
#include "interfaces/idiagnostic_tool.h"

class HeaptrackTool : public IDiagnosticTool
{
public:
    std::string getName() const override { return "heaptrack"; }
    std::string execute(
        const std::string &target,
        const std::unordered_map<std::string, std::string> &params) override;
    std::string parseResult(const std::string &rawOutput) override;
};
