/**
 * @file TextBlock.cpp
 * @brief TextBlock 基础实现。
 */

#include <mine/ui/controls/TextBlock.h>

#include <mine/text/FontFace.h>        // 用于 measure_text() 真实字形测量
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

// TextBlock::ForegroundProperty — 前景色（Brush，默认纯色黑色）
const DependencyProperty& TextBlock::ForegroundProperty =
    register_property<TextBlock>(
        "Foreground",
        core::Variant{ paint::Brush::solid(math::Color::Black) },
        PropertyMetadata{
            .affects_render = true,
            .changed        = &TextBlock::on_foreground_changed,
        });

// TextBlock::BackgroundProperty — 背景色（Brush，默认透明）
const DependencyProperty& TextBlock::BackgroundProperty =
    register_property<TextBlock>(
        "Background",
        core::Variant{ paint::Brush::solid(math::Color::Transparent) },
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

// ============================================================================
// 生命周期
// ============================================================================

TextBlock::TextBlock()
{
    set_style_slot("DefaultTextBlock");
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

paint::Brush TextBlock::foreground() const noexcept
{
    return foreground_;
}

void TextBlock::set_foreground(paint::Brush brush)
{
    foreground_ = brush;
    set_value(ForegroundProperty, core::Variant{ foreground_ });
    invalidate_render();
}

paint::Brush TextBlock::background() const noexcept
{
    return background_;
}

void TextBlock::set_background(paint::Brush brush)
{
    background_ = brush;
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
    if (font_face_ != nullptr) {
        // ── 有字体：FreeType 真实字形测量 ──────────────────────────────────────
        auto* face = static_cast<text::FontFace*>(font_face_);

        // measure_text 内部临时调用 FT_Set_Pixel_Sizes；
        // 调用完成后 ascender()/descender() 返回当前字号的真实行高度量。
        const float text_w    = face->measure_text(text_.data(), text_.size(), font_size_px_);
        cached_ascender_      = face->ascender();   // 正值，基线上方像素数
        cached_descender_     = face->descender();  // 负值，基线下方像素数

        // 真实行高 = ascender + |descender|（descender 为负，相减即相加）
        const float text_h = static_cast<float>(cached_ascender_ - cached_descender_);
        set_desired_size({
            text_w + padding_.horizontal(),
            text_h + padding_.vertical(),
        });
    } else {
        // ── 无字体：估算（向后兼容回退路径）──────────────────────────────────
        const float content_width  = static_cast<float>(text_.size()) * font_size_px_ * 0.55f;
        const float content_height = font_size_px_ * 1.4f;

        // 估算行高度量（近似 Latin 字体比例）
        cached_ascender_  = static_cast<int32_t>(font_size_px_ * 0.8f);
        cached_descender_ = -static_cast<int32_t>(font_size_px_ * 0.2f);
        set_desired_size({
            content_width + padding_.horizontal(),
            content_height + padding_.vertical(),
        });
    }
}

void TextBlock::on_render(paint::Canvas& canvas)
{
    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    // 背景画刷可见性检查：纯色全透明时跳过
    const bool bg_visible = (background_.kind() != paint::BrushKind::SolidColor)
                         || (background_.color().a > 0.0f);
    if (bg_visible) {
        canvas.fill_rect(rect, background_);
    }

    if (font_face_ != nullptr && !text_.empty()) {
        // 垂直居中：令文字视觉中心与 rect 竖向中点对齐
        //   文字视觉中心 = 基线 - (ascender + descender) / 2
        //                （descender 为负值，视觉中心在基线上方）
        //   令视觉中心 = rect.center_y ：
        //     baseline = rect.center_y + (ascender + descender) / 2
        const float asc        = static_cast<float>(cached_ascender_);
        const float dsc        = static_cast<float>(cached_descender_); // 负值
        const float baseline_y = rect.y + rect.height * 0.5f + (asc + dsc) * 0.5f;
        const float text_x     = rect.x + padding_.left;

        canvas.draw_text(
            core::StringView{ text_.data(), text_.size() },
            { text_x, baseline_y },
            font_face_,
            font_size_px_,
            foreground_);
        return;
    }

    // 没有字体面时采用占位线，保证可见渲染输出。
    const float line_y = rect.y + rect.height * 0.5f - 1.0f;
    canvas.fill_rect(
        { rect.x + padding_.left, line_y, rect.width - padding_.horizontal(), 2.0f },
        foreground_);
}

} // namespace mine::ui
