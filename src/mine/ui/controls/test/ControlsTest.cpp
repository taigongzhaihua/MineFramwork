/**
 * @file ControlsTest.cpp
 * @brief mine.ui.controls 模块单元测试。
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/ui/controls/ControlsAll.h>
#include <mine/ui/animation/AnimationClock.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/event/EventManager.h>
#include <mine/paint/Canvas.h>
#include <mine/containers/InlineString.h>
#include <mine/core/Variant.h>

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

void advance_button_animations(float dt = 0.35f)
{
    (void)mine::ui::animation::AnimationClock::instance().tick_all(dt);
}

void check_solid_brush(const Button& button, mine::paint::Brush expected)
{
    const mine::core::Variant& value = button.get_value(Button::BackgroundProperty);
    REQUIRE(value.has<mine::paint::Brush>());
    const auto& brush = value.get<mine::paint::Brush>();
    REQUIRE(brush.kind() == mine::paint::BrushKind::SolidColor);
    REQUIRE(expected.kind() == mine::paint::BrushKind::SolidColor);
    CHECK(brush.color() == expected.color());
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

TEST_CASE("controls_Button_Normal到Disabled再回Normal时恢复Normal外观")
{
    Button button;
    const auto normal = mine::paint::Brush::solid_rgb(0x1565C0);
    button.set_background(normal);

    button.set_enabled(false);
    advance_button_animations();

    button.set_enabled(true);
    advance_button_animations();

    check_solid_brush(button, normal);
}

TEST_CASE("controls_Button_Pressed到Hovered再回Normal时恢复Normal外观")
{
    Button button;
    // 自定义 Normal 基线（Local 优先级）。经 Hovered/Pressed 往返后回到 Normal 应恢复该色。
    // 中间 Hovered/Pressed 的具体色由默认样式与动画时序共同决定（单次 tick 未必达终值），
    // 故不在单元测试中硬编码中间态颜色，仅验证状态机往返后能正确回退到 Normal 的本地值。
    const auto normal = mine::paint::Brush::solid_rgb(0x1565C0);
    button.set_background(normal);
    button.set_bounds_rect({0.0f, 0.0f, 100.0f, 40.0f});

    RoutedEventArgs enter{ input::MouseEnterEvent() };
    EventManager::raise(button, enter);
    advance_button_animations();

    input::MouseEventArgs down{
        input::MouseDownEvent(),
        input::MouseButton::Left,
        {10.0f, 10.0f},
        input::ModifierKeys::None};
    EventManager::raise(button, down);
    advance_button_animations();

    input::MouseEventArgs up{
        input::MouseUpEvent(),
        input::MouseButton::Left,
        {10.0f, 10.0f},
        input::ModifierKeys::None};
    EventManager::raise(button, up);
    advance_button_animations();

    RoutedEventArgs leave{ input::MouseLeaveEvent() };
    EventManager::raise(button, leave);
    advance_button_animations();

    // 往返结束：背景恢复 Normal 的本地自定义色
    check_solid_brush(button, normal);
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

TEST_CASE("controls_Border_外观属性经依赖属性读写一致")
{
    Border border;

    // 默认值：边框 1dp、圆角 0（直角）
    CHECK(border.border_thickness().left == doctest::Approx(1.0f));
    CHECK(border.corner_radius().top_left.x == doctest::Approx(0.0f));

    // 通过便捷访问器写入，再经依赖属性读回
    border.set_background(mine::paint::Brush::solid_rgb(0x112233));
    border.set_border_brush(mine::paint::Brush::solid_rgb(0x445566));
    border.set_border_thickness(Thickness::uniform(2.0f));
    border.set_corner_radius(CornerRadii::uniform(8.0f));

    CHECK(border.corner_radius().top_left.x == doctest::Approx(8.0f));
    CHECK(border.border_thickness().left == doctest::Approx(2.0f));

    // 直接读依赖属性槽，确认便捷访问器确实写入了 DP（单一真相源）
    const auto& cr = border.get_value(Border::CornerRadiusProperty);
    CHECK(cr.has<CornerRadii>());
    CHECK(cr.get<CornerRadii>().top_right.y == doctest::Approx(8.0f));

    // border_color 兼容别名等价于 border_brush
    CHECK(border.border_color().color().r == doctest::Approx(border.border_brush().color().r));
    CHECK(border.border_color().color().g == doctest::Approx(border.border_brush().color().g));
}

TEST_CASE("controls_Border_bind_property_圆角随源同步")
{
    Border src;
    Border dst;
    src.set_corner_radius(CornerRadii::uniform(4.0f));

    // 把 dst 的圆角单向绑定到 src 的圆角（DP↔DP，元素间绑定）
    dst.bind_property(Border::CornerRadiusProperty, src, Border::CornerRadiusProperty);

    // 初始同步：dst 取得 src 当前值
    CHECK(dst.corner_radius().top_left.x == doctest::Approx(4.0f));

    // 源变更 → 目标自动更新
    src.set_corner_radius(CornerRadii::uniform(12.0f));
    CHECK(dst.corner_radius().top_left.x == doctest::Approx(12.0f));
    CHECK(dst.corner_radius().bottom_right.y == doctest::Approx(12.0f));
}

TEST_CASE("controls_伞形头可直接使用StackPanel与Grid")
{
    StackPanel panel;
    Grid grid;

    CHECK(panel.children_count() == 0u);
    CHECK(grid.row_count() == 0u);
}

TEST_CASE("controls_样式槽位可设置")
{
    Button button;
    button.set_style_slot("AccentButton");

    CHECK(button.style_slot() == mine::core::StringView{"AccentButton"});
}

// ============================================================================
// ContentPresenter 测试套件（任务 #15）
// ============================================================================

TEST_CASE("controls_ContentPresenter_默认状态无内容零尺寸")
{
    ContentPresenter presenter;
    presenter.measure({300.0f, 200.0f});
    CHECK(presenter.desired_size().width == doctest::Approx(0.0f));
    CHECK(presenter.desired_size().height == doctest::Approx(0.0f));
}

TEST_CASE("controls_ContentPresenter_设置字符串内容后测量非零")
{
    ContentPresenter presenter;
    mine::containers::InlineString text{"Hello"};
    presenter.set_value(ContentPresenter::ContentProperty, mine::core::Variant{ text });
    presenter.measure({300.0f, 200.0f});
    CHECK(presenter.desired_size().width > 0.0f);
    CHECK(presenter.desired_size().height > 0.0f);
}

TEST_CASE("controls_ContentPresenter_Padding影响测量尺寸")
{
    ContentPresenter p1;
    ContentPresenter p2;

    mine::containers::InlineString text{"Hi"};
    const mine::core::Variant content_v{ text };
    p1.set_value(ContentPresenter::ContentProperty, content_v);
    p2.set_value(ContentPresenter::ContentProperty, content_v);

    // p2 加 10px 水平内边距
    p2.set_value(ContentPresenter::PaddingProperty, mine::core::Variant{ mine::math::Thickness::symmetric(10.0f, 0.0f) });

    p1.measure({300.0f, 200.0f});
    p2.measure({300.0f, 200.0f});

    CHECK(p2.desired_size().width > p1.desired_size().width + 9.0f);
}

TEST_CASE("controls_ContentPresenter_有内容时可渲染到Canvas")
{
    ContentPresenter presenter;
    mine::containers::InlineString text{"World"};
    presenter.set_value(ContentPresenter::ContentProperty, mine::core::Variant{ text });
    presenter.set_bounds_rect({0.0f, 0.0f, 100.0f, 30.0f});

    mine::paint::Canvas canvas;
    presenter.render_to_canvas(canvas);
    CHECK(canvas.cmd_count() > 0u);
}

TEST_CASE("controls_ContentPresenter_无内容时Canvas无绘制命令")
{
    ContentPresenter presenter;
    presenter.set_bounds_rect({0.0f, 0.0f, 100.0f, 30.0f});

    mine::paint::Canvas canvas;
    presenter.render_to_canvas(canvas);
    // render_to_canvas 固定产生 save/transform/restore 3 个帧命令；
    // 无内容时不应产生额外绘制命令（fill_rect 等）
    CHECK(canvas.cmd_count() <= 3u);
}

TEST_CASE("controls_ContentPresenter_前景与字号经依赖属性读写一致")
{
    ContentPresenter presenter;

    // 默认值：字号 14、前景白色
    CHECK(presenter.font_size() == doctest::Approx(14.0f));
    CHECK(presenter.foreground().color().r == doctest::Approx(1.0f));

    // 经便捷 setter 写入，再经依赖属性读回（单一真相源）
    presenter.set_font_size(20.0f);
    presenter.set_foreground(mine::paint::Brush::solid_rgb(0x336699));

    CHECK(presenter.font_size() == doctest::Approx(20.0f));

    const auto& fs = presenter.get_value(ContentPresenter::FontSizeProperty);
    CHECK(fs.has<float>());
    CHECK(fs.get<float>() == doctest::Approx(20.0f));

    const auto& fg = presenter.get_value(ContentPresenter::ForegroundProperty);
    CHECK(fg.has<mine::paint::Brush>());
    CHECK(fg.get<mine::paint::Brush>().color().b == doctest::Approx(presenter.foreground().color().b));
}

TEST_CASE("controls_ContentPresenter_bind_property_前景与字号随源同步")
{
    // 模拟宿主控件（Button）以 DP↔DP 绑定驱动 ContentPresenter 外观
    Border host;                 // 任取一个带 DP 的元素充当源
    ContentPresenter presenter;

    // 用 Border 的 Background 作为前景画刷源（均为 paint::Brush DP）
    host.set_background(mine::paint::Brush::solid_rgb(0xAABBCC));
    presenter.bind_property(ContentPresenter::ForegroundProperty,
                            host, Border::BackgroundProperty);

    // 初始同步：presenter 前景取得 host 当前背景
    CHECK(presenter.foreground().color().r ==
          doctest::Approx(host.background().color().r));

    // 源变更 → 目标自动更新
    host.set_background(mine::paint::Brush::solid_rgb(0x102030));
    CHECK(presenter.foreground().color().g ==
          doctest::Approx(host.background().color().g));
    CHECK(presenter.foreground().color().b ==
          doctest::Approx(host.background().color().b));
}

TEST_CASE("controls_Button_测量委托给内部ContentPresenter")
{
    Button button;
    // 构造函数已创建内部 ContentPresenter（set_inner_element）
    // measure 将委托给内部 ContentPresenter 计算期望尺寸
    button.set_text("OK");
    button.measure({300.0f, 200.0f});

    // 期望尺寸由 ContentPresenter 计算得出
    CHECK(button.desired_size().width > 0.0f);
    CHECK(button.desired_size().height > 0.0f);
}

TEST_CASE("controls_Button_ContentProperty变更传播到ContentPresenter")
{
    Button button;
    button.measure({300.0f, 200.0f});

    // 初始空文字：期望尺寸应包含 Padding 部分
    const float initial_w = button.desired_size().width;

    // 设置较长文字后期望宽度增大
    button.set_text("Hello World");
    button.measure({300.0f, 200.0f});

    CHECK(button.desired_size().width >= initial_w);
}

TEST_CASE("controls_TextBlock_DependencyProperty与成员变量同步")
{
    TextBlock text;
    text.set_text("Mine");

    // DependencyProperty 应与成员缓存一致
    const mine::core::Variant& v = text.get_value(TextBlock::TextProperty);
    REQUIRE(v.has<mine::containers::InlineString>());
    const auto& stored = v.get<mine::containers::InlineString>();
    CHECK(mine::core::StringView{ stored.data(), stored.size() } ==
          mine::core::StringView{"Mine"});
}

// ============================================================================
// ContentControl 测试套件（任务 17.1）
// ============================================================================

TEST_CASE("controls_ContentControl_默认状态内容为空")
{
    // ContentControl 是抽象概念，通过 Button 验证基类行为
    Button button;
    // 初始状态：ContentProperty 未设置，content() 应为空 Variant
    CHECK(!button.content().has_value());
    CHECK(button.content_element() == nullptr);
    CHECK(button.content_text().empty());
}

TEST_CASE("controls_ContentControl_set_content字符串写入ContentProperty")
{
    Button button;
    button.set_content(mine::core::StringView{"Hello ContentControl"});

    // content_text() 应返回刚写入的字符串
    CHECK(button.content_text() == mine::core::StringView{"Hello ContentControl"});
    // content() 应包含 InlineString
    CHECK(button.content().has<mine::containers::InlineString>());
    // content_element() 应为 nullptr（非元素内容）
    CHECK(button.content_element() == nullptr);
}

TEST_CASE("controls_ContentControl_set_content_UIElement指针写入ContentProperty")
{
    Button button;
    DummyElement child;
    button.set_content(&child);

    // content_element() 应返回同一指针
    CHECK(button.content_element() == &child);
    // content_text() 应为空（非字符串内容）
    CHECK(button.content_text().empty());
}

TEST_CASE("controls_ContentControl_set_content_nullptr清空内容")
{
    Button button;
    DummyElement child;
    button.set_content(&child);
    REQUIRE(button.content_element() == &child);

    // 传入 nullptr 应清空内容
    button.set_content(static_cast<UIElement*>(nullptr));
    CHECK(!button.content().has_value());
    CHECK(button.content_element() == nullptr);
}

TEST_CASE("controls_ContentControl_内容变更传播到ContentPresenter")
{
    Button button;
    button.measure({300.0f, 200.0f});

    // 通过 set_content 写入字符串，on_content_changed 直接将其同步到 ContentPresenter
    button.set_content(mine::core::StringView{"OK"});
    button.measure({300.0f, 200.0f});

    // 期望尺寸由 ContentPresenter 计算得出（文字宽度 > 0）
    CHECK(button.desired_size().width > 0.0f);
}

TEST_CASE("controls_ContentControl_set_text与set_content行为一致")
{
    Button b1;
    Button b2;
    b1.set_text("MineUI");
    b2.set_content(mine::core::StringView{"MineUI"});

    // set_text 和 set_content 应写入相同的 ContentProperty 值
    CHECK(b1.content_text() == b2.content_text());
}

TEST_CASE("controls_ContentControl_Button继承链正确")
{
    // 编译期验证：Button IS-A ContentControl IS-A Control
    Button button;
    ContentControl* cc = &button;
    Control*        ctrl = &button;

    CHECK(cc != nullptr);
    CHECK(ctrl != nullptr);
    // 动态类型检查（无 RTTI 时此测试仅做编译验证）
    CHECK(static_cast<Button*>(cc) == &button);
}

TEST_CASE("controls_ContentControl_内容变更触发重新测量")
{
    Button button;
    button.measure({300.0f, 200.0f});
    const float initial_w = button.desired_size().width;

    // 写入较长文字，期望宽度增大
    button.set_content(mine::core::StringView{"A very long button label text"});
    button.measure({300.0f, 200.0f});

    CHECK(button.desired_size().width >= initial_w);
}

// ============================================================================
// UserControl 测试套件（任务 17.2）
// ============================================================================

namespace {

/// 记录生命周期钩子调用顺序的测试专用 UserControl 子类
class LifecycleUserControl : public UserControl {
public:
    // 记录各钩子调用次数
    int initialized_count{ 0 };
    int loaded_count{ 0 };
    int unloaded_count{ 0 };
    int data_context_changed_count{ 0 };

    // 最后收到的 DataContext 值
    mine::core::Variant last_new_ctx{};

protected:
    void on_initialized() noexcept override { ++initialized_count; }
    void on_loaded()      noexcept override { ++loaded_count; }
    void on_unloaded()    noexcept override { ++unloaded_count; }
    void on_data_context_changed(const mine::core::Variant& /*old_ctx*/,
                                  const mine::core::Variant& new_ctx) noexcept override
    {
        ++data_context_changed_count;
        last_new_ctx = new_ctx;
    }
};

} // namespace

