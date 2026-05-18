/**
 * @file ComplexRoundedRect.h
 * @brief 四角各自独立的椭圆圆角矩形。
 */

#pragma once

#include <mine/math/Rect.h>
#include <mine/math/CornerRadii.h>
#include <mine/math/Thickness.h>

namespace mine::math {

/**
 * @brief 四角各自独立的椭圆圆角矩形（类似 CSS border-radius 完整形式）。
 *
 * 构造时自动按 CSS 规范的等比缩放算法对圆角半径进行钳制：
 * 若同侧两角的半径之和超过对应边长，则对所有角按比例均匀缩小，
 * 确保圆角几何始终有效（相邻角不重叠）。
 *
 * 点包含测试对每个角独立执行椭圆方程判断，精确且高效。
 */
struct ComplexRoundedRect {
    Rect rect{};
    CornerRadii radii{};

    constexpr ComplexRoundedRect() noexcept = default;

    /**
     * @brief 从矩形与圆角半径构造，自动执行比例钳制。
     */
    constexpr ComplexRoundedRect(Rect rect_val, CornerRadii radii_val) noexcept
        : rect{rect_val}
        , radii{clamp_radii(rect_val, radii_val)}
    {}

    [[nodiscard]] constexpr Rect bounds() const noexcept { return rect; }

    /**
     * @brief 平移矩形，圆角半径不变（不重新钳制）。
     */
    [[nodiscard]] constexpr ComplexRoundedRect translated(Vec2 delta) const noexcept {
        ComplexRoundedRect result;
        result.rect = rect.translated(delta);
        result.radii = radii;
        return result;
    }

    /**
     * @brief 精确点包含测试：对每个角分别执行椭圆方程判断。
     */
    [[nodiscard]] constexpr bool contains(Point p) const noexcept {
        if (!rect.contains(p)) {
            return false;
        }

        // 转换为矩形本地坐标（左上角为原点）
        const float lx = p.x - rect.x;
        const float ly = p.y - rect.y;
        const float w  = rect.width;
        const float h  = rect.height;

        const Vec2& tl = radii.top_left;
        const Vec2& tr = radii.top_right;
        const Vec2& bl = radii.bottom_left;
        const Vec2& br = radii.bottom_right;

        // 左上角（椭圆圆心在 (tl.x, tl.y) 处）
        if (lx < tl.x && ly < tl.y && tl.x > 0.0f && tl.y > 0.0f) {
            const float nx = (lx - tl.x) / tl.x;
            const float ny = (ly - tl.y) / tl.y;
            if (nx * nx + ny * ny > 1.0f) {
                return false;
            }
        }
        // 右上角（椭圆圆心在 (w - tr.x, tr.y) 处）
        if (lx > w - tr.x && ly < tr.y && tr.x > 0.0f && tr.y > 0.0f) {
            const float nx = (lx - (w - tr.x)) / tr.x;
            const float ny = (ly - tr.y) / tr.y;
            if (nx * nx + ny * ny > 1.0f) {
                return false;
            }
        }
        // 左下角（椭圆圆心在 (bl.x, h - bl.y) 处）
        if (lx < bl.x && ly > h - bl.y && bl.x > 0.0f && bl.y > 0.0f) {
            const float nx = (lx - bl.x) / bl.x;
            const float ny = (ly - (h - bl.y)) / bl.y;
            if (nx * nx + ny * ny > 1.0f) {
                return false;
            }
        }
        // 右下角（椭圆圆心在 (w - br.x, h - br.y) 处）
        if (lx > w - br.x && ly > h - br.y && br.x > 0.0f && br.y > 0.0f) {
            const float nx = (lx - (w - br.x)) / br.x;
            const float ny = (ly - (h - br.y)) / br.y;
            if (nx * nx + ny * ny > 1.0f) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 从外侧圆角矩形推导内侧圆角矩形（用于绘制边框区域）。
     *
     * 内侧 Rect = 外侧 Rect 按 border 向内收缩。
     * 每角内侧半径 = max(0, 外侧半径 - 对应边厚度)：
     *   top_left.rx    -= border.left,  top_left.ry    -= border.top
     *   top_right.rx   -= border.right, top_right.ry   -= border.top
     *   bottom_right.rx -= border.right, bottom_right.ry -= border.bottom
     *   bottom_left.rx  -= border.left,  bottom_left.ry  -= border.bottom
     */
    [[nodiscard]] constexpr ComplexRoundedRect inner_rect(const Thickness& border) const noexcept {
        auto sub = [](float v, float t) constexpr noexcept -> float {
            return v > t ? v - t : 0.0f;
        };
        const CornerRadii inner_radii{
            {sub(radii.top_left.x,      border.left),  sub(radii.top_left.y,      border.top)},
            {sub(radii.top_right.x,     border.right), sub(radii.top_right.y,     border.top)},
            {sub(radii.bottom_right.x,  border.right), sub(radii.bottom_right.y,  border.bottom)},
            {sub(radii.bottom_left.x,   border.left),  sub(radii.bottom_left.y,   border.bottom)},
        };
        return ComplexRoundedRect{rect.deflated(border), inner_radii};
    }

    [[nodiscard]] constexpr bool operator==(const ComplexRoundedRect&) const noexcept = default;

private:
    /**
     * @brief CSS 规范等比缩放算法：
     * 计算四条边各自的缩放因子（= min(1, 边长 / 两角半径之和)），
     * 取四者中的最小值统一应用到所有角，确保任何相邻角不重叠。
     */
    [[nodiscard]] static constexpr CornerRadii clamp_radii(
        Rect r, CornerRadii in) noexcept
    {
        if (r.width <= 0.0f || r.height <= 0.0f) {
            return {};
        }

        // 先将各分量限制在非负范围
        auto fix = [](float v) constexpr noexcept -> float {
            return v < 0.0f ? 0.0f : v;
        };

        in.top_left      = {fix(in.top_left.x),      fix(in.top_left.y)};
        in.top_right     = {fix(in.top_right.x),     fix(in.top_right.y)};
        in.bottom_right  = {fix(in.bottom_right.x),  fix(in.bottom_right.y)};
        in.bottom_left   = {fix(in.bottom_left.x),   fix(in.bottom_left.y)};

        // 各边相邻角半径之和
        const float sum_top_x    = in.top_left.x    + in.top_right.x;
        const float sum_bottom_x = in.bottom_left.x + in.bottom_right.x;
        const float sum_left_y   = in.top_left.y    + in.bottom_left.y;
        const float sum_right_y  = in.top_right.y   + in.bottom_right.y;

        // 计算最终缩放因子（取各边约束中的最小值）
        auto fmin2 = [](float a, float b) constexpr noexcept -> float {
            return a < b ? a : b;
        };

        float f = 1.0f;
        if (sum_top_x    > r.width)  { f = fmin2(f, r.width  / sum_top_x); }
        if (sum_bottom_x > r.width)  { f = fmin2(f, r.width  / sum_bottom_x); }
        if (sum_left_y   > r.height) { f = fmin2(f, r.height / sum_left_y); }
        if (sum_right_y  > r.height) { f = fmin2(f, r.height / sum_right_y); }

        if (f < 1.0f) {
            in.top_left      = {in.top_left.x      * f, in.top_left.y      * f};
            in.top_right     = {in.top_right.x     * f, in.top_right.y     * f};
            in.bottom_right  = {in.bottom_right.x  * f, in.bottom_right.y  * f};
            in.bottom_left   = {in.bottom_left.x   * f, in.bottom_left.y   * f};
        }

        return in;
    }
};

} // namespace mine::math
