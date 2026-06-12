/**
 * @file Api.h
 * @brief mine.io 模块 DLL 导出/导入宏定义。
 */

#pragma once

#if defined(_MSC_VER) || defined(__MINGW32__)
#   if defined(MINE_BUILDING_MINE_IO)
#       define MINE_IO_API __declspec(dllexport)
#   elif defined(MINE_BUILD_SHARED)
#       define MINE_IO_API __declspec(dllimport)
#   else
#       define MINE_IO_API
#   endif
#elif defined(__GNUC__) || defined(__clang__)
#   if defined(MINE_BUILDING_MINE_IO)
#       define MINE_IO_API __attribute__((visibility("default")))
#   else
#       define MINE_IO_API
#   endif
#else
#   define MINE_IO_API
#endif
