/**
 * @file MouseButton.h
 * @brief 鼠标按键枚举。
 */

#pragma once

#include <cstdint>

namespace mine::ui::input {

/// @brief 鼠标按键枚举。
/// 值与 WindowEvent::mouse_button 字段约定一致：
///   0=左键, 1=右键, 2=中键, 3=X1, 4=X2
enum class MouseButton : uint8_t {
    None   = 255, ///< 无特定按键（例如 MouseMove、MouseWheel）
    Left   = 0,
    Right  = 1,
    Middle = 2,
    X1     = 3,
    X2     = 4,
};

} // namespace mine::ui::input
