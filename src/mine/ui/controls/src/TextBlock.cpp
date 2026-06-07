/**
 * @file TextBlock.cpp
 * @brief TextBlock 多行文本控件实现。
 *
 * 支持换行（字符/单词边界）、行高覆盖、字符间距、
 * 文字对齐（左/居中/右）、省略号裁剪、最大行数限制。
 */

#include <mine/ui/controls/TextBlock.h>

#include <mine/text/FontFace.h>
#include <mine/text/Utf8.h>
#include <mine/text/TextLayout.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>

#include <algorithm>
#include <cstdio>  // 调试用，验证 char_spacing_px_ 运行时值

namespace mine::ui {

/// 省略号 UTF-8 编码（U+2026）
static constexpr const char  kEllipsis[]  = "\xE2\x80\xA6";
static constexpr uint32_t    kEllipsisLen = 3u;

/**
 * @brief 测量单行显示文本的真实墨迹包围盒。
 *
 * 对省略号场景，会将正文与省略号的墨迹盒合并成最终可见包围盒。
 */
static text::TextInkBounds measure_text_run_ink_bounds(void*         font_face,
                                                       float         font_size_px,
                                                       float         char_spacing_px,
                                                       const char*   data,
                                                       uint32_t      len,
                                                       bool          has_ellipsis) noexcept
{
    if (font_face == nullptr || data == nullptr) {
        return {};
    }

    auto* face = static_cast<text::FontFace*>(font_face);
    if (!has_ellipsis) {
        return face->measure_text_ink_bounds(data, len, font_size_px, char_spacing_px);
    }

    const text::TextInkBounds prefix =
        face->measure_text_ink_bounds(data, len, font_size_px, char_spacing_px);
    const text::TextInkBounds ellipsis =
        face->measure_text_ink_bounds(kEllipsis, kEllipsisLen, font_size_px, 0.0f);

    text::TextInkBounds combined{};
    combined.advance_width = prefix.advance_width + ellipsis.advance_width;

    const bool prefix_has_ink   = prefix.width > 0.0f && prefix.height > 0.0f;
    const bool ellipsis_has_ink = ellipsis.width > 0.0f && ellipsis.height > 0.0f;
    if (!prefix_has_ink && !ellipsis_has_ink) {
        return combined;
    }
    if (!prefix_has_ink) {
        combined.left   = prefix.advance_width + ellipsis.left;
        combined.top    = ellipsis.top;
        combined.width  = ellipsis.width;
        combined.height = ellipsis.height;
        return combined;
    }
    if (!ellipsis_has_ink) {
        return prefix;
    }

    const float left   = std::min(prefix.left, prefix.advance_width + ellipsis.left);
    const float top    = std::min(prefix.top, ellipsis.top);
    const float right  = std::max(prefix.left + prefix.width,
                                  prefix.advance_width + ellipsis.left + ellipsis.width);
    const float bottom = std::max(prefix.top + prefix.height,
                                  ellipsis.top + ellipsis.height);
    combined.left   = left;
    combined.top    = top;
    combined.width  = right - left;
    combined.height = bottom - top;
    return combined;
}

// ============================================================================
// 依赖属性注册
// ============================================================================

const DependencyProperty& TextBlock::TextProperty =
    register_property<TextBlock>(
        "Text",
        core::Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_text_changed,
        });

const DependencyProperty& TextBlock::FontSizeProperty =
    register_property<TextBlock>(
        "FontSize",
        core::Variant{ 14.0f },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_font_size_changed,
        });

const DependencyProperty& TextBlock::ForegroundProperty =
    register_property<TextBlock>(
        "Foreground",
        core::Variant{ paint::Brush::solid(math::Color::Black) },
        PropertyMetadata{
            .affects_render = true,
            .changed        = &TextBlock::on_foreground_changed,
        });

const DependencyProperty& TextBlock::BackgroundProperty =
    register_property<TextBlock>(
        "Background",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
        PropertyMetadata{
            .affects_render = true,
            .changed        = &TextBlock::on_background_changed,
        });

