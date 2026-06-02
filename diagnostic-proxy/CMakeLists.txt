cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(DiagnosticProxy
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Diagnostic Proxy for system performance analysis"
    HOMEPAGE_URL "https://github.com/your-org/DiagnosAI"
)

# ------------------------------
# 1. 全局基础配置（仅影响当前项目，不污染子模块）
# ------------------------------
# 设置默认构建类型为Debug（如果用户未指定）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
    )
endif()

# 启用ccache（现代写法，仅影响当前项目）
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "C++ compiler launcher")
endif()

# ------------------------------
# 2. 第三方依赖管理（目标隔离，不污染全局）
# ------------------------------
# 检查子模块是否存在（关键！避免晦涩的编译错误）
macro(check_submodule_exists path)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${path}/CMakeLists.txt")
        message(FATAL_ERROR
            "Submodule ${path} not found!\n"
            "Please run: git submodule update --init --recursive"
        )
    endif()
endmacro()

# 2.1 spdlog（编译型依赖）
check_submodule_exists("lib/spdlog")
# 关闭spdlog不必要的编译选项，加快构建速度
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE) # 使用C++17 std::format（如果可用）
add_subdirectory(lib/spdlog EXCLUDE_FROM_ALL) # EXCLUDE_FROM_ALL：不安装spdlog到系统

# 2.2 httplib（header-only依赖）
check_submodule_exists("lib/httplib")
add_library(httplib INTERFACE)
add_library(httplib::httplib ALIAS httplib) # 现代别名，和spdlog用法一致
target_include_directories(httplib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/httplib>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(httplib INTERFACE cxx_std_17) # 强制依赖C++17
target_link_libraries(httplib INTERFACE pthread)

# 2.3 nlohmann/json（header-only依赖）
check_submodule_exists("lib/json")
add_library(json INTERFACE)
add_library(nlohmann::json ALIAS json) # 官方标准别名
target_include_directories(json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/json/single_include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(json INTERFACE cxx_std_17)

# ------------------------------
# 3. 主目标定义（完全目标导向，无全局设置）
# ------------------------------
add_executable(diagnostic-proxy
    src/main.cpp
    src/core/server.cpp
    src/plugins/heaptrack_tool.cpp
    src/plugins/perf_tool.cpp
)

# 设置目标属性（仅影响当前目标，不污染第三方库）
target_compile_features(diagnostic-proxy PRIVATE cxx_std_17) # 替代全局CMAKE_CXX_STANDARD
set_target_properties(diagnostic-proxy PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF # 禁用GNU扩展，保证跨编译器兼容性
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 头文件目录（使用生成器表达式，区分构建和安装阶段）
target_include_directories(diagnostic-proxy PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include> # 用于生成的头文件
)

# 链接依赖（PRIVATE：仅内部使用，不传递给依赖者）
target_link_libraries(diagnostic-proxy PRIVATE
    httplib::httplib
    nlohmann::json
    spdlog::spdlog # 必须使用命名空间目标，避免链接系统库
)

# ------------------------------
# 4. 编译警告（仅应用到自己的代码，不影响第三方库）
# ------------------------------
if(MSVC)
    target_compile_options(diagnostic-proxy PRIVATE
        /W4 # 最高警告等级
        /WX # 警告视为错误（可选，推荐开发阶段开启）
        /wd4100 # 忽略未使用参数警告（和你当前代码兼容）
    )
else()
    target_compile_options(diagnostic-proxy PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow # 检测变量阴影
        -Wnon-virtual-dtor # 检测虚析构函数缺失
        -Wold-style-cast # 检测C风格强制转换
        -Wunused-parameter # 保留未使用参数警告（你当前有两个，建议后续修复）
        # -Werror # 警告视为错误（可选，推荐开发阶段开启）
    )
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
