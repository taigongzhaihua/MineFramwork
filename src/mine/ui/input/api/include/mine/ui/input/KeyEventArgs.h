/**
 * @file KeyEventArgs.h
 * @brief 键盘路由事件参数。
 *
 * KeyEventArgs 携带键盘事件（KeyDown / KeyUp 及其 Preview 变体）的上下文数据，
 * 继承自 RoutedEventArgs，在调用栈上分配，不可拷贝。
 */

#pragma once

#include <cstdint>

#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/input/Api.h>
#include <mine/ui/input/Key.h>
#include <mine/ui/input/ModifierKeys.h>

namespace mine::ui::input {

/**
 * @brief 键盘事件参数。
 *
 * 包含按键信息（Key 枚举）、硬件扫描码、修饰键状态及是否为自动重复。
 *
 * 示例：
 * @code
 *   KeyEventArgs args{KeyDownEvent(), Key::A, 0, ModifierKeys::None, false};
 *   EventManager::raise(*element, args);
 * @endcode
 */
class MINE_UI_INPUT_API KeyEventArgs : public RoutedEventArgs {
public:
    /**
     * @brief 构造键盘事件参数。
     * @param event     路由事件描述符（如 KeyDownEvent()）
     * @param key       平台无关虚拟键枚举
     * @param scan_code 硬件扫描码
     * @param modifiers 修饰键状态
     * @param is_repeat 是否为键盘自动重复
     */
    explicit KeyEventArgs(const RoutedEvent& event,
                          Key                key,
                          uint32_t           scan_code,
                          ModifierKeys       modifiers,
                          bool               is_repeat) noexcept;

    /// 触发事件的虚拟键
    [[nodiscard]] Key key() const noexcept;
    /// 硬件扫描码
    [[nodiscard]] uint32_t scan_code() const noexcept;
    /// 修饰键状态（Ctrl / Shift / Alt 位域）
    [[nodiscard]] ModifierKeys modifiers() const noexcept;
    /// 是否为键盘自动重复（按住不放产生的重复事件）
    [[nodiscard]] bool is_repeat() const noexcept;

    /// 快捷访问：Ctrl 是否按下
    [[nodiscard]] bool ctrl() const noexcept;
    /// 快捷访问：Shift 是否按下
    [[nodiscard]] bool shift() const noexcept;
    /// 快捷访问：Alt 是否按下
    [[nodiscard]] bool alt() const noexcept;

private:
    Key          key_;
    uint32_t     scan_code_;
    ModifierKeys modifiers_;
    bool         is_repeat_;
};

} // namespace mine::ui::input
