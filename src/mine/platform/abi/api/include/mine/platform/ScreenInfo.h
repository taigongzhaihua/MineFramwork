/**
 * @file ScreenInfo.h
 * @brief 显示器（屏幕）信息描述结构体。
 */

#pragma once

#include <mine/math/Rect.h>

namespace mine::platform {

/**
 * @brief 单个显示器的静态信息快照。
 *
 * 所有坐标和尺寸均为逻辑像素（logical pixel），即已按 DPI 缩放换算后的值。
 * 若需要物理像素值，乘以 scale 即可。
 */
struct ScreenInfo {
    math::Rect bounds{};       ///< 显示器在虚拟桌面上的逻辑像素矩形（含任务栏）
    math::Rect work_area{};    ///< 去掉任务栏/Dock 后的可用工作区（逻辑像素）
    float      dpi{96.0f};     ///< 当前 DPI（Windows 默认 96 = 1x 缩放）
    float      scale{1.0f};    ///< 物理像素 / 逻辑像素缩放比（Retina = 2.0 等）
    bool       is_primary{};   ///< 是否为主显示器
};

} // namespace mine::platform
