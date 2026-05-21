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
    containers::InlineString style_slot_;
    containers::InlineString template_slot_;
    ControlVisualState       visual_state_ = ControlVisualState::Normal;
};

} // namespace mine::ui
