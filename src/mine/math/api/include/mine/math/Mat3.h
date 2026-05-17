/**
 * @file Mat3.h
 * @brief 3x3 浮点矩阵，主要用于二维仿射变换。
 */

#pragma once

#include <cstddef>
#include <cmath>

#include <mine/core/Assert.h>
#include <mine/core/Result.h>
#include <mine/core/Status.h>
#include <mine/math/Common.h>
#include <mine/math/Vec2.h>
#include <mine/math/Vec3.h>

namespace mine::math {

/**
 * @brief 3x3 行主序矩阵，按列向量语义参与运算。
 */
struct Mat3 {
    float m[3][3] {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };

    constexpr Mat3() noexcept = default;

    constexpr Mat3(
        float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22) noexcept
        : m{{m00, m01, m02}, {m10, m11, m12}, {m20, m21, m22}}
    {}

    [[nodiscard]] static constexpr Mat3 zero() noexcept {
        return {
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
        };
    }

    [[nodiscard]] static constexpr Mat3 identity() noexcept {
        return Mat3{};
    }

    [[nodiscard]] static constexpr Mat3 translation(float tx, float ty) noexcept {
        return {
            1.0f, 0.0f, tx,
            0.0f, 1.0f, ty,
            0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] static constexpr Mat3 scaling(float sx, float sy) noexcept {
        return {
            sx,   0.0f, 0.0f,
            0.0f, sy,   0.0f,
            0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] static Mat3 rotation(float radians_value) noexcept {
        const float c = std::cos(radians_value);
        const float s = std::sin(radians_value);
        return {
            c,   -s,  0.0f,
            s,    c,  0.0f,
            0.0f, 0.0f, 1.0f,
        };
    }

    [[nodiscard]] constexpr float* operator[](size_t row) noexcept {
        MINE_ASSERT(row < 3);
        return m[row];
    }

    [[nodiscard]] constexpr const float* operator[](size_t row) const noexcept {
        MINE_ASSERT(row < 3);
        return m[row];
    }

    [[nodiscard]] constexpr Vec3 row(size_t index) const noexcept {
        MINE_ASSERT(index < 3);
        return {m[index][0], m[index][1], m[index][2]};
    }

    [[nodiscard]] constexpr Vec3 column(size_t index) const noexcept {
        MINE_ASSERT(index < 3);
        return {m[0][index], m[1][index], m[2][index]};
    }

    [[nodiscard]] constexpr Mat3 operator*(const Mat3& rhs) const noexcept {
        Mat3 out = Mat3::zero();
        for (size_t row_index = 0; row_index < 3; ++row_index) {
            for (size_t col_index = 0; col_index < 3; ++col_index) {
                out[row_index][col_index] =
                    m[row_index][0] * rhs[0][col_index] +
                    m[row_index][1] * rhs[1][col_index] +
                    m[row_index][2] * rhs[2][col_index];
            }
        }
        return out;
    }

    constexpr Mat3& operator*=(const Mat3& rhs) noexcept {
        *this = *this * rhs;
        return *this;
    }

    [[nodiscard]] constexpr Vec3 operator*(Vec3 rhs) const noexcept {
        return {
            row(0).dot(rhs),
            row(1).dot(rhs),
            row(2).dot(rhs),
        };
    }

    [[nodiscard]] constexpr Vec2 transform_point(Vec2 point) const noexcept {
        const Vec3 result = *this * Vec3{point.x, point.y, 1.0f};
        return {result.x, result.y};
    }

    [[nodiscard]] constexpr Vec2 transform_vector(Vec2 vector) const noexcept {
        const Vec3 result = *this * Vec3{vector.x, vector.y, 0.0f};
        return {result.x, result.y};
    }

    [[nodiscard]] constexpr Mat3 transposed() const noexcept {
        return {
            m[0][0], m[1][0], m[2][0],
            m[0][1], m[1][1], m[2][1],
            m[0][2], m[1][2], m[2][2],
        };
    }

    [[nodiscard]] constexpr float determinant() const noexcept {
        return
            m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
            m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
            m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    [[nodiscard]] mine::core::Result<Mat3> inverted(
        float epsilon = kDefaultEpsilon) const noexcept
    {
        const float det = determinant();
        if (std::fabs(det) <= epsilon) {
            return mine::core::Result<Mat3>{
                mine::core::err_tag,
                mine::core::Status{mine::core::StatusCode::InvalidState}
            };
        }

        const float inv_det = 1.0f / det;
        return mine::core::Result<Mat3>{
            mine::core::ok_tag,
            Mat3{
                (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv_det,
                (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inv_det,
                (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv_det,
                (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inv_det,
                (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv_det,
                (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inv_det,
                (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv_det,
                (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inv_det,
                (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv_det,
            }
        };
    }

    [[nodiscard]] constexpr bool operator==(const Mat3&) const noexcept = default;
};

} // namespace mine::math