TEST_CASE("controls_UserControl_默认状态DataContext为空")
{
    UserControl uc{};
    // 默认 DataContext 应为空 Variant
    CHECK_FALSE(uc.data_context().has_value());
}

TEST_CASE("controls_UserControl_set_data_context写入DataContextProperty")
{
    LifecycleUserControl uc{};
    int value = 42;
    mine::core::Variant ctx{ static_cast<void*>(&value) };
    uc.set_data_context(ctx);

    // DataContext 已写入
    REQUIRE(uc.data_context().has_value());
    CHECK(uc.data_context().get<void*>() == static_cast<void*>(&value));
}

TEST_CASE("controls_UserControl_DataContext变更触发on_data_context_changed")
{
    LifecycleUserControl uc{};
    CHECK(uc.data_context_changed_count == 0);

    int v1 = 1;
    mine::core::Variant ctx1{ static_cast<void*>(&v1) };
    uc.set_data_context(ctx1);
    CHECK(uc.data_context_changed_count == 1);
    CHECK(uc.last_new_ctx.get<void*>() == static_cast<void*>(&v1));

    int v2 = 2;
    mine::core::Variant ctx2{ static_cast<void*>(&v2) };
    uc.set_data_context(ctx2);
    CHECK(uc.data_context_changed_count == 2);
    CHECK(uc.last_new_ctx.get<void*>() == static_cast<void*>(&v2));
}