const DependencyProperty& TextBlock::PaddingProperty =
    register_property<TextBlock>(
        "Padding",
        core::Variant{ math::Thickness::symmetric(4.0f, 2.0f) },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_padding_changed,
        });

const DependencyProperty& TextBlock::TextWrappingProperty =
    register_property<TextBlock>(
        "TextWrapping",
        core::Variant{ TextWrapping::NoWrap },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_text_wrapping_changed,
        });

const DependencyProperty& TextBlock::TextAlignmentProperty =
    register_property<TextBlock>(
        "TextAlignment",
        core::Variant{ TextAlignment::Left },
        PropertyMetadata{
            .affects_render = true,
            .changed        = &TextBlock::on_text_alignment_changed,
        });

const DependencyProperty& TextBlock::TextTrimmingProperty =
    register_property<TextBlock>(
        "TextTrimming",
        core::Variant{ TextTrimming::None },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_text_trimming_changed,
        });

const DependencyProperty& TextBlock::LineHeightProperty =
    register_property<TextBlock>(
        "LineHeight",
        core::Variant{ 0.0f },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_line_height_changed,
        });

const DependencyProperty& TextBlock::CharacterSpacingProperty =
    register_property<TextBlock>(
        "CharacterSpacing",
        core::Variant{ 0.0f },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_char_spacing_changed,
        });

const DependencyProperty& TextBlock::MaxLinesProperty =
    register_property<TextBlock>(
        "MaxLines",
        core::Variant{ static_cast<int32_t>(0) },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_max_lines_changed,
        });

// ============================================================================
// 依赖属性变更回调
// ============================================================================

void TextBlock::on_text_changed(DependencyObject*         sender,
                                const DependencyProperty& /*prop*/,
                                const core::Variant&      /*old_v*/,
                                const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<containers::InlineString>()) {
        self->text_ = new_v.get<containers::InlineString>();
    } else {
        self->text_ = containers::InlineString{};
    }
}

void TextBlock::on_font_size_changed(DependencyObject*         sender,
                                     const DependencyProperty& /*prop*/,
                                     const core::Variant&      /*old_v*/,
                                     const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<float>()) {
        const float sz = new_v.get<float>();
        self->font_size_px_ = sz < 1.0f ? 1.0f : sz;
    }
}

void TextBlock::on_foreground_changed(DependencyObject*         sender,
                                      const DependencyProperty& /*prop*/,
                                      const core::Variant&      /*old_v*/,
                                      const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<paint::Brush>()) {
        self->foreground_ = new_v.get<paint::Brush>();
    }
}

void TextBlock::on_background_changed(DependencyObject*         sender,
                                      const DependencyProperty& /*prop*/,
                                      const core::Variant&      /*old_v*/,
                                      const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<paint::Brush>()) {
        self->background_ = new_v.get<paint::Brush>();
    }
}

void TextBlock::on_padding_changed(DependencyObject*         sender,
                                   const DependencyProperty& /*prop*/,
                                   const core::Variant&      /*old_v*/,
                                   const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<math::Thickness>()) {
        self->padding_ = new_v.get<math::Thickness>();
    }
}

void TextBlock::on_text_wrapping_changed(DependencyObject*         sender,
                                         const DependencyProperty& /*prop*/,
                                         const core::Variant&      /*old_v*/,
                                         const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<TextWrapping>()) {
        self->text_wrapping_ = new_v.get<TextWrapping>();
    }
}

void TextBlock::on_text_alignment_changed(DependencyObject*         sender,
                                          const DependencyProperty& /*prop*/,
                                          const core::Variant&      /*old_v*/,
                                          const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<TextAlignment>()) {
        self->text_alignment_ = new_v.get<TextAlignment>();
    }
}

void TextBlock::on_text_trimming_changed(DependencyObject*         sender,
                                         const DependencyProperty& /*prop*/,
                                         const core::Variant&      /*old_v*/,
                                         const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<TextTrimming>()) {
        self->text_trimming_ = new_v.get<TextTrimming>();
    }
}

void TextBlock::on_line_height_changed(DependencyObject*         sender,
                                       const DependencyProperty& /*prop*/,
                                       const core::Variant&      /*old_v*/,
                                       const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<float>()) {
        const float h = new_v.get<float>();
        self->line_height_px_ = h < 0.0f ? 0.0f : h;
    }
}

