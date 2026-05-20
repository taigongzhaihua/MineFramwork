/**
 * @file Api.h
 * @brief mine.ui.visual 模块 DLL 导出/导入宏定义。
 */

#pragma once

#if defined(_MSC_VER) || defined(__MINGW32__)
#   if defined(MINE_BUILDING_MINE_UI_VISUAL)
#       define MINE_UI_VISUAL_API __declspec(dllexport)
#   elif defined(MINE_BUILD_SHARED)
#       define MINE_UI_VISUAL_API __declspec(dllimport)
#   else
#       define MINE_UI_VISUAL_API
#   endif
#elif defined(__GNUC__) || defined(__clang__)
#   if defined(MINE_BUILDING_MINE_UI_VISUAL)
#       define MINE_UI_VISUAL_API __attribute__((visibility("default")))
#   else
#       define MINE_UI_VISUAL_API
#   endif
#else
#   define MINE_UI_VISUAL_API
#endif
