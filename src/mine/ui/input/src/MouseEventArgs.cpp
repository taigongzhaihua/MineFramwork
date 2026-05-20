/**
 * @file MouseEventArgs.cpp
 * @brief MouseEventArgs 实现。
 */

#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/ModifierKeys.h>

namespace mine::ui::input {

MouseEventArgs::MouseEventArgs(const RoutedEvent& event,
                               MouseButton        button,
                               math::Point        position,
                               ModifierKeys       modifiers,
                               float              wheel_delta) noexcept
    : RoutedEventArgs(event)
    , button_(button)
    , position_(position)
    , modifiers_(modifiers)
    , wheel_delta_(wheel_delta)
{}

MouseButton MouseEventArgs::button() const noexcept {
    return button_;
}

math::Point MouseEventArgs::position() const noexcept {
    return position_;
}

ModifierKeys MouseEventArgs::modifiers() const noexcept {
    return modifiers_;
}

float MouseEventArgs::wheel_delta() const noexcept {
    return wheel_delta_;
}

bool MouseEventArgs::ctrl() const noexcept {
    return has_flag(modifiers_, ModifierKeys::Ctrl);
}

bool MouseEventArgs::shift() const noexcept {
    return has_flag(modifiers_, ModifierKeys::Shift);
}

bool MouseEventArgs::alt() const noexcept {
    return has_flag(modifiers_, ModifierKeys::Alt);
}

} // namespace mine::ui::input
