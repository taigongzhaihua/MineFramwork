/**
 * @file TextBox.cpp
 * @brief TextBox 单行文本输入控件实现（MD3 Filled Text Field 风格）。
 *
 * 架构三层分离：
 *   层1（逻辑层）：DP + 交互状态 bool（is_hovered_/is_focused_/is_enabled_）+ VSM 状态计算
 *   层2（模板层）：VSM define_state / add_transition + 内联样式绑定
 *   层3（样式层）：StyleSetter(P5) 基线值 + StyleTrigger(P4) 状态终值
 */

#include <mine/ui/controls/TextBox.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>
#include <mine/math/RoundedRect.h>
#include <mine/math/Color.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/input/Key.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/style/StyleSetter.h>
#include <mine/ui/animation/Storyboard.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/Duration.h>
#include <mine/ui/animation/AnimationClock.h>
#include <mine/text/FontFace.h>
#include <mine/core/Memory.h>

#include <cmath>
#include <cstdint>
#include <cstring>

namespace mine::ui {

// ============================================================================
// 内部 UTF-8 辅助函数
// ============================================================================

/// 向前移动一个 UTF-8 字符，返回下一个字符的字节偏移
static uint32_t utf8_next(const char* data, uint32_t pos, uint32_t end) noexcept
{
    if (pos >= end) {
        return end;
    }
    const auto c = static_cast<uint8_t>(data[pos]);
    uint32_t skip = 1u;
    if      ((c & 0xF8u) == 0xF0u) { skip = 4u; }
    else if ((c & 0xF0u) == 0xE0u) { skip = 3u; }
    else if ((c & 0xE0u) == 0xC0u) { skip = 2u; }
    const uint32_t next = pos + skip;
    return next <= end ? next : end;
}

/// 向后移动一个 UTF-8 字符，返回前一个字符起始的字节偏移
static uint32_t utf8_prev(const char* data, uint32_t pos) noexcept
{
    if (pos == 0u) {
        return 0u;
    }
    uint32_t p = pos - 1u;
    while (p > 0u && (static_cast<uint8_t>(data[p]) & 0xC0u) == 0x80u) {
        --p;
    }
    return p;
}

/// 将 UTF-32 码点转换为 UTF-8 字节序列（最多 4 字节），返回字节数
static uint32_t utf32_to_utf8(uint32_t cp, char out[4]) noexcept
{
    if (cp < 0x80u) {
        out[0] = static_cast<char>(cp);
        return 1u;
    }
    if (cp < 0x800u) {
        out[0] = static_cast<char>(0xC0u | (cp >> 6));
        out[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 2u;
    }
    if (cp < 0x10000u) {
        out[0] = static_cast<char>(0xE0u | (cp >> 12));
        out[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 3u;
    }
    out[0] = static_cast<char>(0xF0u | (cp >> 18));
    out[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = static_cast<char>(0x80u | ((cp >> 6)  & 0x3Fu));
    out[3] = static_cast<char>(0x80u | (cp         & 0x3Fu));
    return 4u;
}

// ============================================================================
// 依赖属性注册
// ============================================================================

// TextProperty — 文本内容（InlineString，默认空）
const DependencyProperty& TextBox::TextProperty =
    register_property<TextBox>(
        "Text",
        core::Variant{ containers::InlineString{} },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
        });

// PlaceholderProperty — 占位文字（InlineString，默认空）
const DependencyProperty& TextBox::PlaceholderProperty =
    register_property<TextBox>(
        "Placeholder",
        core::Variant{ containers::InlineString{} },
        PropertyMetadata{
            .affects_render = true,
        });

// IsReadOnlyProperty — 只读模式（bool，默认 false）
const DependencyProperty& TextBox::IsReadOnlyProperty =
    register_property<TextBox>(
        "IsReadOnly",
        core::Variant{ false },
        PropertyMetadata{
            .affects_render = true,
        });

// BackgroundProperty — 背景画刷（MD3 Surface #FFFBFE）
const DependencyProperty& TextBox::BackgroundProperty =
    register_property<TextBox>(
        "Background",
        core::Variant{ paint::Brush::solid_rgb(0xFFFBFE) },
        PropertyMetadata{
            .affects_render = true,
        });

// ForegroundProperty — 文字前景画刷（MD3 On Surface #1C1B1F）
const DependencyProperty& TextBox::ForegroundProperty =
    register_property<TextBox>(
        "Foreground",
        core::Variant{ paint::Brush::solid_rgb(0x1C1B1F) },
        PropertyMetadata{
            .affects_render = true,
        });

// BorderColorProperty — 边框画刷（MD3 Outline #79747E）
const DependencyProperty& TextBox::BorderColorProperty =
    register_property<TextBox>(
        "BorderColor",
        core::Variant{ paint::Brush::solid_rgb(0x79747E) },
        PropertyMetadata{
            .affects_render = true,
        });

// PlaceholderForegroundProperty — 占位文字颜色（MD3 On Surface Variant 60%）
const DependencyProperty& TextBox::PlaceholderForegroundProperty =
    register_property<TextBox>(
        "PlaceholderForeground",
        core::Variant{ paint::Brush::solid(math::Color{ 0.29f, 0.27f, 0.31f, 0.60f }) },
        PropertyMetadata{
            .affects_render = true,
        });

// PaddingProperty — 内边距（Thickness，默认左右 16px、上下 8px）
const DependencyProperty& TextBox::PaddingProperty =
    register_property<TextBox>(
        "Padding",
        core::Variant{ math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f } },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
        });

// CornerRadiusProperty — 圆角半径（float，默认 4.0f）
const DependencyProperty& TextBox::CornerRadiusProperty =
    register_property<TextBox>(
        "CornerRadius",
        core::Variant{ 4.0f },
        PropertyMetadata{
            .affects_arrange = true,
            .affects_render  = true,
        });

// ============================================================================
// 路由事件注册
// ============================================================================

const RoutedEvent& TextBox::TextChangedEvent()
{
    static const RoutedEvent& ev =
        register_event<TextBox>("TextChanged", RoutingStrategy::Bubble);
    return ev;
}

// ============================================================================
// 默认样式（样式层，第三层）
// ============================================================================

/**
 * @brief 构建默认 TextBox 样式对象（Construct On First Use，避免静态初始化顺序问题）。
 *
 * P5 StyleSetter：Normal 基线外观（背景色、前景色、边框色）。
 * P4 StyleTrigger：Hovered / Focused / Disabled 状态终值。
 * 动画曲线在 VSM add_transition 中配置（构造函数内联模板层）。
 */
static style::Style& default_textbox_style()
{
    static style::Style s = [] {
        using namespace mine::paint;
        using namespace mine::math;

        style::Style s;
        s.set_target_type(core::TypeId::of<TextBox>());
        s.set_name("DefaultTextBox");

        // ── P5 StyleSetter：Normal 基线外观 ────────────────────────────────
        s.add_setter({ &TextBox::BackgroundProperty,
                       core::Variant{ Brush::solid_rgb(0xFFFBFE) } });  // MD3 Surface
        s.add_setter({ &TextBox::ForegroundProperty,
                       core::Variant{ Brush::solid_rgb(0x1C1B1F) } });  // MD3 On Surface
        s.add_setter({ &TextBox::BorderColorProperty,
                       core::Variant{ Brush::solid_rgb(0x79747E) } });  // MD3 Outline

        // ── P4 StyleTrigger：Hovered 状态终值 ──────────────────────────────
        style::VisualStateSetters hovered;
        hovered.state_name = "Hovered";
        hovered.setters.push_back({ &TextBox::BorderColorProperty,
            core::Variant{ Brush::solid_rgb(0x1C1B1F) } });  // MD3 On Surface（边框深色）
        s.add_state_setters(std::move(hovered));

        // ── P4 StyleTrigger：Focused 状态终值 ──────────────────────────────
        style::VisualStateSetters focused;
        focused.state_name = "Focused";
        focused.setters.push_back({ &TextBox::BorderColorProperty,
            core::Variant{ Brush::solid_rgb(0x6750A4) } });  // MD3 Primary（主色边框）
        s.add_state_setters(std::move(focused));

        // ── P4 StyleTrigger：Disabled 状态终值 ─────────────────────────────
        style::VisualStateSetters disabled;
        disabled.state_name = "Disabled";
        disabled.setters.push_back({ &TextBox::BackgroundProperty,
            core::Variant{ Brush::solid(Color{ 0.11f, 0.11f, 0.14f, 0.04f }) } });  // On Surface 4%
        disabled.setters.push_back({ &TextBox::BorderColorProperty,
            core::Variant{ Brush::solid(Color{ 0.11f, 0.11f, 0.14f, 0.38f }) } });  // On Surface 38%
        s.add_state_setters(std::move(disabled));

        return s;
    }();
    return s;
}

// ============================================================================
// 生命周期
// ============================================================================

TextBox::TextBox()
{
    // 声明为可聚焦（InputRouter::dispatch_mouse_event 在 MouseDown 时自动调用 set_keyboard_focus）
    set_focusable(true);

    // ── 配置 VisualStateManager（模板层第二层） ─────────────────────────────
    style::VisualStateManager vsm{ *this };
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Focused");
    vsm.define_state("Disabled");

    // 过渡动画：边框颜色插值
    auto* tb_ptr = this;
    vsm.add_transition("*", "Normal",
        [tb_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*tb_ptr, TextBox::BorderColorProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Hovered",
        [tb_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*tb_ptr, TextBox::BorderColorProperty,
                          animation::Duration::milliseconds(80.0f),
                          animation::QuadEaseOut);
        });
    vsm.add_transition("*", "Focused",
        [tb_ptr](animation::Storyboard& sb) {
            sb.animate_dp(*tb_ptr, TextBox::BorderColorProperty,
                          animation::Duration::milliseconds(120.0f),
                          animation::QuadEaseOut);
        });

    // 连接样式层（P4 StyleTrigger 终值 + P5 StyleSetter 基线）
    vsm.set_style(&default_textbox_style());
    set_visual_state_manager(std::move(vsm));

    // 应用 P5 基线值（Normal 背景色、前景色、边框色）
    default_textbox_style().apply(*this);

    // ── 注册事件处理器 ─────────────────────────────────────────────────────
    add_handler(input::MouseEnterEvent(), &TextBox::on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &TextBox::on_mouse_leave_router, this);
    add_handler(input::MouseDownEvent(),  &TextBox::on_mouse_down_router,  this);
    add_handler(input::KeyDownEvent(),    &TextBox::on_key_down_router,    this);
    add_handler(input::TextInputEvent(),  &TextBox::on_text_input_router,  this);
    add_handler(input::GotFocusEvent(),   &TextBox::on_got_focus_router,   this);
    add_handler(input::LostFocusEvent(),  &TextBox::on_lost_focus_router,  this);

    // 同步到初始视觉状态（Normal）
    update_visual_state();
}

TextBox::~TextBox()
{
    // 注销 AnimationClock 注册项，防止 tick_all 回调访问已释放内存
    animation::AnimationClock::instance().unregister_animation(this);
}

// ============================================================================
// 属性访问器
// ============================================================================

core::StringView TextBox::text() const noexcept
{
    return { text_buf_.data(), text_buf_.size() };
}

void TextBox::set_text(core::StringView text)
{
    // 更新内部缓存
    text_buf_ = containers::InlineString{};
    if (!text.empty()) {
        text_buf_.append(text.data(), text.size());
    }
    // 光标移到末尾
    cursor_pos_ = static_cast<uint32_t>(text_buf_.size());
    // 同步 DP（affects_measure/affects_render=true → 自动触发重新测量和重绘）
    set_value(TextProperty, core::Variant{ text_buf_ });
}

core::StringView TextBox::placeholder() const noexcept
{
    return { placeholder_buf_.data(), placeholder_buf_.size() };
}

void TextBox::set_placeholder(core::StringView placeholder)
{
    placeholder_buf_ = containers::InlineString{};
    if (!placeholder.empty()) {
        placeholder_buf_.append(placeholder.data(), placeholder.size());
    }
    set_value(PlaceholderProperty, core::Variant{ placeholder_buf_ });
}

bool TextBox::is_read_only() const noexcept
{
    return is_read_only_;
}

void TextBox::set_read_only(bool read_only) noexcept
{
    is_read_only_ = read_only;
    set_value(IsReadOnlyProperty, core::Variant{ is_read_only_ });
    invalidate_render();
}

bool TextBox::is_enabled() const noexcept
{
    return is_enabled_;
}

void TextBox::set_enabled(bool enabled) noexcept
{
    is_enabled_ = enabled;
    if (!is_enabled_) {
        is_hovered_ = false;
        is_focused_ = false;
    }
    update_visual_state();
    invalidate_render();
}

void TextBox::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    invalidate_measure();
    invalidate_render();
}

