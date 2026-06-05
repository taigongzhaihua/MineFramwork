/**
 * @file ContentPresenter.h
 * @brief ContentPresenter —— 控件模板内容占位元素（F2.1 任务 #15）。
 *
 * ContentPresenter 是 ControlTemplate 视觉树中的"内容槽"，
 * 根据 ContentProperty 的值类型决定渲染方式：
 *   - InlineString / StringView：自动创建内联 TextBlock 渲染文本
 *   - UIElement*               ：将该元素加入视觉子树并委托其测量/渲染
 *   - 空值                     ：不渲染
 *
 * 典型用法（由 ControlTemplate 构建函数调用）：
 * @code
 *   auto* presenter = core::make_raw<ContentPresenter>();
 *   presenter->set_template_name("content");
 *   button.bind_template(*presenter,
 *                         ContentPresenter::ContentProperty,
 *                         Button::ContentProperty);
 *   button.bind_template(*presenter,
 *                         ContentPresenter::PaddingProperty,
 *                         Button::PaddingProperty);
 *   button.set_template_root(presenter);
 * @endcode
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/math/Thickness.h>
#include <mine/math/Color.h>
#include <mine/paint/Brush.h>
#include <mine/containers/InlineString.h>
#include <mine/ui/controls/TextBlock.h>   // TextAlignment 枚举

namespace mine::paint { class Canvas; }

namespace mine::ui {

// 前向声明：TextBlock 完整定义仅在 ContentPresenter.cpp 中引入
class TextBlock;

/**
 * @brief 控件模板内容占位元素（单子项容器）。
 *
 * 继承自 Control 以获得依赖属性系统、模板绑定和视觉树支持。
 *
 * 内部行为：
 *   - content 为字符串时，自动创建并持有一个 TextBlock 子元素渲染文字。
 *   - content 为 UIElement* 时，将该元素原位放入视觉子树（不持有所有权）。
 *
 * ContentProperty 可通过 bind_template 连接到宿主控件的内容属性。
 */
class MINE_UI_CONTROLS_API ContentPresenter : public Control {
public:
    // ── 依赖属性 ──────────────────────────────────────────────────────────

    /**
     * @brief 内容属性（Variant 存储 InlineString 或 UIElement*）。
     *
     * 值类型：
     *   - containers::InlineString：文字内容，自动创建内联 TextBlock 渲染
     *   - UIElement*              ：元素内容，将其加入视觉子树
     *   - 空值                   ：无内容
     */
    static const DependencyProperty& ContentProperty;

    /**
     * @brief 内边距属性（Variant 存储 math::Thickness）。
     *
     * 转发给内联 TextBlock 的 PaddingProperty，默认值 {0, 0, 0, 0}。
     */
    static const DependencyProperty& PaddingProperty;

    /**
     * @brief 前景画刷属性（Variant 存储 paint::Brush），默认白色纯色画刷。
     *
     * DP 化后可被 bind_property 从宿主控件（如 Button.Foreground）单向/双向驱动，
     * 变更时自动同步给内联 TextBlock 的前景色。
     */
    static const DependencyProperty& ForegroundProperty;

    /**
     * @brief 字号属性（Variant 存储 float，逻辑像素），默认 14.0f。
     *
     * DP 化后可被 bind_property 从宿主控件（如 Button.FontSize）驱动，
     * 变更时自动同步给内联 TextBlock 的字号并触发重新测量。
     */
    static const DependencyProperty& FontSizeProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    ContentPresenter();
    ~ContentPresenter() override;

    ContentPresenter(const ContentPresenter&)            = delete;
    ContentPresenter& operator=(const ContentPresenter&) = delete;
    ContentPresenter(ContentPresenter&&)                 = default;
    ContentPresenter& operator=(ContentPresenter&&)      = default;

    // ── 字体与前景色（代理到内联 TextBlock）────────────────────────────────

    /**
     * @brief 设置渲染文字时使用的字体对象（由宿主控件传入）。
     * @param font_face FontFace 指针（void* 以避免公开 FontFace 头文件依赖）
     */
    void set_font_face(void* font_face) noexcept;

