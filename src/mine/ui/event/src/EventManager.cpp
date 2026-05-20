/**
 * @file EventManager.cpp
 * @brief 路由事件派发算法实现。
 *
 * 路由算法说明：
 *
 * Bubble（冒泡）：
 *   从 source 出发，沿 parent_target() 向根部逐步传播。
 *   每步调用 invoke_handlers()，若 handled() 为 true 则停止。
 *
 * Tunnel（隧道 / Preview）：
 *   先从 source 沿 parent_target() 向根收集完整路径到 Vector，
 *   然后反向遍历（根 → source），调用 invoke_handlers()，
 *   若 handled() 为 true 则停止。
 *
 * Direct（直接）：
 *   仅在 source 上调用 invoke_handlers()，不传播到其他节点。
 *
 * Tunnel 路径收集使用栈局部 Vector<IRoutedEventTarget*>，
 * 容量与路径长度相关（通常很浅，几十个元素），性能可接受。
 */

#include <mine/ui/event/EventManager.h>

#include <mine/containers/Vector.h>

namespace mine::ui {

// ────────────────────────────────────────────────────────────────────────────
// 内部辅助函数
// ────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief 执行冒泡传播：source → parent → ... → root。
 */
void bubble_raise(IRoutedEventTarget& source, RoutedEventArgs& args) noexcept {
    const RoutedEvent& event = args.routed_event();
    IRoutedEventTarget* current = &source;
    while (current != nullptr) {
        current->invoke_handlers(event, args);
        if (args.handled()) {
            break;
        }
        current = current->parent_target();
    }
}

/**
 * @brief 执行隧道传播：root → ... → parent → source。
 *
 * 先收集 source→root 路径，再反向派发。
 */
void tunnel_raise(IRoutedEventTarget& source, RoutedEventArgs& args) noexcept {
    // 收集路径（source 在索引 0，根节点在末尾）
    containers::Vector<IRoutedEventTarget*> path;
    IRoutedEventTarget* current = &source;
    while (current != nullptr) {
        path.push_back(current);
        current = current->parent_target();
    }

    // 反向遍历（从根到 source）
    const RoutedEvent& event = args.routed_event();
    for (size_t i = path.size(); i > 0u; --i) {
        path[i - 1u]->invoke_handlers(event, args);
        if (args.handled()) {
            break;
        }
    }
}

/**
 * @brief 执行直接传播：仅在 source 上触发。
 */
void direct_raise(IRoutedEventTarget& source, RoutedEventArgs& args) noexcept {
    const RoutedEvent& event = args.routed_event();
    source.invoke_handlers(event, args);
}

} // namespace (anonymous)

// ────────────────────────────────────────────────────────────────────────────
// EventManager::raise() 实现
// ────────────────────────────────────────────────────────────────────────────

void EventManager::raise(IRoutedEventTarget& source,
                          RoutedEventArgs&    args) noexcept {
    switch (args.routed_event().strategy()) {
        case RoutingStrategy::Bubble:
            bubble_raise(source, args);
            break;
        case RoutingStrategy::Tunnel:
            tunnel_raise(source, args);
            break;
        case RoutingStrategy::Direct:
            direct_raise(source, args);
            break;
    }
}

} // namespace mine::ui
