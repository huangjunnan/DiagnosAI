#include "plugins/heaptrack_tool.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <unistd.h>
#include <vector>
#include "logger.h"

DEFINE_MODULE_LOG(heaptrack_tool)

namespace
{

    std::string execCommand(const std::string &cmd)
    {
        L_TRACE("执行命令: {}", cmd);
        std::array<char, 128> buffer;
        std::string result;
        FILE *pipe = popen(cmd.c_str(), "r");
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
            {
                errorMsg = "Command failed with exit code " + std::to_string(WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status))
            {
                errorMsg = "Command killed by signal " + std::to_string(WTERMSIG(status));
            }
            else
            {
                errorMsg = "Command terminated abnormally";
            }
            L_ERROR("命令执行失败: {} (命令: {})", errorMsg, cmd);
            throw std::runtime_error(errorMsg + ": " + cmd);
        }

        L_TRACE("命令执行成功，输出大小: {} 字节", result.size());
        return result;
    }

    std::string getUniqueTempPrefix()
    {
        std::string tempPath = "/tmp/diagnosai_heaptrack_XXXXXX";
        int fd = mkstemp(tempPath.data());
        if (fd == -1)
        {
            L_ERROR("创建临时文件失败");
            throw std::runtime_error("Failed to create temporary file");
        }
        close(fd);
        std::filesystem::remove(tempPath);
        L_DEBUG("生成临时文件前缀: {}", tempPath);
        return tempPath;
    }

} // anonymous namespace

std::string HeaptrackTool::execute(
    const std::string &target,
    const std::unordered_map<std::string, std::string> & /*params*/)
{

    L_INFO("Heaptrack 诊断开始，目标: {}", target);

    const std::string outputBase = getUniqueTempPrefix();

    // RAII 自动清理临时文件（无论正常返回还是抛出异常）
    auto cleaner = std::shared_ptr<void>(nullptr, [outputBase](...)
                                         {
        std::error_code ec;
        std::filesystem::remove(outputBase, ec);
        std::filesystem::remove(outputBase + ".gz", ec);
        std::filesystem::remove(outputBase + ".gz.gz", ec); });

    // 执行 heaptrack
    std::string heaptrackCmd = "heaptrack -o " + outputBase + " \"" + target + "\"";
    L_DEBUG("执行 heaptrack: {}", heaptrackCmd);
    execCommand(heaptrackCmd);

    // 查找实际输出文件（兼容不同版本 heaptrack 的文件名）
    const std::vector<std::string> possibleFiles = {
        outputBase + ".gz.gz",
        outputBase + ".gz",
        outputBase};
    std::string actualFile;
    for (const auto &file : possibleFiles)
    {
        if (std::filesystem::exists(file))
        {
            actualFile = file;
            L_DEBUG("找到 heaptrack 输出文件: {}", actualFile);
            break;
        }
    }
    if (actualFile.empty())
    {
        L_ERROR("未找到 heaptrack 输出文件，前缀: {}", outputBase);
        throw std::runtime_error("heaptrack output file not found");
    }

    // 生成文本报告
    std::string printCmd = "heaptrack_print \"" + actualFile + "\"";
    L_DEBUG("执行 heaptrack_print: {}", printCmd);
    std::string result = execCommand(printCmd);
    L_INFO("Heaptrack 诊断完成，报告长度: {} 字节", result.size());

    return result;
}

std::string HeaptrackTool::parseResult(const std::string &rawOutput)
{
    L_DEBUG("Heaptrack 解析输入大小: {} 字节", rawOutput.size());
    return rawOutput;
}
