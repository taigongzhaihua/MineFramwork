/**
 * @file Storyboard.cpp
 * @brief Storyboard 属性动画时间线容器实现。
 *
 * 实现包含：
 *   - lerp_variant()  ：支持 float / math::Color / math::Thickness 的线性插值；
 *                       其他类型在 t >= 1.0f 时直接 snap 到终止值。
 *   - animate_dp / animate_dp_to ：向 animations_ 注册 PropertyAnimation。
 *   - capture_from_values()       ：在状态切换前采样所有目标属性的当前生效值。
 *   - resolve_and_begin()         ：解析终止值并以 Animation 优先级写入起始值。
 *   - tick(dt)                    ：逐帧推进动画，计算插值结果并写回属性。
 *   - stop()                      ：清除所有 Animation 优先级槽，属性退回到 StyleTrigger 层。
 */

#include <mine/ui/animation/Storyboard.h>

#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>

#include <mine/core/Variant.h>
#include <mine/math/Color.h>
#include <mine/math/Thickness.h>
#include <mine/math/Common.h>

namespace mine::ui::animation {

// ─────────────────────────────────────────────────────────────────────────────
// 内部辅助：Variant 线性插值
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 对两个 Variant 值进行线性插值。
 *
 * 支持的类型：
 *   - float           ：标量线性插值
 *   - math::Color     ：RGBA 四分量各自线性插值
 *   - math::Thickness ：left/top/right/bottom 四边各自线性插值
 *
 * 其他类型：t >= 1.0f 时返回 to，否则返回 from（离散跳变）。
 *
 * @param from 起始值
 * @param to   终止值
 * @param t    插值参数（通常已由缓动函数变换，范围 [0, 1]）
 * @return     插值结果
 */
static core::Variant lerp_variant(const core::Variant& from,
                                  const core::Variant& to,
                                  float                t) noexcept
{
    // float 标量
    if (from.has<float>() && to.has<float>()) {
        return math::lerp(from.get<float>(), to.get<float>(), t);
    }

    // 线性空间 RGBA 颜色（逐分量）
    if (from.has<math::Color>() && to.has<math::Color>()) {
        const math::Color& f = from.get<math::Color>();
        const math::Color& t2 = to.get<math::Color>();
        return math::Color{
            math::lerp(f.r, t2.r, t),
            math::lerp(f.g, t2.g, t),
            math::lerp(f.b, t2.b, t),
            math::lerp(f.a, t2.a, t),
        };
    }

    // 四边间距（逐边）
    if (from.has<math::Thickness>() && to.has<math::Thickness>()) {
        const math::Thickness& f  = from.get<math::Thickness>();
        const math::Thickness& t2 = to.get<math::Thickness>();
        return math::Thickness{
            math::lerp(f.left,   t2.left,   t),
            math::lerp(f.top,    t2.top,    t),
            math::lerp(f.right,  t2.right,  t),
            math::lerp(f.bottom, t2.bottom, t),
        };
    }

    // 其他类型：离散跳变
    return (t >= 1.0f) ? to : from;
}

// ─────────────────────────────────────────────────────────────────────────────
// Storyboard：配置阶段
// ─────────────────────────────────────────────────────────────────────────────

void Storyboard::animate_dp(ui::DependencyObject&        target,
                            const ui::DependencyProperty& prop,
                            Duration                      duration,
                            EasingFn                      easing) noexcept
{
    PropertyAnimation anim;
    anim.target         = &target;
    anim.prop           = &prop;
    anim.duration       = duration;
    anim.easing         = easing;
    anim.to_is_resolved = false; // to 将在 resolve_and_begin() 中从 StyleTrigger 读取
    animations_.push_back(std::move(anim));
}

void Storyboard::animate_dp_to(ui::DependencyObject&        target,
                               const ui::DependencyProperty& prop,
                               core::Variant                 to,
                               Duration                      duration,
                               EasingFn                      easing) noexcept
{
    PropertyAnimation anim;
    anim.target         = &target;
    anim.prop           = &prop;
    anim.to             = std::move(to);
    anim.to_is_resolved = true; // to 已由调用方显式指定
    anim.duration       = duration;
    anim.easing         = easing;
    animations_.push_back(std::move(anim));
}

// ─────────────────────────────────────────────────────────────────────────────
// Storyboard：生命周期阶段
// ─────────────────────────────────────────────────────────────────────────────

void Storyboard::capture_from_values() noexcept
{
    // 在新状态的 StyleTrigger 写入之前调用
    // 采样目标属性的当前生效值作为动画起始值
    for (PropertyAnimation& anim : animations_) {
        if (anim.target && anim.prop) {
            anim.from = anim.target->get_value(*anim.prop);
        }
    }
}

void Storyboard::resolve_and_begin() noexcept
{
    // 防止重复调用
    if (began_) {
        return;
    }
    began_ = true;

    // 在新状态的 StyleTrigger 写入之后调用：
    //   1. 对 to_is_resolved == false 的动画，从当前生效值（StyleTrigger 层）读取终止值
    //   2. 以 Animation 优先级写入起始值，使属性从 from 出发开始插值
    for (PropertyAnimation& anim : animations_) {
        if (!anim.target || !anim.prop) {
            continue;
        }

        // 解析未确定的终止值
        if (!anim.to_is_resolved) {
            anim.to             = anim.target->get_value(*anim.prop);
            anim.to_is_resolved = true;
        }

        // 以 Animation 优先级写入起始值（优先级 60，高于 StyleTrigger=30）
        // 这会使 get_value() 返回 from，动画从起始值开始
        anim.target->set_value(*anim.prop, anim.from, ValuePriority::Animation);
    }
}

bool Storyboard::tick(float dt) noexcept
{
    if (complete_) {
        return true;
    }

    bool all_done = true;

    for (PropertyAnimation& anim : animations_) {
        if (anim.complete) {
            continue; // 此动画已完成，跳过
        }
        if (!anim.target || !anim.prop) {
            anim.complete = true;
            continue;
        }

        // 累加已流逝时间
        anim.elapsed += dt;

        // 计算归一化进度 t ∈ [0, 1]
        const float dur_sec = anim.duration.to_seconds();
        const float raw_t   = (dur_sec > 0.0f) ? (anim.elapsed / dur_sec) : 1.0f;
        const float t       = math::clamp01(raw_t);

        // 应用缓动函数
        const float t_eased = (anim.easing != nullptr) ? anim.easing(t) : t;

        // 计算插值结果并以 Animation 优先级写回属性
        core::Variant value = lerp_variant(anim.from, anim.to, t_eased);
        anim.target->set_value(*anim.prop, std::move(value), ValuePriority::Animation);

        // 检查是否完成
        if (t >= 1.0f) {
            anim.complete = true;
        } else {
            all_done = false;
        }
    }

    complete_ = all_done;
    return complete_;
}

void Storyboard::stop() noexcept
{
    // 清除所有 Animation 优先级槽
    // 属性值将退回到 StyleTrigger 层（或更低优先级）
    for (PropertyAnimation& anim : animations_) {
        if (anim.target && anim.prop) {
            anim.target->clear_value(*anim.prop, ValuePriority::Animation);
        }
    }
}

}  // namespace mine::ui::animation
