/**
 * @file Rect.h
 * @brief 轴对齐矩形类型。
 */

#pragma once

#include <mine/math/Point.h>
#include <mine/math/Size.h>

namespace mine::math {

/**
 * @brief 轴对齐矩形，使用左上角 + 宽高表达。
 */
struct Rect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    constexpr Rect() noexcept = default;
    constexpr Rect(float x_value, float y_value, float width_value, float height_value) noexcept
        : x{x_value}, y{y_value}, width{width_value}, height{height_value}
    {}
    constexpr Rect(Point origin, Size size) noexcept
        : x{origin.x}, y{origin.y}, width{size.width}, height{size.height}
    {}

    [[nodiscard]] static constexpr Rect from_points(Point a, Point b) noexcept {
        const float left = a.x < b.x ? a.x : b.x;
        const float top = a.y < b.y ? a.y : b.y;
        const float right = a.x > b.x ? a.x : b.x;
        const float bottom = a.y > b.y ? a.y : b.y;
        return {left, top, right - left, bottom - top};
    }

    [[nodiscard]] constexpr float left() const noexcept { return x; }
    [[nodiscard]] constexpr float top() const noexcept { return y; }
    [[nodiscard]] constexpr float right() const noexcept { return x + width; }
    [[nodiscard]] constexpr float bottom() const noexcept { return y + height; }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] constexpr float area() const noexcept {
        return empty() ? 0.0f : width * height;
    }

    [[nodiscard]] constexpr Point origin() const noexcept { return {x, y}; }
    [[nodiscard]] constexpr Size size() const noexcept { return {width, height}; }
    [[nodiscard]] constexpr Point top_left() const noexcept { return {left(), top()}; }
    [[nodiscard]] constexpr Point top_right() const noexcept { return {right(), top()}; }
    [[nodiscard]] constexpr Point bottom_left() const noexcept { return {left(), bottom()}; }
    [[nodiscard]] constexpr Point bottom_right() const noexcept { return {right(), bottom()}; }
    [[nodiscard]] constexpr Point center() const noexcept {
        return {x + width * 0.5f, y + height * 0.5f};
    }

    [[nodiscard]] constexpr bool contains(Point point) const noexcept {
        return point.x >= left() && point.x <= right() &&
               point.y >= top() && point.y <= bottom();
    }

    [[nodiscard]] constexpr bool contains(Rect other) const noexcept {
        if (other.empty()) {
            return true;
        }
        return other.left() >= left() && other.right() <= right() &&
               other.top() >= top() && other.bottom() <= bottom();
    }

    [[nodiscard]] constexpr bool intersects(Rect other) const noexcept {
        return right() > other.left() && other.right() > left() &&
               bottom() > other.top() && other.bottom() > top();
    }

    [[nodiscard]] constexpr Rect translated(Vec2 delta) const noexcept {
        return {x + delta.x, y + delta.y, width, height};
    }

    [[nodiscard]] constexpr Rect inflated(float horizontal, float vertical) const noexcept {
        return {x - horizontal, y - vertical, width + horizontal * 2.0f, height + vertical * 2.0f};
    }

    [[nodiscard]] constexpr Rect deflated(float horizontal, float vertical) const noexcept {
        return inflated(-horizontal, -vertical);
    }

    [[nodiscard]] constexpr Rect intersection(Rect other) const noexcept {
        const float intersect_left = left() > other.left() ? left() : other.left();
        const float intersect_top = top() > other.top() ? top() : other.top();
        const float intersect_right = right() < other.right() ? right() : other.right();
        const float intersect_bottom = bottom() < other.bottom() ? bottom() : other.bottom();
        if (intersect_right <= intersect_left || intersect_bottom <= intersect_top) {
            return {intersect_left, intersect_top, 0.0f, 0.0f};
        }
        return {
            intersect_left,
            intersect_top,
            intersect_right - intersect_left,
            intersect_bottom - intersect_top,
        };
    }

    [[nodiscard]] constexpr Rect union_with(Rect other) const noexcept {
        if (empty()) {
            return other;
        }
        if (other.empty()) {
            return *this;
        }
        const float union_left = left() < other.left() ? left() : other.left();
        const float union_top = top() < other.top() ? top() : other.top();
        const float union_right = right() > other.right() ? right() : other.right();
        const float union_bottom = bottom() > other.bottom() ? bottom() : other.bottom();
        return {union_left, union_top, union_right - union_left, union_bottom - union_top};
    }

    [[nodiscard]] constexpr bool operator==(const Rect&) const noexcept = default;
};

using Rectf = Rect;

} // namespace mine::math