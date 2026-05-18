/**
 * @file Win32Helpers.h
 * @brief Win32 内部工具函数：UTF-8 与 UTF-16 互转、RECT 转换等。
 */

#pragma once

// 在所有 Windows 头文件之前定义，避免宏污染
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mine/core/StringView.h>
#include <mine/math/Rect.h>
#include <mine/math/Size.h>
#include <mine/math/Point.h>

#include <string>

namespace mine::platform::win32 {

/**
 * @brief 将 UTF-8 字符串转换为 UTF-16 宽字符串。
 * @param utf8 输入的 UTF-8 字符串视图
 * @return std::wstring（仅在内部实现中使用，不暴露到公共头）
 */
inline std::wstring utf8_to_utf16(core::StringView utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0);
    if (required <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        result.data(), required);
    return result;
}

/**
 * @brief 将 UTF-16 宽字符串转换为 UTF-8 字符串。
 * @param utf16   输入宽字符指针
 * @param length  字符数（-1 表示 null 终止）
 * @return std::string（UTF-8 编码）
 */
inline std::string utf16_to_utf8(const wchar_t* utf16, int length = -1) {
    if (!utf16 || length == 0) {
        return {};
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, 0,
        utf16, length,
        nullptr, 0,
        nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0,
        utf16, length,
        result.data(), required,
        nullptr, nullptr);
    return result;
}

/// 将 Win32 RECT 转换为 mine::math::Rect（物理像素）
inline math::Rect rect_from_win32(const RECT& r) noexcept {
    return {
        static_cast<float>(r.left),
        static_cast<float>(r.top),
        static_cast<float>(r.right  - r.left),
        static_cast<float>(r.bottom - r.top),
    };
}

/// 将 mine::math::Rect 转换为 Win32 RECT（物理像素）
inline RECT rect_to_win32(math::Rect r) noexcept {
    return {
        static_cast<LONG>(r.left()),
        static_cast<LONG>(r.top()),
        static_cast<LONG>(r.right()),
        static_cast<LONG>(r.bottom()),
    };
}

/// 从 DPI 值计算缩放比（以 96 DPI = 1x 为基准）
inline float dpi_to_scale(float dpi) noexcept {
    return dpi / 96.0f;
}

/// 物理像素转逻辑像素
inline float phys_to_logical(float physical, float dpi) noexcept {
    return physical * 96.0f / dpi;
}

/// 逻辑像素转物理像素
inline float logical_to_phys(float logical, float dpi) noexcept {
    return logical * dpi / 96.0f;
}

} // namespace mine::platform::win32
