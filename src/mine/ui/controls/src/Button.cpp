/**
 * @file Button.cpp
 * @brief Button 基础实现。
 */

#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/ContentPresenter.h>
#include <mine/ui/controls/Border.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/math/RoundedRect.h>
#include <mine/math/ComplexRoundedRect.h>
#include <mine/math/CornerRadii.h>
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
// 变更经组合式 bind_property（Button.Foreground → ContentPresenter.Foreground）自动传播
const DependencyProperty& Button::ForegroundProperty =
    register_property<Button>(
        "Foreground",
        core::Variant{ paint::Brush::solid(math::Color::White) },  // MD3 On Primary
        PropertyMetadata{
            .affects_render = true,
        });

// Button::BorderColorProperty — 边框画刷（MD3 Filled Button 无外边框，默认透明）
// 边框画刷经 bind_property（Button.BorderColor → Border.BorderBrush）下沉给基元 Border 绘制；
// 边框粗细由 on_border_color_changed 根据是否透明推导（透明 0 / 非透明 2dp）。
const DependencyProperty& Button::BorderColorProperty =
    register_property<Button>(
        "BorderColor",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
        PropertyMetadata{
            .affects_render = true,
            .changed        = &Button::on_border_color_changed,
        });

// Button::StateLayerBrushProperty — State Layer 蒙版画刷（MD3 hover/press 反馈层）
// 默认白色全透明 Color{1,1,1,0}（避免与非透明白插值时 RGB 由 0 渐变导致中途偏暗）。
// 状态终值由默认样式 StyleTrigger 写入，VSM Storyboard 插值缓动；
// 经 bind_property 同步到独立的 State Layer Border 背景，由该 Border 基元真正绘制。
const DependencyProperty& Button::StateLayerBrushProperty =
    register_property<Button>(
        "StateLayerBrush",
        core::Variant{ paint::Brush::solid(math::Color{ 1.0f, 1.0f, 1.0f, 0.0f }) },
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

// Button::CornerRadiusProperty — 圆角半径（CornerRadii，默认全 -1.0f = 自动胶囊）
// 各角 rx < 0 表示应用 height/2（MD3 Filled Button 规范），非负数为用户指定的固定圆角。
const DependencyProperty& Button::CornerRadiusProperty =
    register_property<Button>(
        "CornerRadius",
        core::Variant{ math::CornerRadii::uniform(-1.0f) },  // 默认：自动胶囊（height / 2）
        PropertyMetadata{
            .affects_arrange = true,
            .affects_render  = true,
            // 圆角变化需重新设置 clip_complex_rounded_rect
        });

// ============================================================================
// 默认样式（第三层：样式层）
// ============================================================================

/**
 * @brief 构建默认 Button 样式对象（Construct On First Use，避免静态初始化顺序问题）。
 *
 * 层 P5 StyleSetter：Normal 状态的基线外观值（背景色、前景色、边框色、State Layer 透明）。
 * 层 P4 StyleTrigger：Hovered/Pressed 的 State Layer 终值、Disabled 的背景/前景终值。
 *
 * MD3 Filled Button 语义：背景色不随 hover/press 变化，交互反馈完全交给 State Layer
 * （半透明白色蒙版叠加）；仅 Disabled 状态置灰背景/前景。
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
        s.add_setter({ &Button::StateLayerBrushProperty,
                       core::Variant{ Brush::solid(Color{ 1.0f, 1.0f, 1.0f, 0.0f }) } });  // 白色全透明

        // ── P4 StyleTrigger：Hovered 的 State Layer 终值（On Primary 8% 白）─────────────
        style::VisualStateSetters hovered;
        hovered.state_name = "Hovered";
        hovered.setters.push_back({ &Button::StateLayerBrushProperty,
            core::Variant{ Brush::solid(Color{ 1.0f, 1.0f, 1.0f, 0.08f }) } });
        s.add_state_setters(std::move(hovered));

        // ── P4 StyleTrigger：Pressed 的 State Layer 终值（On Primary 12% 白）────────────
        style::VisualStateSetters pressed;
        pressed.state_name = "Pressed";
        pressed.setters.push_back({ &Button::StateLayerBrushProperty,
            core::Variant{ Brush::solid(Color{ 1.0f, 1.0f, 1.0f, 0.12f }) } });
        s.add_state_setters(std::move(pressed));

        // ── P4 StyleTrigger：Disabled 状态终值（无动画——直接跳变） ─────────────────
        style::VisualStateSetters disabled;
        disabled.state_name = "Disabled";
        disabled.setters.push_back({ &Button::BackgroundProperty,
            core::Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.12f}) } });  // OnSurface 12%
        disabled.setters.push_back({ &Button::ForegroundProperty,
            core::Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.38f}) } });  // OnSurface 38%
        disabled.setters.push_back({ &Button::StateLayerBrushProperty,
            core::Variant{ Brush::solid(Color{ 1.0f, 1.0f, 1.0f, 0.0f }) } });  // State Layer 无反馈
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
    // 仅同步文字成员缓存（供 text() 返回）；
    // 内容到 ContentPresenter 的同步由构造函数建立的 bind_property 负责。
    if (new_v.has<containers::InlineString>()) {
        text_ = new_v.get<containers::InlineString>();
    } else {
        text_ = containers::InlineString{};
    }
}

void Button::on_padding_changed(DependencyObject*         sender,
                                const DependencyProperty& /*prop*/,
                                const core::Variant&      /*old_v*/,
                                const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<Button*>(sender);
    // 仅同步内边距成员缓存；
    // 内边距到 ContentPresenter 的同步由构造函数建立的 bind_property 负责。
    if (new_v.has<math::Thickness>()) {
        self->padding_ = new_v.get<math::Thickness>();
    }
}

