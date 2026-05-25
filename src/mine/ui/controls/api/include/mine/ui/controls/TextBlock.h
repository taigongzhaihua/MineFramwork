/**
 * @file TextBlock.h
 * @brief TextBlock —— 基础文本展示控件（M1.1 最小可用实现）。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/containers/InlineString.h>
#include <mine/math/Color.h>
#include <mine/math/Thickness.h>

namespace mine::ui {

/**
 * @brief 只读文本控件。
 *
 * 当前阶段提供：
 * 1. 文本内容与基础测量
 * 2. 背景色和前景色
 * 3. 样式/模板槽位与视觉状态挂点（继承自 Control）
 * 4. 关键属性迁移至 DependencyProperty，供样式系统（StyleSetter）控制
 */
class MINE_UI_CONTROLS_API TextBlock : public Control {
public:
    // ── 依赖属性 ───────────────────────────────────────────────────────────

    /** @brief 文字内容属性（Variant 存储 containers::InlineString）。 */
    static const DependencyProperty& TextProperty;

    /** @brief 字体大小属性（Variant 存储 float，单位像素，默认 14）。 */
    static const DependencyProperty& FontSizeProperty;

    /** @brief 前景色属性（Variant 存储 math::Color，默认 Black）。 */
    static const DependencyProperty& ForegroundProperty;

    /** @brief 背景色属性（Variant 存储 math::Color，默认 Transparent）。 */
    static const DependencyProperty& BackgroundProperty;

    /** @brief 内边距属性（Variant 存储 math::Thickness，默认 {4, 2, 4, 2}）。 */
    static const DependencyProperty& PaddingProperty;

    // ── 生命周期 ───────────────────────────────────────────────────────────

    TextBlock();
    ~TextBlock() override;

    TextBlock(const TextBlock&)            = delete;
    TextBlock& operator=(const TextBlock&) = delete;
    TextBlock(TextBlock&&)                 = default;
    TextBlock& operator=(TextBlock&&)      = default;

    // ── 属性访问 ───────────────────────────────────────────────────────────

    [[nodiscard]] core::StringView text() const noexcept;
    void set_text(core::StringView text);

    [[nodiscard]] float font_size() const noexcept;
    void set_font_size(float size_px);

    [[nodiscard]] math::Color foreground() const noexcept;
    void set_foreground(math::Color color);

    [[nodiscard]] math::Color background() const noexcept;
    void set_background(math::Color color);

    [[nodiscard]] math::Thickness padding() const noexcept;
    void set_padding(math::Thickness padding);

    /**
     * @brief 设置绘制文字所用字体对象（由外部持有生命周期）。
     */
    void set_font_face(void* font_face) noexcept;

protected:
    void on_measure(math::Size available_size) override;
    void on_render(paint::Canvas& canvas) override;

private:
    // ── 依赖属性变更回调 ───────────────────────────────────────────────────

    static void on_text_changed(DependencyObject*         sender,
                                const DependencyProperty& prop,
                                const core::Variant&      old_v,
                                const core::Variant&      new_v) noexcept;

    static void on_font_size_changed(DependencyObject*         sender,
                                     const DependencyProperty& prop,
                                     const core::Variant&      old_v,
                                     const core::Variant&      new_v) noexcept;

    static void on_foreground_changed(DependencyObject*         sender,
                                      const DependencyProperty& prop,
                                      const core::Variant&      old_v,
                                      const core::Variant&      new_v) noexcept;

    static void on_background_changed(DependencyObject*         sender,
                                      const DependencyProperty& prop,
                                      const core::Variant&      old_v,
                                      const core::Variant&      new_v) noexcept;

    static void on_padding_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    // ── 成员变量（成员缓存，与 DependencyProperty 值保持同步）───────────────

    containers::InlineString text_;
    float                    font_size_px_ = 14.0f;
    math::Color              foreground_   = math::Color::Black;
    math::Color              background_   = math::Color::Transparent;
    math::Thickness          padding_      = math::Thickness::symmetric(4.0f, 2.0f);
    void*                    font_face_    = nullptr;

    // ── 测量缓存（on_measure 计算，on_render 读取）────────────────────────

    /// 上行距（ascender）：基线以上像素数，正值。无字体时按字号估算。
    int32_t cached_ascender_  = 0;
    /// 下行距（descender）：基线以下像素数，负值。无字体时按字号估算。
    int32_t cached_descender_ = 0;
};

} // namespace mine::ui
