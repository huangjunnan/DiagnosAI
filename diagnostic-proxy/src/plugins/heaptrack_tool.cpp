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
#include <utility> // ✅ 添加：std::exchange 定义在这里
#include <vector>

namespace
{
    // RAII临时文件自动清理器（无论是否抛出异常，都会自动删除文件）
    class TempFileCleaner
    {
    public:
        explicit TempFileCleaner(std::string path) : path_(std::move(path)) {}
        ~TempFileCleaner()
        {
            if (!path_.empty())
            {
                std::error_code ec;
                std::filesystem::remove(path_, ec);
                // 同时清理可能的双后缀文件
                std::filesystem::remove(path_ + ".gz", ec);
                std::filesystem::remove(path_ + ".gz.gz", ec);
            }
        }

        // 禁止拷贝
        TempFileCleaner(const TempFileCleaner &) = delete;
        TempFileCleaner &operator=(const TempFileCleaner &) = delete;

        // ✅ 修复：兼容所有C++17编译器的移动构造（不使用std::exchange）
        TempFileCleaner(TempFileCleaner &&other) noexcept
            : path_(other.path_)
        {
            other.path_ = "";
        }

        // ✅ 修复：兼容所有C++17编译器的移动赋值（不使用std::exchange）
        TempFileCleaner &operator=(TempFileCleaner &&other) noexcept
        {
            if (this != &other)
            {
                // 先清理自己的旧文件
                std::error_code ec;
                std::filesystem::remove(path_, ec);
                std::filesystem::remove(path_ + ".gz", ec);
                std::filesystem::remove(path_ + ".gz.gz", ec);

                // 转移所有权
                path_ = other.path_;
                other.path_ = "";
            }
            return *this;
        }

    private:
        std::string path_;
    };

    // 执行shell命令并返回标准输出，修复退出状态判断
    std::string execCommand(const std::string &cmd)
    {
        std::array<char, 128> buffer;
        std::string result;
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe)
        {
            throw std::runtime_error("popen() failed: " + cmd);
        }

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        {
            result += buffer.data();
        }

        int status = pclose(pipe);
        // 正确判断退出状态：只有正常退出且退出码为0才算成功
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
            throw std::runtime_error(errorMsg + ": " + cmd);
        }

        return result;
    }

    // 生成唯一的临时文件前缀（避免多个实例同时运行冲突）
    std::string getUniqueTempPrefix()
    {
        std::string tempPath = "/tmp/diagnosai_heaptrack_XXXXXX";
        int fd = mkstemp(tempPath.data());
        if (fd == -1)
        {
            throw std::runtime_error("Failed to create temporary file");
        }
        close(fd);
        // mkstemp会创建文件，我们先删除它，让heaptrack自己创建
        std::filesystem::remove(tempPath);
        return tempPath;
    }

} // anonymous namespace

// ✅ 修复：去掉未使用的params参数名，消除警告
std::string HeaptrackTool::execute(
    const std::string &target,
    const std::unordered_map<std::string, std::string> &)
{
    // 1. 生成唯一临时文件前缀（彻底解决文件名冲突）
    const std::string outputBase = getUniqueTempPrefix();
    // RAII自动清理：无论函数正常返回还是抛出异常，临时文件都会被删除
    TempFileCleaner cleaner(outputBase);

    // 2. 执行heaptrack采集数据（不再手动加.gz，让heaptrack自己处理）
    std::string heaptrackCmd = "heaptrack -o " + outputBase + " \"" + target + "\"";
    execCommand(heaptrackCmd);

    // 3. 兼容所有版本heaptrack的文件名查找逻辑
    std::string actualFile;
    // 按优先级检查可能的文件名
    const std::vector<std::string> possibleFiles = {
        outputBase + ".gz.gz", // 旧版本自动追加两次.gz
        outputBase + ".gz",    // 标准版本自动追加一次.gz
        outputBase             // 极少数版本不追加后缀
    };

    for (const auto &file : possibleFiles)
    {
        if (std::filesystem::exists(file))
        {
            actualFile = file;
            break;
        }
    }

    if (actualFile.empty())
    {
        throw std::runtime_error(
            "heaptrack output file not found. Checked: " +
            outputBase + ".gz.gz, " + outputBase + ".gz, " + outputBase);
    }

    // 4. 用heaptrack_print提取文本报告
    std::string printCmd = "heaptrack_print \"" + actualFile + "\"";
    std::string result = execCommand(printCmd);

    // 5. 临时文件会在cleaner析构时自动清理，无需手动调用remove
    return result;
}

std::string HeaptrackTool::parseResult(const std::string &rawOutput)
{
    // 原样返回原始输出，后续可在此处过滤冗余信息
    return rawOutput;
}
