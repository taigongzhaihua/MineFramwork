#include <mine/ui/controls/CheckBox.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/layout/Orientation.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/animation/Storyboard.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/Duration.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/paint/PathBuilder.h>
#include <mine/math/Color.h>
#include <mine/math/CornerRadii.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/property/ValuePriority.h>
#include <mine/ui/animation/AnimationClock.h>
#include <mine/core/Memory.h>
#include <cmath>

using namespace mine;

namespace mine::ui {

// ============================================================================
// 依赖属性注册
// ============================================================================

const DependencyProperty& CheckBox::IsCheckedProperty =
    register_property<CheckBox>("IsChecked", core::Variant{false},
        PropertyMetadata{
            .affects_render = true,
            .changed = &CheckBox::on_is_checked_changed,
        });

const DependencyProperty& CheckBox::IconBackgroundProperty =
    register_property<CheckBox>("IconBackground",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
        PropertyMetadata{.affects_render = true});

const DependencyProperty& CheckBox::IconBorderBrushProperty =
    register_property<CheckBox>("IconBorderBrush",
        core::Variant{ paint::Brush::solid_rgb(0x79747E) },
        PropertyMetadata{.affects_render = true});

const DependencyProperty& CheckBox::StateLayerBrushProperty =
    register_property<CheckBox>("StateLayerBrush",
        core::Variant{ paint::Brush::solid(math::Color{1.0f, 1.0f, 1.0f, 0.0f}) },
        PropertyMetadata{.affects_render = true});

const DependencyProperty& CheckBox::CheckMarkBrushProperty =
    register_property<CheckBox>("CheckMarkBrush",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
        PropertyMetadata{.affects_render = true});

const DependencyProperty& CheckBox::TextForegroundProperty =
    register_property<CheckBox>("TextForeground",
        core::Variant{ paint::Brush::solid_rgb(0x1C1B1F) },
        PropertyMetadata{.affects_render = true});

const DependencyProperty& CheckBox::IsEnabledProperty =
    register_property<CheckBox>("IsEnabled", core::Variant{true},
        PropertyMetadata{.affects_render = true, .changed = &CheckBox::on_is_enabled_changed});

// ============================================================================
// 路由事件注册
// ============================================================================

const RoutedEvent& CheckBox::CheckedChangedEvent() {
    static const RoutedEvent& ev = register_event<CheckBox>("CheckedChanged", RoutingStrategy::Bubble);
    return ev;
}

// ============================================================================
// 默认样式
// ============================================================================

static style::Style& default_checkbox_style()
{
    static style::Style s = [] {
        using namespace mine::paint;
        using namespace mine::math;

        style::Style s;
        s.set_target_type(core::TypeId::of<CheckBox>());
        s.set_name("DefaultCheckBox");

        // ── P5 StyleSetter：Normal 基线外观 ──────────────────────────────────
        s.add_setter({ &CheckBox::IconBackgroundProperty,
                       core::Variant{ Brush::solid(Color::Transparent) } });
        s.add_setter({ &CheckBox::IconBorderBrushProperty,
                       core::Variant{ Brush::solid_rgb(0x79747E) } });          // #79747E
        s.add_setter({ &CheckBox::StateLayerBrushProperty,
                       core::Variant{ Brush::solid(Color{1.0f, 1.0f, 1.0f, 0.0f}) } });  // 白色全透明
        s.add_setter({ &CheckBox::CheckMarkBrushProperty,
                       core::Variant{ Brush::solid(Color::Transparent) } });
        s.add_setter({ &CheckBox::TextForegroundProperty,
                       core::Variant{ Brush::solid_rgb(0x1C1B1F) } });          // #1C1B1F

        // ── P4 StyleTrigger：Hovered ─────────────────────────────────────────
        style::VisualStateSetters hovered;
        hovered.state_name = "Hovered";
        hovered.setters.push_back({ &CheckBox::IconBorderBrushProperty,
            core::Variant{ Brush::solid_rgb(0x6750A4) } });                     // Primary
        hovered.setters.push_back({ &CheckBox::StateLayerBrushProperty,
            core::Variant{ Brush::solid(Color{1.0f, 1.0f, 1.0f, 0.08f}) } });  // 8% 白
        s.add_state_setters(std::move(hovered));

        // ── P4 StyleTrigger：Pressed ─────────────────────────────────────────
        style::VisualStateSetters pressed;
        pressed.state_name = "Pressed";
        pressed.setters.push_back({ &CheckBox::IconBorderBrushProperty,
            core::Variant{ Brush::solid_rgb(0x6750A4) } });                     // Primary
        pressed.setters.push_back({ &CheckBox::StateLayerBrushProperty,
            core::Variant{ Brush::solid(Color{1.0f, 1.0f, 1.0f, 0.12f}) } });  // 12% 白
        s.add_state_setters(std::move(pressed));

        // ── P4 StyleTrigger：Disabled ───────────────────────────────────────
        style::VisualStateSetters disabled;
        disabled.state_name = "Disabled";
        disabled.setters.push_back({ &CheckBox::IconBorderBrushProperty,
            core::Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.38f}) } });
        disabled.setters.push_back({ &CheckBox::TextForegroundProperty,
            core::Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.38f}) } });
        disabled.setters.push_back({ &CheckBox::CheckMarkBrushProperty,
            core::Variant{ Brush::solid(Color{1.0f, 1.0f, 1.0f, 0.38f}) } });
        disabled.setters.push_back({ &CheckBox::StateLayerBrushProperty,
            core::Variant{ Brush::solid(Color{1.0f, 1.0f, 1.0f, 0.0f}) } });
        s.add_state_setters(std::move(disabled));

        return s;
    }();
    return s;
}

