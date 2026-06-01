/**
 * @file TextBlock.h
 * @brief TextBlock —— 多行只读文本展示控件。
 *
 * 支持：换行（字符/单词边界）、行高、字间距、文字对齐、
 * 省略号裁剪、最大行数限制，依赖属性全面集成。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/SmallVector.h>
#include <mine/math/Thickness.h>
#include <mine/paint/Brush.h>
#include <cstdint>

namespace mine::ui {

// ── 文字相关枚举 ──────────────────────────────────────────────────────────

/**
 * @brief 自动换行模式。
 */
enum class TextWrapping : uint8_t {
    NoWrap    = 0,  ///< 不自动换行（默认）
    Wrap,           ///< 按字符边界自动换行
    WrapAtWord,     ///< 按单词边界换行，不够宽时回退到字符边界
};

/**
 * @brief 水平文字对齐方式。
 */
enum class TextAlignment : uint8_t {
    Left   = 0,    ///< 左对齐（默认）
    Center,        ///< 居中对齐
    Right,         ///< 右对齐
};

/**
 * @brief 文字溢出裁剪模式。
 */
enum class TextTrimming : uint8_t {
    None              = 0,  ///< 不裁剪（默认）
    CharacterEllipsis,      ///< 字符级截断后追加省略号 "…"
};

// ── TextBlock 控件 ────────────────────────────────────────────────────────

/**
 * @brief 多行只读文本控件。
 *
 * 功能清单：
 *  - TextProperty：文字内容（支持 '\\n' 硬换行）
 *  - FontSizeProperty：字号（px）
 *  - ForegroundProperty / BackgroundProperty：颜色
 *  - PaddingProperty：内边距
 *  - TextWrappingProperty：NoWrap / Wrap / WrapAtWord
 *  - TextAlignmentProperty：Left / Center / Right
 *  - TextTrimmingProperty：None / CharacterEllipsis
 *  - LineHeightProperty：行高覆盖（0 = 使用字体默认行高）
 *  - CharacterSpacingProperty：字符间额外间距（px）
 *  - MaxLinesProperty：最大显示行数（0 = 不限制）
 */
class MINE_UI_CONTROLS_API TextBlock : public Control {
public:
    // ── 依赖属性 ───────────────────────────────────────────────────────────

    /** @brief 文字内容（Variant 存储 containers::InlineString；'\\n' 表示硬换行）。 */
    static const DependencyProperty& TextProperty;

    /** @brief 字体大小（Variant 存储 float，单位像素，默认 14）。 */
    static const DependencyProperty& FontSizeProperty;

    /** @brief 前景色（Variant 存储 paint::Brush，默认 SolidColor(Black)）。 */
    static const DependencyProperty& ForegroundProperty;

    /** @brief 背景色（Variant 存储 paint::Brush，默认 SolidColor(Transparent)）。 */
    static const DependencyProperty& BackgroundProperty;

    /** @brief 内边距（Variant 存储 math::Thickness，默认 {4, 2, 4, 2}）。 */
    static const DependencyProperty& PaddingProperty;

    /** @brief 换行模式（Variant 存储 TextWrapping，默认 NoWrap）。 */
    static const DependencyProperty& TextWrappingProperty;

    /** @brief 水平对齐（Variant 存储 TextAlignment，默认 Left）。 */
    static const DependencyProperty& TextAlignmentProperty;

    /** @brief 文字裁剪（Variant 存储 TextTrimming，默认 None）。 */
    static const DependencyProperty& TextTrimmingProperty;

    /**
     * @brief 行高覆盖（Variant 存储 float，单位像素，默认 0）。
     *
     * 0 表示自动（使用字体 line_height()）；正值强制设定行高。
     */
    static const DependencyProperty& LineHeightProperty;

    /**
     * @brief 字符间额外间距（Variant 存储 float，单位像素，默认 0）。
     *
     * 每个字形的 advance 之后追加此间距。
     */
    static const DependencyProperty& CharacterSpacingProperty;

    /**
     * @brief 最大显示行数（Variant 存储 int32_t，默认 0）。
     *
     * 0 表示不限制；正值截断超出的行。
     */
    static const DependencyProperty& MaxLinesProperty;

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

    [[nodiscard]] paint::Brush foreground() const noexcept;
    void set_foreground(paint::Brush brush);

    [[nodiscard]] paint::Brush background() const noexcept;
    void set_background(paint::Brush brush);

    [[nodiscard]] math::Thickness padding() const noexcept;
    void set_padding(math::Thickness padding);

    [[nodiscard]] TextWrapping text_wrapping() const noexcept;
    void set_text_wrapping(TextWrapping mode);

    [[nodiscard]] TextAlignment text_alignment() const noexcept;
    void set_text_alignment(TextAlignment align);

    [[nodiscard]] TextTrimming text_trimming() const noexcept;
    void set_text_trimming(TextTrimming mode);

    /** @brief 返回行高（0 = 自动）。 */
    [[nodiscard]] float line_height() const noexcept;
    void set_line_height(float height_px);