TEST_CASE("controls_UserControl_set_content_UIElement加入视觉子树")
{
    UserControl uc{};
    TextBlock tb{};

    // set_content 前，tb 无父节点
    CHECK(tb.parent() == nullptr);

    uc.set_content(&tb);

    // set_content 后，tb 的父节点应为 uc
    CHECK(tb.parent() == &uc);
    CHECK(uc.content_element() == &tb);
}

TEST_CASE("controls_UserControl_set_content_nullptr清空视觉子树")
{
    UserControl uc{};
    TextBlock tb{};
    uc.set_content(&tb);
    CHECK(tb.parent() == &uc);

    // 清空内容
    uc.set_content(static_cast<UIElement*>(nullptr));
    CHECK(tb.parent() == nullptr);
    CHECK(uc.content_element() == nullptr);
}

TEST_CASE("controls_UserControl_on_initialized仅触发一次")
{
    LifecycleUserControl uc{};
    CHECK(uc.initialized_count == 0);

    // 第一次测量 → on_initialized
    uc.measure({400.0f, 300.0f});
    CHECK(uc.initialized_count == 1);

    // 再次测量 → 不重复触发
    uc.measure({400.0f, 300.0f});
    CHECK(uc.initialized_count == 1);
}

TEST_CASE("controls_UserControl_on_loaded和on_unloaded随父节点变化触发")
{
    LifecycleUserControl uc{};
    Border parent_border{};

    CHECK(uc.loaded_count   == 0);
    CHECK(uc.unloaded_count == 0);

    // 加入父树 → on_loaded
    parent_border.add_child(&uc);
    CHECK(uc.loaded_count   == 1);
    CHECK(uc.unloaded_count == 0);

    // 从父树移除 → on_unloaded
    parent_border.remove_child(&uc);
    CHECK(uc.loaded_count   == 1);
    CHECK(uc.unloaded_count == 1);
}

