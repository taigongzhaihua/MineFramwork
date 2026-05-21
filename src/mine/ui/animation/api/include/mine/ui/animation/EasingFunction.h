/**
 * @file EasingFunction.h
 * @brief 缓动函数库 —— 标准缓动曲线与 CubicBezier 工厂。
 *
 * ## 设计概述
 *
 * 缓动函数（Easing Function）将归一化进度 t ∈ [0, 1] 映射为
 * 经缓动后的进度 t' ∈ [0, 1]，用于驱动动画插值：
 *
 *   animated_value = lerp(from, to, easing_fn(progress))
 *
 * ## 函数分类
 *
 * | 系列       | In（加速）         | Out（减速）         | InOut（先加后减）        |
 * |------------|---------------------|----------------------|--------------------------|
 * | 线性       | linear              | ——                  | ——                       |
 * | 二次（Quad）| quad_ease_in       | quad_ease_out        | quad_ease_in_out         |
 * | 三次（Cubic）| cubic_ease_in     | cubic_ease_out       | cubic_ease_in_out        |
 * | 四次（Quart）| quart_ease_in     | quart_ease_out       | quart_ease_in_out        |
 * | 五次（Quint）| quint_ease_in     | quint_ease_out       | quint_ease_in_out        |
 * | 指数（Expo）| expo_ease_in      | expo_ease_out        | expo_ease_in_out         |
 * | 正弦（Sine）| sine_ease_in      | sine_ease_out        | sine_ease_in_out         |
 * | 回弹（Back）| back_ease_in      | back_ease_out        | back_ease_in_out         |
 * | 弹性（Elastic）| elastic_ease_out（最常用）                                      |
 * | 弹跳（Bounce）| bounce_ease_out（最常用）                                       |
 * | 参数贝塞尔 | CubicBezier{x1,y1,x2,y2}.solve(t)                                |
 *
 * ## 全局函数指针常量
 *
 * 对于最常用的曲线，提供与文档一致的全局函数指针常量（直接可传给 Storyboard）：
 *   mine::ui::animation::CubicEaseIn   = &cubic_ease_in
 *   mine::ui::animation::CubicEaseOut  = &cubic_ease_out
 *   ...
 *
 * ## 使用方式
 *
 * @code
 *   // 使用预置函数指针（适用于 Storyboard 配置）
 *   float eased = mine::ui::animation::cubic_ease_out(0.5f); // ≈ 0.875
 *
 *   // 使用 CubicBezier 自定义贝塞尔（CSS ease 等价）
 *   using mine::ui::animation::CubicBezier;
 *   CubicBezier ease_curve{0.25f, 0.1f, 0.25f, 1.0f};
 *   float v = ease_curve.solve(0.5f);
 * @endcode
 */

#pragma once

#include <mine/ui/animation/Api.h>
#include <cmath>

namespace mine::ui::animation {

// ════════════════════════════════════════════════════════════════════════════
// 缓动函数类型别名
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief 缓动函数指针类型。
 *
 * 接受归一化进度 t ∈ [0, 1]，返回经缓动后的进度 t'。
 * 大多数缓动函数的输出也在 [0, 1]，但 Back/Elastic 系列可能短暂超出此范围。
 */
using EasingFn = float(*)(float t) noexcept;

// ════════════════════════════════════════════════════════════════════════════
// 线性
// ════════════════════════════════════════════════════════════════════════════

/// 线性缓动（无缓动），t' = t。
[[nodiscard]] constexpr float linear(float t) noexcept
{
    return t;
}

// ════════════════════════════════════════════════════════════════════════════
// 二次方（Quad）：t² 系列
// ════════════════════════════════════════════════════════════════════════════

/// Quad EaseIn：缓慢起步，加速结束。
[[nodiscard]] constexpr float quad_ease_in(float t) noexcept
{
    return t * t;
}

/// Quad EaseOut：快速起步，缓慢结束。
[[nodiscard]] constexpr float quad_ease_out(float t) noexcept
{
    return t * (2.0f - t);
}

/// Quad EaseInOut：缓慢起步，中间加速，缓慢结束。
[[nodiscard]] constexpr float quad_ease_in_out(float t) noexcept
{
    if (t < 0.5f) {
        return 2.0f * t * t;
    }
    const float u = t - 1.0f;
    return 1.0f - 2.0f * u * u;
}

// ════════════════════════════════════════════════════════════════════════════
// 三次方（Cubic）：t³ 系列
// ════════════════════════════════════════════════════════════════════════════

/// Cubic EaseIn：缓慢起步，强力加速结束。
[[nodiscard]] constexpr float cubic_ease_in(float t) noexcept
{
    return t * t * t;
}

/// Cubic EaseOut：快速起步，强力减速结束。
[[nodiscard]] constexpr float cubic_ease_out(float t) noexcept
{
    const float u = t - 1.0f;
    return 1.0f + u * u * u;
}

/// Cubic EaseInOut：三次方先加速后减速。
[[nodiscard]] constexpr float cubic_ease_in_out(float t) noexcept
{
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    }
    const float u = 2.0f * t - 2.0f;
    return 1.0f + 0.5f * u * u * u;
}

