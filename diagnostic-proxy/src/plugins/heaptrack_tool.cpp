#include "plugins/heaptrack_tool.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <stdexcept>
#include <algorithm>
#include <sys/stat.h>
#include <glob.h>
#include <vector>
#include <unordered_map>

static std::string execCommand(const std::string &cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    auto pipe_closer = [](FILE *f)
    { if (f) pclose(f); };
    std::unique_ptr<FILE, decltype(pipe_closer)> pipe(popen(cmd.c_str(), "r"), pipe_closer);
    if (!pipe)
        throw std::runtime_error("popen() failed: " + cmd);
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    return result;
}

static std::string findLatestFile(const std::string &pattern)
{
    glob_t g;
    if (glob(pattern.c_str(), GLOB_TILDE, nullptr, &g) != 0 || g.gl_pathc == 0)
    {
        globfree(&g);
        throw std::runtime_error("No files match pattern: " + pattern);
    }
    std::vector<std::string> files(g.gl_pathv, g.gl_pathv + g.gl_pathc);
    globfree(&g);

    // 找出最新文件，跳过 stat 失败的文件
    std::string latest;
    time_t newest = 0;
    for (const auto &f : files)
    {
        struct stat st{};
        if (stat(f.c_str(), &st) == 0)
        {
            if (st.st_mtime > newest)
            {
                newest = st.st_mtime;
                latest = f;
            }
        }
    }
    if (latest.empty())
        throw std::runtime_error("Cannot determine latest file for pattern: " + pattern);
    return latest;
}

std::string HeaptrackTool::execute(
    const std::string &target,
    const std::unordered_map<std::string, std::string> &params)
{

    std::string cmd = "heaptrack \"" + target + "\"";
    for (const auto &[k, v] : params)
        cmd += " " + k + " " + v;
    // 执行 heaptrack，忽略其标准输出（报告在 .gz 文件中）
    execCommand(cmd + " >/dev/null 2>&1");

    std::string latestGz = findLatestFile("heaptrack.*.gz");
    return execCommand("heaptrack_print \"" + latestGz + "\"");
}

std::string HeaptrackTool::parseResult(const std::string &rawOutput)
{
    return rawOutput; // 原型阶段直接传递
}