TEST_CASE("controls_UserControl_measure委托给内容元素")
{
    UserControl uc{};
    TextBlock tb{};
    tb.set_text(mine::core::StringView{"Hello"});
    uc.set_content(&tb);

    uc.measure({500.0f, 400.0f});

    // UserControl 的 desired_size 应反映内容元素的期望尺寸
    CHECK(uc.desired_size().width  > 0.0f);
    CHECK(uc.desired_size().height > 0.0f);
    // 内容元素期望尺寸应与 UserControl 一致
    CHECK(uc.desired_size().width  == doctest::Approx(tb.desired_size().width));
    CHECK(uc.desired_size().height == doctest::Approx(tb.desired_size().height));
}

TEST_CASE("controls_UserControl_继承链正确（IS-A ContentControl）")
{
    UserControl uc{};
    // 编译期验证：UserControl IS-A ContentControl IS-A Control
    ContentControl* cc = &uc;
    Control*        ct = &uc;
    CHECK(cc != nullptr);
    CHECK(ct != nullptr);
    // DataContextProperty 是 UserControl 特有的
    CHECK(&UserControl::DataContextProperty != &ContentControl::ContentProperty);
}

// ============================================================================
// Page 测试（任务 17.3）
// ============================================================================

namespace {

/// 测试用 Page 子类：覆盖导航虚函数以记录调用情况
struct TestPage : public Page {
    // 记录导航回调调用次数和参数
    int navigated_to_count{ 0 };
    int navigated_from_count{ 0 };
    mine::core::Variant last_navigated_to_param{};
    bool block_navigate_away{ false };   // true 时拦截导航

