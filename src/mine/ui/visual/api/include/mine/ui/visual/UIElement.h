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
     * 默认实现：
     *   1. 若 Visibility == Collapsed，返回 nullptr
     *   2. 将输入点逆变换到本节点局部坐标系
     *   3. 若有裁剪区域且点在裁剪外，返回 nullptr
     *   4. 逆序遍历子节点，跳过命中穿透（is_hit_transparent）的子节点，
     *      首个命中子节点立即返回
     *   5. 子节点均未命中时，调用 hit_test_local() 判断本节点自身
     *
     * 控件通常不需要覆盖此方法：通过 set_hit_transparent() 标记内部实现子元素，
     * 并在 on_arrange() 中调用 set_clip_rounded_rect() 定义控件形状，
     * 即可使基类实现正确完成命中测试。
     *
     * @param p 屏幕坐标系中的测试点（相对于父节点坐标系）
     * @return  命中的最内层 UIElement 指针；若无命中则返回 nullptr
     */
    virtual UIElement* hit_test(math::Point p);

    /**
     * @brief 设置命中穿透标志（等价 Qt WA_TransparentForMouseEvents）。
     *
     * 设为 true 后，此元素在 hit_test 遍历子树时被跳过，不会作为命中目标；
     * 但仍参与正常渲染。适用于 Control 内部的 ContentPresenter 等实现细节元素，
     * 确保鼠标事件始终派发给控件本身而非内部元素。
     *
     * Control::set_inner_element() 会自动为内部元素设置此标志，
     * 通常不需要手动调用。
     */
    void set_hit_transparent(bool transparent) noexcept;

    /**
     * @brief 返回命中穿透标志（true 表示此元素对命中测试不可见）。
     */
    [[nodiscard]] bool is_hit_transparent() const noexcept;

    // ── 键盘焦点 ─────────────────────────────────────────────────────────

    /**
     * @brief 设置可聚焦标志。
     *
     * 为 true 时，InputRouter 在 MouseDown 命中此元素后会自动将其设为键盘焦点，
     * 后续键盘输入（KeyDown/KeyUp/TextInput）将发往此元素。
     * 默认为 false（不可聚焦，不参与焦点切换）。
     */
    void set_focusable(bool focusable) noexcept;

    /**
     * @brief 返回可聚焦标志（默认 false）。
     */
    [[nodiscard]] bool is_focusable() const noexcept;

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
     *
     * 默认实现优先级：
     *   1. 若设置了 clip_rounded_rect：测试点是否在圆角矩形内（外角不命中）
     *   2. 否则：测试点是否在 bounds_rect() 范围内
     *
     * 控件通常在 on_arrange() 中通过 set_clip_rounded_rect() 设置圆角裁剪，
     * 使命中测试与视觉边界自动保持一致，无需覆盖此方法。
     *
     * @param p 已变换到本节点局部坐标系的测试点
     * @return  true 表示命中本节点
     */
    virtual bool hit_test_local(math::Point p) const;

public:
    // ── 覆盖 DependencyObject 布局失效接口 ───────────────────────────────

    /// 布局测量失效：设置内部脏标志（mine.ui.layout 接管后转为队列通知）
    void invalidate_measure() override;

    /// 布局排列失效：设置内部脏标志
    void invalidate_arrange() override;
protected:
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
