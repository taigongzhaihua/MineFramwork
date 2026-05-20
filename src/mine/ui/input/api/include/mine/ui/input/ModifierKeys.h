/**
 * @file ModifierKeys.h
 * @brief 修饰键（Ctrl / Shift / Alt）位域标志枚举。
 */

#pragma once

#include <cstdint>

namespace mine::ui::input {

/// @brief 修饰键位域枚举，支持 |、& 等位运算。
enum class ModifierKeys : uint8_t {
    None  = 0,
    Ctrl  = 1u << 0,
    Shift = 1u << 1,
    Alt   = 1u << 2,
};

[[nodiscard]] inline ModifierKeys operator|(ModifierKeys a, ModifierKeys b) noexcept {
    return static_cast<ModifierKeys>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

[[nodiscard]] inline ModifierKeys operator&(ModifierKeys a, ModifierKeys b) noexcept {
    return static_cast<ModifierKeys>(
        static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

[[nodiscard]] inline ModifierKeys operator~(ModifierKeys a) noexcept {
    return static_cast<ModifierKeys>(~static_cast<uint8_t>(a));
}

inline ModifierKeys& operator|=(ModifierKeys& a, ModifierKeys b) noexcept {
    a = a | b;
    return a;
}

inline ModifierKeys& operator&=(ModifierKeys& a, ModifierKeys b) noexcept {
    a = a & b;
    return a;
}

/// @brief 检查 flags 中是否包含指定标志位。
[[nodiscard]] inline bool has_flag(ModifierKeys flags, ModifierKeys flag) noexcept {
    return (flags & flag) != ModifierKeys::None;
}

} // namespace mine::ui::input
