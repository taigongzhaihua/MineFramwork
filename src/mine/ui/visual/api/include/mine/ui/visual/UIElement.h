/**
 * @file UIElement.h
 * @brief UIElement —— 具有布局接口与命中测试能力的视觉元素基类。
 *
 * UIElement 在 Visual 的基础上添加：
 *   - 布局接口桩（Measure/Arrange，mine.ui.layout 实现后覆盖）
 *   - 布局边界（bounds_rect：由布局系统设置的最终排列矩形）
 *   - 命中测试（hit_test：将屏幕坐标映射到局部坐标后判断是否命中）
 *
 * 继承关系：
 *   Visual
 *       └─ UIElement  (mine.ui.visual)
 *           └─ Control
 */

#pragma once

#include <mine/ui/visual/Api.h>
#include <mine/ui/visual/Visual.h>
#include <mine/math/Size.h>
#include <mine/math/Point.h>
#include <mine/core/StringView.h>

namespace mine::ui {

/**
 * @brief 具有布局接口与命中测试能力的视觉元素基类。
 *
 * 命中测试流程（hit_test）：
 *   1. 若 Visibility == Collapsed，返回 nullptr
 *   2. 将输入点逆变换到本节点局部坐标系
 *   3. 若有裁剪区域且点在裁剪区域之外，返回 nullptr
 *   4. 按逆序遍历子节点，递归调用 hit_test；首个命中子节点立即返回
 *   5. 若子节点均未命中，调用 hit_test_local() 判断本节点自身是否命中
 *
 * @note 命中测试不依赖路由事件系统，仅用于确定"哪个元素在指定坐标处"。
 *       mine.ui.input 模块通过命中测试结果决定事件派发目标。
 */
class MINE_UI_VISUAL_API UIElement : public Visual {
public:
    // ── 生命周期 ──────────────────────────────────────────────────────────

    UIElement();
    ~UIElement() override;

    UIElement(const UIElement&)            = delete;
    UIElement& operator=(const UIElement&) = delete;
    UIElement(UIElement&&)                 = default;
    UIElement& operator=(UIElement&&)      = default;

    /**
     * @brief 返回 this，供 Visual::as_element() 虚方法链使用（无 RTTI）。
     */
    [[nodiscard]] UIElement* as_element() noexcept override;

    // ── 布局边界 ─────────────────────────────────────────────────────────

    /**
     * @brief 返回由布局系统设置的最终排列矩形（相对于父节点坐标系）。
     *
     * 在布局系统（mine.ui.layout）Arrange 阶段完成后由 set_bounds_rect() 写入。
     * 未经布局时默认为零矩形 {0, 0, 0, 0}。
     */
    [[nodiscard]] math::Rect bounds_rect() const noexcept;

    /**
     * @brief 由布局系统调用，设置元素的最终排列矩形。
     *
     * 同时调用 on_arrange(rect) 让子类更新内部状态。
     */
    void set_bounds_rect(math::Rect rect);

    /**
     * @brief 返回期望尺寸（由 Measure 阶段写入）。
     *
     * 默认为 {0, 0}，由布局系统完成 Measure 后更新。
     */
    [[nodiscard]] math::Size desired_size() const noexcept;

    /**
     * @brief 公共 Measure 入口，驱动元素完成测量并更新 desired_size。
     *
     * 调用受保护的 on_measure(available_size)，mine.ui.layout 中的
     * FrameworkElement 覆盖 on_measure 以实现完整的约束与边距处理。
     *
     * @param available_size 父节点提供的可用空间
     */
    void measure(math::Size available_size);

    /**
     * @brief 公共 Arrange 入口，将元素排列到指定矩形区域内。
     *
     * 默认实现直接调用 set_bounds_rect(slot)。
     * mine.ui.layout 中的 FrameworkElement 覆盖此方法，在调用 set_bounds_rect
     * 之前处理 Margin 和 HorizontalAlignment/VerticalAlignment 对齐。
     *
     * @param slot 父节点为本元素分配的矩形区域（父节点坐标系）
     */
    virtual void arrange(math::Rect slot);

    // ── 命中测试 ─────────────────────────────────────────────────────────

    /**
     * @brief 在指定屏幕坐标处进行命中测试，返回最顶层命中的 UIElement。
     *
     * 声明为 virtual，允许控件子类（如 Button）覆盖以控制命中测试语义。
     * 例如：Button 覆盖此方法以屏蔽模板子元素的命中，确保 Direct 路由事件
     * （MouseEnter / MouseLeave）始终派发给控件本身而非内部的 ContentPresenter。
     *
     * @param p 屏幕坐标系中的测试点（相对于父节点坐标系）
     * @return  命中的最内层 UIElement 指针；若无命中则返回 nullptr
     */
    virtual UIElement* hit_test(math::Point p);

    // ── 模板命名 ──────────────────────────────────────────────────────────

    /**
     * @brief 设置模板树中的元素标识名称（供 Control::find_template_child 使用）。
     *
     * 通常由 mmlc 生成的 build_fn_ 在构建模板树时调用，
     * 使控件可以通过名称定位特定的模板子元素。
     *
     * @param name 元素名称（如 "border"、"content"）
     */
    void set_template_name(core::StringView name);

    /**
     * @brief 返回模板树中的元素标识名称。
     *
     * 默认为空字符串（未命名元素）。
     *
     * @return 元素名称的 StringView（引用内部存储，生命周期与对象相同）
     */
    [[nodiscard]] core::StringView template_name() const noexcept;

protected:
    // ── 布局虚方法（由 mine.ui.layout 覆盖）─────────────────────────────

    /**
     * @brief Measure 阶段虚方法，计算元素的期望尺寸。
     *
     * 子类覆盖此方法实现自定义尺寸计算逻辑。
     * 默认实现将 desired_size 置为 {0, 0}。
     *
     * @param available_size 父节点提供的可用空间
     */
    virtual void on_measure(math::Size available_size);

    /**
     * @brief Arrange 阶段虚方法，响应布局系统的最终排列。
     *
     * 子类覆盖此方法在布局确定后更新内部状态（如子元素位置）。
     * 默认实现为空操作。
     *
     * @param final_rect 布局系统分配的最终矩形
     */
    virtual void on_arrange(math::Rect final_rect);

    /**
     * @brief 局部坐标系下的命中测试判断。
     *
     * 子类覆盖此方法实现自定义命中测试逻辑（如非矩形控件）。
     * 默认实现：若 p 落在 bounds_rect() 范围内则命中。
     *
     * @param p 已变换到本节点局部坐标系的测试点
     * @return  true 表示命中本节点
     */
    virtual bool hit_test_local(math::Point p) const;

    // ── 覆盖 DependencyObject 布局失效接口 ───────────────────────────────

    /// 布局测量失效：设置内部脏标志（mine.ui.layout 接管后转为队列通知）
    void invalidate_measure() override;

    /// 布局排列失效：设置内部脏标志
    void invalidate_arrange() override;
    /**
     * @brief 设置期望尺寸（供 FrameworkElement 等子类在 on_measure 中调用）。
     *
     * 同时清除 measure_dirty_ 标志。
     *
     * @param size 计算出的期望尺寸
     */
    void set_desired_size(math::Size size) noexcept;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
