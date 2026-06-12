/**
 * @file ContentControl.h
 * @brief ContentControl —— 单内容控件基类（任务 17.1）。
 *
 * ContentControl 在 Control 的基础上引入"内容"概念：
 *   - 通过 ContentProperty（DependencyProperty）持有一个 Variant，
 *     其值可为 InlineString（文字内容）或 UIElement*（元素内容）。
 *   - 提供 set_content(UIElement*) 标准接口（任务 17.1 核心要求）。
 *   - 提供 set_content(StringView) 字符串快捷接口。
 *   - 子类可重写 on_content_changed() 响应内容变更（如同步内部缓存）。
 *
 * 继承关系：
 *   DependencyObject (mine.ui.property)
 *       └─ Visual (mine.ui.visual)
 *           └─ UIElement (mine.ui.visual)
 *               └─ FrameworkElement (mine.ui.visual)
 *                   └─ Control (mine.ui.visual)
 *                       └─ ContentControl (mine.ui.controls)
 *                           └─ Button (mine.ui.controls)
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/controls/InteractableControl.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/containers/InlineString.h>
#include <mine/core/StringView.h>
#include <mine/core/Variant.h>

namespace mine::ui {

/**
 * @brief 单内容控件基类。
 *
 * 所有以"内容"为核心的控件（Button、CheckBox、RadioButton 等）
 * 均应继承此类，而非直接继承 Control。
 *
 * ContentProperty 是 ContentControl 的核心属性：
 *   - 变更时自动触发 invalidate_measure + invalidate_render（PropertyMetadata 控制）。
 *   - 子类可通过重写 on_content_changed() 在内容变更时同步额外缓存。
 *
 * 注意：ContentControl 自身不含渲染逻辑；具体控件通过 ControlTemplate
 * 中的 ContentPresenter 展示内容。
 */
class MINE_UI_CONTROLS_API ContentControl : public InteractableControl {
public:
    // ── 依赖属性 ──────────────────────────────────────────────────────────

    /**
     * @brief 内容属性（Variant 存储 InlineString 或 UIElement*）。
     *
     * 值类型：
     *   - containers::InlineString ：文字内容，由 ContentPresenter 渲染为 TextBlock
     *   - UIElement*               ：元素内容，由 ContentPresenter 纳入视觉子树
     *   - 空值（默认）            ：无内容
     *
     * 更改该属性时，ContentPresenter 会通过 bind_template 自动收到同步值。
     */
    static const DependencyProperty& ContentProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    ContentControl();
    ~ContentControl() override;

    ContentControl(const ContentControl&)            = delete;
    ContentControl& operator=(const ContentControl&) = delete;
    ContentControl(ContentControl&&)                 = default;
    ContentControl& operator=(ContentControl&&)      = default;

    // ── 标准内容接口 ──────────────────────────────────────────────────────

    /**
     * @brief 设置 UIElement 内容（任务 17.1 标准接口）。
     *
     * 将 element 写入 ContentProperty（Local 优先级）。
     * ContentControl 不持有元素所有权，调用方负责元素生命周期。
     * 传入 nullptr 时清空内容（等价于 reset_content()）。
     *
     * @param element 要展示的 UIElement 指针，或 nullptr 清空内容。
     */
    void set_content(UIElement* element);

    /**
     * @brief 设置字符串内容（将 text 写入 ContentProperty 作为 InlineString）。
     *
     * @param text 文字内容（内部以 InlineString 存储）。
     */
    void set_content(core::StringView text);

    /**
     * @brief 读取内容属性的原始 Variant 值（最高优先级生效值）。
     */
    [[nodiscard]] const core::Variant& content() const noexcept;

    /**
     * @brief 若当前内容为 UIElement*，返回指针；否则返回 nullptr。
     */
    [[nodiscard]] UIElement* content_element() const noexcept;

    /**
     * @brief 若当前内容为字符串，返回 StringView；否则返回空串。
     */
    [[nodiscard]] core::StringView content_text() const noexcept;

protected:
    /**
     * @brief 内容属性变更钩子（子类可重写以同步内部缓存）。
     *
     * 基类实现为空；PropertyMetadata 的 affects_measure/affects_render 已自动
     * 触发 invalidate_measure() + invalidate_render()，子类无需重复调用。
     *
     * 典型用途：Button 重写此方法以同步 text_ 成员缓存（用于无模板回退路径）。
     *
     * @param old_v 变更前的 Variant 值
     * @param new_v 变更后的 Variant 值
     */
    virtual void on_content_changed(const core::Variant& old_v,
                                    const core::Variant& new_v) noexcept;

private:
    /// DependencyProperty 变更时调用的静态回调，转发到虚方法 on_content_changed
    static void s_on_content_changed(DependencyObject*         sender,
                                     const DependencyProperty& prop,
                                     const core::Variant&      old_v,
                                     const core::Variant&      new_v) noexcept;
};

} // namespace mine::ui
