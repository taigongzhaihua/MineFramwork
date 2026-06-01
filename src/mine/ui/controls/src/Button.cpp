/**
 * @file Button.cpp
 * @brief Button 基础实现。
 */

#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/ContentPresenter.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/math/RoundedRect.h>
#include <mine/math/Color.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/ICommand.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/property/ValuePriority.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/style/StyleSetter.h>
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
// 注意：Button::ContentProperty 已上移至 ContentControl，此处仅注册 Button 自身的属性。

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

// Button::BackgroundProperty — 背景画刷（MD3 Primary #6750A4；样式层会在 P5 覆写基线值）
const DependencyProperty& Button::BackgroundProperty =
    register_property<Button>(
        "Background",
        core::Variant{ paint::Brush::solid_rgb(0x6750A4) },  // Default(P0)
        PropertyMetadata{
            .affects_render = true,
        });

// Button::ForegroundProperty — 文字前景画刷（MD3 On Primary 白色）
// 变更时通过 on_foreground_changed 自动传播到 ContentPresenter
const DependencyProperty& Button::ForegroundProperty =
    register_property<Button>(
        "Foreground",
        core::Variant{ paint::Brush::solid(math::Color::White) },  // MD3 On Primary
        PropertyMetadata{
            .affects_render = true,
            .changed        = &Button::on_foreground_changed,
        });

// Button::BorderColorProperty — 边框画刷（MD3 Filled Button 无外边框，默认透明）
const DependencyProperty& Button::BorderColorProperty =
    register_property<Button>(
        "BorderColor",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
        PropertyMetadata{
            .affects_render = true,
        });

// Button::CommandProperty — 命令（Variant 存储 ICommand*，默认为空）
// 属性变更时通过 on_command_changed 自动管理 can_execute_changed 订阅并刷新 is_enabled_。
const DependencyProperty& Button::CommandProperty =
    register_property<Button>(
        "Command",
        core::Variant{},
        PropertyMetadata{
            .changed = &Button::on_command_changed,
        });

// Button::CommandParameterProperty — 命令参数（Variant，默认为空）
// 传递给 ICommand::execute() 和 ICommand::can_execute()。
const DependencyProperty& Button::CommandParameterProperty =
    register_property<Button>(
        "CommandParameter",
        core::Variant{},
        PropertyMetadata{});

// ============================================================================
// 默认样式（第三层：样式层）
// ============================================================================

/**
 * @brief 构建默认 Button 样式对象（Construct On First Use，避免静态初始化顺序问题）。
 *
 * 层 P5 StyleSetter：Normal 状态的基线外观值（背景色、前景色、边框色）。
 * 层 P4 StyleTrigger：Hovered/Pressed/Disabled 状态的终值（由 VSM go_to_state 自动写入）。
 *
 * 注意：状态 setter 只写终值，不描述动画曲线（动画曲线在模板的 VSM 过渡中配置）。
 */
static style::Style& default_button_style()
{
    static style::Style s = [] {
        using namespace mine::paint;
        using namespace mine::math;

        style::Style s;
        s.set_target_type(core::TypeId::of<Button>());
        s.set_name("DefaultButton");

        // ── P5 StyleSetter：Normal 基线外观 ──────────────────────────────────
        s.add_setter({ &Button::BackgroundProperty,
                       core::Variant{ Brush::solid_rgb(0x6750A4) } });          // MD3 Primary
        s.add_setter({ &Button::ForegroundProperty,
                       core::Variant{ Brush::solid(Color::White) } });           // MD3 On Primary
        s.add_setter({ &Button::BorderColorProperty,
                       core::Variant{ Brush::solid(Color::Transparent) } });

        // ── P4 StyleTrigger：Hovered 状态终值 ───────────────────────────────────
        style::VisualStateSetters hovered;
        hovered.state_name = "Hovered";
        hovered.setters.push_back({ &Button::BackgroundProperty,
            core::Variant{ Brush::solid_rgb(0x8876C0) } });  // 悬停：较 Normal 亮约 20%，明显变亮
        s.add_state_setters(std::move(hovered));

        // ── P4 StyleTrigger：Pressed 状态终值 ───────────────────────────────────
        style::VisualStateSetters pressed;
        pressed.state_name = "Pressed";
        pressed.setters.push_back({ &Button::BackgroundProperty,
            core::Variant{ Brush::solid_rgb(0x5743A0) } });  // 按下：较 Normal 暗约 15%，明显变暗
        s.add_state_setters(std::move(pressed));

        // ── P4 StyleTrigger：Disabled 状态终值（无动画——直接跳变） ─────────────────
        style::VisualStateSetters disabled;
        disabled.state_name = "Disabled";
        disabled.setters.push_back({ &Button::BackgroundProperty,
            core::Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.12f}) } });  // OnSurface 12%
        disabled.setters.push_back({ &Button::ForegroundProperty,
            core::Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.38f}) } });  // OnSurface 38%
        s.add_state_setters(std::move(disabled));

        return s;
    }();
    return s;
}

