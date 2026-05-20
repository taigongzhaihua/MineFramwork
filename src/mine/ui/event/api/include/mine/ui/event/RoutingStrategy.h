/**
 * @file RoutingStrategy.h
 * @brief 路由事件的传播策略枚举。
 *
 * 路由事件支持三种传播方式，与 WPF 路由事件模型对齐：
 *   - Bubble（冒泡）：从命中元素向可视化树根部逐级传播（最常用）
 *   - Tunnel（隧道/Preview）：从根部向命中元素逐级传播，先于冒泡阶段
 *   - Direct（直接）：仅在事件源上派发，不在树上传播
 *
 * 使用示例：
 * @code
 *   // 注册一个冒泡路由事件
 *   const RoutedEvent& ClickEvent =
 *       register_event<Button>("Click", RoutingStrategy::Bubble);
 *
 *   // 注册配对的隧道预览事件
 *   const RoutedEvent& PreviewClickEvent =
 *       register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);
 * @endcode
 */

#pragma once

#include <cstdint>

namespace mine::ui {

/**
 * @brief 路由事件传播策略。
 */
enum class RoutingStrategy : uint8_t {
    /// 冒泡：从事件源向可视化树根部逐级传播
    Bubble,
    /// 隧道（Preview）：从可视化树根部向事件源逐级传播
    Tunnel,
    /// 直接：仅在事件源上派发，不向树上/下传播
    Direct,
};

} // namespace mine::ui
