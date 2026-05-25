/**
 * @file Control.h
 * @brief 向后兼容转发头 —— Control 已迁移至 mine.ui.layout 模块。
 *
 * 为保持现有 #include <mine/ui/visual/Control.h> 代码可以继续编译，
 * 本文件直接转发到新位置 <mine/ui/layout/Control.h>。
 *
 * 新的继承关系：
 *   UIElement → FrameworkElement → Control  (mine.ui.layout)
 *       └─ Button / TextBlock / ...（mine.ui.controls.*）
 */

#pragma once

// Control 已迁移至 mine.ui.layout，此处转发以保持向后兼容
#include <mine/ui/layout/Control.h>
