/**
 * @file UIElement.cpp
 * @brief UIElement 实现。
 *
 * UIElement 扩展 Visual，添加布局边界（bounds_rect）、期望尺寸（desired_size）
 * 以及递归命中测试（hit_test / hit_test_local）。
 *
 * 布局脏区管理：M1.1 阶段仅维护内部脏标志，
 * mine.ui.layout 接管后会将这些标志连接到布局队列。
 */

#include <mine/ui/visual/UIElement.h>
#include <mine/core/Assert.h>

namespace mine::ui {

// ============================================================================
// Impl 内部实现结构体
// ============================================================================

struct UIElement::Impl {
    /// 由布局系统写入的最终排列矩形（相对于父节点坐标系）
    math::Rect bounds_rect_{};

    /// Measure 阶段计算出的期望尺寸
    math::Size desired_size_{};

    /// 测量失效标志
    bool measure_dirty_ = true;

    /// 排列失效标志
    bool arrange_dirty_ = true;
};

// ============================================================================
// 生命周期
// ============================================================================

UIElement::UIElement()
    : p_{ core::make_pimpl<Impl>() }
{}

UIElement::~UIElement() = default;

// ============================================================================
// 无 RTTI 类型探查
// ============================================================================

UIElement* UIElement::as_element() noexcept
{
    return this;
}

// ============================================================================
// 布局边界
// ============================================================================

math::Rect UIElement::bounds_rect() const noexcept
{
    return p_->bounds_rect_;
}

void UIElement::set_bounds_rect(math::Rect rect)
{
    p_->bounds_rect_   = rect;
    p_->arrange_dirty_ = false;
    on_arrange(rect);
    // 布局变化后触发重绘
    invalidate_render();
}

math::Size UIElement::desired_size() const noexcept
{
    return p_->desired_size_;
}

// ============================================================================
// 命中测试
// ============================================================================

UIElement* UIElement::hit_test(math::Point p)
{
    // Collapsed 时整棵子树均不参与命中测试
    if (visibility() == Visibility::Collapsed) {
        return nullptr;
    }

    // 将输入点逆变换到本节点局部坐标系
    // 若变换矩阵奇异（不可逆），跳过变换（保守策略：仍测试）
    math::Point local_p = p;
    const auto inv = render_transform().inverted();
    if (inv) {
        local_p = inv.value().apply(p);
    }

    // 若有裁剪区域且点在裁剪外，直接返回 nullptr
    if (has_clip_rect()) {
        const math::Point clip_test{ local_p.x, local_p.y };
        if (!clip_rect().contains(clip_test)) {
            return nullptr;
        }
    }

    // 按逆序遍历子节点（后绘制的在上层，优先命中）
    const uint32_t count = child_count();
    for (uint32_t i = count; i > 0; --i) {
        Visual* child_visual = child_at(i - 1u);
        // 仅 UIElement 子节点参与命中测试（使用虚方法替代 RTTI）
        UIElement* child_elem = child_visual->as_element();
        if (child_elem == nullptr) {
            continue;
        }
        UIElement* hit = child_elem->hit_test(local_p);
        if (hit != nullptr) {
            return hit;
        }
    }

    // 子树均未命中，判断本节点自身
    if (hit_test_local(local_p)) {
        return this;
    }

    return nullptr;
}

// ============================================================================
// 布局虚方法（Mine.ui.layout 接管后覆盖）
// ============================================================================

void UIElement::on_measure(math::Size /*available_size*/)
{
    // 默认：期望尺寸为零（等待布局模块覆盖）
    set_desired_size({});
}

void UIElement::on_arrange(math::Rect /*final_rect*/)
{
    // 默认空操作；子类在排列确定后更新内部状态（如子元素位置）
}

bool UIElement::hit_test_local(math::Point p) const
{
    // 默认：测试点是否在 bounds_rect() 内
    return p_->bounds_rect_.contains(p);
}

// ============================================================================
// 布局失效
// ============================================================================

void UIElement::invalidate_measure()
{
    p_->measure_dirty_ = true;
    p_->arrange_dirty_ = true;
    // 测量失效同时触发渲染失效（子树大小可能变化）
    invalidate_render();
}

void UIElement::invalidate_arrange()
{
    p_->arrange_dirty_ = true;
    invalidate_render();
}

// ============================================================================
// 公共布局入口（FrameworkElement 通过 on_measure 覆盖实现约束处理）
// ============================================================================

void UIElement::measure(math::Size available_size)
{
    on_measure(available_size);
}

// ============================================================================
// 受保护辅助：设置期望尺寸（供 FrameworkElement 的 on_measure 覆盖调用）
// ============================================================================

void UIElement::set_desired_size(math::Size size) noexcept
{
    p_->desired_size_  = size;
    p_->measure_dirty_ = false;
}

} // namespace mine::ui