void TextBlock::on_char_spacing_changed(DependencyObject*         sender,
                                        const DependencyProperty& /*prop*/,
                                        const core::Variant&      /*old_v*/,
                                        const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<float>()) {
        self->char_spacing_px_ = new_v.get<float>();
    }
}

void TextBlock::on_max_lines_changed(DependencyObject*         sender,
                                     const DependencyProperty& /*prop*/,
                                     const core::Variant&      /*old_v*/,
                                     const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<int32_t>()) {
        const int32_t ml = new_v.get<int32_t>();
        self->max_lines_ = ml < 0 ? 0 : ml;
    }
}

// ============================================================================
// 生命周期
// ============================================================================

TextBlock::TextBlock()
{
    set_style_slot("DefaultTextBlock");
}

TextBlock::~TextBlock() = default;

// ============================================================================
// 属性访问器
// ============================================================================

core::StringView TextBlock::text() const noexcept
{
    return { text_.data(), text_.size() };
}

void TextBlock::set_text(core::StringView text)
{
    text_ = text;
    set_value(TextProperty, core::Variant{ text_ });
    invalidate_measure();
    invalidate_render();
}

float TextBlock::font_size() const noexcept { return font_size_px_; }

void TextBlock::set_font_size(float size_px)
{
    if (size_px < 1.0f) size_px = 1.0f;
    font_size_px_ = size_px;
    set_value(FontSizeProperty, core::Variant{ font_size_px_ });
    invalidate_measure();
    invalidate_render();
}

paint::Brush TextBlock::foreground() const noexcept { return foreground_; }

void TextBlock::set_foreground(paint::Brush brush)
{
    foreground_ = brush;
    set_value(ForegroundProperty, core::Variant{ foreground_ });
    invalidate_render();
}

paint::Brush TextBlock::background() const noexcept { return background_; }

void TextBlock::set_background(paint::Brush brush)
{
    background_ = brush;
    set_value(BackgroundProperty, core::Variant{ background_ });
    invalidate_render();
}

math::Thickness TextBlock::padding() const noexcept { return padding_; }

void TextBlock::set_padding(math::Thickness padding)
{
    padding_ = padding;
    set_value(PaddingProperty, core::Variant{ padding_ });
    invalidate_measure();
    invalidate_render();
}

TextWrapping TextBlock::text_wrapping() const noexcept { return text_wrapping_; }

void TextBlock::set_text_wrapping(TextWrapping mode)
{
    text_wrapping_ = mode;
    set_value(TextWrappingProperty, core::Variant{ text_wrapping_ });
    invalidate_measure();
    invalidate_render();
}

TextAlignment TextBlock::text_alignment() const noexcept { return text_alignment_; }

void TextBlock::set_text_alignment(TextAlignment align)
{
    text_alignment_ = align;
    set_value(TextAlignmentProperty, core::Variant{ text_alignment_ });
    invalidate_render();
}

bool TextBlock::use_ink_alignment() const noexcept { return use_ink_alignment_; }

void TextBlock::set_use_ink_alignment(bool enabled) noexcept
{
    if (use_ink_alignment_ == enabled) {
        return;
    }
    use_ink_alignment_ = enabled;
    invalidate_render();
}

TextTrimming TextBlock::text_trimming() const noexcept { return text_trimming_; }

void TextBlock::set_text_trimming(TextTrimming mode)
{
    text_trimming_ = mode;
    set_value(TextTrimmingProperty, core::Variant{ text_trimming_ });
    invalidate_measure();
    invalidate_render();
}

float TextBlock::line_height() const noexcept { return line_height_px_; }

void TextBlock::set_line_height(float height_px)
{
    line_height_px_ = height_px < 0.0f ? 0.0f : height_px;
    set_value(LineHeightProperty, core::Variant{ line_height_px_ });
    invalidate_measure();
    invalidate_render();
}

float TextBlock::character_spacing() const noexcept { return char_spacing_px_; }

