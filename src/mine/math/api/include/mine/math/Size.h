/**
 * @file Size.h
 * @brief 二维尺寸类型。
 */

#pragma once

#include <mine/math/Common.h>
#include <mine/math/Vec2.h>

namespace mine::math {

/**
 * @brief 二维尺寸，语义上表示宽高而非位置。
 */
struct Size {
    float width{0.0f};
    float height{0.0f};

    constexpr Size() noexcept = default;
    constexpr Size(float width_value, float height_value) noexcept
        : width{width_value}, height{height_value}
    {}

    [[nodiscard]] static constexpr Size zero() noexcept { return {}; }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] constexpr float area() const noexcept {
        return empty() ? 0.0f : width * height;
    }

    [[nodiscard]] constexpr Vec2 to_vec2() const noexcept {
        return {width, height};
    }

    [[nodiscard]] constexpr Size operator+(Size rhs) const noexcept {
        return {width + rhs.width, height + rhs.height};
    }

    [[nodiscard]] constexpr Size operator-(Size rhs) const noexcept {
        return {width - rhs.width, height - rhs.height};
    }

    [[nodiscard]] constexpr Size operator*(float scalar) const noexcept {
        return {width * scalar, height * scalar};
    }

    [[nodiscard]] constexpr Size operator/(float scalar) const noexcept {
        return {width / scalar, height / scalar};
    }

    constexpr Size& operator+=(Size rhs) noexcept {
        width += rhs.width;
        height += rhs.height;
        return *this;
    }

    constexpr Size& operator-=(Size rhs) noexcept {
        width -= rhs.width;
        height -= rhs.height;
        return *this;
    }

    constexpr Size& operator*=(float scalar) noexcept {
        width *= scalar;
        height *= scalar;
        return *this;
    }

    constexpr Size& operator/=(float scalar) noexcept {
        width /= scalar;
        height /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr Size clamped(Size min_size, Size max_size) const noexcept {
        return {
            clamp(width, min_size.width, max_size.width),
            clamp(height, min_size.height, max_size.height),
        };
    }

    [[nodiscard]] constexpr bool operator==(const Size&) const noexcept = default;
};

using Size2f = Size;

} // namespace mine::math