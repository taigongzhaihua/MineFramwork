/**
 * @file ContentControl.cpp
 * @brief ContentControl 实现 —— 自包含内容渲染。
 */

#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/paint/Brush.h>
#include <mine/containers/InlineString.h>
#include <mine/core/Memory.h>

namespace mine::ui {

// ============================================================================
// 依赖属性
// ============================================================================

const DependencyProperty& ContentControl::ContentProperty =
    register_property<ContentControl>(
        "Content", core::Variant{},
        PropertyMetadata{ .affects_measure = true, .affects_render = true,
                          .changed = &ContentControl::s_on_content_changed });

const DependencyProperty& ContentControl::ForegroundProperty =
    register_property<ContentControl>(
        "Foreground", core::Variant{ paint::Brush::solid(math::Color::White) },
        PropertyMetadata{ .affects_render = true,
                          .changed = &ContentControl::s_on_foreground_changed });

const DependencyProperty& ContentControl::FontSizeProperty =
    register_property<ContentControl>(
        "FontSize", core::Variant{ 14.0f },
        PropertyMetadata{ .affects_measure = true,
                          .changed = &ContentControl::s_on_font_size_changed });

const DependencyProperty& ContentControl::PaddingProperty =
    register_property<ContentControl>(
        "Padding", core::Variant{ math::Thickness{} },
        PropertyMetadata{ .affects_measure = true,
                          .changed = &ContentControl::s_on_padding_changed });

// ============================================================================
// DP 回调
// ============================================================================

void ContentControl::s_on_content_changed(DependencyObject* s, const DependencyProperty&,
                                          const core::Variant& o, const core::Variant& n) noexcept
{
    auto* self = static_cast<ContentControl*>(s);
    if (n.has<containers::InlineString>()) {
        if (!self->inline_text_block_) {
            self->inline_text_block_ = MINE_NEW(TextBlock);
            self->inline_text_block_->set_font_face(self->font_face_);
            self->inline_text_block_->set_font_size(self->font_size());
            self->inline_text_block_->set_foreground(self->foreground());
            self->inline_text_block_->set_padding(self->padding_cache_);
            self->inline_text_block_->set_text_alignment(self->text_alignment_cache_);
            self->add_child(self->inline_text_block_);
        }
        const auto& str = n.get<containers::InlineString>();
        self->inline_text_block_->set_text(core::StringView{str.data(), str.size()});
        self->content_element_ = nullptr;
    } else {
        if (self->inline_text_block_) self->inline_text_block_->set_text({});
        self->content_element_ = nullptr;
    }
    self->on_content_changed(o, n);
}

void ContentControl::s_on_foreground_changed(DependencyObject* s, const DependencyProperty&,
                                             const core::Variant&, const core::Variant& n) noexcept
{
    auto* self = static_cast<ContentControl*>(s);
    if (n.has<paint::Brush>() && self->inline_text_block_)
        self->inline_text_block_->set_foreground(n.get<paint::Brush>());
}

void ContentControl::s_on_font_size_changed(DependencyObject* s, const DependencyProperty&,
                                            const core::Variant&, const core::Variant& n) noexcept
{
    auto* self = static_cast<ContentControl*>(s);
    if (n.has<float>() && self->inline_text_block_)
        self->inline_text_block_->set_font_size(n.get<float>());
}

void ContentControl::s_on_padding_changed(DependencyObject* s, const DependencyProperty&,
                                          const core::Variant&, const core::Variant& n) noexcept
{
    auto* self = static_cast<ContentControl*>(s);
    if (n.has<math::Thickness>()) {
        self->padding_cache_ = n.get<math::Thickness>();
        if (self->inline_text_block_)
            self->inline_text_block_->set_padding(self->padding_cache_);
    }
}

// ============================================================================
// 生命周期
// ============================================================================

ContentControl::ContentControl() = default;

ContentControl::~ContentControl()
{
    if (inline_text_block_) { remove_child(inline_text_block_); MINE_DELETE(inline_text_block_); }
}

// ============================================================================
// 内容接口
// ============================================================================

void ContentControl::set_content(UIElement* e) { set_value(ContentProperty, e ? core::Variant{e} : core::Variant{}); }
void ContentControl::set_content(core::StringView t) { set_value(ContentProperty, core::Variant{containers::InlineString{t}}); }

const core::Variant& ContentControl::content() const noexcept { return get_value(ContentProperty); }

UIElement* ContentControl::content_element() const noexcept
{ const core::Variant& v = content(); return v.has<UIElement*>() ? v.get<UIElement*>() : nullptr; }

core::StringView ContentControl::content_text() const noexcept
{
    const core::Variant& v = content();
    if (v.has<containers::InlineString>()) { const auto& s = v.get<containers::InlineString>(); return {s.data(), s.size()}; }
    return {};
}

// ============================================================================
// 外观
// ============================================================================

void ContentControl::set_foreground(paint::Brush b) noexcept { set_value(ForegroundProperty, core::Variant{b}, ValuePriority::Local); }
paint::Brush ContentControl::foreground() const noexcept
{ const core::Variant& v = get_value(ForegroundProperty); return v.has<paint::Brush>() ? v.get<paint::Brush>() : paint::Brush::solid(math::Color::White); }

void ContentControl::set_font_size(float px) noexcept { set_value(FontSizeProperty, core::Variant{px}, ValuePriority::Local); }
float ContentControl::font_size() const noexcept
{ const core::Variant& v = get_value(FontSizeProperty); return v.has<float>() ? v.get<float>() : 14.0f; }

void ContentControl::set_font_face(void* ff) noexcept { font_face_ = ff; if (inline_text_block_) inline_text_block_->set_font_face(ff); invalidate_measure(); }
void ContentControl::set_text_alignment(TextAlignment a) noexcept { text_alignment_cache_ = a; if (inline_text_block_) inline_text_block_->set_text_alignment(a); }

// ============================================================================
// 布局（measure_override / arrange_override）
// ============================================================================

math::Size ContentControl::measure_override(math::Size available)
{
    if (inline_text_block_) {
        inline_text_block_->measure(available);
        return inline_text_block_->desired_size();
    }
    if (content_element_) {
        content_element_->measure(available);
        return content_element_->desired_size();
    }
    return Control::measure_override(available);
}

math::Size ContentControl::arrange_override(math::Size final_size)
{
    if (inline_text_block_) {
        inline_text_block_->arrange(bounds_rect());
    } else if (content_element_) {
        content_element_->arrange(bounds_rect());
    }
    return Control::arrange_override(final_size);
}

// ============================================================================
// 钩子
// ============================================================================

void ContentControl::on_content_changed(const core::Variant&, const core::Variant&) noexcept {}

} // namespace mine::ui