void TextBlock::set_character_spacing(float spacing_px)
{
    char_spacing_px_ = spacing_px;
    set_value(CharacterSpacingProperty, core::Variant{ char_spacing_px_ });
    invalidate_measure();
    invalidate_render();
}

int32_t TextBlock::max_lines() const noexcept { return max_lines_; }

void TextBlock::set_max_lines(int32_t max)
{
    max_lines_ = max < 0 ? 0 : max;
    set_value(MaxLinesProperty, core::Variant{ max_lines_ });
    invalidate_measure();
    invalidate_render();
}

void TextBlock::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    invalidate_render();
}

// ============================================================================
// 私有辅助
// ============================================================================

float TextBlock::measure_segment(const char* p, uint32_t len) const noexcept
{
    if (len == 0u) return 0.0f;
    float w;
    if (font_face_ != nullptr) {
        // 使用 FreeType 真实字形宽度
        auto* face = static_cast<text::FontFace*>(font_face_);
        w = face->measure_text(p, len, font_size_px_);
    } else {
        // 无字体：按字符数估算（ASCII 平均比例 0.55）
        w = static_cast<float>(text::count_utf8_chars(p, len)) * font_size_px_ * 0.55f;
    }
    // 叠加字符间距
    if (char_spacing_px_ != 0.0f) {
        w += static_cast<float>(text::count_utf8_chars(p, len)) * char_spacing_px_;
    }
    return w;
}

float TextBlock::effective_line_height() const noexcept
{
    if (line_height_px_ > 0.0f) return line_height_px_;
    if (font_face_ != nullptr) {
        auto* face = static_cast<text::FontFace*>(font_face_);
        return static_cast<float>(face->line_height());
    }
    return font_size_px_ * 1.4f;
}

