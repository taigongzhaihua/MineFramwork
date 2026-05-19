/**
 * @file Brush.h
 * @brief 填充画刷类型。
 *
 * Brush 是一个可拷贝的值类型，可内联存储在 DrawCmd 中。
 * 支持纯色、线性渐变、径向渐变和亚克力（模糊+色调）四种画刷。
 *
 * 渐变坐标系约定：
 *   - 使用归一化坐标 [0,1]，(0,0) = 形状包围盒左上角，(1,1) = 右下角
 *   - 中心点 = (0.5, 0.5)
 *
 * 亚克力画刷（AcrylicBrush）：
 *   - 捕获背景（模糊）后叠加色调，产生磨砂玻璃效果
 *   - blur_amount 单位为逻辑像素，建议范围 10~60
 *   - 仅在 RhiRenderer 有亚克力管线时生效
 *
 * 依赖：mine.math（Color、Vec2），mine.core（Span）
 */

#pragma once

#include <cstdint>
#include <mine/math/Color.h>
#include <mine/math/Vec2.h>
#include <mine/core/Span.h>

namespace mine::paint {

/// 渐变画刷最大色标数（内联存储，避免动态分配）
static constexpr uint32_t kMaxGradientStops = 4;

/**
 * @brief 渐变色标（颜色 + 偏移量）。
 *
 * offset 范围为 [0.0, 1.0]，表示在渐变路径上的归一化位置。
 */
struct GradientStop {
    float       offset{0.0f};  ///< 渐变路径上的归一化位置 [0, 1]
    math::Color color{};       ///< 此位置对应的线性空间颜色（RGBA）
};

/**
 * @brief 线性渐变数据。
 *
 * 起/终点使用归一化坐标，(0,0)=形状左上角，(1,1)=形状右下角。
 * 例：从左到右的渐变：start=(0,0.5), end=(1,0.5)
 */
struct LinearGradientData {
    math::Vec2   start{0.0f, 0.5f};             ///< 起点（归一化 [0,1]）
    math::Vec2   end{1.0f, 0.5f};               ///< 终点（归一化 [0,1]）
    uint32_t     stop_count{0};                 ///< 色标数量 [2..kMaxGradientStops]
    float        _pad{0.0f};                    ///< 对齐填充
    GradientStop stops[kMaxGradientStops]{};    ///< 色标数组（按 offset 升序排列）
};

/**
 * @brief 径向渐变数据。
 *
 * 圆心和半径均使用归一化坐标，1.0 对应形状包围盒的较短边长度的一半。
 * inner_radius=0 表示实心圆（从圆心开始渐变）；
 * inner_radius>0 表示环形渐变（从内圆边缘开始）。
 */
struct RadialGradientData {
    math::Vec2   center{0.5f, 0.5f};            ///< 圆心（归一化 [0,1]）
    float        inner_radius{0.0f};            ///< 内径（归一化，0=实心圆，>0=环形）
    float        outer_radius{1.0f};            ///< 外径（归一化，1.0=形状对角线的一半）
    uint32_t     stop_count{0};                 ///< 色标数量 [2..kMaxGradientStops]
    float        _pad{0.0f};                    ///< 对齐填充
    GradientStop stops[kMaxGradientStops]{};    ///< 色标数组（按 offset 升序排列）
};

/**
 * @brief 亚克力画刷数据（磨砂玻璃效果）。
 *
 * 亚克力效果由三个步骤组成：
 *   1. 捕获当前渲染目标背景（高斯模糊）
 *   2. 增强饱和度（可选）
 *   3. 叠加色调颜色
 */
struct AcrylicData {
    math::Color  tint_color{1.0f, 1.0f, 1.0f, 1.0f};  ///< 叠加色调（线性 RGBA）
    float        tint_opacity{0.7f};                    ///< 色调混合比例 [0,1]
    float        blur_amount{30.0f};                    ///< 模糊强度（逻辑像素）
    float        saturation{1.25f};                     ///< 饱和度增益（1.0=原始）
    float        _pad{0.0f};                            ///< 对齐填充
};

/**
 * @brief 画刷类型枚举。
 */
enum class BrushKind : uint8_t {
    SolidColor,       ///< 纯色填充（M0 已实现）
    LinearGradient,   ///< 线性渐变（M0 已实现）
    RadialGradient,   ///< 径向渐变（M0 已实现）
    AcrylicBrush,     ///< 亚克力（模糊背景 + 色调，M0 已实现）
    ImageBrush,       ///< 图像画刷（M1+ 实现）
};

/**
 * @brief 填充画刷（可拷贝的值类型）。
 *
 * 内联存储所有画刷数据（使用 union），无动态分配。
 * 可直接嵌入 DrawCmd，拷贝安全。
 *
 * 使用静态工厂函数创建具体类型的画刷：
 * @code
 *   // 纯色
 *   auto b1 = Brush::solid(Color{0.2f, 0.6f, 1.0f});
 *
 *   // 线性渐变（从左到右，蓝→橙）
 *   GradientStop stops[] = {{0.0f, {0.2f,0.5f,1.0f,1.0f}}, {1.0f, {1.0f,0.5f,0.1f,1.0f}}};
 *   auto b2 = Brush::linear_gradient({0,0.5f}, {1,0.5f}, stops);
 *
 *   // 亚克力
 *   auto b3 = Brush::acrylic({0.5f,0.7f,1.0f,1.0f}, 0.6f, 30.0f);
 * @endcode
 */
class Brush {
public:
    // ── 工厂函数 ────────────────────────────────────────────────────────