void TextBox::set_font_size(float size_px) noexcept
{
    font_size_px_ = size_px < 1.0f ? 1.0f : size_px;
    invalidate_measure();
    invalidate_render();
}

// ============================================================================
// 布局
// ============================================================================

math::Size TextBox::measure_override(math::Size available)
{
    // 读取内边距
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };

    // 计算行高（有字体则精确，否则估算）
    const float line_h = effective_line_height();

    // 期望高度 = 上下内边距 + 行高 + 边框厚度（最大 2px）
    const float desired_h = pad.top + pad.bottom + line_h + 2.0f;

    // 期望宽度策略：
    //   - available.width 为有限值（父容器有限宽度）时，填充可用宽度
    //   - available.width 为无穷（StackPanel / 无约束父容器）时，使用最小宽度 120px
    float desired_w;
    if (std::isinf(available.width)) {
        // 自然宽度 = 文字内容宽度 + 内边距，最小 120px
        const float text_w = measure_text_width(text_buf_.data(),
                                                static_cast<uint32_t>(text_buf_.size()));
        desired_w = text_w + pad.left + pad.right;
        if (desired_w < 120.0f) {
            desired_w = 120.0f;
        }
    } else {
        // 填充可用宽度（常见于 Grid 列 / 水平 StackPanel）
        desired_w = available.width;
    }

    // 返回内容期望尺寸（不含 Margin）；FrameworkElement::on_measure 加回 Margin 并调用 set_desired_size
    return { desired_w, desired_h };
}

