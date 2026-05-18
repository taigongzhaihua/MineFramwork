/**
 * @file Api.h
 * @brief mine.platform.win32 模块 DLL 导出宏。
 */

#pragma once

#ifdef MINE_SHARED_BUILD
    #ifdef MINE_BUILDING_MINE_PLATFORM_WIN32
        #define MINE_PLATFORM_WIN32_API __declspec(dllexport)
    #else
        #define MINE_PLATFORM_WIN32_API __declspec(dllimport)
    #endif
#else
    #define MINE_PLATFORM_WIN32_API
#endif
