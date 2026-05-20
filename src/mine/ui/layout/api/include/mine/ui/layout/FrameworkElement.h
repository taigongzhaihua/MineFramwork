/**
 * @file FrameworkElement.h
 * @brief FrameworkElement —— 具有 Margin、尺寸约束和对齐方式的布局元素基类。
 *
 * FrameworkElement 在 UIElement 基础上添加完整的两遍布局协议（WPF 风格）：
 *
 * Measure 阶段（自顶向下提供 availableSize，从底向上报告 desiredSize）：
 *   父节点调用 child->measure(available)
 *     → FrameworkElement::on_measure(available)
 *         → 减去 Margin，应用 Width/Height/Min/Max 约束
 *         → 调用 measure_override(constrained) → 子类返回内容期望尺寸
 *         → 加回 Margin，调用 set_desired_size(total)
 *
 * Arrange 阶段（自顶向下分配 finalRect）：
 *   父节点调用 child->arrange(slot)
 *     → FrameworkElement::arrange(slot)
 *         → 减去 Margin，应用 HorizontalAlignment/VerticalAlignment
 *         → 调用 set_bounds_rect(content_rect) → UIElement 存储 bounds_rect
 *         → UIElement::set_bounds_rect 回调 on_arrange(content_rect)
 *         → FrameworkElement::on_arrange(content_rect)
 *             → 调用 arrange_override(content_rect.size()) → 子类安排子元素
 *
 * 继承关系：
 *   Visual (mine.ui.visual)
 *       └─ UIElement (mine.ui.visual)
 *           └─ FrameworkElement (mine.ui.layout)
 *               └─ Panel (mine.ui.layout)
 *               └─ Control (mine.ui.visual，将迁移到 mine.ui.layout)
 */

#pragma once

#include <mine/ui/layout/Api.h>
#include <mine/ui/layout/HorizontalAlignment.h>
#include <mine/ui/layout/VerticalAlignment.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/math/Thickness.h>
#include <mine/math/Rect.h>
#include <mine/math/Size.h>
#include <mine/ui/property/DependencyProperty.h>

namespace mine::ui {

/**
 * @brief 具有尺寸约束、Margin 和对齐方式的布局框架元素基类。
 *
 * 所有需要参与布局系统的 UI 元素都应继承自 FrameworkElement（而非直接继承 UIElement）。
 * 控件（Control）、面板（Panel）等均以 FrameworkElement 为基础。
 *
 * 属性默认值：
 *   - Width / Height        ：NaN（自动，由内容决定）
 *   - MinWidth / MinHeight  ：0
 *   - MaxWidth / MaxHeight  ：无限大
 *   - Margin                ：{0, 0, 0, 0}
 *   - HorizontalAlignment   ：Stretch
 *   - VerticalAlignment     ：Stretch
 */
class MINE_UI_LAYOUT_API FrameworkElement : public UIElement {
public:
    // ── 依赖属性声明 ───────────────────────────────────────────────────────

    static const DependencyProperty& WidthProperty;              ///< 宽度（NaN = 自动）
    static const DependencyProperty& HeightProperty;             ///< 高度（NaN = 自动）
    static const DependencyProperty& MinWidthProperty;           ///< 最小宽度
    static const DependencyProperty& MaxWidthProperty;           ///< 最大宽度
    static const DependencyProperty& MinHeightProperty;          ///< 最小高度
    static const DependencyProperty& MaxHeightProperty;          ///< 最大高度
    static const DependencyProperty& MarginProperty;             ///< 外边距（Thickness）
    static const DependencyProperty& HorizontalAlignmentProperty;///< 水平对齐
    static const DependencyProperty& VerticalAlignmentProperty;  ///< 垂直对齐

    // ── 生命周期 ──────────────────────────────────────────────────────────

    FrameworkElement();
    ~FrameworkElement() override;

