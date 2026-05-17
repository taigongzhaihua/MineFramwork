/**
 * @file Color.h
 * @brief 线性空间 RGBA 颜色类型。
 */

#pragma once

#include <cstdint>

#include <mine/math/Common.h>

namespace mine::math {

/**
 * @brief 线性空间 RGBA 颜色。
 *
 * 分量范围通常为 [0, 1]，输入输出层负责与 sRGB 做转换。
 */
struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    constexpr Color() noexcept = default;
    constexpr Color(float red, float green, float blue, float alpha = 1.0f) noexcept
        : r{red}, g{green}, b{blue}, a{alpha}
    {}

    [[nodiscard]] static constexpr Color from_rgba8(
        uint8_t red,
        uint8_t green,
        uint8_t blue,
        uint8_t alpha = 255) noexcept
    {
        constexpr float inv = 1.0f / 255.0f;
        return {
            static_cast<float>(red) * inv,
            static_cast<float>(green) * inv,
            static_cast<float>(blue) * inv,
            static_cast<float>(alpha) * inv,
        };
    }

    /// 从 0xRRGGBB 颜色字面值创建不透明颜色。
    [[nodiscard]] static constexpr Color from_rgb_u32(uint32_t rgb) noexcept {
        return from_rgba8(
            static_cast<uint8_t>((rgb >> 16) & 0xFFu),
            static_cast<uint8_t>((rgb >> 8) & 0xFFu),
            static_cast<uint8_t>(rgb & 0xFFu),
            255);
    }

    /// 从 0xRRGGBBAA 打包值创建颜色。
    [[nodiscard]] static constexpr Color from_rgba_u32(uint32_t rgba) noexcept {
        return from_rgba8(
            static_cast<uint8_t>((rgba >> 24) & 0xFFu),
            static_cast<uint8_t>((rgba >> 16) & 0xFFu),
            static_cast<uint8_t>((rgba >> 8) & 0xFFu),
            static_cast<uint8_t>(rgba & 0xFFu));
    }

    [[nodiscard]] constexpr Color clamped() const noexcept {
        return {clamp01(r), clamp01(g), clamp01(b), clamp01(a)};
    }

    [[nodiscard]] constexpr Color with_alpha(float alpha) const noexcept {
        return {r, g, b, alpha};
    }

    [[nodiscard]] constexpr Color premultiplied() const noexcept {
        return {r * a, g * a, b * a, a};
    }

    [[nodiscard]] Color blend_over(Color dst) const noexcept {
        const float out_a = a + dst.a * (1.0f - a);
        if (out_a <= kDefaultEpsilon) {
            return Transparent;
        }

        const float out_r = (r * a + dst.r * dst.a * (1.0f - a)) / out_a;
        const float out_g = (g * a + dst.g * dst.a * (1.0f - a)) / out_a;
        const float out_b = (b * a + dst.b * dst.a * (1.0f - a)) / out_a;
        return {out_r, out_g, out_b, out_a};
    }

    [[nodiscard]] uint32_t to_rgba8() const noexcept {
        const Color c = clamped();
        const auto pack = [](float value) noexcept -> uint32_t {
            return static_cast<uint32_t>(value * 255.0f + 0.5f);
        };
        return (pack(c.r) << 24) |
               (pack(c.g) << 16) |
               (pack(c.b) << 8) |
               pack(c.a);
    }

    [[nodiscard]] constexpr Color operator+(Color rhs) const noexcept {
        return {r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a};
    }

    [[nodiscard]] constexpr Color operator-(Color rhs) const noexcept {
        return {r - rhs.r, g - rhs.g, b - rhs.b, a - rhs.a};
    }

    [[nodiscard]] constexpr Color operator*(float scalar) const noexcept {
        return {r * scalar, g * scalar, b * scalar, a * scalar};
    }

    [[nodiscard]] constexpr Color modulate(Color rhs) const noexcept {
        return {r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a};
    }

    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;

    static const Color Transparent;
    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
};

[[nodiscard]] inline constexpr Color operator*(float scalar, Color value) noexcept {
    return value * scalar;
}

inline constexpr Color Color::Transparent{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Color Color::Black{0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr Color Color::White{1.0f, 1.0f, 1.0f, 1.0f};
inline constexpr Color Color::Red{1.0f, 0.0f, 0.0f, 1.0f};
inline constexpr Color Color::Green{0.0f, 1.0f, 0.0f, 1.0f};
inline constexpr Color Color::Blue{0.0f, 0.0f, 1.0f, 1.0f};

} // namespace mine::math