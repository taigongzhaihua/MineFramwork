cmake_minimum_required(VERSION 3.25)

project(MineFramework 
    VERSION 0.1.0
    DESCRIPTION "MineUI - 跨平台原生 UI 框架"
    LANGUAGES CXX C
)

# 使用 C++23 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出 compile_commands.json 供 IDE 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含 CMake 模块
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 引入自定义函数和宏
include(MineModule)
include(MineOptions)
include(MineCompilerFlags)
include(MineDependencies)

# 配置第三方依赖（使用国内镜像）
mine_setup_dependencies()

# 添加模块目录
# L0 Foundation
add_subdirectory(src/mine/core)
add_subdirectory(src/mine/containers)
add_subdirectory(src/mine/diag)
add_subdirectory(src/mine/async)
add_subdirectory(src/mine/io)
add_subdirectory(src/mine/math)
add_subdirectory(src/mine/text)
add_subdirectory(src/mine/reflect)

# L1 Platform
add_subdirectory(src/mine/platform/abi)
add_subdirectory(src/mine/platform/win32)

# L2 Graphics
add_subdirectory(src/mine/gfx/rhi)
add_subdirectory(src/mine/gfx/d3d11)

# L3 Composition/2D
add_subdirectory(src/mine/paint)

# L4 UI Core
add_subdirectory(src/mine/ui/property)
add_subdirectory(src/mine/ui/binding)
add_subdirectory(src/mine/ui/event)
add_subdirectory(src/mine/ui/visual)
add_subdirectory(src/mine/ui/input)
add_subdirectory(src/mine/ui/layout)
add_subdirectory(src/mine/ui/window)
add_subdirectory(src/mine/ui/app)
add_subdirectory(src/mine/ui/controls)
add_subdirectory(src/mine/ui/style)
add_subdirectory(src/mine/ui/animation)

# L5 App Services
add_subdirectory(src/mine/di)
add_subdirectory(src/mine/mvvm)
add_subdirectory(src/mine/nav)
add_subdirectory(src/mine/config)
add_subdirectory(src/mine/localization)
add_subdirectory(src/mine/logging)

# 示例程序（可选）
if(MINE_BUILD_SAMPLES)
    # 只添加存在 CMakeLists.txt 的示例目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/00-hello-rect/CMakeLists.txt")
        add_subdirectory(samples/00-hello-rect)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/00-blank-window/CMakeLists.txt")
        add_subdirectory(samples/00-blank-window)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/01-mvvm-binding/CMakeLists.txt")
        add_subdirectory(samples/01-mvvm-binding)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/02-controls-demo/CMakeLists.txt")
        add_subdirectory(samples/02-controls-demo)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/03-custom-chrome/CMakeLists.txt")
        add_subdirectory(samples/03-custom-chrome)
    endif()
endif()

# 工具程序（可选）
if(MINE_BUILD_TOOLS)
    # 只添加存在 CMakeLists.txt 的工具目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/mmlc/CMakeLists.txt")
        add_subdirectory(tools/mmlc)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/mmlfmt/CMakeLists.txt")
        add_subdirectory(tools/mmlfmt)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/mmlls/CMakeLists.txt")
        add_subdirectory(tools/mmlls)
    endif()
endif()

# 测试（可选）
if(MINE_BUILD_TESTS)
    enable_testing()
    include(CTest)
endif()

# 安装配置
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 生成并安装包配置文件
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MineFrameworkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MineFrameworkConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MineFramework
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MineFrameworkConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MineFrameworkConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MineFrameworkConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MineFramework
)

# 打印配置摘要
message(STATUS "")
message(STATUS "MineFramework ${PROJECT_VERSION} 配置摘要")
message(STATUS "==========================================")
message(STATUS "  构建类型:         ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ 标准:         ${CMAKE_CXX_STANDARD}")
message(STATUS "  编译器:           ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  共享库:           ${MINE_BUILD_SHARED}")
message(STATUS "  LTO:              ${MINE_ENABLE_LTO}")
message(STATUS "  异常处理:         ${MINE_ENABLE_EXCEPTIONS}")
message(STATUS "  RTTI:             ${MINE_ENABLE_RTTI}")
message(STATUS "  构建示例:         ${MINE_BUILD_SAMPLES}")
message(STATUS "  构建测试:         ${MINE_BUILD_TESTS}")
message(STATUS "  构建工具:         ${MINE_BUILD_TOOLS}")
message(STATUS "  图形后端:         ${MINE_GFX_BACKENDS}")
message(STATUS "==========================================")
message(STATUS "")
