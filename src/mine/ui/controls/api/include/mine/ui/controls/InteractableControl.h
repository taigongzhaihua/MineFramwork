/**
 * @file InteractableControl.h
 * @brief InteractableControl —— 可交互控件基类（控件架构重构阶段 1）。
 *
 * 在 Control 基础上添加所有可交互控件的公共基础：
 *   - 交互状态管理（is_hovered / is_pressed / is_disabled）
 *   - 鼠标事件路由基础设施
 *   - State Layer 蒙版画刷属性
 *   - 交互状态变更时的 VSM 驱动
 *
 * 继承关系：
 *   Control → InteractableControl → ContentControl → Button
 *   Control → InteractableControl → CheckBox
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/paint/Brush.h>
#include <mine/math/Color.h>

namespace mine::ui::input { class MouseEventArgs; }

namespace mine::ui {

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4275)
#endif
class MINE_UI_CONTROLS_API InteractableControl : public Control {
public:
    // ── 依赖属性 ──────────────────────────────────────────────────────────

    /// State Layer 蒙版画刷（Brush，默认全透明白色）
    static const DependencyProperty& StateLayerBrushProperty;

    /// 控件启用/禁用状态（bool，默认 true）
    static const DependencyProperty& IsEnabledProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    InteractableControl();
    ~InteractableControl() override;

    InteractableControl(const InteractableControl&)            = delete;
    InteractableControl& operator=(const InteractableControl&) = delete;

    // ── 交互状态查询 ──────────────────────────────────────────────────────

    [[nodiscard]] bool is_hovered() const noexcept;
    [[nodiscard]] bool is_pressed() const noexcept;
    [[nodiscard]] bool is_disabled() const noexcept;

    // ── 交互状态修改 ──────────────────────────────────────────────────────

    void set_disabled(bool disabled) noexcept;

    /// 触发交互状态刷新（鼠标事件处理器自动调用）
    void update_interaction_state();

protected:
    // ── 鼠标事件路由桩 ────────────────────────────────────────────────────

    static void on_mouse_enter_router(void* self, RoutedEventArgs& args, void*);
    static void on_mouse_leave_router(void* self, RoutedEventArgs& args, void*);
    static void on_mouse_down_router(void* self, RoutedEventArgs& args, void*);
    static void on_mouse_up_router(void* self, RoutedEventArgs& args, void*);

    // ── 虚方法 ────────────────────────────────────────────────────────────

    /// 计算当前交互视觉状态（默认：Disabled > Pressed > Hovered > Normal）
    [[nodiscard]] ControlVisualState compute_visual_state() const override;

    /// 交互状态变更回调
    virtual void on_interaction_state_changed(
        ControlVisualState old_state,
        ControlVisualState new_state);

private:
    bool is_hovered_{false};
    bool is_pressed_{false};
};
#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

} // namespace mine::ui
