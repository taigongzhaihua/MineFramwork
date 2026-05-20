/**
 * @file Window.h
 * @brief UI 窗口类 —— 将视觉树与平台窗口及渲染管线桥接。
 *
 * Window 是 mine.ui.window 模块的核心类，职责：
 *   1. 持有并转发 platform::IWindow 的生命周期事件（Resized / DpiChanged / Closed）。
 *   2. 在 Resized / DpiChanged 时驱动两遍布局（Measure + Arrange）并重新渲染。
 *   3. 持有对渲染基础设施的引用（IDevice / IQueue / IRenderer），
 *      自行创建并管理 ISwapchain。
 *   4. 在内容根变化或渲染脏时通过 render() 将视觉树绘制到交换链后缓冲并 Present。
 *
 * 所有权规则：
 *   - native_window / device / queue / renderer 由外部（Application）拥有，
 *     Window 仅持有引用，不负责销毁。
 *   - content（UIElement*）由应用层管理，Window 不拥有其生命周期。
 *   - ISwapchain 由 Window 自行创建并独占持有（OwnedPtr）。
 *
 * 线程安全：所有方法须在 UI 线程（创建本对象的线程）调用。
 *
 * 依赖：mine.platform.abi / mine.gfx.rhi / mine.paint / mine.ui.visual / mine.ui.layout
 */

#pragma once

#include <mine/ui/window/Api.h>
#include <mine/core/StringView.h>
#include <mine/core/Pimpl.h>
#include <mine/math/Size.h>
#include <mine/math/Rect.h>

// 前向声明，避免将大型头文件拉入公共接口
namespace mine::platform { class IWindow; }
namespace mine::gfx       { class IDevice; class IQueue; }
namespace mine::paint      { class IRenderer; }
namespace mine::ui         { class UIElement; }

namespace mine::ui {

/**
 * @brief UI 窗口。
 *
 * 典型用法：
 * @code
 *   // 由 Application 创建 host / device / queue / renderer 后：
 *   auto native_win = host->create_window(desc);
 *   auto window = core::make_unique<Window>(
 *       *native_win, *device, *queue, *renderer);
 *   window->set_content(&root_element);
 *   window->show();
 * @endcode
 */
class MINE_UI_WINDOW_API Window {
public:
    /**
     * @brief 构造 Window，立即创建 ISwapchain 并订阅平台事件。
     *
     * @param native_window 平台窗口，生命周期必须长于 Window
     * @param device        图形设备，用于创建交换链，生命周期必须长于 Window
     * @param queue         图形命令队列，用于 resize 前 wait_idle，生命周期必须长于 Window
     * @param renderer      2D 渲染器，用于提交 Canvas DrawCall，生命周期必须长于 Window
     */
    Window(platform::IWindow& native_window,
           gfx::IDevice&      device,
           gfx::IQueue&       queue,
           paint::IRenderer&  renderer);

    ~Window();

    // ── 内容根 ───────────────────────────────────────────────────────────────

    /**
     * @brief 设置窗口的 UI 内容根元素。
     *
     * 设置后立即触发一次布局（Measure + Arrange）与渲染。
     * 传入 nullptr 则清除内容根（窗口仅绘制背景色）。
     *
     * @param element 新的内容根元素（非拥有指针，调用方负责生命周期）
     */
    void set_content(ui::UIElement* element);

    /**
     * @brief 获取当前内容根元素（可能为 nullptr）。
     */
    [[nodiscard]] ui::UIElement* content() const noexcept;

    // ── 平台窗口委托 ─────────────────────────────────────────────────────────

    /// 显示窗口
    void show();

    /// 隐藏窗口（不销毁）
    void hide();

    /// 请求关闭窗口（会触发 Closing → Closed 事件链）
    void close();

    /**
     * @brief 设置窗口标题（UTF-8 字符串）。
     */
    void set_title(core::StringView title);

    /**
     * @brief 设置窗口客户区逻辑像素尺寸。
     */
    void set_size(math::Size size);

    /**
     * @brief 获取窗口当前客户区逻辑像素尺寸。
     */
    [[nodiscard]] math::Size size() const;

    /**
     * @brief 获取窗口当前 DPI（通常为 96.0 * 缩放比）。
     */
    [[nodiscard]] float dpi() const;

    /**
     * @brief 窗口是否已关闭（Closed 事件触发后为 true）。
     *
     * 关闭后不得再调用本对象的任何方法。
     */
    [[nodiscard]] bool is_closed() const noexcept;

    // ── 平台窗口访问 ─────────────────────────────────────────────────────────

    /**
     * @brief 获取底层平台窗口引用（供 Application 访问事件源等）。
     */
    [[nodiscard]] platform::IWindow& native_window() noexcept;

    // ── 渲染 ─────────────────────────────────────────────────────────────────

    /**
     * @brief 驱动一次完整的布局 + 渲染 + Present。
     *
     * 通常由内部平台事件自动触发（Resized / DpiChanged / 渲染脏区）；
     * 应用层也可在内容变化后手动调用以立即刷新画面。
     *
     * 若窗口已关闭（is_closed() == true），此函数为空操作。
     */
    void render();

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
