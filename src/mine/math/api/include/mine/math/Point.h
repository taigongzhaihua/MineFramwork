/**
 * @file Point.h
 * @brief 二维点类型。
 */

#pragma once

#include <mine/math/Vec2.h>

namespace mine::math {

/**
 * @brief 二维点，语义上表示位置而非方向。
 */
struct Point {
    float x{0.0f};
    float y{0.0f};

    constexpr Point() noexcept = default;
    constexpr Point(float x_value, float y_value) noexcept : x{x_value}, y{y_value} {}
    explicit constexpr Point(Vec2 value) noexcept : x{value.x}, y{value.y} {}

    [[nodiscard]] static constexpr Point zero() noexcept { return {}; }

    [[nodiscard]] constexpr Vec2 to_vec2() const noexcept { return {x, y}; }

    [[nodiscard]] constexpr Point operator+(Vec2 delta) const noexcept {
        return {x + delta.x, y + delta.y};
    }

    [[nodiscard]] constexpr Point operator-(Vec2 delta) const noexcept {
        return {x - delta.x, y - delta.y};
    }

    [[nodiscard]] constexpr Vec2 operator-(Point rhs) const noexcept {
        return {x - rhs.x, y - rhs.y};
    }

    constexpr Point& operator+=(Vec2 delta) noexcept {
        x += delta.x;
        y += delta.y;
        return *this;
    }

    constexpr Point& operator-=(Vec2 delta) noexcept {
        x -= delta.x;
        y -= delta.y;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Point&) const noexcept = default;
};

using Point2f = Point;

} // namespace mine::math