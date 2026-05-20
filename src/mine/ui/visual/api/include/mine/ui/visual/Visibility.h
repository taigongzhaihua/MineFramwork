/**
 * @file Visibility.h
 * @brief UI 元素可见性枚举定义。
 *
 * Visibility 控制元素是否参与渲染和布局：
 *   - Visible：正常显示，参与布局与渲染
 *   - Hidden：不渲染，但仍占据布局空间（WPF 语义）
 *   - Collapsed：不渲染，也不占据布局空间（彻底折叠）
 */

#pragma once

#include <cstdint>

namespace mine::ui {

/**
 * @brief 元素可见性枚举。
 *
 * 与 WPF/Avalonia 语义一致：Collapsed 同时影响布局与渲染，
 * Hidden 仅影响渲染（元素仍占用空间）。
 */
enum class Visibility : uint8_t {
    Visible   = 0, ///< 可见：参与布局与渲染
    Hidden    = 1, ///< 隐藏：占据布局空间但不渲染
    Collapsed = 2, ///< 折叠：不渲染也不占据布局空间
};

} // namespace mine::ui
