/**
 * @file Orientation.h
 * @brief 排列方向枚举（水平 / 垂直）。
 */

#pragma once

#include <cstdint>

namespace mine::ui {

/**
 * @brief 布局方向，用于 StackPanel 等线性排列容器。
 *
 *   - Horizontal ：子元素沿水平方向依次排列（从左到右）
 *   - Vertical   ：子元素沿垂直方向依次排列（从上到下）
 */
enum class Orientation : uint8_t {
    Horizontal = 0,
    Vertical   = 1,
};

} // namespace mine::ui
