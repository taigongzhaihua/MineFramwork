/**
 * @file ContentPresenter.h
 * @brief ContentPresenter —— 模板树内容占位元素（F2.1 任务 #15）。
 *
 * ContentPresenter 是 ControlTemplate 视觉树中的"内容槽"，
 * 根据 ContentProperty 的值类型选择渲染方式：
 *   - InlineString / StringView：以文字行占位线展示（字体系统尚未就绪时的替代方案）
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
#include <mine/containers/InlineString.h>

namespace mine::paint { class Canvas; }

namespace mine::ui {

/**
 * @brief 控件模板内容占位元素。
 *
 * 继承自 Control 以获得依赖属性系统、模板绑定和视觉树支持。
 * ContentProperty 可通过 bind_template 连接到宿主控件的文字/内容属性。
 */
class MINE_UI_CONTROLS_API ContentPresenter : public Control {
public:
    // ── 依赖属性 ──────────────────────────────────────────────────────────

    /**
     * @brief 内容属性（Variant 存储 InlineString 或 UIElement*）。
     *
     * 值类型：
     *   - containers::InlineString：文字内容，以占位线渲染
     *   - UIElement*              ：元素内容，将其加入视觉子树
     *   - 空值                   ：无内容
     */
    static const DependencyProperty& ContentProperty;

    /**
     * @brief 内边距属性（Variant 存储 math::Thickness）。
     *
     * 用于在内容周围留出间距，默认值为 {0, 0, 0, 0}。
     */
    static const DependencyProperty& PaddingProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    ContentPresenter();
    ~ContentPresenter() override;

    ContentPresenter(const ContentPresenter&)            = delete;
    ContentPresenter& operator=(const ContentPresenter&) = delete;
    ContentPresenter(ContentPresenter&&)                 = default;
    ContentPresenter& operator=(ContentPresenter&&)      = default;

protected:
    // ── 布局与渲染虚方法 ───────────────────────────────────────────────────

    /**
     * @brief 根据内容类型计算期望尺寸。
     *
     * - 文字内容：按字符数估算宽度（占位算法，与 TextBlock 相同）
     * - 元素内容：委托给内容元素的 measure
     * - 无内容：返回 {0, 0}
     */
    void on_measure(math::Size available_size) override;

    /**
     * @brief 渲染内容。
     *
     * - 文字内容：绘制水平占位线（无真实字体时的视觉替代）
     * - 元素内容：元素作为子节点自动渲染，此处无需操作
     * - 无内容：空操作
     */
    void on_render(paint::Canvas& canvas) override;

private:
    // ── 依赖属性变更回调 ───────────────────────────────────────────────────

    /**
     * @brief ContentProperty 变更时同步内部状态。
     *
     * 根据新值类型更新 content_text_、content_element_、is_text_mode_。
     * 若新值为 UIElement*，将其加入/替换视觉子树中的内容元素。
     */
    static void on_content_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    /**
     * @brief PaddingProperty 变更时同步内部 padding 缓存。
     */
    static void on_padding_changed(DependencyObject*         sender,
                                   const DependencyProperty& prop,
                                   const core::Variant&      old_v,
                                   const core::Variant&      new_v) noexcept;

    // ── 内部状态 ───────────────────────────────────────────────────────────

    /// 文字内容缓存（当 ContentProperty 为 InlineString 时有效）
    containers::InlineString content_text_;

    /// 元素内容（非拥有指针，生命周期由外部控制）
    UIElement*               content_element_{nullptr};

    /// 内边距缓存（从 PaddingProperty 同步）
    math::Thickness          padding_cache_{};

    /// true 表示文字模式；false 表示元素模式或无内容
    bool                     is_text_mode_{false};
};

} // namespace mine::ui
