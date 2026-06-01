/**
 * @file Style.cpp
 * @brief mine::ui::style::Style 实现。
 *
 * 样式应用流程：
 *   apply()       ──> 以 ValuePriority::StyleSetter(20) 写入属性值
 *   apply_state() ──> 以 ValuePriority::StyleTrigger(30) 写入属性值
 *
 * 两者均低于 Local(50)，因此本地值不会被样式覆盖。
 */

#include <mine/ui/style/Style.h>

#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/ValuePriority.h>
#include <mine/containers/InlineString.h>

namespace mine::ui::style {

// ─────────────────────────────────────────────────────────────────────────────
// apply
// ─────────────────────────────────────────────────────────────────────────────

void Style::apply(ui::DependencyObject& target) const {
    // 1. 先应用父样式（BasedOn 继承），父样式值优先级最低（后写的子样式可覆盖）
    if (base_) {
        base_->apply(target);
    }

    // 2. mmlc 生成函数路径（优先于程序化 setters_）
    //    mmlc 路径包含 DynamicResource 订阅、类型转换等复杂逻辑
    if (apply_fn_) {
        apply_fn_(target);
        return;
    }

    // 3. 程序化 setter 路径：遍历 setters_，对静态 setter 以 StyleSetter 优先级写入
    for (const auto& setter : setters_) {
        // 跳过空属性指针（防御性检查）
        if (!setter.property) {
            continue;
        }
        if (setter.res_key.empty()) {
            // 静态值：直接写入 StyleSetter(20) 优先级
            // 若目标已有 Local(50) 或更高优先级值，set_value 内部会忽略本次写入
            target.set_value(*setter.property, setter.value, ValuePriority::StyleSetter);
        }
        // DynamicResource（res_key 非空）：需要资源字典上下文与订阅机制
        // 程序化路径暂不处理，由 mmlc 生成的 apply_fn_ 负责（后续任务实现）
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// apply_state
// ─────────────────────────────────────────────────────────────────────────────

void Style::apply_state(ui::DependencyObject& target, core::StringView state_name) const {
    // 查找与 state_name 匹配的视觉状态 setter 集合
    for (const auto& state : state_setters_) {
        if (state.state_name == state_name) {
            // 以 StyleTrigger(30) 优先级写入该状态下的所有属性值
            // StyleTrigger(30) > StyleSetter(20)，但仍低于 Local(50)
            for (const auto& setter : state.setters) {
                if (!setter.property) {
                    continue;
                }
                if (setter.res_key.empty()) {
                    target.set_value(*setter.property, setter.value,
                                     ValuePriority::StyleTrigger);
                }
                // DynamicResource：同 apply()，程序化路径暂不处理
            }
            return;  // 找到状态后立即返回，每个状态名唯一
        }
    }
    // 状态名不存在：空操作（不报错，允许状态尚未定义 setter）
}

// ─────────────────────────────────────────────────────────────────────────────
// clear_all_state_values
// ─────────────────────────────────────────────────────────────────────────────

void Style::clear_all_state_values(ui::DependencyObject& target) const {
    // 遍历所有状态的 setter，对每个属性清除 StyleTrigger(30) 槽。
    // 使用 has_value 先检查槽是否存在，避免触发无意义的属性变更通知。
    for (const auto& state : state_setters_) {
        for (const auto& setter : state.setters) {
            if (!setter.property) {
                continue;
            }
            if (target.has_value(*setter.property, ValuePriority::StyleTrigger)) {
                target.clear_value(*setter.property, ValuePriority::StyleTrigger);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 构建器接口
// ─────────────────────────────────────────────────────────────────────────────

Style& Style::set_name(core::StringView name) {
    // InlineString 接受 (const char*, size_t)，无越界风险（SSO 截断）
    name_ = containers::InlineString{name.data(), name.size()};
    return *this;
}

Style& Style::set_target_type(core::TypeId type_id) noexcept {
    target_type_ = type_id;
    return *this;
}

Style& Style::set_base(Style* base) noexcept {
    base_ = base;
    return *this;
}

Style& Style::add_setter(StyleSetter setter) {
    setters_.push_back(std::move(setter));
    return *this;
}

Style& Style::add_state_setters(VisualStateSetters state_setters) {
    state_setters_.push_back(std::move(state_setters));
    return *this;
}

}  // namespace mine::ui::style
