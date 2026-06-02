cmake_minimum_required(VERSION 3.16)
project(TestApp LANGUAGES CXX)

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

# 主可执行文件（根据实际路径调整）
add_executable(test_app src/main.cpp)

# 编译警告选项
if(MSVC)
    target_compile_options(test_app PRIVATE /W4)
else()
    target_compile_options(test_app PRIVATE -Wall -Wextra -pedantic)
endif()
