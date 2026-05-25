/**
 * @file ContentPresenter.cpp
 * @brief ContentPresenter 实现 —— 控件模板内容占位元素。
 */

#include <mine/ui/controls/ContentPresenter.h>

#include <mine/text/FontFace.h>       // 用于 measure_text() 真实测量

#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/core/StringView.h>

namespace mine::ui {

// ============================================================================
// 依赖属性注册
// ============================================================================

// ContentPresenter::ContentProperty
// 值类型：InlineString（文字内容）或 UIElement*（元素内容）；默认空值
const DependencyProperty& ContentPresenter::ContentProperty =
    register_property<ContentPresenter>(
        "Content",
        core::Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &ContentPresenter::on_content_changed,
        });

// ContentPresenter::PaddingProperty
// 值类型：math::Thickness；默认 {0, 0, 0, 0}
const DependencyProperty& ContentPresenter::PaddingProperty =
    register_property<ContentPresenter>(
        "Padding",
        core::Variant{ math::Thickness{} },
        PropertyMetadata{
            .affects_measure = true,
            .affects_render  = true,
            .changed         = &ContentPresenter::on_padding_changed,
        });

// ============================================================================
// 依赖属性变更回调
// ============================================================================

void ContentPresenter::on_content_changed(DependencyObject*         sender,
                                          const DependencyProperty& /*prop*/,
                                          const core::Variant&      /*old_v*/,
                                          const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<ContentPresenter*>(sender);

    if (new_v.has<containers::InlineString>()) {
        // 字符串内容：缓存文字并进入文字模式
        self->content_text_    = new_v.get<containers::InlineString>();
        self->is_text_mode_    = true;
        self->content_element_ = nullptr;
    } else {
        // 空值或其他类型：清空内容
        self->content_text_    = containers::InlineString{};
        self->is_text_mode_    = false;
        self->content_element_ = nullptr;
    }
}

void ContentPresenter::on_padding_changed(DependencyObject*         sender,
                                          const DependencyProperty& /*prop*/,
                                          const core::Variant&      /*old_v*/,
                                          const core::Variant&      new_v) noexcept
{
    auto* self = static_cast<ContentPresenter*>(sender);
    if (new_v.has<math::Thickness>()) {
        self->padding_cache_ = new_v.get<math::Thickness>();
    }
}

// ============================================================================
// 生命周期
// ============================================================================

ContentPresenter::ContentPresenter() = default;
ContentPresenter::~ContentPresenter() = default;

// ============================================================================
// 字体与前景色 setter
// ============================================================================

void ContentPresenter::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    invalidate_render();
}

void ContentPresenter::set_font_size(float size_px) noexcept
{
    font_size_px_ = size_px;
    // 字号影响测量尺寸，同时需令布局重新计算
    invalidate_measure();
    invalidate_render();
}

void ContentPresenter::set_foreground(math::Color color) noexcept
{
    foreground_ = color;
    invalidate_render();
}

// ============================================================================
// 布局（测量）
// ============================================================================

void ContentPresenter::on_measure(math::Size available_size)
{
    // ContentPresenter 本身不包含 ControlTemplate，直接计算内容尺寸
    if (is_text_mode_) {
        if (font_face_ != nullptr) {
            // ── 有字体：调用 FreeType 真实测量 ──────────────────────────────────
            auto* face = static_cast<text::FontFace*>(font_face_);

            // measure_text 内部会临时调用 FT_Set_Pixel_Sizes，
            // 调用完成后 ascender()/descender() 返回对应字号的真实行高度量
            const float text_w = face->measure_text(
                content_text_.data(),
                content_text_.size(),
                font_size_px_);

            // 缓存宽度和行高度量，on_render 直接使用，避免渲染时重复查询 FreeType
            measured_text_width_ = text_w;
            cached_ascender_     = face->ascender();    // 正值，基线上方像素数
            cached_descender_    = face->descender();   // 负值，基线下方像素数

            // 真实行高 = ascender + |descender|（descender 为负，相减即相加）
            const float text_h = static_cast<float>(cached_ascender_ - cached_descender_);
            set_desired_size({
                text_w + padding_cache_.horizontal(),
                text_h + padding_cache_.vertical(),
            });
        } else {
            // ── 无字体：按字符数估算（向后兼容回退路径）──────────────────────────
            constexpr float k_char_width_rate = 0.55f;
            const float text_w = static_cast<float>(content_text_.size())
                                  * font_size_px_ * k_char_width_rate;
            const float text_h = font_size_px_ * 1.4f;

            // 估算行高度量（近似 Latin 字体比例）
            measured_text_width_ = text_w;
            cached_ascender_     = static_cast<int32_t>(font_size_px_ * 0.8f);
            cached_descender_    = -static_cast<int32_t>(font_size_px_ * 0.2f);

            set_desired_size({
                text_w + padding_cache_.horizontal(),
                text_h + padding_cache_.vertical(),
            });
        }
        return;
    }

    if (content_element_) {
        // 元素内容：委托给子元素测量
        content_element_->measure(available_size);
        set_desired_size(content_element_->desired_size());
        return;
    }

    // 无内容：零尺寸
    set_desired_size({ 0.0f, 0.0f });
}

// ============================================================================
// 渲染
// ============================================================================

void ContentPresenter::on_render(paint::Canvas& canvas)
{
    if (!is_text_mode_) {
        // 元素内容由视觉子树自动渲染；无内容时无操作
        return;
    }

    const math::Rect rect = bounds_rect();
    if (rect.empty()) {
        return;
    }

    if (font_face_ != nullptr) {
        // 水平居中：使用 on_measure 缓存的真实文字宽度，可用宽度不足时左对齐防裁剪
        const float text_x = rect.x + std::max(0.0f, (rect.width - measured_text_width_) * 0.5f);

        // 垂直居中：
        //   设文字视觉中心 = 基线上方 ascender、下方 |descender| 的中间位置
        //   视觉中心 Y = baseline - (ascender + descender) / 2
        //              （descender 为负值，(a+d)/2 = (a-|d|)/2 > 0 → 视觉中心在基线上方）
        //   令视觉中心 = rect 竖向中点：
        //     baseline = rect.center_y + (ascender + descender) / 2
        const float asc        = static_cast<float>(cached_ascender_);
        const float dsc        = static_cast<float>(cached_descender_); // 负值
        const float baseline_y = rect.y + rect.height * 0.5f + (asc + dsc) * 0.5f;

        const core::StringView sv{ content_text_.data(), content_text_.size() };
        canvas.draw_text(sv, { text_x, baseline_y }, font_face_, font_size_px_,
                         paint::Brush::solid(foreground_));
    } else {
        // 无字体时：居中水平占位线（向后兼容）
        const float line_w = rect.width - padding_cache_.horizontal();
        if (line_w <= 0.0f) { return; }
        const float line_y = rect.y + rect.height * 0.5f - 1.0f;
        canvas.fill_rect(
            { rect.x + padding_cache_.left, line_y, line_w, 2.0f },
            paint::Brush::solid(math::Color::Black));
    }
}

} // namespace mine::ui
