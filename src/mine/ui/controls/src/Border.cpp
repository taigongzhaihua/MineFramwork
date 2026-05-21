/**
 * @file Border.cpp
 * @brief Border 基础实现。
 */

#include <mine/ui/controls/Border.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>

namespace mine::ui {

Border::Border()
{
    set_style_slot("DefaultBorder");
    set_template_slot("DefaultBorderTemplate");
}

Border::~Border() = default;

UIElement* Border::child() const noexcept
{
    return child_;
}

void Border::set_child(UIElement* child)
{
    if (child_ == child) {
        return;
    }

    if (child_ != nullptr) {
        Visual::remove_child(child_);
    }

    child_ = child;
    if (child_ != nullptr) {
        Visual::add_child(child_);
    }

    invalidate_measure();
    invalidate_render();
}

math::Thickness Border::border_thickness() const noexcept
{
    return border_thickness_;
}

void Border::set_border_thickness(math::Thickness thickness)
{
    border_thickness_ = thickness;
    invalidate_measure();
    invalidate_render();
}

math::Color Border::border_color() const noexcept
{
    return border_color_;
}

void Border::set_border_color(math::Color color)
{
    border_color_ = color;
    invalidate_render();
}

math::Color Border::background() const noexcept
{
    return background_;
}

void Border::set_background(math::Color color)
{
    background_ = color;
    invalidate_render();
}

void Border::on_measure(math::Size available_size)
{
    const math::Rect outer{0.0f, 0.0f, available_size.width, available_size.height};
    const math::Rect inner = outer.deflated(border_thickness_);

    if (child_ != nullptr) {
        const math::Size child_available{
            inner.width > 0.0f ? inner.width : 0.0f,
            inner.height > 0.0f ? inner.height : 0.0f,
        };
        child_->measure(child_available);
        const math::Size child_desired = child_->desired_size();
        set_desired_size({
            child_desired.width + border_thickness_.horizontal(),
            child_desired.height + border_thickness_.vertical(),
        });
        return;
    }

    set_desired_size({
        border_thickness_.horizontal(),
        border_thickness_.vertical(),
    });
}

void Border::on_arrange(math::Rect final_rect)
{
    if (child_ == nullptr) {
        return;
    }

    const math::Rect inner = final_rect.deflated(border_thickness_);
    const math::Rect slot{
        inner.x,
        inner.y,
        inner.width > 0.0f ? inner.width : 0.0f,
        inner.height > 0.0f ? inner.height : 0.0f,
    };
    child_->arrange(slot);
}

void Border::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    if (background_.a > 0.0f) {
        canvas.fill_rect(rect, paint::Brush::solid(background_));
    }

    const math::Thickness t = border_thickness_;
    if (!(t.left == 0.0f && t.top == 0.0f && t.right == 0.0f && t.bottom == 0.0f)) {
        canvas.stroke_bordered_rect(rect, paint::Brush::solid(border_color_), t);
    }
}

} // namespace mine::ui