// ════════════════════════════════════════════════════════════════════════════
// 四次方（Quart）：t⁴ 系列
// ════════════════════════════════════════════════════════════════════════════

/// Quart EaseIn：比 Cubic 更强的加速起步。
[[nodiscard]] constexpr float quart_ease_in(float t) noexcept
{
    return t * t * t * t;
}

/// Quart EaseOut：比 Cubic 更强的减速结束。
[[nodiscard]] constexpr float quart_ease_out(float t) noexcept
{
    const float u = t - 1.0f;
    return 1.0f - u * u * u * u;
}

/// Quart EaseInOut：四次方先加速后减速。
[[nodiscard]] constexpr float quart_ease_in_out(float t) noexcept
{
    if (t < 0.5f) {
        return 8.0f * t * t * t * t;
    }
    const float u = t - 1.0f;
    return 1.0f - 8.0f * u * u * u * u;
}

// ════════════════════════════════════════════════════════════════════════════
// 五次方（Quint）：t⁵ 系列
// ════════════════════════════════════════════════════════════════════════════

/// Quint EaseIn：最强的多项式加速起步。
[[nodiscard]] constexpr float quint_ease_in(float t) noexcept
{
    return t * t * t * t * t;
}

/// Quint EaseOut：最强的多项式减速结束。
[[nodiscard]] constexpr float quint_ease_out(float t) noexcept
{
    const float u = t - 1.0f;
    return 1.0f + u * u * u * u * u;
}

/// Quint EaseInOut：五次方先加速后减速。
[[nodiscard]] constexpr float quint_ease_in_out(float t) noexcept
{
    if (t < 0.5f) {
        return 16.0f * t * t * t * t * t;
    }
    const float u = 2.0f * t - 2.0f;
    return 1.0f + 0.5f * u * u * u * u * u;
}

// ════════════════════════════════════════════════════════════════════════════
// 指数型（Expo）：基于 2 的指数曲线
// ════════════════════════════════════════════════════════════════════════════

/// Expo EaseIn：极慢起步，最后瞬间加速。
[[nodiscard]] inline float expo_ease_in(float t) noexcept
{
    if (t <= 0.0f) { return 0.0f; }
    return std::pow(2.0f, 10.0f * (t - 1.0f));
}

/// Expo EaseOut：瞬间起步，极慢减速。
[[nodiscard]] inline float expo_ease_out(float t) noexcept
{
    if (t >= 1.0f) { return 1.0f; }
    return 1.0f - std::pow(2.0f, -10.0f * t);
}

/// Expo EaseInOut：指数先加速后减速。
[[nodiscard]] inline float expo_ease_in_out(float t) noexcept
{
    if (t <= 0.0f) { return 0.0f; }
    if (t >= 1.0f) { return 1.0f; }
    if (t < 0.5f) {
        return 0.5f * std::pow(2.0f, 20.0f * t - 10.0f);
    }
    return 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);
}

// ════════════════════════════════════════════════════════════════════════════
// 正弦型（Sine）：基于余弦/正弦曲线
// ════════════════════════════════════════════════════════════════════════════

/// Sine EaseIn：正弦曲线加速（比 Quad 更柔和）。
[[nodiscard]] inline float sine_ease_in(float t) noexcept
{
    // 使用 cos 实现：1 - cos(t * π/2)
    return 1.0f - std::cos(t * 1.5707963267948966f);
}

/// Sine EaseOut：正弦曲线减速（比 Quad 更柔和）。
[[nodiscard]] inline float sine_ease_out(float t) noexcept
{
    return std::sin(t * 1.5707963267948966f);
}

/// Sine EaseInOut：正弦曲线先加速后减速。
[[nodiscard]] inline float sine_ease_in_out(float t) noexcept
{
    return 0.5f * (1.0f - std::cos(t * 3.1415926535897932f));
}

// ════════════════════════════════════════════════════════════════════════════
// 回弹型（Back）：超出目标再回到目标
// ════════════════════════════════════════════════════════════════════════════

namespace detail {
    /// Back 缓动的超调量系数（CSS 标准默认值）。
    inline constexpr float kBackOvershoot = 1.70158f;
}

/// Back EaseIn：先向后退再向前加速（起始超调）。
[[nodiscard]] constexpr float back_ease_in(float t) noexcept
{
    return t * t * ((detail::kBackOvershoot + 1.0f) * t - detail::kBackOvershoot);
}

