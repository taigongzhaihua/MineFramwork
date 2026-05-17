/**
 * @file Vec3.h
 * @brief 三维浮点向量。
 */

#pragma once

#include <cstddef>
#include <cmath>

#include <mine/core/Assert.h>
#include <mine/math/Common.h>

namespace mine::math {

/**
 * @brief 三维向量，适用于矩阵运算与三维几何基础计算。
 */
struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() noexcept = default;
    constexpr Vec3(float x_value, float y_value, float z_value) noexcept
        : x{x_value}, y{y_value}, z{z_value}
    {}

    [[nodiscard]] static constexpr Vec3 zero() noexcept { return {}; }
    [[nodiscard]] static constexpr Vec3 one() noexcept { return {1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec3 unit_x() noexcept { return {1.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 unit_y() noexcept { return {0.0f, 1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 unit_z() noexcept { return {0.0f, 0.0f, 1.0f}; }

    [[nodiscard]] constexpr float& operator[](size_t index) noexcept {
        MINE_ASSERT(index < 3);
        return (&x)[index];
    }

    [[nodiscard]] constexpr const float& operator[](size_t index) const noexcept {
        MINE_ASSERT(index < 3);
        return (&x)[index];
    }

    [[nodiscard]] constexpr Vec3 operator+() const noexcept { return *this; }
    [[nodiscard]] constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }

    [[nodiscard]] constexpr Vec3 operator+(Vec3 rhs) const noexcept {
        return {x + rhs.x, y + rhs.y, z + rhs.z};
    }

    [[nodiscard]] constexpr Vec3 operator-(Vec3 rhs) const noexcept {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }

    [[nodiscard]] constexpr Vec3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }

    [[nodiscard]] constexpr Vec3 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar, z / scalar};
    }

    constexpr Vec3& operator+=(Vec3 rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    constexpr Vec3& operator-=(Vec3 rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    constexpr Vec3& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    constexpr Vec3& operator/=(float scalar) noexcept {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr float dot(Vec3 rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    [[nodiscard]] constexpr Vec3 cross(Vec3 rhs) const noexcept {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x,
        };
    }

    [[nodiscard]] constexpr float length_squared() const noexcept {
        return dot(*this);
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] Vec3 normalized(float epsilon = kDefaultEpsilon) const noexcept {
        const float len = length();
        if (len <= epsilon) {
            return zero();
        }
        return *this / len;
    }

    [[nodiscard]] constexpr bool operator==(const Vec3&) const noexcept = default;
};

[[nodiscard]] inline constexpr Vec3 operator*(float scalar, Vec3 value) noexcept {
    return value * scalar;
}

} // namespace mine::math