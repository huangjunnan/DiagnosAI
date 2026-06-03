#include <unistd.h>

#include <chrono>
#include <iostream>
#include <vector>

int main() {
  std::cout << "[frequent_alloc] PID: " << getpid() << " 频繁分配内存 5 秒"
            << std::endl;
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
    std::vector<int*> ptrs;
    for (int i = 0; i < 10000; ++i) {
      int* p = new int[10];
      ptrs.push_back(p);
    }
    for (auto p : ptrs) delete[] p;
  }
  std::cout << "[frequent_alloc] 结束" << std::endl;
  return 0;
}
