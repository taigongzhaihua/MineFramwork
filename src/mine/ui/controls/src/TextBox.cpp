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

#include <mine/ui/window/Window.h>
#include <mine/platform/IMEService.h>
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
#include <string>  // std::string（剪贴板 UTF-16 转换用）

// ── Win32 剪贴板支持 ─────────────────────────────────────────────────────────────────
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#  define NOMINMAX
#  endif
#  include <windows.h>
#endif  // _WIN32

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
// 多行文本辅助（行分割与查找）
// ============================================================================

/// UTF-8 下一字符边界
static uint32_t next_utf8_boundary(const char* data, uint32_t pos, uint32_t end) noexcept
{
    if (pos >= end) return end;
    const auto c = static_cast<uint8_t>(data[pos]);
    uint32_t skip = 1u;
    if      ((c & 0xF8u) == 0xF0u) { skip = 4u; }
    else if ((c & 0xF0u) == 0xE0u) { skip = 3u; }
    else if ((c & 0xE0u) == 0xC0u) { skip = 2u; }
    const uint32_t next = pos + skip;
    return next <= end ? next : end;
}

/// 行信息：起始字节偏移和字节长度
struct LineInfo {
    uint32_t start_offset;  ///< 行起始字节偏移
    uint32_t byte_length;   ///< 行字节长度（不含换行符 \n）
    uint32_t disp_length;   ///< 实际显示长度（可能被截断，用于省略号等）
    float    disp_width;    ///< 显示宽度（像素）
};

/// 将文本按 \n 和可用宽度分割为行数组（支持自动换行）
/// @param text 文本指针
/// @param text_len 文本字节长度
/// @param max_width 最大宽度（像素），0 = 无限制
/// @param font_face FontFace 指针
/// @param font_size 字号
/// @param wrapping 换行模式
static containers::Vector<LineInfo> split_lines(
    const char* text,
    size_t text_len,
    float max_width,
    void* font_face,
    float font_size,
    TextWrapping wrapping)
{
    containers::Vector<LineInfo> lines;
    
    // 是否启用宽度自动折叠
    const bool use_width_limit = (wrapping != TextWrapping::NoWrap)
                                  && (max_width > 0.0f)
                                  && (max_width < 1.0e9f);

    // 按字符折叠的辅助 lambda
    auto measure_segment = [&](const char* p, uint32_t len) -> float {
        if (len == 0u) return 0.0f;
        if (font_face != nullptr) {
            auto* face = static_cast<text::FontFace*>(font_face);
            return face->measure_text(p, len, font_size);
        }
        // 无字体：估算
        return static_cast<float>(len) * font_size * 0.5f;
    };

    auto append_char_wrap = [&](uint32_t seg_start, uint32_t seg_end) {
        if (seg_start == seg_end) {
            // 空段落保留一个空行
            lines.push_back({seg_start, 0u, 0u, 0.0f});
            return;
        }
        uint32_t pos = seg_start;
        while (pos < seg_end) {
            // 找最长不超过 max_width 的前缀
            uint32_t line_end = pos;
            float    accum_w  = 0.0f;

            while (line_end < seg_end) {
                const uint32_t next_b = next_utf8_boundary(text, line_end, seg_end);
                const float    cw     = measure_segment(text + line_end, next_b - line_end);
                // 首字符强制放入，后续若超出则停止
                if (line_end > pos && accum_w + cw > max_width) break;
                accum_w  += cw;
                line_end  = next_b;
            }
            // 保证至少放入一个字符（防死循环）
            if (line_end == pos) {
                line_end = next_utf8_boundary(text, pos, seg_end);
                accum_w  = measure_segment(text + pos, line_end - pos);
            }

            lines.push_back({pos, line_end - pos, line_end - pos, accum_w});
            pos = line_end;
        }
    };

    // 按 '\n' 将文本分割为段落，逐段构建行
    uint32_t seg_start   = 0u;
    bool     last_was_nl = false;

    for (uint32_t i = 0u; i <= text_len; ++i) {
        const bool is_nl  = (i < text_len && text[i] == '\n');
        const bool is_end = (i == text_len);
        if (!is_nl && !is_end) continue;

        const uint32_t seg_end = i;
        last_was_nl = is_nl;

        if (!use_width_limit) {
            // NoWrap：整段作为一行
            const float w = measure_segment(text + seg_start, seg_end - seg_start);
            lines.push_back({seg_start, seg_end - seg_start, seg_end - seg_start, w});
        } else {
            // Wrap：字符边界折叠（暂不支持 WrapAtWord）
            append_char_wrap(seg_start, seg_end);
        }

        seg_start = i + 1u;  // 跳过 '\n'
    }

    // 文本以 '\n' 结尾时，追加一个空行
    if (last_was_nl) {
        lines.push_back({static_cast<uint32_t>(text_len), 0u, 0u, 0.0f});
    }

    // 保证至少一行（空文本）
    if (lines.empty()) {
        lines.push_back({0u, 0u, 0u, 0.0f});
    }

    return lines;
}

/// 根据字节偏移查找所在行号和行内偏移
static bool find_line_by_offset(const containers::Vector<LineInfo>& lines,
                                 uint32_t offset,
                                 uint32_t* out_line_idx,
                                 uint32_t* out_line_offset)
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(lines.size()); ++i) {
        const uint32_t line_start = lines[i].start_offset;
        const uint32_t line_end   = line_start + lines[i].disp_length;  // 使用 disp_length
        // 允许光标在行末尾（line_end，不含 \n 的位置）
        if (offset >= line_start && offset <= line_end) {
            *out_line_idx    = i;
            *out_line_offset = offset - line_start;
            return true;
        }
    }
    return false;  // 不应发生（offset 超出文本范围）
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

// AcceptsReturnProperty — 接受回车键（bool，默认 false，单行模式）
const DependencyProperty& TextBox::AcceptsReturnProperty =
    register_property<TextBox>(
        "AcceptsReturn",
        core::Variant{ false },
        PropertyMetadata{
            .affects_measure = true,  // 多行模式影响高度测量
            .affects_render  = true,  // 影响行布局和渲染
        });