void TextBlock::build_lines(float max_content_width)
{
    cached_lines_.clear();

    const char*    data  = text_.data();
    const uint32_t total = static_cast<uint32_t>(text_.size());

    // 是否启用宽度自动折叠
    const bool use_width_limit =
        (text_wrapping_ != TextWrapping::NoWrap)
        && (max_content_width > 0.0f)
        && (max_content_width < 1.0e9f);

    // 字符级折叠辅助 lambda：委托给 mine::text::split_lines
    auto char_wrap_segment = [&](uint32_t seg_start, uint32_t seg_end) {
        // 空段落保留一个空行
        if (seg_start == seg_end) {
            cached_lines_.push_back({seg_start, 0u, 0u, 0.0f, false});
            return;
        }
        // 适配器：static 包装 this->measure_segment
        struct Ctx { TextBlock* self; };
        Ctx ctx{this};
        auto measure_fn = [](void* c, const char* d, uint32_t len) -> float {
            return static_cast<Ctx*>(c)->self->measure_segment(d, len);
        };
        auto tl = mine::text::split_lines(
            data + seg_start, seg_end - seg_start, max_content_width,
            measure_fn, &ctx, true);
        for (auto& l : tl) {
            cached_lines_.push_back({seg_start + l.start_offset,
                                     l.byte_length, l.byte_length, l.disp_width, false});
        }
    };

    // 按 '\n' 将文本分割为段落，逐段构建行
    uint32_t seg_start       = 0u;
    bool     last_was_nl     = false;

    for (uint32_t i = 0u; i <= total; ++i) {
        const bool is_nl  = (i < total && data[i] == '\n');
        const bool is_end = (i == total);
        if (!is_nl && !is_end) continue;

        const uint32_t seg_end = i;
        last_was_nl = is_nl;

        if (!use_width_limit) {
            // ── NoWrap：整段作为一行 ──────────────────────────────────────
            const float w = measure_segment(data + seg_start, seg_end - seg_start);
            cached_lines_.push_back({seg_start,
                                      seg_end - seg_start,
                                      seg_end - seg_start,
                                      w, false});
        } else if (text_wrapping_ == TextWrapping::Wrap) {
            // ── Wrap：字符边界折叠 ────────────────────────────────────────
            char_wrap_segment(seg_start, seg_end);
        } else {
            // ── WrapAtWord：单词边界折叠，回退到字符折叠 ─────────────────
            if (seg_start == seg_end) {
                cached_lines_.push_back({seg_start, 0u, 0u, 0.0f, false});
            } else {
                uint32_t line_start    = seg_start;
                uint32_t last_word_end = seg_start;
                bool     line_has_word = false;
                uint32_t pos           = seg_start;

                while (pos <= seg_end) {
                    if (pos == seg_end) {
                        // 提交最后一行（若还有内容）
                        if (line_start < seg_end) {
                            const float w = measure_segment(data + line_start,
                                                             seg_end - line_start);
                            cached_lines_.push_back({line_start,
                                                      seg_end - line_start,
                                                      seg_end - line_start,
                                                      w, false});
                        }
                        break;
                    }

                    // 跳过空格
                    if (data[pos] == ' ') { ++pos; continue; }

                    // 找当前词 [word_start, word_end)
                    const uint32_t word_start = pos;
                    uint32_t       word_end   = pos;
                    while (word_end < seg_end && data[word_end] != ' ') ++word_end;

                    // 测量：当前行加入此词后的总宽
                    const float w_with_word =
                        measure_segment(data + line_start, word_end - line_start);

                    if (w_with_word <= max_content_width || !line_has_word) {
                        // 词适合（或当前行无词，强制放入）
                        last_word_end = word_end;
                        line_has_word = true;
                        pos           = word_end;
                    } else {
                        // 词不适合：提交当前行 [line_start, last_word_end)
                        const float commit_w =
                            measure_segment(data + line_start,
                                             last_word_end - line_start);
                        cached_lines_.push_back({line_start,
                                                  last_word_end - line_start,
                                                  last_word_end - line_start,
                                                  commit_w, false});

                        // 检查单个词是否本身就超出宽度
                        const float word_w =
                            measure_segment(data + word_start, word_end - word_start);

                        if (word_w > max_content_width) {
                            // 词太宽：字符级断行
                            char_wrap_segment(word_start, word_end);
                            pos           = word_end;
                            line_start    = word_end;
                            last_word_end = word_end;
                            line_has_word = false;
                        } else {
                            // 从当前词重新开始新行（不 advance pos）
                            line_start    = word_start;
                            last_word_end = word_start;
                            line_has_word = false;
                        }
                    }
                }
            }
        }

        seg_start = i + 1u;  // 跳过 '\n'，进入下一段
    }

    // 文本以 '\n' 结尾时，追加一个空行
    if (last_was_nl) {
        cached_lines_.push_back({total, 0u, 0u, 0.0f, false});
    }

    // 保证至少一行（空文本）
    if (cached_lines_.empty()) {
        cached_lines_.push_back({0u, 0u, 0u, 0.0f, false});
    }

    // ── 省略号后处理（CharacterEllipsis）─────────────────────────────────
    const bool do_ellipsis =
        (text_trimming_ == TextTrimming::CharacterEllipsis)
        && (max_content_width > 0.0f)
        && (max_content_width < 1.0e9f);

    if (do_ellipsis) {
        const float ell_w = measure_segment(kEllipsis, kEllipsisLen);

        for (auto& line : cached_lines_) {
            if (line.disp_width > max_content_width && line.disp_length > 0u) {
                const float avail    = max_content_width - ell_w;
                const char* p        = data + line.start;
                uint32_t    trim_pos = 0u;
                float       accum    = 0.0f;

                if (avail > 0.0f) {
                    while (trim_pos < line.disp_length) {
                        const uint32_t nb = text::utf8_next(p, trim_pos, line.disp_length);
                        const float    cw = measure_segment(p + trim_pos, nb - trim_pos);
                        if (accum + cw > avail) break;
                        accum   += cw;
                        trim_pos = nb;
                    }
                }
                line.disp_length  = trim_pos;
                line.disp_width   = accum + ell_w;
                line.has_ellipsis = true;
            }
        }
    }

    // ── max_lines_ 截断 ───────────────────────────────────────────────────
    if (max_lines_ > 0 &&
        static_cast<int32_t>(cached_lines_.size()) > max_lines_)
    {
        // 若启用省略号，在最后一个可见行标记省略号
        if (text_trimming_ == TextTrimming::CharacterEllipsis) {
            auto&       last  = cached_lines_[static_cast<size_t>(max_lines_ - 1)];
            const float ell_w = measure_segment(kEllipsis, kEllipsisLen);

            if (!last.has_ellipsis) {
                const bool limit_w =
                    (max_content_width > 0.0f) && (max_content_width < 1.0e9f);

                if (limit_w && (last.disp_width + ell_w) > max_content_width) {
                    // 需要截断前缀才能容纳省略号
                    const float avail    = max_content_width - ell_w;
                    const char* p        = data + last.start;
                    uint32_t    trim_pos = 0u;
                    float       accum    = 0.0f;

                    if (avail > 0.0f) {
                        while (trim_pos < last.disp_length) {
                            const uint32_t nb = text::utf8_next(p, trim_pos,
                                                                    last.disp_length);
                            const float cw = measure_segment(p + trim_pos, nb - trim_pos);
                            if (accum + cw > avail) break;
                            accum   += cw;
                            trim_pos = nb;
                        }
                    }
                    last.disp_length  = trim_pos;
                    last.disp_width   = accum + ell_w;
                } else {
                    last.disp_width += ell_w;
                }
                last.has_ellipsis = true;
            }
        }

        cached_lines_.resize(static_cast<size_t>(max_lines_));
    }
}

