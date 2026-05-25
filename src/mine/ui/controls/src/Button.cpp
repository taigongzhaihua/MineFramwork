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
#include <mine/ui/animation/AnimationClock.h>
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

// Button::ForegroundProperty — 文字前景色（MD3 On Primary 白色）
// 变更时通过 on_foreground_changed 自动传播到 ContentPresenter
const DependencyProperty& Button::ForegroundProperty =
    register_property<Button>(
        "Foreground",
        core::Variant{ math::Color::White },  // MD3 On Primary
        PropertyMetadata{
            .affects_render = true,
            .changed        = &Button::on_foreground_changed,
        });

// Button::BorderColorProperty — 边框颜色（MD3 Filled Button 无外边框，默认透明）
const DependencyProperty& Button::BorderColorProperty =
    register_property<Button>(
        "BorderColor",
        core::Variant{ math::Color::Transparent },
        PropertyMetadata{
            .affects_render = true,
        });

// Button::HoveredBackgroundProperty — Hovered 状态目标背景色
// MD3 Primary + OnPrimary * 8%（hover state layer = 8% of OnPrimary）
const DependencyProperty& Button::HoveredBackgroundProperty =
    register_property<Button>(
        "HoveredBackground",
        core::Variant{ math::Color{0.452f, 0.369f, 0.672f, 1.0f} },  // ≈ #735BAC
        PropertyMetadata{
            .affects_render = true,
        });