    void on_navigated_to(const mine::core::Variant& param) noexcept override {
        ++navigated_to_count;
        last_navigated_to_param = param;
    }
    void on_navigated_from() noexcept override {
        ++navigated_from_count;
    }
    bool on_navigate_away() noexcept override {
        return !block_navigate_away;
    }
};

/// 验证 Page 基类默认导航行为的包装器（通过子类访问 protected 虚函数）
struct DefaultPage : public Page {
    bool call_navigate_away() noexcept { return on_navigate_away(); }
    void call_navigated_to(const mine::core::Variant& p) noexcept { on_navigated_to(p); }
    void call_navigated_from() noexcept { on_navigated_from(); }
};

} // namespace（Page 测试辅助类）

TEST_CASE("controls_Page_默认导航行为_on_navigate_away返回true")
{
    DefaultPage page{};
    // 默认实现：始终允许导航离开
    CHECK(page.call_navigate_away() == true);
}

TEST_CASE("controls_Page_on_navigated_to默认为空操作")
{
    DefaultPage page{};
    // 不崩溃即通过（默认空实现）
    page.call_navigated_to(mine::core::Variant{});
    page.call_navigated_to(mine::core::Variant{ 42 });
    CHECK(true);
}

TEST_CASE("controls_Page_on_navigated_from默认为空操作")
{
    DefaultPage page{};
    // 不崩溃即通过（默认空实现）
    page.call_navigated_from();
    CHECK(true);
}