// ============================================================================
// measure_override
// ============================================================================

// available 已由 FrameworkElement::on_measure 减去了 Margin 并应用了 Width/Height 约束，
// 此处只需再减去自身 padding 即可得到文字内容区可用宽度。
math::Size TextBlock::measure_override(math::Size available)
{
    // 计算内容区最大宽度（去掉水平 padding）
    const float max_content_w = available.width - padding_.horizontal();

    if (font_face_ != nullptr) {
        auto* face = static_cast<text::FontFace*>(font_face_);
        // 先设置字号，使 ascender/descender/line_height 度量值有效
        face->set_pixel_size(0u, static_cast<uint32_t>(font_size_px_ + 0.5f));
        cached_ascender_  = face->ascender();
        cached_descender_ = face->descender();
    } else {
        // 无字体：估算度量值
        cached_ascender_  = static_cast<int32_t>(font_size_px_ * 0.8f);
        cached_descender_ = -static_cast<int32_t>(font_size_px_ * 0.2f);
    }

    // 重建行布局缓存
    build_lines(max_content_w);

    // 计算期望尺寸
    const float eff_lh  = effective_line_height();
    float total_h = eff_lh * static_cast<float>(cached_lines_.size());
    const bool  use_single_line_ink_measure = !use_ink_alignment_
                                           && line_height_px_ <= 0.0f
                                           && cached_lines_.size() == 1u
                                           && font_face_ != nullptr;
    text::TextInkBounds single_line_ink_bounds{};

    // 单行自动高度默认按真实字形墨迹计算，避免背景/内边距看起来被额外行距稀释。
    if (use_single_line_ink_measure)
    {
        const TextLine& line = cached_lines_.front();
        single_line_ink_bounds = measure_text_run_ink_bounds(
            font_face_,
            font_size_px_,
            char_spacing_px_,
            text_.data() + line.start,
            line.disp_length,
            line.has_ellipsis);
        if (single_line_ink_bounds.height > 0.0f) {
            total_h = single_line_ink_bounds.height;
        }
    }

    // 单行左对齐时，测量首字形 bearing_x 用于修正起始 X
    first_glyph_bearing_x_ = 0.0f;
    if (use_single_line_ink_measure
        && text_alignment_ == TextAlignment::Left
        && !text_.empty()
        && font_face_ != nullptr)
    {
        auto* face = static_cast<text::FontFace*>(font_face_);
        text::GlyphBitmap gb{};
        const uint32_t first_cp = [this]() -> uint32_t {
            const auto c0 = static_cast<uint8_t>(text_[0]);
            if (c0 < 0x80u) return c0;
            if ((c0 & 0xE0u) == 0xC0u && text_.size() >= 2u)
                return ((c0 & 0x1Fu) << 6u) | (static_cast<uint8_t>(text_[1]) & 0x3Fu);
            if ((c0 & 0xF0u) == 0xE0u && text_.size() >= 3u)
                return ((c0 & 0x0Fu) << 12u) | ((static_cast<uint8_t>(text_[1]) & 0x3Fu) << 6u) | (static_cast<uint8_t>(text_[2]) & 0x3Fu);
            if ((c0 & 0xF8u) == 0xF0u && text_.size() >= 4u)
                return ((c0 & 0x07u) << 18u) | ((static_cast<uint8_t>(text_[1]) & 0x3Fu) << 12u) | ((static_cast<uint8_t>(text_[2]) & 0x3Fu) << 6u) | (static_cast<uint8_t>(text_[3]) & 0x3Fu);
            return 0xFFFDu;
        }();
        if (first_cp != 0xFFFDu && face->rasterize(first_cp, gb)) {
            first_glyph_bearing_x_ = static_cast<float>(gb.metrics.bearing_x);
        }
    }

    float max_w = 0.0f;
    for (const auto& line : cached_lines_) {
        if (line.disp_width > max_w) max_w = line.disp_width;
    }
    if (use_single_line_ink_measure && single_line_ink_bounds.width > 0.0f) {
        max_w = single_line_ink_bounds.width;
    }

    // 返回内容区期望尺寸（不含 Margin）；FrameworkElement::on_measure 会自动加回 Margin 并调用 set_desired_size
    return {
        max_w  + padding_.horizontal(),
        total_h + padding_.vertical(),
    };
}