// Button::PressedBackgroundProperty — Pressed 状态目标背景色
// MD3 Primary + OnPrimary * 12%（pressed state layer = 12% of OnPrimary）
const DependencyProperty& Button::PressedBackgroundProperty =
    register_property<Button>(
        "PressedBackground",
        core::Variant{ math::Color{0.476f, 0.396f, 0.686f, 1.0f} },  // ≈ #7A65AF
        PropertyMetadata{
            .affects_render = true,
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

void Button::on_foreground_changed(DependencyObject*         sender,
                                   const DependencyProperty& /*prop*/,
                                   const core::Variant&      /*old_v*/,
                                   const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<Button*>(sender);
    // 若模板已构建，将新前景色推送到 ContentPresenter（幂等，template_root 为 nullptr 则跳过）
    UIElement* child = self->find_template_child("content");
    if (child != nullptr && new_v.has<math::Color>()) {
        static_cast<ContentPresenter*>(child)->set_foreground(new_v.get<math::Color>());
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
    add_handler(input::MouseEnterEvent(), &Button::on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &Button::on_mouse_leave_router, this);
}

Button::~Button()
{
    // 析构时注销 AnimationClock 注册项，防止后续 tick_all 回调访问已释放内存
    animation::AnimationClock::instance().unregister_animation(this);
}

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
        is_hovered_ = false;
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
    const core::Variant& v = get_value(ForegroundProperty);
    return v.has<math::Color>() ? v.get<math::Color>() : math::Color::White;
}

void Button::set_foreground(math::Color color)
{
    // 写入 ForegroundProperty Local 槽；on_foreground_changed 回调负责传播到 ContentPresenter
    // affects_render=true → set_value 内部自动触发 invalidate_render
    set_value(ForegroundProperty, core::Variant{color}, ValuePriority::Local);
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
    const core::Variant& v = get_value(HoveredBackgroundProperty);
    return v.has<math::Color>() ? v.get<math::Color>() : math::Color{};
}

void Button::set_background_hovered(math::Color color)
{
    // 写入 HoveredBackgroundProperty Local 槽；affects_render=true → 自动触发 invalidate_render
    set_value(HoveredBackgroundProperty, core::Variant{color}, ValuePriority::Local);
}

math::Color Button::background_pressed() const noexcept
{
    const core::Variant& v = get_value(PressedBackgroundProperty);
    return v.has<math::Color>() ? v.get<math::Color>() : math::Color{};
}

void Button::set_background_pressed(math::Color color)
{
    // 写入 PressedBackgroundProperty Local 槽；affects_render=true → 自动触发 invalidate_render
    set_value(PressedBackgroundProperty, core::Variant{color}, ValuePriority::Local);
}

math::Color Button::border_color() const noexcept
{
    const core::Variant& v = get_value(BorderColorProperty);
    return v.has<math::Color>() ? v.get<math::Color>() : math::Color::Transparent;
}

void Button::set_border_color(math::Color color)
{
    // 写入 BorderColorProperty Local 槽；affects_render=true → 自动触发 invalidate_render
    set_value(BorderColorProperty, core::Variant{color}, ValuePriority::Local);
}

void Button::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    // 若模板已构建，立即将字体与当前前景色传播到 ContentPresenter
    UIElement* child = find_template_child("content");
    if (child != nullptr) {
        auto* presenter = static_cast<ContentPresenter*>(child);
        presenter->set_font_face(font_face_);
        // 从 ForegroundProperty（唯一真相源）读取当前前景色并同步
        const core::Variant& fg_var = get_value(ForegroundProperty);
        if (fg_var.has<math::Color>()) {
            presenter->set_foreground(fg_var.get<math::Color>());
        }
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
            // 从 ForegroundProperty（唯一真相源）读取前景色
            const core::Variant& fg_var = get_value(ForegroundProperty);
            const math::Color base_fg = fg_var.has<math::Color>()
                ? fg_var.get<math::Color>() : math::Color::White;
            // MD3 Disabled 状态：OnSurface 38% opacity（暗灰半透明文字）
            const math::Color effective_fg = is_enabled_
                ? base_fg
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
    // elapsed_ms 由 AnimationClock 驱动的 anim_tick_callback 每帧累加
    if (ripple_.active) {
        constexpr float kRippleDurationMs = 200.0f;  // MD3 Ripple 动画时长
        const float t = ripple_.elapsed_ms / kRippleDurationMs;
        if (t < 1.0f) {
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
        // 若 t >= 1 时 ripple_.active 已被 tick 设为 false，此处无需额外处理
    }

    // 有模板根时，文字由 ContentPresenter 渲染，Button 自身不再绘制占位线
    if (template_root()) {
        return;
    }

    // 无模板时的回退路径：用居中横条表示文字区域
    const core::Variant& fg_var = get_value(ForegroundProperty);
    const math::Color base_fg  = fg_var.has<math::Color>() ? fg_var.get<math::Color>() : math::Color::White;
    const float fallback_fg_a = is_enabled_ ? 1.0f : 0.38f;
    const math::Color fallback_fg = base_fg.with_alpha(fallback_fg_a);
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
    if (is_hovered_) {
        return ControlVisualState::Hovered;
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

void Button::on_mouse_enter_router(void* /*sender*/, RoutedEventArgs& /*args*/, void* user_data)
{
    static_cast<Button*>(user_data)->on_mouse_enter();
}

void Button::on_mouse_leave_router(void* /*sender*/, RoutedEventArgs& /*args*/, void* user_data)
{
    static_cast<Button*>(user_data)->on_mouse_leave();
}

void Button::on_mouse_enter()
{
    if (!is_enabled_) {
        return;
    }
    is_hovered_ = true;
    update_visual_state();
}

void Button::on_mouse_leave()
{
    is_hovered_ = false;
    is_pressed_ = false;  // 鼠标离开时同时解除按下态（防止跨元素 press 残留）
    update_visual_state();
}

void Button::on_mouse_down(input::MouseEventArgs& args)
{
    if (!is_enabled_ || args.button() != input::MouseButton::Left) {
        return;
    }
    is_pressed_ = true;

    // 记录 MD3 Ripple 涟漪动画起始状态（圆心为鼠标按下点的局部坐标）
    const math::Rect rect = bounds_rect();
    ripple_.center_x   = args.position().x - rect.x;
    ripple_.center_y   = args.position().y - rect.y;
    ripple_.elapsed_ms = 0.0f;  // 由 AnimationClock 驱动的 tick 每帧累加
    ripple_.active     = true;
    // 注册到 AnimationClock（幂等：若 Storyboard 已注册则只刷新 tick_fn）
    animation::AnimationClock::instance().register_animation(this, &Button::anim_tick_callback);

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

// ============================================================================
// AnimationClock tick 回调（由 AnimationClock::tick_all 每帧调用）
// ============================================================================

bool Button::anim_tick_callback(void* user_data, float dt) noexcept
{
    auto* self = static_cast<Button*>(user_data);
    bool  any_active = false;

    // ── Ripple 涟漪动画 ─────────────────────────────────────────────────────
    if (self->ripple_.active) {
        constexpr float kRippleDurationMs = 200.0f;
        self->ripple_.elapsed_ms += dt * 1000.0f;
        if (self->ripple_.elapsed_ms < kRippleDurationMs) {
            any_active = true;
        } else {
            self->ripple_.active = false;  // 动画完成，on_render 不再绘制涟漪
        }
        // 触发重绘以反映当前 elapsed_ms 对应的涟漪半径/透明度
        self->invalidate_render();
    }

    // ── 背景色过渡 Storyboard ────────────────────────────────────────────────
    if (self->bg_storyboard_ && !self->bg_storyboard_->is_complete()) {
        // tick 内部将插值色以 Animation 优先级写回 BackgroundProperty，
        // affects_render=true → 自动触发 invalidate_render
        const bool done = self->bg_storyboard_->tick(dt);
        if (done) {
            // 动画完成：将 Animation 槽的终值持久化到 Local 槽，
            // 再 stop() 清除 Animation 槽，防止残留 Animation(60) 遮盖后续 set_background()
            const core::Variant& final_val = self->get_value(BackgroundProperty);
            self->set_value(BackgroundProperty, final_val, ValuePriority::Local);
            self->bg_storyboard_->stop();
        } else {
            any_active = true;
        }
    }

    // 返回 false → AnimationClock 自动移除此注册项（无需手动注销）
    return any_active;
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

    // ── 4. 从对应 DP 读取目标颜色（唯一真相源）────────────────────────────
    // HoveredBackgroundProperty / PressedBackgroundProperty 已通过
    // set_background_hovered/pressed() 写入 Local 槽，默认为 MD3 规范值。
    math::Color to_color;
    switch (new_state) {
    case ControlVisualState::Pressed: {
        const core::Variant& v = get_value(PressedBackgroundProperty);
        to_color = v.has<math::Color>() ? v.get<math::Color>() : background_;
        break;
    }
    case ControlVisualState::Hovered: {
        const core::Variant& v = get_value(HoveredBackgroundProperty);
        to_color = v.has<math::Color>() ? v.get<math::Color>() : background_;
        break;
    }
    case ControlVisualState::Disabled:
        to_color = math::Color{0.11f, 0.11f, 0.12f, 0.12f};
        break;
    default:
        to_color = background_;
        break;
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

    // ── 6. 向 AnimationClock 注册 tick 回调（幂等，不会产生重复项）────────
    // AnimationClock 统一驱动 Ripple + Storyboard 两种动画，
    // 取代了原来分散在 App 层的 tick_bg_animation() 调用
    animation::AnimationClock::instance().register_animation(this, &Button::anim_tick_callback);
}

} // namespace mine::ui
