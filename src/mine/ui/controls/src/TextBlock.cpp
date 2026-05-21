/**
 * @file TextBlock.cpp
 * @brief TextBlock 基础实现。
 */

#include <mine/ui/controls/TextBlock.h>

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>

namespace mine::ui {

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
    invalidate_render();
}

math::Color TextBlock::background() const noexcept
{
    return background_;
}

void TextBlock::set_background(math::Color color)
{
    background_ = color;
    invalidate_render();
}

math::Thickness TextBlock::padding() const noexcept
{
    return padding_;
}

void TextBlock::set_padding(math::Thickness padding)
{
    padding_ = padding;
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
