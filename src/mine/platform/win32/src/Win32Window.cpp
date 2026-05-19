/**
 * @file Win32Window.cpp
 * @brief IWindow 的 Win32 HWND 封装实现。
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// 开启 Per-Monitor DPI v2 支持
#include <shellscalingapi.h>

#include <algorithm>
#include <cstring>

#include "Win32Window.h"
#include "Win32WindowClass.h"

namespace mine::platform::win32 {

// ── 构造与销毁 ────────────────────────────────────────────────────────────────

Win32Window::Win32Window(const WindowDesc& desc, WindowDestroyCallback on_destroy,
                         Win32IMEService* ime_service)
    : on_destroy_(std::move(on_destroy))
    , ime_service_(ime_service) {

    // 注册窗口类（首次调用时注册）
    Win32WindowClass::ensure_registered(&Win32Window::window_proc);

    // 根据窗口类型确定基础样式
    DWORD style    = 0;
    DWORD ex_style = 0;

    switch (desc.kind) {
    case WindowKind::Normal:
        // 普通应用窗口：带标题栏/边框/最大最小化按钮，显示在任务栏
        style    = WS_OVERLAPPEDWINDOW;
        ex_style = WS_EX_APPWINDOW;
        break;
    case WindowKind::Tool:
        // 工具窗口：带标题栏但字体较小，不在任务栏显示
        style    = WS_OVERLAPPEDWINDOW;
        ex_style = WS_EX_TOOLWINDOW;
        break;
    case WindowKind::Dialog:
        // 对话框：带标题栏和系统菜单，对话框边框风格，不可调整大小
        style    = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_DLGFRAME;
        ex_style = WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW;
        break;
    case WindowKind::Splash:
        // 启动闪屏：无边框无标题，不在任务栏显示
        style    = WS_POPUP;
        ex_style = WS_EX_TOOLWINDOW;
        break;
    case WindowKind::Popup:
        // 弹出窗口：无边框，不抢键盘焦点，不在任务栏显示
        style    = WS_POPUP;
        ex_style = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        break;
    default:
        style    = WS_OVERLAPPEDWINDOW;
        ex_style = WS_EX_APPWINDOW;
        break;
    }

    // frameless：对有标题栏的 kind（Normal/Tool/Dialog）去掉系统边框，用于自定义绘制
    if (desc.frameless &&
        desc.kind != WindowKind::Popup &&
        desc.kind != WindowKind::Splash) {
        style = (style & ~(WS_CAPTION | WS_DLGFRAME | WS_THICKFRAME)) | WS_POPUP;
    }

    // resizable 仅对含可调整边框的窗口有意义
    if (!desc.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (desc.always_on_top) {
        ex_style |= WS_EX_TOPMOST;
    }
    if (desc.transparent) {
        ex_style |= WS_EX_LAYERED;
    }

    // 保存窗口类型，供后续 show() 等行为判断
    kind_ = desc.kind;

    // 先用临时 DPI=96 估算初始物理尺寸，创建后再根据实际 DPI 调整
    constexpr float kDefaultDpi = 96.0f;
    const int phys_w = static_cast<int>(desc.size.width  * kDefaultDpi / 96.0f);
    const int phys_h = static_cast<int>(desc.size.height * kDefaultDpi / 96.0f);

    // 父窗口句柄
    HWND parent_hwnd = nullptr;
    if (desc.parent_native_handle) {
        parent_hwnd = reinterpret_cast<HWND>(desc.parent_native_handle);
    }

    // 将客户区尺寸换算为窗口尺寸（包含标题栏、边框）
    // 注意：必须先算窗口尺寸，WS_POPUP 居中时需要用到
    RECT window_rect{0, 0, phys_w, phys_h};
    AdjustWindowRectEx(&window_rect, style, FALSE, ex_style);
    const int window_w = window_rect.right  - window_rect.left;
    const int window_h = window_rect.bottom - window_rect.top;

    // 计算窗口初始位置
    // CW_USEDEFAULT 只对 WS_OVERLAPPEDWINDOW 有效；WS_POPUP 窗口须手动居中，
    // 否则部分 Windows 版本会将窗口定位到屏幕外导致不可见。
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (desc.auto_position) {
        if (style & WS_POPUP) {
            // 在主显示器上居中
            const int screen_w = GetSystemMetrics(SM_CXSCREEN);
            const int screen_h = GetSystemMetrics(SM_CYSCREEN);
            x = (screen_w - window_w) / 2;
            y = (screen_h - window_h) / 2;
            if (x < 0) { x = 0; }
            if (y < 0) { y = 0; }
        }
        // else: WS_OVERLAPPEDWINDOW 使用系统默认级联位置（CW_USEDEFAULT）
    } else {
        x = static_cast<int>(desc.position.x * kDefaultDpi / 96.0f);
        y = static_cast<int>(desc.position.y * kDefaultDpi / 96.0f);
    }

    // 将 UTF-8 标题转为 UTF-16
    const std::wstring title_w = utf8_to_utf16(desc.title);

    // 创建窗口（此时 WM_CREATE 会被触发）
    hwnd_ = CreateWindowExW(
        ex_style,
        kWindowClassName,
        title_w.c_str(),
        style,
        x, y,
        window_w, window_h,
        parent_hwnd,
        nullptr,
        Win32WindowClass::hinstance(),
        this                // 将 this 指针通过 CreateParams 传递给 WM_CREATE
    );

    if (!hwnd_) {
        return; // 创建失败，hwnd_ 保持 nullptr
    }

    // 查询实际 DPI 并根据真实 DPI 调整窗口尺寸
    dpi_ = query_dpi();
    if (dpi_ != kDefaultDpi) {
        const int real_w = static_cast<int>(desc.size.width  * dpi_ / 96.0f);
        const int real_h = static_cast<int>(desc.size.height * dpi_ / 96.0f);
        RECT real_rect{0, 0, real_w, real_h};
        AdjustWindowRectEx(&real_rect, style, FALSE, ex_style);
        SetWindowPos(hwnd_, nullptr, 0, 0,
                     real_rect.right  - real_rect.left,
                     real_rect.bottom - real_rect.top,
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }
}

Win32Window::~Win32Window() {
    if (hwnd_) {
        // 强制销毁（通常不会走到这里，因为 WM_DESTROY 已经发生）
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

// ── IWindow 接口实现 ──────────────────────────────────────────────────────────

void Win32Window::show() {
    if (!hwnd_) {
        return;
    }
    // Popup/Splash 不应抢占键盘焦点；其余类型激活并显示
    const int cmd = (kind_ == WindowKind::Popup || kind_ == WindowKind::Splash)
                        ? SW_SHOWNOACTIVATE
                        : SW_SHOWNORMAL;
    ShowWindow(hwnd_, cmd);
    UpdateWindow(hwnd_);
}

void Win32Window::hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void Win32Window::close() {
    if (hwnd_) {
        // 发送 WM_CLOSE，允许事件处理器取消关闭
        SendMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
}

void Win32Window::set_title(core::StringView title) {
    if (hwnd_) {
        const std::wstring title_w = utf8_to_utf16(title);
        SetWindowTextW(hwnd_, title_w.c_str());
    }
}

void Win32Window::set_size(math::Size size) {
    if (!hwnd_) {
        return;
    }
    // 逻辑像素转物理像素
    const int phys_w = static_cast<int>(size.width  * dpi_ / 96.0f);
    const int phys_h = static_cast<int>(size.height * dpi_ / 96.0f);

    // 获取当前窗口样式，以便正确计算含边框的窗口矩形
    const DWORD style    = static_cast<DWORD>(GetWindowLongW(hwnd_, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLongW(hwnd_, GWL_EXSTYLE));
    RECT rect{0, 0, phys_w, phys_h};
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);

    SetWindowPos(hwnd_, nullptr, 0, 0,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

void Win32Window::set_position(math::Point position) {
    if (!hwnd_) {
        return;
    }
    const int phys_x = static_cast<int>(position.x * dpi_ / 96.0f);
    const int phys_y = static_cast<int>(position.y * dpi_ / 96.0f);
    SetWindowPos(hwnd_, nullptr, phys_x, phys_y, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

math::Size Win32Window::size() const {
    return client_size_logical();
}

math::Point Win32Window::position() const {
    return window_position_logical();
}

float Win32Window::dpi() const {
    return dpi_;
}

bool Win32Window::is_visible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

NativeHandle Win32Window::native_handle() const {
    return NativeHandle(reinterpret_cast<void*>(hwnd_));
}

IWindowEventSource& Win32Window::events() {
    return event_source_;
}

// ── 内部辅助函数 ──────────────────────────────────────────────────────────────

float Win32Window::query_dpi() const noexcept {
    if (!hwnd_) {
        return 96.0f;
    }
    // GetDpiForWindow 需要 Windows 10 1607+
    const UINT dpi = GetDpiForWindow(hwnd_);
    return dpi > 0 ? static_cast<float>(dpi) : 96.0f;
}

math::Size Win32Window::client_size_logical() const noexcept {
    if (!hwnd_) {
        return {};
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const float w = phys_to_logical(
        static_cast<float>(client.right  - client.left), dpi_);
    const float h = phys_to_logical(
        static_cast<float>(client.bottom - client.top), dpi_);
    return {w, h};
}

math::Point Win32Window::window_position_logical() const noexcept {
    if (!hwnd_) {
        return {};
    }
    RECT window{};
    GetWindowRect(hwnd_, &window);
    const float x = phys_to_logical(static_cast<float>(window.left), dpi_);
    const float y = phys_to_logical(static_cast<float>(window.top),  dpi_);
    return {x, y};
}

// ── 静态窗口过程 ──────────────────────────────────────────────────────────────

LRESULT CALLBACK Win32Window::window_proc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {

    Win32Window* self = nullptr;

    if (msg == WM_NCCREATE) {
        // CreateWindowEx 的最后一个参数（lpParam）就是 this 指针
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<Win32Window*>(cs->lpCreateParams);
        // 将 this 存入 GWLP_USERDATA，后续消息通过此获取 self
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
        // 此时对象的 hwnd_ 尚未赋值，临时设置以便后续辅助函数可用
        if (self) {
            self->hwnd_ = hwnd;
        }
    } else {
        self = reinterpret_cast<Win32Window*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handle_message(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// ── 消息处理 ──────────────────────────────────────────────────────────────────

LRESULT Win32Window::handle_message(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {

    switch (msg) {
    // 擦除背景：填充系统窗口颜色，防止客户区像素未定义（尤其对无边框的 Popup/Splash）。
    // 图形引擎（GFX 层）接管后，应用层可拦截此消息并返回 1 以跳过填充。
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(reinterpret_cast<HDC>(wparam), &client,
                 reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        return 1; // 非零表示背景已擦除，阻止系统再次擦除
    }

    // 窗口尺寸改变
    case WM_SIZE: {
        if (wparam == SIZE_MINIMIZED) {
            break; // 最小化时不触发 Resized 事件
        }
        WindowEvent ev{};
        ev.kind     = WindowEventKind::Resized;
        ev.new_size = client_size_logical();
        event_source_.fire(ev);
        break;
    }

    // 窗口位置改变
    case WM_MOVE: {
        WindowEvent ev{};
        ev.kind         = WindowEventKind::Moved;
        ev.new_position = window_position_logical();
        event_source_.fire(ev);
        break;
    }

    // 窗口关闭请求（用户点击 X 或调用 close()）
    case WM_CLOSE: {
        WindowEvent ev{};
        ev.kind   = WindowEventKind::Closing;
        ev.cancel = false;
        event_source_.fire(ev);
        if (!ev.cancel) {
            // 未被取消：销毁窗口
            DestroyWindow(hwnd);
        }
        return 0; // 不再传给 DefWindowProc（默认会直接 DestroyWindow）
    }

    // 窗口已被销毁
    case WM_DESTROY: {
        // 触发 Closed 事件
        WindowEvent ev{};
        ev.kind = WindowEventKind::Closed;
        event_source_.fire(ev);

        // 清空 HWND 引用并通知宿主
        hwnd_ = nullptr;
        if (on_destroy_) {
            on_destroy_(this);
        }
        break;
    }

    // 获得焦点
    case WM_SETFOCUS: {
        // 通知 IME 服务：当前活跃窗口更新
        if (ime_service_) {
            ime_service_->on_focus(hwnd);
        }
        WindowEvent ev{};
        ev.kind = WindowEventKind::FocusGained;
        event_source_.fire(ev);
        break;
    }

    // 失去焦点
    case WM_KILLFOCUS: {
        // 通知 IME 服务：清除活跃窗口
        if (ime_service_) {
            ime_service_->on_blur();
        }
        WindowEvent ev{};
        ev.kind = WindowEventKind::FocusLost;
        event_source_.fire(ev);
        break;
    }

    // 窗口激活/反激活
    case WM_ACTIVATE: {
        const bool activated = (LOWORD(wparam) != WA_INACTIVE);
        WindowEvent ev{};
        ev.kind = activated ? WindowEventKind::Activated
                            : WindowEventKind::Deactivated;
        event_source_.fire(ev);
        break;
    }

    // DPI 改变（per-monitor DPI v2）
    case WM_DPICHANGED: {
        const float new_dpi = static_cast<float>(HIWORD(wparam));
        dpi_ = new_dpi;

        // 系统建议的新窗口矩形（已根据新 DPI 缩放）
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);

        WindowEvent ev{};
        ev.kind           = WindowEventKind::DpiChanged;
        ev.new_dpi        = new_dpi;
        ev.suggested_rect = rect_from_win32(*suggested);
        event_source_.fire(ev);

        // 将窗口移动并调整到系统建议的位置与大小
        SetWindowPos(hwnd, nullptr,
                     suggested->left, suggested->top,
                     suggested->right  - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    // ── IME 输入法消息 ──────────────────────────────────────────────────────

    // IME 上下文激活/切换：交给系统默认处理（显示系统 IME UI）
    case WM_IME_SETCONTEXT:
        // lParam 控制系统 IME 子窗口（候选框、组合窗口等）的显示；
        // 直接转发给 DefWindowProcW 使用系统默认 IME 界面。
        return DefWindowProcW(hwnd, msg, wparam, lparam);

    // IME 组合输入开始（用户开始拼写，尚未确认）
    case WM_IME_STARTCOMPOSITION: {
        WindowEvent ev{};
        ev.kind = WindowEventKind::ImeCompositionStarted;
        event_source_.fire(ev);
        // 传递给系统让 IME 继续显示默认组合窗口
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    // IME 组合字符串更新或确认提交
    case WM_IME_COMPOSITION: {
        HIMC himc = ImmGetContext(hwnd);
        if (himc) {
            // GCS_COMPSTR：当前预编辑（拼写中）字符串
            if (lparam & GCS_COMPSTR) {
                const LONG byte_len = ImmGetCompositionStringW(
                    himc, GCS_COMPSTR, nullptr, 0);
                if (byte_len > 0) {
                    std::wstring comp_buf(
                        static_cast<size_t>(byte_len) / sizeof(wchar_t), L'\0');
                    ImmGetCompositionStringW(himc, GCS_COMPSTR,
                        comp_buf.data(), static_cast<DWORD>(byte_len));
                    const std::string utf8 = utf16_to_utf8(
                        comp_buf.data(), static_cast<int>(comp_buf.size()));
                    WindowEvent ev{};
                    ev.kind = WindowEventKind::ImeCompositionChanged;
                    const size_t copy_len = std::min(
                        utf8.size(), sizeof(ev.ime_text_utf8) - 1);
                    std::memcpy(ev.ime_text_utf8, utf8.data(), copy_len);
                    ev.ime_text_utf8[copy_len] = '\0';
                    ev.ime_text_length = static_cast<uint32_t>(copy_len);
                    event_source_.fire(ev);
                }
            }

            // GCS_RESULTSTR：用户最终确认提交的字符串
            if (lparam & GCS_RESULTSTR) {
                const LONG byte_len = ImmGetCompositionStringW(
                    himc, GCS_RESULTSTR, nullptr, 0);
                if (byte_len > 0) {
                    std::wstring result_buf(
                        static_cast<size_t>(byte_len) / sizeof(wchar_t), L'\0');
                    ImmGetCompositionStringW(himc, GCS_RESULTSTR,
                        result_buf.data(), static_cast<DWORD>(byte_len));
                    const std::string utf8 = utf16_to_utf8(
                        result_buf.data(), static_cast<int>(result_buf.size()));
                    WindowEvent ev{};
                    ev.kind = WindowEventKind::ImeCompositionCommitted;
                    const size_t copy_len = std::min(
                        utf8.size(), sizeof(ev.ime_text_utf8) - 1);
                    std::memcpy(ev.ime_text_utf8, utf8.data(), copy_len);
                    ev.ime_text_utf8[copy_len] = '\0';
                    ev.ime_text_length = static_cast<uint32_t>(copy_len);
                    event_source_.fire(ev);
                }
            }

            ImmReleaseContext(hwnd, himc);
        }
        // 传递给系统，确保 IME 继续正常工作（更新组合字符显示）
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    // IME 组合输入结束（确认或取消）
    case WM_IME_ENDCOMPOSITION: {
        WindowEvent ev{};
        ev.kind = WindowEventKind::ImeCompositionEnded;
        event_source_.fire(ev);
        // 传递给系统让 IME 清理组合窗口
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace mine::platform::win32
