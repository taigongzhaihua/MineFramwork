/**
 * @file Api.h
 * @brief 定义 mine.reflect 模块的符号导出宏。
 */

#pragma once

#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_REFLECT)
#            define MINE_REFLECT_API __declspec(dllexport)
#        else
#            define MINE_REFLECT_API __declspec(dllimport)
#        endif
#    else
#        define MINE_REFLECT_API
#    endif
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_REFLECT_API __attribute__((visibility("default")))
#    else
#        define MINE_REFLECT_API
#    endif
#endif
