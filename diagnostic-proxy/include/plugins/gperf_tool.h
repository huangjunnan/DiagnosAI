#pragma once
#include "interfaces/idiagnostic_tool.h"

class GperfTool : public IDiagnosticTool
{
public:
    std::string getName() const override
    {
        return "gperf";
    }
    std::string execute(const std::string& target, const std::unordered_map<std::string, std::string>& params) override;
    std::string parseResult(const std::string& rawOutput) override;
};
