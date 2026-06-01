/**
 * @file Grid.cpp
 * @brief Grid 行列网格布局面板实现。
 *
 * 行列尺寸计算算法（三步）：
 *   1. Pixel 行/列：直接使用固定像素值（应用 min/max 约束）
 *   2. Auto 行/列：收集该行/列内所有子元素在该方向的 desiredSize 取最大值
 *      （子元素必须先经过 Measure 才有 desiredSize）
 *   3. Star 行/列：
 *      a. 可用空间有限时：按权重比例分配剩余空间（剩余 = 可用 - Pixel - Auto 之和）
 *      b. 可用空间无限时（如 StackPanel 在堆叠方向给子元素无限空间）：
 *         Star 行/列不得无限延伸，改为基于内容的最小等比分配：
 *           unit = max(content_size[i] / weight[i])  对所有 Star 行/列取最大值
 *           每个 Star 行/列的实际尺寸 = weight[i] * unit
 *         该策略保证各 Star 行/列均能容纳其内容，且彼此保持正确的权重比例。
 *         示例：1* 内容 30px，2* 内容 40px
 *           unit = max(30/1, 40/2) = 30px
 *           1* 得 30px，2* 得 60px，两列比例正确且内容均可显示
 *
 * 注意事项：
 *   - 跨行列（RowSpan/ColumnSpan > 1）的子元素在内容尺寸收集时不参与
 *     （简化实现，与 WPF 不同；后续可完善）
 *   - Star 权重之和为 0 时，所有 Star 行/列尺寸为 0
 */

#include <mine/ui/layout/Grid.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/core/TypeId.h>
#include <mine/containers/SmallVector.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mine::ui {

// ============================================================================
// Impl（内部实现细节）
// ============================================================================

struct Grid::Impl {
    /// 行定义列表
    containers::SmallVector<RowDefinition, 4> rows;
    /// 列定义列表
    containers::SmallVector<ColumnDefinition, 4> columns;

    /// 布局时计算出的每行实际高度（在 measure_override / arrange_override 中填充）
    containers::SmallVector<float, 4> row_heights;
    /// 布局时计算出的每列实际宽度
    containers::SmallVector<float, 4> col_widths;
};

// ============================================================================
// 附加属性注册
// ============================================================================

const DependencyProperty& Grid::RowProperty =
    register_attached_property(
        "Grid.Row",
        core::TypeId::of<Grid>(),
        core::TypeId::of<int>(),
        core::Variant{0},
        PropertyMetadata{.affects_arrange = true});

const DependencyProperty& Grid::ColumnProperty =
    register_attached_property(
        "Grid.Column",
        core::TypeId::of<Grid>(),
        core::TypeId::of<int>(),
        core::Variant{0},
        PropertyMetadata{.affects_arrange = true});

