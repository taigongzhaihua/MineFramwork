/**
 * @file Common.h
 * @brief mine.math 模块的标量辅助函数与常量。
 */

#pragma once

#include <cmath>

namespace mine::math {

/// 浮点比较与归一化默认使用的容差。
inline constexpr float kDefaultEpsilon = 1.0e-6f;

/// 比较两个浮点值是否在给定误差范围内近似相等。
[[nodiscard]] inline bool nearly_equal(
    float lhs,
    float rhs,
    float epsilon = kDefaultEpsilon) noexcept
{
    return std::fabs(lhs - rhs) <= epsilon;
}

/// 将标量限制在 [min_value, max_value] 区间内。
[[nodiscard]] constexpr float clamp(
    float value,
    float min_value,
    float max_value) noexcept
{
    return value < min_value ? min_value : (value > max_value ? max_value : value);
}

/// 将标量限制在 [0, 1] 区间内。
[[nodiscard]] constexpr float clamp01(float value) noexcept {
    return clamp(value, 0.0f, 1.0f);
}

/// 线性插值。
[[nodiscard]] constexpr float lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

/// 角度转弧度。
[[nodiscard]] constexpr float radians(float degrees_value) noexcept {
    return degrees_value * 0.01745329251994329577f;
}

/// 弧度转角度。
[[nodiscard]] constexpr float degrees(float radians_value) noexcept {
    return radians_value * 57.295779513082320876f;
}

} // namespace mine::math