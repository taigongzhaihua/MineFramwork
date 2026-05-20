/**
 * @file StackPanel.cpp
 * @brief StackPanel 线性堆叠布局面板实现。
 *
 * Measure 算法：
 *   Vertical：主轴（垂直）方向累加 desiredHeight，交叉轴（水平）取最大 desiredWidth。
 *   Horizontal：主轴（水平）方向累加 desiredWidth，交叉轴（垂直）取最大 desiredHeight。
 *
 * Arrange 算法：
 *   沿主轴方向逐一分配子元素的矩形槽，偏移量从 0 开始累加。
 *   交叉轴方向：槽的尺寸等于面板分配的交叉轴尺寸（子元素通过 HorizontalAlignment/
 *   VerticalAlignment 实现在槽内对齐）。
 */

#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/property/DependencyProperty.h>

#include <algorithm>
#include <limits>

namespace mine::ui {

// ============================================================================
// 依赖属性注册
// ============================================================================

const DependencyProperty& StackPanel::OrientationProperty =
    register_property<StackPanel, Orientation>(
        "Orientation",
        Orientation::Vertical,
        PropertyMetadata{.affects_measure = true});

// ============================================================================
// 生命周期
// ============================================================================

StackPanel::StackPanel() = default;

StackPanel::~StackPanel() = default;

// ============================================================================
// 属性访问器
// ============================================================================

Orientation StackPanel::orientation() const noexcept
{
    return get_value(OrientationProperty).get<Orientation>();
}

void StackPanel::set_orientation(Orientation o)
{
    set_value(OrientationProperty, core::Variant{o});
}

// ============================================================================
// Measure 实现
// ============================================================================

math::Size StackPanel::measure_override(math::Size available)
{
    const Orientation orient = orientation();
    const bool is_vertical   = (orient == Orientation::Vertical);

    float total_w = 0.0f;  // 水平方向累计或最大值
    float total_h = 0.0f;  // 垂直方向累计或最大值

    const uint32_t count = children_count();
    for (uint32_t i = 0; i < count; ++i) {
        FrameworkElement* child = child_at(i);

        // 对主轴方向传入无限制（StackPanel 不限制主轴方向的子元素尺寸）
        math::Size child_avail = available;
        if (is_vertical) {
            child_avail.height = std::numeric_limits<float>::infinity();
        } else {
            child_avail.width = std::numeric_limits<float>::infinity();
        }

        // 调用子元素 measure（UIElement::measure → FrameworkElement::on_measure）
        child->measure(child_avail);
        const math::Size ds = child->desired_size();

        if (is_vertical) {
            // 垂直方向：主轴累加高度，交叉轴取最大宽度
            total_h += ds.height;
            total_w = std::max(total_w, ds.width);
        } else {
            // 水平方向：主轴累加宽度，交叉轴取最大高度
            total_w += ds.width;
            total_h = std::max(total_h, ds.height);
        }
    }

    return {total_w, total_h};
}

// ============================================================================
// Arrange 实现
// ============================================================================

math::Size StackPanel::arrange_override(math::Size final_size)
{
    const Orientation orient = orientation();
    const bool is_vertical   = (orient == Orientation::Vertical);

    float offset = 0.0f;  // 主轴方向的当前偏移量
    const uint32_t count = children_count();

    for (uint32_t i = 0; i < count; ++i) {
        FrameworkElement* child = child_at(i);
        const math::Size ds = child->desired_size();

        math::Rect slot;
        if (is_vertical) {
            // 垂直排列：每个子元素占用 {全宽, desiredHeight}
            slot.x      = 0.0f;
            slot.y      = offset;
            slot.width  = final_size.width;
            slot.height = ds.height;
            offset += ds.height;
        } else {
            // 水平排列：每个子元素占用 {desiredWidth, 全高}
            slot.x      = offset;
            slot.y      = 0.0f;
            slot.width  = ds.width;
            slot.height = final_size.height;
            offset += ds.width;
        }

        // 调用子元素 arrange（FrameworkElement::arrange 处理 Margin 和对齐）
        child->arrange(slot);
    }

    return final_size;
}

} // namespace mine::ui
