#include "plugins/gperf_tool.h"

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "logger.h"

DEFINE_MODULE_LOG(gperf_tool)

namespace {

// 生成唯一文件名前缀，用于临时 profile 文件，避免并发冲突
std::string generateUniqueProfilePrefix() {
  // 获取当前线程 ID 的字符串表示
  std::ostringstream tid_oss;
  tid_oss << std::this_thread::get_id();
  std::string tid_str = tid_oss.str();

  // 使用随机设备生成高熵随机数
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned long long> dis;
  unsigned long long rand_num = dis(gen);

  // 组合成唯一前缀：/tmp/diagnosai_gperf_<tid>_<random>.prof
  return "/tmp/diagnosai_gperf_" + tid_str + "_" + std::to_string(rand_num) +
         ".prof";
}

// 安全执行命令，与 heaptrack_tool 保持一致
std::string execCommand(const std::string& cmd) {
  L_TRACE("执行命令: {}", cmd);
  std::array<char, 128> buffer{};
  std::string result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    L_ERROR("popen 失败: {}", cmd);
    throw std::runtime_error("popen() failed: " + cmd);
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  int status = pclose(pipe);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::string errorMsg;
    if (WIFEXITED(status))
      errorMsg = "Command failed with exit code " +
                 std::to_string(WEXITSTATUS(status));
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

}  // namespace

std::string GperfTool::execute(
    const std::string& target,
    const std::unordered_map<std::string, std::string>& /*params*/) {
  L_INFO("Gperf CPU 分析开始，目标: {}", target);

  // 生成唯一文件名前缀，避免并发冲突
  std::string profilePath = generateUniqueProfilePrefix();

  // RAII 清理器：删除所有以此前缀开头的临时文件
  auto cleaner = std::shared_ptr<void>(nullptr, [profilePath](...) {
    std::error_code ec;
    // 尝试删除可能生成的文件（包括直接的文件名和带后缀的）
    std::filesystem::remove(profilePath, ec);
    // 遍历 /tmp 删除所有匹配前缀的遗留文件
    for (const auto& entry : std::filesystem::directory_iterator("/tmp")) {
      if (entry.is_regular_file() &&
          entry.path().string().find(profilePath) != std::string::npos) {
        std::filesystem::remove(entry.path(), ec);
      }
    }
  });

  // 构造运行命令，通过 LD_PRELOAD 加载 libprofiler，并设置 CPUPROFILE 环境变量
  std::string runCmd =
      "LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so"
      " CPUPROFILE=" +
      profilePath + " \"" + target + "\"";
  L_DEBUG("执行: {}", runCmd);
  execCommand(runCmd);

  // 查找实际生成的 profile 文件（可能存在 .gz 等后缀）
  std::string actualFile;
  for (const auto& entry : std::filesystem::directory_iterator("/tmp")) {
    if (entry.is_regular_file() &&
        entry.path().string().find(profilePath) != std::string::npos) {
      if (actualFile.empty() ||
          entry.last_write_time() >
              std::filesystem::last_write_time(actualFile)) {
        actualFile = entry.path().string();
      }
    }
  }

  if (actualFile.empty()) {
    L_WARN("未找到任何以 {} 开头的 profile 文件（程序可能空闲或运行时间过短）",
           profilePath);
    return "CPU profiling completed. However, no profile file was generated "
           "(program may be too short or idle).";
  }

  L_DEBUG("实际 profile 文件: {}", actualFile);

  // 使用 google-pprof 解析为文本报告
  std::string pprofCmd = "google-pprof --text --lines --cum \"" + target +
                         "\" \"" + actualFile + "\"";
  L_DEBUG("解析 profile: {}", pprofCmd);
  std::string report = execCommand(pprofCmd);

  if (report.empty()) {
    report =
        "CPU profiling completed successfully. However, no samples were "
        "collected.\n"
        "This typically means the program runtime was too short or the CPU was "
        "mostly idle.";
  }

  L_INFO("Gperf 分析完成，报告长度: {} 字节", report.size());
  return report;
}

std::string GperfTool::parseResult(const std::string& rawOutput) {
  return rawOutput;
}