    /** @brief 返回字符间额外间距（px）。 */
    [[nodiscard]] float character_spacing() const noexcept;
    void set_character_spacing(float spacing_px);

    /** @brief 返回最大显示行数（0 = 不限制）。 */
    [[nodiscard]] int32_t max_lines() const noexcept;
    void set_max_lines(int32_t max);

    /**
     * @brief 设置绘制文字所用字体对象（由外部持有生命周期）。
     */
    void set_font_face(void* font_face) noexcept;

protected:
    math::Size measure_override(math::Size available) override;
    void on_render(paint::Canvas& canvas) override;

private:
    // ── 依赖属性变更回调 ───────────────────────────────────────────────────

    static void on_text_changed(DependencyObject*, const DependencyProperty&,
                                const core::Variant&, const core::Variant&) noexcept;
    static void on_font_size_changed(DependencyObject*, const DependencyProperty&,
                                     const core::Variant&, const core::Variant&) noexcept;
    static void on_foreground_changed(DependencyObject*, const DependencyProperty&,
                                      const core::Variant&, const core::Variant&) noexcept;
    static void on_background_changed(DependencyObject*, const DependencyProperty&,
                                      const core::Variant&, const core::Variant&) noexcept;
    static void on_padding_changed(DependencyObject*, const DependencyProperty&,
                                   const core::Variant&, const core::Variant&) noexcept;
    static void on_text_wrapping_changed(DependencyObject*, const DependencyProperty&,
                                         const core::Variant&, const core::Variant&) noexcept;
    static void on_text_alignment_changed(DependencyObject*, const DependencyProperty&,
                                          const core::Variant&, const core::Variant&) noexcept;
    static void on_text_trimming_changed(DependencyObject*, const DependencyProperty&,
                                         const core::Variant&, const core::Variant&) noexcept;
    static void on_line_height_changed(DependencyObject*, const DependencyProperty&,
                                       const core::Variant&, const core::Variant&) noexcept;
    static void on_char_spacing_changed(DependencyObject*, const DependencyProperty&,
                                        const core::Variant&, const core::Variant&) noexcept;
    static void on_max_lines_changed(DependencyObject*, const DependencyProperty&,
                                     const core::Variant&, const core::Variant&) noexcept;

    // ── 内部行布局结构 ─────────────────────────────────────────────────────

    /**
     * @brief 单行布局信息。
     *
     * disp_length 在 TextTrimming::CharacterEllipsis 模式下可能短于 orig_length，
     * 此时 has_ellipsis 为 true，渲染时需在 disp_length 后追加省略号 "…"。
     */
    struct TextLine {
        uint32_t start;        ///< 字节起始偏移（在 text_ 中）
        uint32_t disp_length;  ///< 显示字节数（经过省略号截断后）
        uint32_t orig_length;  ///< 原始行字节数（未截断）
        float    disp_width;   ///< 显示宽度（px，含省略号占用的宽度）
        bool     has_ellipsis; ///< 末尾是否追加省略号
    };

    // ── 成员缓存 ───────────────────────────────────────────────────────────

    containers::InlineString text_;
    float            font_size_px_   = 14.0f;
    paint::Brush     foreground_     = paint::Brush::solid(math::Color::Black);
    paint::Brush     background_     = paint::Brush::solid(math::Color::Transparent);
    math::Thickness  padding_        = math::Thickness::symmetric(4.0f, 2.0f);
    TextWrapping     text_wrapping_  = TextWrapping::NoWrap;
    TextAlignment    text_alignment_ = TextAlignment::Left;
    TextTrimming     text_trimming_  = TextTrimming::None;
    float            line_height_px_ = 0.0f;   ///< 0 = 自动
    float            char_spacing_px_= 0.0f;
    int32_t          max_lines_      = 0;       ///< 0 = 不限制
    void*            font_face_      = nullptr;

    // ── 行布局缓存 ─────────────────────────────────────────────────────────

    containers::SmallVector<TextLine, 8> cached_lines_;
    int32_t  cached_ascender_  = 0;  ///< 基线上方像素数（正值）
    int32_t  cached_descender_ = 0;  ///< 基线下方像素数（负值）

    // ── 私有辅助 ───────────────────────────────────────────────────────────

    /**
     * @brief 测量 [p, p+len) 的文字宽度（含字符间距）。
     *
     * 有字体时通过 FontFace::measure_text 测量；无字体时按字符数估算。
     */
    [[nodiscard]] float measure_segment(const char* p, uint32_t len) const noexcept;

    /**
     * @brief 重建行布局缓存。
     *
     * @param max_content_width 内容区最大宽度（不含 padding）。
     *        <= 0 或 >= 1e9 时视为无限制。
     */
    void build_lines(float max_content_width);

    /**
     * @brief 计算有效行高（px）。
     *
     * line_height_px_ > 0 时返回该值；否则返回字体行高（无字体则估算）。
     */
    [[nodiscard]] float effective_line_height() const noexcept;
};

} // namespace mine::ui