void TextBox::on_arrange(math::Rect final_rect)
{
    FrameworkElement::on_arrange(final_rect);

    // 设置圆角裁剪区，使内容渲染不超出控件边界
    const core::Variant& cr_var = get_value(CornerRadiusProperty);
    const float radius = cr_var.has<float>() ? cr_var.get<float>() : 4.0f;
    set_clip_rounded_rect(math::RoundedRect{ final_rect, radius });
}

// ============================================================================
// 渲染
// ============================================================================

void TextBox::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    // 读取圆角半径
    const core::Variant& cr_var = get_value(CornerRadiusProperty);
    const float radius = cr_var.has<float>() ? cr_var.get<float>() : 4.0f;
    const math::RoundedRect rrect{ rect, radius };

    // ── 1. 背景填充 ─────────────────────────────────────────────────────────
    const core::Variant& bg_var = get_value(BackgroundProperty);
    const paint::Brush bg = bg_var.has<paint::Brush>()
        ? bg_var.get<paint::Brush>()
        : paint::Brush::solid_rgb(0xFFFBFE);
    canvas.fill_rounded_rect(rrect, bg);

    // ── 2. 边框描边 ─────────────────────────────────────────────────────────
    // 获焦时加粗至 2px（MD3 Filled Text Field 规范），其余状态 1px
    const core::Variant& bc_var = get_value(BorderColorProperty);
    const paint::Brush border_brush = bc_var.has<paint::Brush>()
        ? bc_var.get<paint::Brush>()
        : paint::Brush::solid_rgb(0x79747E);
    if (!border_brush.is_transparent()) {
        paint::Pen pen;
        pen.width = is_focused_ ? 2.0f : 1.0f;
        canvas.stroke_rounded_rect(rrect, border_brush, pen);
    }

    // ── 3. 准备文字绘制区域（减去内边距）──────────────────────────────────
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };

    const float text_x0 = rect.x + pad.left;
    const float text_y0 = rect.y + pad.top;
    const float text_w  = rect.width  - pad.left - pad.right;
    const float text_h  = rect.height - pad.top  - pad.bottom;
    if (text_w <= 0.0f || text_h <= 0.0f) {
        return;
    }

    // 计算文字基线 y（使行在内容区垂直居中）
    const float line_h = effective_line_height();
    float baseline_y = text_y0 + (text_h - line_h) * 0.5f;  // 行顶部 y
    if (font_face_ != nullptr) {
        auto* face = static_cast<text::FontFace*>(font_face_);
        face->set_pixel_size(0u, static_cast<uint32_t>(font_size_px_ + 0.5f));
        baseline_y += static_cast<float>(face->ascender());   // 行顶部 + ascender = 基线
    } else {
        baseline_y += line_h * 0.8f;  // 无字体时估算：ascender ≈ 80% line_height
    }

    // 保存变换状态，裁剪到文字区域防止溢出
    canvas.save();
    canvas.clip_rect({ text_x0, text_y0, text_w, text_h });

    const bool has_text = !text_buf_.empty();

    if (has_text) {
        // ── 4a. 绘制实际文字 ─────────────────────────────────────────────────
        const core::Variant& fg_var = get_value(ForegroundProperty);
        const paint::Brush fg = fg_var.has<paint::Brush>()
            ? fg_var.get<paint::Brush>()
            : paint::Brush::solid_rgb(0x1C1B1F);
        canvas.draw_text(
            core::StringView{ text_buf_.data(), text_buf_.size() },
            { text_x0, baseline_y },
            font_face_,
            font_size_px_,
            fg);
    } else if (!placeholder_buf_.empty()) {
        // ── 4b. 绘制占位文字（无实际文本时显示）────────────────────────────
        const core::Variant& ph_var = get_value(PlaceholderForegroundProperty);
        const paint::Brush ph_fg = ph_var.has<paint::Brush>()
            ? ph_var.get<paint::Brush>()
            : paint::Brush::solid(math::Color{ 0.29f, 0.27f, 0.31f, 0.60f });
        canvas.draw_text(
            core::StringView{ placeholder_buf_.data(), placeholder_buf_.size() },
            { text_x0, baseline_y },
            font_face_,
            font_size_px_,
            ph_fg);
    }

    // ── 5. 绘制插入光标（仅 Focused 且非只读时可见）──────────────────────
    if (is_focused_ && cursor_visible_ && !is_read_only_) {
        // 计算光标 x：光标前方文字的水平宽度
        float cursor_x = text_x0;
        if (cursor_pos_ > 0u && has_text) {
            cursor_x += measure_text_width(text_buf_.data(), cursor_pos_);
        }

        // 光标高度与行高一致，垂直居中于内容区
        const float cursor_top = text_y0 + (text_h - line_h) * 0.5f;
        canvas.fill_rect(
            { cursor_x, cursor_top, 1.5f, line_h },
            paint::Brush::solid_rgb(0x6750A4));  // MD3 Primary
    }

    canvas.restore();
}

