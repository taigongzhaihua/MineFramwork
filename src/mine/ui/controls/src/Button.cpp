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
#include <mine/ui/property/ValuePriority.h>
#include <mine/ui/style/TemplateRegistry.h>
#include <mine/ui/animation/Storyboard.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/Duration.h>
#include <mine/core/Memory.h>

#include <cmath>

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

// Button::PaddingProperty — 内边距（Thickness，MD3 Filled Button 默认：水平 24px 垂直 10px）
const DependencyProperty& Button::PaddingProperty =
    register_property<Button>(
        "Padding",
        core::Variant{ math::Thickness::symmetric(24.0f, 10.0f) },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &Button::on_padding_changed,
        });

// Button::BackgroundProperty — 当前渲染背景色（由 Storyboard 在 Animation 优先级写入插值色）
// 默认值为 MD3 Primary（#6750A4），on_visual_state_changed 启动时先将当前色写入 Local 槽，
// 之后 Storyboard 在 Animation 槽（优先级 60）写入帧间插值，on_render 通过 get_value 读取。
const DependencyProperty& Button::BackgroundProperty =
    register_property<Button>(
        "Background",
        core::Variant{ math::Color::from_rgb_u32(0x6750A4) },  // MD3 Primary
        PropertyMetadata{
            .affects_render = true,  // 属性变更时自动触发 invalidate_render
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
    // 状态变化需重新测量以同步 ContentPresenter 前景色（Disabled/Enabled 颜色不同）
    invalidate_measure();
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
    // 若有进行中的背景过渡动画，先停止并释放，防止 Animation 槽(60) 遮盖 Local 槽(50)
    if (bg_storyboard_ && !bg_storyboard_->is_complete()) {
        bg_storyboard_->stop();
        bg_storyboard_ = nullptr;
    }
    // 将新颜色写入 BackgroundProperty Local 槽：on_render 通过 get_value 读取此属性，
    // affects_render=true → set_value 内部自动触发 invalidate_render，无需额外调用
    set_value(BackgroundProperty, core::Variant{color}, ValuePriority::Local);
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

void Button::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    // 若模板已构建，立即将字体传播到 ContentPresenter
    UIElement* child = find_template_child("content");
    if (child != nullptr) {
        auto* presenter = static_cast<ContentPresenter*>(child);
        presenter->set_font_face(font_face_);
        presenter->set_foreground(foreground_);
    }
}

void Button::set_font_size(float size_px) noexcept
{
    font_size_px_ = size_px;
    // 若模板已构建，立即将字号传播到 ContentPresenter
    UIElement* child = find_template_child("content");
    if (child != nullptr) {
        auto* presenter = static_cast<ContentPresenter*>(child);
        presenter->set_font_size(font_size_px_);
    }
}

void Button::on_measure(math::Size available_size)
{
    // 通过 Control::on_measure 自动构建模板（首次调用）并委托给模板根
    Control::on_measure(available_size);

    // 模板已构建，Control::on_measure 已采用模板根的期望尺寸
    if (template_root()) {
        // 每次 measure 时将字体属性同步到 ContentPresenter（幂等，确保动态设置生效）
        UIElement* child = find_template_child("content");
        if (child != nullptr && font_face_ != nullptr) {
            auto* presenter = static_cast<ContentPresenter*>(child);
            presenter->set_font_face(font_face_);
            // MD3 Disabled 状态：OnSurface 38% opacity（暗灰半透明文字）
            const math::Color effective_fg = is_enabled_
                ? foreground_
                : math::Color{0.11f, 0.11f, 0.12f, 0.38f};
            presenter->set_foreground(effective_fg);
            presenter->set_font_size(font_size_px_);
        }
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

    // Material Design 3 Filled Button：背景色从 BackgroundProperty DP 读取
    // 动画期间 Storyboard 写入 Animation 优先级（60）的插值色，get_value 自动返回最高优先级值
    math::Color fill;
    if (!is_enabled_) {
        // MD3 Disabled：OnSurface(#1C1B1F) at 12% opacity，不参与过渡
        fill = math::Color{0.11f, 0.11f, 0.12f, 0.12f};
    } else {
        const core::Variant& bg_var = get_value(BackgroundProperty);
        fill = bg_var.has<math::Color>() ? bg_var.get<math::Color>() : background_;
    }

    // Material Design 3 Filled Button：完全圆角（胶囊形，radius = height / 2）
    const float radius = rect.height * 0.5f;
    canvas.fill_rounded_rect(math::RoundedRect{rect, radius}, paint::Brush::solid(fill));

    // MD3 Ripple 涟漪动画：在背景之上、文字之下绘制涟漪圆
    if (ripple_.active) {
        using Clock = std::chrono::steady_clock;
        using Msf   = std::chrono::duration<float, std::milli>;
        const float elapsed_ms = std::chrono::duration_cast<Msf>(
            Clock::now() - ripple_.start).count();
        constexpr float kRippleDurationMs = 200.0f;  // MD3 Ripple 动画时长
        const float t = elapsed_ms / kRippleDurationMs;
        if (t >= 1.0f) {
            // 动画结束，关闭 ripple（下一帧 timer proc 会停止定时器）
            ripple_.active = false;
        } else {
            // 半径：从 0 扩展到按钮对角线的 60%（确保覆盖整个按钮）
            const float max_r = std::sqrt(rect.width  * rect.width
                                        + rect.height * rect.height) * 0.6f;
            const float ripple_r = max_r * t;
            // alpha：从 0.24 线性淡出到 0（MD3 Ripple 使用 On Primary 颜色）
            const float alpha = 0.24f * (1.0f - t);
            const math::Vec2 center{
                rect.x + ripple_.center_x,
                rect.y + ripple_.center_y,
            };
            // 裁剪到按钮胶囊边界，防止涟漪溢出
            canvas.save();
            canvas.clip_rounded_rect(math::RoundedRect{rect, radius});
            canvas.fill_circle(center, ripple_r,
                paint::Brush::solid(math::Color::White.with_alpha(alpha)));
            canvas.restore();
        }
    }

    // 有模板根时，文字由 ContentPresenter 渲染，Button 自身不再绘制占位线
    if (template_root()) {
        return;
    }

    // 无模板时的回退路径：用居中横条表示文字区域
    const float fallback_fg_a = is_enabled_ ? 1.0f : 0.38f;
    const math::Color fallback_fg = foreground_.with_alpha(fallback_fg_a);
    const float line_w = rect.width - padding_.horizontal();
    const float line_y = rect.y + rect.height * 0.5f - 1.0f;
    if (line_w > 0.0f) {
        canvas.fill_rect(
            { rect.x + padding_.left, line_y, line_w, 2.0f },
            paint::Brush::solid(fallback_fg));
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

    // 记录 MD3 Ripple 涟漪动画起始状态（圆心为鼠标按下点的局部坐标）
    const math::Rect rect = bounds_rect();
    ripple_.center_x = args.position().x - rect.x;
    ripple_.center_y = args.position().y - rect.y;
    ripple_.start    = std::chrono::steady_clock::now();
    ripple_.active   = true;

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

bool Button::has_active_animation() const noexcept
{
    // ripple 使用手动 chrono 计时；背景色过渡通过 Storyboard 驱动
    return ripple_.active || (bg_storyboard_ && !bg_storyboard_->is_complete());
}

void Button::tick_bg_animation(float dt)
{
    if (!bg_storyboard_ || bg_storyboard_->is_complete()) {
        return;
    }
    // tick 内部调用 set_value(BackgroundProperty, interpolated, Animation)
    // PropertyMetadata.affects_render = true → 触发 invalidate_render（由外部 timer 驱动刷帧）
    const bool done = bg_storyboard_->tick(dt);
    if (done) {
        // 动画完成：将最终插值色（Animation 槽当前值）持久化到 Local 槽，
        // 再调用 stop() 清除 Animation 槽（complete_ 不变，仍为 true）。
        // 这样后续 set_background() 写 Local(50) 能立即生效，不被残留 Animation(60) 遮盖。
        const core::Variant& final_val = get_value(BackgroundProperty);
        set_value(BackgroundProperty, final_val, ValuePriority::Local);
        bg_storyboard_->stop();
    }
}

void Button::on_visual_state_changed(ControlVisualState /*old_state*/,
                                     ControlVisualState new_state)
{
    // ── 1. 采样当前渲染色作为新动画的起始色 ────────────────────────────────
    // get_value 在 Storyboard 活跃时返回 Animation 优先级的插值色，
    // 确保中断旧过渡时从当前可见色开始，而非从目标色跳变。
    math::Color from_color = background_;  // 默认 fallback
    if (!is_enabled_) {
        from_color = math::Color{0.11f, 0.11f, 0.12f, 0.12f};
    } else {
        const core::Variant& cur = get_value(BackgroundProperty);
        if (cur.has<math::Color>()) {
            from_color = cur.get<math::Color>();
        }
    }

    // ── 2. 停止上一个 Storyboard（清除 Animation 槽）────────────────────────
    if (bg_storyboard_) {
        bg_storyboard_->stop();
    }

    // ── 3. 将采样色写入 Local 槽，使 capture_from_values 读到正确起始色 ────
    // （stop() 已清除 Animation 槽，若不写 Local 则 capture 会读到 Default 色）
    set_value(BackgroundProperty, core::Variant{from_color}, ValuePriority::Local);

    // ── 4. 计算目标颜色 ───────────────────────────────────────────────────
    math::Color to_color;
    switch (new_state) {
    case ControlVisualState::Pressed:  to_color = background_press_; break;
    case ControlVisualState::Hovered:  to_color = background_hover_; break;
    case ControlVisualState::Disabled: to_color = math::Color{0.11f, 0.11f, 0.12f, 0.12f}; break;
    default:                           to_color = background_;        break;
    }

    // ── 5. 构建新 Storyboard 并启动 ─────────────────────────────────────
    bg_storyboard_ = core::make_owned<animation::Storyboard>();
    bg_storyboard_->animate_dp_to(
        *this,
        BackgroundProperty,
        core::Variant{to_color},
        animation::Duration::milliseconds(100.0f),  // MD3 状态切换过渡时长
        animation::QuadEaseOut);                     // ease-out quad：末尾减速感更自然
    bg_storyboard_->capture_from_values();  // 读取 Local 槽中的 from_color
    bg_storyboard_->resolve_and_begin();    // 写入 Animation=from，启动插值
}

} // namespace mine::ui
