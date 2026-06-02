#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <stdexcept>

// ========== 场景1：条件提前 return 导致泄漏 ==========
void conditional_early_return_leak(bool need_early_return)
{
    std::cout << "[conditional_early_return_leak] 进入函数，分配 50MB ..." << std::endl;
    int *data = new int[12.5 * 1024 * 1024]; // 约 50 MB

    if (need_early_return)
    {
        std::cout << "[conditional_early_return_leak] 提前 return ! 内存未被释放 !" << std::endl;
        return; // ❌ 这里直接返回，data 泄漏
    }

    // 正常路径
    std::cout << "[conditional_early_return_leak] 正常路径，释放内存。" << std::endl;
    delete[] data;
}

// ========== 场景2：异常导致泄漏 ==========
void exception_leak()
{
    std::cout << "[exception_leak] 进入函数，分配 80MB ..." << std::endl;
    int *data = new int[20 * 1024 * 1024]; // 约 80 MB

    // 模拟一些操作后抛出异常
    std::cout << "[exception_leak] 即将抛出异常 ..." << std::endl;
    throw std::runtime_error("模拟异常");

    // ❌ 下面的代码永远不会执行，data 泄漏
    std::cout << "[exception_leak] 释放内存。" << std::endl;
    delete[] data;
}

// ========== 场景3：正常操作（不泄漏） ==========
void normal_operation()
{
    std::vector<int> v(1000000);
    std::cout << "[normal_operation] 正常操作，内存被正确管理。" << std::endl;
}

// ========== 主函数 ==========
int main()
{
    std::cout << "复杂内存泄漏测试程序启动，PID: " << getpid() << std::endl;

    // --- 演示提前 return 泄漏 ---
    std::cout << "\n>>> 第一次调用 conditional_early_return_leak(true)，触发提前 return 泄漏" << std::endl;
    conditional_early_return_leak(true);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n>>> 第二次调用 conditional_early_return_leak(false)，正常执行" << std::endl;
    conditional_early_return_leak(false);

    // --- 演示异常泄漏 ---
    std::cout << "\n>>> 调用 exception_leak()，将抛出异常导致泄漏" << std::endl;
    try
    {
        exception_leak();
    }
    catch (const std::exception &e)
    {
        std::cout << "[main] 捕获异常: " << e.what() << "，但内存已经泄漏！" << std::endl;
    }

    // --- 穿插正常操作 ---
    normal_operation();

    std::cout << "\n程序结束。" << std::endl;
    return 0;
}