// TextWrappingProperty — 自动换行模式（TextWrapping，默认 Wrap）
const DependencyProperty& TextBox::TextWrappingProperty =
    register_property<TextBox>(
        "TextWrapping",
        core::Variant{ TextWrapping::Wrap },
        PropertyMetadata{
            .affects_measure = true,  // 换行模式影响高度测量
            .affects_render  = true,  // 影响行布局和渲染
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
    add_handler(input::MouseMoveEvent(),  &TextBox::on_mouse_move_router,  this);
    add_handler(input::MouseUpEvent(),    &TextBox::on_mouse_up_router,    this);
    add_handler(input::MouseWheelEvent(), &TextBox::on_mouse_wheel_router, this);
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

core::StringView TextBox::selected_text() const noexcept
{
    if (!has_selection()) {
        return {};
    }
    const uint32_t start = sel_start();
    const uint32_t end   = sel_end();
    return { text_buf_.data() + start, static_cast<size_t>(end - start) };
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
    sel_anchor_ = cursor_pos_;  // 设置文字后取消选区
    // 同步 DP（affects_measure/affects_render=true → 自动触发重新测量和重绘）
    set_value(TextProperty, core::Variant{ text_buf_ });
    // 自动滚动到文本末尾
    auto_scroll_to_cursor();
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

bool TextBox::accepts_return() const noexcept
{
    const core::Variant& v = get_value(AcceptsReturnProperty);
    return v.has<bool>() ? v.get<bool>() : false;
}

void TextBox::set_accepts_return(bool accepts) noexcept
{
    set_value(AcceptsReturnProperty, core::Variant{ accepts });
}

TextWrapping TextBox::text_wrapping() const noexcept
{
    const core::Variant& v = get_value(TextWrappingProperty);
    return v.has<TextWrapping>() ? v.get<TextWrapping>() : TextWrapping::Wrap;
}

void TextBox::set_text_wrapping(TextWrapping mode) noexcept
{
    set_value(TextWrappingProperty, core::Variant{ mode });
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

    // 自动换行布局默认开启：
    // - TextWrapping != NoWrap 时使用多行布局（即使 AcceptsReturn=false）
    // - AcceptsReturn=true 时也使用多行布局（支持硬换行）
    const auto wrapping = text_wrapping();
    const bool use_multiline_layout = accepts_return() || (wrapping != TextWrapping::NoWrap);

    // 计算期望高度
    float desired_h;
    if (use_multiline_layout) {
        // 多行模式：计算实际行数（考虑自动换行）
        const float text_area_w = std::isinf(available.width) 
            ? 1000.0f  // 无限宽度时使用较大默认值
            : (available.width - pad.left - pad.right);

        const auto lines = split_lines(
            text_buf_.data(), text_buf_.size(),
            text_area_w,
            font_face_, font_size_px_,
            wrapping);
        
        const float content_h = static_cast<float>(lines.size()) * line_h;
        desired_h = pad.top + pad.bottom + content_h + 2.0f;  // +2 边框厚度
    } else {
        // 单行模式：固定单行高度
        desired_h = pad.top + pad.bottom + line_h + 2.0f;
    }

    // 期望宽度策略：
    //   - available.width 为有限值（父容器有限宽度）时，填充可用宽度
    //   - available.width 为无穷（StackPanel / 无约束父容器）时，使用最小宽度 120px
    float desired_w;
    if (std::isinf(available.width)) {
        // 自然宽度 = 文字内容宽度 + 内边距，最小 120px
        float max_line_w = 0.0f;
        if (use_multiline_layout) {
            // 多行模式：NoWrap 时取最长行，Wrap 时使用默认宽度
            if (wrapping == TextWrapping::NoWrap && !text_buf_.empty()) {
                const auto lines = split_lines(
                    text_buf_.data(), text_buf_.size(),
                    0.0f,
                    nullptr, 0.0f,
                    TextWrapping::NoWrap);
                for (const auto& line : lines) {
                    const float w = measure_text_width(text_buf_.data() + line.start_offset, line.byte_length);
                    if (w > max_line_w) max_line_w = w;
                }
            } else {
                max_line_w = 200.0f;  // Wrap 模式默认宽度
            }
        } else {
            max_line_w = measure_text_width(text_buf_.data(), static_cast<uint32_t>(text_buf_.size()));
        }
        desired_w = max_line_w + pad.left + pad.right;
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

    // 每次渲染前确保滚动偏移在有效范围内
    clamp_scroll_offsets();

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

    // 自动换行布局默认开启：
    // - TextWrapping != NoWrap 时使用多行布局（支持自动折行）
    // - AcceptsReturn=true 时允许 Enter 插入硬换行
    const bool use_multiline_layout = accepts_return() || (text_wrapping() != TextWrapping::NoWrap);

    // ────────────────────────────────────────────────────────────────────────
    // 单行模式快速路径
    // ────────────────────────────────────────────────────────────────────────
    if (!use_multiline_layout) {
        // ── 4a-pre. 选择高亮（在文字下方渲染）──────────────────────────────
        if (is_focused_ && has_selection() && has_text) {
            const float x_sel0   = text_x0 - scroll_offset_x_ + measure_text_width(text_buf_.data(), sel_start());
            const float x_sel1   = text_x0 - scroll_offset_x_ + measure_text_width(text_buf_.data(), sel_end());
            const float sel_top  = text_y0 + (text_h - line_h) * 0.5f;
            canvas.fill_rect(
                { x_sel0, sel_top, x_sel1 - x_sel0, line_h },
                paint::Brush::solid(math::Color{ 0.404f, 0.314f, 0.643f, 0.30f }));
        }

        if (has_text) {
            // ── 4a. 绘制实际文字 ─────────────────────────────────────────────
            const core::Variant& fg_var = get_value(ForegroundProperty);
            const paint::Brush fg = fg_var.has<paint::Brush>()
                ? fg_var.get<paint::Brush>()
                : paint::Brush::solid_rgb(0x1C1B1F);
            canvas.draw_text(
                core::StringView{ text_buf_.data(), text_buf_.size() },
                { text_x0 - scroll_offset_x_, baseline_y },
                font_face_,
                font_size_px_,
                fg);
        } else if (!placeholder_buf_.empty()) {
            // ── 4b. 绘制占位文字 ─────────────────────────────────────────────
            const core::Variant& ph_var = get_value(PlaceholderForegroundProperty);
            const paint::Brush ph_fg = ph_var.has<paint::Brush>()
                ? ph_var.get<paint::Brush>()
                : paint::Brush::solid(math::Color{ 0.29f, 0.27f, 0.31f, 0.60f });
            canvas.draw_text(
                core::StringView{ placeholder_buf_.data(), placeholder_buf_.size() },
                { text_x0 - scroll_offset_x_, baseline_y },
                font_face_,
                font_size_px_,
                ph_fg);
        }

        // ── 5. 绘制插入光标 ──────────────────────────────────────────────────
        if (is_focused_ && (cursor_visible_ || is_read_only_)) {
            float cursor_x = text_x0 - scroll_offset_x_;
            if (cursor_pos_ > 0u && has_text) {
                cursor_x += measure_text_width(text_buf_.data(), cursor_pos_);
            }
            const float cursor_top = text_y0 + (text_h - line_h) * 0.5f;
            canvas.fill_rect(
                { cursor_x, cursor_top, 1.5f, line_h },
                paint::Brush::solid_rgb(0x6750A4));
        }
    }
    // ────────────────────────────────────────────────────────────────────────
    // 多行模式
    // ────────────────────────────────────────────────────────────────────────
    else {
        const float text_area_w = rect.width - pad.left - pad.right;
        const auto wrapping = text_wrapping();
        const auto lines = split_lines(
            text_buf_.data(), text_buf_.size(),
            text_area_w,
            font_face_, font_size_px_,
            wrapping);

        if (!has_text && !placeholder_buf_.empty()) {
            // 无文字时显示占位文字（单行，不分割）
            const core::Variant& ph_var = get_value(PlaceholderForegroundProperty);
            const paint::Brush ph_fg = ph_var.has<paint::Brush>()
                ? ph_var.get<paint::Brush>()
                : paint::Brush::solid(math::Color{ 0.29f, 0.27f, 0.31f, 0.60f });
            canvas.draw_text(
                core::StringView{ placeholder_buf_.data(), placeholder_buf_.size() },
                { text_x0 - scroll_offset_x_, baseline_y },
                font_face_,
                font_size_px_,
                ph_fg);
        }

        if (has_text) {
            // ── 选择高亮（逐行计算）──────────────────────────────────────────
            if (is_focused_ && has_selection()) {
                const uint32_t sel_s = sel_start();
                const uint32_t sel_e = sel_end();
                for (uint32_t i = 0; i < static_cast<uint32_t>(lines.size()); ++i) {
                    const LineInfo& line = lines[i];
                    const uint32_t line_end = line.start_offset + line.byte_length;
                    // 该行与选区有交集
                    if (sel_s < line_end && sel_e > line.start_offset) {
                        const uint32_t hl_start = sel_s > line.start_offset ? sel_s : line.start_offset;
                        const uint32_t hl_end   = sel_e < line_end ? sel_e : line_end;
                        const char*    line_text = text_buf_.data() + line.start_offset;
                        const float    x0 = text_x0 - scroll_offset_x_ + measure_text_width(line_text, hl_start - line.start_offset);
                        const float    x1 = text_x0 - scroll_offset_x_ + measure_text_width(line_text, hl_end   - line.start_offset);
                        const float    y  = text_y0 + static_cast<float>(i) * line_h - scroll_offset_y_;
                        canvas.fill_rect(
                            { x0, y, x1 - x0, line_h },
                            paint::Brush::solid(math::Color{ 0.404f, 0.314f, 0.643f, 0.30f }));
                    }
                }
            }

            // ── 绘制文字（逐行）──────────────────────────────────────────────
            const core::Variant& fg_var = get_value(ForegroundProperty);
            const paint::Brush fg = fg_var.has<paint::Brush>()
                ? fg_var.get<paint::Brush>()
                : paint::Brush::solid_rgb(0x1C1B1F);

            float line_baseline_y = baseline_y;  // 单行时的基线 y
            // 多行从顶部开始（不垂直居中）
            if (font_face_ != nullptr) {
                auto* face = static_cast<text::FontFace*>(font_face_);
                line_baseline_y = text_y0 + static_cast<float>(face->ascender());
            } else {
                line_baseline_y = text_y0 + line_h * 0.8f;
            }

            for (uint32_t i = 0; i < static_cast<uint32_t>(lines.size()); ++i) {
                const LineInfo& line = lines[i];
                if (line.byte_length > 0) {
                    canvas.draw_text(
                        core::StringView{ text_buf_.data() + line.start_offset, line.byte_length },
                        { text_x0 - scroll_offset_x_, line_baseline_y + static_cast<float>(i) * line_h - scroll_offset_y_ },
                        font_face_,
                        font_size_px_,
                        fg);
                }
            }

        }

        // ── 绘制光标（多行，空文本时也可见）────────────────────────────────
        if (is_focused_ && (cursor_visible_ || is_read_only_)) {
            uint32_t cursor_line_idx = 0u;
            uint32_t cursor_line_offset = 0u;
            if (find_line_by_offset(lines, cursor_pos_, &cursor_line_idx, &cursor_line_offset)) {
                const LineInfo& cursor_line = lines[cursor_line_idx];
                const char* line_text = text_buf_.data() + cursor_line.start_offset;
                const float cursor_x = text_x0 - scroll_offset_x_ + measure_text_width(line_text, cursor_line_offset);
                const float cursor_y = text_y0 + static_cast<float>(cursor_line_idx) * line_h - scroll_offset_y_;
                canvas.fill_rect(
                    { cursor_x, cursor_y, 1.5f, line_h },
                    paint::Brush::solid_rgb(0x6750A4));
            }
        }
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

    // ── 光标闪烁（仅获焦且非只读时闪烁）──────────────────────────────
    if (self->is_focused_) {
        if (!self->is_read_only_) {
            // 只读模式下光标常亮，不需要闪烁
            constexpr float kBlinkInterval = 0.5f;  // 500ms 半周期（闪烁周期 1s）
            self->cursor_blink_accum_ += dt;
            if (self->cursor_blink_accum_ >= kBlinkInterval) {
                self->cursor_blink_accum_ -= kBlinkInterval;
                self->cursor_visible_ = !self->cursor_visible_;
                self->invalidate_render();
            }
        }
        // 有焦点时持续保持注册（需要一直驱动光标闪烁和 VSM 动画）
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

void TextBox::on_mouse_move_router(void* /*sender*/, RoutedEventArgs& args, void* ud)
{
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    static_cast<TextBox*>(ud)->on_mouse_move(mouse_args);
}

void TextBox::on_mouse_up_router(void* /*sender*/, RoutedEventArgs& args, void* ud)
{
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    static_cast<TextBox*>(ud)->on_mouse_up(mouse_args);
}

void TextBox::on_mouse_wheel_router(void* /*sender*/, RoutedEventArgs& args, void* ud)
{
    auto& mouse_args = static_cast<input::MouseEventArgs&>(args);
    static_cast<TextBox*>(ud)->on_mouse_wheel(mouse_args);
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

    // 启用 IME 并设置候选框位置
    if (Window* win = self->get_window()) {
        try {
            math::Rect cursor_rect = self->get_cursor_rect_in_window();
            win->ime().enable(cursor_rect);
        } catch (...) {
            // 防止 IME 调用异常导致程序崩溃
        }
    }
}

void TextBox::on_lost_focus_router(void* /*sender*/, RoutedEventArgs& /*args*/, void* ud)
{
    auto* self = static_cast<TextBox*>(ud);
    self->is_focused_  = false;
    self->is_mouse_selecting_ = false;
    self->cursor_visible_ = false;
    self->update_visual_state();

    // 禁用 IME
    if (Window* win = self->get_window()) {
        try {
            win->ime().disable();
        } catch (...) {
            // 防止 IME 调用异常导致程序崩溃
        }
    }
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

void TextBox::on_mouse_down(input::MouseEventArgs& args)
{
    if (args.button() != input::MouseButton::Left) {
        return;
    }

    // 焦点切换已由 InputRouter 的 GotFocusEvent 处理

    // 计算文字区域起始 x/y（bounds_rect 和 position() 同为窗口根坐标系）
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };

    const float text_x0    = bounds_rect().x + pad.left;
    const float text_y0    = bounds_rect().y + pad.top;
    // 点击坐标需加回滚动偏移（因文字渲染时已减去 scroll_offset_）
    const float click_rel_x = args.position().x - text_x0 + scroll_offset_x_;
    const float click_rel_y = args.position().y - text_y0 + scroll_offset_y_;

    const bool use_multiline_layout = accepts_return() || (text_wrapping() != TextWrapping::NoWrap);

    uint32_t new_pos;
    if (use_multiline_layout) {
        // 多行模式：使用 xy 定位
        new_pos = cursor_pos_from_xy(
            click_rel_x < 0.0f ? 0.0f : click_rel_x,
            click_rel_y < 0.0f ? 0.0f : click_rel_y);
    } else {
        // 单行模式：仅用 x 定位
        new_pos = cursor_pos_from_x(click_rel_x < 0.0f ? 0.0f : click_rel_x);
    }

    if (args.shift()) {
        // Shift+Click：从现有 sel_anchor_ 延伸选择到点击位置
        cursor_pos_ = new_pos;
    } else {
        // 普通点击：取消选择，光标移到点击位置
        cursor_pos_ = new_pos;
        sel_anchor_ = new_pos;
    }

    cursor_target_x_    = 0.0f;  // 鼠标点击后重置目标 x
    is_mouse_selecting_ = true;
    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    update_ime_position();
    auto_scroll_to_cursor();
    invalidate_render();
    args.set_handled(true);
}

void TextBox::on_mouse_move(input::MouseEventArgs& args)
{
    if (!is_mouse_selecting_) {
        return;
    }

    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };

    const float text_x0 = bounds_rect().x + pad.left;
    const float text_y0 = bounds_rect().y + pad.top;
    // 点击坐标需加回滚动偏移（因文字渲染时已减去 scroll_offset_）
    const float click_rel_x = args.position().x - text_x0 + scroll_offset_x_;
    const float click_rel_y = args.position().y - text_y0 + scroll_offset_y_;
    const bool use_multiline_layout = accepts_return() || (text_wrapping() != TextWrapping::NoWrap);

    uint32_t new_pos;
    if (use_multiline_layout) {
        new_pos = cursor_pos_from_xy(
            click_rel_x < 0.0f ? 0.0f : click_rel_x,
            click_rel_y < 0.0f ? 0.0f : click_rel_y);
    } else {
        new_pos = cursor_pos_from_x(click_rel_x < 0.0f ? 0.0f : click_rel_x);
    }

    cursor_pos_ = new_pos;
    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    update_ime_position();
    auto_scroll_to_cursor();
    invalidate_render();
    args.set_handled(true);
}

void TextBox::on_mouse_up(input::MouseEventArgs& args)
{
    if (args.button() == input::MouseButton::Left && is_mouse_selecting_) {
        is_mouse_selecting_ = false;
        args.set_handled(true);
    }
}

void TextBox::on_mouse_wheel(input::MouseEventArgs& args)
{
    const float delta = args.wheel_delta();
    if (delta == 0.0f) {
        return;
    }

    const math::Size tas = text_area_size();
    const float content_w = total_content_width();
    const float content_h = total_content_height();
    const bool has_h_scroll = (content_w > tas.width  && tas.width  > 0.0f);
    const bool has_v_scroll = (content_h > tas.height && tas.height > 0.0f);

    // 无需滚动时不做处理，让父级有机会处理滚轮事件
    if (!has_h_scroll && !has_v_scroll) {
        return;
    }

    const float line_h = effective_line_height();
    // 每格滚轮（WHEEL_DELTA = 120）滚动的像素量
    constexpr float kScrollLinesPerWheelDelta = 3.0f;
    const float scroll_amount = (delta / 120.0f) * line_h * kScrollLinesPerWheelDelta;

    // ── 滚动方向决策 ───────────────────────────────────────────────────────
    // Shift + 滚轮：强制横向滚动
    // 普通滚轮：
    //   1. 有纵向溢出 → 纵向滚动（常规行为）
    //   2. 无纵向溢出但有横向溢出 → 自动重映射为横向滚动（单行 NoWrap 场景）
    if (args.shift()) {
        scroll_offset_x_ -= scroll_amount;
    } else if (has_v_scroll) {
        scroll_offset_y_ -= scroll_amount;
    } else {
        scroll_offset_x_ -= scroll_amount;
    }

    clamp_scroll_offsets();
    invalidate_render();
    args.set_handled(true);
}

void TextBox::on_key_down(input::KeyEventArgs& args)
{
    if (!is_focused_) {
        return;
    }

    const uint32_t sz    = static_cast<uint32_t>(text_buf_.size());
    const bool     shift = args.shift();
    const bool     ctrl  = args.ctrl();
    const bool     use_multiline_layout = accepts_return() || (text_wrapping() != TextWrapping::NoWrap);

    // 光标移动后立即显示（重置闪烁计时）
    auto reset_blink = [this]() {
        cursor_visible_     = true;
        cursor_blink_accum_ = 0.0f;
    };
    // 非 Shift 时取消选择（将 sel_anchor_ 置于光标新位置）
    auto commit_move = [this, shift]() {
        if (!shift) { sel_anchor_ = cursor_pos_; }
    };

    switch (args.key()) {
    case input::Key::Left:
        if (has_selection() && !shift) {
            // 有选区且无 Shift：光标跳到选区左端，取消选择
            cursor_pos_ = sel_start();
            sel_anchor_ = cursor_pos_;
        } else {
            move_cursor_left();
            commit_move();
        }
        reset_blink(); update_ime_position(); auto_scroll_to_cursor(); invalidate_render(); args.set_handled(true);
        break;

    case input::Key::Right:
        if (has_selection() && !shift) {
            // 有选区且无 Shift：光标跳到选区右端，取消选择
            cursor_pos_ = sel_end();
            sel_anchor_ = cursor_pos_;
        } else {
            move_cursor_right();
            commit_move();
        }
        reset_blink(); update_ime_position(); auto_scroll_to_cursor(); invalidate_render(); args.set_handled(true);
        break;

    case input::Key::Home:
        cursor_pos_ = 0u;
        commit_move();
        reset_blink(); update_ime_position(); auto_scroll_to_cursor(); invalidate_render(); args.set_handled(true);
        break;

    case input::Key::End:
        cursor_pos_ = sz;
        commit_move();
        reset_blink(); update_ime_position(); auto_scroll_to_cursor(); invalidate_render(); args.set_handled(true);
        break;

    case input::Key::Up:
        // 多行模式：向上移动一行
        if (use_multiline_layout) {
            move_cursor_up();
            commit_move();
            reset_blink(); update_ime_position(); auto_scroll_to_cursor(); invalidate_render(); args.set_handled(true);
        }
        break;

    case input::Key::Down:
        // 多行模式：向下移动一行
        if (use_multiline_layout) {
            move_cursor_down();
            commit_move();
            reset_blink(); update_ime_position(); auto_scroll_to_cursor(); invalidate_render(); args.set_handled(true);
        }
        break;

    case input::Key::Enter:
        // 多行模式：插入换行符 \n
        if (accepts_return() && !is_read_only_) {
            insert_char('\n');
            args.set_handled(true);
        }
        break;

    case input::Key::Backspace:
        if (!is_read_only_) {
            if (has_selection()) {
                delete_selection();
            } else {
                delete_char_before();
            }
            args.set_handled(true);
        }
        break;

    case input::Key::Delete:
        if (!is_read_only_) {
            if (has_selection()) {
                delete_selection();
            } else {
                delete_char_after();
            }
            args.set_handled(true);
        }
        break;

    case input::Key::A:
        // Ctrl+A：全选文字
        if (ctrl) {
            sel_anchor_ = 0u;
            cursor_pos_ = sz;
            reset_blink(); update_ime_position(); invalidate_render(); args.set_handled(true);
        }
        break;

    case input::Key::C:
        // Ctrl+C：复制选中文字（无选区则复制全部）
        if (ctrl) {
            copy_to_clipboard();
            args.set_handled(true);
        }
        break;

    case input::Key::V:
        // Ctrl+V：粘贴（只读模式禁止）
        if (ctrl && !is_read_only_) {
            paste_from_clipboard();
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
    // 若当前有选区，先删除选区内容（以新字符替换选区）
    if (has_selection()) {
        delete_selection();
    }

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
    sel_anchor_ = cursor_pos_;  // 插入后取消选择

    // 同步 DP 并派发 TextChangedEvent
    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed{ TextChangedEvent() };
    EventManager::raise(*this, changed);

    // 插入后光标立即显示（重置闪烁计时）
    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    auto_scroll_to_cursor();
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
    sel_anchor_ = cursor_pos_;  // 删除后取消选择

    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed1{ TextChangedEvent() };
    EventManager::raise(*this, changed1);

    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    update_ime_position();
    auto_scroll_to_cursor();
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
    sel_anchor_ = cursor_pos_;  // 删除后取消选择

    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed{ TextChangedEvent() };
    EventManager::raise(*this, changed);

    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    update_ime_position();
    auto_scroll_to_cursor();
    invalidate_render();
}

void TextBox::move_cursor_left()
{
    cursor_pos_      = utf8_prev(text_buf_.data(), cursor_pos_);
    cursor_target_x_ = 0.0f;  // 水平移动，重置目标 x（下次垂直移动时重新计算）
}

void TextBox::move_cursor_right()
{
    const uint32_t sz = static_cast<uint32_t>(text_buf_.size());
    cursor_pos_      = utf8_next(text_buf_.data(), cursor_pos_, sz);
    cursor_target_x_ = 0.0f;  // 水平移动，重置目标 x
    update_ime_position();
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

// ============================================================================
// 选择区间辅助方法
// ============================================================================

bool TextBox::has_selection() const noexcept
{
    return cursor_pos_ != sel_anchor_;
}

uint32_t TextBox::sel_start() const noexcept
{
    return cursor_pos_ < sel_anchor_ ? cursor_pos_ : sel_anchor_;
}

uint32_t TextBox::sel_end() const noexcept
{
    return cursor_pos_ < sel_anchor_ ? sel_anchor_ : cursor_pos_;
}

void TextBox::delete_selection()
{
    if (!has_selection()) {
        return;
    }
    const uint32_t start = sel_start();
    const uint32_t end   = sel_end();
    const uint32_t sz    = static_cast<uint32_t>(text_buf_.size());

    // 构建新字符串：删除 [start, end) 区间
    containers::InlineString new_buf;
    new_buf.reserve(sz - (end - start));
    new_buf.append(text_buf_.data(), start);
    new_buf.append(text_buf_.data() + end, sz - end);
    text_buf_   = std::move(new_buf);
    cursor_pos_ = start;
    sel_anchor_ = start;  // 删除后无选择

    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed{ TextChangedEvent() };
    EventManager::raise(*this, changed);

    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    update_ime_position();
    auto_scroll_to_cursor();
    invalidate_render();
}

uint32_t TextBox::cursor_pos_from_x(float click_x) const noexcept
{
    const uint32_t len = static_cast<uint32_t>(text_buf_.size());
    if (click_x <= 0.0f || len == 0u) {
        return 0u;
    }

    uint32_t i = 0;
    while (i < len) {
        const uint32_t next     = utf8_next(text_buf_.data(), i, len);
        const float    x_before = measure_text_width(text_buf_.data(), i);
        const float    x_after  = measure_text_width(text_buf_.data(), next);
        const float    midpoint = (x_before + x_after) * 0.5f;
        if (click_x < midpoint) {
            return i;
        }
        i = next;
    }
    // 点击在最后一个字符右半边或文字末尾之后
    return len;
}

// ============================================================================
// 剪贴板操作（Win32 API 直接实现）
// ============================================================================

void TextBox::copy_to_clipboard() const
{
    // 有选区则复制选中内容，无选区则复制全部文字
    const char* data;
    size_t      len;
    if (has_selection()) {
        data = text_buf_.data() + sel_start();
        len  = static_cast<size_t>(sel_end() - sel_start());
    } else {
        data = text_buf_.data();
        len  = text_buf_.size();
    }
    if (len == 0) {
        return;
    }

#ifdef _WIN32
    // UTF-8 → UTF-16 转换（不含 null 终止符：显式传入字节数）
    const int wlen = MultiByteToWideChar(
        CP_UTF8, 0, data, static_cast<int>(len), nullptr, 0);
    if (wlen <= 0) {
        return;
    }
    if (!OpenClipboard(nullptr)) {
        return;
    }
    EmptyClipboard();
    HGLOBAL hglobal = GlobalAlloc(
        GMEM_MOVEABLE, (static_cast<size_t>(wlen) + 1u) * sizeof(wchar_t));
    if (!hglobal) {
        CloseClipboard();
        return;
    }
    auto* dest = static_cast<wchar_t*>(GlobalLock(hglobal));
    if (!dest) {
        GlobalFree(hglobal);
        CloseClipboard();
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(len), dest, wlen);
    dest[wlen] = L'\0';
    GlobalUnlock(hglobal);
    SetClipboardData(CF_UNICODETEXT, hglobal);
    CloseClipboard();
#endif  // _WIN32
}

void TextBox::paste_from_clipboard()
{
    if (is_read_only_) {
        return;
    }
#ifdef _WIN32
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return;
    }
    if (!OpenClipboard(nullptr)) {
        return;
    }
    HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
    if (!hdata) {
        CloseClipboard();
        return;
    }
    const auto* ws = static_cast<const wchar_t*>(GlobalLock(hdata));
    if (!ws) {
        CloseClipboard();
        return;
    }

    // 在 GlobalLock 持有期间完成 UTF-16 → UTF-8 转换
    // -1 表示 null 终止宽字符串；返回值包含 null 终止符
    const int utf8_bytes = WideCharToMultiByte(
        CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);

    std::string utf8;
    if (utf8_bytes > 1) {
        utf8.resize(static_cast<size_t>(utf8_bytes - 1));  // 不含 null 终止符
        WideCharToMultiByte(CP_UTF8, 0, ws, -1,
                            utf8.data(), utf8_bytes - 1, nullptr, nullptr);
    }
    GlobalUnlock(hdata);
    CloseClipboard();

    if (utf8.empty()) {
        return;
    }

    // 若当前有选区，先删除选区内容
    if (has_selection()) {
        delete_selection();
    }

    const uint32_t sz        = static_cast<uint32_t>(text_buf_.size());
    const uint32_t pos       = cursor_pos_;
    const uint32_t paste_len = static_cast<uint32_t>(utf8.size());

    containers::InlineString new_buf;
    new_buf.reserve(sz + paste_len);
    new_buf.append(text_buf_.data(), pos);
    new_buf.append(utf8.data(), paste_len);
    new_buf.append(text_buf_.data() + pos, sz - pos);
    text_buf_   = std::move(new_buf);
    cursor_pos_ = pos + paste_len;
    sel_anchor_ = cursor_pos_;

    set_value(TextProperty, core::Variant{ text_buf_ });
    RoutedEventArgs changed{ TextChangedEvent() };
    EventManager::raise(*this, changed);

    cursor_visible_     = true;
    cursor_blink_accum_ = 0.0f;
    update_ime_position();
    auto_scroll_to_cursor();
    invalidate_render();
#endif  // _WIN32
}

uint32_t TextBox::cursor_pos_from_xy(float click_x, float click_y) const noexcept
{
    const uint32_t len = static_cast<uint32_t>(text_buf_.size());
    if (len == 0u) {
        return 0u;
    }

    const math::Rect final_rc = bounds_rect();
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };
    
    const float text_area_w = final_rc.width - pad.left - pad.right;
    const auto wrapping = text_wrapping();
    const auto lines = split_lines(
        text_buf_.data(), text_buf_.size(),
        text_area_w,
        font_face_, font_size_px_,
        wrapping);
    const float line_h    = effective_line_height();
    const uint32_t line_idx = static_cast<uint32_t>(click_y / line_h);

    // 超出范围返回文本末尾
    if (line_idx >= static_cast<uint32_t>(lines.size())) {
        return len;
    }

    const LineInfo& line       = lines[line_idx];
    const char*     line_start = text_buf_.data() + line.start_offset;

    // 在该行内按 x 坐标定位字符（与单行逻辑相同）
    uint32_t i = 0;
    while (i < line.byte_length) {
        const uint32_t next     = utf8_next(line_start, i, line.byte_length);
        const float    x_before = measure_text_width(line_start, i);
        const float    x_after  = measure_text_width(line_start, next);
        const float    midpoint = (x_before + x_after) * 0.5f;
        if (click_x < midpoint) {
            return line.start_offset + i;
        }
        i = next;
    }
    // 点击在行末尾或之后
    return line.start_offset + line.byte_length;
}

void TextBox::move_cursor_up()
{
    if (!accepts_return() && text_wrapping() == TextWrapping::NoWrap) {
        return;  // 单行模式不支持上下移动
    }

    const math::Rect final_rc = bounds_rect();
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };
    
    const float text_area_w = final_rc.width - pad.left - pad.right;
    const auto wrapping = text_wrapping();
    const auto lines = split_lines(
        text_buf_.data(), text_buf_.size(),
        text_area_w,
        font_face_, font_size_px_,
        wrapping);
    uint32_t     line_idx, line_offset;
    if (!find_line_by_offset(lines, cursor_pos_, &line_idx, &line_offset)) {
        return;
    }

    if (line_idx == 0) {
        return;  // 已在第一行，无法上移
    }

    // 上一行
    const LineInfo& prev_line = lines[line_idx - 1];
    const char*     prev_text = text_buf_.data() + prev_line.start_offset;

    // 使用 cursor_target_x_（左右移动时更新）保持水平位置，或用当前 x
    if (cursor_target_x_ == 0.0f) {
        // 首次垂直移动，记录当前 x
        const LineInfo& curr_line = lines[line_idx];
        const char*     curr_text = text_buf_.data() + curr_line.start_offset;
        cursor_target_x_ = measure_text_width(curr_text, line_offset);
    }

    // 在上一行中找最接近 target_x 的位置
    uint32_t best_offset = 0;
    float    min_dist    = 1e9f;
    uint32_t i           = 0;
    while (i <= prev_line.byte_length) {
        const float x = measure_text_width(prev_text, i);
        const float dist = std::abs(x - cursor_target_x_);
        if (dist < min_dist) {
            min_dist    = dist;
            best_offset = i;
        }
        if (i >= prev_line.byte_length) break;
        i = utf8_next(prev_text, i, prev_line.byte_length);
    }

    cursor_pos_ = prev_line.start_offset + best_offset;
    update_ime_position();
}

void TextBox::move_cursor_down()
{
    if (!accepts_return() && text_wrapping() == TextWrapping::NoWrap) {
        return;
    }

    const math::Rect final_rc = bounds_rect();
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };
    
    const float text_area_w = final_rc.width - pad.left - pad.right;
    const auto wrapping = text_wrapping();
    const auto lines = split_lines(
        text_buf_.data(), text_buf_.size(),
        text_area_w,
        font_face_, font_size_px_,
        wrapping);
    uint32_t     line_idx, line_offset;
    if (!find_line_by_offset(lines, cursor_pos_, &line_idx, &line_offset)) {
        return;
    }

    if (line_idx + 1 >= static_cast<uint32_t>(lines.size())) {
        return;  // 已在最后一行
    }

    // 下一行
    const LineInfo& next_line = lines[line_idx + 1];
    const char*     next_text = text_buf_.data() + next_line.start_offset;

    if (cursor_target_x_ == 0.0f) {
        const LineInfo& curr_line = lines[line_idx];
        const char*     curr_text = text_buf_.data() + curr_line.start_offset;
        cursor_target_x_ = measure_text_width(curr_text, line_offset);
    }

    uint32_t best_offset = 0;
    float    min_dist    = 1e9f;
    uint32_t i           = 0;
    while (i <= next_line.byte_length) {
        const float x = measure_text_width(next_text, i);
        const float dist = std::abs(x - cursor_target_x_);
        if (dist < min_dist) {
            min_dist    = dist;
            best_offset = i;
        }
        if (i >= next_line.byte_length) break;
        i = utf8_next(next_text, i, next_line.byte_length);
    }

    cursor_pos_ = next_line.start_offset + best_offset;
    update_ime_position();
}

// ============================================================================
// 滚动辅助方法
// ============================================================================

math::Size TextBox::text_area_size() const noexcept
{
    const math::Rect rect = bounds_rect();
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };
    return {
        rect.width  - pad.left - pad.right,
        rect.height - pad.top  - pad.bottom
    };
}

float TextBox::total_content_width() const noexcept
{
    if (text_buf_.empty()) {
        return 0.0f;
    }

    const bool use_multiline_layout = accepts_return() || (text_wrapping() != TextWrapping::NoWrap);
    float max_width = 0.0f;

    if (use_multiline_layout) {
        // 多行模式：对每一行测量宽度，取最大值
        const math::Size tas = text_area_size();
        const auto lines = split_lines(
            text_buf_.data(), text_buf_.size(),
            text_wrapping() == TextWrapping::NoWrap ? 0.0f : tas.width,
            font_face_, font_size_px_,
            text_wrapping());
        for (const auto& line : lines) {
            if (line.disp_width > max_width) {
                max_width = line.disp_width;
            }
        }
    } else {
        // 单行模式：不换行，测量全部文字宽度
        max_width = measure_text_width(
            text_buf_.data(), static_cast<uint32_t>(text_buf_.size()));
    }
    return max_width;
}

float TextBox::total_content_height() const noexcept
{
    const bool use_multiline_layout = accepts_return() || (text_wrapping() != TextWrapping::NoWrap);
    const float line_h = effective_line_height();

    if (!use_multiline_layout) {
        return line_h;
    }

    const math::Size tas = text_area_size();
    const auto lines = split_lines(
        text_buf_.data(), text_buf_.size(),
        text_wrapping() == TextWrapping::NoWrap ? 0.0f : tas.width,
        font_face_, font_size_px_,
        text_wrapping());
    return static_cast<float>(lines.size()) * line_h;
}

void TextBox::clamp_scroll_offsets() noexcept
{
    const math::Size tas = text_area_size();
    const float content_w = total_content_width();
    const float content_h = total_content_height();

    // 水平滚动范围：[0, max(0, content_w - visible_w)]
    const float max_sx = content_w > tas.width ? (content_w - tas.width) : 0.0f;
    if (scroll_offset_x_ < 0.0f) {
        scroll_offset_x_ = 0.0f;
    }
    if (scroll_offset_x_ > max_sx) {
        scroll_offset_x_ = max_sx;
    }

    // 垂直滚动范围：[0, max(0, content_h - visible_h)]
    const float max_sy = content_h > tas.height ? (content_h - tas.height) : 0.0f;
    if (scroll_offset_y_ < 0.0f) {
        scroll_offset_y_ = 0.0f;
    }
    if (scroll_offset_y_ > max_sy) {
        scroll_offset_y_ = max_sy;
    }
}

void TextBox::auto_scroll_to_cursor() noexcept
{
    const math::Size tas = text_area_size();
    if (tas.width <= 0.0f || tas.height <= 0.0f) {
        return;
    }

    const float line_h = effective_line_height();
    const bool use_multiline_layout = accepts_return() || (text_wrapping() != TextWrapping::NoWrap);

    // ── 横向自动滚动（单行/多行通用）────────────────────────────────────
    // 计算光标相对于文字区域左边缘的视觉 x 坐标
    float cursor_visual_x = 0.0f;
    if (use_multiline_layout) {
        // 多行：定位光标所在行的 x
        const auto lines = split_lines(
            text_buf_.data(), text_buf_.size(),
            tas.width,
            font_face_, font_size_px_,
            text_wrapping());
        uint32_t line_idx = 0u, line_offset = 0u;
        if (find_line_by_offset(lines, cursor_pos_, &line_idx, &line_offset)) {
            const char* line_text = text_buf_.data() + lines[line_idx].start_offset;
            cursor_visual_x = measure_text_width(line_text, line_offset) - scroll_offset_x_;
        }
    } else {
        // 单行 NoWrap
        cursor_visual_x = measure_text_width(
            text_buf_.data(), cursor_pos_) - scroll_offset_x_;
    }

    // 光标偏左：滚动到让光标出现在左边缘
    if (cursor_visual_x < 0.0f) {
        scroll_offset_x_ += cursor_visual_x;  // cursor_visual_x 为负，实际减少 scroll_offset_x_
    }
    // 光标偏右：滚动到让光标出现在右边缘（留 2px 余量给光标线宽）
    else if (cursor_visual_x > tas.width - 2.0f) {
        scroll_offset_x_ += (cursor_visual_x - tas.width + 2.0f);
    }

    // ── 纵向自动滚动（仅多行模式）────────────────────────────────────────
    if (use_multiline_layout) {
        const auto lines = split_lines(
            text_buf_.data(), text_buf_.size(),
            tas.width,
            font_face_, font_size_px_,
            text_wrapping());
        uint32_t line_idx = 0u, line_offset = 0u;
        if (find_line_by_offset(lines, cursor_pos_, &line_idx, &line_offset)) {
            const float cursor_visual_y = static_cast<float>(line_idx) * line_h - scroll_offset_y_;

            // 光标偏上：滚动到让光标所在行出现在顶部
            if (cursor_visual_y < 0.0f) {
                scroll_offset_y_ += cursor_visual_y;
            }
            // 光标偏下：滚动到让光标所在行出现在底部
            else if (cursor_visual_y + line_h > tas.height) {
                scroll_offset_y_ += (cursor_visual_y + line_h - tas.height);
            }
        }
    }

    // 最终钳制到合法范围
    clamp_scroll_offsets();
}

// ============================================================================
// IME 支持
// ============================================================================

Window* TextBox::get_window() noexcept
{
    // 使用 Window::from_element 静态方法获取当前事件处理上下文中的窗口
    // 由于 Window 不在视觉树中，无法通过 parent() 遍历找到
    return Window::from_element(this);
}

math::Rect TextBox::get_cursor_rect_in_window() const noexcept
{
    // 1. 计算光标在本控件坐标系中的位置
    const core::Variant& pad_var = get_value(PaddingProperty);
    const math::Thickness pad = pad_var.has<math::Thickness>()
        ? pad_var.get<math::Thickness>()
        : math::Thickness{ 16.0f, 8.0f, 16.0f, 8.0f };

    // 光标相对于控件内容区左上角的偏移
    float cursor_local_x = pad.left;
    float cursor_local_y = pad.top;

    // 计算光标 x 坐标（在单行模式下简化处理）
    if (cursor_pos_ > 0 && !text_buf_.empty()) {
        cursor_local_x += measure_text_width(text_buf_.data(), cursor_pos_);
    }
    // 应用滚动偏移（IME 候选框跟随视觉光标位置）
    cursor_local_x -= scroll_offset_x_;
    cursor_local_y -= scroll_offset_y_;

    float cursor_h = effective_line_height();

    // 2. 转换到窗口坐标系：从本控件开始递归向上累加所有祖先的 bounds_rect 偏移
    float window_x = cursor_local_x;
    float window_y = cursor_local_y;
    
    const UIElement* current = this;
    while (current != nullptr) {
        const math::Rect bounds = current->bounds_rect();
        window_x += bounds.x;
        window_y += bounds.y;
        
        // 到达根节点（Window）时停止
        if (current->parent() == nullptr) {
            break;
        }
        current = static_cast<const UIElement*>(current->parent());
    }

    return math::Rect{ window_x, window_y, 2.0f, cursor_h };
}

void TextBox::update_ime_position()
{
    if (!is_focused_) {
        return;  // 未获得焦点时不更新 IME
    }

    Window* win = get_window();
    if (win == nullptr) {
        return;  // 未挂载到窗口树
    }

    try {
        platform::IMEService& ime = win->ime();
        math::Rect cursor_rect = get_cursor_rect_in_window();
        ime.set_composition_rect(cursor_rect);
    } catch (...) {
        // 防止 IME 调用异常导致程序崩溃
    }
}

} // namespace mine::ui
