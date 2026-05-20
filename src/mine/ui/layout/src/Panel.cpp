/**
 * @file Panel.cpp
 * @brief Panel 布局面板基类实现。
 *
 * Panel 管理 FrameworkElement* 子元素集合：
 *   - add_child：同时加入 children_ 列表和 Visual 视觉树
 *   - remove_child：从两处同时移除
 */

#include <mine/ui/layout/Panel.h>
#include <mine/core/Assert.h>

namespace mine::ui {

// ============================================================================
// 生命周期
// ============================================================================

Panel::Panel() = default;

Panel::~Panel() = default;

// ============================================================================
// 子元素管理
// ============================================================================

void Panel::add_child(FrameworkElement* child)
{
    MINE_ASSERT_MSG(child != nullptr, "Panel::add_child: child 不能为 nullptr");

    // 检查是否已存在（避免重复添加）
    for (uint32_t i = 0; i < static_cast<uint32_t>(children_.size()); ++i) {
        if (children_[i] == child) {
            return;  // 已存在，忽略
        }
    }

    children_.push_back(child);
    // 同时纳入 Visual 视觉树（Visual::add_child 接受 Visual*）
    Visual::add_child(child);
    // 子元素变化导致布局失效
    invalidate_measure();
}

void Panel::remove_child(FrameworkElement* child)
{
    if (child == nullptr) {
        return;
    }

    const uint32_t n = static_cast<uint32_t>(children_.size());
    for (uint32_t i = 0; i < n; ++i) {
        if (children_[i] == child) {
            // 将后续元素前移（保持顺序）
            for (uint32_t j = i; j + 1 < n; ++j) {
                children_[j] = children_[j + 1];
            }
            children_.pop_back();
            Visual::remove_child(child);
            invalidate_measure();
            return;
        }
    }
    // child 不存在时静默忽略
}

FrameworkElement* Panel::child_at(uint32_t index) const noexcept
{
    MINE_ASSERT_MSG(index < static_cast<uint32_t>(children_.size()),
                    "Panel::child_at: 索引越界");
    return children_[index];
}

uint32_t Panel::children_count() const noexcept
{
    return static_cast<uint32_t>(children_.size());
}

} // namespace mine::ui