/// Back EaseOut：先超过目标再回弹（结束超调）。
[[nodiscard]] constexpr float back_ease_out(float t) noexcept
{
    const float u = t - 1.0f;
    return 1.0f + u * u * ((detail::kBackOvershoot + 1.0f) * u + detail::kBackOvershoot);
}

/// Back EaseInOut：两端都有超调。
[[nodiscard]] constexpr float back_ease_in_out(float t) noexcept
{
    constexpr float c = detail::kBackOvershoot * 1.525f;
    if (t < 0.5f) {
        const float u = 2.0f * t;
        return 0.5f * u * u * ((c + 1.0f) * u - c);
    }
    const float u = 2.0f * t - 2.0f;
    return 0.5f * (2.0f + u * u * ((c + 1.0f) * u + c));
}

// ════════════════════════════════════════════════════════════════════════════
// 弹性型（Elastic）：弹簧振荡衰减
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Elastic EaseOut：以衰减振荡的方式到达目标。
 *
 * 输出范围约为 [-0.1, 1.1]（短暂超出 [0,1]），适用于"弹入"效果。
 */
[[nodiscard]] inline float elastic_ease_out(float t) noexcept
{
    if (t <= 0.0f) { return 0.0f; }
    if (t >= 1.0f) { return 1.0f; }
    // 振荡周期约 0.3s，振幅 1，相位调整使 t=0 时值为 0
    constexpr float period = 0.3f;
    constexpr float amplitude = 1.0f;
    const float s = period / 4.0f;
    return amplitude * std::pow(2.0f, -10.0f * t)
           * std::sin((t - s) * (2.0f * 3.1415926535897932f) / period)
           + 1.0f;
}

/**
 * @brief Elastic EaseIn：从零以振荡方式开始加速。
 *
 * 输出范围约为 [-0.1, 1.1]（短暂超出 [0,1]）。
 */
[[nodiscard]] inline float elastic_ease_in(float t) noexcept
{
    return 1.0f - elastic_ease_out(1.0f - t);
}

/// Elastic EaseInOut：两端均有弹性振荡。
[[nodiscard]] inline float elastic_ease_in_out(float t) noexcept
{
    if (t < 0.5f) {
        return 0.5f * elastic_ease_in(t * 2.0f);
    }
    return 0.5f + 0.5f * elastic_ease_out(t * 2.0f - 1.0f);
}

// ════════════════════════════════════════════════════════════════════════════
// 弹跳型（Bounce）：模拟物理弹跳
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Bounce EaseOut：像弹跳球一样减速到达目标。
 *
 * 输出始终在 [0, 1]，适合"落地弹跳"效果。
 */
[[nodiscard]] inline float bounce_ease_out(float t) noexcept
{
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    }
    if (t < 2.0f / 2.75f) {
        const float u = t - 1.5f / 2.75f;
        return 7.5625f * u * u + 0.75f;
    }
    if (t < 2.5f / 2.75f) {
        const float u = t - 2.25f / 2.75f;
        return 7.5625f * u * u + 0.9375f;
    }
    const float u = t - 2.625f / 2.75f;
    return 7.5625f * u * u + 0.984375f;
}

/// Bounce EaseIn：从弹跳状态开始加速。
[[nodiscard]] inline float bounce_ease_in(float t) noexcept
{
    return 1.0f - bounce_ease_out(1.0f - t);
}

/// Bounce EaseInOut：两端均有弹跳效果。
[[nodiscard]] inline float bounce_ease_in_out(float t) noexcept
{
    if (t < 0.5f) {
        return 0.5f * bounce_ease_in(t * 2.0f);
    }
    return 0.5f + 0.5f * bounce_ease_out(t * 2.0f - 1.0f);
}

// ════════════════════════════════════════════════════════════════════════════
// 全局函数指针常量（与文档保持一致的命名）
// ════════════════════════════════════════════════════════════════════════════

/**
 * @defgroup EasingConstants 缓动函数指针常量
 * 可直接传给 Storyboard::animate_dp(duration, easing) 的预置常量。
 * @{
 */

/// 线性缓动（无缓动效果）。
inline constexpr EasingFn Linear = &linear;

// 二次方
inline constexpr EasingFn QuadEaseIn    = &quad_ease_in;
inline constexpr EasingFn QuadEaseOut   = &quad_ease_out;
inline constexpr EasingFn QuadEaseInOut = &quad_ease_in_out;

// 三次方（最常用：CubicEaseIn / CubicEaseOut）
inline constexpr EasingFn CubicEaseIn    = &cubic_ease_in;
inline constexpr EasingFn CubicEaseOut   = &cubic_ease_out;
inline constexpr EasingFn CubicEaseInOut = &cubic_ease_in_out;

// 四次方
inline constexpr EasingFn QuartEaseIn    = &quart_ease_in;
inline constexpr EasingFn QuartEaseOut   = &quart_ease_out;
inline constexpr EasingFn QuartEaseInOut = &quart_ease_in_out;

