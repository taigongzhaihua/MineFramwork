/**
 * @file InputEvents.cpp
 * @brief mine.ui.input 标准路由事件注册。
 *
 * 每个函数内部使用 Meyer's 单例：首次调用时通过 register_event<UIElement>() 注册，
 * 此后返回同一常量引用。UIElement 被选作所有者类型，因为输入事件在视觉树上路由。
 */

#include <mine/ui/input/InputEvents.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutingStrategy.h>
#include <mine/ui/visual/UIElement.h>

namespace mine::ui::input {

// ── 键盘事件注册 ─────────────────────────────────────────────────────────────

const RoutedEvent& PreviewKeyDownEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("PreviewKeyDown", RoutingStrategy::Tunnel);
    return ev;
}

const RoutedEvent& KeyDownEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("KeyDown", RoutingStrategy::Bubble);
    return ev;
}

const RoutedEvent& PreviewKeyUpEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("PreviewKeyUp", RoutingStrategy::Tunnel);
    return ev;
}

const RoutedEvent& KeyUpEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("KeyUp", RoutingStrategy::Bubble);
    return ev;
}

// ── 鼠标事件注册 ─────────────────────────────────────────────────────────────

const RoutedEvent& PreviewMouseDownEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("PreviewMouseDown", RoutingStrategy::Tunnel);
    return ev;
}

const RoutedEvent& MouseDownEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("MouseDown", RoutingStrategy::Bubble);
    return ev;
}

const RoutedEvent& PreviewMouseUpEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("PreviewMouseUp", RoutingStrategy::Tunnel);
    return ev;
}

const RoutedEvent& MouseUpEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("MouseUp", RoutingStrategy::Bubble);
    return ev;
}

const RoutedEvent& MouseMoveEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("MouseMove", RoutingStrategy::Bubble);
    return ev;
}

const RoutedEvent& MouseWheelEvent() {
    static const RoutedEvent& ev =
        register_event<UIElement>("MouseWheel", RoutingStrategy::Bubble);
    return ev;
}

} // namespace mine::ui::input
