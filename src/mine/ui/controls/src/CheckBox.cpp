#include <mine/ui/controls/CheckBox.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/TextBlock.h>
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

    if (checked) {
        self->set_value(IconBackgroundProperty,
            core::Variant{ paint::Brush::solid_rgb(0x6750A4) },     // Primary
            ValuePriority::Local);
        self->set_value(IconBorderBrushProperty,
            core::Variant{ paint::Brush::solid_rgb(0x6750A4) },     // Primary
            ValuePriority::Local);
        self->set_value(CheckMarkBrushProperty,
            core::Variant{ paint::Brush::solid(math::Color::White) },
            ValuePriority::Local);
    } else {
        self->clear_value(IconBackgroundProperty, ValuePriority::Local);
        self->clear_value(IconBorderBrushProperty, ValuePriority::Local);
        self->clear_value(CheckMarkBrushProperty, ValuePriority::Local);
    }
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
    //   CheckBox
    //   ├── Border(icon) → Border(state) → CheckMarkElement  [inner_element]
    //   └── TextBlock(label)                                  [Visual child]

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

    // 4) TextBlock（文字标签），添加为 Visual 子节点
    owned_label_ = core::make_owned<TextBlock>();
    label_ = owned_label_.get();
    add_child(label_);

    // ── VSM 配置 ───────────────────────────────────────────────────────────

    style::VisualStateManager vsm{ *this };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");

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

    // ── 安装根元素（IconBorder 为 inner_element）───────────────────────────

    set_inner_element(std::move(owned_icon_));

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
    // 先移除 label_ 子节点引用，再让 OwnedPtr 成员析构
    if (label_) {
        remove_child(label_);
    }
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

// ============================================================================
// 布局（手动排列：icon_border_ 左 + gap + label_ 右）
// ============================================================================

void CheckBox::on_measure(math::Size available) {
    const float icon = std::roundf(font_size_ * 1.3f);
    const float gap  = font_size_ * 0.5f;

    // 图标 Border 固定尺寸
    const float iconTotal = icon + 4.0f;  // icon + 2px padding each side

    // 测量文字标签
    float textW = 0.0f;
    if (label_) {
        label_->measure(available);
        textW = label_->desired_size().width;
    }

    set_desired_size({iconTotal + gap + textW, iconTotal});
}

void CheckBox::on_arrange(math::Rect final_rect) {
    const float icon = std::roundf(font_size_ * 1.3f);
    const float gap  = font_size_ * 0.5f;
    const float iconTotal = icon + 4.0f;

    // 排列图标 Border：左侧、垂直居中
    if (icon_border_) {
        const float iconY = final_rect.y + (final_rect.height - iconTotal) * 0.5f;
        icon_border_->arrange(math::Rect{final_rect.x, iconY, iconTotal, iconTotal});
    }

    // 排列文字标签：图标右侧、垂直居中
    if (label_) {
        const float textH = label_->desired_size().height;
        const float labelX = final_rect.x + iconTotal + gap;
        const float labelY = final_rect.y + (final_rect.height - textH) * 0.5f;
        const float labelW = std::max(0.0f, final_rect.width - iconTotal - gap);
        label_->arrange(math::Rect{labelX, labelY, labelW, textH});
    }
}

// ============================================================================
// 视觉状态
// ============================================================================

ControlVisualState CheckBox::compute_visual_state() const noexcept {
    if (is_pressed_) return ControlVisualState::Pressed;
    if (is_hovered_) return ControlVisualState::Hovered;
    return ControlVisualState::Normal;
}

// ============================================================================
// 鼠标事件处理
// ============================================================================

void CheckBox::on_mouse_enter_router(void*, RoutedEventArgs&, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    self->is_hovered_ = true;
    self->update_visual_state();
}

void CheckBox::on_mouse_leave_router(void*, RoutedEventArgs&, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    self->is_hovered_ = false;
    self->update_visual_state();
}

void CheckBox::on_mouse_down_router(void*, RoutedEventArgs& a, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    auto& args = static_cast<input::MouseEventArgs&>(a);
    if (args.button() == input::MouseButton::Left) {
        self->is_pressed_ = true;
        self->update_visual_state();
    }
}

void CheckBox::on_mouse_up_router(void*, RoutedEventArgs& a, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    auto& args = static_cast<input::MouseEventArgs&>(a);
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
}

} // namespace mine::ui
