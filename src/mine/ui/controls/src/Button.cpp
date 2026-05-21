/**
 * @file Button.cpp
 * @brief Button 基础实现。
 */

#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/ContentPresenter.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/style/TemplateRegistry.h>
#include <mine/core/Memory.h>

namespace mine::ui {

// ============================================================================
// 依赖属性注册
// ============================================================================

// Button::ContentProperty — 按钮文字内容（InlineString）
const DependencyProperty& Button::ContentProperty =
    register_property<Button>(
        "Content",
        core::Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &Button::on_content_changed,
        });

// Button::PaddingProperty — 内边距（Thickness，默认水平 12px 垂直 8px）
const DependencyProperty& Button::PaddingProperty =
    register_property<Button>(
        "Padding",
        core::Variant{ math::Thickness::symmetric(12.0f, 8.0f) },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &Button::on_padding_changed,
        });

// ============================================================================
// 默认 ControlTemplate 构建函数
// ============================================================================

/**
 * @brief 默认按钮模板构建函数（无捕获，可作为函数指针传入 register_template）。
 *
 * 创建 ContentPresenter 作为模板根，并与 Button 的 Content/Padding 属性建立绑定。
 */
static void s_build_default_button_template(DependencyObject& target)
{
    auto& button    = static_cast<Button&>(target);
    auto  presenter = core::make_owned<ContentPresenter>();
    presenter->set_template_name("content");

    // 建立 TemplateBind：宿主属性 → 模板子元素属性
    button.bind_template(*presenter,
                         ContentPresenter::ContentProperty,
                         Button::ContentProperty);
    button.bind_template(*presenter,
                         ContentPresenter::PaddingProperty,
                         Button::PaddingProperty);

    // 将模板根加入视觉子树，并转移所有权给 Control（避免内存泄漏）
    button.set_template_root(std::move(presenter));
}

/**
 * @brief 注册 "DefaultButtonTemplate"（程序启动时静态初始化，保证仅注册一次）。
 */
static const style::ControlTemplate& s_default_button_template =
    style::TemplateRegistry::instance().register_template(
        "DefaultButtonTemplate",
        core::TypeId::of<Button>(),
        &s_build_default_button_template);

// ============================================================================
// 依赖属性变更回调
// ============================================================================

void Button::on_content_changed(DependencyObject*         sender,
                                const DependencyProperty& /*prop*/,
                                const core::Variant&      /*old_v*/,
                                const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<Button*>(sender);
    // 同步文字成员缓存
    if (new_v.has<containers::InlineString>()) {
        self->text_ = new_v.get<containers::InlineString>();
    } else {
        self->text_ = containers::InlineString{};
    }
}

void Button::on_padding_changed(DependencyObject*         sender,
                                const DependencyProperty& /*prop*/,
                                const core::Variant&      /*old_v*/,
                                const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<Button*>(sender);
    // 同步内边距成员缓存
    if (new_v.has<math::Thickness>()) {
        self->padding_ = new_v.get<math::Thickness>();
    }
}

// ============================================================================
// 路由事件注册
// ============================================================================

const RoutedEvent& Button::ClickEvent()
{
    static const RoutedEvent& ev = register_event<Button>("Click", RoutingStrategy::Bubble);
    return ev;
}

Button::Button()
{
    set_style_slot("DefaultButton");
    set_template_slot("DefaultButtonTemplate");

    add_handler(input::MouseDownEvent(), &Button::on_mouse_down_router, this);
    add_handler(input::MouseUpEvent(), &Button::on_mouse_up_router, this);
}

Button::~Button() = default;

core::StringView Button::text() const noexcept
{
    return core::StringView{ text_.data(), text_.size() };
}

void Button::set_text(core::StringView text)
{
    text_ = text;
    // 同步 DependencyProperty，使 bind_template 传播到模板树中的 ContentPresenter
    set_value(ContentProperty, core::Variant{ text_ });
    invalidate_measure();
    invalidate_render();
}

bool Button::is_enabled() const noexcept
{
    return is_enabled_;
}

void Button::set_enabled(bool enabled) noexcept
{
    is_enabled_ = enabled;
    if (!is_enabled_) {
        is_pressed_ = false;
    }
    update_visual_state();
    invalidate_render();
}

math::Thickness Button::padding() const noexcept
{
    return padding_;
}