void Button::on_border_color_changed(DependencyObject*         sender,
                                     const DependencyProperty& /*prop*/,
                                     const core::Variant&      /*old_v*/,
                                     const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<Button*>(sender);
    if (self->border_part_ == nullptr) {
        return;
    }
    // 边框画刷本身经 bind_property 同步到 Border.BorderBrush；
    // 此处仅按是否透明推导 Border 的边框粗细，保持「设色即显示 2dp 边框、
    // 默认透明无边框且不占布局」的便捷语义（边框由基元 Border 真正绘制）。
    const bool visible = new_v.has<paint::Brush>() && !new_v.get<paint::Brush>().is_transparent();
    self->border_part_->set_border_thickness(
        visible ? math::Thickness::uniform(2.0f) : math::Thickness::uniform(0.0f));
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
// 交互覆盖层（私有嵌套类）
// ============================================================================

/**
 * @brief Button 的交互覆盖层（组合式视觉树中间层）。
 *
 * 视觉树层级（自底向上）：
 *   Border（背景+圆角+边框）→ InteractionLayer（State Layer+Ripple）→ ContentPresenter（文字）
 *
 * 继承 Control 以复用 inner_element 委托机制：其 inner_element 为 ContentPresenter，
 * 测量/排列自动转发；on_render 仅负责绘制交互覆盖（在背景之上、文字之下）。
 * 作为 Button 的嵌套类，可直接访问 Button 的私有交互状态。
 */
class Button::InteractionLayer : public Control {
public:
    explicit InteractionLayer(Button* owner) noexcept : owner_(owner) {}

    /// 公开包装：设置内容元素（转发给 protected set_inner_element，支持子类型 OwnedPtr）
    template <typename TElement>
    void set_content(core::OwnedPtr<TElement> content) noexcept
    {
        set_inner_element(std::move(content));
    }

protected:
    /// 在背景之上、文字之下绘制交互覆盖（委托回宿主 Button）
    void on_render(paint::Canvas& canvas) override
    {
        owner_->render_interaction(canvas, bounds_rect());
    }

private:
    Button* owner_;  ///< 宿主 Button（不拥有；生命周期覆盖本层）
};

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
    // ── 组合式视觉树装配（自内向外）─────────────────────────────────────────
    // 目标层级（自底向上）：
    //   Border（背景+圆角+边框）→ StateLayerBorder（半透明白叠加）
    //     → InteractionLayer（Ripple）→ ContentPresenter（文字）
    // Button 自身不再 on_render 手画背景/边框/圆角/蒙版，全部下沉到基元 Border 与覆盖层。

    // 1) ContentPresenter（文字层，最内）
    auto content = core::make_owned<ContentPresenter>();
    content_part_ = content.get();
    content_part_->set_text_alignment(TextAlignment::Center);
    // Button 需要以真实字形墨迹为基准做视觉居中，避免文字看起来偏右偏下
    content_part_->set_use_ink_alignment(true);
    content_part_->set_vertical_alignment(VerticalAlignment::Center);

    // 2) InteractionLayer（仅 Ripple，中间层），其 inner_element 为 content
    auto interaction = core::make_owned<InteractionLayer>(this);
    interaction->set_hit_transparent(true);     // 内部实现层不参与命中测试
    interaction->set_content(std::move(content));

    // 3) StateLayerBorder（State Layer 半透明白叠加层，其 child 为 interaction）
    //    背景色由 VSM 驱动的 StateLayerBrush 经 bind_property 同步，实现状态机动画缓动。
    auto state_border = core::make_owned<Border>();
    state_border->set_border_thickness(math::Thickness::uniform(0.0f));  // 纯叠加层，无边框
    state_border_ = state_border.get();
    {
        auto deleter = interaction.get_deleter();
        UIElement* raw = interaction.release();
        state_border->set_child(core::OwnedPtr<UIElement>{ raw, deleter });
    }

    // 4) Border（背景 + 圆角 + 边框，最底层基元控件），其 child 为 state_border
    auto border = core::make_owned<Border>();
    // 边框粗细默认 0（不占布局）；当 BorderColor 非透明时由 on_border_color_changed 推导为 2dp
    border->set_border_thickness(math::Thickness::uniform(0.0f));
    border_part_ = border.get();
    // 把 StateLayerBorder 作为背景 Border 的子元素（Border 转移所有权）
    {
        auto deleter = state_border.get_deleter();
        UIElement* raw = state_border.release();
        border->set_child(core::OwnedPtr<UIElement>{ raw, deleter });
    }

    // 配置 VisualStateManager（内联原 BuildFn 中的 VSM 配置）
    style::VisualStateManager vsm{ *this };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Disabled");

    auto* btn_ptr = this;
    // 每个过渡的 Storyboard 同时驱动两个属性：
    //   - StateLayerBrush：hover/press 反馈的核心（缓动 0↔8%↔12% 白），经 bind 同步给 state_border_
    //   - Background：仅 Disabled→Normal 时从禁用灰 P60 缓动回基线色；Hovered/Pressed 因
    //     背景无 StyleTrigger 终值故 from==to 无变化（背景色恒定，符合 MD3）
    vsm.add_transition("*", "Hovered",
        [btn_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*btn_ptr, Button::StateLayerBrushProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
            sb.animate_dp(*btn_ptr, Button::BackgroundProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Normal",
        [btn_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*btn_ptr, Button::StateLayerBrushProperty,
                          animation::Duration::milliseconds(100.0f),
                          animation::QuadEaseOut);
            sb.animate_dp(*btn_ptr, Button::BackgroundProperty,
                          animation::Duration::milliseconds(100.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Pressed",
        [btn_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*btn_ptr, Button::StateLayerBrushProperty,
                          animation::Duration::milliseconds(60.0f),
                          animation::QuadEaseIn);
            sb.animate_dp(*btn_ptr, Button::BackgroundProperty,
                          animation::Duration::milliseconds(60.0f),
                          animation::QuadEaseIn);
        });
    // 注意：Disabled 不配置过渡——它是权限状态，必须即时跳变：
    // 走无动画路径 → apply_state_animation 以 P60 覆盖 Local(P50)，
    // 确保用户 set_background 自定义色仍可被禁用灰覆盖。
    // Normal 过渡的 Background animate_dp 负责 Disabled→Normal 回弹缓动。

    // 连接样式层（若用户已通过 set_vsm_style 指定自定义样式则使用该样式）
    style::Style& active_style = vsm_style_ ? *vsm_style_ : default_button_style();
    vsm.set_style(&active_style);
    set_visual_state_manager(std::move(vsm));

    // 应用 P5 基线值（StyleSetter：Normal 背景色、前景色、边框色）
    active_style.apply(*this);

    // 组合式装配绑定：以 DP↔DP 绑定把宿主外观属性单向同步到子树元素，
    // 取代手工 push。绑定建立时自动完成一次初始同步；
    // 生命周期托管于各目标元素的内置存储（其析构均早于本 Button）。
    //   背景 → Border（VSM 动画改 Button.Background(P1) 会经此绑定实时同步给 Border）
    border_part_->bind_property(Border::BackgroundProperty,
                                *this, Button::BackgroundProperty);
    //   边框画刷 → Border（边框粗细由 on_border_color_changed 按透明与否推导）
    border_part_->bind_property(Border::BorderBrushProperty,
                                *this, Button::BorderColorProperty);
    //   State Layer 蒙版色 → StateLayerBorder 背景（VSM 缓动 Button.StateLayerBrush 实时同步）
    state_border_->bind_property(Border::BackgroundProperty,
                                 *this, Button::StateLayerBrushProperty);
    //   内容/内边距/前景 → ContentPresenter
    content_part_->bind_property(ContentPresenter::ContentProperty,
                                 *this, ContentControl::ContentProperty);
    content_part_->bind_property(ContentPresenter::PaddingProperty,
                                 *this, Button::PaddingProperty);
    content_part_->bind_property(ContentPresenter::ForegroundProperty,
                                 *this, Button::ForegroundProperty);

    // 安装 Border 作为 Button 内部元素（由 Control::set_inner_element 管理生命周期）
    set_inner_element(std::move(border));

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

math::CornerRadii Button::corner_radii() const noexcept
{
    const core::Variant& v = get_value(CornerRadiusProperty);
    return v.has<math::CornerRadii>() ? v.get<math::CornerRadii>() : math::CornerRadii::uniform(-1.0f);
}

void Button::set_corner_radii(math::CornerRadii radii)
{
    set_value(CornerRadiusProperty, core::Variant{ radii }, ValuePriority::Local);
}

void Button::set_corner_radius(float radius)
{
    // 快捷方法：四角相同的圆角
    set_corner_radii(math::CornerRadii::uniform(radius));
}

math::CornerRadii Button::compute_radii(float height) const noexcept
{
    const math::CornerRadii radii = corner_radii();
    // 各角独立解析：rx < 0 则该角使用自动胶囊（height / 2）
    const float auto_r = height * 0.5f;
    auto resolve = [auto_r](math::Vec2 v) -> math::Vec2 {
        return { v.x < 0.0f ? auto_r : v.x, v.y < 0.0f ? auto_r : v.y };
    };
    return {
        resolve(radii.top_left),
        resolve(radii.top_right),
        resolve(radii.bottom_right),
        resolve(radii.bottom_left),
    };
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
    // 若模板已构建，立即将字体传播到 ContentPresenter
    // （前景色同步由 bind_property 负责，此处无需重推）
    if (content_part_ != nullptr) {
        content_part_->set_font_face(font_face_);
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

void Button::render_interaction(paint::Canvas& canvas, const math::Rect& rect)
{
    if (rect.empty()) {
        return;
    }

    // 与背景/裁剪一致的圆角（MD3 Filled Button 默认胶囊形 = height/2）
    const math::CornerRadii radii = compute_radii(rect.height);
    const math::ComplexRoundedRect crr{ rect, radii };

    // ── MD3 Ripple 涟漪 ─────────────────────────────────────────────
    // State Layer 蒙版、背景、边框均已下沉到各自的 Border 基元，此处只绘涟漪。
    // elapsed_ms 由 AnimationClock 驱动的 anim_tick_callback 每帧累加
    if (ripple_.active) {
        constexpr float kRippleDurationMs = 300.0f;  // MD3 medium2
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
            // 裁剪到按钮圆角边界，防止涟漪溢出
            canvas.save();
            canvas.clip_complex_rounded_rect(crr);
            canvas.fill_circle(center, ripple_r,
                paint::Brush::solid(math::Color::White.with_alpha(alpha)));
            canvas.restore();
        }
        // 若 t >= 1 时 ripple_.active 已被 tick 设为 false，此处无需额外处理
    }
}

void Button::on_arrange(math::Rect final_rect)
{
    // 先调用基类完成 ContentPresenter 的排列
    // （FrameworkElement::on_arrange → arrange_override → ContentPresenter::arrange）
    // 若跳过此调用，inner_element 的 bounds_rect 将保持零矩形，文字不可见
    FrameworkElement::on_arrange(final_rect);

    // 设置四角独立圆角裁剪（由 CornerRadiusProperty 驱动），统一驱动：
    //   1. 渲染裁剪：子元素（Border/InteractionLayer/ContentPresenter）不会溢出边界
    //   2. 命中测试边界：hit_test_local() 自动使用此形状，无需覆写 hit_test()
    const math::CornerRadii radii = compute_radii(final_rect.height);
    set_clip_complex_rounded_rect(math::ComplexRoundedRect{ final_rect, radii });

    // 同步解析后的胶囊圆角到各 Border（bind_property 无法表达 -1 → height/2 的胶囊解析，
    // 因为它依赖布局结果高度，故在排列阶段显式写入背景 Border 与 State Layer Border 的圆角）。
    if (border_part_ != nullptr) {
        border_part_->set_corner_radius(radii);
    }
    if (state_border_ != nullptr) {
        state_border_->set_corner_radius(radii);
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
