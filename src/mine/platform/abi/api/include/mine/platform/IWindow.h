/**
 * @file IWindow.h
 * @brief 平台窗口抽象接口。
 */

#pragma once

#include <mine/core/StringView.h>
#include <mine/math/Size.h>
#include <mine/math/Point.h>
#include <mine/platform/NativeHandle.h>
#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/WindowChromeDesc.h>

namespace mine::platform {

/**
 * @brief 平台窗口抽象接口。
 *
 * 每个 IWindow 对象代表操作系统中的一个顶级窗口（或子窗口）。
 * 通过 IApplicationHost::create_window() 创建，以 OwnedPtr<IWindow> 返回。
 *
 * 线程安全：所有方法须在创建该窗口的线程（通常为主线程）调用。
 *
 * 生命周期：
 *   - close() → 触发 Closing 事件 → 若未取消，销毁原生窗口 → 触发 Closed 事件
 *   - Closed 事件触发后，不得再调用任何 IWindow 方法
 */
class IWindow {
public:
    virtual ~IWindow() = default;

    // ── 可见性控制 ────────────────────────────────────────────────────────────

    /// 显示窗口（若已可见则无操作）
    virtual void show() = 0;

    /// 隐藏窗口（不销毁，可再次 show()）
    virtual void hide() = 0;

    /// 请求关闭窗口（触发 Closing 事件，可被取消）
    virtual void close() = 0;

    // ── 属性设置 ──────────────────────────────────────────────────────────────

    /// 设置窗口标题（UTF-8）
    virtual void set_title(core::StringView title) = 0;

    /// 设置客户区尺寸（逻辑像素）
    virtual void set_size(math::Size size) = 0;

    /// 设置窗口左上角屏幕位置（逻辑像素）
    virtual void set_position(math::Point position) = 0;

    // ── 属性查询 ──────────────────────────────────────────────────────────────

    /// 当前客户区尺寸（逻辑像素）
    [[nodiscard]] virtual math::Size size() const = 0;

    /// 当前窗口左上角屏幕位置（逻辑像素）
    [[nodiscard]] virtual math::Point position() const = 0;

    /// 当前窗口 DPI（通常为 96.0 * 缩放比）
    [[nodiscard]] virtual float dpi() const = 0;

    /// 窗口当前是否可见
    [[nodiscard]] virtual bool is_visible() const = 0;

    /// 获取平台原生句柄（不透明，供 RHI 后端使用）
    [[nodiscard]] virtual NativeHandle native_handle() const = 0;

    // ── 事件订阅 ──────────────────────────────────────────────────────────────

    /// 获取事件分发器，用于订阅/取消订阅窗口事件
    [[nodiscard]] virtual IWindowEventSource& events() = 0;

    // ── 自定义 Chrome（可选功能）─────────────────────────────────────────────

    /**
     * @brief 应用自定义窗口 Chrome 配置。
     *
     * 默认实现为空操作（no-op），各平台后端可按需重写：
     *   - Win32：处理 WM_NCCALCSIZE / WM_NCHITTEST，调用 DWM API
     *   - macOS：使用 NSWindow 标题栏/透明 API
     *   - 其他平台：忽略不支持的功能字段
     *
     * 窗口创建后可随时调用，立即生效。
     *
     * @param chrome 自定义 Chrome 描述符
     */
    virtual void set_chrome(const WindowChromeDesc& chrome) {}

    /**
     * @brief 以编程方式发起窗口拖拽，类似 WPF 的 Window.DragMove()。
     *
     * 释放框架内部鼠标捕获后，通知系统"用户在标题栏（HTCAPTION）区域按下鼠标"，
     * 系统接管后续拖拽逻辑（移动窗口 + 磁吸/Snap 布局）。
     *
     * 典型用法：在自定义标题栏区域的 MouseDownEvent 处理函数中调用。
     * 调用后当前帧内不应再处理鼠标事件（建议同时调用 args.set_handled(true)）。
     *
     * 默认实现为空操作（no-op），各平台后端按需重写。
     */
    virtual void begin_drag() {}
};

} // namespace mine::platform
