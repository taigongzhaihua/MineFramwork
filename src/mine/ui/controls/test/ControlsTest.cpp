/**
 * @file ControlsTest.cpp
 * @brief mine.ui.controls 模块单元测试。
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/controls/ControlsAll.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/event/EventManager.h>
#include <mine/paint/Canvas.h>

using namespace mine::ui;
using namespace mine::math;

namespace {

class DummyElement : public UIElement {
protected:
    void on_measure(Size /*available_size*/) override {
        set_desired_size({40.0f, 20.0f});
    }
};

void on_click_counter(void* /*sender*/, RoutedEventArgs& /*args*/, void* user_data)
{
    auto* counter = static_cast<int*>(user_data);
    ++(*counter);
}

} // namespace

TEST_CASE("controls_TextBlock_设置文本后可测量")
{
    TextBlock text;
    text.set_text("MineUI");
    text.set_font_size(16.0f);

    text.measure({500.0f, 200.0f});
    CHECK(text.desired_size().width > 0.0f);
    CHECK(text.desired_size().height > 0.0f);
}

TEST_CASE("controls_TextBlock_可渲染到Canvas")
{
    TextBlock text;
    text.set_text("Hello");
    text.set_bounds_rect({0.0f, 0.0f, 120.0f, 30.0f});

    mine::paint::Canvas canvas;
    text.render_to_canvas(canvas);
    CHECK(canvas.cmd_count() > 0u);
}

TEST_CASE("controls_Button_鼠标按下抬起触发Click")
{
    Button button;
    button.set_bounds_rect({0.0f, 0.0f, 100.0f, 40.0f});

    int click_count = 0;
    button.add_handler(Button::ClickEvent(), &on_click_counter, &click_count);

    input::MouseEventArgs down{
        input::MouseDownEvent(),
        input::MouseButton::Left,
        {10.0f, 10.0f},
        input::ModifierKeys::None};
    EventManager::raise(button, down);

    input::MouseEventArgs up{
        input::MouseUpEvent(),
        input::MouseButton::Left,
        {10.0f, 10.0f},
        input::ModifierKeys::None};
    EventManager::raise(button, up);

    CHECK(click_count == 1);
}

TEST_CASE("controls_Button_禁用后不触发Click")
{
    Button button;
    button.set_enabled(false);

    int click_count = 0;
    button.add_handler(Button::ClickEvent(), &on_click_counter, &click_count);

    input::MouseEventArgs down{
        input::MouseDownEvent(),
        input::MouseButton::Left,
        {10.0f, 10.0f},
        input::ModifierKeys::None};
    EventManager::raise(button, down);

    input::MouseEventArgs up{
        input::MouseUpEvent(),
        input::MouseButton::Left,
        {10.0f, 10.0f},
        input::ModifierKeys::None};
    EventManager::raise(button, up);

    CHECK(click_count == 0);
}

TEST_CASE("controls_Border_测量包含子元素与边框")
{
    Border border;
    DummyElement child;
    border.set_child(&child);
    border.set_border_thickness(Thickness::uniform(2.0f));

    border.measure({300.0f, 200.0f});

    CHECK(border.desired_size().width == doctest::Approx(44.0f));
    CHECK(border.desired_size().height == doctest::Approx(24.0f));
}

TEST_CASE("controls_Border_Arrange会排列子元素")
{
    Border border;
    DummyElement child;
    border.set_child(&child);
    border.set_border_thickness(Thickness::symmetric(3.0f, 4.0f));

    border.set_bounds_rect({0.0f, 0.0f, 100.0f, 60.0f});

    const Rect slot = child.bounds_rect();
    CHECK(slot.x == doctest::Approx(3.0f));
    CHECK(slot.y == doctest::Approx(4.0f));
    CHECK(slot.width == doctest::Approx(94.0f));
    CHECK(slot.height == doctest::Approx(52.0f));
}

TEST_CASE("controls_伞形头可直接使用StackPanel与Grid")
{
    StackPanel panel;
    Grid grid;

    CHECK(panel.children_count() == 0u);
    CHECK(grid.row_count() == 0u);
}

TEST_CASE("controls_样式模板槽位可设置")
{
    Button button;
    button.set_style_slot("AccentButton");
    button.set_template_slot("AccentButtonTemplate");

    CHECK(button.style_slot() == mine::core::StringView{"AccentButton"});
    CHECK(button.template_slot() == mine::core::StringView{"AccentButtonTemplate"});
}
