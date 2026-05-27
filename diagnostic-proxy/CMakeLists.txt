cmake_minimum_required(VERSION 3.12)
project(DiagnosticProxy VERSION 0.1.0 LANGUAGES CXX)

# 要求 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===== 第三方库 =====
# httplib (header-only, 需要 pthread)
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(httplib INTERFACE pthread)

# nlohmann/json (header-only)
add_library(json INTERFACE)
target_include_directories(json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# ===== 主可执行文件 =====
add_executable(diagnostic-proxy
    src/main.cpp
    src/core/server.cpp
    src/plugins/heaptrack_tool.cpp
)

# 包含目录（自己的头文件）
target_include_directories(diagnostic-proxy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接第三方库
target_link_libraries(diagnostic-proxy PRIVATE httplib json)

# 编译选项建议
if(MSVC)
    target_compile_options(diagnostic-proxy PRIVATE /W4)
else()
    target_compile_options(diagnostic-proxy PRIVATE -Wall -Wextra -pedantic)
endif()

# 安装规则（可选）
install(TARGETS diagnostic-proxy RUNTIME DESTINATION bin)