// ============================================================================
// on_render
// ============================================================================

void TextBlock::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) return;

    // ── 绘制背景 ──────────────────────────────────────────────────────────
    const bool bg_visible =
        (background_.kind() != paint::BrushKind::SolidColor)
        || (background_.color().a > 0.0f);
    if (bg_visible) {
        canvas.fill_rect(rect, background_);
    }

    if (font_face_ == nullptr) {
        // 无字体时绘制占位横线
        const float line_y = rect.y + rect.height * 0.5f - 1.0f;
        canvas.fill_rect(
            { rect.x + padding_.left, line_y,
              rect.width - padding_.horizontal(), 2.0f },
            foreground_);
        return;
    }

    if (cached_lines_.empty()) return;

    // ── 排版参数计算 ──────────────────────────────────────────────────────
    const float eff_lh         = effective_line_height();
    const float asc            = static_cast<float>(cached_ascender_);
    const float dsc            = static_cast<float>(cached_descender_);  // 负值
    // 行框内基线垂直居中偏移
    const float baseline_off   = eff_lh * 0.5f + (asc + dsc) * 0.5f;
    const float content_left   = rect.x + padding_.left;
    const float content_right  = rect.x + rect.width - padding_.right;
    const float content_top    = rect.y + padding_.top;
    const float content_w      = rect.width - padding_.horizontal();
    const float content_h      = rect.height - padding_.vertical();

    auto visible_slice_for_line = [&](const char* data, uint32_t len, float base_x) {
        struct VisibleSlice {
            uint32_t start_offset{0};
            uint32_t byte_length{0};
            float    draw_x{0.0f};
        } slice{};

        if (data == nullptr || len == 0u) {
            return slice;
        }

        const float margin_px = 48.0f;
        float pen_x = base_x;
        bool started = false;
        uint32_t pos = 0u;
        uint32_t end = len;
        while (pos < len) {
            const uint32_t next = text::utf8_next(data, pos, len);
            const float glyph_w = measure_segment(data + pos, next - pos);
            const float glyph_left = pen_x;
            const float glyph_right = pen_x + glyph_w;

            if (!started && glyph_right >= content_left - margin_px) {
                started = true;
                slice.start_offset = pos;
                slice.draw_x = glyph_left;
            }
            if (started && glyph_left > content_right + margin_px) {
                end = pos;
                break;
            }

            pen_x = glyph_right;
            pos = next;
        }

        if (!started) {
            return slice;
        }
        slice.byte_length = end - slice.start_offset;
        return slice;
    };

    // 确定可见行数
    const size_t max_visible = (max_lines_ > 0)
        ? std::min(cached_lines_.size(), static_cast<size_t>(max_lines_))
        : cached_lines_.size();
    const bool  use_single_line_ink_layout = !use_ink_alignment_
                                          && line_height_px_ <= 0.0f
                                          && max_visible == 1u;

    // ── 逐行渲染（裁剪到内容区，防止文字渲染到 padding 区域或控件之外）──
    canvas.save();
    canvas.clip_rect({
        content_left,
        content_top,
        content_w,
        content_h
    });

    for (size_t i = 0u; i < max_visible; ++i) {
        const TextLine& line       = cached_lines_[i];
        const float line_top = content_top + static_cast<float>(i) * eff_lh;
        const float line_bottom = line_top + eff_lh;

        // 跳过完全在可见区域外的行（大幅减少长文本的 DrawText 调用量）
        if (line_bottom < content_top || line_top > content_top + content_h) {
            continue;
        }

        const bool  need_ink_bounds = use_ink_alignment_
                                   || use_single_line_ink_layout
                                   || text_alignment_ == TextAlignment::Center
                                   || text_alignment_ == TextAlignment::Right;
        text::TextInkBounds ink_bounds{};
        if (need_ink_bounds) {
            ink_bounds = measure_text_run_ink_bounds(
                font_face_,
                font_size_px_,
                char_spacing_px_,
                text_.data() + line.start,
                line.disp_length,
                line.has_ellipsis);
        }

        float baseline_y = line_top + baseline_off;
        if ((use_ink_alignment_ || use_single_line_ink_layout) && ink_bounds.height > 0.0f) {
            baseline_y = line_top + (eff_lh - ink_bounds.height) * 0.5f - ink_bounds.top;
            if (use_single_line_ink_layout) {
                baseline_y = content_top + (content_h - ink_bounds.height) * 0.5f - ink_bounds.top;
            }
        }

        // 根据对齐模式计算行起始 X
        float text_x = content_left;
        const bool use_horizontal_ink_layout = (use_ink_alignment_ || use_single_line_ink_layout)
                                            && ink_bounds.width > 0.0f;
        if (text_alignment_ == TextAlignment::Center) {
            if (use_horizontal_ink_layout) {
                text_x = content_left + (content_w - ink_bounds.width) * 0.5f - ink_bounds.left;
            } else {
                text_x = content_left + (content_w - line.disp_width) * 0.5f;
            }
        } else if (text_alignment_ == TextAlignment::Right) {
            if (use_horizontal_ink_layout) {
                text_x = content_right - ink_bounds.width - ink_bounds.left;
            } else {
                text_x = content_right - line.disp_width;
            }
        } else if (use_horizontal_ink_layout) {
            text_x = content_left - ink_bounds.left;
        }
        // 单行左对齐再按首字形 bearing 精确修正
        if (use_single_line_ink_layout && text_alignment_ == TextAlignment::Left
            && first_glyph_bearing_x_ > 0.0f) {
            text_x = content_left - first_glyph_bearing_x_;
        }

        // 绘制行文字（含字符间距，不含省略号）
        if (line.disp_length > 0u) {
            const char* line_data = text_.data() + line.start;
            const bool need_visible_slice = !line.has_ellipsis && line.disp_width > content_w + 96.0f;
            const auto slice = need_visible_slice
                ? visible_slice_for_line(line_data, line.disp_length, text_x)
                : decltype(visible_slice_for_line(line_data, line.disp_length, text_x)){};

            canvas.draw_text(
                core::StringView{
                    need_visible_slice ? (line_data + slice.start_offset) : line_data,
                    need_visible_slice ? slice.byte_length : line.disp_length },
                { need_visible_slice ? slice.draw_x : text_x, baseline_y },
                font_face_,
                font_size_px_,
                foreground_,
                char_spacing_px_);
        }

        // 在文字末尾追加省略号（省略号本身不叠加字符间距）
        if (line.has_ellipsis) {
            const float prefix_w = measure_segment(
                text_.data() + line.start, line.disp_length);
            canvas.draw_text(
                core::StringView{ kEllipsis, kEllipsisLen },
                { text_x + prefix_w, baseline_y },
                font_face_,
                font_size_px_,
                foreground_,
                0.0f);
        }
    }

    canvas.restore();
}

} // namespace mine::ui
