/**
 * @file FrameworkElement.cpp
 * @brief FrameworkElement 实现。
 *
 * FrameworkElement 在 UIElement 基础上实现完整的两遍布局协议（WPF 风格）：
 *   - on_measure：处理 Margin、Width/Height/Min/Max 约束后调用 measure_override
 *   - arrange   ：处理 Margin 和对齐方式，计算内容矩形后调用 set_bounds_rect
 *   - on_arrange：回调 arrange_override 让子类安排子元素
 */

#include <mine/ui/visual/FrameworkElement.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/core/TypeId.h>
#include <mine/math/Thickness.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mine::ui {

// ============================================================================
// 内部辅助：对 float 的 clamp（兼容 NaN）
// ============================================================================

namespace {

/// 将 v 限制在 [lo, hi] 范围内；NaN 返回 lo
[[nodiscard]] inline float clamp_f(float v, float lo, float hi) noexcept
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/// 若 v 为 NaN 则返回 fallback，否则返回 v
[[nodiscard]] inline float nan_or(float v, float fallback) noexcept
{
    return std::isnan(v) ? fallback : v;
}

} // anonymous namespace

// ============================================================================
// 依赖属性注册
// ============================================================================

const DependencyProperty& FrameworkElement::WidthProperty =
    register_property<FrameworkElement, float>(
        "Width",
        std::numeric_limits<float>::quiet_NaN(),  // 默认 NaN = Auto
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& FrameworkElement::HeightProperty =
    register_property<FrameworkElement, float>(
        "Height",
        std::numeric_limits<float>::quiet_NaN(),
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& FrameworkElement::MinWidthProperty =
    register_property<FrameworkElement, float>(
        "MinWidth",
        0.0f,
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& FrameworkElement::MaxWidthProperty =
    register_property<FrameworkElement, float>(
        "MaxWidth",
        std::numeric_limits<float>::infinity(),
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& FrameworkElement::MinHeightProperty =
    register_property<FrameworkElement, float>(
        "MinHeight",
        0.0f,
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& FrameworkElement::MaxHeightProperty =
    register_property<FrameworkElement, float>(
        "MaxHeight",
        std::numeric_limits<float>::infinity(),
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& FrameworkElement::MarginProperty =
    register_property<FrameworkElement, math::Thickness>(
        "Margin",
        math::Thickness{},
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& FrameworkElement::HorizontalAlignmentProperty =
    register_property<FrameworkElement, HorizontalAlignment>(
        "HorizontalAlignment",
        HorizontalAlignment::Stretch,
        PropertyMetadata{.affects_arrange = true});

const DependencyProperty& FrameworkElement::VerticalAlignmentProperty =
    register_property<FrameworkElement, VerticalAlignment>(
        "VerticalAlignment",
        VerticalAlignment::Stretch,
        PropertyMetadata{.affects_arrange = true});

// ============================================================================
// 生命周期
// ============================================================================

FrameworkElement::FrameworkElement() = default;
FrameworkElement::~FrameworkElement() = default;

// ============================================================================
// 属性访问器
// ============================================================================

float FrameworkElement::width() const noexcept
{
    return get_value(WidthProperty).get<float>();
}

void FrameworkElement::set_width(float w)
{
    set_value(WidthProperty, core::Variant{w});
}

float FrameworkElement::height() const noexcept
{
    return get_value(HeightProperty).get<float>();
}

void FrameworkElement::set_height(float h)
{
    set_value(HeightProperty, core::Variant{h});
}

float FrameworkElement::min_width() const noexcept
{
    return get_value(MinWidthProperty).get<float>();
}

void FrameworkElement::set_min_width(float v)
{
    set_value(MinWidthProperty, core::Variant{v});
}

float FrameworkElement::max_width() const noexcept
{
    return get_value(MaxWidthProperty).get<float>();
}

void FrameworkElement::set_max_width(float v)
{
    set_value(MaxWidthProperty, core::Variant{v});
}

float FrameworkElement::min_height() const noexcept
{
    return get_value(MinHeightProperty).get<float>();
}

void FrameworkElement::set_min_height(float v)
{
    set_value(MinHeightProperty, core::Variant{v});
}

float FrameworkElement::max_height() const noexcept
{
    return get_value(MaxHeightProperty).get<float>();
}

void FrameworkElement::set_max_height(float v)
{
    set_value(MaxHeightProperty, core::Variant{v});
}

math::Thickness FrameworkElement::margin() const noexcept
{
    return get_value(MarginProperty).get<math::Thickness>();
}

void FrameworkElement::set_margin(math::Thickness m)
{
    set_value(MarginProperty, core::Variant{m});
}

HorizontalAlignment FrameworkElement::horizontal_alignment() const noexcept
{
    return get_value(HorizontalAlignmentProperty).get<HorizontalAlignment>();
}

void FrameworkElement::set_horizontal_alignment(HorizontalAlignment ha)
{
    set_value(HorizontalAlignmentProperty, core::Variant{ha});
}

VerticalAlignment FrameworkElement::vertical_alignment() const noexcept
{
    return get_value(VerticalAlignmentProperty).get<VerticalAlignment>();
}

void FrameworkElement::set_vertical_alignment(VerticalAlignment va)
{
    set_value(VerticalAlignmentProperty, core::Variant{va});
}

// ============================================================================
// Measure 实现（覆盖 UIElement::on_measure）
// ============================================================================

void FrameworkElement::on_measure(math::Size available_size)
{
    const math::Thickness m    = margin();
    const float           mh   = m.horizontal();   // margin 水平分量
    const float           mv   = m.vertical();     // margin 垂直分量

    // 1. 减去 Margin，得到内容区可用尺寸（不允许负数）
    float avail_w = std::max(0.0f, available_size.width  - mh);
    float avail_h = std::max(0.0f, available_size.height - mv);

    // 2. 应用明确的 Width / Height 覆盖（NaN 表示不覆盖）
    const float w = width();
    const float h = height();
    if (!std::isnan(w)) avail_w = w;
    if (!std::isnan(h)) avail_h = h;

    // 3. 应用 Min/Max 约束
    avail_w = clamp_f(avail_w, min_width(),  max_width());
    avail_h = clamp_f(avail_h, min_height(), max_height());

    // 4. 调用子类测量（得到内容期望尺寸）
    math::Size content = measure_override({avail_w, avail_h});

    // 5. 内容尺寸再次应用 Width / Height 明确值（覆盖子类返回值）
    if (!std::isnan(w)) content.width  = w;
    if (!std::isnan(h)) content.height = h;

    // 6. 再次应用 Min/Max 约束（确保子类不越界）
    content.width  = clamp_f(content.width,  min_width(),  max_width());
    content.height = clamp_f(content.height, min_height(), max_height());

    // 7. 加回 Margin，得到元素总期望尺寸
    set_desired_size({content.width + mh, content.height + mv});
}

// ============================================================================
// Arrange 公共入口：处理 Margin 和对齐方式
// ============================================================================

void FrameworkElement::arrange(math::Rect slot)
{
    const math::Thickness m  = margin();

    // 1. 减去 Margin，得到内容可用区
    float cx = slot.x + m.left;
    float cy = slot.y + m.top;
    float cw = std::max(0.0f, slot.width  - m.horizontal());
    float ch = std::max(0.0f, slot.height - m.vertical());

    // 2. 获取内容期望尺寸（desired_size 已包含 Margin，减去 Margin 得内容部分）
    const math::Size ds = desired_size();
    float content_w = std::max(0.0f, ds.width  - m.horizontal());
    float content_h = std::max(0.0f, ds.height - m.vertical());

    // 应用明确 Width/Height 覆盖
    const float ew = width();
    const float eh = height();
    if (!std::isnan(ew)) content_w = ew;
    if (!std::isnan(eh)) content_h = eh;

    // 3. 应用水平对齐
    const HorizontalAlignment ha = horizontal_alignment();
    float actual_x = cx;
    float actual_w = cw;
    if (ha != HorizontalAlignment::Stretch) {
        // 非 Stretch：使用内容期望宽度（不超过可用宽度）
        actual_w = std::min(content_w, cw);
        if (ha == HorizontalAlignment::Center) {
            actual_x = cx + (cw - actual_w) * 0.5f;
        } else if (ha == HorizontalAlignment::Right) {
            actual_x = cx + (cw - actual_w);
        }
        // Left: actual_x = cx（已赋值）
    }

    // 4. 应用垂直对齐
    const VerticalAlignment va = vertical_alignment();
    float actual_y = cy;
    float actual_h = ch;
    if (va != VerticalAlignment::Stretch) {
        actual_h = std::min(content_h, ch);
        if (va == VerticalAlignment::Center) {
            actual_y = cy + (ch - actual_h) * 0.5f;
        } else if (va == VerticalAlignment::Bottom) {
            actual_y = cy + (ch - actual_h);
        }
        // Top: actual_y = cy（已赋值）
    }

    // 5. 设置最终内容矩形（UIElement::set_bounds_rect 存储并回调 on_arrange）
    set_bounds_rect(math::Rect{actual_x, actual_y, actual_w, actual_h});
}

// ============================================================================
// on_arrange 实现（覆盖 UIElement::on_arrange）
// ============================================================================

void FrameworkElement::on_arrange(math::Rect final_rect)
{
    // 调用子类 arrange_override，传入内容区域尺寸
    // bounds_rect() 已在 UIElement::set_bounds_rect 中设置为 final_rect
    arrange_override(final_rect.size());
}

// ============================================================================
// 默认布局覆盖点（叶子元素无子元素时的默认实现）
// ============================================================================

math::Size FrameworkElement::measure_override(math::Size /*available*/)
{
    // 叶子元素默认：期望零尺寸（子类覆盖以报告实际需求）
    return {0.0f, 0.0f};
}

math::Size FrameworkElement::arrange_override(math::Size final_size)
{
    // 叶子元素默认：直接接受分配尺寸（无子元素需安排）
    return final_size;
}

} // namespace mine::ui
