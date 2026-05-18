/**
 * @file CornerRadii.h
 * @brief 四角各自独立的椭圆圆角半径。
 */

#pragma once

#include <mine/math/Vec2.h>

namespace mine::math {

/**
 * @brief 四角各自独立的椭圆圆角半径，每角用 (rx, ry) 椭圆半轴表示。
 *
 * 角标顺序（顺时针）：
 *   top_left → top_right → bottom_right → bottom_left
 *
 * 当 rx == ry 时退化为圆形圆角；当 rx == 0 || ry == 0 时退化为直角。
 */
struct CornerRadii {
    Vec2 top_left{};
    Vec2 top_right{};
    Vec2 bottom_right{};
    Vec2 bottom_left{};

    constexpr CornerRadii() noexcept = default;

    constexpr CornerRadii(Vec2 tl, Vec2 tr, Vec2 br, Vec2 bl) noexcept
        : top_left{tl}
        , top_right{tr}
        , bottom_right{br}
        , bottom_left{bl}
    {}

    /// 四角完全相同的圆形圆角（rx == ry == r）
    [[nodiscard]] static constexpr CornerRadii uniform(float r) noexcept {
        return uniform(r, r);
    }

    /// 四角完全相同的椭圆圆角（rx 与 ry 可不同）
    [[nodiscard]] static constexpr CornerRadii uniform(float rx, float ry) noexcept {
        const Vec2 v{rx, ry};
        return {v, v, v, v};
    }

    /// 四角各给一个对称圆角半径（每角 rx == ry）
    [[nodiscard]] static constexpr CornerRadii from_corners(
        float tl, float tr, float br, float bl) noexcept
    {
        return {{tl, tl}, {tr, tr}, {br, br}, {bl, bl}};
    }

    /// 是否全为零（即无圆角）
    [[nodiscard]] constexpr bool is_zero() const noexcept {
        return top_left.x    <= 0.0f && top_left.y    <= 0.0f
            && top_right.x   <= 0.0f && top_right.y   <= 0.0f
            && bottom_right.x <= 0.0f && bottom_right.y <= 0.0f
            && bottom_left.x  <= 0.0f && bottom_left.y  <= 0.0f;
    }

    /// 是否四角完全相同
    [[nodiscard]] constexpr bool is_uniform() const noexcept {
        return top_left == top_right
            && top_right == bottom_right
            && bottom_right == bottom_left;
    }

    [[nodiscard]] constexpr bool operator==(const CornerRadii&) const noexcept = default;
};

} // namespace mine::math
