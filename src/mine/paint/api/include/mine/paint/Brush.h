/**
 * @file Brush.h
 * @brief 填充画刷类型。
 *
 * Brush 是一个轻量的值类型，可内联存储在 DrawCmd 中。
 * M0 阶段仅实现纯色画刷（SolidColor）；渐变画刷仅定义枚举占位，
 * 完整实现留待 M1 渲染管线完善后补充。
 *
 * 依赖：mine.math（Color）
 */

#pragma once

#include <mine/math/Color.h>

namespace mine::paint {

/**
 * @brief 画刷类型枚举。
 */
enum class BrushKind : uint8_t {
    SolidColor,       ///< 纯色填充
    LinearGradient,   ///< 线性渐变（M0 占位，暂无渲染实现）
    RadialGradient,   ///< 径向渐变（M0 占位，暂无渲染实现）
    ImageBrush,       ///< 图像画刷（M1+ 实现）
};

/**
 * @brief 渐变色标（颜色 + 偏移量）。
 *
 * 用于线性/径向渐变中描述颜色在路径上的分布。
 * offset 范围为 [0.0, 1.0]。
 */
struct GradientStop {
    float       offset{0.0f};  ///< 渐变路径上的归一化位置
    math::Color color{};       ///< 此位置对应的线性空间颜色
};

/**
 * @brief 填充画刷。
 *
 * 设计为可拷贝的轻量值类型，可直接嵌入 DrawCmd。
 *
 * 使用静态工厂函数创建具体类型的画刷：
 * @code
 *   auto brush = mine::paint::Brush::solid(mine::math::Color{0.2f, 0.6f, 1.0f});
 * @endcode
 */
class Brush {
public:
    // ── 工厂函数 ────────────────────────────────────────────────────────

    /// 创建纯色画刷。
    [[nodiscard]] static Brush solid(math::Color color) noexcept {
        Brush b;
        b.kind_  = BrushKind::SolidColor;
        b.color_ = color;
        return b;
    }

    /// 创建纯色画刷（从 0xRRGGBB 整数字面值，Alpha 默认为 1.0）。
    [[nodiscard]] static Brush solid_rgb(uint32_t rgb) noexcept {
        return solid(math::Color::from_rgb_u32(rgb));
    }

    /// 创建纯色画刷（从 0xRRGGBBAA 整数字面值）。
    [[nodiscard]] static Brush solid_rgba(uint32_t rgba) noexcept {
        return solid(math::Color::from_rgba_u32(rgba));
    }

    // ── 访问器 ──────────────────────────────────────────────────────────

    /// 返回画刷类型。
    [[nodiscard]] BrushKind kind() const noexcept { return kind_; }

    /**
     * @brief 返回纯色画刷的颜色值（线性空间）。
     * @pre kind() == BrushKind::SolidColor
     */
    [[nodiscard]] math::Color color() const noexcept { return color_; }

    /// 判断画刷是否完全透明（Alpha == 0）。
    [[nodiscard]] bool is_transparent() const noexcept {
        return kind_ == BrushKind::SolidColor && color_.a <= 0.0f;
    }

    // ── 默认构造 ────────────────────────────────────────────────────────

    /// 默认构造为不透明黑色纯色画刷。
    Brush() noexcept = default;

private:
    BrushKind   kind_ {BrushKind::SolidColor};
    math::Color color_{0.0f, 0.0f, 0.0f, 1.0f};  ///< 纯色值（SolidColor 时有效）
};

} // namespace mine::paint
