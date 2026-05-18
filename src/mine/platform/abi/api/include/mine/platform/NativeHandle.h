/**
 * @file NativeHandle.h
 * @brief 平台原生窗口句柄的 ABI 安全包装。
 *
 * NativeHandle 是一个不透明 union，仅由对应平台后端解释：
 *   - Windows：ptr 存储 HWND
 *   - macOS：ptr 存储 NSWindow*
 *   - Linux：value 存储 Window (XID) 或 Wayland wl_surface*
 *   - Android：ptr 存储 ANativeWindow*
 *   - iOS：ptr 存储 UIWindow*
 */

#pragma once

#include <cstdint>

namespace mine::platform {

/**
 * @brief 平台原生窗口句柄，ABI 安全的不透明包装。
 * 上层代码应将此值视为不透明标识符，不得直接解引用。
 */
union NativeHandle {
    void*    ptr;    ///< 通用指针型句柄（HWND、NSWindow* 等）
    intptr_t value;  ///< 整数型句柄（XID 等）

    constexpr NativeHandle() noexcept : value{0} {}
    explicit constexpr NativeHandle(void* p) noexcept : ptr{p} {}
    explicit constexpr NativeHandle(intptr_t v) noexcept : value{v} {}

    [[nodiscard]] constexpr bool is_null() const noexcept { return value == 0; }

    [[nodiscard]] constexpr bool operator==(const NativeHandle& other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(const NativeHandle& other) const noexcept {
        return value != other.value;
    }
};

} // namespace mine::platform
