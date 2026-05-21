/**
 * @file SpringEasing.h
 * @brief SpringEasing —— 弹簧物理缓动模拟器。
 *
 * ## 物理模型
 *
 * 基于阻尼弹簧（二阶线性常微分方程）：
 *
 *   x'' + 2ζω₀x' + ω₀²x = ω₀²·x_target
 *
 * 参数说明：
 *   - mass      质量（kg）：影响响应速度，越小越灵敏。
 *   - stiffness 刚度（N/m）：弹簧劲度系数，越大越"硬"（到达目标越快）。
 *   - damping   阻尼系数（N·s/m）：越大振荡衰减越快；等于临界阻尼时无振荡。
 *
 * 衍生参数：
 *   - ω₀ = sqrt(stiffness / mass)      自然（角）频率
 *   - ζ  = damping / (2 * sqrt(stiffness * mass))   阻尼比
 *
 * ## 三种工作模式（按阻尼比区分）
 *
 * | 模式       | 条件        | 特征                                 |
 * |------------|-------------|--------------------------------------|
 * | 欠阻尼     | ζ < 1       | 振荡后衰减到目标（有"弹"的感觉）      |
 * | 临界阻尼   | ζ = 1       | 最快无振荡到达目标                    |
 * | 过阻尼     | ζ > 1       | 缓慢无振荡到达目标（"厚重"感）        |
 *
 * ## 推荐参数（UI 动画常用）
 *
 * | 场景         | mass | stiffness | damping | 特征           |
 * |--------------|------|-----------|---------|----------------|
 * | 快速弹入     | 1.0  | 400       | 30      | 欠阻尼，轻微振荡 |
 * | 标准过渡     | 1.0  | 200       | 20      | 欠阻尼，适度弹性 |
 * | 重物感       | 1.0  | 100       | 20      | 欠阻尼，较多振荡 |
 * | 无振荡快速   | 1.0  | 200       | 28.3    | 近临界（ζ≈1）   |
 *
 * ## 使用方式
 *
 * @code
 *   using mine::ui::animation::SpringEasing;
 *
 *   // 创建弹簧缓动（标准参数）
 *   SpringEasing spring{1.0f, 200.0f, 20.0f};
 *
 *   // 设置动画目标：从 0.0 运动到 1.0，初始速度为 0
 *   spring.reset(0.0f, 1.0f, 0.0f);
 *
 *   // 每帧步进（dt 为帧间隔秒数，例如 60fps → 1/60）
 *   constexpr float dt = 1.0f / 60.0f;
 *   for (int i = 0; i < 60 && !spring.is_settled(); ++i) {
 *       float value = spring.step(dt);
 *       // 用 value 更新属性...
 *   }
 * @endcode
 *
 * ## 与 EasingFn 的区别
 *
 * EasingFn 是纯函数（t ∈ [0,1] → float），无内部状态，适合固定时长动画。
 * SpringEasing 是有状态的物理模拟器，适合：
 *   - 不确定动画时长（运动直到稳定）
 *   - 动画过程中目标被打断并重定向（保持当前速度继续运动）
 *   - 需要真实物理弹性感的交互响应
 */

#pragma once

#include <mine/ui/animation/Api.h>
#include <cmath>

namespace mine::ui::animation {

/**
 * @brief 有状态的弹簧物理缓动模拟器。
 *
 * 每次调用 step(dt) 更新内部位置与速度，返回当前位置值。
 * 可随时调用 reset() 更改目标或重置初始状态。
 */
class MINE_UI_ANIMATION_API SpringEasing {
public:
    /**
     * @brief 构造弹簧缓动参数。
     *
     * @param mass      质量（正浮点数，单位：kg，默认 1.0）
     * @param stiffness 刚度（正浮点数，单位：N/m，默认 200.0）
     * @param damping   阻尼系数（非负浮点数，单位：N·s/m，默认 20.0）
     */
    constexpr SpringEasing(
        float mass      = 1.0f,
        float stiffness = 200.0f,
        float damping   = 20.0f) noexcept
        : mass_(mass > 0.0f ? mass : 1.0f)
        , stiffness_(stiffness > 0.0f ? stiffness : 200.0f)
        , damping_(damping >= 0.0f ? damping : 20.0f)
        , target_(0.0f)
        , position_(0.0f)
        , velocity_(0.0f)
    {}

    // ── 状态控制 ─────────────────────────────────────────────────────────

    /**
     * @brief 重置弹簧状态，设定新的起点和目标。
     *
     * 常用于动画开始或目标变更（保持初始速度可实现无缝重定向）。
     *
     * @param from             起始位置
     * @param to               目标位置
     * @param initial_velocity 初始速度（正值向目标方向，负值反向）
     */
    void reset(float from, float to, float initial_velocity = 0.0f) noexcept
    {
        target_   = to;
        position_ = from;
        velocity_ = initial_velocity;
    }

