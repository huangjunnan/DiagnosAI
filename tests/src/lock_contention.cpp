#include <unistd.h>

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

static std::mutex m;
static long long counter = 0;

void worker(int tid) {
  (void)tid;  // 故意未使用，仅用于区分线程
  for (int i = 0; i < 200000; ++i) {
    std::lock_guard<std::mutex> lock(m);
    counter++;
  }
}

int main() {
  std::cout << "[lock_contention] PID: " << getpid()
            << " 启动 4 个线程竞争互斥锁" << std::endl;
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) threads.emplace_back(worker, i);
  for (auto& t : threads) t.join();
  std::cout << "[lock_contention] 结束, counter=" << counter << std::endl;
  return 0;
}
