/**
 * @file MouseEventArgs.h
 * @brief 鼠标路由事件参数。
 *
 * MouseEventArgs 携带鼠标事件（MouseDown / MouseUp / MouseMove / MouseWheel
 * 及其 Preview 变体）的上下文数据，继承自 RoutedEventArgs，在调用栈上分配，不可拷贝。
 */

#pragma once

#include <mine/math/Point.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/input/Api.h>
#include <mine/ui/input/ModifierKeys.h>
#include <mine/ui/input/MouseButton.h>

namespace mine::ui::input {

/**
 * @brief 鼠标事件参数。
 *
 * 包含按键（按下/释放场景）、鼠标当前位置（相对于根元素的逻辑坐标）、
 * 修饰键状态，以及滚轮增量（MouseWheel 场景）。
 *
 * 示例（鼠标按下）：
 * @code
 *   MouseEventArgs args{MouseDownEvent(), MouseButton::Left,
 *                       {x, y}, ModifierKeys::None};
 *   EventManager::raise(*element, args);
 * @endcode
 *
 * 示例（滚轮）：
 * @code
 *   MouseEventArgs args{MouseWheelEvent(), MouseButton::None,
 *                       {x, y}, ModifierKeys::None, 120.0f};
 *   EventManager::raise(*element, args);
 * @endcode
 */
class MINE_UI_INPUT_API MouseEventArgs : public RoutedEventArgs {
public:
    /**
     * @brief 构造鼠标事件参数。
     * @param event       路由事件描述符
     * @param button      触发事件的鼠标按键（MouseMove/MouseWheel 传 None）
     * @param position    鼠标位置（相对于根元素的逻辑坐标，单位：逻辑像素）
     * @param modifiers   修饰键状态
     * @param wheel_delta 滚轮增量（正值向上，一格约 120.0f；非滚轮事件传 0）
     */
    explicit MouseEventArgs(const RoutedEvent& event,
                            MouseButton        button,
                            math::Point        position,
                            ModifierKeys       modifiers,
                            float              wheel_delta = 0.0f) noexcept;

    /// 触发事件的鼠标按键（MouseMove / MouseWheel 返回 None）
    [[nodiscard]] MouseButton button() const noexcept;
    /// 鼠标位置（逻辑坐标）
    [[nodiscard]] math::Point position() const noexcept;
    /// 修饰键状态
    [[nodiscard]] ModifierKeys modifiers() const noexcept;
    /// 滚轮增量（正值向上；非滚轮事件为 0.0f）
    [[nodiscard]] float wheel_delta() const noexcept;

    /// 快捷访问：Ctrl 是否按下
    [[nodiscard]] bool ctrl() const noexcept;
    /// 快捷访问：Shift 是否按下
    [[nodiscard]] bool shift() const noexcept;
    /// 快捷访问：Alt 是否按下
    [[nodiscard]] bool alt() const noexcept;

private:
    MouseButton  button_;
    math::Point  position_;
    ModifierKeys modifiers_;
    float        wheel_delta_;
};

} // namespace mine::ui::input