// ============================================================================
// 视觉状态
// ============================================================================

ControlVisualState TextBox::compute_visual_state() const
{
    if (!is_enabled_) {
        return ControlVisualState::Disabled;
    }
    if (is_focused_) {
        return ControlVisualState::Focused;
    }
    if (is_hovered_) {
        return ControlVisualState::Hovered;
    }
    return ControlVisualState::Normal;
}

void TextBox::on_visual_state_changed(ControlVisualState old_state,
                                      ControlVisualState new_state)
{
    // 调用基类：
    //   1. 触发 invalidate_render（确保下一帧重绘）
    //   2. 若 VSM 已配置，调用 vsm()->go_to_state(新状态名)
    //      其内部完成：capture_from_values → apply_state(P4) → resolve_and_begin(Storyboard)
    Control::on_visual_state_changed(old_state, new_state);

    // 注册 AnimationClock（幂等）：
    //   - 驱动 VSM::tick_animations(dt)（边框颜色过渡 Storyboard）
    //   - 驱动光标闪烁（is_focused_ 时每 500ms 翻转 cursor_visible_）
    // tick 回调返回 false 时 AnimationClock 自动移除注册项
    animation::AnimationClock::instance().register_animation(this, &TextBox::anim_tick_callback);
}

// ============================================================================
// AnimationClock tick 回调
// ============================================================================

