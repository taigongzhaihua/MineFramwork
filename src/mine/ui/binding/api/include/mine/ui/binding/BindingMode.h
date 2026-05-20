/**
 * @file BindingMode.h
 * @brief 绑定方向枚举，控制数据在源与目标之间的流动方向。
 *
 * M1.1 实现：OneWay
 * M2 实现：TwoWay、OneWayToSource（需要 setter + 防循环机制）
 */

#pragma once

#include <cstdint>

namespace mine::ui {

/**
 * @brief 绑定方向，决定 BindingExpression 的数据流动模式。
 *
 * 值优先级在 DependencyObject 中通过 ValuePriority::TemplateBind 管理，
 * TwoWay 的回写以 ValuePriority::Local 写入源对象。
 */
enum class BindingMode : uint8_t {
    /**
     * @brief 单向绑定（源 → 目标）。
     *
     * 源属性变更时自动更新目标属性；目标属性的变更不影响源。
     * M1.1 实现此模式。
     */
    OneWay,

    /**
     * @brief 双向绑定（源 ↔ 目标）。
     *
     * 源或目标任一端变更均触发对方更新。
     * 内置防循环保护（is_updating 标志）。
     * M2 实现此模式。
     */
    TwoWay,

    /**
     * @brief 单向回写绑定（目标 → 源）。
     *
     * 目标属性变更时自动写回源，源变更不影响目标。
     * M2 实现此模式。
     */
    OneWayToSource,

    /**
     * @brief 一次性绑定（仅初始化时读取一次）。
     *
     * attach() 时求值并写入目标，之后不再订阅源变更。
     * 比 OneWay 性能更好，适用于静态内容。
     * M1.1 支持（attach 后立即 detach）。
     */
    OneTime,
};

} // namespace mine::ui
