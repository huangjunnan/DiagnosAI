#include <unistd.h>

#include <chrono>
#include <iostream>

static inline void busy_spin() {
  volatile long long x = 0;
  for (int i = 0; i < 1000000; ++i) x += i * i;
}

int main() {
  std::cout << "[cpu_hotspin] PID: " << getpid() << " 开始 CPU 忙等 5 秒"
            << std::endl;
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
    busy_spin();
  }
  std::cout << "[cpu_hotspin] 结束" << std::endl;
  return 0;
}
