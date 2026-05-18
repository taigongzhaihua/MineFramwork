/**
 * @file Thickness.h
 * @brief 四边各自独立的厚度/间距描述（left、top、right、bottom）。
 */

#pragma once

namespace mine::math {

/**
 * @brief 四边各自独立的厚度值，对应 CSS box model 中的 border-width/padding/margin。
 *
 * 值通常为非负浮点数；负值在语义上表示向外扩展（与 inflate 等效）。
 * 可与 Rect / ComplexRoundedRect 配合进行收缩（deflate）或扩展（inflate）操作。
 */
struct Thickness {
    float left{};
    float top{};
    float right{};
    float bottom{};

    constexpr Thickness() noexcept = default;

    constexpr Thickness(float left_val, float top_val,
                        float right_val, float bottom_val) noexcept
        : left{left_val}
        , top{top_val}
        , right{right_val}
        , bottom{bottom_val}
    {}

    /// 四边相同
    [[nodiscard]] static constexpr Thickness uniform(float v) noexcept {
        return {v, v, v, v};
    }

    /// 水平（left/right）和垂直（top/bottom）分别指定
    [[nodiscard]] static constexpr Thickness symmetric(float horizontal, float vertical) noexcept {
        return {horizontal, vertical, horizontal, vertical};
    }

    /// left + right
    [[nodiscard]] constexpr float horizontal() const noexcept { return left + right; }

    /// top + bottom
    [[nodiscard]] constexpr float vertical() const noexcept { return top + bottom; }

    /// 是否四边全为零
    [[nodiscard]] constexpr bool is_zero() const noexcept {
        return left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f;
    }

    /// 是否四边相同
    [[nodiscard]] constexpr bool is_uniform() const noexcept {
        return left == top && top == right && right == bottom;
    }

    [[nodiscard]] constexpr Thickness operator+(const Thickness& other) const noexcept {
        return {left + other.left, top + other.top,
                right + other.right, bottom + other.bottom};
    }

    [[nodiscard]] constexpr Thickness operator*(float scale) const noexcept {
        return {left * scale, top * scale, right * scale, bottom * scale};
    }

    [[nodiscard]] constexpr bool operator==(const Thickness&) const noexcept = default;
};

} // namespace mine::math
