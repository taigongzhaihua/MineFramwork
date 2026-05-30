/**
 * @file Win32Window.h
 * @brief IWindow 的 Win32 具体实现。
 */

#pragma once

#include <mine/platform/IWindow.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/WindowChromeDesc.h>
#include "Win32Helpers.h"
#include "Win32WindowEventSource.h"
#include "Win32IMEService.h"

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
     * @param ime_service   IME 服务指针（可为 nullptr，焦点变化时通知）
     */
    Win32Window(const WindowDesc& desc, WindowDestroyCallback on_destroy,
                Win32IMEService* ime_service = nullptr);

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

    // ── 自定义 Chrome 实现 ──────────────────────────────────────────────────

    /**
     * @brief 应用自定义窗口 Chrome 配置（IWindow 接口实现）。
     *
     * 具体操作：
     *   1. 存储 chrome 描述符
     *   2. 调用 DwmExtendFrameIntoClientArea 配置玻璃帧延伸
     *   3. 调用 DwmSetWindowAttribute 配置圆角（Windows 11+）
     *   4. 调用 SetWindowPos(SWP_FRAMECHANGED) 触发 WM_NCCALCSIZE 重新计算
     *
     * 当 chrome.enabled = false 时，恢复系统默认行为（重新调用 DefWindowProc 处理 NC 消息）。
     */
    void set_chrome(const WindowChromeDesc& chrome) override;

    /**
     * @brief 发起系统窗口拖拽（IWindow 接口实现）。
     *
     * Win32 实现：
     *   1. ReleaseCapture()：释放框架内部鼠标捕获
     *   2. PostMessageW(WM_NCLBUTTONDOWN, HTCAPTION, 0)：模拟用户在
     *      标题栏区域按下鼠标左键，由系统接管后续拖拽（含 Windows 11 Snap 布局）
     *
     * 仅在 HWND 有效时执行（已销毁的窗口调用为空操作）。
     */
    void begin_drag() override;

    /**
     * @brief 设置窗口显示状态（IWindow 接口实现）。
     *
     * Win32 实现：
     *   - Minimized → ShowWindow(hwnd_, SW_MINIMIZE)
     *   - Maximized → ShowWindow(hwnd_, SW_MAXIMIZE)
     *   - Normal    → ShowWindow(hwnd_, SW_RESTORE)
     *
     * 仅在 HWND 有效时执行。
     */
    void set_state(platform::WindowState state) override;

    /**
     * @brief 查询窗口当前显示状态（IWindow 接口实现）。
     *
     * Win32 实现：IsIconic → Minimized，IsZoomed → Maximized，否则 Normal。
     */
    [[nodiscard]] platform::WindowState state() const override;

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

    /**
     * @brief 应用当前 chrome_ 配置到窗口（内部实现，在 HWND 有效时调用）。
     *
     * 调用 DWM API 设置玻璃帧和圆角，并通过 SWP_FRAMECHANGED 触发 NC 区域重新计算。
     */
    void apply_chrome_() noexcept;

    /**
     * @brief 处理 WM_NCCALCSIZE 消息：在自定义 Chrome 模式下将客户区扩展至整个窗口区域。
     * @return 消息处理返回值
     */
    LRESULT handle_nccalcsize_(WPARAM wparam, LPARAM lparam) noexcept;

    /**
     * @brief 处理 WM_NCHITTEST 消息：在自定义 Chrome 模式下提供标题栏/边框命中测试。
     * @return 命中测试结果（HTCAPTION / HTLEFT 等），或 HTCLIENT
     */
    LRESULT handle_nchittest_(LPARAM lparam) noexcept;

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
    Win32IMEService*         ime_service_{nullptr};     ///< IME 服务（弱引用，由宿主持有）

    /// 当前自定义 Chrome 配置（默认 enabled=false，即使用系统标题栏）
    WindowChromeDesc         chrome_{};
};

} // namespace mine::platform::win32
