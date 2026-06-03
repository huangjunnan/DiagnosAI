cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(DiagnosticProxy
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Diagnostic Proxy for system performance analysis"
    HOMEPAGE_URL "https://github.com/huangjunnan/DiagnosAI"
)

# ------------------------------
# 1. 构建类型与ccache
# ------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    message(STATUS "Setting build type to 'Debug' as default")
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# ------------------------------
# 2. 第三方依赖（子模块检查统一为文件存在性）
# ------------------------------
function(require_file path description)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "${description} 缺失！请运行 git submodule update --init")
    endif()
endfunction()

# spdlog（编译为库，使用其目标）
require_file("lib/spdlog/include/spdlog/spdlog.h" "spdlog")
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/spdlog EXCLUDE_FROM_ALL)

# httplib (header-only)
require_file("lib/httplib/httplib.h" "httplib")
add_library(httplib INTERFACE)
add_library(httplib::httplib ALIAS httplib)
target_include_directories(httplib INTERFACE lib/httplib)
target_compile_features(httplib INTERFACE cxx_std_17)
target_link_libraries(httplib INTERFACE pthread)

# nlohmann/json (header-only)
require_file("lib/json/single_include/nlohmann/json.hpp" "nlohmann/json")
add_library(json INTERFACE)
add_library(nlohmann::json ALIAS json)
target_include_directories(json INTERFACE lib/json/single_include)
target_compile_features(json INTERFACE cxx_std_17)

# ------------------------------
# 3. diagnostic_logger 静态库
# ------------------------------
add_library(diagnostic_logger STATIC
    lib/diagnostic_logger/src/logger.cpp
)
target_include_directories(diagnostic_logger PUBLIC
    lib/diagnostic_logger/include
    lib/spdlog/include
)
target_link_libraries(diagnostic_logger PUBLIC spdlog::spdlog nlohmann::json pthread)

# ------------------------------
# 4. 主可执行文件
# ------------------------------
add_executable(diagnostic-proxy
    src/main.cpp
    src/core/server.cpp
    src/plugins/heaptrack_tool.cpp
    src/plugins/perf_tool.cpp
)

target_compile_features(diagnostic-proxy PRIVATE cxx_std_17)
target_include_directories(diagnostic-proxy PRIVATE include)

target_link_libraries(diagnostic-proxy PRIVATE
    httplib::httplib
    nlohmann::json
    diagnostic_logger
)

# ------------------------------
# 5. 编译警告（仅对主目标）
# ------------------------------
if(MSVC)
    target_compile_options(diagnostic-proxy PRIVATE /W4)
else()
    target_compile_options(diagnostic-proxy PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ------------------------------
# 5. 生成版本头文件（代码中可直接使用版本号）
# ------------------------------
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
    @ONLY
)

# ------------------------------
# 6. 安装规则（生产级，支持导出目标）
# ------------------------------
include(GNUInstallDirs)

# 安装可执行文件
install(TARGETS diagnostic-proxy
    EXPORT DiagnosticProxyTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 安装头文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# 安装生成的头文件
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 导出目标（其他项目可通过find_package引用）
install(EXPORT DiagnosticProxyTargets
    FILE DiagnosticProxyTargets.cmake
    NAMESPACE DiagnosticProxy::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DiagnosticProxy
)

# ------------------------------
# 7. RPATH设置（Linux/macOS专用，解决安装后找不到库的问题）
# ------------------------------
if(UNIX AND NOT APPLE)
    set_target_properties(diagnostic-proxy PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN"
        BUILD_RPATH "$ORIGIN/../lib:$ORIGIN"
    )
elseif(APPLE)
    set_target_properties(diagnostic-proxy PROPERTIES
        INSTALL_RPATH "@loader_path/../lib:@loader_path"
        BUILD_RPATH "@loader_path/../lib:@loader_path"
    )
endif()
