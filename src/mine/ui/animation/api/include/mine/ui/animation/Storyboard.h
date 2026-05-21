/**
 * @file Storyboard.h
 * @brief Storyboard —— 属性动画时间线容器。
 *
 * Storyboard 是 Task #17 的核心类，管理一组 PropertyAnimation 并协调其
 * 采样、推进和清除。典型使用流程（由 VisualStateManager 驱动）：
 *
 *   1. 调用方通过 animate_dp / animate_dp_to 声明要驱动的属性及参数。
 *   2. 状态切换前：capture_from_values() — 采样所有目标属性的当前生效值作为起始值。
 *   3. 新状态的 StyleTrigger 写入后：resolve_and_begin() —
 *      解析 "to" 值（从 StyleTrigger 层读取），然后以 Animation 优先级写入起始值，
 *      使属性从 "from" 出发开始插值。
 *   4. 每帧：tick(dt) — 推进所有动画，将插值结果以 Animation 优先级写回属性。
 *      全部完成时返回 true。
 *   5. 动画完成或被打断时：stop() — 清除 Animation 优先级，属性退回到 StyleTrigger 层。
 *
 * Storyboard 是可移动但不可拷贝的。
 */

#pragma once

#include <mine/ui/animation/Api.h>
#include <mine/ui/animation/Duration.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/PropertyAnimation.h>

#include <mine/containers/SmallVector.h>
#include <mine/core/Variant.h>

// 前向声明
namespace mine::ui {
class DependencyObject;
class DependencyProperty;
}  // namespace mine::ui

namespace mine::ui::animation {

/**
 * @brief 属性动画时间线容器。
 *
 * 一个 Storyboard 实例通常对应一次 VisualState 过渡，包含若干个 PropertyAnimation
 * 并行播放。VisualStateManager 通过用户提供的 configure_fn 向 Storyboard 注册动画，
 * 然后驱动其生命周期。
 *
 * @note 不可拷贝（PropertyAnimation 含 Variant 字段，Storyboard 设计为独占所有权）。
 */
class MINE_UI_ANIMATION_API Storyboard {
public:
    // ── 构造 / 移动 ──────────────────────────────────────────────────────────

    Storyboard() noexcept                            = default;
    Storyboard(Storyboard&&) noexcept                = default;
    Storyboard& operator=(Storyboard&&) noexcept     = default;
    Storyboard(const Storyboard&)                    = delete;
    Storyboard& operator=(const Storyboard&)         = delete;
    ~Storyboard() noexcept                           = default;

    // ── 配置阶段（在 configure_fn 中调用）────────────────────────────────────

    /**
     * @brief 注册依赖属性动画（自动 from；to 在 resolve_and_begin() 时从 StyleTrigger 层读取）。
     *
     * @param target   被驱动的目标元素
     * @param prop     目标依赖属性
     * @param duration 动画持续时长
     * @param easing   缓动函数（默认线性）
     */
    void animate_dp(ui::DependencyObject& target,
                    const ui::DependencyProperty& prop,
                    Duration duration,
                    EasingFn easing = Linear) noexcept;

    /**
     * @brief 注册依赖属性动画（自动 from；to 由调用方显式指定）。
     *
     * @param target   被驱动的目标元素
     * @param prop     目标依赖属性
     * @param to       动画终止值
     * @param duration 动画持续时长
     * @param easing   缓动函数（默认线性）
     */
    void animate_dp_to(ui::DependencyObject& target,
                       const ui::DependencyProperty& prop,
                       core::Variant to,
                       Duration duration,
                       EasingFn easing = Linear) noexcept;

    // ── 生命周期阶段（由 VisualStateManager 调用）────────────────────────────

    /**
     * @brief 采样所有动画的起始值（from）。
     *
     * 必须在新状态的 StyleTrigger 写入之前调用，以捕捉切换前的属性生效值。
     */
    void capture_from_values() noexcept;

    /**
     * @brief 解析终止值（to）并开始动画（以 Animation 优先级写入起始值）。
     *
     * 必须在新状态的 StyleTrigger 写入之后调用：
     *   - 对 to_is_resolved == false 的动画，读取 DependencyObject 当前生效值作为 to；
     *   - 然后以 Animation 优先级写入 from，使属性从起始值开始插值。
     */
    void resolve_and_begin() noexcept;

    /**
     * @brief 推进所有动画一帧。
     *
     * @param dt 本帧时间步长（秒），必须 >= 0
     * @return true  所有 PropertyAnimation 均已播放完毕
     * @return false 仍有动画未完成
     */
    [[nodiscard]] bool tick(float dt) noexcept;

    /**
     * @brief 停止所有动画并清除 Animation 优先级。
     *
     * 调用后所有被驱动属性的值退回到 StyleTrigger 层（或更低优先级）。
     */
    void stop() noexcept;

    // ── 状态查询 ──────────────────────────────────────────────────────────────

    /// 所有动画是否均已完成
    [[nodiscard]] bool is_complete() const noexcept { return complete_; }

    /// 已注册的动画数量（主要用于测试）
    [[nodiscard]] int animation_count() const noexcept {
        return static_cast<int>(animations_.size());
    }

private:
    /// 内联容量 4：大多数过渡只驱动少量属性（背景色、透明度、缩放等）
    containers::SmallVector<PropertyAnimation, 4> animations_;

    /// 全部动画是否已完成（tick 驱动）
    bool complete_{false};

    /// 是否已调用 resolve_and_begin（防止重复开始）
    bool began_{false};
};

}  // namespace mine::ui::animation
