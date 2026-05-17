/**
 * @file Api.h
 * @brief 定义 mine.math 模块的符号导出宏。
 */

#pragma once

/**
 * @def MINE_MATH_API
 * @brief 为跨模块公开符号附加平台相关的导出或导入修饰。
 *
 * math 当前主要由头文件内联类型组成；若后续引入非内联实现，
 * 则应使用此宏标记公开符号。
 */

#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_MATH)
#            define MINE_MATH_API __declspec(dllexport)
#        else
#            define MINE_MATH_API __declspec(dllimport)
#        endif
#    else
#        define MINE_MATH_API
#    endif
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_MATH_API __attribute__((visibility("default")))
#    else
#        define MINE_MATH_API
#    endif
#endif