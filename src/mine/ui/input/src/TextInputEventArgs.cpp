/**
 * @file TextInputEventArgs.cpp
 * @brief TextInputEventArgs 实现。
 */

#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/event/RoutedEvent.h>

namespace mine::ui::input {

TextInputEventArgs::TextInputEventArgs(const RoutedEvent& event,
                                       uint32_t           char_utf32) noexcept
    : RoutedEventArgs{ event }
    , char_utf32_{ char_utf32 }
{}

uint32_t TextInputEventArgs::char_utf32() const noexcept
{
    return char_utf32_;
}

} // namespace mine::ui::input
