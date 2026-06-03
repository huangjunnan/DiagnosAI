cmake_minimum_required(VERSION 3.16)
project(PerformanceTests LANGUAGES CXX)

# 启用 ccache（如果可用）
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 公共编译警告接口库
add_library(perf_test_warnings INTERFACE)
if(MSVC)
    target_compile_options(perf_test_warnings INTERFACE /W4)
else()
    target_compile_options(perf_test_warnings INTERFACE -Wall -Wextra -pedantic)
endif()

# 各测试目标
add_executable(cpu_hotspin src/cpu_hotspin.cpp)
target_link_libraries(cpu_hotspin pthread perf_test_warnings)

add_executable(lock_contention src/lock_contention.cpp)
target_link_libraries(lock_contention pthread perf_test_warnings)

add_executable(frequent_alloc src/frequent_alloc.cpp)
target_link_libraries(frequent_alloc perf_test_warnings)

add_executable(memory_leak src/memory_leak.cpp)
target_link_libraries(memory_leak perf_test_warnings)
