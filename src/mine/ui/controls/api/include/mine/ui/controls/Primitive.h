/**
 * @file Primitive.h
 * @brief Primitive —— 基元控件基类（控件架构重构阶段 2）。
 *
 * 基元控件是外观体系中唯一直接调用 Canvas API 的层。
 * 它们不包含交互逻辑、不管理视觉状态机——只负责从 DP 读取
 * 外观参数并绘制到 Canvas。
 *
 * 继承关系：
 *   Control → Primitive → Border / TextBlock / Rectangle / Image
 *
 * 与 InteractableControl 的区别：
 *   - Primitive：纯视觉叶子，不响应鼠标/键盘，无 VSM
 *   - InteractableControl：可交互控件，管理状态机
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>

namespace mine::ui {

/**
 * @brief 基元控件基类。
 *
 * 所有仅负责绘制的叶子控件均继承此类。Primitive 在 Control 的
 * 布局委托和渲染入口基础上，明确声明「不参与交互、不管理视觉状态」。
 *
 * 子类职责：
 *   - 覆盖 on_render() 调用 Canvas API 绘制自身
 *   - 所有外观参数从 DependencyProperty 读取
 *   - 需要子元素管理的基元（如 Border）自行管理（不通过 inner_element）
 */
class MINE_UI_CONTROLS_API Primitive : public Control {
public:
    Primitive() = default;
    ~Primitive() override = default;

    Primitive(const Primitive&)            = delete;
    Primitive& operator=(const Primitive&) = delete;

    /**
     * @brief 标记外观失效（触发重绘）。
     *
     * 当基元控件的外观参数通过非 DP 方式变更时（如字体对象指针更新），
     * 调用此方法通知框架重新渲染。
     */
    void invalidate_appearance();
};

} // namespace mine::ui