    FrameworkElement(const FrameworkElement&)            = delete;
    FrameworkElement& operator=(const FrameworkElement&) = delete;
    FrameworkElement(FrameworkElement&&)                 = default;
    FrameworkElement& operator=(FrameworkElement&&)      = default;

    // ── 尺寸属性访问 ──────────────────────────────────────────────────────

    /// 返回明确指定的宽度，NaN 表示由内容自动决定
    [[nodiscard]] float width() const noexcept;
    void set_width(float w);

    /// 返回明确指定的高度，NaN 表示由内容自动决定
    [[nodiscard]] float height() const noexcept;
    void set_height(float h);

    [[nodiscard]] float min_width()  const noexcept;
    void set_min_width(float v);

    [[nodiscard]] float max_width()  const noexcept;
    void set_max_width(float v);

    [[nodiscard]] float min_height() const noexcept;
    void set_min_height(float v);

    [[nodiscard]] float max_height() const noexcept;
    void set_max_height(float v);

    // ── 对齐与外边距 ──────────────────────────────────────────────────────

    [[nodiscard]] math::Thickness     margin()               const noexcept;
    void set_margin(math::Thickness m);

    [[nodiscard]] HorizontalAlignment horizontal_alignment() const noexcept;
    void set_horizontal_alignment(HorizontalAlignment ha);

    [[nodiscard]] VerticalAlignment   vertical_alignment()   const noexcept;
    void set_vertical_alignment(VerticalAlignment va);

    // ── Arrange 公共入口 ───────────────────────────────────────────────────

    /**
     * @brief Arrange 公共入口（由父 Panel 调用）。
     *
     * 处理 Margin 和对齐方式后，调用 set_bounds_rect(content_rect)，
     * 进而触发 on_arrange(content_rect) → arrange_override(content_size)。
     *
     * @param slot 父节点分配给本元素的完整矩形槽（含 Margin 区域）
     */
    void arrange(math::Rect slot);

protected:
    // ── 布局扩展点（子类覆盖）─────────────────────────────────────────────

    /**
     * @brief Measure 覆盖点：计算元素内容区域的期望尺寸。
     *
     * 子类（Panel、StackPanel、Grid 等）覆盖此方法实现自定义测量逻辑。
     * 输入 available 已减去 Margin 并应用了 Width/Height/Min/Max 约束。
     * 返回值为内容区域期望尺寸（不含 Margin）。
     *
     * 默认返回 {0, 0}（叶子元素无需测量）。
     *
     * @param available 可用内容区域尺寸
     * @return 内容期望尺寸
     */
    virtual math::Size measure_override(math::Size available);

    /**
     * @brief Arrange 覆盖点：在确定的内容区域内安排子元素。
     *
     * 输入 final_size 是经过对齐处理后的内容区域尺寸。
     * 子类覆盖此方法调用子元素的 arrange() 方法进行递归排列。
     * 返回值为元素实际占用的内容尺寸（通常与 final_size 相同）。
     *
     * 默认直接返回 final_size（叶子元素无子元素需排列）。
     *
     * @param final_size 分配给内容区域的最终尺寸（已减去 Margin 并经对齐处理）
     * @return 实际占用的内容尺寸
     */
    virtual math::Size arrange_override(math::Size final_size);

    // ── 覆盖 UIElement 的布局虚方法 ──────────────────────────────────────

    /**
     * @brief 覆盖 UIElement::on_measure，处理 Margin 和尺寸约束后调用 measure_override。
     */
    void on_measure(math::Size available_size) override;

    /**
     * @brief 覆盖 UIElement::on_arrange，调用 arrange_override。
     *
     * 由 UIElement::set_bounds_rect() 在设置 bounds_rect_ 后回调。
     * 此时 bounds_rect() 已是经过 Margin 和对齐处理的内容矩形。
     *
     * @param final_rect 内容区域最终矩形（与 bounds_rect() 相同）
     */
    void on_arrange(math::Rect final_rect) override;
};

} // namespace mine::ui
