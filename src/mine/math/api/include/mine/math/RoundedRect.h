/**
 * @file RoundedRect.h
 * @brief 带圆角半径的轴对齐矩形。
 */

#pragma once

#include <mine/math/Rect.h>

namespace mine::math {

/**
 * @brief 圆角矩形，圆角半径会自动限制在矩形可容纳范围内。
 */
struct RoundedRect {
    Rect rect{};
    float radius_x{0.0f};
    float radius_y{0.0f};

    constexpr RoundedRect() noexcept = default;

    constexpr RoundedRect(Rect rect_value, float radius) noexcept
        : RoundedRect(rect_value, radius, radius)
    {}

    constexpr RoundedRect(Rect rect_value, float radius_x_value, float radius_y_value) noexcept
        : rect{rect_value}
        , radius_x{clamp(radius_x_value, 0.0f, rect_value.width * 0.5f)}
        , radius_y{clamp(radius_y_value, 0.0f, rect_value.height * 0.5f)}
    {}

    [[nodiscard]] constexpr Rect bounds() const noexcept { return rect; }

    [[nodiscard]] constexpr RoundedRect translated(Vec2 delta) const noexcept {
        return {rect.translated(delta), radius_x, radius_y};
    }

    [[nodiscard]] constexpr bool contains(Point point) const noexcept {
        if (!rect.contains(point)) {
            return false;
        }

        if (radius_x <= 0.0f || radius_y <= 0.0f) {
            return true;
        }

        const float left_inner = rect.left() + radius_x;
        const float right_inner = rect.right() - radius_x;
        const float top_inner = rect.top() + radius_y;
        const float bottom_inner = rect.bottom() - radius_y;

        if ((point.x >= left_inner && point.x <= right_inner) ||
            (point.y >= top_inner && point.y <= bottom_inner)) {
            return true;
        }

        const float center_x = point.x < left_inner ? left_inner : right_inner;
        const float center_y = point.y < top_inner ? top_inner : bottom_inner;
        const float dx = (point.x - center_x) / radius_x;
        const float dy = (point.y - center_y) / radius_y;
        return dx * dx + dy * dy <= 1.0f;
    }

    [[nodiscard]] constexpr bool operator==(const RoundedRect&) const noexcept = default;
};

} // namespace mine::math