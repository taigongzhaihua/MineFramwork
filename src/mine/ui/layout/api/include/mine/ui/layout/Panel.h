/**
 * @file Panel.h
 * @brief Panel —— 布局面板基类，管理 UIElement 子元素集合。
 *
 * Panel 继承自 FrameworkElement，维护一个 UIElement* 子元素列表。
 * 添加子元素时同时调用 Visual::add_visual_child() 将其纳入视觉树。
 *
 * 具体布局逻辑（StackPanel、Grid 等）通过覆盖 measure_override / arrange_override 实现。
 *
 * 继承关系：
 *   FrameworkElement (mine.ui.layout)
 *       └─ Panel
 *           ├─ StackPanel
 *           └─ Grid
 */

#pragma once

#include <mine/ui/layout/Api.h>
#include <mine/ui/layout/FrameworkElement.h>
#include <mine/containers/SmallVector.h>

namespace mine::ui {

/**
 * @brief 布局面板基类。
 *
 * Panel 提供子元素管理（add/remove/at/count），子类负责在
 * measure_override / arrange_override 中实现具体布局算法。
 *
 * 注意事项：
 *   - Panel 不拥有子元素的生命周期（存储裸指针）
 *   - 子元素析构时应先从 Panel 中移除（或确保 Panel 生命周期短于子元素）
 *   - 子元素要求是 UIElement 的实例（或子类）
 */
class MINE_UI_LAYOUT_API Panel : public FrameworkElement {
public:
    // ── 生命周期 ──────────────────────────────────────────────────────────

    Panel();
    ~Panel() override;

    Panel(const Panel&)            = delete;
    Panel& operator=(const Panel&) = delete;
    Panel(Panel&&)                 = default;
    Panel& operator=(Panel&&)      = default;

    // ── 子元素管理 ────────────────────────────────────────────────────────

    /**
     * @brief 向面板末尾添加子元素。
     *
     * 同时调用 Visual::add_visual_child() 将其纳入视觉树。
     * 添加后触发 invalidate_measure()。
     *
     * @param child 要添加的子元素（非空，不重复添加）
     */
    void add_child(UIElement* child);

    /**
     * @brief 从面板移除子元素。
     *
     * 同时调用 Visual::remove_visual_child() 从视觉树移除。
     * 移除后触发 invalidate_measure()。
     *
     * @param child 要移除的子元素；若不存在则忽略
     */
    void remove_child(UIElement* child);

    /**
     * @brief 按索引获取子元素。
     *
     * @param index 子元素索引（范围：[0, child_count())）
     * @return      对应子元素指针
     */
    [[nodiscard]] UIElement* child_at(uint32_t index) const noexcept;

    /// 子元素数量
    [[nodiscard]] uint32_t children_count() const noexcept;

protected:
    /// 子元素指针列表（非拥有，裸指针）
    containers::SmallVector<UIElement*, 8> children_;
};

} // namespace mine::ui
