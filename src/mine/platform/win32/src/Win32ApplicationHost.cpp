/**
 * @file Win32ApplicationHost.cpp
 * @brief IApplicationHost 的 Win32 实现及工厂函数。
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// 开启 Per-Monitor DPI v2 支持（需 Windows 10 1607+）
#include <shellscalingapi.h>

#include "Win32ApplicationHostImpl.h"
#include <mine/platform/win32/Win32ApplicationHost.h>
#include <mine/core/Memory.h>

#include <algorithm>

namespace mine::platform::win32 {

// ── Win32ApplicationHostImpl 构造与析构 ──────────────────────────────────────

Win32ApplicationHostImpl::Win32ApplicationHostImpl() {
    // 设置 per-monitor DPI v2 感知（必须在创建任何窗口前调用）
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 注册 MineFramework 窗口类（使用 Win32Window 的静态窗口过程）
    Win32WindowClass::ensure_registered(&Win32Window::window_proc);
}

Win32ApplicationHostImpl::~Win32ApplicationHostImpl() {
    // alive_windows_ 只是弱引用，析构时无需清理（调用方 OwnedPtr 负责销毁窗口）
    alive_windows_.clear();
    // 注销窗口类
    Win32WindowClass::unregister();
}

// ── IApplicationHost::run ────────────────────────────────────────────────────

int Win32ApplicationHostImpl::run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    // WM_QUIT 中的 wParam 是 exit_code
    return static_cast<int>(msg.wParam);
}

// ── IApplicationHost::quit ───────────────────────────────────────────────────

void Win32ApplicationHostImpl::quit(int exit_code) {
    exit_code_ = exit_code;
    // 向消息队列投递 WM_QUIT，结束 GetMessage 循环
    PostQuitMessage(exit_code);
}

// ── IApplicationHost::create_window ─────────────────────────────────────────

core::OwnedPtr<IWindow> Win32ApplicationHostImpl::create_window(
    const WindowDesc& desc) {

    // 通过统一分配器创建 Win32Window 对象
    Win32Window* raw = MINE_NEW(Win32Window, desc,
        [this](Win32Window* w) { on_window_destroyed(w); });

    if (!raw || raw->is_destroyed()) {
        // CreateWindowEx 失败：清理并返回空指针
        if (raw) {
            MINE_DELETE(raw);
        }
        return nullptr;
    }

    // 追踪存活窗口（弱引用，不拥有生命周期）
    alive_windows_.push_back(raw);

    // 返回真正拥有所有权的 OwnedPtr<IWindow>。
    // 由于 Win32Window 是 IWindow 的单继承子类，指针地址相同，
    // typed_deleter<Win32Window> 可以安全地从 void* 还原为 Win32Window*。
    return core::OwnedPtr<IWindow>(
        static_cast<IWindow*>(raw),
        &core::detail::typed_deleter<Win32Window>);
}

void Win32ApplicationHostImpl::on_window_destroyed(Win32Window* window) {
    // 从弱引用追踪列表中移除已销毁的窗口
    alive_windows_.erase(
        std::remove(alive_windows_.begin(), alive_windows_.end(), window),
        alive_windows_.end());

    // 若所有窗口均已关闭，自动退出消息循环
    if (alive_windows_.empty()) {
        PostQuitMessage(0);
    }
}

// ── 服务访问器 ───────────────────────────────────────────────────────────────

IClipboard& Win32ApplicationHostImpl::clipboard() {
    return clipboard_;
}

IScreenManager& Win32ApplicationHostImpl::screens() {
    return screen_manager_;
}

IMEService& Win32ApplicationHostImpl::ime() {
    return ime_service_;
}

// ── 工厂函数 ─────────────────────────────────────────────────────────────────

core::OwnedPtr<IApplicationHost> create_application_host() {
    return core::OwnedPtr<IApplicationHost>(
        MINE_NEW(Win32ApplicationHostImpl),
        &core::detail::typed_deleter<Win32ApplicationHostImpl>);
}

} // namespace mine::platform::win32
