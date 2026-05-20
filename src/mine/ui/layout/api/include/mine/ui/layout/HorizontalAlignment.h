/**
 * @file HorizontalAlignment.h
 * @brief 水平对齐方式枚举。
 */

#pragma once

#include <cstdint>

namespace mine::ui {

/**
 * @brief 元素在容器水平空间中的对齐方式。
 *
 * 与 WPF HorizontalAlignment 语义一致：
 *   - Left    ：靠左，占用 desired_size.width 宽度
 *   - Center  ：居中，占用 desired_size.width 宽度
 *   - Right   ：靠右，占用 desired_size.width 宽度
 *   - Stretch ：拉伸填满容器可用宽度（Arrange 时宽度 = 容器分配宽度）
 */
enum class HorizontalAlignment : uint8_t {
    Left    = 0,
    Center  = 1,
    Right   = 2,
    Stretch = 3,  ///< 默认值
};

} // namespace mine::ui
