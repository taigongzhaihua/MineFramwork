/**
 * @file InputTest.cpp
 * @brief mine.ui.input 模块单元测试。
 *
 * 覆盖场景：
 *   ModifierKeys：
 *     - 默认为 None
 *     - 位运算 |、& 和 has_flag
 *   Key：
 *     - key_from_win32_vk 直接转换
 *   KeyEventArgs：
 *     - 构造和访问器
 *     - ctrl/shift/alt 快捷访问
 *   MouseEventArgs：
 *     - 构造和访问器
 *     - wheel_delta 默认值
 *   InputEvents：
 *     - 同一事件多次调用返回同一引用
 *     - 策略正确（Tunnel/Bubble）
 *   InputRouter：
 *     - 无根节点时不崩溃
 *     - 命中测试集成：命中元素接收 MouseDown 事件
 *     - 键盘焦点元素接收 KeyDown 事件
 *     - Preview 事件处理后正式事件被跳过
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/input/InputAll.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/ui/event/RoutingStrategy.h>
#include <mine/platform/WindowEvent.h>

using namespace mine::ui::input;
using namespace mine::ui;
using namespace mine::math;
using namespace mine::platform;

// ============================================================================
// 测试辅助：可记录事件的 UIElement 子类
// ============================================================================

namespace {

/// 测试用 UIElement，记录收到的路由事件类型和参数
class TestElement : public UIElement {
public:
    int key_down_count    = 0;
    int key_up_count      = 0;
    int mouse_down_count  = 0;
    int mouse_up_count    = 0;
    int mouse_move_count  = 0;
    int mouse_wheel_count = 0;

    // 最近一次键盘事件数据
    Key          last_key     = Key::None;
    ModifierKeys last_mods    = ModifierKeys::None;
    bool         last_repeat  = false;

    // 最近一次鼠标事件数据
    MouseButton  last_button      = MouseButton::None;
    Point        last_pos         = {};
    float        last_wheel_delta = 0.0f;

    TestElement() {
        add_handler(KeyDownEvent(),
            [](void*, RoutedEventArgs& a, void* ud) {
                auto* self = static_cast<TestElement*>(ud);
                ++self->key_down_count;
                auto& ka     = static_cast<KeyEventArgs&>(a);
                self->last_key    = ka.key();
                self->last_mods   = ka.modifiers();
                self->last_repeat = ka.is_repeat();
            }, this);

        add_handler(KeyUpEvent(),
            [](void*, RoutedEventArgs&, void* ud) {
                ++static_cast<TestElement*>(ud)->key_up_count;
            }, this);

        add_handler(MouseDownEvent(),
            [](void*, RoutedEventArgs& a, void* ud) {
                auto* self = static_cast<TestElement*>(ud);
                ++self->mouse_down_count;
                auto& ma       = static_cast<MouseEventArgs&>(a);
                self->last_button = ma.button();
                self->last_pos    = ma.position();
            }, this);

        add_handler(MouseUpEvent(),
            [](void*, RoutedEventArgs&, void* ud) {
                ++static_cast<TestElement*>(ud)->mouse_up_count;
            }, this);

        add_handler(MouseMoveEvent(),
            [](void*, RoutedEventArgs& a, void* ud) {
                auto* self = static_cast<TestElement*>(ud);
                ++self->mouse_move_count;
                self->last_pos = static_cast<MouseEventArgs&>(a).position();
            }, this);

        add_handler(MouseWheelEvent(),
            [](void*, RoutedEventArgs& a, void* ud) {
                auto* self = static_cast<TestElement*>(ud);
                ++self->mouse_wheel_count;
                self->last_wheel_delta = static_cast<MouseEventArgs&>(a).wheel_delta();
            }, this);
    }
};

/// 构造鼠标 WindowEvent 辅助函数
inline WindowEvent make_mouse_event(WindowEventKind kind,
                                    float x, float y,
                                    uint8_t button = 0,
                                    float wheel    = 0.0f) {
    WindowEvent ev{};
    ev.kind              = kind;
    ev.mouse_x           = x;
    ev.mouse_y           = y;
    ev.mouse_button      = button;
    ev.mouse_wheel_delta = wheel;
    return ev;
}

/// 构造键盘 WindowEvent 辅助函数
inline WindowEvent make_key_event(WindowEventKind kind,
                                  uint32_t vk,
                                  bool is_repeat = false) {
    WindowEvent ev{};
    ev.kind          = kind;
    ev.key_vk_code   = vk;
    ev.key_scan_code  = 0;
    ev.key_is_repeat  = is_repeat;
    return ev;
}

} // anonymous namespace

// ============================================================================
// ModifierKeys 测试
// ============================================================================

TEST_SUITE("ModifierKeys") {
    TEST_CASE("默认值为 None") {
        ModifierKeys m = ModifierKeys::None;
        CHECK(m == ModifierKeys::None);
    }

    TEST_CASE("位或运算") {
        ModifierKeys m = ModifierKeys::Ctrl | ModifierKeys::Shift;
        CHECK(has_flag(m, ModifierKeys::Ctrl));
        CHECK(has_flag(m, ModifierKeys::Shift));
        CHECK_FALSE(has_flag(m, ModifierKeys::Alt));
    }

    TEST_CASE("位与运算") {
        ModifierKeys m = ModifierKeys::Ctrl | ModifierKeys::Alt;
        CHECK((m & ModifierKeys::Ctrl) == ModifierKeys::Ctrl);
        CHECK((m & ModifierKeys::Shift) == ModifierKeys::None);
    }

    TEST_CASE("has_flag") {
        ModifierKeys m = ModifierKeys::Shift;
        CHECK(has_flag(m, ModifierKeys::Shift));
        CHECK_FALSE(has_flag(m, ModifierKeys::Ctrl));
        CHECK_FALSE(has_flag(m, ModifierKeys::Alt));
    }
}

// ============================================================================
// Key 测试
// ============================================================================

TEST_SUITE("Key") {
    TEST_CASE("key_from_win32_vk 直接转换") {
        CHECK(key_from_win32_vk(0x41) == Key::A);
        CHECK(key_from_win32_vk(0x0D) == Key::Enter);
        CHECK(key_from_win32_vk(0x70) == Key::F1);
        CHECK(key_from_win32_vk(0x25) == Key::Left);
        CHECK(key_from_win32_vk(0x00) == Key::None);
    }
}

// ============================================================================
// KeyEventArgs 测试
// ============================================================================

TEST_SUITE("KeyEventArgs") {
    TEST_CASE("构造与访问器") {
        KeyEventArgs args{ KeyDownEvent(), Key::A, 30u,
                           ModifierKeys::Ctrl | ModifierKeys::Shift, true };

        CHECK(args.key() == Key::A);
        CHECK(args.scan_code() == 30u);
        CHECK(args.modifiers() == (ModifierKeys::Ctrl | ModifierKeys::Shift));
        CHECK(args.is_repeat());
    }

    TEST_CASE("快捷修饰键访问") {
        KeyEventArgs args{ KeyUpEvent(), Key::Escape, 0,
                           ModifierKeys::Alt, false };

        CHECK_FALSE(args.ctrl());
        CHECK_FALSE(args.shift());
        CHECK(args.alt());
        CHECK_FALSE(args.is_repeat());
    }

    TEST_CASE("初始 handled() 为 false") {
        KeyEventArgs args{ KeyDownEvent(), Key::Space, 0, ModifierKeys::None, false };
        CHECK_FALSE(args.handled());
    }

    TEST_CASE("set_handled 后 handled() 为 true") {
        KeyEventArgs args{ KeyDownEvent(), Key::Enter, 0, ModifierKeys::None, false };
        args.set_handled(true);
        CHECK(args.handled());
    }
}

// ============================================================================
// MouseEventArgs 测试
// ============================================================================

TEST_SUITE("MouseEventArgs") {
    TEST_CASE("构造与访问器") {
        MouseEventArgs args{ MouseDownEvent(), MouseButton::Left,
                             { 100.0f, 200.0f }, ModifierKeys::Shift };

        CHECK(args.button() == MouseButton::Left);
        CHECK(args.position().x == doctest::Approx(100.0f));
        CHECK(args.position().y == doctest::Approx(200.0f));
        CHECK(has_flag(args.modifiers(), ModifierKeys::Shift));
        CHECK(args.wheel_delta() == doctest::Approx(0.0f));
    }

    TEST_CASE("滚轮增量") {
        MouseEventArgs args{ MouseWheelEvent(), MouseButton::None,
                             { 0.0f, 0.0f }, ModifierKeys::None, 120.0f };
        CHECK(args.wheel_delta() == doctest::Approx(120.0f));
    }

    TEST_CASE("初始 handled() 为 false") {
        MouseEventArgs args{ MouseMoveEvent(), MouseButton::None,
                             {}, ModifierKeys::None };
        CHECK_FALSE(args.handled());
    }
}

// ============================================================================
// InputEvents 测试
// ============================================================================

TEST_SUITE("InputEvents") {
    TEST_CASE("同一事件多次调用返回同一引用") {
        CHECK(&KeyDownEvent()        == &KeyDownEvent());
        CHECK(&KeyUpEvent()          == &KeyUpEvent());
        CHECK(&PreviewKeyDownEvent() == &PreviewKeyDownEvent());
        CHECK(&MouseDownEvent()      == &MouseDownEvent());
        CHECK(&MouseWheelEvent()     == &MouseWheelEvent());
    }

    TEST_CASE("键盘事件传播策略正确") {
        using namespace mine::ui;
        CHECK(PreviewKeyDownEvent().strategy() == RoutingStrategy::Tunnel);
        CHECK(KeyDownEvent().strategy()        == RoutingStrategy::Bubble);
        CHECK(PreviewKeyUpEvent().strategy()   == RoutingStrategy::Tunnel);
        CHECK(KeyUpEvent().strategy()          == RoutingStrategy::Bubble);
    }

    TEST_CASE("鼠标事件传播策略正确") {
        using namespace mine::ui;
        CHECK(PreviewMouseDownEvent().strategy() == RoutingStrategy::Tunnel);
        CHECK(MouseDownEvent().strategy()        == RoutingStrategy::Bubble);
        CHECK(PreviewMouseUpEvent().strategy()   == RoutingStrategy::Tunnel);
        CHECK(MouseUpEvent().strategy()          == RoutingStrategy::Bubble);
        CHECK(MouseMoveEvent().strategy()        == RoutingStrategy::Bubble);
        CHECK(MouseWheelEvent().strategy()       == RoutingStrategy::Bubble);
    }

    TEST_CASE("事件名称正确") {
        CHECK(KeyDownEvent().name()        == mine::core::StringView{"KeyDown"});
        CHECK(MouseDownEvent().name()      == mine::core::StringView{"MouseDown"});
        CHECK(MouseWheelEvent().name()     == mine::core::StringView{"MouseWheel"});
    }
}

// ============================================================================
// InputRouter 测试
// ============================================================================

TEST_SUITE("InputRouter") {
    TEST_CASE("无根节点时不崩溃") {
        InputRouter router;
        // 无根节点，派发键盘事件不崩溃
        auto ev = make_key_event(WindowEventKind::KeyDown, 0x41);
        router.on_window_event(ev);
        // 无根节点，派发鼠标事件不崩溃
        auto me = make_mouse_event(WindowEventKind::MouseDown, 50.0f, 50.0f);
        router.on_window_event(me);
        // 无崩溃即通过
    }

    TEST_CASE("命中测试：命中元素接收 MouseDown 事件") {
        TestElement root;
        // 设置根节点边界，使命中测试能够匹配
        root.set_bounds_rect({ 0.0f, 0.0f, 200.0f, 200.0f });

        InputRouter router;
        router.set_root(&root);

        auto ev = make_mouse_event(WindowEventKind::MouseDown, 50.0f, 80.0f, 0);
        router.on_window_event(ev);

        CHECK(root.mouse_down_count == 1);
        CHECK(root.last_button == MouseButton::Left);
        CHECK(root.last_pos.x == doctest::Approx(50.0f));
        CHECK(root.last_pos.y == doctest::Approx(80.0f));
    }

    TEST_CASE("命中测试：坐标超出边界时不派发") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 100.0f, 100.0f });

        InputRouter router;
        router.set_root(&root);

        // 坐标 (200, 200) 超出 [0,100]x[0,100]
        auto ev = make_mouse_event(WindowEventKind::MouseDown, 200.0f, 200.0f);
        router.on_window_event(ev);

        CHECK(root.mouse_down_count == 0);
    }

    TEST_CASE("鼠标移动事件派发") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 300.0f, 300.0f });

        InputRouter router;
        router.set_root(&root);

        auto ev = make_mouse_event(WindowEventKind::MouseMove, 60.0f, 90.0f);
        router.on_window_event(ev);

        CHECK(root.mouse_move_count == 1);
        CHECK(root.last_pos.x == doctest::Approx(60.0f));
    }

    TEST_CASE("滚轮事件派发") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 300.0f, 300.0f });

        InputRouter router;
        router.set_root(&root);

        WindowEvent ev{};
        ev.kind              = WindowEventKind::MouseWheel;
        ev.mouse_x           = 50.0f;
        ev.mouse_y           = 50.0f;
        ev.mouse_wheel_delta = 120.0f;
        router.on_window_event(ev);

        CHECK(root.mouse_wheel_count == 1);
        CHECK(root.last_wheel_delta == doctest::Approx(120.0f));
    }

    TEST_CASE("键盘焦点元素接收 KeyDown 事件") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 300.0f, 300.0f });

        InputRouter router;
        router.set_root(&root);
        router.set_keyboard_focus(&root);

        auto ev = make_key_event(WindowEventKind::KeyDown, 0x41u); // VK 'A'
        router.on_window_event(ev);

        CHECK(root.key_down_count == 1);
        CHECK(root.last_key == Key::A);
    }

    TEST_CASE("无焦点时键盘事件退化到根节点") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 300.0f, 300.0f });

        InputRouter router;
        router.set_root(&root);
        // 不设置 keyboard_focus

        auto ev = make_key_event(WindowEventKind::KeyDown, 0x0Du); // Enter
        router.on_window_event(ev);

        CHECK(root.key_down_count == 1);
        CHECK(root.last_key == Key::Enter);
    }

    TEST_CASE("KeyUp 事件正确派发") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 300.0f, 300.0f });

        InputRouter router;
        router.set_root(&root);
        router.set_keyboard_focus(&root);

        auto ev = make_key_event(WindowEventKind::KeyUp, 0x41u);
        router.on_window_event(ev);

        CHECK(root.key_up_count == 1);
        CHECK(root.key_down_count == 0);
    }

    TEST_CASE("Preview KeyDown 被处理后正式 KeyDown 不派发") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 300.0f, 300.0f });

        InputRouter router;
        router.set_root(&root);
        router.set_keyboard_focus(&root);

        // 注册 PreviewKeyDown 处理器并标记 handled（非捕获 lambda + this 作为 ud）
        root.add_handler(PreviewKeyDownEvent(),
            [](void*, RoutedEventArgs& a, void*) {
                a.set_handled(true);
            }, nullptr);

        auto ev = make_key_event(WindowEventKind::KeyDown, 0x41u);
        router.on_window_event(ev);

        // KeyDown 处理器不应被调用
        CHECK(root.key_down_count == 0);
    }

    TEST_CASE("mouse_over_element 在命中后更新") {
        TestElement root;
        root.set_bounds_rect({ 0.0f, 0.0f, 200.0f, 200.0f });

        InputRouter router;
        router.set_root(&root);

        CHECK(router.mouse_over_element() == nullptr);

        auto ev = make_mouse_event(WindowEventKind::MouseMove, 50.0f, 50.0f);
        router.on_window_event(ev);

        CHECK(router.mouse_over_element() == &root);
    }
}
