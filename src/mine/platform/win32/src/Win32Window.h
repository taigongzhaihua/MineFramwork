/**
 * @file Win32Window.h
 * @brief IWindow 的 Win32 具体实现。
 */

#pragma once

#include <mine/platform/IWindow.h>
#include <mine/platform/WindowDesc.h>
#include "Win32Helpers.h"
#include "Win32WindowEventSource.h"

#include <functional>

namespace mine::platform::win32 {

class Win32Window; // 前向声明，供 WindowDestroyCallback 使用

/**
 * @brief 窗口关闭时的通知回调类型。
 *
 * 由 Win32ApplicationHost 传入，当窗口销毁时通知宿主从窗口列表中移除该窗口。
 */
using WindowDestroyCallback = std::function<void(Win32Window*)>;

/**
 * @brief IWindow 的 Win32 HWND 封装实现。
 *
 * 创建并管理一个 Win32 顶级窗口。消息循环由 Win32ApplicationHost 运行。
 * Win32Window 的指针通过 GWLP_USERDATA 关联到 HWND，
 * 静态窗口过程函数 window_proc() 通过此关联分发消息。
 */
class Win32Window final : public IWindow {
public:
    /**
     * @brief 构造并创建 Win32 窗口。
     * @param desc          窗口创建参数
     * @param on_destroy    窗口被销毁时的回调（通知宿主）
     */
    Win32Window(const WindowDesc& desc, WindowDestroyCallback on_destroy);

    ~Win32Window() override;

    // 禁止拷贝和移动
    Win32Window(const Win32Window&)            = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    // ── IWindow 接口实现 ───────────────────────────────────────────────────────

    void show() override;
    void hide() override;
    void close() override;
    void set_title(core::StringView title) override;
    void set_size(math::Size size) override;
    void set_position(math::Point position) override;

    [[nodiscard]] math::Size         size()          const override;
    [[nodiscard]] math::Point        position()      const override;
    [[nodiscard]] float              dpi()           const override;
    [[nodiscard]] bool               is_visible()    const override;
    [[nodiscard]] NativeHandle       native_handle() const override;
    [[nodiscard]] IWindowEventSource& events()        override;

    // ── 内部接口（供 Win32ApplicationHost 使用）──────────────────────────────

    /// 返回 Win32 HWND
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// 判断窗口是否已被销毁（WM_DESTROY 之后）
    [[nodiscard]] bool is_destroyed() const noexcept { return hwnd_ == nullptr; }

    /**
     * @brief Win32 静态窗口过程函数（由 Win32WindowClass 注册）。
     *
     * 通过 GWLP_USERDATA 获取 Win32Window* 后转发到 handle_message()。
     */
    static LRESULT CALLBACK window_proc(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept;

private:
    /**
     * @brief 处理单条 Win32 消息。
     */
    LRESULT handle_message(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept;

    /// 获取当前窗口 DPI（系统 API）
    [[nodiscard]] float query_dpi() const noexcept;

    /// 将物理像素客户区大小转为逻辑像素
    [[nodiscard]] math::Size client_size_logical() const noexcept;

    /// 将物理像素窗口位置转为逻辑像素
    [[nodiscard]] math::Point window_position_logical() const noexcept;

private:
    HWND                     hwnd_{nullptr};
    float                    dpi_{96.0f};
    WindowKind               kind_{WindowKind::Normal}; ///< 窗口类型（决定 show() 行为等）
    Win32WindowEventSource   event_source_;
    WindowDestroyCallback    on_destroy_;
};

} // namespace mine::platform::win32
