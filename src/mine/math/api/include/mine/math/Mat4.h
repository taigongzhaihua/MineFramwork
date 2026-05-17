/**
 * @file Mat4.h
 * @brief 4x4 浮点矩阵。
 */

#pragma once

#include <cstddef>
#include <cmath>

#include <mine/core/Assert.h>
#include <mine/math/Common.h>
#include <mine/math/Vec3.h>
#include <mine/math/Vec4.h>

namespace mine::math {

/**
 * @brief 4x4 行主序矩阵，按列向量语义参与运算。
 */
struct Mat4 {
    float m[4][4] {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    constexpr Mat4() noexcept = default;

    constexpr Mat4(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) noexcept
        : m{
            {m00, m01, m02, m03},
            {m10, m11, m12, m13},
            {m20, m21, m22, m23},
            {m30, m31, m32, m33},
        }
    {}

    [[nodiscard]] static constexpr Mat4 zero() noexcept {
        return {
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
        };
    }

    [[nodiscard]] static constexpr Mat4 identity() noexcept {
        return Mat4{};
    }

    [[nodiscard]] static constexpr Mat4 translation(float tx, float ty, float tz) noexcept {
        return {
            1.0f, 0.0f, 0.0f, tx,
            0.0f, 1.0f, 0.0f, ty,
            0.0f, 0.0f, 1.0f, tz,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] static constexpr Mat4 scaling(float sx, float sy, float sz) noexcept {
        return {
            sx,   0.0f, 0.0f, 0.0f,
            0.0f, sy,   0.0f, 0.0f,
            0.0f, 0.0f, sz,   0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] static constexpr Mat4 scaling(float uniform_scale) noexcept {
        return scaling(uniform_scale, uniform_scale, uniform_scale);
    }

    [[nodiscard]] static Mat4 rotation_x(float radians_value) noexcept {
        const float c = std::cos(radians_value);
        const float s = std::sin(radians_value);
        return {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, c,   -s,   0.0f,
            0.0f, s,    c,   0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] static Mat4 rotation_y(float radians_value) noexcept {
        const float c = std::cos(radians_value);
        const float s = std::sin(radians_value);
        return {
            c,    0.0f, s,   0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
           -s,    0.0f, c,   0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] static Mat4 rotation_z(float radians_value) noexcept {
        const float c = std::cos(radians_value);
        const float s = std::sin(radians_value);
        return {
            c,   -s,   0.0f, 0.0f,
            s,    c,   0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] constexpr float* operator[](size_t row) noexcept {
        MINE_ASSERT(row < 4);
        return m[row];
    }

    [[nodiscard]] constexpr const float* operator[](size_t row) const noexcept {
        MINE_ASSERT(row < 4);
        return m[row];
    }

    [[nodiscard]] constexpr Vec4 row(size_t index) const noexcept {
        MINE_ASSERT(index < 4);
        return {m[index][0], m[index][1], m[index][2], m[index][3]};
    }

    [[nodiscard]] constexpr Mat4 operator*(const Mat4& rhs) const noexcept {
        Mat4 out = Mat4::zero();
        for (size_t row_index = 0; row_index < 4; ++row_index) {
            for (size_t col_index = 0; col_index < 4; ++col_index) {
                out[row_index][col_index] =
                    m[row_index][0] * rhs[0][col_index] +
                    m[row_index][1] * rhs[1][col_index] +
                    m[row_index][2] * rhs[2][col_index] +
                    m[row_index][3] * rhs[3][col_index];
            }
        }
        return out;
    }

    constexpr Mat4& operator*=(const Mat4& rhs) noexcept {
        *this = *this * rhs;
        return *this;
    }

    [[nodiscard]] constexpr Vec4 operator*(Vec4 rhs) const noexcept {
        return {
            row(0).dot(rhs),
            row(1).dot(rhs),
            row(2).dot(rhs),
            row(3).dot(rhs),
        };
    }

    [[nodiscard]] Vec3 transform_point(Vec3 point) const noexcept {
        const Vec4 result = *this * Vec4{point.x, point.y, point.z, 1.0f};
        if (std::fabs(result.w) <= kDefaultEpsilon) {
            return {result.x, result.y, result.z};
        }
        return {result.x / result.w, result.y / result.w, result.z / result.w};
    }

    [[nodiscard]] constexpr Vec3 transform_vector(Vec3 vector) const noexcept {
        const Vec4 result = *this * Vec4{vector.x, vector.y, vector.z, 0.0f};
        return {result.x, result.y, result.z};
    }

    [[nodiscard]] constexpr Mat4 transposed() const noexcept {
        return {
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3],
        };
    }

    [[nodiscard]] constexpr bool operator==(const Mat4&) const noexcept = default;
};

} // namespace mine::math