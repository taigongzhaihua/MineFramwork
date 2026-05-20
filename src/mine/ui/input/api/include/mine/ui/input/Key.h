/**
 * @file Key.h
 * @brief 平台无关虚拟键枚举。
 *
 * 键值与 Win32 VK_ 常量对齐（对于大多数按键），
 * 因此可以从 Win32 VK 码直接转换，无需查表。
 */

#pragma once

#include <cstdint>

namespace mine::ui::input {

/// @brief 平台无关虚拟键枚举。
/// 值设计上与 Win32 VK 码对齐，便于直接转换。
enum class Key : uint32_t {
    None = 0,

    // ── 鼠标按键（兼容 WM_KEYDOWN 传入场景，值保留） ──────────────────────────

    // ── 退格、Tab、回车 ────────────────────────────────────────────────────────
    Backspace  = 0x08,
    Tab        = 0x09,
    Enter      = 0x0D,
    Escape     = 0x1B,
    Space      = 0x20,

    // ── 导航键 ─────────────────────────────────────────────────────────────────
    PageUp     = 0x21,
    PageDown   = 0x22,
    End        = 0x23,
    Home       = 0x24,
    Left       = 0x25,
    Up         = 0x26,
    Right      = 0x27,
    Down       = 0x28,
    Insert     = 0x2D,
    Delete     = 0x2E,
    PrintScreen = 0x2C,

    // ── 数字键 0-9（主键盘） ────────────────────────────────────────────────────
    D0 = 0x30, D1, D2, D3, D4, D5, D6, D7, D8, D9,

    // ── 字母键 A-Z ─────────────────────────────────────────────────────────────
    A = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // ── 数字键盘 ────────────────────────────────────────────────────────────────
    NumPad0          = 0x60,
    NumPad1, NumPad2, NumPad3, NumPad4,
    NumPad5, NumPad6, NumPad7, NumPad8, NumPad9,
    NumPadMultiply   = 0x6A,
    NumPadAdd        = 0x6B,
    NumPadSeparator  = 0x6C,
    NumPadSubtract   = 0x6D,
    NumPadDecimal    = 0x6E,
    NumPadDivide     = 0x6F,

    // ── 功能键 F1-F12 ───────────────────────────────────────────────────────────
    F1  = 0x70, F2,  F3,  F4,  F5,  F6,
    F7,  F8,  F9,  F10, F11, F12,

    // ── 锁定键 ─────────────────────────────────────────────────────────────────
    CapsLock    = 0x14,
    NumLock     = 0x90,
    ScrollLock  = 0x91,

    // ── 系统键 ─────────────────────────────────────────────────────────────────
    Pause       = 0x13,

    // ── 修饰键 ─────────────────────────────────────────────────────────────────
    LeftShift   = 0xA0,
    RightShift  = 0xA1,
    LeftCtrl    = 0xA2,
    RightCtrl   = 0xA3,
    LeftAlt     = 0xA4,
    RightAlt    = 0xA5,
};

/// @brief 将 Win32 VK 虚拟键码转换为平台无关的 Key 枚举。
/// 对于大多数按键，Win32 VK 值与 Key 枚举值直接对应，无需查表。
[[nodiscard]] inline Key key_from_win32_vk(uint32_t vk) noexcept {
    return static_cast<Key>(vk);
}

} // namespace mine::ui::input
