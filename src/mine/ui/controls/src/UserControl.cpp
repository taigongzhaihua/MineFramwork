/**
 * @file UserControl.cpp
 * @brief UserControl 可复用 UI 组件基类实现（任务 17.2）。
 *
 * 实现要点：
 *   - DataContextProperty 静态注册，changed 回调 → s_on_data_context_changed
 *   - on_content_changed：管理内容根元素的视觉子树加入/移除
 *   - on_parent_changed：老parent=nullptr+新parent非null→on_loaded；
 *     老parent非null+新parent=nullptr→on_unloaded
 *   - measure_override：首次测量后触发 on_initialized，委托给内容元素
 *   - arrange_override：将内容元素排列至全部内容区域
 */

#include <mine/ui/controls/UserControl.h>
#include <mine/ui/visual/Visual.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/core/TypeId.h>
#include <mine/core/Variant.h>
#include <mine/math/Size.h>
#include <mine/math/Rect.h>

namespace mine::ui {

// ============================================================================
// 依赖属性静态注册
// ============================================================================

/// DataContext 属性变更静态回调：转发到虚方法 on_data_context_changed
void UserControl::s_on_data_context_changed(DependencyObject*         sender,
                                             const DependencyProperty& /*prop*/,
                                             const core::Variant&      old_v,
                                             const core::Variant&      new_v) noexcept
{
    // 将 DependencyObject* 安全向下转型（继承链保证 sender 是 UserControl*）
    static_cast<UserControl*>(sender)->on_data_context_changed(old_v, new_v);
}

const DependencyProperty& UserControl::DataContextProperty =
    register_property<UserControl>(
        "DataContext",
        core::Variant{},          // 默认值：空 Variant（无数据上下文）
        PropertyMetadata{
            // DataContext 不影响布局/渲染，只触发通知
            .affects_measure = false,
            .affects_render  = false,
            .changed         = &UserControl::s_on_data_context_changed
        });

// ============================================================================
// 生命周期
// ============================================================================

UserControl::UserControl()
    : initialized_{ false }
{}

UserControl::~UserControl() = default;

// ============================================================================
// 数据上下文接口
// ============================================================================

void UserControl::set_data_context(const core::Variant& ctx)
{
    // 写入 Local 优先级槽（最高用户优先级）
    set_value(DataContextProperty, ctx);
}

const core::Variant& UserControl::data_context() const noexcept
{
    return get_value(DataContextProperty);
}

// ============================================================================
// 内容变更钩子（管理视觉子树）
// ============================================================================

void UserControl::on_content_changed(const core::Variant& /*old_v*/,
                                      const core::Variant& new_v) noexcept
{
    // DependencyObject::set_value 在触发 changed 回调之前已将槽值更新为 new_val，
    // 导致 old_v 引用到的实际上是更新后的槽（即等于 new_v）。
    // 因此不依赖 old_v，改为直接移除当前所有视觉子节点，再挂入新内容元素。

    // 移除当前视觉子树中的内容根（UserControl 只允许一个内容元素）
    while (child_count() > 0) {
        remove_child(child_at(0));
    }

    // 挂入新内容元素（若存在）
    if (new_v.has<UIElement*>()) {
        UIElement* new_root = new_v.get<UIElement*>();
        if (new_root != nullptr) {
            add_child(new_root);
        }
    }
    // 注意：文字内容（InlineString）无对应 UIElement*，不操作视觉子树
}

// ============================================================================
// 父节点变更钩子（on_loaded / on_unloaded 调度）
// ============================================================================

void UserControl::on_parent_changed(Visual* old_parent, Visual* new_parent) noexcept
{
    if (old_parent == nullptr && new_parent != nullptr) {
        // 从"无父"变为"有父"：加入视觉树
        on_loaded();
    } else if (old_parent != nullptr && new_parent == nullptr) {
        // 从"有父"变为"无父"：离开视觉树
        on_unloaded();
    }
    // 注意：parent 从 A 切换到 B（先移除再添加）会依次触发 unloaded 然后 loaded
}

// ============================================================================
// 布局覆盖
// ============================================================================

math::Size UserControl::measure_override(math::Size available)
{
    // 首次测量完成后触发 on_initialized（保证仅触发一次）
    // 在委托测量之后触发，确保内容尺寸已确定
    UIElement* root = content_element();
    math::Size result{};
    if (root != nullptr) {
        root->measure(available);
        result = root->desired_size();
    }

    if (!initialized_) {
        initialized_ = true;
        on_initialized();
    }

    return result;
}

math::Size UserControl::arrange_override(math::Size final_size)
{
    // 将内容根元素排列至整个内容区域
    UIElement* root = content_element();
    if (root != nullptr) {
        root->arrange(math::Rect{ 0.0f, 0.0f, final_size.width, final_size.height });
    }
    return final_size;
}

// ============================================================================
// 生命周期虚函数默认实现（空操作）
// ============================================================================

void UserControl::on_initialized() noexcept
{
    // 默认空实现，子类可覆盖
}

void UserControl::on_loaded() noexcept
{
    // 默认空实现，子类可覆盖
}

void UserControl::on_unloaded() noexcept
{
    // 默认空实现，子类可覆盖
}

void UserControl::on_data_context_changed(const core::Variant& /*old_ctx*/,
                                           const core::Variant& /*new_ctx*/) noexcept
{
    // 默认空实现，子类可覆盖
}

} // namespace mine::ui
