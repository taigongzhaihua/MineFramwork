/**
 * @file ContentControl.h
 * @brief ContentControl —— 自包含内容渲染的单内容控件基类（重构阶段 3）。
 *
 * 合并原 ContentPresenter 的渲染能力：文字内容自动内联 TextBlock 渲染。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/controls/InteractableControl.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/containers/InlineString.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>
#include <mine/math/Thickness.h>
#include <mine/math/Color.h>
#include <mine/paint/Brush.h>

namespace mine::ui {

class MINE_UI_CONTROLS_API ContentControl : public InteractableControl {
public:
    static const DependencyProperty& ContentProperty;
    static const DependencyProperty& ForegroundProperty;
    static const DependencyProperty& FontSizeProperty;
    static const DependencyProperty& PaddingProperty;

    ContentControl();
    ~ContentControl() override;

    ContentControl(const ContentControl&)            = delete;
    ContentControl& operator=(const ContentControl&) = delete;

    void set_content(UIElement* element);
    void set_content(core::StringView text);
    [[nodiscard]] const core::Variant& content() const noexcept;
    [[nodiscard]] UIElement* content_element() const noexcept;
    [[nodiscard]] core::StringView content_text() const noexcept;

    void set_foreground(paint::Brush brush) noexcept;
    [[nodiscard]] paint::Brush foreground() const noexcept;

    void set_font_size(float size_px) noexcept;
    [[nodiscard]] float font_size() const noexcept;

    void set_font_face(void* font_face) noexcept;

    void set_text_alignment(TextAlignment align) noexcept;

protected:
    math::Size measure_override(math::Size available) override;
    math::Size arrange_override(math::Size final_size) override;

    virtual void on_content_changed(const core::Variant& old_v,
                                    const core::Variant& new_v) noexcept;

private:
    static void s_on_content_changed(DependencyObject* s, const DependencyProperty& p,
                                     const core::Variant& o, const core::Variant& n) noexcept;
    static void s_on_foreground_changed(DependencyObject* s, const DependencyProperty& p,
                                        const core::Variant& o, const core::Variant& n) noexcept;
    static void s_on_font_size_changed(DependencyObject* s, const DependencyProperty& p,
                                       const core::Variant& o, const core::Variant& n) noexcept;
    static void s_on_padding_changed(DependencyObject* s, const DependencyProperty& p,
                                     const core::Variant& o, const core::Variant& n) noexcept;

    TextBlock*       inline_text_block_{nullptr};
    UIElement*       content_element_{nullptr};
    math::Thickness  padding_cache_{};
    void*            font_face_{nullptr};
    TextAlignment    text_alignment_cache_{TextAlignment::Left};
};

} // namespace mine::ui
