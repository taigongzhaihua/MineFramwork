# MineFramework 构建选项
# 对应 xmake/options.lua

# 体积与功能裁剪预设
set(MINE_PROFILE "std" CACHE STRING "MineFramework 体积与功能裁剪预设 (min/std/full)")
set_property(CACHE MINE_PROFILE PROPERTY STRINGS "min" "std" "full")

# 库类型
option(MINE_BUILD_SHARED "输出共享库而不是静态库" OFF)

# 链接时优化
option(MINE_ENABLE_LTO "启用链接时优化 (LTO)" OFF)

# C++ 特性开关
option(MINE_ENABLE_EXCEPTIONS "为需要的平台边界模块启用异常" OFF)
option(MINE_ENABLE_RTTI "为需要的模块启用 RTTI" OFF)

# 构建选项
option(MINE_BUILD_SAMPLES "构建示例程序" ON)
option(MINE_BUILD_TESTS "构建单元测试" ON)
option(MINE_BUILD_TOOLS "构建工具程序 (mmlc/mmlfmt/mmlls)" ON)

# 图形后端选择
if(WIN32)
    set(MINE_GFX_BACKENDS "d3d12;d3d11" CACHE STRING "图形后端列表")
elseif(APPLE)
    set(MINE_GFX_BACKENDS "metal" CACHE STRING "图形后端列表")
else()
    set(MINE_GFX_BACKENDS "vulkan" CACHE STRING "图形后端列表")
endif()

# TLS 后端选择
set(MINE_TLS_BACKEND "mbedtls" CACHE STRING "TLS 后端选择 (mbedtls/openssl)")
set_property(CACHE MINE_TLS_BACKEND PROPERTY STRINGS "mbedtls" "openssl")

# 热重载支持
option(MINE_ENABLE_HOT_RELOAD "启用 MML 运行期反射与热重载" OFF)

# 模块开关（自动根据 MINE_PROFILE 设置）
# 基础模块
option(MINE_MODULE_CORE "启用 mine.core 模块" ON)
option(MINE_MODULE_CONTAINERS "启用 mine.containers 模块" ON)
option(MINE_MODULE_DIAG "启用 mine.diag 模块" ON)
option(MINE_MODULE_ASYNC "启用 mine.async 模块" ON)
option(MINE_MODULE_IO "启用 mine.io 模块" ON)
option(MINE_MODULE_MATH "启用 mine.math 模块" ON)
option(MINE_MODULE_TEXT "启用 mine.text 模块" ON)
option(MINE_MODULE_REFLECT "启用 mine.reflect 模块" ON)

# 根据 profile 调整默认值
if(MINE_PROFILE STREQUAL "min")
    message(STATUS "使用 'min' 预设：最小体积配置")
    set(MINE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(MINE_ENABLE_HOT_RELOAD OFF CACHE BOOL "" FORCE)
elseif(MINE_PROFILE STREQUAL "full")
    message(STATUS "使用 'full' 预设：完整功能配置")
    set(MINE_BUILD_TOOLS ON CACHE BOOL "" FORCE)
    set(MINE_ENABLE_HOT_RELOAD ON CACHE BOOL "" FORCE)
endif()

# 设置库类型变量
if(MINE_BUILD_SHARED)
    set(MINE_LIBRARY_TYPE SHARED)
else()
    set(MINE_LIBRARY_TYPE STATIC)
endif()