// ============================================================================
// 勾选组专用样式（仅包含 Checked / Unchecked 状态的 setter）
// ============================================================================
static style::Style& default_checkbox_checked_style()
{
    static style::Style s = [] {
        using namespace mine::paint;
        using namespace mine::math;

        style::Style s;
        s.set_target_type(core::TypeId::of<CheckBox>());
        s.set_name("DefaultCheckBox_CheckedGroup");

        // Unchecked 基线（透明、灰边、无勾号）——作为基线可省略，显式写入以防样式缺省
        s.add_setter({ &CheckBox::IconBackgroundProperty,
                       core::Variant{ Brush::solid(Color::Transparent) } });
        s.add_setter({ &CheckBox::IconBorderBrushProperty,
                       core::Variant{ Brush::solid_rgb(0x79747E) } });
        s.add_setter({ &CheckBox::CheckMarkBrushProperty,
                       core::Variant{ Brush::solid(Color::Transparent) } });

        // Checked 状态：图标主色 + 勾号白
        style::VisualStateSetters checked;
        checked.state_name = "Checked";
        checked.setters.push_back({ &CheckBox::IconBackgroundProperty,
            core::Variant{ Brush::solid_rgb(0x6750A4) } });
        checked.setters.push_back({ &CheckBox::IconBorderBrushProperty,
            core::Variant{ Brush::solid_rgb(0x6750A4) } });
        checked.setters.push_back({ &CheckBox::CheckMarkBrushProperty,
            core::Variant{ Brush::solid(Color::White) } });
        s.add_state_setters(std::move(checked));

        // Unchecked 明确状态（可选）
        style::VisualStateSetters unchecked;
        unchecked.state_name = "Unchecked";
        unchecked.setters.push_back({ &CheckBox::IconBackgroundProperty,
            core::Variant{ Brush::solid(Color::Transparent) } });
        unchecked.setters.push_back({ &CheckBox::IconBorderBrushProperty,
            core::Variant{ Brush::solid_rgb(0x79747E) } });
        unchecked.setters.push_back({ &CheckBox::CheckMarkBrushProperty,
            core::Variant{ Brush::solid(Color::Transparent) } });
        s.add_state_setters(std::move(unchecked));

        return s;
    }();
    return s;
}

// ============================================================================
// IsChecked 变更回调 —— 勾选/取消时直接写入图标外观属性（P50 Local）
// ============================================================================