const DependencyProperty& Grid::RowSpanProperty =
    register_attached_property(
        "Grid.RowSpan",
        core::TypeId::of<Grid>(),
        core::TypeId::of<int>(),
        core::Variant{1},
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& Grid::ColumnSpanProperty =
    register_attached_property(
        "Grid.ColumnSpan",
        core::TypeId::of<Grid>(),
        core::TypeId::of<int>(),
        core::Variant{1},
        PropertyMetadata{.affects_measure = true});

// ============================================================================
// 附加属性便捷静态方法
// ============================================================================

int Grid::get_row(const UIElement& elem) noexcept
{
    return elem.get_value(RowProperty).get<int>();
}

void Grid::set_row(UIElement& elem, int row)
{
    elem.set_value(RowProperty, core::Variant{row});
}

int Grid::get_column(const UIElement& elem) noexcept
{
    return elem.get_value(ColumnProperty).get<int>();
}

void Grid::set_column(UIElement& elem, int col)
{
    elem.set_value(ColumnProperty, core::Variant{col});
}

int Grid::get_row_span(const UIElement& elem) noexcept
{
    return elem.get_value(RowSpanProperty).get<int>();
}

void Grid::set_row_span(UIElement& elem, int span)
{
    elem.set_value(RowSpanProperty, core::Variant{span});
}

int Grid::get_column_span(const UIElement& elem) noexcept
{
    return elem.get_value(ColumnSpanProperty).get<int>();
}

void Grid::set_column_span(UIElement& elem, int span)
{
    elem.set_value(ColumnSpanProperty, core::Variant{span});
}

// ============================================================================
// 生命周期
// ============================================================================

Grid::Grid()
    : p_{ core::make_pimpl<Impl>() }
{}

Grid::~Grid() = default;

// ============================================================================
// 行列定义管理
// ============================================================================

void Grid::add_row(RowDefinition def)
{
    p_->rows.push_back(def);
    invalidate_measure();
}

void Grid::add_column(ColumnDefinition def)
{
    p_->columns.push_back(def);
    invalidate_measure();
}

uint32_t Grid::row_count() const noexcept
{
    return static_cast<uint32_t>(p_->rows.size());
}

uint32_t Grid::column_count() const noexcept
{
    return static_cast<uint32_t>(p_->columns.size());
}

// ============================================================================
// 内部辅助：计算行列实际尺寸
// ============================================================================

namespace {

/**
 * @brief 将 float 限制在 [lo, hi]（兼容 lo/hi 为 inf）
 */
[[nodiscard]] inline float clamp_sz(float v, float lo, float hi) noexcept
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * @brief 根据行定义列表计算实际行高数组。
 *
 * @param rows           行定义列表
 * @param row_count      行数
 * @param content_max_h  每行子元素期望高度的最大值（Pixel/Auto/Star 行均填充，来自 Measure 后的 desiredSize）
 * @param available_h    可用总高度（有限值时 Star 按剩余比例分配；infinity 时按内容最小等比策略）
 * @param[out] heights   输出：各行实际高度
 */
void compute_row_heights(
    const containers::SmallVector<RowDefinition, 4>& rows,
    uint32_t                                          row_count,
    const containers::SmallVector<float, 4>&          content_max_h,
    float                                             available_h,
    containers::SmallVector<float, 4>&                heights)
{
    heights.resize(row_count, 0.0f);

    float star_total_weight = 0.0f;
    float fixed_used_h      = 0.0f;  // Pixel + Auto 已消耗高度

    // ── Pass 1：Pixel 和 Auto ──────────────────────────────────────────────
    for (uint32_t r = 0; r < row_count; ++r) {
        const RowDefinition& def = rows[r];
        if (def.height.is_pixel()) {
            const float h = clamp_sz(def.height.value, def.min_height, def.max_height);
            heights[r]   = h;
            fixed_used_h += h;
        } else if (def.height.is_auto()) {
            const float h = clamp_sz(content_max_h[r], def.min_height, def.max_height);
            heights[r]   = h;
            fixed_used_h += h;
        } else {
            // Star：暂不设值，累计权重
            star_total_weight += def.height.value;
        }
    }

    // ── Pass 2：Star 分配 ──────────────────────────────────────────────────
    if (star_total_weight > 0.0f) {
        const float remaining = std::max(0.0f, available_h - fixed_used_h);

        if (std::isinf(remaining)) {
            // 可用空间无限：Star 行不得无限延伸，改为基于内容的最小等比分配。
            // unit = max(content_max_h[r] / weight[r])，保证每行均能容纳其内容，
            // 且各 Star 行之间保持正确的权重比例关系。
            float star_unit = 0.0f;
            for (uint32_t r = 0; r < row_count; ++r) {
                const RowDefinition& def = rows[r];
                if (def.height.is_star() && def.height.value > 0.0f)
                    star_unit = std::max(star_unit, content_max_h[r] / def.height.value);
            }
            for (uint32_t r = 0; r < row_count; ++r) {
                const RowDefinition& def = rows[r];
                if (def.height.is_star())
                    heights[r] = clamp_sz(def.height.value * star_unit, def.min_height, def.max_height);
            }
        } else {
            // 可用空间有限：按剩余空间权重比例分配
            for (uint32_t r = 0; r < row_count; ++r) {
                const RowDefinition& def = rows[r];
                if (def.height.is_star()) {
                    heights[r] = clamp_sz(
                        remaining * (def.height.value / star_total_weight),
                        def.min_height, def.max_height);
                }
            }
        }
    }
}

/**
 * @brief 根据列定义列表计算实际列宽数组（与 compute_row_heights 对称）。
 *
 * @param columns        列定义列表
 * @param col_count      列数
 * @param content_max_w  每列子元素期望宽度的最大值（Pixel/Auto/Star 列均填充）
 * @param available_w    可用总宽度（有限值时 Star 按剩余比例分配；infinity 时按内容最小等比策略）
 * @param[out] widths    输出：各列实际宽度
 */
void compute_col_widths(
    const containers::SmallVector<ColumnDefinition, 4>& columns,
    uint32_t                                             col_count,
    const containers::SmallVector<float, 4>&             content_max_w,
    float                                                available_w,
    containers::SmallVector<float, 4>&                   widths)
{
    widths.resize(col_count, 0.0f);

    float star_total_weight = 0.0f;
    float fixed_used_w      = 0.0f;

    // ── Pass 1：Pixel 和 Auto ──────────────────────────────────────────────
    for (uint32_t c = 0; c < col_count; ++c) {
        const ColumnDefinition& def = columns[c];
        if (def.width.is_pixel()) {
            const float w = clamp_sz(def.width.value, def.min_width, def.max_width);
            widths[c]    = w;
            fixed_used_w += w;
        } else if (def.width.is_auto()) {
            const float w = clamp_sz(content_max_w[c], def.min_width, def.max_width);
            widths[c]    = w;
            fixed_used_w += w;
        } else {
            star_total_weight += def.width.value;
        }
    }

    // ── Pass 2：Star 分配 ──────────────────────────────────────────────────
    if (star_total_weight > 0.0f) {
        const float remaining = std::max(0.0f, available_w - fixed_used_w);

        if (std::isinf(remaining)) {
            // 可用空间无限：Star 列不得无限延伸，改为基于内容的最小等比分配。
            // unit = max(content_max_w[c] / weight[c])，保证每列均能容纳其内容，
            // 且各 Star 列之间保持正确的权重比例关系。
            float star_unit = 0.0f;
            for (uint32_t c = 0; c < col_count; ++c) {
                const ColumnDefinition& def = columns[c];
                if (def.width.is_star() && def.width.value > 0.0f)
                    star_unit = std::max(star_unit, content_max_w[c] / def.width.value);
            }
            for (uint32_t c = 0; c < col_count; ++c) {
                const ColumnDefinition& def = columns[c];
                if (def.width.is_star())
                    widths[c] = clamp_sz(def.width.value * star_unit, def.min_width, def.max_width);
            }
        } else {
            // 可用空间有限：按剩余空间权重比例分配
            for (uint32_t c = 0; c < col_count; ++c) {
                const ColumnDefinition& def = columns[c];
                if (def.width.is_star()) {
                    widths[c] = clamp_sz(
                        remaining * (def.width.value / star_total_weight),
                        def.min_width, def.max_width);
                }
            }
        }
    }
}

} // anonymous namespace

// ============================================================================
// Measure 实现
// ============================================================================

math::Size Grid::measure_override(math::Size available)
{
    // 若无行/列定义，视为单行单列
    const uint32_t num_rows = (row_count()    == 0) ? 1u : row_count();
    const uint32_t num_cols = (column_count() == 0) ? 1u : column_count();

    // 每行/列中所有子元素期望尺寸的最大值（跨度为 1 的子元素参与；Auto/Star/Pixel 行列均填充）
    containers::SmallVector<float, 4> content_max_h(num_rows, 0.0f);
    containers::SmallVector<float, 4> content_max_w(num_cols, 0.0f);

    // ── Measure Pass：对所有子元素提供 available 约束，收集各行列内容最大尺寸 ──
    // 注：子元素先以父给定的 available 尺寸进行 measure，子元素内部
    //     （如嵌套 Grid）会递归应用同样的无限空间 star_unit 策略。
    const uint32_t child_cnt = children_count();
    for (uint32_t i = 0; i < child_cnt; ++i) {
        UIElement* child = child_at(i);

        // 读取附加属性（若无行/列定义，则固定为 0）
        const int row      = (row_count() == 0) ? 0 : get_row(*child);
        const int col      = (column_count() == 0) ? 0 : get_column(*child);
        const int row_span = (row_count() == 0) ? 1 : get_row_span(*child);
        const int col_span = (column_count() == 0) ? 1 : get_column_span(*child);

        // 钳制索引在有效范围内
        const int r_clamped = std::max(0, std::min(row, (int)num_rows - 1));
        const int c_clamped = std::max(0, std::min(col, (int)num_cols - 1));

        // 以 available 约束 measure，收集各行列的内容最大尺寸
        child->measure({available.width, available.height});
        const math::Size ds = child->desired_size();

        // 仅跨度为 1 的子元素参与内容最大值收集（跨行列简化处理）
        if (row_span == 1) {
            content_max_h[static_cast<uint32_t>(r_clamped)] =
                std::max(content_max_h[static_cast<uint32_t>(r_clamped)], ds.height);
        }
        if (col_span == 1) {
            content_max_w[static_cast<uint32_t>(c_clamped)] =
                std::max(content_max_w[static_cast<uint32_t>(c_clamped)], ds.width);
        }
    }

    // ── 计算行高 / 列宽 ───────────────────────────────────────────────────

    // 若无行/列定义，使用默认 1* 定义
    containers::SmallVector<RowDefinition, 4> eff_rows;
    containers::SmallVector<ColumnDefinition, 4> eff_cols;

    if (row_count() == 0) {
        eff_rows.push_back(RowDefinition{});  // 默认 1*
    } else {
        for (uint32_t r = 0; r < row_count(); ++r) {
            eff_rows.push_back(p_->rows[r]);
        }
    }

    if (column_count() == 0) {
        eff_cols.push_back(ColumnDefinition{});  // 默认 1*
    } else {
        for (uint32_t c = 0; c < column_count(); ++c) {
            eff_cols.push_back(p_->columns[c]);
        }
    }

    compute_row_heights(eff_rows, num_rows, content_max_h, available.height, p_->row_heights);
    compute_col_widths(eff_cols,  num_cols, content_max_w, available.width,  p_->col_widths);

    // ── 汇总总期望尺寸 ────────────────────────────────────────────────────
    float total_w = 0.0f;
    float total_h = 0.0f;
    for (uint32_t c = 0; c < num_cols; ++c) total_w += p_->col_widths[c];
    for (uint32_t r = 0; r < num_rows; ++r) total_h += p_->row_heights[r];

    return {total_w, total_h};
}

// ============================================================================
// Arrange 实现
// ============================================================================

math::Size Grid::arrange_override(math::Size final_size)
{
    const uint32_t num_rows = (row_count()    == 0) ? 1u : row_count();
    const uint32_t num_cols = (column_count() == 0) ? 1u : column_count();

    // 用 final_size 重新计算行列尺寸（Star 按实际 final_size 分配，Auto 复用 Measure 缓存）
    containers::SmallVector<float, 4> content_max_h(num_rows, 0.0f);
    containers::SmallVector<float, 4> content_max_w(num_cols, 0.0f);

    // 收集各行列子元素的 desiredSize（复用 Measure 后的缓存结果，避免重新 measure）
    const uint32_t child_cnt = children_count();
    for (uint32_t i = 0; i < child_cnt; ++i) {
        UIElement* child = child_at(i);
        const int row      = (row_count() == 0) ? 0 : get_row(*child);
        const int col      = (column_count() == 0) ? 0 : get_column(*child);
        const int row_span = (row_count() == 0) ? 1 : get_row_span(*child);
        const int col_span = (column_count() == 0) ? 1 : get_column_span(*child);

        const int r_clamped = std::max(0, std::min(row, (int)num_rows - 1));
        const int c_clamped = std::max(0, std::min(col, (int)num_cols - 1));

        const math::Size ds = child->desired_size();
        if (row_span == 1) {
            content_max_h[static_cast<uint32_t>(r_clamped)] =
                std::max(content_max_h[static_cast<uint32_t>(r_clamped)], ds.height);
        }
        if (col_span == 1) {
            content_max_w[static_cast<uint32_t>(c_clamped)] =
                std::max(content_max_w[static_cast<uint32_t>(c_clamped)], ds.width);
        }
    }

    // 构造行/列定义（复用内部定义或默认 1*）
    containers::SmallVector<RowDefinition, 4> eff_rows;
    containers::SmallVector<ColumnDefinition, 4> eff_cols;

    if (row_count() == 0) {
        eff_rows.push_back(RowDefinition{});
    } else {
        for (uint32_t r = 0; r < row_count(); ++r) eff_rows.push_back(p_->rows[r]);
    }
    if (column_count() == 0) {
        eff_cols.push_back(ColumnDefinition{});
    } else {
        for (uint32_t c = 0; c < column_count(); ++c) eff_cols.push_back(p_->columns[c]);
    }

    // 用 final_size 重新计算，Star 行/列按 final_size 中的有限剩余空间展开
    compute_row_heights(eff_rows, num_rows, content_max_h, final_size.height, p_->row_heights);
    compute_col_widths(eff_cols,  num_cols, content_max_w, final_size.width,  p_->col_widths);

    // ── 计算每行/列的起始偏移 ─────────────────────────────────────────────
    containers::SmallVector<float, 4> row_offsets(num_rows, 0.0f);
    containers::SmallVector<float, 4> col_offsets(num_cols, 0.0f);

    float offset_y = 0.0f;
    for (uint32_t r = 0; r < num_rows; ++r) {
        row_offsets[r] = offset_y;
        offset_y      += p_->row_heights[r];
    }

    float offset_x = 0.0f;
    for (uint32_t c = 0; c < num_cols; ++c) {
        col_offsets[c] = offset_x;
        offset_x      += p_->col_widths[c];
    }

    // ── 安排每个子元素 ────────────────────────────────────────────────────
    // 自身在父坐标系中的绝对起点（arrange_override 调用时 bounds_rect 已设置好）
    const math::Rect self_bounds = bounds_rect();

    for (uint32_t i = 0; i < child_cnt; ++i) {
        UIElement* child = child_at(i);

        const int row      = (row_count() == 0) ? 0 : get_row(*child);
        const int col      = (column_count() == 0) ? 0 : get_column(*child);
        const int row_span = (row_count() == 0) ? 1 : get_row_span(*child);
        const int col_span = (column_count() == 0) ? 1 : get_column_span(*child);

        // 钳制到有效范围
        const uint32_t r_start = static_cast<uint32_t>(std::max(0, std::min(row, (int)num_rows - 1)));
        const uint32_t c_start = static_cast<uint32_t>(std::max(0, std::min(col, (int)num_cols - 1)));
        const uint32_t r_end   = std::min(r_start + static_cast<uint32_t>(std::max(1, row_span)), num_rows);
        const uint32_t c_end   = std::min(c_start + static_cast<uint32_t>(std::max(1, col_span)), num_cols);

        // 合并跨越的行/列矩形，加上自身绝对起点得到绝对坐标
        const float slot_x = self_bounds.x + col_offsets[c_start];
        const float slot_y = self_bounds.y + row_offsets[r_start];
        float slot_w = 0.0f;
        float slot_h = 0.0f;
        for (uint32_t c = c_start; c < c_end; ++c) slot_w += p_->col_widths[c];
        for (uint32_t r = r_start; r < r_end; ++r) slot_h += p_->row_heights[r];

        child->arrange(math::Rect{slot_x, slot_y, slot_w, slot_h});
    }

    return final_size;
}

} // namespace mine::ui
