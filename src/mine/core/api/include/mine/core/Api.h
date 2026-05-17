/**
 * @file Api.h
 * @brief 定义 mine.core 模块的符号导出宏。
 */

#pragma once

/**
 * @def MINE_API
 * @brief 为跨模块公开符号附加平台相关的导出或导入修饰。
 *
 * 在静态库构建下该宏展开为空；在共享库构建下根据当前编译单元是否属于
 * mine.core 模块切换为导出或导入修饰，从而保证 ABI 边界上的符号可见性。
 */

#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_CORE)
#            define MINE_API __declspec(dllexport)
#        else
#            define MINE_API __declspec(dllimport)
#        endif
#    else
#        define MINE_API
#    endif
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_API __attribute__((visibility("default")))
#    else
#        define MINE_API
#    endif
#endif