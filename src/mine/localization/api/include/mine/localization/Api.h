/**
 * @file Api.h
 * @brief mine.localization 模块 DLL 导出宏定义。
 *
 * 在编译该模块自身时（MINE_BUILDING_MINE_LOCALIZATION 已定义），使用 dllexport；
 * 在引用该模块的其他目标中，使用 dllimport。
 */
#pragma once

#if defined(_WIN32)
#   if defined(MINE_BUILDING_MINE_LOCALIZATION)
#       define MINE_LOCALIZATION_API __declspec(dllexport)
#   else
#       define MINE_LOCALIZATION_API __declspec(dllimport)
#   endif
#else
#   if defined(MINE_BUILDING_MINE_LOCALIZATION)
#       define MINE_LOCALIZATION_API __attribute__((visibility("default")))
#   else
#       define MINE_LOCALIZATION_API
#   endif
#endif
