# MineFramework 编译器标志配置
# 对应 xmake/toolchains.lua

# 通用编译器标志
function(mine_set_compiler_flags target)
    # Windows 编码设置：强制 UTF-8
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()

    # MSVC 编译器标志
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4                    # 警告级别 4
            /WX                    # 警告视为错误
            /permissive-          # 严格标准符合模式
            /Zc:__cplusplus       # 正确设置 __cplusplus 宏
            /Zc:inline            # 移除未引用的内联函数
            /Gy                   # 函数级链接
            /Gw                   # 全局数据优化
            /diagnostics:caret    # 改进的诊断格式
            $<$<CONFIG:Release>:/O2>        # Release 优化
            $<$<CONFIG:Release>:/Ob3>       # 激进内联
            $<$<CONFIG:Release>:/GL>        # 全程序优化
        )
        
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:/LTCG>      # 链接时代码生成
            $<$<CONFIG:Release>:/OPT:REF>   # 移除未引用函数
            $<$<CONFIG:Release>:/OPT:ICF=4> # 相同函数折叠
            $<$<CONFIG:Release>:/OPT:LBR>   # 长分支优化
        )
        
        # 异常和 RTTI 控制
        if(NOT MINE_ENABLE_EXCEPTIONS)
            target_compile_definitions(${target} PRIVATE MINE_NO_EXCEPTIONS=1)
            # MSVC 默认启用异常，需显式禁用
            string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            target_compile_options(${target} PRIVATE /EHs-c-)
        endif()
        
        if(NOT MINE_ENABLE_RTTI)
            target_compile_definitions(${target} PRIVATE MINE_NO_RTTI=1)
            target_compile_options(${target} PRIVATE /GR-)
        endif()
        
    # Clang/GCC 编译器标志
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -Wall                  # 常规警告
            -Wextra                # 额外警告
            -Werror                # 警告视为错误
            -Wpedantic             # 严格标准符合
            -ffunction-sections    # 函数独立段
            -fdata-sections        # 数据独立段
            $<$<CONFIG:Release>:-O3>              # Release 优化
            $<$<CONFIG:Release>:-fmerge-all-constants> # 合并常量
        )
        
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:-Wl,--gc-sections>  # 垃圾回收未使用段
        )
        
        # Clang 特定选项
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_link_options(${target} PRIVATE
                $<$<CONFIG:Release>:-Wl,--icf=safe>  # 相同函数折叠
            )
        endif()
        
        # 异常和 RTTI 控制
        if(NOT MINE_ENABLE_EXCEPTIONS)
            target_compile_definitions(${target} PRIVATE MINE_NO_EXCEPTIONS=1)
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif()
        
        if(NOT MINE_ENABLE_RTTI)
            target_compile_definitions(${target} PRIVATE MINE_NO_RTTI=1)
            target_compile_options(${target} PRIVATE -fno-rtti)
        endif()
        
        # 符号可见性控制
        target_compile_options(${target} PRIVATE -fvisibility=hidden)
        set_target_properties(${target} PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )
    endif()
    
    # LTO 支持
    if(MINE_ENABLE_LTO)
        set_target_properties(${target} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION ON
        )
        
        # Clang 使用 ThinLTO
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE -flto=thin)
            target_link_options(${target} PRIVATE -flto=thin)
        endif()
    endif()
    
    # 平台定义
    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            MINE_PLATFORM_WINDOWS=1
            NOMINMAX                # 禁用 Windows.h 的 min/max 宏
            WIN32_LEAN_AND_MEAN     # 精简 Windows 头文件
            _CRT_SECURE_NO_WARNINGS # 禁用 CRT 安全警告
        )
    elseif(APPLE)
        target_compile_definitions(${target} PRIVATE MINE_PLATFORM_MACOS=1)
    elseif(UNIX)
        target_compile_definitions(${target} PRIVATE MINE_PLATFORM_LINUX=1)
    endif()
    
    # 构建类型定义
    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Debug>:MINE_BUILD_DEBUG=1>
        $<$<CONFIG:Release>:MINE_BUILD_RELEASE=1>
    )
    
    # 库类型定义
    if(MINE_BUILD_SHARED)
        target_compile_definitions(${target} PRIVATE MINE_BUILD_SHARED=1)
    else()
        target_compile_definitions(${target} PRIVATE MINE_BUILD_STATIC=1)
    endif()
endfunction()

# 设置全局编译器标志
if(MSVC)
    # UTF-8 源文件和执行字符集
    add_compile_options(/utf-8)
    
    # 并行编译
    add_compile_options(/MP)
    
    # 禁用特定警告
    add_compile_options(
        /wd4251  # DLL 接口警告（使用 PIMPL 时可忽略）
    )
endif()

# 设置 Debug 调试信息格式
if(MSVC)
    # 使用 /Zi 生成完整调试信息（与 CMake 3.25+ /Z7 冲突）
    string(REPLACE "/Z7" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Z7" "/Zi" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
endif()
