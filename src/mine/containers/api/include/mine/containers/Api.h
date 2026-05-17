/**
 * @file Api.h
 * @brief 定义 mine.containers 模块的符号导出宏。
 */

#pragma once

/**
 * @def MINE_CONTAINERS_API
 * @brief 为跨模块公开符号附加平台相关的导出或导入修饰。
 *
 * 纯模板库通常无需此宏，但若将来添加非模板实现（如哈希工具函数），
 * 则需使用此宏标记。
 */

#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_CONTAINERS)
#            define MINE_CONTAINERS_API __declspec(dllexport)
#        else
#            define MINE_CONTAINERS_API __declspec(dllimport)
#        endif
#    else
#        define MINE_CONTAINERS_API
#    endif
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_CONTAINERS_API __attribute__((visibility("default")))
#    else
#        define MINE_CONTAINERS_API
#    endif
#endif
