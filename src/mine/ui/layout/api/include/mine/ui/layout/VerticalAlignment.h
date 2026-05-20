/**
 * @file VerticalAlignment.h
 * @brief 垂直对齐方式枚举。
 */

#pragma once

#include <cstdint>

namespace mine::ui {

/**
 * @brief 元素在容器垂直空间中的对齐方式。
 *
 * 与 WPF VerticalAlignment 语义一致：
 *   - Top     ：靠顶部，占用 desired_size.height 高度
 *   - Center  ：居中，占用 desired_size.height 高度
 *   - Bottom  ：靠底部，占用 desired_size.height 高度
 *   - Stretch ：拉伸填满容器可用高度（Arrange 时高度 = 容器分配高度）
 */
enum class VerticalAlignment : uint8_t {
    Top     = 0,
    Center  = 1,
    Bottom  = 2,
    Stretch = 3,  ///< 默认值
};

} // namespace mine::ui