bool TextBox::anim_tick_callback(void* handle, float dt) noexcept
{
    auto* self = static_cast<TextBox*>(handle);
    bool  any_active = false;

    // ── VSM 边框颜色过渡动画 ───────────────────────────────────────────────
    if (auto* v = self->vsm()) {
        if (v->tick_animations(dt)) {
            any_active = true;
        }
    }

    // ── 光标闪烁（仅获焦时）──────────────────────────────────────────────
    if (self->is_focused_) {
        constexpr float kBlinkInterval = 0.5f;  // 500ms 半周期（闪烁周期 1s）
        self->cursor_blink_accum_ += dt;
        if (self->cursor_blink_accum_ >= kBlinkInterval) {
            self->cursor_blink_accum_ -= kBlinkInterval;
            self->cursor_visible_ = !self->cursor_visible_;
            self->invalidate_render();
        }
        // 有焦点时持续保持注册（需要一直驱动光标闪烁）
        any_active = true;
    }

    return any_active;
}

// ============================================================================
// 事件路由静态方法（转发到实例方法）
// ============================================================================

void TextBox::on_mouse_enter_router(void* /*sender*/, RoutedEventArgs& /*args*/, void* ud)
{
    static_cast<TextBox*>(ud)->on_mouse_enter();
}

void TextBox::on_mouse_leave_router(void* /*sender*/, RoutedEventArgs& /*args*/, void* ud)
{
    static_cast<TextBox*>(ud)->on_mouse_leave();
}

