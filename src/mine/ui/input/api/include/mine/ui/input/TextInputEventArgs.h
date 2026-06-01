/**
 * @file TextInputEventArgs.h
 * @brief 字符输入路由事件参数。
 *
 * TextInputEventArgs 携带 TextInputEvent 的上下文数据：
 *   - char_utf32：已处理修饰键的可打印 Unicode 码点（UTF-32）
 *
 * TextInputEvent 由 InputRouter::dispatch_char_event 在收到平台
 * WindowEventKind::Char 时派发，目标为当前键盘焦点元素（Bubble 策略）。
 */

#pragma once

#include <cstdint>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/input/Api.h>

namespace mine::ui { class RoutedEvent; }

namespace mine::ui::input {

/**
 * @brief 字符输入事件参数。
 *
 * 每个 TextInputEventArgs 对象对应平台发送的一个可打印字符输入。
 * 控制字符（< 32，如退格、回车）不通过此事件传递，由 KeyDownEvent 处理。
 *
 * 示例（TextBox 内部注册处理器）：
 * @code
 *   add_handler(TextInputEvent(), &TextBox::on_text_input_router, this);
 *
 *   void TextBox::on_text_input(TextInputEventArgs& args) {
 *       insert_char(args.char_utf32());
 *       args.set_handled(true);
 *   }
 * @endcode
 */
class MINE_UI_INPUT_API TextInputEventArgs : public RoutedEventArgs {
public:
    /**
     * @brief 构造字符输入事件参数。
     * @param event      路由事件描述符（TextInputEvent）
     * @param char_utf32 Unicode 码点（UTF-32），保证为可打印字符
     */
    explicit TextInputEventArgs(const RoutedEvent& event,
                                uint32_t           char_utf32) noexcept;

    /// 字符 Unicode 码点（UTF-32）
    [[nodiscard]] uint32_t char_utf32() const noexcept;

private:
    uint32_t char_utf32_;
};

} // namespace mine::ui::input