    /**
     * @brief 更改运动目标（保持当前位置和速度，实现平滑重定向）。
     *
     * @param new_target 新目标位置
     */
    void retarget(float new_target) noexcept
    {
        target_ = new_target;
    }

    // ── 模拟步进 ─────────────────────────────────────────────────────────

    /**
     * @brief 步进物理模拟，返回当前位置值。
     *
     * 使用半隐式欧拉积分（Symplectic Euler），在小步长下具有良好的能量守恒性。
     * 建议 dt ≤ 1/30 秒（即不低于 30fps）；更大步长可用子步（sub-step）处理。
     *
     * @param dt 时间步长（秒），必须为正数
     * @return 当前位置值
     */
    float step(float dt) noexcept
    {
        if (dt <= 0.0f) {
            return position_;
        }

        // 弹力 F = -stiffness * (position - target)
        // 阻力 F_d = -damping * velocity
        // 加速度 a = (F + F_d) / mass
        const float displacement = position_ - target_;
        const float spring_force = -stiffness_ * displacement;
        const float damping_force = -damping_ * velocity_;
        const float acceleration = (spring_force + damping_force) / mass_;

        // 半隐式欧拉积分：先更新速度，再用新速度更新位置
        velocity_ += acceleration * dt;
        position_ += velocity_ * dt;

        return position_;
    }

    // ── 查询 ─────────────────────────────────────────────────────────────

    /// 获取当前位置值。
    [[nodiscard]] float current()  const noexcept { return position_; }

    /// 获取当前目标值。
    [[nodiscard]] float target()   const noexcept { return target_;   }

    /// 获取当前速度。
    [[nodiscard]] float velocity() const noexcept { return velocity_; }

    /**
     * @brief 判断弹簧是否已稳定（接近目标且速度接近零）。
     *
     * @param position_epsilon 位置容差（默认 0.001）
     * @param velocity_epsilon 速度容差（默认 0.001）
     * @return 若位置在目标附近且速度接近零，返回 true
     */
    [[nodiscard]] bool is_settled(
        float position_epsilon = 0.001f,
        float velocity_epsilon = 0.001f) const noexcept
    {
        const float pos_diff = position_ - target_;
        const float pos_ok = (pos_diff >= -position_epsilon && pos_diff <= position_epsilon);
        const float vel_ok = (velocity_ >= -velocity_epsilon && velocity_ <= velocity_epsilon);
        return pos_ok && vel_ok;
    }

    // ── 弹簧参数查询 ─────────────────────────────────────────────────────

    [[nodiscard]] constexpr float mass()      const noexcept { return mass_;      }
    [[nodiscard]] constexpr float stiffness() const noexcept { return stiffness_; }
    [[nodiscard]] constexpr float damping()   const noexcept { return damping_;   }

    /**
     * @brief 计算自然频率 ω₀ = sqrt(stiffness / mass)。
     */
    [[nodiscard]] float natural_frequency() const noexcept
    {
        return std::sqrt(stiffness_ / mass_);
    }

    /**
     * @brief 计算阻尼比 ζ = damping / (2 * sqrt(stiffness * mass))。
     *
     * ζ < 1：欠阻尼（振荡）
     * ζ = 1：临界阻尼（最快无振荡）
     * ζ > 1：过阻尼（缓慢无振荡）
     */
    [[nodiscard]] float damping_ratio() const noexcept
    {
        return damping_ / (2.0f * std::sqrt(stiffness_ * mass_));
    }

    /**
     * @brief 估算稳定所需时间（秒）。
     *
     * 基于阻尼指数衰减估算到 0.1% 误差范围所需时间，
     * 仅适用于欠阻尼和临界阻尼情况（ζ ≤ 1）。
     *
     * @return 估算秒数（欠阻尼：约 7/(ζ·ω₀)；过阻尼：需更长时间）
     */
    [[nodiscard]] float estimated_settle_time() const noexcept
    {
        const float omega0 = natural_frequency();
        const float zeta   = damping_ratio();
        if (zeta <= 0.0f || omega0 <= 0.0f) {
            return 0.0f;
        }
        // 对数衰减：e^(-ζω₀t) < 0.001 → t > ln(1000)/(ζω₀) ≈ 6.9/(ζω₀)
        return 6.9f / (zeta * omega0);
    }

private:
    float mass_;       ///< 质量（kg）
    float stiffness_;  ///< 刚度（N/m）
    float damping_;    ///< 阻尼系数（N·s/m）
    float target_;     ///< 当前目标位置
    float position_;   ///< 当前位置
    float velocity_;   ///< 当前速度（m/s）
};

} // namespace mine::ui::animation
