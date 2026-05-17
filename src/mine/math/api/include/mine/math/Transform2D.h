/**
 * @file Transform2D.h
 * @brief 二维仿射变换包装类型。
 */

#pragma once

#include <mine/core/Result.h>
#include <mine/math/Mat3.h>
#include <mine/math/Point.h>
#include <mine/math/Rect.h>

namespace mine::math {

/**
 * @brief 二维仿射变换。
 *
 * 内部使用 3x3 矩阵，支持平移、缩放、旋转及组合。
 */
struct Transform2D {
    Mat3 matrix_{};

    constexpr Transform2D() noexcept = default;
    explicit constexpr Transform2D(const Mat3& matrix_value) noexcept : matrix_{matrix_value} {}

    [[nodiscard]] static constexpr Transform2D identity() noexcept {
        return Transform2D{Mat3::identity()};
    }

    [[nodiscard]] static constexpr Transform2D translation(float tx, float ty) noexcept {
        return Transform2D{Mat3::translation(tx, ty)};
    }

    [[nodiscard]] static constexpr Transform2D translation(Vec2 offset) noexcept {
        return translation(offset.x, offset.y);
    }

    [[nodiscard]] static constexpr Transform2D scale(float uniform_scale) noexcept {
        return Transform2D{Mat3::scaling(uniform_scale, uniform_scale)};
    }

    [[nodiscard]] static constexpr Transform2D scale(float sx, float sy) noexcept {
        return Transform2D{Mat3::scaling(sx, sy)};
    }

    [[nodiscard]] static Transform2D rotation(float radians_value) noexcept {
        return Transform2D{Mat3::rotation(radians_value)};
    }

    [[nodiscard]] static Transform2D rotation_about(float radians_value, Point pivot) noexcept {
        return translation({pivot.x, pivot.y}) *
               rotation(radians_value) *
               translation({-pivot.x, -pivot.y});
    }

    [[nodiscard]] constexpr const Mat3& matrix() const noexcept { return matrix_; }

    [[nodiscard]] constexpr Point apply(Point point) const noexcept {
        const Vec2 value = matrix_.transform_point({point.x, point.y});
        return {value.x, value.y};
    }

    [[nodiscard]] constexpr Vec2 apply(Vec2 vector) const noexcept {
        return matrix_.transform_vector(vector);
    }

    [[nodiscard]] Rect apply(Rect rect) const noexcept {
        const Point p0 = apply(rect.top_left());
        const Point p1 = apply(rect.top_right());
        const Point p2 = apply(rect.bottom_left());
        const Point p3 = apply(rect.bottom_right());

        const float left = p0.x < p1.x ? (p0.x < p2.x ? (p0.x < p3.x ? p0.x : p3.x)
                                                      : (p2.x < p3.x ? p2.x : p3.x))
                                       : (p1.x < p2.x ? (p1.x < p3.x ? p1.x : p3.x)
                                                      : (p2.x < p3.x ? p2.x : p3.x));
        const float top = p0.y < p1.y ? (p0.y < p2.y ? (p0.y < p3.y ? p0.y : p3.y)
                                                     : (p2.y < p3.y ? p2.y : p3.y))
                                      : (p1.y < p2.y ? (p1.y < p3.y ? p1.y : p3.y)
                                                     : (p2.y < p3.y ? p2.y : p3.y));
        const float right = p0.x > p1.x ? (p0.x > p2.x ? (p0.x > p3.x ? p0.x : p3.x)
                                                       : (p2.x > p3.x ? p2.x : p3.x))
                                        : (p1.x > p2.x ? (p1.x > p3.x ? p1.x : p3.x)
                                                       : (p2.x > p3.x ? p2.x : p3.x));
        const float bottom = p0.y > p1.y ? (p0.y > p2.y ? (p0.y > p3.y ? p0.y : p3.y)
                                                         : (p2.y > p3.y ? p2.y : p3.y))
                                          : (p1.y > p2.y ? (p1.y > p3.y ? p1.y : p3.y)
                                                         : (p2.y > p3.y ? p2.y : p3.y));
        return {left, top, right - left, bottom - top};
    }

    [[nodiscard]] mine::core::Result<Transform2D> inverted(
        float epsilon = kDefaultEpsilon) const noexcept
    {
        auto inv = matrix_.inverted(epsilon);
        if (!inv) {
            return mine::core::Result<Transform2D>{mine::core::err_tag, inv.error()};
        }
        return mine::core::Result<Transform2D>{mine::core::ok_tag, Transform2D{inv.value()}};
    }

    [[nodiscard]] constexpr Transform2D operator*(const Transform2D& rhs) const noexcept {
        return Transform2D{matrix_ * rhs.matrix_};
    }

    constexpr Transform2D& operator*=(const Transform2D& rhs) noexcept {
        matrix_ *= rhs.matrix_;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Transform2D&) const noexcept = default;
};

} // namespace mine::math