    /**
     * @brief 设置文字渲染字号（逻辑像素，默认 14.0f）。
     *
     * 内部写入 FontSizeProperty 的 Local 槽（向后兼容入口）；
     * 等价于 set_value(FontSizeProperty, size_px)。
     */
    void set_font_size(float size_px) noexcept;

    /// @brief 读取当前字号（从 FontSizeProperty 的最高优先级值取）。
    float font_size() const noexcept;

    /**
     * @brief 设置文字前景画刷（默认为白色纯色画刷）。
     *
     * 内部写入 ForegroundProperty 的 Local 槽（向后兼容入口）；
     * 仅支持纯色画刷（SolidColor）的文字渲染（当前文字渲染层限制）。
     * 渐变、亚克力等非纯色画刷降级为白色。
     */
    void set_foreground(paint::Brush brush) noexcept;

    /// @brief 读取当前前景画刷（从 ForegroundProperty 的最高优先级值取）。
    paint::Brush foreground() const noexcept;

    /**
     * @brief 设置文字对齐方式（代理到内联 TextBlock）。
     *
     * 若内联 TextBlock 尚未创建，对齐方式会被缓存并在创建时应用。
     * @param align 文字对齐方式（Left / Center / Right）
     */
    void set_text_alignment(TextAlignment align) noexcept;

    /**
     * @brief 设置是否按真实字形墨迹进行视觉对齐修正。
     *
     * 若内联 TextBlock 尚未创建，该设置会先缓存，待创建时再应用。
     */
    void set_use_ink_alignment(bool enabled) noexcept;

protected:
    // ── 布局虚方法 ────────────────────────────────────────────────────────

    /**
     * @brief 测量内容尺寸，委托给内联 TextBlock 或外部元素。
     */
    void on_measure(math::Size available_size) override;

    /**
     * @brief 排列内容子元素至给定矩形（覆盖 Control::on_arrange）。
     *
     * ContentPresenter 自身无模板根，需手动排列内联 TextBlock 或外部元素。
     */
    void on_arrange(math::Rect final_rect) override;

private:
    // ── 依赖属性变更回调 ───────────────────────────────────────────────────

    /**
     * @brief ContentProperty 变更时：
     *   - 字符串 → 创建（或复用）内联 TextBlock 并更新文字；
     *   - UIElement* → 将其加入视觉子树（暂未实现，预留）；
     *   - 空值 → 清空文字内容。
     */
    static void on_content_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    /**
     * @brief PaddingProperty 变更时同步缓存并转发给内联 TextBlock。
     */
    static void on_padding_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    /**
     * @brief ForegroundProperty 变更时把前景画刷同步给内联 TextBlock。
     */
    static void on_foreground_changed(DependencyObject*         sender,
                                      const DependencyProperty& prop,
                                      const core::Variant&      old_v,
                                      const core::Variant&      new_v) noexcept;

    /**
     * @brief FontSizeProperty 变更时把字号同步给内联 TextBlock 并重新测量。
     */
    static void on_font_size_changed(DependencyObject*         sender,
                                     const DependencyProperty& prop,
                                     const core::Variant&      old_v,
                                     const core::Variant&      new_v) noexcept;

    // ── 内部状态 ───────────────────────────────────────────────────────────

    /// 内联 TextBlock（content 为字符串时自动创建并由 ContentPresenter 拥有）。
    /// 首次设置字符串 content 时构造，析构时销毁。
    TextBlock*      inline_text_block_{nullptr};

    /// 外部元素内容（非拥有指针，生命周期由调用方控制）
    UIElement*      content_element_{nullptr};

    /// 内边距缓存（同步自 PaddingProperty，并转发给 inline_text_block_）
    math::Thickness padding_cache_{};

    /// 字体对象缓存（inline_text_block_ 未创建时暂存，创建后立即传入）
    void*           font_face_{nullptr};

    /// 文字对齐方式缓存（默认左对齐；inline_text_block_ 未创建时暂存，创建后立即传入）
    TextAlignment   text_alignment_cache_{TextAlignment::Left};

    /// 是否按真实字形墨迹做视觉对齐修正（默认关闭）
    bool            use_ink_alignment_cache_{false};
};

} // namespace mine::ui
