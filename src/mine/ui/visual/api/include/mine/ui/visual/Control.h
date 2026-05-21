/**
 * @file Control.h
 * @brief Control —— 具有样式属性的控件基类。
 *
 * Control 在 UIElement 基础上预留样式属性接入点，
 * 完整的样式/主题支持将在 mine.ui.style（任务 #38）中实现。
 *
 * M1.1 阶段仅声明类继承关系，不添加额外公开 API。
 *
 * 继承关系：
 *   UIElement
 *       └─ Control  (mine.ui.visual)
 *           └─ Button / TextBlock / ...（mine.ui.controls.*）
 */

#pragma once

#include <cstdint>

#include <mine/ui/visual/Api.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/core/Pimpl.h>
#include <mine/core/StringView.h>
#include <mine/containers/InlineString.h>

namespace mine::ui {

/**
 * @brief 控件视觉状态（M1.1 基础定义，后续样式系统扩展）。
 */
enum class ControlVisualState : uint8_t {
    Normal = 0,
    Hovered,
    Pressed,
    Focused,
    Disabled,
};

/**
 * @brief 控件基类。
 *
 * 所有标准控件（Button、TextBlock 等）均继承自 Control。
 * Control 本身不添加任何渲染逻辑；具体控件在 on_render() 中实现自身外观。
 *
 * 后续里程碑（mine.ui.style）将在此类添加：
 *   - BackgroundProperty（Brush）
 *   - ForegroundProperty（Brush）
 *   - PaddingProperty（Thickness）
 *   - TemplateProperty（ControlTemplate）
 *   - 等
 */
class MINE_UI_VISUAL_API Control : public UIElement {
public:
    Control();
    ~Control() override;

    Control(const Control&)            = delete;
    Control& operator=(const Control&) = delete;
    Control(Control&&)                 = default;
    Control& operator=(Control&&)      = default;

    /**
     * @brief 设置样式槽位名（例如 "DefaultButton"）。
     *
     * 当前仅存储槽位，不执行样式解析。
     */
    void set_style_slot(core::StringView slot);

    /**
     * @brief 返回样式槽位名。
     */
    [[nodiscard]] core::StringView style_slot() const noexcept;

    /**
     * @brief 设置模板槽位名（例如 "DefaultButtonTemplate"）。
     *
     * 当前仅存储槽位，不执行模板构建。
     */
    void set_template_slot(core::StringView slot);

    /**
     * @brief 返回模板槽位名。
     */
    [[nodiscard]] core::StringView template_slot() const noexcept;

    /**
     * @brief 返回当前视觉状态。
     */
    [[nodiscard]] ControlVisualState visual_state() const noexcept;

    /**
     * @brief 刷新视觉状态（调用 compute_visual_state + on_visual_state_changed）。
     */
    void update_visual_state();

    // ── 控件模板（mine.ui.style 任务 #12）────────────────────────────────

    /**
     * @brief 设置模板根元素，将其加入控件的视觉子树。
     *
     * 若此前已有模板根元素，则先将旧根从子树中移除。
     * 通常在 ControlTemplate::BuildFn 内部调用：
     * @code
     *   auto& ctrl = static_cast<Control&>(target);
     *   ctrl.set_template_root(&border_element);
     * @endcode
     *
     * @param root 模板根元素指针（nullptr 表示清除模板根）
     */
    void set_template_root(UIElement* root) noexcept;

    /**
     * @brief 在模板子树中查找具有指定 template_name 的 UIElement。
     *
     * 从模板根元素开始深度优先搜索，返回第一个匹配的元素。
     * 若模板根为空或未找到匹配元素，返回 nullptr。
     *
     * @param name 目标元素的模板名称（与 UIElement::set_template_name 对应）
     * @return     匹配的 UIElement 指针；未找到则返回 nullptr
     */
    [[nodiscard]] UIElement* find_template_child(core::StringView name) const noexcept;

    /**
     * @brief 将模板内子元素绑定到宿主控件属性（TemplateBinding）。
     *
     * 建立单向同步绑定：宿主控件的 host_prop 变更时，
     * 自动以 ValuePriority::TemplateBind 写入子元素的 child_prop。
     * 调用时立即完成首次同步（当前宿主属性值 → 子元素属性）。
     *
     * 所有绑定共享一个属性变更订阅（内部去重），不会重复订阅。
     *
     * @param child      模板子元素（须为 DependencyObject 实例）
     * @param child_prop 子元素上的目标属性
     * @param host_prop  宿主控件（this）上的源属性
     */
    void bind_template(DependencyObject&         child,
                       const DependencyProperty& child_prop,
                       const DependencyProperty& host_prop) noexcept;

protected:
    /**
     * @brief 由子类计算当前视觉状态。
     *
     * 默认返回 Normal，Button 等控件可覆盖。
     */
    [[nodiscard]] virtual ControlVisualState compute_visual_state() const;

    /**
     * @brief 视觉状态变更回调。
     *
     * 默认触发重绘；子类可覆盖追加逻辑。
     */
    virtual void on_visual_state_changed(ControlVisualState old_state,
                                         ControlVisualState new_state);

private:
    struct Impl;
    core::Pimpl<Impl> cp_;  ///< Control 私有实现（模板绑定数据）

    /// 宿主属性变更时同步所有绑定的静态回调（raw function pointer，无捕获）
    static void on_host_prop_changed(DependencyObject*,
                                     const DependencyProperty&,
                                     const core::Variant&,
                                     const core::Variant&,
                                     void* user_data) noexcept;

    containers::InlineString style_slot_;
    containers::InlineString template_slot_;
    ControlVisualState       visual_state_ = ControlVisualState::Normal;
};

} // namespace mine::ui
