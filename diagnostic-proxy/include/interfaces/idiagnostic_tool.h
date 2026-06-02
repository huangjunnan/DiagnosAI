#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

class IDiagnosticTool
{
public:
    virtual ~IDiagnosticTool() = default;
    virtual std::string getName() const = 0;
    virtual std::string execute(
        const std::string &target,
        const std::unordered_map<std::string, std::string> &params) = 0;
    virtual std::string parseResult(const std::string &rawOutput) = 0;
};

class ToolRegistry
{
public:
    void registerTool(std::unique_ptr<IDiagnosticTool> tool)
    {
        tools_[tool->getName()] = std::move(tool);
    }
    IDiagnosticTool *getTool(const std::string &name)
    {
        auto it = tools_.find(name);
        return (it != tools_.end()) ? it->second.get() : nullptr;
    }
    std::vector<std::string> listTools() const
    {
        std::vector<std::string> names;
        for (const auto &pair : tools_)
            names.push_back(pair.first);
        return names;
    }

private:
    std::unordered_map<std::string, std::unique_ptr<IDiagnosticTool>> tools_;
};