    /// 创建纯色画刷。
    [[nodiscard]] static Brush solid(math::Color color) noexcept {
        Brush b;
        b.kind_   = BrushKind::SolidColor;
        b.color_  = color;
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

    /**
     * @brief 创建线性渐变画刷。
     *
     * @param start  渐变起点（归一化 [0,1]，(0,0)=左上，(1,1)=右下）
     * @param end    渐变终点（归一化 [0,1]）
     * @param stops  色标数组（2..kMaxGradientStops 个，按 offset 升序）
     */
    [[nodiscard]] static Brush linear_gradient(
        math::Vec2                  start,
        math::Vec2                  end,
        core::Span<const GradientStop> stops) noexcept
    {
        Brush b;
        b.kind_ = BrushKind::LinearGradient;
        b.linear_.start = start;
        b.linear_.end   = end;
        // 限制色标数量并复制
        const uint32_t count = stops.size() < kMaxGradientStops
            ? static_cast<uint32_t>(stops.size())
            : kMaxGradientStops;
        b.linear_.stop_count = count;
        for (uint32_t i = 0; i < count; ++i) {
            b.linear_.stops[i] = stops[i];
        }
        return b;
    }

    /**
     * @brief 创建径向渐变画刷（实心圆，从圆心向外）。
     *
     * @param center        圆心（归一化 [0,1]，(0.5,0.5)=形状中心）
     * @param outer_radius  外径（归一化，1.0=形状短边的一半）
     * @param stops         色标数组（2..kMaxGradientStops 个）
     */
    [[nodiscard]] static Brush radial_gradient(
        math::Vec2                  center,
        float                       outer_radius,
        core::Span<const GradientStop> stops) noexcept
    {
        return radial_gradient_ring(center, 0.0f, outer_radius, stops);
    }

    /**
     * @brief 创建径向渐变画刷（环形，从内圆到外圆）。
     *
     * @param center        圆心（归一化 [0,1]）
     * @param inner_radius  内径（归一化，0=实心圆）
     * @param outer_radius  外径（归一化）
     * @param stops         色标数组（2..kMaxGradientStops 个）
     */
    [[nodiscard]] static Brush radial_gradient_ring(
        math::Vec2                  center,
        float                       inner_radius,
        float                       outer_radius,
        core::Span<const GradientStop> stops) noexcept
    {
        Brush b;
        b.kind_ = BrushKind::RadialGradient;
        b.radial_.center       = center;
        b.radial_.inner_radius = inner_radius;
        b.radial_.outer_radius = outer_radius;
        const uint32_t count = stops.size() < kMaxGradientStops
            ? static_cast<uint32_t>(stops.size())
            : kMaxGradientStops;
        b.radial_.stop_count = count;
        for (uint32_t i = 0; i < count; ++i) {
            b.radial_.stops[i] = stops[i];
        }
        return b;
    }

    /**
     * @brief 创建亚克力画刷（磨砂玻璃效果）。
     *
     * @param tint          叠加色调（线性 RGBA）
     * @param tint_opacity  色调混合比例 [0,1]，0=纯背景，1=纯色调
     * @param blur_amount   模糊强度（逻辑像素），建议值 20~40
     * @param saturation    饱和度增益，1.0=不变，>1.0=增强（建议 1.25）
     */
    [[nodiscard]] static Brush acrylic(
        math::Color tint,
        float       tint_opacity  = 0.7f,
        float       blur_amount   = 30.0f,
        float       saturation    = 1.25f) noexcept
    {
        Brush b;
        b.kind_                   = BrushKind::AcrylicBrush;
        b.acrylic_.tint_color     = tint;
        b.acrylic_.tint_opacity   = tint_opacity;
        b.acrylic_.blur_amount    = blur_amount;
        b.acrylic_.saturation     = saturation;
        return b;
    }

    // ── 访问器 ──────────────────────────────────────────────────────────

    /// 返回画刷类型。
    [[nodiscard]] BrushKind kind() const noexcept { return kind_; }

    /**
     * @brief 返回纯色画刷的颜色值（线性空间）。
     * @pre kind() == BrushKind::SolidColor
     */
    [[nodiscard]] math::Color color() const noexcept { return color_; }

    /**
     * @brief 返回线性渐变数据。
     * @pre kind() == BrushKind::LinearGradient
     */
    [[nodiscard]] const LinearGradientData& linear_gradient_data() const noexcept {
        return linear_;
    }

    /**
     * @brief 返回径向渐变数据。
     * @pre kind() == BrushKind::RadialGradient
     */
    [[nodiscard]] const RadialGradientData& radial_gradient_data() const noexcept {
        return radial_;
    }

    /**
     * @brief 返回亚克力画刷数据。
     * @pre kind() == BrushKind::AcrylicBrush
     */
    [[nodiscard]] const AcrylicData& acrylic_data() const noexcept {
        return acrylic_;
    }

    /// 判断画刷是否完全透明（Alpha == 0）。
    [[nodiscard]] bool is_transparent() const noexcept {
        return kind_ == BrushKind::SolidColor && color_.a <= 0.0f;
    }

    // ── 默认构造 ────────────────────────────────────────────────────────

    /// 默认构造为不透明黑色纯色画刷。
    Brush() noexcept = default;

private:
    BrushKind kind_{BrushKind::SolidColor};  ///< 画刷类型标签

    /// 内联存储所有画刷数据（union，无动态分配，拷贝安全）
    union {
        math::Color        color_   {0.0f, 0.0f, 0.0f, 1.0f};  ///< SolidColor 颜色值
        LinearGradientData linear_;                              ///< LinearGradient 数据
        RadialGradientData radial_;                              ///< RadialGradient 数据
        AcrylicData        acrylic_;                             ///< AcrylicBrush 数据
    };
};

} // namespace mine::paint
