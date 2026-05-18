/**
 * @file Win32WindowClass.cpp
 * @brief Win32 WNDCLASSEX 注册实现。
 */

#include "Win32WindowClass.h"

namespace mine::platform::win32 {

bool      Win32WindowClass::registered_ = false;
HINSTANCE Win32WindowClass::hinstance_  = nullptr;

bool Win32WindowClass::ensure_registered(WNDPROC wnd_proc) noexcept {
    if (registered_) {
        return true;
    }

    hinstance_ = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW; // 窗口尺寸改变时强制重绘
    wc.lpfnWndProc   = wnd_proc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hinstance_;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // 不填充背景（由应用程序自己绘制）
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = kWindowClassName;
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);

    if (RegisterClassExW(&wc) == 0) {
        return false;
    }

    registered_ = true;
    return true;
}

void Win32WindowClass::unregister() noexcept {
    if (registered_ && hinstance_) {
        UnregisterClassW(kWindowClassName, hinstance_);
        registered_ = false;
    }
}

HINSTANCE Win32WindowClass::hinstance() noexcept {
    return hinstance_;
}

} // namespace mine::platform::win32