void CheckBox::on_is_checked_changed(DependencyObject*         sender,
                                     const DependencyProperty& /*prop*/,
                                     const core::Variant&      /*old_v*/,
                                     const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<CheckBox*>(sender);
    const bool checked = new_v.has<bool>() && new_v.get<bool>();
    // 切换勾选组状态，使用独立 VSM 驱动视觉。
    // 先停止交互组 VSM 的活跃 Storyboard，防止两组同时动画 IconBorderBrush 导致抖动。
    if (self->owned_checked_vsm_) {
        if (auto* ivsm = self->vsm()) {
            ivsm->stop_all_storyboards();
        }
        if (checked) {
            self->owned_checked_vsm_.get()->go_to_state("Checked");
        } else {
            self->owned_checked_vsm_.get()->go_to_state("Unchecked");
        }
        animation::AnimationClock::instance().register_animation(self, &CheckBox::anim_tick_callback);
    }
}

void CheckBox::on_is_enabled_changed(DependencyObject*         sender,
                                     const DependencyProperty& /*prop*/,
                                     const core::Variant&      /*old_v*/,
                                     const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<CheckBox*>(sender);
    const bool enabled = new_v.has<bool>() ? new_v.get<bool>() : true;
    if (self->is_enabled_ == enabled) return;
    self->is_enabled_ = enabled;
    if (!self->is_enabled_) {
        self->is_pressed_ = false;
        self->is_hovered_ = false;
    }
    self->update_visual_state();
    self->invalidate_render();
}

// ============================================================================
// 勾号元素（私有嵌套类）
// ============================================================================

/**
 * @brief CheckBox 的勾号元素（组合式视觉树最内层）。
 *
 * 仅负责在 on_render 中绘制 MD3 "check" 图标填充路径。
 * 勾号颜色由 CheckMarkBrushProperty 驱动（通过 bind_property 从父 CheckBox 同步），
 * 透明画刷时自然不绘制（实现勾选/取消切换）。
 */
class CheckBox::CheckMarkElement : public Control {
public:
    static const DependencyProperty& BrushProperty;

    CheckMarkElement() {
        // 注册自己的 Brush DP
    }

    /// 公开包装：返回 Brush DP 的静态引用（供 bind_property 使用）
    static const DependencyProperty& brush_prop() { return BrushProperty; }

protected:
    void on_render(paint::Canvas& canvas) override {
        const math::Rect r = bounds_rect();
        if (r.empty()) return;

        // 输出勾号元素的 bounds（调试信息已移除）

        // 读取当前画刷颜色
        const core::Variant& bv = get_value(BrushProperty);
        if (!bv.has<paint::Brush>() || bv.get<paint::Brush>().is_transparent()) {
            return;  // 透明 → 不绘制（未勾选状态）
        }

        // 图标框即 bounds_rect（由父 StackPanel 布局分配）
        const float icon = r.width;  // 图标框为正方形
        const float s    = icon * 0.72f;
        const float ox   = r.x + (icon - s) * 0.5f;
        const float oy   = r.y + (icon - s) * 0.5f;

        auto path = paint::PathBuilder{}
            .move_to({ox + s * 0.375f, oy + s * 0.674f})  // M9, 16.17
            .line_to({ox + s * 0.201f, oy + s * 0.500f})  // L4.83, 12
            .line_to({ox + s * 0.142f, oy + s * 0.559f})  // l-1.42, 1.41
            .line_to({ox + s * 0.375f, oy + s * 0.792f})  // L9, 19
            .line_to({ox + s * 0.875f, oy + s * 0.292f})  // L21, 7
            .line_to({ox + s * 0.816f, oy + s * 0.233f})  // l-1.41, -1.41
            .close()
            .build();

        canvas.fill_path(path, bv.get<paint::Brush>());
    }
};

const DependencyProperty& CheckBox::CheckMarkElement::BrushProperty =
    register_property<CheckMarkElement>("Brush",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
        PropertyMetadata{.affects_render = true});

// ============================================================================
// 构造 / 析构
// ============================================================================

