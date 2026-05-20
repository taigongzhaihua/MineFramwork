/**
 * @file EventManager.h
 * @brief 路由事件派发器 —— 实现 Bubble/Tunnel/Direct 传播算法。
 *
 * EventManager 是纯静态工具类，提供统一的事件派发入口：
 *   - Bubble（冒泡）：从 source 沿可视化树向根部传播
 *   - Tunnel（隧道）：从根部沿可视化树向 source 传播（"Preview" 事件）
 *   - Direct（直接）：仅在 source 上触发，不传播
 *
 * 传播规则：
 *   - 任意处理器将 RoutedEventArgs::handled() 设为 true 后，传播立即停止
 *   - Tunnel 事件收集完整路径后再反向派发，确保根部先处理
 *
 * 使用示例：
 * @code
 *   RoutedEventArgs args{Button::ClickEvent};
 *   args.set_source(button_ptr);
 *   args.set_original_source(button_ptr);
 *   EventManager::raise(*button_element, args);
 * @endcode
 */

#pragma once

#include <mine/ui/event/Api.h>
#include <mine/ui/event/IRoutedEventTarget.h>

namespace mine::ui {

/**
 * @brief 路由事件派发器（纯静态工具类，不可实例化）。
 *
 * 调用 raise() 后，EventManager 根据 args.routed_event().strategy() 选择传播路径，
 * 并在路径上的每个节点调用 IRoutedEventTarget::invoke_handlers()。
 */
class MINE_UI_EVENT_API EventManager {
public:
    EventManager()                               = delete;
    EventManager(const EventManager&)            = delete;
    EventManager& operator=(const EventManager&) = delete;

    /**
     * @brief 派发路由事件。
     *
     * 根据 args.routed_event().strategy() 执行相应的路由算法：
     *   - RoutingStrategy::Bubble  → source → parent → ... → root
     *   - RoutingStrategy::Tunnel  → root → ... → parent → source
     *   - RoutingStrategy::Direct  → source only
     *
     * 任意处理器将 args.handled() 置为 true 后，传播立即停止（不再调用后续节点）。
     *
     * @param source 事件起源的目标元素（路由路径的叶节点）
     * @param args   路由事件参数（内含事件描述符、源信息、Handled 标志）
     */
    static void raise(IRoutedEventTarget& source,
                      RoutedEventArgs&    args) noexcept;
};

} // namespace mine::ui