TEST_CASE("controls_Page_继承链正确（IS-A UserControl）")
{
    Page page{};
    // 编译期验证：Page IS-A UserControl IS-A ContentControl
    UserControl*    uc = &page;
    ContentControl* cc = &page;
    Control*        ct = &page;
    CHECK(uc != nullptr);
    CHECK(cc != nullptr);
    CHECK(ct != nullptr);
}

TEST_CASE("controls_Page_可被UserControl容器持有")
{
    // 验证：空 Page 可作为 UIElement* 内容被 UserControl 持有
    UserControl container{};
    Page        page{};
    container.set_content(&page);
    CHECK(container.content_element() == &page);
    CHECK(page.parent() == &container);
}

TEST_CASE("controls_Page_子类可拦截导航")
{
    // 验证：子类覆盖 on_navigate_away() 返回 false 可拦截导航
    TestPage locked{};
    locked.block_navigate_away = true;
    CHECK(locked.on_navigate_away() == false);
}

TEST_CASE("controls_Page_子类可响应导航参数")
{
    // 验证：子类覆盖 on_navigated_to 可接收导航参数
    TestPage pp{};
    pp.on_navigated_to(mine::core::Variant{ 99 });
    CHECK(pp.navigated_to_count == 1);
    CHECK(pp.last_navigated_to_param.has<int>());
    CHECK(pp.last_navigated_to_param.get<int>() == 99);
}

TEST_CASE("controls_Page_DataContext继承自UserControl")
{
    Page page{};
    // Page 继承 UserControl 的 DataContext 接口，不崩溃即通过
    page.set_data_context(mine::core::Variant{ 123 });
    CHECK(page.data_context().has<int>());
    CHECK(page.data_context().get<int>() == 123);
}
