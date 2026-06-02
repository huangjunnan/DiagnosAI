cmake_minimum_required(VERSION 3.16)
project(DiagnosticProxy VERSION 0.1.0 LANGUAGES CXX)

# 启用 ccache（如果可用）
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# ---- 基础设置 ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 第三方库：header-only 子模块 ----
# httplib
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/httplib)
target_link_libraries(httplib INTERFACE pthread)   # httplib 需要 pthread

# nlohmann/json（使用 single_include 版本）
add_library(json INTERFACE)
target_include_directories(json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/json/single_include)

# ---- 主可执行文件 ----
add_executable(diagnostic-proxy
    src/main.cpp
    src/core/server.cpp
    src/plugins/heaptrack_tool.cpp
)

target_include_directories(diagnostic-proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(diagnostic-proxy PRIVATE httplib json)

# ---- 编译选项 ----
if(MSVC)
    target_compile_options(diagnostic-proxy PRIVATE /W4)
else()
    target_compile_options(diagnostic-proxy PRIVATE -Wall -Wextra -pedantic)
endif()

# ---- 安装（可选） ----
# install(TARGETS diagnostic-proxy RUNTIME DESTINATION bin)
