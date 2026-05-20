/**
 * @file KeyEventArgs.cpp
 * @brief KeyEventArgs 实现。
 */

#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/ModifierKeys.h>

namespace mine::ui::input {

KeyEventArgs::KeyEventArgs(const RoutedEvent& event,
                           Key                key,
                           uint32_t           scan_code,
                           ModifierKeys       modifiers,
                           bool               is_repeat) noexcept
    : RoutedEventArgs(event)
    , key_(key)
    , scan_code_(scan_code)
    , modifiers_(modifiers)
    , is_repeat_(is_repeat)
{}

Key KeyEventArgs::key() const noexcept {
    return key_;
}

uint32_t KeyEventArgs::scan_code() const noexcept {
    return scan_code_;
}

ModifierKeys KeyEventArgs::modifiers() const noexcept {
    return modifiers_;
}

bool KeyEventArgs::is_repeat() const noexcept {
    return is_repeat_;
}

bool KeyEventArgs::ctrl() const noexcept {
    return has_flag(modifiers_, ModifierKeys::Ctrl);
}

bool KeyEventArgs::shift() const noexcept {
    return has_flag(modifiers_, ModifierKeys::Shift);
}

bool KeyEventArgs::alt() const noexcept {
    return has_flag(modifiers_, ModifierKeys::Alt);
}

} // namespace mine::ui::input
