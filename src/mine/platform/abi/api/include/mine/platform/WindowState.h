/**
 * @file WindowState.h
 * @brief 窗口显示状态枚举。
 *
 * 对应系统窗口的三种典型状态：
 *   - Normal    ：正常大小（可调整大小）
 *   - Minimized ：最小化到任务栏
 *   - Maximized ：最大化（占满工作区）
 */

#pragma once

namespace mine::platform {

/**
 * @brief 窗口显示状态。
 *
 * 对应 Win32 的 SW_RESTORE / SW_MINIMIZE / SW_MAXIMIZE，
 * 以及 macOS 的 miniaturize / zoom 操作。
 */
enum class WindowState : int {
    /// 正常大小的窗口（可调整大小，居中或上次位置）
    Normal = 0,

    /// 最小化到任务栏（隐藏窗口内容，仅保留任务栏图标）
    Minimized = 1,

    /// 最大化（占满当前工作区，不包含任务栏区域）
    Maximized = 2,
};

} // namespace mine::platform
