/**
 * @file GridLength.h
 * @brief Grid 行列尺寸描述，支持像素（Pixel）、自动（Auto）、比例（Star）三种模式。
 */

#pragma once

#include <cstdint>
#include <limits>

namespace mine::ui {

/**
 * @brief Grid 行列尺寸描述符。
 *
 * 三种模式：
 *   - Pixel ：固定像素尺寸（value 为像素数）
 *   - Auto  ：根据子元素期望尺寸自动确定（value 忽略）
 *   - Star  ：按比例分配剩余空间（value 为权重，默认 1.0f）
 *
 * 示例：
 * @code
 *   GridLength::pixel(100)    // 固定 100 像素
 *   GridLength::auto_()       // 自动
 *   GridLength::star(2.0f)    // 2* 权重
 *   GridLength::star()        // 1* 权重（默认）
 * @endcode
 */
struct GridLength {
    /// 尺寸类型
    enum class Type : uint8_t {
        Pixel = 0,  ///< 固定像素
        Auto  = 1,  ///< 自动（内容驱动）
        Star  = 2,  ///< 比例（剩余空间权重分配）
    };

    float value = 1.0f;       ///< 像素值或 Star 权重（Auto 时忽略）
    Type  type  = Type::Star; ///< 默认 1*

    constexpr GridLength() noexcept = default;
    constexpr GridLength(float v, Type t) noexcept : value{v}, type{t} {}

    /// 创建固定像素长度
    [[nodiscard]] static constexpr GridLength pixel(float pixels) noexcept {
        return {pixels, Type::Pixel};
    }

    /// 创建 Auto 长度
    [[nodiscard]] static constexpr GridLength auto_() noexcept {
        return {0.0f, Type::Auto};
    }

    /// 创建 Star 比例长度（默认权重 1.0）
    [[nodiscard]] static constexpr GridLength star(float weight = 1.0f) noexcept {
        return {weight, Type::Star};
    }

    [[nodiscard]] constexpr bool is_pixel() const noexcept { return type == Type::Pixel; }
    [[nodiscard]] constexpr bool is_auto()  const noexcept { return type == Type::Auto;  }
    [[nodiscard]] constexpr bool is_star()  const noexcept { return type == Type::Star;  }
};

// ── 行定义 ────────────────────────────────────────────────────────────────────

/**
 * @brief Grid 行定义。
 *
 * 描述 Grid 一行的尺寸策略：
 *   - height    ：行高（Pixel / Auto / Star），默认 1*
 *   - min_height：最小行高（像素），默认 0
 *   - max_height：最大行高（像素），默认无限大
 */
struct RowDefinition {
    GridLength height     = GridLength::star();
    float      min_height = 0.0f;
    float      max_height = std::numeric_limits<float>::infinity();
};

// ── 列定义 ────────────────────────────────────────────────────────────────────

/**
 * @brief Grid 列定义。
 *
 * 描述 Grid 一列的尺寸策略：
 *   - width    ：列宽（Pixel / Auto / Star），默认 1*
 *   - min_width：最小列宽（像素），默认 0
 *   - max_width：最大列宽（像素），默认无限大
 */
struct ColumnDefinition {
    GridLength width     = GridLength::star();
    float      min_width = 0.0f;
    float      max_width = std::numeric_limits<float>::infinity();
};

} // namespace mine::ui
