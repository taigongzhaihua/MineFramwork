/**
 * @file RoutedEventArgs.cpp
 * @brief RoutedEventArgs 基类实现。
 */

#include <mine/ui/event/RoutedEventArgs.h>

#include <mine/core/Assert.h>

namespace mine::ui {

RoutedEventArgs::RoutedEventArgs(const RoutedEvent& event) noexcept
    : event_ {&event}
{}

const RoutedEvent& RoutedEventArgs::routed_event() const noexcept {
    MINE_ASSERT(event_ != nullptr);
    return *event_;
}

void* RoutedEventArgs::source() const noexcept {
    return source_;
}

void* RoutedEventArgs::original_source() const noexcept {
    return original_source_;
}

void RoutedEventArgs::set_source(void* src) noexcept {
    source_ = src;
}

void RoutedEventArgs::set_original_source(void* src) noexcept {
    original_source_ = src;
}

bool RoutedEventArgs::handled() const noexcept {
    return handled_;
}

void RoutedEventArgs::set_handled(bool handled) noexcept {
    handled_ = handled;
}

} // namespace mine::ui
