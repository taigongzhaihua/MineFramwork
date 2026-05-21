#pragma once

// ── DLL 导出宏 ────────────────────────────────────────────────────────────────
// 构建本模块时定义 MINE_BUILDING_MINE_UI_STYLE；
// 消费者在共享库模式下自动获得 dllimport；
// 静态库 / 头文件引用模式下宏为空。

#if defined(MINE_BUILDING_MINE_UI_STYLE)
#    define MINE_UI_STYLE_API __declspec(dllexport)
#elif defined(MINE_BUILD_SHARED)
#    define MINE_UI_STYLE_API __declspec(dllimport)
#else
#    define MINE_UI_STYLE_API
#endif
