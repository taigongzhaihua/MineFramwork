#include <mine/ui/controls/CheckBox.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>
#include <mine/math/RoundedRect.h>
#include <mine/math/Color.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/text/FontFace.h>
#include <cmath>

namespace mine::ui {

const DependencyProperty& CheckBox::IsCheckedProperty =
    register_property<CheckBox>("IsChecked", core::Variant{false},
        PropertyMetadata{.affects_render = true});

const RoutedEvent& CheckBox::CheckedChangedEvent() {
    static const RoutedEvent& ev = register_event<CheckBox>("CheckedChanged", RoutingStrategy::Bubble);
    return ev;
}

// ── 构造 ──────────────────────────────────────────────────────────────────

CheckBox::CheckBox() {
    add_handler(input::MouseEnterEvent(), &CheckBox::on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &CheckBox::on_mouse_leave_router, this);
    add_handler(input::MouseDownEvent(),  &CheckBox::on_mouse_down_router,  this);
    add_handler(input::MouseUpEvent(),    &CheckBox::on_mouse_up_router,    this);
}

CheckBox::~CheckBox() = default;

// ── 属性访问 ──────────────────────────────────────────────────────────────

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
    invalidate_measure();
}
void CheckBox::set_font_face(void* f) noexcept {
    font_face_ = f;
    invalidate_measure();
}
void CheckBox::set_font_size(float s) noexcept {
    font_size_ = s < 1.0f ? 1.0f : s;
    invalidate_measure();
}

// ── 布局 ──────────────────────────────────────────────────────────────────

void CheckBox::on_measure(math::Size available) {
    const float icon      = std::roundf(font_size_ * 1.3f);
    const float gap       = font_size_ * 0.5f;
    float text_w = 0.0f;
    if (!text_.empty() && font_face_ != nullptr) {
        auto* face = static_cast<text::FontFace*>(font_face_);
        text_w = face->measure_text(text_.data(), text_.size(), font_size_);
    }
    set_desired_size({icon + gap + text_w + 4.0f, icon + 4.0f});
}

// ── 渲染 ──────────────────────────────────────────────────────────────────

void CheckBox::on_render(paint::Canvas& canvas) {
    const math::Rect bounds = bounds_rect();
    if (bounds.empty()) return;

    const float icon = std::roundf(font_size_ * 1.3f);
    const float y    = bounds.y + (bounds.height - icon) * 0.5f;
    const math::Rect ir{bounds.x + 2.0f, y, icon, icon};

    const bool checked = is_checked();
    const math::Color primary{0.404f, 0.314f, 0.643f, 1.0f}; // #6750A4
    const math::Color outline{0.475f, 0.455f, 0.494f, 1.0f}; // #79747E

    // 背景填充
    canvas.fill_rounded_rect(math::RoundedRect{ir, 2.0f},
        paint::Brush::solid(checked ? primary : math::Color::Transparent));

    // 边框
    const math::Color sc = (is_hovered_ && !checked) ? primary : outline;
    paint::Pen pen; pen.width = 2.0f;
    canvas.stroke_rounded_rect(math::RoundedRect{ir, 2.0f},
        paint::Brush::solid(sc), pen);

    // 勾号：使用字体渲染 Unicode 勾号字符 ✓（U+2713，MD3 风格）
    if (checked && font_face_ != nullptr) {
        // UTF-8 编码：U+2713 → E2 9C 93（3 字节）
        static const char kCheckMark[] = "\xE2\x9C\x93";
        constexpr size_t kCheckMarkLen = 3;

        auto* face       = static_cast<text::FontFace*>(font_face_);
        const float chkSz = icon * 0.68f;           // 勾号字号 = 图标框的 68%

        // 测量勾号宽度以水平居中
        const float chkW = face->measure_text(kCheckMark, kCheckMarkLen, chkSz);

        // 计算居中位置：X = 图标中心 − 勾号半宽；Y = 图标基线（ascender 偏移）
        const float cx = ir.x + (ir.width  - chkW)  * 0.5f;
        const float cy = ir.y + (ir.height - chkSz) * 0.5f
                         + static_cast<float>(face->ascender() * (chkSz / font_size_));

        canvas.draw_text(core::StringView{kCheckMark, kCheckMarkLen},
            math::Vec2{cx, cy}, face, chkSz,
            paint::Brush::solid_rgb(0xFFFFFF));
    }

    // 文字
    if (!text_.empty() && font_face_ != nullptr) {
        auto* face = static_cast<text::FontFace*>(font_face_);
        face->set_pixel_size(0u, static_cast<uint32_t>(font_size_ + 0.5f));
        const float tx = ir.x + icon + font_size_ * 0.5f;
        const float ty = bounds.y + (bounds.height - font_size_) * 0.5f + static_cast<float>(face->ascender());
        canvas.draw_text(core::StringView{text_},
            math::Vec2{tx, ty}, face, font_size_,
            paint::Brush::solid_rgb(0x1C1B1F));
    }
}

// ── 路由桩（内联处理）────────────────────────────────────────────────────

void CheckBox::on_mouse_enter_router(void*, RoutedEventArgs&, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    self->is_hovered_ = true;
    self->update_visual_state();
    self->invalidate_render();
}
void CheckBox::on_mouse_leave_router(void*, RoutedEventArgs&, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    self->is_hovered_ = false;
    self->is_pressed_ = false;
    self->update_visual_state();
    self->invalidate_render();
}
void CheckBox::on_mouse_down_router(void*, RoutedEventArgs& a, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    auto& args = static_cast<input::MouseEventArgs&>(a);
    if (args.button() == input::MouseButton::Left) {
        self->is_pressed_ = true;
        self->update_visual_state();
        self->invalidate_render();
    }
}
void CheckBox::on_mouse_up_router(void*, RoutedEventArgs& a, void* ud) {
    auto* self = static_cast<CheckBox*>(ud);
    auto& args = static_cast<input::MouseEventArgs&>(a);
    if (self->is_pressed_ && args.button() == input::MouseButton::Left) {
        self->set_checked(!self->is_checked());
        RoutedEventArgs ev{CheckedChangedEvent()};
        EventManager::raise(*self, ev);
    }
    self->is_pressed_ = false;
    self->update_visual_state();
    self->invalidate_render();
}

// ── 视觉状态 ──────────────────────────────────────────────────────────────

ControlVisualState CheckBox::compute_visual_state() const noexcept {
    if (is_pressed_) return ControlVisualState::Pressed;
    if (is_hovered_) return ControlVisualState::Hovered;
    return ControlVisualState::Normal;
}

} // namespace mine::ui