void TextBox::on_mouse_down_router(void* /*sender*/, RoutedEventArgs& args, void* ud)
{
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    static_cast<TextBox*>(ud)->on_mouse_down(mouse_args);
}

void TextBox::on_key_down_router(void* /*sender*/, RoutedEventArgs& args, void* ud)
{
    auto& key_args = static_cast<input::KeyEventArgs&>(args);
    static_cast<TextBox*>(ud)->on_key_down(key_args);
}

void TextBox::on_text_input_router(void* /*sender*/, RoutedEventArgs& args, void* ud)
{
    auto& text_args = static_cast<input::TextInputEventArgs&>(args);
    static_cast<TextBox*>(ud)->on_text_input(text_args);
}

void TextBox::on_got_focus_router(void* /*sender*/, RoutedEventArgs& /*args*/, void* ud)
{
    auto* self = static_cast<TextBox*>(ud);
    // 重置光标状态（获焦时光标立即显示，闪烁重新计时）
    self->cursor_visible_      = true;
    self->cursor_blink_accum_  = 0.0f;
    self->is_focused_          = true;
    self->update_visual_state();
}

void TextBox::on_lost_focus_router(void* /*sender*/, RoutedEventArgs& /*args*/, void* ud)
{
    auto* self = static_cast<TextBox*>(ud);
    self->is_focused_  = false;
    self->cursor_visible_ = false;
    self->update_visual_state();
}

// ============================================================================
// 事件处理逻辑
// ============================================================================

void TextBox::on_mouse_enter()
{
    if (!is_enabled_) {
        return;
    }
    is_hovered_ = true;
    update_visual_state();
}

void TextBox::on_mouse_leave()
{
    is_hovered_ = false;
    update_visual_state();
}

void TextBox::on_mouse_down(input::MouseEventArgs& /*args*/)
{
    // 焦点设置由 InputRouter 和 GotFocusEvent 负责，此处无需额外操作。
    // 后续可在此实现鼠标点击定位光标位置。
}

void TextBox::on_key_down(input::KeyEventArgs& args)
{
    if (!is_focused_) {
        return;
    }

    const uint32_t sz = static_cast<uint32_t>(text_buf_.size());

    switch (args.key()) {
    case input::Key::Left:
        move_cursor_left();
        // 光标移动后立即显示（重置闪烁计时）
        cursor_visible_     = true;
        cursor_blink_accum_ = 0.0f;
        invalidate_render();
        args.set_handled(true);
        break;

    case input::Key::Right:
        move_cursor_right();
        cursor_visible_     = true;
        cursor_blink_accum_ = 0.0f;
        invalidate_render();
        args.set_handled(true);
        break;

    case input::Key::Home:
        cursor_pos_         = 0u;
        cursor_visible_     = true;
        cursor_blink_accum_ = 0.0f;
        invalidate_render();
        args.set_handled(true);
        break;

    case input::Key::End:
        cursor_pos_         = sz;
        cursor_visible_     = true;
        cursor_blink_accum_ = 0.0f;
        invalidate_render();
        args.set_handled(true);
        break;

    case input::Key::Backspace:
        if (!is_read_only_) {
            delete_char_before();
            args.set_handled(true);
        }
        break;

    case input::Key::Delete:
        if (!is_read_only_) {
            delete_char_after();
            args.set_handled(true);
        }
        break;

    default:
        break;
    }
}

void TextBox::on_text_input(input::TextInputEventArgs& args)
{
    if (!is_focused_ || is_read_only_) {
        return;
    }
    // 过滤控制字符（码点 < 32 由 KeyDown 处理，如 Backspace=0x08 / Tab=0x09 / Enter=0x0D 等）
    if (args.char_utf32() < 32u) {
        return;
    }
    insert_char(args.char_utf32());
    args.set_handled(true);
}