CheckBox::CheckBox()
{
    // ── 组合式视觉树装配（自内向外）─────────────────────────────────────────
    //
    // 目标层级：
    //   StackPanel(H)
    //   ├── Border(icon) → Border(state) → CheckMarkElement
    //   └── TextBlock(label)

    // 1) CheckMarkElement（勾号，最内层）
    owned_check_ = core::make_owned<CheckMarkElement>();
    check_mark_  = owned_check_.get();

    // 2) StateLayerBorder（hover/press 蒙版），其 child 为 CheckMarkElement
    owned_state_ = core::make_owned<Border>();
    state_border_ = owned_state_.get();
    state_border_->set_border_thickness(math::Thickness::uniform(0.0f));
    state_border_->set_child(check_mark_);

    // 3) IconBorder（背景+边框+2px圆角），其 child 为 state_border
    owned_icon_ = core::make_owned<Border>();
    icon_border_ = owned_icon_.get();
    icon_border_->set_border_thickness(math::Thickness::uniform(2.0f));
    icon_border_->set_corner_radius(math::CornerRadii::uniform(2.0f));
    icon_border_->set_child(state_border_);

    // 4) TextBlock（文字标签）
    owned_label_ = core::make_owned<TextBlock>();
    label_ = owned_label_.get();

    // 5) StackPanel（水平布局根），添加图标 Border 和 TextBlock
    owned_root_ = core::make_owned<StackPanel>();
    layout_root_ = owned_root_.get();
    layout_root_->set_orientation(Orientation::Horizontal);
    layout_root_->add_child(icon_border_);
    layout_root_->add_child(label_);

    // 将内部布局标记为命中穿透，使 CheckBox 本身成为命中目标，
    // 以便 Direct 策略的 MouseEnter/MouseLeave 在控件上触发（而非其子元素）。
    layout_root_->set_hit_transparent(true);
    // ── VSM 配置 ───────────────────────────────────────────────────────────

    style::VisualStateManager vsm{ *this };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Disabled");

    auto* cb_ptr = this;
    vsm.add_transition("*", "Hovered",
        [cb_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*cb_ptr, StateLayerBrushProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
            sb.animate_dp(*cb_ptr, IconBorderBrushProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Normal",
        [cb_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*cb_ptr, StateLayerBrushProperty,
                          animation::Duration::milliseconds(100.0f),
                          animation::QuadEaseOut);
            sb.animate_dp(*cb_ptr, IconBorderBrushProperty,
                          animation::Duration::milliseconds(100.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Pressed",
        [cb_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*cb_ptr, StateLayerBrushProperty,
                          animation::Duration::milliseconds(60.0f),
                          animation::QuadEaseIn);
            sb.animate_dp(*cb_ptr, IconBorderBrushProperty,
                          animation::Duration::milliseconds(60.0f),
                          animation::QuadEaseIn);
        });

    style::Style& active_style = default_checkbox_style();
    vsm.set_style(&active_style);
    set_visual_state_manager(std::move(vsm));

    // 应用 P5 基线值
    active_style.apply(*this);

    // ── 勾选组 VSM（Checked / Unchecked）──────────────────────────────────
    owned_checked_vsm_ = core::make_owned<style::VisualStateManager>(*this);
    {
        auto* chk_vsm = owned_checked_vsm_.get();
        chk_vsm->define_state("Unchecked");
        chk_vsm->define_state("Checked");
        // 动画过渡：任意 -> Checked / Unchecked，缓动 IconBackground / IconBorder / CheckMark
        chk_vsm->add_transition("*", "Checked",
            [cb_ptr](animation::Storyboard& sb) {
                sb.animate_dp(*cb_ptr, IconBackgroundProperty,
                              animation::Duration::milliseconds(120.0f),
                              animation::QuadEaseOut);
                sb.animate_dp(*cb_ptr, IconBorderBrushProperty,
                              animation::Duration::milliseconds(120.0f),
                              animation::QuadEaseOut);
                sb.animate_dp(*cb_ptr, CheckMarkBrushProperty,
                              animation::Duration::milliseconds(120.0f),
                              animation::QuadEaseOut);
            });
        chk_vsm->add_transition("*", "Unchecked",
            [cb_ptr](animation::Storyboard& sb) {
                sb.animate_dp(*cb_ptr, IconBackgroundProperty,
                              animation::Duration::milliseconds(100.0f),
                              animation::QuadEaseOut);
                sb.animate_dp(*cb_ptr, IconBorderBrushProperty,
                              animation::Duration::milliseconds(100.0f),
                              animation::QuadEaseOut);
                sb.animate_dp(*cb_ptr, CheckMarkBrushProperty,
                              animation::Duration::milliseconds(100.0f),
                              animation::QuadEaseOut);
            });

        style::Style& checked_style = default_checkbox_checked_style();
        chk_vsm->set_style(&checked_style);

        // 初始勾选状态（立即跳变）
        if (is_checked()) {
            chk_vsm->go_to_state("Checked", false);
        } else {
            chk_vsm->go_to_state("Unchecked", false);
        }
    }

    // ── DP↔DP 绑定 ─────────────────────────────────────────────────────────

    // 图标背景 → icon_border_ 背景
    icon_border_->bind_property(Border::BackgroundProperty,
                                *this, IconBackgroundProperty);
    // 图标边框画刷 → icon_border_ 边框色
    icon_border_->bind_property(Border::BorderBrushProperty,
                                *this, IconBorderBrushProperty);
    // State Layer 蒙版色 → state_border_ 背景
    state_border_->bind_property(Border::BackgroundProperty,
                                 *this, StateLayerBrushProperty);
    // 勾号画刷 → check_mark_ 画刷
    check_mark_->bind_property(CheckMarkElement::brush_prop(),
                               *this, CheckMarkBrushProperty);
    // 文字前景 → label_ 前景
    label_->bind_property(TextBlock::ForegroundProperty,
                          *this, TextForegroundProperty);

    // ── 安装根元素（StackPanel 为 inner_element）───────────────────────────

    set_inner_element(std::move(owned_root_));

    // ── 注册鼠标事件处理器 ─────────────────────────────────────────────────

    add_handler(input::MouseEnterEvent(), &CheckBox::on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &CheckBox::on_mouse_leave_router, this);
    add_handler(input::MouseDownEvent(),  &CheckBox::on_mouse_down_router,  this);
    add_handler(input::MouseUpEvent(),    &CheckBox::on_mouse_up_router,    this);

    // 同步到初始视觉状态
    update_visual_state();
}

CheckBox::~CheckBox()
{
    // 先移除 StackPanel 对子元素的引用，再让 OwnedPtr 析构
    if (layout_root_) {
        layout_root_->remove_all_children();
    }
    // 注销 AnimationClock，防止析构后回调悬空
    animation::AnimationClock::instance().unregister_animation(this);
}

// ============================================================================
// 属性访问
// ============================================================================

bool CheckBox::is_checked() const noexcept {
    const auto& v = get_value(IsCheckedProperty);
    return v.has<bool>() && v.get<bool>();
}

void CheckBox::set_checked(bool checked) {
    set_value(IsCheckedProperty, core::Variant{checked});
}

core::StringView CheckBox::text() const noexcept { return text_; }

void CheckBox::set_text(core::StringView t) {
    text_ = containers::InlineString{t.data(), t.size()};
    if (label_) {
        label_->set_text(t);
    }
    invalidate_measure();
}

void CheckBox::set_font_face(void* f) noexcept {
    font_face_ = f;
    // font_face 同步到子 label（无调试输出）
    if (label_) {
        label_->set_font_face(f);
    }
    invalidate_measure();
}

void CheckBox::set_font_size(float s) noexcept {
    font_size_ = s < 1.0f ? 1.0f : s;
    if (label_) {
        label_->set_font_size(s);
    }
    invalidate_measure();
}

bool CheckBox::is_enabled() const noexcept {
    const auto& v = get_value(IsEnabledProperty);
    return v.has<bool>() ? v.get<bool>() : true;
}

void CheckBox::set_enabled(bool enabled) noexcept {
    const auto& v = get_value(IsEnabledProperty);
    const bool cur = v.has<bool>() ? v.get<bool>() : true;
    if (cur == enabled) return;
    set_value(IsEnabledProperty, core::Variant{enabled}, ValuePriority::Local);
}

// ============================================================================
// 布局（委托 StackPanel 标准布局）
// ============================================================================

void CheckBox::on_measure(math::Size available) {
    const float icon = std::roundf(font_size_ * 1.0f);

    // 图标 Border 显式固定尺寸
    if (icon_border_) {
        icon_border_->set_width(icon + 4.0f);
        icon_border_->set_height(icon + 4.0f);
    }

    // 委托 StackPanel 递归测量
    if (layout_root_) {
        layout_root_->measure(available);
        // 输出各部分的 desired_size
        const auto root_ds = layout_root_->desired_size();
        const auto icon_ds = icon_border_ ? icon_border_->desired_size() : math::Size{0.0f,0.0f};
        const auto label_ds = label_ ? label_->desired_size() : math::Size{0.0f,0.0f};
         // 运行时不再打印调试信息

        // 注意：FrameworkElement::on_measure 会将 Margin 加回到最终 desired_size，
        // 但这里我们重写了 on_measure，因此必须显式将控件的 Margin 加入到期望尺寸，
        // 否则父布局会把 Margin 当作额外空间从分配槽中扣除，导致内容区域过小。
         const math::Thickness m = margin();
         set_desired_size({ root_ds.width + m.horizontal(), root_ds.height + m.vertical() });
         // 不再输出 margin/desired_size 调试信息
    } else {
        const math::Thickness m = margin();
        set_desired_size({ icon + 4.0f + m.horizontal(), icon + 4.0f + m.vertical() });
    }
}

// ============================================================================
// 视觉状态
// ============================================================================

ControlVisualState CheckBox::compute_visual_state() const noexcept {
    if (!is_enabled()) return ControlVisualState::Disabled;
    if (is_pressed_) return ControlVisualState::Pressed;
    if (is_hovered_) return ControlVisualState::Hovered;
    return ControlVisualState::Normal;
}

void CheckBox::on_visual_state_changed(ControlVisualState old_state,
                                       ControlVisualState new_state)
{
    // 基类处理（invalidate_render + vsm()->go_to_state 已在 update_visual_state 中调用）
    Control::on_visual_state_changed(old_state, new_state);

    // 交互组已启动新 Storyboard，停止勾选组的活跃 Storyboard
    // 防止两组同时动画 IconBorderBrush 导致抖动
    if (owned_checked_vsm_) {
        owned_checked_vsm_->stop_all_storyboards();
    }

    // 注册 AnimationClock（幂等）：驱动 VSM Storyboard 的逐帧推进
    animation::AnimationClock::instance().register_animation(this, &CheckBox::anim_tick_callback);
}

bool CheckBox::anim_tick_callback(void* handle, float dt) noexcept
{
    auto* self = static_cast<CheckBox*>(handle);
    bool  any_active = false;

    if (auto* v = self->vsm()) {
        if (v->tick_animations(dt)) {
            any_active = true;
        }
    }

    // 进推进勾选组的动画（若存在）
    if (self->owned_checked_vsm_) {
        if (self->owned_checked_vsm_.get()->tick_animations(dt)) {
            any_active = true;
        }
    }

    return any_active;
}

// ============================================================================
// 鼠标事件处理
// ============================================================================

void CheckBox::on_mouse_enter_router(void*, RoutedEventArgs&, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    if (!self->is_enabled()) return;
    // 接收到 MouseEnter
    self->is_hovered_ = true;
    self->update_visual_state();
    // MouseEnter 处理完成
}

void CheckBox::on_mouse_leave_router(void*, RoutedEventArgs&, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    if (!self->is_enabled()) return;
    // 接收到 MouseLeave
    self->is_hovered_ = false;
    self->update_visual_state();
    // MouseLeave 处理完成
}

void CheckBox::on_mouse_down_router(void*, RoutedEventArgs& a, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    auto& args = static_cast<input::MouseEventArgs&>(a);
    if (!self->is_enabled()) return;
    if (args.button() == input::MouseButton::Left) {
        // MouseDown 接收
        self->is_pressed_ = true;
        self->update_visual_state();
        // MouseDown 处理完成
    }
}

void CheckBox::on_mouse_up_router(void*, RoutedEventArgs& a, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    auto& args = static_cast<input::MouseEventArgs&>(a);
    if (!self->is_enabled()) {
        self->is_pressed_ = false;
        self->update_visual_state();
        return;
    }

    if (self->is_pressed_ && args.button() == input::MouseButton::Left) {
        // 边界检测：仅在鼠标位于控件矩形内时触发切换
        const math::Rect r = self->bounds_rect();
        const auto&    pt = args.position();
        if (pt.x >= r.x && pt.x <= r.x + r.width &&
            pt.y >= r.y && pt.y <= r.y + r.height) {
            self->set_checked(!self->is_checked());
            RoutedEventArgs ev{CheckedChangedEvent()};
            EventManager::raise(*self, ev);
        }
    }
    self->is_pressed_ = false;
    self->update_visual_state();
    // MouseUp 处理完成
}

} // namespace mine::ui
