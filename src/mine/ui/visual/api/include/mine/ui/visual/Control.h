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

#include <mine/ui/visual/Api.h>
#include <mine/ui/visual/UIElement.h>

namespace mine::ui {

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
};

} // namespace mine::ui
