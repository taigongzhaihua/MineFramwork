/**
 * @file InteractableControl.cpp
 * @brief InteractableControl 可交互控件基类实现。
 *
 * 将 Button 与 CheckBox 重复的交互逻辑提取到本公共基类。
 */

#include <mine/ui/controls/InteractableControl.h>

#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/animation/Storyboard.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/Duration.h>
#include <mine/paint/Brush.h>
#include <mine/math/Color.h>

namespace mine::ui {

// ============================================================================
// 依赖属性
// ============================================================================

const DependencyProperty& InteractableControl::StateLayerBrushProperty =
    register_property<InteractableControl>(
        "StateLayerBrush",
        core::Variant{ paint::Brush::solid(math::Color{1.0f, 1.0f, 1.0f, 0.0f}) },
        PropertyMetadata{ .affects_render = true });

const DependencyProperty& InteractableControl::IsEnabledProperty =
    register_property<InteractableControl>(
        "IsEnabled",
        core::Variant{ true },
        PropertyMetadata{ .affects_render = true });

// ============================================================================
// 构造 / 析构
// ============================================================================

InteractableControl::InteractableControl()
{
    // 默认交互 VSM（Normal / Hovered / Pressed / Disabled）
    style::VisualStateManager vsm{ *this };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Disabled");

    auto* self = this;
    vsm.add_transition("*", "Hovered",
        [self](animation::Storyboard& sb) {
            sb.animate_dp(*self, StateLayerBrushProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Normal",
        [self](animation::Storyboard& sb) {
            sb.animate_dp(*self, StateLayerBrushProperty,
                          animation::Duration::milliseconds(100.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Pressed",
        [self](animation::Storyboard& sb) {
            sb.animate_dp(*self, StateLayerBrushProperty,
                          animation::Duration::milliseconds(60.0f),
                          animation::QuadEaseIn);
        });

    set_visual_state_manager(std::move(vsm));
}

InteractableControl::~InteractableControl() = default;

// ============================================================================
// 交互状态查询
// ============================================================================

bool InteractableControl::is_hovered() const noexcept
{
    return is_hovered_;
}

bool InteractableControl::is_pressed() const noexcept
{
    return is_pressed_;
}

bool InteractableControl::is_disabled() const noexcept
{
    const core::Variant& v = get_value(IsEnabledProperty);
    return v.has<bool>() ? !v.get<bool>() : false;
}

// ============================================================================
// 交互状态修改
// ============================================================================

void InteractableControl::set_disabled(bool disabled) noexcept
{
    set_value(IsEnabledProperty, core::Variant{ !disabled }, ValuePriority::Local);
}

void InteractableControl::update_interaction_state()
{
    update_visual_state();
}

// ============================================================================
// 鼠标事件路由桩
// ============================================================================

void InteractableControl::on_mouse_enter_router(void* self_ptr, RoutedEventArgs&, void*)
{
    auto* self = static_cast<InteractableControl*>(self_ptr);
    if (self->is_disabled()) return;
    self->is_hovered_ = true;
    self->update_interaction_state();
}

void InteractableControl::on_mouse_leave_router(void* self_ptr, RoutedEventArgs&, void*)
{
    auto* self = static_cast<InteractableControl*>(self_ptr);
    self->is_hovered_ = false;
    self->is_pressed_ = false;
    self->update_interaction_state();
}

void InteractableControl::on_mouse_down_router(void* self_ptr, RoutedEventArgs& args, void*)
{
    auto* self = static_cast<InteractableControl*>(self_ptr);
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    if (self->is_disabled() || mouse_args.button() != input::MouseButton::Left) return;
    self->is_pressed_ = true;
    self->update_interaction_state();
}

void InteractableControl::on_mouse_up_router(void* self_ptr, RoutedEventArgs& args, void*)
{
    auto* self = static_cast<InteractableControl*>(self_ptr);
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    if (self->is_disabled() || mouse_args.button() != input::MouseButton::Left) return;
    self->is_pressed_ = false;
    self->update_interaction_state();
}

// ============================================================================
// 视觉状态计算
// ============================================================================

ControlVisualState InteractableControl::compute_visual_state() const
{
    if (is_disabled()) return ControlVisualState::Disabled;
    if (is_pressed_)   return ControlVisualState::Pressed;
    if (is_hovered_)   return ControlVisualState::Hovered;
    return ControlVisualState::Normal;
}

// ============================================================================
// 交互状态变更回调
// ============================================================================

void InteractableControl::on_interaction_state_changed(
    ControlVisualState /*old_state*/,
    ControlVisualState /*new_state*/)
{
    // 默认无操作，子类可覆盖
}

} // namespace mine::ui
