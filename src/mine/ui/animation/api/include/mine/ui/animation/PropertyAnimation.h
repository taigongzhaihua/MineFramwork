/**
 * @file PropertyAnimation.h
 * @brief PropertyAnimation —— 单个依赖属性动画的描述结构体。
 *
 * PropertyAnimation 是 Storyboard 内部存储的基本单元，描述：
 *   - 动画目标：DependencyObject 实例 + DependencyProperty
 *   - 起止值：from（采样自动画开始前的生效值）/ to（显式或从状态样式解析）
 *   - 时序参数：Duration（持续时长）/ EasingFn（缓动函数）
 *   - 运行时状态：elapsed（已流逝时间）/ complete（是否已完成）
 *
 * 线程安全：所有操作必须在 UI 线程调用。
 *
 * 注意：此结构体对用户不透明，通过 Storyboard::animate_dp / animate_dp_to
 * 工厂方法创建，由 Storyboard 统一管理生命周期。
 */

#pragma once

#include <mine/ui/animation/Api.h>
#include <mine/ui/animation/Duration.h>
#include <mine/ui/animation/EasingFunction.h>
// Variant 作为值成员，必须包含完整头文件
#include <mine/core/Variant.h>

// 前向声明避免循环依赖
namespace mine::ui {
class DependencyObject;
class DependencyProperty;
}  // namespace mine::ui

namespace mine::ui::animation {

/**
 * @brief 单个依赖属性动画描述。
 *
 * 由 Storyboard 创建和管理，不对外直接暴露。
 * 生命周期与所属 Storyboard 相同。
 */
struct PropertyAnimation {
    /// 动画目标元素（非拥有指针；调用方负责其生命周期覆盖动画存续期）
    ui::DependencyObject*        target{nullptr};

    /// 目标属性（非拥有指针；DependencyProperty 为全局静态对象，生命周期永远有效）
    const ui::DependencyProperty* prop{nullptr};

    /// 动画起始值（默认在 capture_from_values() 中采样；也可由调用方显式指定）
    core::Variant from;

    /// false = from 尚未确定，将在 capture_from_values()/resolve_and_begin() 中采样
    /// true  = from 已由调用方显式指定
    bool from_is_resolved{false};

    /// 动画终止值（显式指定时为 animate_dp_to 的参数；否则在 resolve_and_begin() 中采样）
    core::Variant to;

    /// false = "to" 尚未确定，在 resolve_and_begin() 中从当前生效值（StyleTrigger）读取
    /// true  = "to" 已在构造时显式指定（animate_dp_to 路径）
    bool to_is_resolved{false};

    /// 动画持续时长（必须大于零）
    Duration duration;

    /// 缓动函数（默认线性）
    EasingFn easing{Linear};

    /// 已流逝时间（秒），由 Storyboard::tick(dt) 累加
    float elapsed{0.0f};

    /// true = 此单个属性动画已播放完毕
    bool complete{false};

    /// 动画完成后是否保持 Animation(P60) 槽不清除。
    ///
    /// true：目标状态有明确的 StyleTrigger(P30) 终值，P60 需保持以覆盖 Local(P50)；
    ///        go_to_state 切换到新状态时才通过 stop() 清除。
    /// false：目标状态无 StyleTrigger 值（如 Normal 回退到 Local 层），动画完成后
    ///        stop() 清除 P60，由 Local 优先级接管（无颜色跳变）。
    /// 由 Storyboard::resolve_and_begin() 根据 has_value(StyleTrigger) 自动决定。
    bool retain_p60{false};
};

}  // namespace mine::ui::animation
