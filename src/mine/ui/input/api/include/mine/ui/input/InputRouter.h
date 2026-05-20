/**
 * @file InputRouter.h
 * @brief 输入事件路由器。
 *
 * InputRouter 订阅平台窗口事件（IWindowEventSink），接收原始 WindowEvent，
 * 通过命中测试（UIElement::hit_test）确定目标元素，
 * 然后调用 EventManager::raise() 将键盘/鼠标路由事件派发到视觉树。
 *
 * 使用方式：
 * @code
 *   InputRouter router;
 *   router.set_root(root_element);
 *   router.set_keyboard_focus(root_element);
 *   window->events().subscribe(&router);
 * @endcode
 */

#pragma once

#include <mine/platform/IWindowEventSink.h>
#include <mine/ui/input/Api.h>

namespace mine::ui {
class UIElement;
} // namespace mine::ui

namespace mine::platform {
struct WindowEvent;
} // namespace mine::platform

namespace mine::ui::input {

/**
 * @brief 输入路由器——将平台 WindowEvent 转换为路由键盘/鼠标事件并派发到视觉树。
 *
 * 生命周期：
 *   InputRouter 不持有 root_ / keyboard_focus_ / mouse_over_ 的所有权，
 *   调用者有责任在元素销毁前调用对应的 set_xxx(nullptr)。
 */
class MINE_UI_INPUT_API InputRouter : public platform::IWindowEventSink {
public:
    InputRouter() noexcept;
    ~InputRouter() override;

    // 禁止拷贝
    InputRouter(const InputRouter&)            = delete;
    InputRouter& operator=(const InputRouter&) = delete;

    // ── 视觉树根节点 ──────────────────────────────────────────────────────

    /// 设置视觉树根节点（命中测试的起点）
    void set_root(UIElement* root) noexcept;
    /// 返回当前视觉树根节点
    [[nodiscard]] UIElement* root() const noexcept;

    // ── 键盘焦点 ──────────────────────────────────────────────────────────

    /// 设置当前键盘焦点元素（键盘事件的目标）
    void set_keyboard_focus(UIElement* element) noexcept;
    /// 返回当前键盘焦点元素（可能为 nullptr）
    [[nodiscard]] UIElement* keyboard_focus() const noexcept;

    // ── 鼠标悬停 ──────────────────────────────────────────────────────────

    /// 返回当前鼠标悬停元素（最近一次命中测试结果）
    [[nodiscard]] UIElement* mouse_over_element() const noexcept;

    // ── IWindowEventSink ─────────────────────────────────────────────────

    /// 接收平台 WindowEvent 并路由为 UI 输入事件
    void on_window_event(platform::WindowEvent& event) override;

private:
    /// 派发键盘事件（KeyDown / KeyUp）
    void dispatch_key_event(const platform::WindowEvent& we);
    /// 派发鼠标事件（MouseDown / MouseUp / MouseMove / MouseWheel）
    void dispatch_mouse_event(const platform::WindowEvent& we);

    UIElement* root_{nullptr};
    UIElement* keyboard_focus_{nullptr};
    UIElement* mouse_over_{nullptr};
};

} // namespace mine::ui::input
