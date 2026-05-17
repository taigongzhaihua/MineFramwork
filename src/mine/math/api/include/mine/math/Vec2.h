/**
 * @file Vec2.h
 * @brief 二维浮点向量。
 */

#pragma once

#include <cstddef>
#include <cmath>

#include <mine/core/Assert.h>
#include <mine/math/Common.h>

namespace mine::math {

/**
 * @brief 二维向量，适用于位置偏移、缩放系数、方向等场景。
 */
struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() noexcept = default;
    constexpr Vec2(float x_value, float y_value) noexcept : x{x_value}, y{y_value} {}

    [[nodiscard]] static constexpr Vec2 zero() noexcept { return {}; }
    [[nodiscard]] static constexpr Vec2 one() noexcept { return {1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec2 unit_x() noexcept { return {1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec2 unit_y() noexcept { return {0.0f, 1.0f}; }

    [[nodiscard]] constexpr float& operator[](size_t index) noexcept {
        MINE_ASSERT(index < 2);
        return (&x)[index];
    }

    [[nodiscard]] constexpr const float& operator[](size_t index) const noexcept {
        MINE_ASSERT(index < 2);
        return (&x)[index];
    }

    [[nodiscard]] constexpr Vec2 operator+() const noexcept { return *this; }
    [[nodiscard]] constexpr Vec2 operator-() const noexcept { return {-x, -y}; }

    [[nodiscard]] constexpr Vec2 operator+(Vec2 rhs) const noexcept {
        return {x + rhs.x, y + rhs.y};
    }

    [[nodiscard]] constexpr Vec2 operator-(Vec2 rhs) const noexcept {
        return {x - rhs.x, y - rhs.y};
    }

    [[nodiscard]] constexpr Vec2 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar};
    }

    [[nodiscard]] constexpr Vec2 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar};
    }

    constexpr Vec2& operator+=(Vec2 rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    constexpr Vec2& operator-=(Vec2 rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    constexpr Vec2& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr Vec2& operator/=(float scalar) noexcept {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr float dot(Vec2 rhs) const noexcept {
        return x * rhs.x + y * rhs.y;
    }

    /// 二维叉积返回标量，表示平行四边形有向面积。
    [[nodiscard]] constexpr float cross(Vec2 rhs) const noexcept {
        return x * rhs.y - y * rhs.x;
    }

    [[nodiscard]] constexpr float length_squared() const noexcept {
        return dot(*this);
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] Vec2 normalized(float epsilon = kDefaultEpsilon) const noexcept {
        const float len = length();
        if (len <= epsilon) {
            return zero();
        }
        return *this / len;
    }

    [[nodiscard]] constexpr bool operator==(const Vec2&) const noexcept = default;
};

[[nodiscard]] inline constexpr Vec2 operator*(float scalar, Vec2 value) noexcept {
    return value * scalar;
}

} // namespace mine::math