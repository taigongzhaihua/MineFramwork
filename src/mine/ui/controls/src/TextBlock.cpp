/**
 * @file TextBlock.cpp
 * @brief TextBlock 基础实现。
 */

#include <mine/ui/controls/TextBlock.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>

namespace mine::ui {

// ============================================================================
// 依赖属性注册
// ============================================================================

// TextBlock::TextProperty — 文字内容（InlineString）
const DependencyProperty& TextBlock::TextProperty =
    register_property<TextBlock>(
        "Text",
        core::Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_text_changed,
        });

// TextBlock::FontSizeProperty — 字体大小（float，像素，默认 14）
const DependencyProperty& TextBlock::FontSizeProperty =
    register_property<TextBlock>(
        "FontSize",
        core::Variant{ 14.0f },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_font_size_changed,
        });

// TextBlock::ForegroundProperty — 前景色（Color，默认黑色）
const DependencyProperty& TextBlock::ForegroundProperty =
    register_property<TextBlock>(
        "Foreground",
        core::Variant{ math::Color::Black },
        PropertyMetadata{
            .affects_render = true,
            .changed        = &TextBlock::on_foreground_changed,
        });

// TextBlock::BackgroundProperty — 背景色（Color，默认透明）
const DependencyProperty& TextBlock::BackgroundProperty =
    register_property<TextBlock>(
        "Background",
        core::Variant{ math::Color::Transparent },
        PropertyMetadata{
            .affects_render = true,
            .changed        = &TextBlock::on_background_changed,
        });

// TextBlock::PaddingProperty — 内边距（Thickness，默认水平 4px 垂直 2px）
const DependencyProperty& TextBlock::PaddingProperty =
    register_property<TextBlock>(
        "Padding",
        core::Variant{ math::Thickness::symmetric(4.0f, 2.0f) },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &TextBlock::on_padding_changed,
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
    if (new_v.has<math::Color>()) {
        self->foreground_ = new_v.get<math::Color>();
    }
}

void TextBlock::on_background_changed(DependencyObject*         sender,
                                      const DependencyProperty& /*prop*/,
                                      const core::Variant&      /*old_v*/,
                                      const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<TextBlock*>(sender);
    if (new_v.has<math::Color>()) {
        self->background_ = new_v.get<math::Color>();
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

// ============================================================================
// 生命周期
// ============================================================================

TextBlock::TextBlock()
{
    set_style_slot("DefaultTextBlock");
    set_template_slot("DefaultTextBlockTemplate");
}

TextBlock::~TextBlock() = default;

core::StringView TextBlock::text() const noexcept
{
    return core::StringView{ text_.data(), text_.size() };
}

void TextBlock::set_text(core::StringView text)
{
    text_ = text;
    set_value(TextProperty, core::Variant{ text_ });
    invalidate_measure();
    invalidate_render();
}

float TextBlock::font_size() const noexcept
{
    return font_size_px_;
}

void TextBlock::set_font_size(float size_px)
{
    if (size_px < 1.0f) {
        size_px = 1.0f;
    }
    font_size_px_ = size_px;
    set_value(FontSizeProperty, core::Variant{ font_size_px_ });
    invalidate_measure();
    invalidate_render();
}

math::Color TextBlock::foreground() const noexcept
{
    return foreground_;
}

void TextBlock::set_foreground(math::Color color)
{
    foreground_ = color;
    set_value(ForegroundProperty, core::Variant{ foreground_ });
    invalidate_render();
}

math::Color TextBlock::background() const noexcept
{
    return background_;
}

void TextBlock::set_background(math::Color color)
{
    background_ = color;
    set_value(BackgroundProperty, core::Variant{ background_ });
    invalidate_render();
}

math::Thickness TextBlock::padding() const noexcept
{
    return padding_;
}

void TextBlock::set_padding(math::Thickness padding)
{
    padding_ = padding;
    set_value(PaddingProperty, core::Variant{ padding_ });
    invalidate_measure();
    invalidate_render();
}

void TextBlock::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    invalidate_render();
}

void TextBlock::on_measure(math::Size /*available_size*/)
{
    const float content_width  = static_cast<float>(text_.size()) * font_size_px_ * 0.55f;
    const float content_height = font_size_px_ * 1.4f;
    set_desired_size({
        content_width + padding_.horizontal(),
        content_height + padding_.vertical(),
    });
}

void TextBlock::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    if (background_.a > 0.0f) {
        canvas.fill_rect(rect, paint::Brush::solid(background_));
    }

    const math::Vec2 text_origin{ rect.x + padding_.left, rect.y + padding_.top + font_size_px_ };
    if (font_face_ != nullptr && !text_.empty()) {
        canvas.draw_text(
            core::StringView{ text_.data(), text_.size() },
            text_origin,
            font_face_,
            font_size_px_,
            paint::Brush::solid(foreground_));
        return;
    }

    // 没有字体面时采用占位线，保证可见渲染输出。
    const float line_y = rect.y + rect.height * 0.5f - 1.0f;
    canvas.fill_rect(
        { rect.x + padding_.left, line_y, rect.width - padding_.horizontal(), 2.0f },
        paint::Brush::solid(foreground_));
}

} // namespace mine::ui
