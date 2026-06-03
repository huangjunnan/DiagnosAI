cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# ------------------------------
# 提前配置ccache（必须在project()之前）
# ------------------------------
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "Compiler launcher")
endif()

# ------------------------------
# 项目配置
# ------------------------------
project(DiagnosticProxy
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Diagnostic Proxy for system performance analysis"
    HOMEPAGE_URL "https://github.com/huangjunnan/DiagnosAI"
)

# ------------------------------
# 构建类型默认值
# ------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    message(STATUS "Setting build type to 'Debug' as default")
endif()

# ------------------------------
# 生成编译数据库（clang-tidy必需）
# ------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands" FORCE)

# ------------------------------
# 编译警告配置
# ------------------------------
# 公共编译警告目标（应用到所有内部目标）
add_library(compiler_warnings INTERFACE)
if(MSVC)
    target_compile_options(compiler_warnings INTERFACE /W4)
    # 仅Debug模式启用/WX（警告视为错误）
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(compiler_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(compiler_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter  # 允许未使用的参数（常见于接口实现）
    )
    # 仅Debug模式启用-Werror（警告视为错误）
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(compiler_warnings INTERFACE -Werror)
    endif()
endif()

# ------------------------------
# 收集所有源文件（仅用于format目标）
# ------------------------------
file(GLOB_RECURSE ALL_SOURCE_FILES
    src/*.cpp
    include/*.h
    lib/diagnostic_logger/src/*.cpp
    lib/diagnostic_logger/include/*.h
)

# ------------------------------
# 第三方依赖（子模块管理）
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
# 内部库：diagnostic_logger
# ------------------------------
add_library(diagnostic_logger STATIC
    lib/diagnostic_logger/src/logger.cpp
)
target_include_directories(diagnostic_logger PUBLIC
    lib/diagnostic_logger/include
    lib/spdlog/include
)
target_link_libraries(diagnostic_logger PUBLIC
    spdlog::spdlog
    nlohmann::json
    pthread
)
target_link_libraries(diagnostic_logger PRIVATE compiler_warnings)  # 应用编译警告

# ------------------------------
# 主可执行文件
# ------------------------------
add_executable(diagnostic-proxy
    src/main.cpp
    src/core/server.cpp
    src/plugins/heaptrack_tool.cpp
    src/plugins/perf_tool.cpp
    src/plugins/gperf_tool.cpp
)

target_compile_features(diagnostic-proxy PRIVATE cxx_std_17)
target_include_directories(diagnostic-proxy PRIVATE
    include
    # ${CMAKE_CURRENT_BINARY_DIR}/include  # 版本头文件生成后启用
)

target_link_libraries(diagnostic-proxy PRIVATE
    httplib::httplib
    nlohmann::json
    diagnostic_logger
    compiler_warnings  # 应用编译警告
)

# ------------------------------
# 版本头文件生成（已注释，待创建模板文件后启用）
# ------------------------------
# configure_file(
#     ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in
#     ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
#     @ONLY
# )

# ------------------------------
# 代码质量工具目标（lint + format）
# ------------------------------
# 1. lint 目标（使用项目根目录的 .clang-tidy 配置）
find_program(CLANG_TIDY_PROGRAM clang-tidy)

add_custom_target(lint
    COMMENT "Running clang-tidy static analysis"
    VERBATIM
)

if(CLANG_TIDY_PROGRAM)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_PROGRAM}")
    set(CLANG_TIDY_CONFIG "${PROJECT_SOURCE_DIR}/../.clang-tidy")   # 指向根目录

    if(EXISTS "${CLANG_TIDY_CONFIG}")
        message(STATUS "Using clang-tidy config: ${CLANG_TIDY_CONFIG}")
        add_custom_command(TARGET lint POST_BUILD
            COMMAND ${CLANG_TIDY_PROGRAM}
                -p ${CMAKE_BINARY_DIR}
                --config-file="${CLANG_TIDY_CONFIG}"
                ${ALL_SOURCE_FILES}
            COMMENT "✅ clang-tidy analysis completed"
        )
    else()
        message(WARNING "clang-tidy config not found at ${CLANG_TIDY_CONFIG}, using default checks")
        add_custom_command(TARGET lint POST_BUILD
            COMMAND ${CLANG_TIDY_PROGRAM}
                -p ${CMAKE_BINARY_DIR}
                ${ALL_SOURCE_FILES}
            COMMENT "✅ clang-tidy analysis completed (default checks)"
        )
    endif()
else()
    message(WARNING "clang-tidy not found, lint target will only show a warning")
    add_custom_command(TARGET lint POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "⚠️ clang-tidy not installed, skipping static analysis"
        COMMAND ${CMAKE_COMMAND} -E echo "   Install with: sudo apt install clang-tidy"
    )
endif()

# 2. format 目标（使用项目根目录的 .clang-format 配置，Allman 花括号风格）
find_program(CLANG_FORMAT_PROGRAM clang-format)

add_custom_target(format
    COMMENT "Formatting code with clang-format (Allman style)"
    VERBATIM
)

add_custom_target(format-check
    COMMENT "Checking code formatting (dry run)"
    VERBATIM
)

if(CLANG_FORMAT_PROGRAM)
    message(STATUS "Found clang-format: ${CLANG_FORMAT_PROGRAM}")
    # 指向项目根目录的 .clang-format
    set(CLANG_FORMAT_CONFIG "${PROJECT_SOURCE_DIR}/../.clang-format")

    if(EXISTS "${CLANG_FORMAT_CONFIG}")
        message(STATUS "Using clang-format config: ${CLANG_FORMAT_CONFIG}")
    else()
        message(WARNING "clang-format config not found at ${CLANG_FORMAT_CONFIG}, using default style")
    endif()

    # 格式化所有代码
    add_custom_command(TARGET format
        POST_BUILD
        COMMAND ${CLANG_FORMAT_PROGRAM}
            -i
            --style=file
            --verbose
            ${ALL_SOURCE_FILES}
        COMMENT "✅ Code formatting completed (Allman style)"
    )

    # 格式化检查（不修改文件，用于CI）
    add_custom_command(TARGET format-check
        POST_BUILD
        COMMAND ${CLANG_FORMAT_PROGRAM}
            --dry-run
            --Werror
            --style=file
            ${ALL_SOURCE_FILES}
        COMMENT "✅ Code formatting check passed"
    )
else()
    message(WARNING "clang-format not found, format targets disabled")

    add_custom_command(TARGET format
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "⚠️  clang-format not installed, skipping formatting"
        COMMAND ${CMAKE_COMMAND} -E echo "   Install with: sudo apt install clang-format"
    )

    add_custom_command(TARGET format-check
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "⚠️  clang-format not installed, skipping format check"
    )
endif()

# ------------------------------
# 安装规则与RPATH设置（原型阶段已注释，发布时启用）
# ------------------------------
# include(GNUInstallDirs)
#
# # 安装可执行文件
# install(TARGETS diagnostic-proxy
#     EXPORT DiagnosticProxyTargets
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )
#
# # 安装头文件
# install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#     FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
# )
#
# # 安装生成的头文件
# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
# )
#
# # 导出CMake目标
# install(EXPORT DiagnosticProxyTargets
#     FILE DiagnosticProxyTargets.cmake
#     NAMESPACE DiagnosticProxy::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DiagnosticProxy
# )
#
# # RPATH设置（解决安装后库查找问题）
# if(UNIX AND NOT APPLE)
#     set_target_properties(diagnostic-proxy PROPERTY
#         INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN"
#         BUILD_RPATH "$ORIGIN/../lib:$ORIGIN"
#     )
# elseif(APPLE)
#     set_target_properties(diagnostic-proxy PROPERTY
#         INSTALL_RPATH "@loader_path/../lib:@loader_path"
#         BUILD_RPATH "@loader_path/../lib:@loader_path"
#     )
# endif()
