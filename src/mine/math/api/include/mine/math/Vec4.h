/**
 * @file Vec4.h
 * @brief 四维浮点向量。
 */

#pragma once

#include <cstddef>
#include <cmath>

#include <mine/core/Assert.h>
#include <mine/math/Common.h>

namespace mine::math {

/**
 * @brief 四维向量，适用于齐次坐标与颜色/矩阵运算场景。
 */
struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};

    constexpr Vec4() noexcept = default;
    constexpr Vec4(float x_value, float y_value, float z_value, float w_value) noexcept
        : x{x_value}, y{y_value}, z{z_value}, w{w_value}
    {}

    [[nodiscard]] static constexpr Vec4 zero() noexcept { return {}; }
    [[nodiscard]] static constexpr Vec4 one() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }

    [[nodiscard]] constexpr float& operator[](size_t index) noexcept {
        MINE_ASSERT(index < 4);
        return (&x)[index];
    }

    [[nodiscard]] constexpr const float& operator[](size_t index) const noexcept {
        MINE_ASSERT(index < 4);
        return (&x)[index];
    }

    [[nodiscard]] constexpr Vec4 operator+() const noexcept { return *this; }
    [[nodiscard]] constexpr Vec4 operator-() const noexcept { return {-x, -y, -z, -w}; }

    [[nodiscard]] constexpr Vec4 operator+(Vec4 rhs) const noexcept {
        return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w};
    }

    [[nodiscard]] constexpr Vec4 operator-(Vec4 rhs) const noexcept {
        return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w};
    }

    [[nodiscard]] constexpr Vec4 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar, w * scalar};
    }

    [[nodiscard]] constexpr Vec4 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar, z / scalar, w / scalar};
    }

    constexpr Vec4& operator+=(Vec4 rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }

    constexpr Vec4& operator-=(Vec4 rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }

    constexpr Vec4& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    constexpr Vec4& operator/=(float scalar) noexcept {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr float dot(Vec4 rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }

    [[nodiscard]] constexpr float length_squared() const noexcept {
        return dot(*this);
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] Vec4 normalized(float epsilon = kDefaultEpsilon) const noexcept {
        const float len = length();
        if (len <= epsilon) {
            return zero();
        }
        return *this / len;
    }

    [[nodiscard]] constexpr bool operator==(const Vec4&) const noexcept = default;
};

[[nodiscard]] inline constexpr Vec4 operator*(float scalar, Vec4 value) noexcept {
    return value * scalar;
}

} // namespace mine::math