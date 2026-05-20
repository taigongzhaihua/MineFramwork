/**
 * @file Grid.h
 * @brief Grid —— 行列网格布局面板，支持像素/Auto/Star 三种行列尺寸。
 *
 * Grid 将可用空间划分为若干行和列（RowDefinition / ColumnDefinition），
 * 子元素通过附加属性 Grid::RowProperty / Grid::ColumnProperty 指定所在位置，
 * 并可通过 Grid::RowSpanProperty / Grid::ColumnSpanProperty 实现跨行列。
 *
 * 行列尺寸计算（Measure 阶段）：
 *   1. 先处理所有 Pixel 行/列：直接设定固定尺寸
 *   2. 处理 Auto 行/列：收集该行/列中所有子元素的 desiredSize 取最大值
 *   3. 处理 Star 行/列：按权重比例分配剩余空间
 *
 * Arrange 阶段：
 *   根据每行/列的计算尺寸，生成各子元素对应的 slot 矩形（支持跨行列合并），
 *   逐一调用 child->arrange(slot)。
 */

#pragma once

#include <mine/ui/layout/Api.h>
#include <mine/ui/layout/Panel.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/containers/SmallVector.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/core/Pimpl.h>

namespace mine::ui {

/**
 * @brief 行列网格布局面板。
 *
 * 附加属性（可在任意 FrameworkElement 上设置）：
 *   - Grid::RowProperty      ：子元素所在行索引（默认 0）
 *   - Grid::ColumnProperty   ：子元素所在列索引（默认 0）
 *   - Grid::RowSpanProperty  ：子元素跨越的行数（默认 1）
 *   - Grid::ColumnSpanProperty：子元素跨越的列数（默认 1）
 */
class MINE_UI_LAYOUT_API Grid : public Panel {
public:
    // ── 附加属性（注册在 Grid 上，可设置到任意 FrameworkElement）──────────

    /// 子元素所在行（0-based，默认 0）
    static const DependencyProperty& RowProperty;

    /// 子元素所在列（0-based，默认 0）
    static const DependencyProperty& ColumnProperty;

    /// 子元素跨越行数（默认 1）
    static const DependencyProperty& RowSpanProperty;

    /// 子元素跨越列数（默认 1）
    static const DependencyProperty& ColumnSpanProperty;

    // ── 便捷静态方法（等价于在子元素上 get/set 对应附加属性）──────────────

    static int  get_row(const FrameworkElement& elem) noexcept;
    static void set_row(FrameworkElement& elem, int row);

    static int  get_column(const FrameworkElement& elem) noexcept;
    static void set_column(FrameworkElement& elem, int col);

    static int  get_row_span(const FrameworkElement& elem) noexcept;
    static void set_row_span(FrameworkElement& elem, int span);

    static int  get_column_span(const FrameworkElement& elem) noexcept;
    static void set_column_span(FrameworkElement& elem, int span);

    // ── 生命周期 ──────────────────────────────────────────────────────────

    Grid();
    ~Grid() override;

    Grid(const Grid&)            = delete;
    Grid& operator=(const Grid&) = delete;
    Grid(Grid&&)                 = default;
    Grid& operator=(Grid&&)      = default;

    // ── 行列定义 ──────────────────────────────────────────────────────────

    /**
     * @brief 添加行定义（末尾追加）。
     *
     * 若未添加任何行定义，Grid 视为只有一行（1*）。
     *
     * @param def 行定义
     */
    void add_row(RowDefinition def);

    /**
     * @brief 添加列定义（末尾追加）。
     *
     * 若未添加任何列定义，Grid 视为只有一列（1*）。
     *
     * @param def 列定义
     */
    void add_column(ColumnDefinition def);

    [[nodiscard]] uint32_t row_count()    const noexcept;
    [[nodiscard]] uint32_t column_count() const noexcept;

protected:
    // ── 布局实现 ──────────────────────────────────────────────────────────

    math::Size measure_override(math::Size available) override;
    math::Size arrange_override(math::Size final_size) override;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
