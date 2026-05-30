/**
 * @file WindowCornerPreference.h
 * @brief 窗口圆角首选项枚举，对应平台 DWM/Compositor 圆角能力。
 *
 * 语义：
 *   SystemDefault  — 不主动干预，由操作系统决定（Windows 11 默认圆角）
 *   DoNotRound     — 明确禁用圆角（矩形窗口）
 *   Round          — 系统标准圆角（Windows 11: DWMWCP_ROUND）
 *   RoundSmall     — 系统小圆角（Windows 11: DWMWCP_ROUNDSMALL）
 *
 * 在不支持圆角的操作系统（如 Windows 10）上，所有非 SystemDefault 值均被忽略。
 */

#pragma once

namespace mine::platform {

/**
 * @brief 窗口圆角首选项。
 *
 * 平台无关枚举；Win32 实现层将其映射到 DWM_WINDOW_CORNER_PREFERENCE（Windows 11+）。
 */
enum class WindowCornerPreference : int {
    /// 使用操作系统默认行为，不主动调用平台 API
    SystemDefault = 0,

    /// 明确禁用圆角，窗口保持矩形边角
    DoNotRound = 1,

    /// 系统标准圆角（较大弧度）
    Round = 2,

    /// 系统小圆角（较小弧度）
    RoundSmall = 3,
};

} // namespace mine::platform
