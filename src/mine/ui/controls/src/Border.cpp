/**
 * @file Border.cpp
 * @brief Border 实现（合格基元控件：外观完全由依赖属性驱动）。
 */

#include <mine/ui/controls/Border.h>

#include <mine/core/Memory.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>
#include <mine/math/ComplexRoundedRect.h>
#include <mine/math/CornerRadii.h>
#include <mine/math/Color.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <cstdio>

namespace mine::ui {

// ============================================================================
// 依赖属性注册
// ============================================================================

// BackgroundProperty — 背景画刷（Brush，默认透明）
const DependencyProperty& Border::BackgroundProperty =
    register_property<Border>(
        "Background",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
        PropertyMetadata{
            .affects_render = true,
        });

// BorderBrushProperty — 边框画刷（Brush，默认 #808080）
const DependencyProperty& Border::BorderBrushProperty =
    register_property<Border>(
        "BorderBrush",
        core::Variant{ paint::Brush::solid_rgb(0x808080) },
        PropertyMetadata{
            .affects_render = true,
        });

// BorderThicknessProperty — 边框粗细（Thickness，默认四边 1dp）
// 边框粗细影响内容可用区域，故同时触发 measure / arrange 失效
const DependencyProperty& Border::BorderThicknessProperty =
    register_property<Border>(
        "BorderThickness",
        core::Variant{ math::Thickness::uniform(1.0f) },
        PropertyMetadata{
            .affects_measure = true,
            .affects_arrange = true,
            .affects_render  = true,
        });

// CornerRadiusProperty — 圆角半径（CornerRadii，默认四角直角 0）
const DependencyProperty& Border::CornerRadiusProperty =
    register_property<Border>(
        "CornerRadius",
        core::Variant{ math::CornerRadii::uniform(0.0f) },
        PropertyMetadata{
            .affects_render = true,
        });

// ============================================================================
// 生命周期
// ============================================================================

Border::Border()
{
    set_style_slot("DefaultBorder");
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

    // 设置裸指针时，清除之前可能持有的所有权（不应同时持有两种引用）
    owned_child_.reset();
    child_ = child;
    if (child_ != nullptr) {
        Visual::add_child(child_);
    }

    invalidate_measure();
    invalidate_render();
}

void Border::set_child(core::OwnedPtr<UIElement> child)
{
    if (child_ != nullptr) {
        Visual::remove_child(child_);
    }

    // 转移所有权：Border 拥有子元素，析构时自动释放
    owned_child_ = std::move(child);
    child_ = owned_child_.get();
    if (child_ != nullptr) {
        Visual::add_child(child_);
    }

    invalidate_measure();
    invalidate_render();
}

math::Thickness Border::border_thickness() const noexcept
{
    const core::Variant& v = get_value(BorderThicknessProperty);
    return v.has<math::Thickness>() ? v.get<math::Thickness>() : math::Thickness::uniform(1.0f);
}

void Border::set_border_thickness(math::Thickness thickness)
{
    set_value(BorderThicknessProperty, core::Variant{ thickness }, ValuePriority::Local);
}

paint::Brush Border::border_brush() const noexcept
{
    const core::Variant& v = get_value(BorderBrushProperty);
    return v.has<paint::Brush>() ? v.get<paint::Brush>() : paint::Brush::solid_rgb(0x808080);
}

void Border::set_border_brush(paint::Brush brush)
{
    set_value(BorderBrushProperty, core::Variant{ brush }, ValuePriority::Local);
}

paint::Brush Border::background() const noexcept
{
    const core::Variant& v = get_value(BackgroundProperty);
    return v.has<paint::Brush>() ? v.get<paint::Brush>() : paint::Brush::solid(math::Color::Transparent);
}

void Border::set_background(paint::Brush brush)
{
    set_value(BackgroundProperty, core::Variant{ brush }, ValuePriority::Local);
}

math::CornerRadii Border::corner_radius() const noexcept
{
    const core::Variant& v = get_value(CornerRadiusProperty);
    return v.has<math::CornerRadii>() ? v.get<math::CornerRadii>() : math::CornerRadii::uniform(0.0f);
}

void Border::set_corner_radius(math::CornerRadii radii)
{
    set_value(CornerRadiusProperty, core::Variant{ radii }, ValuePriority::Local);
}

void Border::on_measure(math::Size available_size)
{
    const math::Thickness t = border_thickness();

    // 读取 FrameworkElement 的 Width/Height DP（显式尺寸覆盖可用空间）
    const float w = width();
    const float h = height();

    float child_avail_w = available_size.width  - t.horizontal();
    float child_avail_h = available_size.height - t.vertical();
    if (!std::isnan(w)) child_avail_w = std::max(0.0f, w - t.horizontal());
    if (!std::isnan(h)) child_avail_h = std::max(0.0f, h - t.vertical());

    math::Size child_desired;
    if (child_ != nullptr) {
        const math::Size child_available{
            std::max(0.0f, child_avail_w),
            std::max(0.0f, child_avail_h),
        };
        child_->measure(child_available);
        child_desired = child_->desired_size();
    }

    // 计算自身期望尺寸：子元素尺寸 + 边框厚度；若设了显式宽高则直接使用
    float desired_w = child_desired.width  + t.horizontal();
    float desired_h = child_desired.height + t.vertical();
    if (!std::isnan(w)) desired_w = w;
    if (!std::isnan(h)) desired_h = h;

    set_desired_size({desired_w, desired_h});
}

void Border::on_arrange(math::Rect final_rect)
{
    if (child_ == nullptr) {
        return;
    }

    const math::Rect inner = final_rect.deflated(border_thickness());
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

    const paint::Brush      bg     = background();
    const paint::Brush      stroke = border_brush();
    const math::Thickness   t      = border_thickness();
    const math::CornerRadii radii  = corner_radius();

    // 背景画刷可见性检查：纯色全透明时跳过
    const bool bg_visible = (bg.kind() != paint::BrushKind::SolidColor)
                         || (bg.color().a > 0.0f);

    // 是否存在圆角：任一角的 rx/ry 大于 0
    const bool has_corner =
        radii.top_left.x > 0.0f || radii.top_left.y > 0.0f ||
        radii.top_right.x > 0.0f || radii.top_right.y > 0.0f ||
        radii.bottom_right.x > 0.0f || radii.bottom_right.y > 0.0f ||
        radii.bottom_left.x > 0.0f || radii.bottom_left.y > 0.0f;

    const bool border_visible =
        !(t.left == 0.0f && t.top == 0.0f && t.right == 0.0f && t.bottom == 0.0f);

    // 调试输出：渲染时的边框/背景/矩形信息
    printf("[Border::on_render] rect=(%.1f,%.1f,%.1f,%.1f) bg_kind=%d bg_a=%.2f border_visible=%d t=(%.1f,%.1f,%.1f,%.1f) corner=%s\n",
           rect.x, rect.y, rect.width, rect.height,
           static_cast<int>(bg.kind()), bg.color().a,
           static_cast<int>(border_visible), t.left, t.top, t.right, t.bottom,
           has_corner ? "true" : "false");

    if (has_corner) {
        // ── 圆角路径：使用复合圆角矩形绘制背景与描边 ──────────
        const math::ComplexRoundedRect rr{ rect, radii };
        if (bg_visible) {
            canvas.fill_complex_rounded_rect(rr, bg);
        }
        if (border_visible) {
            // 圆角描边为单一线宽：取四边代表值（圆角场景边框通常等宽）
            paint::Pen pen;
            pen.width     = t.left;
            pen.line_join = paint::LineJoin::Round;
            canvas.stroke_complex_rounded_rect(rr, stroke, pen);
        }
    } else {
        // ── 直角快路径：与历史行为一致（内描边矩形），视觉零变化 ──────
        if (bg_visible) {
            canvas.fill_rect(rect, bg);
        }
        if (border_visible) {
            canvas.stroke_bordered_rect(rect, stroke, t);
        }
    }
}

} // namespace mine::ui
