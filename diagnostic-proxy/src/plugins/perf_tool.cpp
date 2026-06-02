#include "plugins/perf_tool.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <stdexcept>

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

std::string PerfTool::execute(const std::string &target,
                              const std::unordered_map<std::string, std::string> &params)
{
    // 原型：执行 perf stat
    std::string cmd = "perf stat \"" + target + "\" 2>&1";
    return execCommand(cmd);
}

std::string PerfTool::parseResult(const std::string &rawOutput)
{
    return rawOutput;
}