void Button::set_padding(math::Thickness padding)
{
    padding_ = padding;
    // 同步 DependencyProperty，使 bind_template 传播到模板树中的 ContentPresenter
    set_value(PaddingProperty, core::Variant{ padding_ });
    invalidate_measure();
    invalidate_render();
}

math::Color Button::foreground() const noexcept
{
    return foreground_;
}

void Button::set_foreground(math::Color color)
{
    foreground_ = color;
    invalidate_render();
}

math::Color Button::background() const noexcept
{
    return background_;
}

void Button::set_background(math::Color color)
{
    background_ = color;
    invalidate_render();
}

math::Color Button::background_hovered() const noexcept
{
    return background_hover_;
}

void Button::set_background_hovered(math::Color color)
{
    background_hover_ = color;
    invalidate_render();
}

math::Color Button::background_pressed() const noexcept
{
    return background_press_;
}

void Button::set_background_pressed(math::Color color)
{
    background_press_ = color;
    invalidate_render();
}

math::Color Button::border_color() const noexcept
{
    return border_color_;
}

void Button::set_border_color(math::Color color)
{
    border_color_ = color;
    invalidate_render();
}

void Button::on_measure(math::Size available_size)
{
    // 通过 Control::on_measure 自动构建模板（首次调用）并委托给模板根
    Control::on_measure(available_size);

    // 模板已构建，Control::on_measure 已采用模板根的期望尺寸，直接返回
    if (template_root()) {
        return;
    }

    // 无模板时的回退路径：按字符数估算宽度
    const float text_w = static_cast<float>(text_.size()) * 14.0f * 0.55f;
    const float text_h = 14.0f * 1.4f;
    set_desired_size({
        text_w + padding_.horizontal(),
        text_h + padding_.vertical(),
    });
}

void Button::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    math::Color fill = background_;
    if (!is_enabled_) {
        fill = math::Color{0.85f, 0.85f, 0.85f, 1.0f};
    } else if (visual_state() == ControlVisualState::Pressed) {
        fill = background_press_;
    } else if (visual_state() == ControlVisualState::Hovered) {
        fill = background_hover_;
    }

    // 背景和边框无论是否有模板根都需要绘制（这是 Button 自身的视觉样式）
    canvas.fill_rect(rect, paint::Brush::solid(fill));
    canvas.stroke_bordered_rect(rect, paint::Brush::solid(border_color_), math::Thickness::uniform(1.0f));

    // 有模板根时，文字由 ContentPresenter 渲染，Button 自身不再绘制文字占位线
    if (template_root()) {
        return;
    }

    // 无模板时的回退路径：用居中横条表示文字区域
    const float line_w = rect.width - padding_.horizontal();
    const float line_y = rect.y + rect.height * 0.5f - 1.0f;
    if (line_w > 0.0f) {
        canvas.fill_rect(
            { rect.x + padding_.left, line_y, line_w, 2.0f },
            paint::Brush::solid(foreground_));
    }
}

ControlVisualState Button::compute_visual_state() const
{
    if (!is_enabled_) {
        return ControlVisualState::Disabled;
    }
    if (is_pressed_) {
        return ControlVisualState::Pressed;
    }
    return ControlVisualState::Normal;
}

void Button::on_mouse_down_router(void* /*sender*/, RoutedEventArgs& args, void* user_data)
{
    auto* self = static_cast<Button*>(user_data);
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    self->on_mouse_down(mouse_args);
}

void Button::on_mouse_up_router(void* /*sender*/, RoutedEventArgs& args, void* user_data)
{
    auto* self = static_cast<Button*>(user_data);
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    self->on_mouse_up(mouse_args);
}

void Button::on_mouse_down(input::MouseEventArgs& args)
{
    if (!is_enabled_ || args.button() != input::MouseButton::Left) {
        return;
    }
    is_pressed_ = true;
    update_visual_state();
}

void Button::on_mouse_up(input::MouseEventArgs& args)
{
    if (!is_enabled_ || args.button() != input::MouseButton::Left) {
        return;
    }

    const bool should_click = is_pressed_;
    is_pressed_ = false;
    update_visual_state();

    if (should_click) {
        raise_click();
    }
}

void Button::raise_click()
{
    RoutedEventArgs args{ ClickEvent() };
    args.set_original_source(this);
    args.set_source(this);
    EventManager::raise(*this, args);
}

} // namespace mine::ui
