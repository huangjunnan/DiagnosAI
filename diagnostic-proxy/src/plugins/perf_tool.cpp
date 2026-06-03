#include "plugins/perf_tool.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>

#include "logger.h"

DEFINE_MODULE_LOG(perf_tool)

namespace
{
    std::string execCommand(const std::string& cmd)
    {
        L_TRACE("执行命令: {}", cmd);
        std::array<char, 128> buffer {};
        std::string result;
        auto pipe_closer = [](FILE* f)
        {
            if (f)
                pclose(f);
        };
        std::unique_ptr<FILE, decltype(pipe_closer)> pipe(popen(cmd.c_str(), "r"), pipe_closer);
        if (!pipe)
        {
            L_ERROR("popen 失败: {}", cmd);
            throw std::runtime_error("popen() failed: " + cmd);
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
            result += buffer.data();
        L_TRACE("命令执行成功，输出大小: {} 字节", result.size());
        return result;
    }
} // namespace

std::string PerfTool::execute(const std::string& target, const std::unordered_map<std::string, std::string>& params)
{
    L_INFO("Perf 诊断开始，目标: {}", target);
    L_DEBUG("Perf 参数数量: {}", params.size());

    std::string cmd = "perf stat \"" + target + "\" 2>&1";
    L_DEBUG("执行 perf 命令: {}", cmd);
    std::string result = execCommand(cmd);
    L_INFO("Perf 诊断完成，输出大小: {} 字节", result.size());

    return result;
}

std::string PerfTool::parseResult(const std::string& rawOutput)
{
    L_DEBUG("Perf 解析输入大小: {} 字节", rawOutput.size());
    return rawOutput;
}