// ============================================================================
// UTF-8 光标操作
// ============================================================================

void TextBox::insert_char(uint32_t char_utf32)
{
    char utf8_bytes[4]{};
    const uint32_t byte_count = utf32_to_utf8(char_utf32, utf8_bytes);
    const uint32_t sz  = static_cast<uint32_t>(text_buf_.size());
    const uint32_t pos = cursor_pos_;

    // 构建新字符串：光标前内容 + 新字符 + 光标后内容
    containers::InlineString new_buf;
    new_buf.reserve(sz + byte_count);
    new_buf.append(text_buf_.data(), pos);
    new_buf.append(utf8_bytes, byte_count);
    new_buf.append(text_buf_.data() + pos, sz - pos);
    text_buf_   = std::move(new_buf);
    cursor_pos_ = pos + byte_count;

    // 同步 DP 并派发 TextChangedEvent
    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed{ TextChangedEvent() };
    EventManager::raise(*this, changed);

    // 插入后光标立即显示（重置闪烁计时）
    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    invalidate_render();
}

void TextBox::delete_char_before()
{
    if (cursor_pos_ == 0u) {
        return;
    }
    const uint32_t prev_pos = utf8_prev(text_buf_.data(), cursor_pos_);
    const uint32_t sz = static_cast<uint32_t>(text_buf_.size());

    // 构建新字符串：删除 [prev_pos, cursor_pos_) 区间
    containers::InlineString new_buf;
    new_buf.reserve(sz - (cursor_pos_ - prev_pos));
    new_buf.append(text_buf_.data(), prev_pos);
    new_buf.append(text_buf_.data() + cursor_pos_, sz - cursor_pos_);
    text_buf_   = std::move(new_buf);
    cursor_pos_ = prev_pos;

    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed{ TextChangedEvent() };
    EventManager::raise(*this, changed);

    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    invalidate_render();
}

void TextBox::delete_char_after()
{
    const uint32_t sz = static_cast<uint32_t>(text_buf_.size());
    if (cursor_pos_ >= sz) {
        return;
    }
    const uint32_t next_pos = utf8_next(text_buf_.data(), cursor_pos_, sz);

    // 构建新字符串：删除 [cursor_pos_, next_pos) 区间
    containers::InlineString new_buf;
    new_buf.reserve(sz - (next_pos - cursor_pos_));
    new_buf.append(text_buf_.data(), cursor_pos_);
    new_buf.append(text_buf_.data() + next_pos, sz - next_pos);
    text_buf_ = std::move(new_buf);
    // cursor_pos_ 不变（光标位置不随 Delete 移动）

    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed{ TextChangedEvent() };
    EventManager::raise(*this, changed);

    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    invalidate_render();
}

void TextBox::move_cursor_left()
{
    cursor_pos_ = utf8_prev(text_buf_.data(), cursor_pos_);
}

void TextBox::move_cursor_right()
{
    const uint32_t sz = static_cast<uint32_t>(text_buf_.size());
    cursor_pos_ = utf8_next(text_buf_.data(), cursor_pos_, sz);
}

// ============================================================================
// 私有辅助
// ============================================================================

float TextBox::effective_line_height() const noexcept
{
    if (font_face_ != nullptr) {
        auto* face = static_cast<text::FontFace*>(font_face_);
        // 设置字号使 ascender/descender/line_height 度量值有效
        face->set_pixel_size(0u, static_cast<uint32_t>(font_size_px_ + 0.5f));
        return static_cast<float>(face->line_height());
    }
    // 无字体时按 1.4 倍字号估算行高
    return font_size_px_ * 1.4f;
}

float TextBox::measure_text_width(const char* utf8, uint32_t len) const noexcept
{
    if (len == 0u) {
        return 0.0f;
    }
    if (font_face_ != nullptr) {
        auto* face = static_cast<text::FontFace*>(font_face_);
        return face->measure_text(utf8, len, font_size_px_);
    }
    // 无字体时按字符数估算宽度（等宽近似：0.55 倍字号）
    uint32_t char_count = 0u;
    for (uint32_t i = 0u; i < len;) {
        i = utf8_next(utf8, i, len);
        ++char_count;
    }
    return static_cast<float>(char_count) * font_size_px_ * 0.55f;
}

} // namespace mine::ui
