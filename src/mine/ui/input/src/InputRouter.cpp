/**
 * @file InputRouter.cpp
 * @brief InputRouter 实现——将平台 WindowEvent 路由为 UI 键盘/鼠标事件。
 *
 * 路由流程：
 *   1. on_window_event() 根据 WindowEvent::kind 分发到 dispatch_key_event 或
 *      dispatch_mouse_event
 *   2. dispatch_mouse_event 先通过 UIElement::hit_test() 确定命中目标，
 *      然后发送 Preview（隧道）+ 正式（冒泡）事件对
 *   3. dispatch_key_event 向 keyboard_focus_ 元素发送键盘事件，
 *      无焦点时退化到根节点
 *
 * Preview-正式事件对：
 *   - 先派发 Preview 事件（隧道）
 *   - 只有 Preview 未被处理（!handled()）时，才派发正式事件（冒泡）
 */

#include <mine/ui/input/InputRouter.h>

#include <mine/platform/WindowEvent.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/Key.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/ModifierKeys.h>
#include <mine/ui/input/MouseButton.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/math/Point.h>

namespace mine::ui::input {

// ── 内部辅助 ─────────────────────────────────────────────────────────────────

/// 从 WindowEvent 中提取修饰键状态
static ModifierKeys make_modifiers(const platform::WindowEvent& we) noexcept {
    ModifierKeys mods = ModifierKeys::None;
    if (we.mod_ctrl)  mods |= ModifierKeys::Ctrl;
    if (we.mod_shift) mods |= ModifierKeys::Shift;
    if (we.mod_alt)   mods |= ModifierKeys::Alt;
    return mods;
}

// ── InputRouter 实现 ─────────────────────────────────────────────────────────

InputRouter::InputRouter() noexcept = default;
InputRouter::~InputRouter()          = default;

void InputRouter::set_root(UIElement* root) noexcept {
    root_ = root;
}

UIElement* InputRouter::root() const noexcept {
    return root_;
}

void InputRouter::set_keyboard_focus(UIElement* element) noexcept {
    if (keyboard_focus_ == element) {
        return;
    }
    // 向旧焦点元素派发 LostFocusEvent
    if (keyboard_focus_) {
        RoutedEventArgs lost_args{ LostFocusEvent() };
        EventManager::raise(*keyboard_focus_, lost_args);
    }
    keyboard_focus_ = element;
    // 向新焦点元素派发 GotFocusEvent
    if (keyboard_focus_) {
        RoutedEventArgs got_args{ GotFocusEvent() };
        EventManager::raise(*keyboard_focus_, got_args);
    }
}

UIElement* InputRouter::keyboard_focus() const noexcept {
    return keyboard_focus_;
}

UIElement* InputRouter::mouse_over_element() const noexcept {
    return mouse_over_;
}

void InputRouter::on_window_event(platform::WindowEvent& event) {
    using Kind = platform::WindowEventKind;
    switch (event.kind) {
    case Kind::KeyDown:
    case Kind::KeyUp:
        dispatch_key_event(event);
        break;
    case Kind::MouseMove:
    case Kind::MouseDown:
    case Kind::MouseUp:
    case Kind::MouseWheel:
        dispatch_mouse_event(event);
        break;
    case Kind::Char:
        dispatch_char_event(event);
        break;
    case Kind::ImeCompositionCommitted:
        dispatch_ime_text_event(event);
        break;
    default:
        // 非输入事件（窗口大小、焦点等）忽略
        break;
    }
}

void InputRouter::dispatch_key_event(const platform::WindowEvent& we) {
    // 键盘事件派发到焦点元素；若无焦点则退化到根节点
    UIElement* target = keyboard_focus_;
    if (!target && root_) {
        target = root_->as_element();
    }
    if (!target) {
        return;
    }

    const Key          k    = key_from_win32_vk(we.key_vk_code);
    const ModifierKeys mods = make_modifiers(we);
    const bool is_down      = (we.kind == platform::WindowEventKind::KeyDown);

    if (is_down) {
        // Preview（隧道）先行
        KeyEventArgs preview{ PreviewKeyDownEvent(), k, we.key_scan_code, mods, we.key_is_repeat };
        EventManager::raise(*target, preview);
        // Preview 未被处理，派发正式冒泡事件
        if (!preview.handled()) {
            KeyEventArgs args{ KeyDownEvent(), k, we.key_scan_code, mods, we.key_is_repeat };
            EventManager::raise(*target, args);
        }
    } else {
        KeyEventArgs preview{ PreviewKeyUpEvent(), k, we.key_scan_code, mods, false };
        EventManager::raise(*target, preview);
        if (!preview.handled()) {
            KeyEventArgs args{ KeyUpEvent(), k, we.key_scan_code, mods, false };
            EventManager::raise(*target, args);
        }
    }
}

void InputRouter::dispatch_mouse_event(const platform::WindowEvent& we) {
    const math::Point  pos  { we.mouse_x, we.mouse_y };
    const ModifierKeys mods = make_modifiers(we);

    // 命中测试——确定目标元素
    UIElement* target = nullptr;
    if (root_) {
        target = root_->hit_test(pos);
    }

    // 悬停元素发生切换时，合成 MouseLeave / MouseEnter
    if (target != prev_mouse_over_) {
        // 将当前悬停元素更新到 mouse_over_ 前，先向旧元素派发 MouseLeave
        if (prev_mouse_over_) {
            MouseEventArgs leave_args{ MouseLeaveEvent(), MouseButton::None, pos, mods };
            EventManager::raise(*prev_mouse_over_, leave_args);
        }
        // 再向新元素派发 MouseEnter
        if (target) {
            MouseEventArgs enter_args{ MouseEnterEvent(), MouseButton::None, pos, mods };
            EventManager::raise(*target, enter_args);
        }
        prev_mouse_over_ = target;
    }

    // 更新公开的悬停属性
    mouse_over_ = target;

    if (!target) {
        return;
    }

    using Kind = platform::WindowEventKind;

    switch (we.kind) {
    case Kind::MouseDown: {
        const MouseButton btn = static_cast<MouseButton>(we.mouse_button);
        // MouseDown 时：若命中元素可聚焦，自动将其设为键盘焦点
        if (target->is_focusable()) {
            set_keyboard_focus(target);
        } else {
            // 命中非可聚焦元素时，清除键盘焦点（支持点击空白区域失焦）
            set_keyboard_focus(nullptr);
        }
        MouseEventArgs preview{ PreviewMouseDownEvent(), btn, pos, mods };
        EventManager::raise(*target, preview);
        if (!preview.handled()) {
            MouseEventArgs args{ MouseDownEvent(), btn, pos, mods };
            EventManager::raise(*target, args);
        }
        break;
    }
    case Kind::MouseUp: {
        const MouseButton btn = static_cast<MouseButton>(we.mouse_button);
        MouseEventArgs preview{ PreviewMouseUpEvent(), btn, pos, mods };
        EventManager::raise(*target, preview);
        if (!preview.handled()) {
            MouseEventArgs args{ MouseUpEvent(), btn, pos, mods };
            EventManager::raise(*target, args);
        }
        break;
    }
    case Kind::MouseMove: {
        MouseEventArgs args{ MouseMoveEvent(), MouseButton::None, pos, mods };
        EventManager::raise(*target, args);
        break;
    }
    case Kind::MouseWheel: {
        MouseEventArgs args{ MouseWheelEvent(), MouseButton::None, pos, mods,
                             we.mouse_wheel_delta };
        EventManager::raise(*target, args);
        break;
    }
    default:
        break;
    }
}

void InputRouter::dispatch_char_event(const platform::WindowEvent& we) {
    // 字符输入派发到键盘焦点；无焦点时退化到根节点
    UIElement* target = keyboard_focus_;
    if (!target && root_) {
        target = root_->as_element();
    }
    if (!target) {
        return;
    }
    TextInputEventArgs args{ TextInputEvent(), we.char_utf32 };
    EventManager::raise(*target, args);
}

void InputRouter::dispatch_ime_text_event(const platform::WindowEvent& we) {
    // IME 确认提交的 UTF-8 字符串逐码点转为 UTF-32，每个码点派发一次 TextInputEvent
    UIElement* target = keyboard_focus_;
    if (!target && root_) {
        target = root_->as_element();
    }
    if (!target) {
        return;
    }

    const char*    data = we.ime_text_utf8;
    const uint32_t len  = we.ime_text_length;
    uint32_t       i    = 0;

    while (i < len) {
        const auto c = static_cast<uint8_t>(data[i]);
        uint32_t   cp      = 0;
        uint32_t   seq_len = 1;

        if ((c & 0x80u) == 0u) {
            cp      = c;
            seq_len = 1;
        } else if ((c & 0xE0u) == 0xC0u && (i + 1u) < len) {
            cp      = static_cast<uint32_t>(c & 0x1Fu) << 6
                    | (static_cast<uint8_t>(data[i + 1]) & 0x3Fu);
            seq_len = 2;
        } else if ((c & 0xF0u) == 0xE0u && (i + 2u) < len) {
            cp      = static_cast<uint32_t>(c & 0x0Fu) << 12
                    | (static_cast<uint32_t>(static_cast<uint8_t>(data[i + 1]) & 0x3Fu) << 6)
                    | (static_cast<uint8_t>(data[i + 2]) & 0x3Fu);
            seq_len = 3;
        } else if ((c & 0xF8u) == 0xF0u && (i + 3u) < len) {
            cp      = static_cast<uint32_t>(c & 0x07u) << 18
                    | (static_cast<uint32_t>(static_cast<uint8_t>(data[i + 1]) & 0x3Fu) << 12)
                    | (static_cast<uint32_t>(static_cast<uint8_t>(data[i + 2]) & 0x3Fu) << 6)
                    | (static_cast<uint8_t>(data[i + 3]) & 0x3Fu);
            seq_len = 4;
        } else {
            // 无效 UTF-8 字节，跳过一个字节
            ++i;
            continue;
        }

        i += seq_len;

        // 过滤控制字符（码点 < 0x20 由 KeyDown 处理）
        if (cp >= 0x20u) {
            TextInputEventArgs args{ TextInputEvent(), cp };
            EventManager::raise(*target, args);
        }
    }
}

} // namespace mine::ui::input
