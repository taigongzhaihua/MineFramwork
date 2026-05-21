/**
 * @file Button.cpp
 * @brief Button 基础实现。
 */

#include <mine/ui/controls/Button.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/MouseEventArgs.h>

namespace mine::ui {

const RoutedEvent& Button::ClickEvent()
{
    static const RoutedEvent& ev = register_event<Button>("Click", RoutingStrategy::Bubble);
    return ev;
}

Button::Button()
{
    set_style_slot("DefaultButton");
    set_template_slot("DefaultButtonTemplate");

    add_handler(input::MouseDownEvent(), &Button::on_mouse_down_router, this);
    add_handler(input::MouseUpEvent(), &Button::on_mouse_up_router, this);
}

Button::~Button() = default;

core::StringView Button::text() const noexcept
{
    return core::StringView{ text_.data(), text_.size() };
}

void Button::set_text(core::StringView text)
{
    text_ = text;
    invalidate_measure();
    invalidate_render();
}

bool Button::is_enabled() const noexcept
{
    return is_enabled_;
}

void Button::set_enabled(bool enabled) noexcept
{
    is_enabled_ = enabled;
    if (!is_enabled_) {
        is_pressed_ = false;
    }
    update_visual_state();
    invalidate_render();
}

math::Thickness Button::padding() const noexcept
{
    return padding_;
}

void Button::set_padding(math::Thickness padding)
{
    padding_ = padding;
    invalidate_measure();
    invalidate_render();
}

math::Color Button::foreground() const noexcept
{
    return foreground_;
}

void Button::set_foreground(math::Color color)
{
    foreground_ = color;
    invalidate_render();
}

math::Color Button::background() const noexcept
{
    return background_;
}

void Button::set_background(math::Color color)
{
    background_ = color;
    invalidate_render();
}

math::Color Button::background_hovered() const noexcept
{
    return background_hover_;
}

void Button::set_background_hovered(math::Color color)
{
    background_hover_ = color;
    invalidate_render();
}

math::Color Button::background_pressed() const noexcept
{
    return background_press_;
}

void Button::set_background_pressed(math::Color color)
{
    background_press_ = color;
    invalidate_render();
}

math::Color Button::border_color() const noexcept
{
    return border_color_;
}

void Button::set_border_color(math::Color color)
{
    border_color_ = color;
    invalidate_render();
}

void Button::on_measure(math::Size /*available_size*/)
{
    const float text_w = static_cast<float>(text_.size()) * 14.0f * 0.55f;
    const float text_h = 14.0f * 1.4f;
    set_desired_size({
        text_w + padding_.horizontal(),
        text_h + padding_.vertical(),
    });
}

void Button::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    math::Color fill = background_;
    if (!is_enabled_) {
        fill = math::Color{0.85f, 0.85f, 0.85f, 1.0f};
    } else if (visual_state() == ControlVisualState::Pressed) {
        fill = background_press_;
    } else if (visual_state() == ControlVisualState::Hovered) {
        fill = background_hover_;
    }

    canvas.fill_rect(rect, paint::Brush::solid(fill));
    canvas.stroke_bordered_rect(rect, paint::Brush::solid(border_color_), math::Thickness::uniform(1.0f));

    // M1.1 阶段没有默认字体对象时，用居中横条表示文字区域。
    const float line_w = rect.width - padding_.horizontal();
    const float line_y = rect.y + rect.height * 0.5f - 1.0f;
    if (line_w > 0.0f) {
        canvas.fill_rect(
            { rect.x + padding_.left, line_y, line_w, 2.0f },
            paint::Brush::solid(foreground_));
    }
}

ControlVisualState Button::compute_visual_state() const
{
    if (!is_enabled_) {
        return ControlVisualState::Disabled;
    }
    if (is_pressed_) {
        return ControlVisualState::Pressed;
    }
    return ControlVisualState::Normal;
}

void Button::on_mouse_down_router(void* /*sender*/, RoutedEventArgs& args, void* user_data)
{
    auto* self = static_cast<Button*>(user_data);
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    self->on_mouse_down(mouse_args);
}

void Button::on_mouse_up_router(void* /*sender*/, RoutedEventArgs& args, void* user_data)
{
    auto* self = static_cast<Button*>(user_data);
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    self->on_mouse_up(mouse_args);
}

void Button::on_mouse_down(input::MouseEventArgs& args)
{
    if (!is_enabled_ || args.button() != input::MouseButton::Left) {
        return;
    }
    is_pressed_ = true;
    update_visual_state();
}

void Button::on_mouse_up(input::MouseEventArgs& args)
{
    if (!is_enabled_ || args.button() != input::MouseButton::Left) {
        return;
    }

    const bool should_click = is_pressed_;
    is_pressed_ = false;
    update_visual_state();

    if (should_click) {
        raise_click();
    }
}

void Button::raise_click()
{
    RoutedEventArgs args{ ClickEvent() };
    args.set_original_source(this);
    args.set_source(this);
    EventManager::raise(*this, args);
}

} // namespace mine::ui
