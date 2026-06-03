#include "plugins/gperf_tool.h"

#include <sys/wait.h> // 确保这些宏可用
#include <unistd.h>

#include <array>
#include <cerrno>  // for errno
#include <csignal> // for WIFSIGNALED
#include <cstdio>
#include <cstdlib>
#include <cstdlib> // for WIFEXITED, WEXITSTATUS, WTERMSIG
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#include "logger.h"

DEFINE_MODULE_LOG(gperf_tool)

namespace
{
    // 安全执行命令，与 heaptrack_tool 保持一致
    std::string execCommand(const std::string& cmd)
    {
        L_TRACE("执行命令: {}", cmd);
        std::array<char, 128> buffer {};
        std::string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe)
        {
            L_ERROR("popen 失败: {}", cmd);
            throw std::runtime_error("popen() failed: " + cmd);
        }
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        {
            result += buffer.data();
        }
        int status = pclose(pipe);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            std::string errorMsg;
            if (WIFEXITED(status))
                errorMsg = "Command failed with exit code " + std::to_string(WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
                errorMsg = "Command killed by signal " + std::to_string(WTERMSIG(status));
            else
                errorMsg = "Command terminated abnormally";
            L_ERROR("命令执行失败: {} (命令: {})", errorMsg, cmd);
            throw std::runtime_error(errorMsg + ": " + cmd);
        }
        L_TRACE("命令执行成功，输出大小: {} 字节", result.size());
        return result;
    }
} // namespace

std::string GperfTool::execute(const std::string& target,
                               const std::unordered_map<std::string, std::string>& /*params*/)
{
    L_INFO("Gperf CPU 分析开始，目标: {}", target);

    std::string profilePath = "/tmp/diagnosai_gperf_" + std::to_string(getpid()) + ".prof";

    // RAII 清理
    auto cleaner = std::shared_ptr<void>(nullptr,
                                         [profilePath](...)
                                         {
                                             std::error_code ec;
                                             std::filesystem::remove(profilePath, ec);
                                         });

    // 关键修复：通过 LD_PRELOAD 加载 libprofiler，这样即使 tests 没有链接也能采样
    std::string runCmd = "LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so"
                         " CPUPROFILE=" +
                         profilePath + " \"" + target + "\"";
    L_DEBUG("执行: {}", runCmd);
    execCommand(runCmd);

    if (!std::filesystem::exists(profilePath))
    {
        L_ERROR("未找到 profile 文件: {}", profilePath);
        throw std::runtime_error("Gperf profile not generated");
    }

    // 解析为文本报告
    std::string pprofCmd = "echo 'Using local file " + target + ".' | google-pprof --text --lines --cum \"" + target +
                           "\" \"" + profilePath + "\"";
    L_DEBUG("解析 profile: {}", pprofCmd);
    std::string report = execCommand(pprofCmd);

    // 如果 google-pprof 没有输出，说明采样数据为空（程序运行时间太短或 CPU 空闲）
    if (report.empty())
    {
        report = "CPU profiling completed successfully. However, no samples were collected.\n"
                 "This typically means the program runtime was too short or the CPU was mostly idle.\n"
                 "Consider increasing the program's CPU load or runtime for more meaningful results.";
        L_WARN("google-pprof output was empty, returning status message instead");
    }

    L_INFO("Gperf 分析完成，报告长度: {} 字节", report.size());
    return report;
}

std::string GperfTool::parseResult(const std::string& rawOutput)
{
    return rawOutput;
}
