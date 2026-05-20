/**
 * @file StackPanel.h
 * @brief StackPanel —— 线性堆叠布局面板。
 *
 * StackPanel 沿指定方向（水平或垂直）依次排列子元素：
 *
 * 垂直（默认）：子元素自上而下逐行排列，主轴（垂直）尺寸累加，
 *   交叉轴（水平）取最大 desiredWidth。
 *
 * 水平：子元素从左到右逐列排列，主轴（水平）尺寸累加，
 *   交叉轴（垂直）取最大 desiredHeight。
 *
 * 注意：StackPanel 不对主轴方向的子元素施加上限约束（传入 INFINITY），
 * 子元素可报告任意 desiredSize；Arrange 时按 desiredSize 分配主轴空间。
 */

#pragma once

#include <mine/ui/layout/Api.h>
#include <mine/ui/layout/Panel.h>
#include <mine/ui/layout/Orientation.h>
#include <mine/ui/property/DependencyProperty.h>

namespace mine::ui {

/**
 * @brief 线性堆叠布局面板。
 *
 * Measure 算法：
 *   - Vertical：传给每个子元素的 available = {panelWidth, INFINITY}
 *     desiredWidth  = max(children desiredWidth)
 *     desiredHeight = sum(children desiredHeight)
 *   - Horizontal：传给每个子元素的 available = {INFINITY, panelHeight}
 *     desiredWidth  = sum(children desiredWidth)
 *     desiredHeight = max(children desiredHeight)
 *
 * Arrange 算法：
 *   - 沿主轴方向累计偏移，逐一调用 child->arrange(slot)
 *   - slot 的交叉轴尺寸 = 面板分配的交叉轴尺寸（允许 Stretch）
 *   - slot 的主轴尺寸   = child->desired_size() 在主轴的分量
 */
class MINE_UI_LAYOUT_API StackPanel : public Panel {
public:
    // ── 依赖属性 ──────────────────────────────────────────────────────────

    /// 排列方向（Orientation::Horizontal / Orientation::Vertical）
    static const DependencyProperty& OrientationProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    StackPanel();
    ~StackPanel() override;

    StackPanel(const StackPanel&)            = delete;
    StackPanel& operator=(const StackPanel&) = delete;
    StackPanel(StackPanel&&)                 = default;
    StackPanel& operator=(StackPanel&&)      = default;

    // ── 属性访问 ──────────────────────────────────────────────────────────

    [[nodiscard]] Orientation orientation() const noexcept;
    void set_orientation(Orientation o);

protected:
    // ── 布局实现 ──────────────────────────────────────────────────────────

    math::Size measure_override(math::Size available) override;
    math::Size arrange_override(math::Size final_size) override;
};

} // namespace mine::ui
