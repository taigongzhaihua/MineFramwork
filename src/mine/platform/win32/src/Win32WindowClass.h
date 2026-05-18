/**
 * @file Win32WindowClass.h
 * @brief Win32 WNDCLASSEX 注册与生命周期管理。
 *
 * 每个进程注册一次窗口类，由 Win32WindowClass 单例持有。
 * 所有 Win32Window 实例共用同一个窗口类。
 */

#pragma once

#include "Win32Helpers.h"

namespace mine::platform::win32 {

/// MineFramework 窗口类名（UTF-16）
inline constexpr const wchar_t* kWindowClassName = L"MineFrameworkWindow";

/**
 * @brief Win32 窗口类注册管理器（进程单例）。
 *
 * 首次调用 ensure_registered() 时向系统注册 WNDCLASSEX，
 * 析构时注销（程序退出时自动发生）。
 */
class Win32WindowClass {
public:
    /**
     * @brief 确保窗口类已注册，并返回模块实例句柄。
     * @param wnd_proc 窗口过程函数指针（需在所有窗口创建前设置）
     * @return 注册成功返回 true；重复调用也返回 true
     */
    static bool ensure_registered(WNDPROC wnd_proc) noexcept;

    /// 取消注册窗口类（通常在进程退出时调用）
    static void unregister() noexcept;

    /// 获取模块实例句柄
    static HINSTANCE hinstance() noexcept;

private:
    static bool      registered_;
    static HINSTANCE hinstance_;
};

} // namespace mine::platform::win32