// ============================================================================
// 依赖属性变更回调
// ============================================================================

void Button::on_content_changed(const core::Variant& /*old_v*/,
                                const core::Variant& new_v) noexcept
{
    // 同步文字成员缓存
    if (new_v.has<containers::InlineString>()) {
        text_ = new_v.get<containers::InlineString>();
    } else {
        text_ = containers::InlineString{};
    }
    // 直接同步到 ContentPresenter（无 bind_template 机制）
    if (content_part_ != nullptr) {
        content_part_->set_value(ContentPresenter::ContentProperty, new_v);
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
    // 直接同步到 ContentPresenter（无 bind_template 机制）
    if (self->content_part_ != nullptr) {
        self->content_part_->set_value(ContentPresenter::PaddingProperty, new_v);
    }
}

void Button::on_foreground_changed(DependencyObject*         sender,
                                   const DependencyProperty& /*prop*/,
                                   const core::Variant&      /*old_v*/,
                                   const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<Button*>(sender);
    // 直接推送到 ContentPresenter（无 find_template_child 间接层）
    if (self->content_part_ != nullptr && new_v.has<paint::Brush>()) {
        self->content_part_->set_foreground(new_v.get<paint::Brush>());
    }
}

void Button::on_command_changed(DependencyObject*         sender,
                                const DependencyProperty& /*prop*/,
                                const core::Variant&      old_v,
                                const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<Button*>(sender);

    // 取消旧命令的 can_execute_changed 订阅
    if (self->cmd_token_ != 0 && old_v.has<ICommand*>()) {
        ICommand* old_cmd = old_v.get<ICommand*>();
        if (old_cmd != nullptr) {
            old_cmd->unsubscribe_can_execute_changed(self->cmd_token_);
        }
        self->cmd_token_ = 0;
    }

    // 订阅新命令并立即刷新启用状态
    if (new_v.has<ICommand*>()) {
        ICommand* new_cmd = new_v.get<ICommand*>();
        if (new_cmd != nullptr) {
            // 订阅 can_execute_changed，持有 token 供后续取消
            self->cmd_token_ = new_cmd->subscribe_can_execute_changed(
                &Button::on_can_execute_changed, self);
            // 立即按 can_execute() 结果刷新按钮启用状态
            const core::Variant& param = self->get_value(Button::CommandParameterProperty);
            self->set_enabled(new_cmd->can_execute(param));
            return;
        }
    }
    // 命令置空时恢复启用状态
    self->set_enabled(true);
}

void Button::on_can_execute_changed(ICommand* sender, void* user_data) noexcept
{
    auto* self = static_cast<Button*>(user_data);
    const core::Variant& param = self->get_value(Button::CommandParameterProperty);
    // 重新查询 can_execute() 并同步按钮启用状态
    self->set_enabled(sender->can_execute(param));
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
    // 创建并直接安装 ContentPresenter 作为内部子元素（无 ControlTemplate 机制）
    auto presenter = core::make_owned<ContentPresenter>();
    content_part_ = presenter.get();

    // 配置 VisualStateManager（内联原 BuildFn 中的 VSM 配置）
    style::VisualStateManager vsm{ *this };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Disabled");

    auto* btn_ptr = this;
    vsm.add_transition("*", "Hovered",
        [btn_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*btn_ptr, Button::BackgroundProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Normal",
        [btn_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*btn_ptr, Button::BackgroundProperty,
                          animation::Duration::milliseconds(100.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Pressed",
        [btn_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*btn_ptr, Button::BackgroundProperty,
                          animation::Duration::milliseconds(60.0f),
                          animation::QuadEaseIn);
        });

    // 连接样式层（若用户已通过 set_vsm_style 指定自定义样式则使用该样式）
    style::Style& active_style = vsm_style_ ? *vsm_style_ : default_button_style();
    vsm.set_style(&active_style);
    set_visual_state_manager(std::move(vsm));

    // 应用 P5 基线值（StyleSetter：Normal 背景色、前景色、边框色）
    active_style.apply(*this);

    // 同步 Padding 默认值到 ContentPresenter（Content 默认为空，无需同步）
    content_part_->set_value(ContentPresenter::PaddingProperty, get_value(PaddingProperty));

    // 初始同步前景色到 ContentPresenter
    {
        const core::Variant& fg_var = get_value(ForegroundProperty);
        if (fg_var.has<paint::Brush>()) {
            content_part_->set_foreground(fg_var.get<paint::Brush>());
        }
    }

    // 安装 ContentPresenter 到视觉子树（由 Control::set_inner_element 管理生命周期）
    set_inner_element(std::move(presenter));

    // 注册鼠标事件处理器
    add_handler(input::MouseDownEvent(), &Button::on_mouse_down_router, this);
    add_handler(input::MouseUpEvent(), &Button::on_mouse_up_router, this);
    add_handler(input::MouseEnterEvent(), &Button::on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &Button::on_mouse_leave_router, this);

    // 同步到初始视觉状态（Normal）
    update_visual_state();
}

Button::~Button()
{
    // 析构时取消命令 can_execute_changed 订阅，防止 ICommand 回调访问已释放的 Button
    if (cmd_token_ != 0) {
        const core::Variant& cmd_var = get_value(CommandProperty);
        if (cmd_var.has<ICommand*>()) {
            ICommand* cmd = cmd_var.get<ICommand*>();
            if (cmd != nullptr) {
                cmd->unsubscribe_can_execute_changed(cmd_token_);
            }
        }
        cmd_token_ = 0;
    }
    // 注销 AnimationClock 注册项，防止后续 tick_all 回调访问已释放内存
    animation::AnimationClock::instance().unregister_animation(this);
}

core::StringView Button::text() const noexcept
{
    return core::StringView{ text_.data(), text_.size() };
}

void Button::set_text(core::StringView text)
{
    // 委托给 ContentControl::set_content：on_content_changed 自动同步 text_ 缓存
    // affects_measure/affects_render=true 自动触发 invalidate_measure/invalidate_render
    set_content(text);
}

bool Button::is_enabled() const noexcept
{
    return is_enabled_;
}

ICommand* Button::command() const noexcept
{
    const core::Variant& v = get_value(CommandProperty);
    return v.has<ICommand*>() ? v.get<ICommand*>() : nullptr;
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
    // 同步 DependencyProperty（on_padding_changed 回调将自动传播到 ContentPresenter）
    set_value(PaddingProperty, core::Variant{ padding_ });
    invalidate_measure();
    invalidate_render();
}

paint::Brush Button::foreground() const noexcept
{
    const core::Variant& v = get_value(ForegroundProperty);
    return v.has<paint::Brush>() ? v.get<paint::Brush>() : paint::Brush::solid(math::Color::White);
}

void Button::set_foreground(paint::Brush brush)
{
    // 写入 ForegroundProperty Local 槽；on_foreground_changed 回调负责传播到 ContentPresenter
    // affects_render=true → set_value 内部自动触发 invalidate_render
    set_value(ForegroundProperty, core::Variant{brush}, ValuePriority::Local);
}

paint::Brush Button::background() const noexcept
{
    // 返回 BackgroundProperty 的最高优先级値（Animation > Local > TemplateBind > StyleTrigger > StyleSetter）
    const core::Variant& v = get_value(BackgroundProperty);
    return v.has<paint::Brush>() ? v.get<paint::Brush>() : paint::Brush::solid_rgb(0x6750A4);
}

void Button::set_background(paint::Brush brush)
{
    // 写入 Local(P2) 槽。由于 P2 > P4(StyleTrigger)，将不受状态色影响。
    // 如需允许状态色影响，应使用样式系统替换占位符样式。
    set_value(BackgroundProperty, core::Variant{brush}, ValuePriority::Local);
}

paint::Brush Button::border_color() const noexcept
{
    const core::Variant& v = get_value(BorderColorProperty);
    return v.has<paint::Brush>() ? v.get<paint::Brush>() : paint::Brush::solid(math::Color::Transparent);
}

void Button::set_border_color(paint::Brush brush)
{
    // 写入 BorderColorProperty Local 槽；affects_render=true → 自动触发 invalidate_render
    set_value(BorderColorProperty, core::Variant{brush}, ValuePriority::Local);
}

void Button::set_vsm_style(style::Style* style) noexcept
{
    vsm_style_ = style;
    style::Style& active = style ? *style : default_button_style();
    // 更新已有 VSM 的样式引用（VSM 在构造函数中已创建，此时必然非 nullptr）
    if (vsm()) {
        vsm()->set_style(&active);
    }
    // 重新应用 P5 基线值（更换样式后 Normal 颜色可能变化）
    active.apply(*this);
    update_visual_state();
}

style::Style* Button::vsm_style() const noexcept
{
    return vsm_style_;
}

void Button::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    // 若模板已构建，立即将字体与当前前景色传播到 ContentPresenter
    if (content_part_ != nullptr) {
        content_part_->set_font_face(font_face_);
        const core::Variant& fg_var = get_value(ForegroundProperty);
        if (fg_var.has<paint::Brush>()) {
            content_part_->set_foreground(fg_var.get<paint::Brush>());
        }
    }
}

void Button::set_font_size(float size_px) noexcept
{
    font_size_px_ = size_px;
    // 若模板已构建，立即将字号传播到 ContentPresenter
    if (content_part_ != nullptr) {
        content_part_->set_font_size(font_size_px_);
    }
}


void Button::on_measure(math::Size available_size)
{
    // 同步字体属性到 ContentPresenter（幂等）
    if (content_part_ != nullptr) {
        if (font_face_ != nullptr) {
            content_part_->set_font_face(font_face_);
        }
        content_part_->set_font_size(font_size_px_);
    }
    // 委托给基类：Control::measure_override 将测量委托给 inner_element()（ContentPresenter）
    Control::on_measure(available_size);
}

void Button::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    // 背景画刷通过 DP 优先级链读取：
    //   Animation(P1) = Storyboard 插値（状态切换动画期间）
    //   StyleTrigger(P4) = 当前状态终値（动画结束后稳定显示）
    //   StyleSetter(P5) = Normal 基线値（没有动画且没有状态覆写时）
    const core::Variant& bg_var = get_value(BackgroundProperty);
    const paint::Brush fill = bg_var.has<paint::Brush>()
        ? bg_var.get<paint::Brush>()
        : paint::Brush::solid_rgb(0x6750A4);  // 回退：MD3 Primary

    // MD3 Filled Button：完全圆角（胶囊形，radius = height / 2）
    const float radius = rect.height * 0.5f;
    canvas.fill_rounded_rect(math::RoundedRect{rect, radius}, fill);

    // MD3 State Layer：当背景色由 set_background()（Local P50）直接指定时，
    // VSM 颜色动画对该按钮无感知（has_local=true → from==to，颜色不变）。
    // 改用半透明叠加层提供悬停/按下视觉反馈（符合 MD3 State Layer 规范）：
    //   Hover   = On Primary 8%  白色叠加 → 颜色略亮
    //   Pressed = On Primary 12% 白色叠加 → 颜色更亮（配合 Ripple 涟漪）
    if (is_enabled_ && has_value(BackgroundProperty, ValuePriority::Local)) {
        float state_alpha = 0.0f;
        if (is_pressed_) {
            state_alpha = 0.12f;
        } else if (is_hovered_) {
            state_alpha = 0.08f;
        }
        if (state_alpha > 0.0f) {
            canvas.fill_rounded_rect(math::RoundedRect{rect, radius},
                paint::Brush::solid(math::Color::White.with_alpha(state_alpha)));
        }
    }

    // 圆角边框（由 BorderColorProperty 驱动；默认透明则跳过）
    // 支持自定义模板通过 BorderColorProperty 添加圆角边框，无需额外 Border 元素
    const core::Variant& bc_var = get_value(BorderColorProperty);
    const paint::Brush border_fill = bc_var.has<paint::Brush>()
        ? bc_var.get<paint::Brush>()
        : paint::Brush::solid(math::Color::Transparent);
    if (!border_fill.is_transparent()) {
        paint::Pen pen;
        pen.width = 2.0f;
        canvas.stroke_rounded_rect(math::RoundedRect{rect, radius}, border_fill, pen);
    }

    // MD3 Ripple 涟漪动画：在背景之上、文字之下绘制涟漪圆
    // elapsed_ms 由 AnimationClock 驱动的 anim_tick_callback 每帧累加
    if (ripple_.active) {
        // MD3 medium2 = 300ms（md.sys.motion.duration.medium2）
        constexpr float kRippleDurationMs = 300.0f;
        const float t = ripple_.elapsed_ms / kRippleDurationMs;
        if (t < 1.0f) {
            // 半径：从 0 扩展到按钮对角线的 60%（确保覆盖整个按钮）
            const float max_r = std::sqrt(rect.width  * rect.width
                                        + rect.height * rect.height) * 0.6f;
            const float ripple_r = max_r * t;
            // alpha：MD3 Pressed State Layer opacity = 0.12，线性淡出到 0
            const float alpha = 0.12f * (1.0f - t);
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

    // ContentPresenter 作为内部子元素由渲染管线自动渲染（无需 Button 显式调用）
}

void Button::on_arrange(math::Rect final_rect)
{
    // 先调用基类完成 ContentPresenter 的排列
    // （FrameworkElement::on_arrange → arrange_override → ContentPresenter::arrange）
    // 若跳过此调用，inner_element 的 bounds_rect 将保持零矩形，文字不可见
    FrameworkElement::on_arrange(final_rect);

    // 设置胶囊形裁剪（MD3 规范：corner_radius = height / 2），统一驱动：
    //   1. 渲染裁剪：子元素（ContentPresenter 等）不会溢出胶囊边界
    //   2. 命中测试边界：hit_test_local() 自动使用此形状，无需覆写 hit_test()
    const float radius = final_rect.height * 0.5f;
    set_clip_rounded_rect(math::RoundedRect{final_rect, radius});
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
    // 先触发 Click 路由事件（Bubble 策略，用户可在处理器中标记 handled 阻止后续传播）
    RoutedEventArgs args{ ClickEvent() };
    args.set_original_source(this);
    args.set_source(this);
    EventManager::raise(*this, args);

    // 若已绑定命令，且命令当前可执行，则调用 execute（等价于 WPF 的 CommandHelpers.ExecuteCommandSource）
    const core::Variant& cmd_var = get_value(CommandProperty);
    if (cmd_var.has<ICommand*>()) {
        ICommand* cmd = cmd_var.get<ICommand*>();
        if (cmd != nullptr) {
            const core::Variant& param = get_value(CommandParameterProperty);
            if (cmd->can_execute(param)) {
                cmd->execute(param);
            }
        }
    }
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
        constexpr float kRippleDurationMs = 300.0f;  // MD3 medium2
        self->ripple_.elapsed_ms += dt * 1000.0f;
        if (self->ripple_.elapsed_ms < kRippleDurationMs) {
            any_active = true;
        } else {
            self->ripple_.active = false;  // 动画完成，on_render 不再绘制涟漪
        }
        // 触发重绘以反映当前 elapsed_ms 对应的涟漪半径/透明度
        self->invalidate_render();
    }

    // ── VSM 背景色过渡 Storyboard（由 VisualStateManager 统一管理）─────────────────────
    // go_to_state 已写入 StyleTrigger(P4) 终値并创建 Storyboard；
    // affects_render=true → tick 内部写入 P1 时自动触发 invalidate_render
    if (auto* v = self->vsm()) {
        const bool vsm_active = v->tick_animations(dt);
        if (vsm_active) {
            any_active = true;
        }
    }

    // 返回 false → AnimationClock 自动移除此注册项（无需手动注销）
    return any_active;
}

void Button::on_visual_state_changed(ControlVisualState old_state,
                                     ControlVisualState new_state)
{
    // 调用基类：
    //   1. 触发 invalidate_render（确保下一帧重绘）
    //   2. 若 VSM 已配置，调用 vsm()->go_to_state(新状态名)——
    //      其内部将自动完成：
    //        a) capture_from_values()  — 采样当前 P1/P2/P3/P4/P5 各层开始值
    //        b) apply_state(P4)        — 将状态终值写入 BackgroundProperty / ForegroundProperty
    //        c) resolve_and_begin()    — 创建 Storyboard，启动插值
    Control::on_visual_state_changed(old_state, new_state);

    // 注册 AnimationClock（幂等）：
    //   - 驱动 VSM::tick_animations(dt)（背景色过渡 Storyboard）
    //   - 驱动 Ripple 涟漪动画（on_mouse_down 中已激活 ripple_.active）
    // tick 回调返回 false 时 AnimationClock 自动移除注册项
    animation::AnimationClock::instance().register_animation(this, &Button::anim_tick_callback);
}

} // namespace mine::ui
