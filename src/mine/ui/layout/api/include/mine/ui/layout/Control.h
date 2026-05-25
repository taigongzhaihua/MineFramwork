/**
 * @file Control.h
 * @brief Control —— 具有样式属性和模板支持的控件基类。
 *
 * Control 在 FrameworkElement 基础上添加：
 *   - 样式/模板槽位（style_slot / template_slot）
 *   - 视觉状态机（VisualStateManager）
 *   - 控件模板根管理（set_template_root / find_template_child）
 *   - 模板属性绑定（bind_template）
 *
 * 继承关系：
 *   DependencyObject (mine.ui.property)
 *       └─ Visual (mine.ui.visual)
 *           └─ UIElement (mine.ui.visual)
 *               └─ FrameworkElement (mine.ui.layout)
 *                   └─ Control (mine.ui.layout)
 *                       └─ Button / TextBlock / ...（mine.ui.controls.*）
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include <mine/ui/layout/Api.h>
#include <mine/ui/layout/FrameworkElement.h>
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
 * 继承 FrameworkElement 后，所有控件自动参与两遍布局协议，
 * 支持 Margin、Width/Height 约束、HorizontalAlignment/VerticalAlignment 以及
 * Panel 容器（StackPanel、Grid）的子元素容纳。
 */
class MINE_UI_LAYOUT_API Control : public FrameworkElement {
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
     * @brief 设置模板根元素，将其加入控件的视觉子树（不拥有所有权）。
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
     * @brief 设置模板根元素并转移所有权（OwnedPtr<UIElement> 版本）。
     *
     * 同 `set_template_root(UIElement*)` 但接管元素的生命周期，
     * 元素将在控件析构或模板根被替换时自动释放。
     *
     * @param root 已拥有所有权的模板根元素（nullptr 等价于清除模板根）
     */
    void set_template_root(core::OwnedPtr<UIElement> root) noexcept;

    /**
     * @brief 设置模板根元素并转移所有权（子类型重载，含类型擦除）。
     *
     * 接受 OwnedPtr<TElement>（TElement 为 UIElement 的子类），
     * 内部将所有权提升为 OwnedPtr<UIElement> 后委托给基类重载。
     * 适用于动态分配的 ContentPresenter 等具体控件作为模板根：
     * @code
     *   auto presenter = core::make_owned<ContentPresenter>();
     *   ctrl.set_template_root(std::move(presenter));
     * @endcode
     *
     * @tparam TElement UIElement 的子类型
     * @param root 已拥有所有权的子类型模板根元素
     */
    template<typename TElement,
             std::enable_if_t<std::is_base_of_v<UIElement, TElement> &&
                              !std::is_same_v<UIElement, TElement>, int> = 0>
    void set_template_root(core::OwnedPtr<TElement> root) noexcept
    {
        // 类型擦除：保留删除器，将裸指针提升为 UIElement*
        auto del = root.get_deleter();
        UIElement* raw = root.release();
        // 构造 OwnedPtr<UIElement>（删除器仍指向正确的 TElement::~TElement）
        set_template_root(core::OwnedPtr<UIElement>{ raw, del });
    }

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

    // ── VisualStateManager（mine.ui.style 任务 #13）────────────────────────

    /**
     * @brief 安装视觉状态管理器（由 ControlTemplate::BuildFn 调用）。
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
     * @brief 返回当前模板根元素指针（nullptr 表示尚未构建模板）。
     *
     * 子类可在 measure_override / on_render 中访问此值，判断是否走模板路径。
     */
    [[nodiscard]] UIElement* template_root() const noexcept;

    /**
     * @brief Measure 覆盖：含自动模板构建逻辑。
     *
     * 若模板尚未构建（template_root_ == nullptr）且 template_slot_ 非空，
     * 则自动从 TemplateRegistry 查找并调用 build_fn_。
     * 构建完成后，将 available 传入模板根并返回其期望尺寸。
     * 若无模板，返回零尺寸。
     *
     * 注意：此处 available 已由 FrameworkElement::on_measure 减去 Margin
     * 并应用 Width/Height/Min/Max 约束，返回值不含 Margin。
     *
     * @param available 经约束处理后的可用内容区域尺寸
     * @return 内容期望尺寸（不含 Margin）
     */
    math::Size measure_override(math::Size available) override;

    /**
     * @brief Arrange 覆盖：将模板根排列至内容区域。
     *
     * 被 FrameworkElement::on_arrange 调用，此时 bounds_rect() 已是经过
     * Margin 和对齐处理的内容矩形，可直接用于模板根的 arrange() 调用。
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

    /// 属性变更静态回调（供 bind_template 内部使用）
    static void on_host_prop_changed(DependencyObject*         sender,
                                     const DependencyProperty& prop,
                                     const core::Variant&      old_v,
                                     const core::Variant&      new_v,
                                     void*                     user_data) noexcept;

private:
    /// 私有实现（Pimpl 惯用法，隐藏 style 依赖细节）
    struct Impl;
    core::Pimpl<Impl> cp_;

    /// 样式槽位名（InlineString 避免堆分配）
    containers::InlineString style_slot_;

    /// 模板槽位名
    containers::InlineString template_slot_;

    /// 当前视觉状态（枚举，由 compute_visual_state 计算得出）
    ControlVisualState visual_state_{ ControlVisualState::Normal };
};

} // namespace mine::ui
