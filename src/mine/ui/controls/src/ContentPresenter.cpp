/**
 * @file ContentPresenter.cpp
 * @brief ContentPresenter 实现 —— 控件模板内容占位元素。
 */

#include <mine/ui/controls/ContentPresenter.h>

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
        // 按字符数估算文字宽度（与 TextBlock/Button 的占位算法一致）
        constexpr float k_font_size_px    = 14.0f;
        constexpr float k_char_width_rate = 0.55f;
        const float content_w = static_cast<float>(content_text_.size()) * k_font_size_px * k_char_width_rate;
        const float content_h = k_font_size_px * 1.4f;
        set_desired_size({
            content_w + padding_cache_.horizontal(),
            content_h + padding_cache_.vertical(),
        });
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

    const float content_area_w = rect.width - padding_cache_.horizontal();
    if (content_area_w <= 0.0f) {
        return;
    }

    if (font_face_ != nullptr) {
        // 有字体时：使用 draw_text 渲染真实文字
        // 基线 y = rect.y + padding.top + font_size（与 TextBlock 保持一致）
        const math::Vec2 text_origin{
            rect.x + padding_cache_.left,
            rect.y + padding_cache_.top + font_size_px_
        };
        const core::StringView sv{ content_text_.data(), content_text_.size() };
        canvas.draw_text(sv, text_origin, font_face_, font_size_px_,
                         paint::Brush::solid(foreground_));
    } else {
        // 无字体时：以水平占位线替代（与 M1.1 行为一致，保持向后兼容）
        const float line_y = rect.y + rect.height * 0.5f - 1.0f;
        canvas.fill_rect(
            { rect.x + padding_cache_.left, line_y, content_area_w, 2.0f },
            paint::Brush::solid(math::Color::Black));
    }
}

} // namespace mine::ui
