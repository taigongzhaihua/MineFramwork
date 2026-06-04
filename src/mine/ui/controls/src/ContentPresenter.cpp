/**
 * @file ContentPresenter.cpp
 * @brief ContentPresenter 实现 —— 控件模板内容占位元素。
 *
 * 设计要点：
 *   - content 为字符串时，自动创建内联 TextBlock 并委托全部测量/渲染；
 *   - content 为 UIElement* 时，原位放入视觉子树（预留，当前里程碑暂不完整实现）；
 *   - ContentPresenter 自身不直接绘制任何内容。
 */

#include <mine/ui/controls/ContentPresenter.h>
#include <mine/ui/controls/TextBlock.h>       // 内联 TextBlock 创建
#include <mine/paint/Brush.h>                 // BrushKind（前景画刷类型判断）

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
        // ── 字符串内容：创建或复用内联 TextBlock ──────────────────────────────
        if (!self->inline_text_block_) {
            // 首次设置字符串内容：创建内联 TextBlock 并配置字体/颜色/内边距/对齐
            self->inline_text_block_ = MINE_NEW(TextBlock);
            self->inline_text_block_->set_font_face(self->font_face_);
            self->inline_text_block_->set_font_size(self->font_size_px_);
            // 直接传入 Brush（TextBlock 已支持 paint::Brush 前景色）
            self->inline_text_block_->set_foreground(self->foreground_);
            self->inline_text_block_->set_padding(self->padding_cache_);
            self->inline_text_block_->set_text_alignment(self->text_alignment_cache_);
            self->inline_text_block_->set_use_ink_alignment(self->use_ink_alignment_cache_);
            // 将 TextBlock 加入视觉子树，后续由视觉树驱动其测量和渲染
            self->add_child(self->inline_text_block_);
        }
        // 更新文字内容（TextBlock 内部会触发自身 invalidate）
        const auto& str = new_v.get<containers::InlineString>();
        self->inline_text_block_->set_text(
            core::StringView{ str.data(), str.size() });
        // 元素内容指针置空（与字符串模式互斥）
        self->content_element_ = nullptr;

    } else {
        // ── 空值或其他类型：清空文字内容 ─────────────────────────────────────
        if (self->inline_text_block_) {
            // 保留 TextBlock 结构但清空文字（避免频繁分配/销毁）
            self->inline_text_block_->set_text({});
        }
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
        // 将新的内边距同步到内联 TextBlock（如果已创建）
        if (self->inline_text_block_) {
            self->inline_text_block_->set_padding(self->padding_cache_);
        }
    }
}

// ============================================================================
// 生命周期
// ============================================================================

ContentPresenter::ContentPresenter() = default;

ContentPresenter::~ContentPresenter()
{
    // 析构内联 TextBlock：先从视觉树移除，再释放内存
    if (inline_text_block_) {
        remove_child(inline_text_block_);
        MINE_DELETE(inline_text_block_);
        inline_text_block_ = nullptr;
    }
}

// ============================================================================
// 字体与前景色 setter（代理到内联 TextBlock）
// ============================================================================

void ContentPresenter::set_font_face(void* font_face) noexcept
{
    font_face_ = font_face;
    if (inline_text_block_) {
        inline_text_block_->set_font_face(font_face);
    }
    // 字体变更影响文字宽度，需重新测量
    invalidate_measure();
}

void ContentPresenter::set_font_size(float size_px) noexcept
{
    font_size_px_ = size_px;
    if (inline_text_block_) {
        // TextBlock::set_font_size 内部已调用 invalidate_measure
        inline_text_block_->set_font_size(size_px);
    } else {
        invalidate_measure();
    }
}

void ContentPresenter::set_foreground(paint::Brush brush) noexcept
{
    foreground_ = brush;
    if (inline_text_block_) {
        // TextBlock 已支持 paint::Brush 前景色，直接传入
        inline_text_block_->set_foreground(brush);
    } else {
        invalidate_render();
    }
}

void ContentPresenter::set_text_alignment(TextAlignment align) noexcept
{
    text_alignment_cache_ = align;
    if (inline_text_block_) {
        inline_text_block_->set_text_alignment(align);
        invalidate_render();
    }
}

void ContentPresenter::set_use_ink_alignment(bool enabled) noexcept
{
    use_ink_alignment_cache_ = enabled;
    if (inline_text_block_) {
        inline_text_block_->set_use_ink_alignment(enabled);
        invalidate_render();
    }
}

// ============================================================================
// 布局（测量）
// ============================================================================

void ContentPresenter::on_measure(math::Size available_size)
{
    if (inline_text_block_) {
        // 字符串内容：委托给内联 TextBlock 测量，取其期望尺寸
        inline_text_block_->measure(available_size);
        set_desired_size(inline_text_block_->desired_size());
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
// 布局（排列）
// ============================================================================

void ContentPresenter::on_arrange(math::Rect final_rect)
{
    // ContentPresenter 自身无模板根，直接排列内容子元素
    if (inline_text_block_) {
        inline_text_block_->arrange(final_rect);
        return;
    }

    if (content_element_) {
        content_element_->arrange(final_rect);
    }
}

} // namespace mine::ui

