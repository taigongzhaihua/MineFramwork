/**
 * @file WindowContext.h
 * @brief 应用窗口上下文接口 —— 供 Window 在 show() 时进行懒初始化。
 *
 * IWindowContext 由 mine::ui::app::Application 实现，通过进程级全局指针注册/获取。
 * Window 在 pending 状态（无参构造后、show() 前）使用此接口完成：
 *   1. 原生窗口创建（platform::IWindow）
 *   2. 图形资源绑定（IDevice / IQueue / IRenderer）
 *   3. Application 级别后处理（安装 tick_and_render 回调、自动主窗口）
 *
 * 设计意图：
 *   mine.ui.window 不依赖 mine.ui.app，通过此接口反向调用 Application 的服务，
 *   避免循环依赖。Application 在 run() 初始化图形设备后注册自身为 context，
 *   退出时清除为 nullptr。
 */

#pragma once

#include <mine/ui/window/Api.h>
#include <mine/core/Memory.h>

// 前向声明，避免将大型头文件拉入公共接口
namespace mine::platform { struct WindowDesc; class IWindow; class IMEService; }
namespace mine::gfx      { class IDevice; class IQueue; }
namespace mine::paint    { class IRenderer; }

namespace mine::ui {

class Window;

/**
 * @brief 应用窗口上下文接口。
 *
 * 由 mine::ui::app::Application 实现。
 * Application::run() 初始化图形设备后，通过 set_application_window_context()
 * 注册自身；退出前清除为 nullptr。
 */
class MINE_UI_WINDOW_API IWindowContext {
public:
    virtual ~IWindowContext() = default;

    /**
     * @brief 创建平台原生窗口。
     * @param desc 窗口创建参数（由 Window 从 pending 属性构建）
     * @return 新创建的原生窗口（不可为 nullptr）
     */
    [[nodiscard]] virtual core::OwnedPtr<platform::IWindow>
        create_native_window(const platform::WindowDesc& desc) = 0;

    /// 返回图形设备（生命周期由 Application 管理）
    [[nodiscard]] virtual gfx::IDevice&       device()   noexcept = 0;
    [[nodiscard]] virtual gfx::IQueue&        queue()    noexcept = 0;
    [[nodiscard]] virtual paint::IRenderer&   renderer() noexcept = 0;
    [[nodiscard]] virtual platform::IMEService& ime()      noexcept = 0;

    /**
     * @brief Window 首次 show() 时的通知回调。
     *
     * Application 在此回调中：
     *   - 安装 tick_and_render 作为 post_input_fn_
     *   - 若尚无主窗口，自动将 win 设为主窗口
     *
     * @param win 刚完成懒初始化的窗口
     */
    virtual void on_window_first_show(Window& win) = 0;
};

/**
 * @brief 注册进程级窗口上下文（由 Application::run() 调用，退出时传 nullptr 清除）。
 */
MINE_UI_WINDOW_API void set_application_window_context(IWindowContext* ctx) noexcept;

/**
 * @brief 获取进程级窗口上下文（Window::show() 懒初始化时使用）。
 * @return 当前 context，nullptr 表示 Application 未运行
 */
[[nodiscard]] MINE_UI_WINDOW_API IWindowContext* get_application_window_context() noexcept;

} // namespace mine::ui