// 五次方
inline constexpr EasingFn QuintEaseIn    = &quint_ease_in;
inline constexpr EasingFn QuintEaseOut   = &quint_ease_out;
inline constexpr EasingFn QuintEaseInOut = &quint_ease_in_out;

// 指数型
inline constexpr EasingFn ExpoEaseIn    = &expo_ease_in;
inline constexpr EasingFn ExpoEaseOut   = &expo_ease_out;
inline constexpr EasingFn ExpoEaseInOut = &expo_ease_in_out;

// 正弦型
inline constexpr EasingFn SineEaseIn    = &sine_ease_in;
inline constexpr EasingFn SineEaseOut   = &sine_ease_out;
inline constexpr EasingFn SineEaseInOut = &sine_ease_in_out;

// 回弹型
inline constexpr EasingFn BackEaseIn    = &back_ease_in;
inline constexpr EasingFn BackEaseOut   = &back_ease_out;
inline constexpr EasingFn BackEaseInOut = &back_ease_in_out;

// 弹性型（常用 Out 版本）
inline constexpr EasingFn ElasticEaseIn    = &elastic_ease_in;
inline constexpr EasingFn ElasticEaseOut   = &elastic_ease_out;
inline constexpr EasingFn ElasticEaseInOut = &elastic_ease_in_out;

// 弹跳型（常用 Out 版本）
inline constexpr EasingFn BounceEaseIn    = &bounce_ease_in;
inline constexpr EasingFn BounceEaseOut   = &bounce_ease_out;
inline constexpr EasingFn BounceEaseInOut = &bounce_ease_in_out;

/** @} */

// ════════════════════════════════════════════════════════════════════════════
// CubicBezier 参数化贝塞尔缓动
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief 参数化三次贝塞尔缓动曲线。
 *
 * 与 CSS `animation-timing-function: cubic-bezier(x1, y1, x2, y2)` 等价。
 * 曲线由两个控制点 (x1, y1) 和 (x2, y2) 定义，起止点固定为 (0,0) 和 (1,1)。
 *
 * 典型预设值：
 *   - `ease`：        {0.25f, 0.1f,  0.25f, 1.0f}
 *   - `ease-in`：     {0.42f, 0.0f,  1.0f,  1.0f}
 *   - `ease-out`：    {0.0f,  0.0f,  0.58f, 1.0f}
 *   - `ease-in-out`： {0.42f, 0.0f,  0.58f, 1.0f}
 *
 * @code
 *   CubicBezier ease{0.25f, 0.1f, 0.25f, 1.0f};
 *   float v = ease.solve(0.5f);
 * @endcode
 */
class MINE_UI_ANIMATION_API CubicBezier {
public:
    /**
     * @brief 构造参数化贝塞尔缓动。
     * @param x1 第一控制点 x（通常 ∈ [0,1]）
     * @param y1 第一控制点 y（可超出 [0,1] 产生超调）
     * @param x2 第二控制点 x（通常 ∈ [0,1]）
     * @param y2 第二控制点 y（可超出 [0,1] 产生超调）
     */
    constexpr CubicBezier(float x1, float y1, float x2, float y2) noexcept
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2)
    {}

    /**
     * @brief 计算给定进度 t 对应的缓动值。
     *
     * 使用牛顿迭代法求解贝塞尔参数 u，使 Bx(u) ≈ t，
     * 再求解 By(u) 作为输出值。最大迭代 8 次，精度约 1e-6。
     *
     * @param t 归一化进度，∈ [0, 1]
     * @return 缓动后的进度值
     */
    [[nodiscard]] float solve(float t) const noexcept;

    // CSS 标准预置常量
    /// 等价 CSS `ease`：{0.25, 0.1, 0.25, 1.0}
    static const CubicBezier Ease;
    /// 等价 CSS `ease-in`：{0.42, 0.0, 1.0, 1.0}
    static const CubicBezier EaseIn;
    /// 等价 CSS `ease-out`：{0.0, 0.0, 0.58, 1.0}
    static const CubicBezier EaseOut;
    /// 等价 CSS `ease-in-out`：{0.42, 0.0, 0.58, 1.0}
    static const CubicBezier EaseInOut;

private:
    float x1_, y1_, x2_, y2_;

    /// 计算三次贝塞尔曲线上 t 处的 x 分量（用于求解逆映射）。
    [[nodiscard]] float bezier_x(float t) const noexcept;
    /// 计算三次贝塞尔曲线上 t 处的 y 分量（作为缓动输出值）。
    [[nodiscard]] float bezier_y(float t) const noexcept;
    /// 贝塞尔 x 分量的导数（用于牛顿迭代）。
    [[nodiscard]] float bezier_x_derivative(float t) const noexcept;
};

} // namespace mine::ui::animation
