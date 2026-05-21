/**
 * @file Duration.h
 * @brief Duration —— 动画时间段描述。
 *
 * 内部以秒（float）存储，提供毫秒/秒两种工厂函数与相互转换。
 * Duration 本身是值语义，可按值传递。
 *
 * 典型用法：
 * @code
 *   Duration d = Duration::milliseconds(120.0f);
 *   float s = d.to_seconds();   // 0.12f
 * @endcode
 */

#pragma once

#include <mine/ui/animation/Api.h>

namespace mine::ui::animation {

/**
 * @brief 时间段描述（精度为单精度浮点秒）。
 *
 * 动画系统中所有持续时间均通过此类型表示，
 * 以避免调用处对"参数是秒还是毫秒"产生歧义。
 */
class MINE_UI_ANIMATION_API Duration {
public:
    /// 默认构造，时长为 0。
    constexpr Duration() noexcept : seconds_(0.0f) {}

    // ── 工厂函数 ──────────────────────────────────────────────────────────

    /// 从毫秒创建时间段。
    [[nodiscard]] static constexpr Duration milliseconds(float ms) noexcept
    {
        Duration d;
        d.seconds_ = ms * 0.001f;
        return d;
    }

    /// 从秒创建时间段。
    [[nodiscard]] static constexpr Duration seconds(float s) noexcept
    {
        Duration d;
        d.seconds_ = s;
        return d;
    }

    // ── 转换 ─────────────────────────────────────────────────────────────

    /// 以秒为单位返回时长。
    [[nodiscard]] constexpr float to_seconds()      const noexcept { return seconds_; }

    /// 以毫秒为单位返回时长。
    [[nodiscard]] constexpr float to_milliseconds() const noexcept { return seconds_ * 1000.0f; }

    // ── 比较 ─────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool operator==(Duration rhs) const noexcept
    {
        return seconds_ == rhs.seconds_;
    }
    [[nodiscard]] constexpr bool operator!=(Duration rhs) const noexcept
    {
        return !(*this == rhs);
    }
    [[nodiscard]] constexpr bool operator< (Duration rhs) const noexcept
    {
        return seconds_ < rhs.seconds_;
    }
    [[nodiscard]] constexpr bool operator<=(Duration rhs) const noexcept
    {
        return seconds_ <= rhs.seconds_;
    }
    [[nodiscard]] constexpr bool operator> (Duration rhs) const noexcept
    {
        return seconds_ > rhs.seconds_;
    }
    [[nodiscard]] constexpr bool operator>=(Duration rhs) const noexcept
    {
        return seconds_ >= rhs.seconds_;
    }

    // ── 算术 ─────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr Duration operator+(Duration rhs) const noexcept
    {
        return seconds(seconds_ + rhs.seconds_);
    }
    [[nodiscard]] constexpr Duration operator-(Duration rhs) const noexcept
    {
        return seconds(seconds_ - rhs.seconds_);
    }
    [[nodiscard]] constexpr Duration operator*(float factor) const noexcept
    {
        return seconds(seconds_ * factor);
    }

    /// 是否为零时长。
    [[nodiscard]] constexpr bool is_zero() const noexcept { return seconds_ <= 0.0f; }

private:
    float seconds_;
};

} // namespace mine::ui::animation
