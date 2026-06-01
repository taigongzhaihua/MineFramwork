/**
 * @file Control.h
 * @brief Control —— 具有样式属性和视觉状态支持的控件基类。
 *
 * Control 在 FrameworkElement 基础上添加：
 *   - 样式槽位（style_slot）
 *   - 视觉状态机（VisualStateManager）
 *   - 内部子元素管理（set_inner_element / inner_element）
 *
 * 继承关系：
 *   DependencyObject (mine.ui.property)
 *       └─ Visual (mine.ui.visual)
 *           └─ UIElement (mine.ui.visual)
 *               └─ FrameworkElement (mine.ui.visual)
 *                   └─ Control (mine.ui.visual)
 *                       └─ Button / TextBlock / ...（mine.ui.controls.*）
 *
 * 自定义控件外观：继承现有控件并重写 on_render() 即可，
 * 无需 ControlTemplate / BuildFn 等额外机制（类似 QWidget 风格）。
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include <mine/ui/visual/Api.h>
#include <mine/ui/visual/FrameworkElement.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/core/Pimpl.h>
#include <mine/core/StringView.h>
#include <mine/containers/InlineString.h>
#include <mine/core/Memory.h>

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
 * 继承 FrameworkElement 后，所有控件自动参与两遗布局协议，
 * 支持 Margin、Width/Height 约束、HorizontalAlignment/VerticalAlignment 以及
 * Panel 容器（StackPanel、Grid）的子元素容纳。
 *
 * 自定义外观：继承此控件并重写 on_render() 实现自定义绘制逻辑，
 * 无需 ControlTemplate / BuildFn 等额外机制（类似 QWidget 风格）。
 */
class MINE_UI_VISUAL_API Control : public FrameworkElement {
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
     * @brief 返回当前视觉状态。
     */
    [[nodiscard]] ControlVisualState visual_state() const noexcept;

    /**
     * @brief 刷新视觉状态（调用 compute_visual_state + on_visual_state_changed）。
     */
    void update_visual_state();

    // ── VisualStateManager ────────────────────────────────────────────────────

    /**
     * @brief 安装视觉状态管理器。
     *
     * VSM 负责管理控件的视觉状态切换（Normal/Hovered/Pressed 等）。
     * 安装后可通过 vsm() 获取并调用 go_to_state() 驱动状态机。
     *
     * @param vsm 已配置好状态和过渡的 VisualStateManager 实例（移动所有权）
     */
    void set_visual_state_manager(style::VisualStateManager vsm) noexcept;

    /**
     * @brief 返回已安装的 VisualStateManager 指针（未安装时返回 nullptr）。
     */
    [[nodiscard]] style::VisualStateManager* vsm() noexcept;

    /**
     * @brief 返回已安装的 VisualStateManager 指针（const 重载）。
     */
    [[nodiscard]] const style::VisualStateManager* vsm() const noexcept;

protected:
    /**
     * @brief 设置内部子元素（不拥有所有权）。
     *
     * 将 root 加入控件的视觉子树。若此前已有内部元素，先将旧元素从子树中移除。
     * 供 Button 等控件在构造函数中直接安装 ContentPresenter 等内部实现元素。
     *
     * @param root 内部子元素指针（nullptr 表示清除）
     */
    void set_inner_element(UIElement* root) noexcept;

    /**
     * @brief 设置内部子元素并转移所有权（OwnedPtr<UIElement> 版本）。
     */
    void set_inner_element(core::OwnedPtr<UIElement> root) noexcept;

    /**
     * @brief 设置内部子元素并转移所有权（子类型重载，含类型擦除）。
     *
     * 接受 OwnedPtr<TElement>（TElement 为 UIElement 的子类），
     * 内部将所有权提升为 OwnedPtr<UIElement> 后委托给基类重载。
     */
    template<typename TElement,
             std::enable_if_t<std::is_base_of_v<UIElement, TElement> &&
                              !std::is_same_v<UIElement, TElement>, int> = 0>
    void set_inner_element(core::OwnedPtr<TElement> root) noexcept
    {
        // 类型擦除：保留删除器，将裸指针提升为 UIElement*
        auto del = root.get_deleter();
        UIElement* raw = root.release();
        set_inner_element(core::OwnedPtr<UIElement>{ raw, del });
    }

    /**
     * @brief 返回当前内部子元素指针（nullptr 表示未安装）。
     *
     * 子类可在 measure_override / on_render 中访问此值。
     */
    [[nodiscard]] UIElement* inner_element() const noexcept;

    /**
     * @brief Measure 覆盖：将测量委托给内部子元素。
     *
     * 若内部元素已安装，委托其 measure() 并返回其期望尺寸；
     * 否则返回零尺寸。
     *
     * @param available 经约束处理后的可用内容区域尺寸
     * @return 内容期望尺寸（不含 Margin）
     */
    math::Size measure_override(math::Size available) override;

    /**
     * @brief Arrange 覆盖：将内部子元素排列至内容区域。
     *
     * @param final_size 分配给内容区域的最终尺寸（已减去 Margin）
     * @return 实际占用的内容尺寸
     */
    math::Size arrange_override(math::Size final_size) override;

    /**
     * @brief 由子类计算当前视觉状态（枚举）。
     *
     * 默认返回 Normal；Button 等控件可覆盖以反映 Hovered/Pressed 等状态。
     */
    [[nodiscard]] virtual ControlVisualState compute_visual_state() const;

    /**
     * @brief 由子类计算当前视觉状态名字符串（供 VisualStateManager 使用）。
     *
     * 默认实现将 compute_visual_state() 返回的枚举映射为字符串：
     *   Normal→"Normal", Hovered→"Hovered", Pressed→"Pressed",
     *   Focused→"Focused", Disabled→"Disabled"。
     * 子类可直接重写此函数以返回自定义状态名。
     */
    [[nodiscard]] virtual core::StringView compute_state_name() const noexcept;

    /**
     * @brief 视觉状态变更回调。
     *
     * 默认触发重绘；子类可覆盖追加逻辑。
     */
    virtual void on_visual_state_changed(ControlVisualState old_state,
                                         ControlVisualState new_state);

private:
    /// 私有实现（Pimpl 惯用法，隐藏 style 依赖细节）
    struct Impl;
    core::Pimpl<Impl> cp_;

    /// 样式槽位名（InlineString 避免堆分配）
    containers::InlineString style_slot_;

    /// 当前视觉状态（枚举，由 compute_visual_state 计算得出）
    ControlVisualState visual_state_{ ControlVisualState::Normal };
};

} // namespace mine::ui
