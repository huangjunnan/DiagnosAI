#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

// ====================== 测试用例命名空间 ======================
namespace LeakTest {

// ====================== 第一类：基础泄漏场景 ======================

/**
 * 场景1：条件提前 return 导致泄漏
 * 预期：need_early_return=true 时泄漏 50MB
 */
void conditionalEarlyReturnLeak(bool needEarlyReturn) {
  std::cout << "[conditional_early_return] 进入函数，分配 50MB ...\n";
  int* data = new int[50 * 1024 * 1024 / sizeof(int)];  // 约 50 MB

  if (needEarlyReturn) {
    std::cout << "[conditional_early_return] 提前 return ! 内存未被释放 !\n";
    return;  // ❌ 泄漏点
  }

  std::cout << "[conditional_early_return] 正常路径，释放内存。\n";
  delete[] data;
}

/**
 * 场景2：异常抛出导致泄漏
 * 预期：永远泄漏 80MB
 */
void exceptionLeak() {
  std::cout << "[exception_leak] 进入函数，分配 80MB ...\n";
  int* data = new int[20 * 1024 * 1024];  // 约 80 MB

  std::cout << "[exception_leak] 即将抛出异常 ...\n";
  throw std::runtime_error("模拟异常");

  // ❌ 永远不会执行，泄漏
  std::cout << "[exception_leak] 释放内存。\n";
  delete[] data;
}

// ====================== 第二类：业务级泄漏场景 ======================

/**
 * 场景3：请求级泄漏（每次请求泄漏1MB）
 * 预期：每调用一次 process() 泄漏 1MB，累积增长
 */
struct HttpRequest {
  void process() {
    char* buf = new char[1024 * 1024];  // 1MB
    memset(buf, 0, 1024 * 1024);
    std::cout << "[HttpRequest] 处理请求，泄漏 1MB\n";
  }
};

void simulateRequestLeak(int count) {
  HttpRequest req;
  for (int i = 0; i < count; ++i) {
    req.process();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::cout << "[request_leak] 完成 " << count << " 次请求，共泄漏 " << count
            << "MB\n";
}

/**
 * 场景4：用户会话泄漏（用户登出后内存未释放）
 * 预期：每个会话泄漏 2MB，累积增长
 */
struct UserSessionLeak {
  char* data = nullptr;

  void login() {
    data = new char[2 * 1024 * 1024];  // 2MB
    memset(data, 0, 2 * 1024 * 1024);
    std::cout << "[UserSessionLeak] 登录，分配 2MB\n";
  }

  void logout() {
    // ❌ 故意不释放 data
    std::cout << "[UserSessionLeak] 登出，但内存未释放（泄漏）\n";
  }
};

void simulateSessionLeak(int count) {
  for (int i = 0; i < count; ++i) {
    UserSessionLeak session;
    session.login();
    session.logout();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
  std::cout << "[session_leak] " << count << " 个会话结束，共泄漏 "
            << (count * 2) << "MB\n";
}

/**
 * 场景5：连接泄漏（连接关闭后缓冲区未释放）
 * 预期：每个连接泄漏 512KB，累积增长
 */
struct TcpConnectionLeak {
  char* buf = nullptr;

  void open() {
    buf = new char[512 * 1024];  // 512KB
    memset(buf, 0, 512 * 1024);
    std::cout << "[TcpConnectionLeak] 打开连接，分配 512KB\n";
  }

  void close() {
    // ❌ 故意不释放 buf
    std::cout << "[TcpConnectionLeak] 关闭连接，但缓冲区泄漏\n";
  }
};

void simulateConnectionLeak(int count) {
  for (int i = 0; i < count; ++i) {
    TcpConnectionLeak conn;
    conn.open();
    conn.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::cout << "[connection_leak] " << count << " 次连接结束，共泄漏 "
            << (count * 512) << "KB\n";
}

// ====================== 第三类：正常非泄漏场景（工具易误报）
// ======================

/**
 * 场景6：全局缓存常驻（程序退出才释放）
 * 预期：分配 4MB，全程常驻，非泄漏
 */
static std::vector<int> globalCache;

void initGlobalCache() {
  globalCache.resize(1024 * 1024, 42);  // 4MB
  std::cout << "[global_cache] 分配 4MB 全局缓存，程序退出才释放\n";
}

/**
 * 场景7：用户会话正常释放
 * 预期：每个会话分配 2MB，登出时正确释放，无泄漏
 */
struct UserSessionNormal {
  char* data = nullptr;

  void login() {
    data = new char[2 * 1024 * 1024];  // 2MB
    memset(data, 0, 2 * 1024 * 1024);
    std::cout << "[UserSessionNormal] 登录，分配 2MB\n";
  }

  void logout() {
    delete[] data;
    data = nullptr;
    std::cout << "[UserSessionNormal] 登出，内存已释放\n";
  }
};

void simulateSessionNormal(int count) {
  for (int i = 0; i < count; ++i) {
    UserSessionNormal session;
    session.login();
    session.logout();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::cout << "[session_normal] " << count << " 个会话正常结束，无泄漏\n";
}

/**
 * 场景8：连接池常驻（析构时释放）
 * 预期：分配 2MB，函数结束时析构释放，非泄漏
 */
struct ConnPool {
  std::vector<char*> pool;

  void init(int count) {
    for (int i = 0; i < count; ++i) {
      char* buf = new char[256 * 1024];  // 256KB
      memset(buf, 0, 256 * 1024);
      pool.push_back(buf);
    }
    std::cout << "[ConnPool] 初始化 " << count << " 个连接，共 "
              << pool.size() * 256 << " KB\n";
  }

  ~ConnPool() {
    for (auto* p : pool) {
      delete[] p;
    }
    std::cout << "[ConnPool] 连接池销毁，所有内存释放\n";
  }
};

void simulateConnectionPool() {
  ConnPool pool;
  pool.init(8);  // 2MB
  std::cout << "[connection_pool] 连接池常驻中...\n";
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // 析构时自动释放
}

/**
 * 场景9：标准库正常操作（vector自动管理）
 * 预期：无泄漏
 */
void normalOperation() {
  std::vector<int> v(1000000);
  std::cout << "[normal_operation] 正常操作，内存被正确管理\n";
}

}  // namespace LeakTest

// ====================== 主函数 ======================
int main() {
  std::cout << "========================================\n";
  std::cout << "  DiagnosAI 标准内存泄漏综合测试用例\n";
  std::cout << "  PID: " << getpid() << "\n";
  std::cout << "========================================\n\n";

  // ---------------------- 工具误报验证区 ----------------------
  std::cout << ">>> 【第一部分：工具易误报的正常场景】\n\n";

  // 1. 标准输出缓冲区（4KB，全局常驻）
  std::cout << "[system_cout] 标准输出测试（触发cout缓冲区分配）\n";

  // 2. 全局缓存（4MB，全局常驻）
  LeakTest::initGlobalCache();

  // 3. 连接池（2MB，析构释放）
  LeakTest::simulateConnectionPool();

  // 4. 正常操作（vector自动管理）
  LeakTest::normalOperation();

  // 5. 用户会话正常释放（无泄漏）
  LeakTest::simulateSessionNormal(2);

  std::cout << "\n>>> 【第二部分：真实内存泄漏场景】\n\n";

  // ---------------------- 基础泄漏验证区 ----------------------
  // 6. 提前return泄漏（50MB）
  LeakTest::conditionalEarlyReturnLeak(true);
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // 7. 异常泄漏（80MB）
  try {
    LeakTest::exceptionLeak();
  } catch (const std::exception& e) {
    std::cout << "[main] 捕获异常: " << e.what() << "，但内存已经泄漏！\n";
  }

  // ---------------------- 业务级泄漏验证区 ----------------------
  // 8. 请求级泄漏（5次，共5MB，累积增长）
  LeakTest::simulateRequestLeak(5);

  // 9. 用户会话泄漏（3次，共6MB，累积增长）
  LeakTest::simulateSessionLeak(3);

  // 10. 连接泄漏（4次，共2MB，累积增长）
  LeakTest::simulateConnectionLeak(4);

  std::cout << "\n========================================\n";
  std::cout << "  所有测试场景执行完毕\n";
  std::cout << "  预期总泄漏：50+80+5+6+2 = 143MB\n";
  std::cout << "  正常常驻：4MB（全局缓存）\n";
  std::cout << "========================================\n";

  // 保持进程运行，方便内存工具采集数据
  std::cout << "\n进程保持运行中，按 Ctrl+C 退出...\n";

  return 0;
}
