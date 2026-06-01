/**
 * @file Control.cpp
 * @brief Control 控件基类实现。
 *
 * Control 位于 mine.ui.visual 模块，继承 FrameworkElement，
 * 从而使所有控件自动参与两遗布局协议（Margin、Width/Height、对齐方式）。
 *
 * 架构说明：
 *   - ControlTemplate / TemplateRegistry / bind_template / on_apply_template 已移除。
 *   - 内部子元素（ContentPresenter 等）由具体控件（如 Button）在构造函数中直接创建并安装。
 *   - 自定义外观请继承现有控件并重写 on_render()（类似 QWidget 风格）。
 */

#include <mine/ui/visual/Control.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/core/Memory.h>

namespace mine::ui {

// ============================================================================
// Control::Impl 私有实现结构体
// ============================================================================

struct Control::Impl {
    /// 内部子元素（非拥有指针，生命周期由 owned_inner_element_ 或外部管理）
    UIElement* inner_element_{nullptr};

    /// 内部子元素的所有权（当通过 OwnedPtr 重载设置时有效，否则为 nullptr）
    core::OwnedPtr<UIElement> owned_inner_element_{nullptr};

    /// VisualStateManager 实例（堆分配，为 nullptr 时表示未安装）
    core::OwnedPtr<style::VisualStateManager> vsm_;
};

// ============================================================================
// 生命周期
// ============================================================================

Control::Control()
    : cp_{ core::make_pimpl<Impl>() }
{}

Control::~Control() = default;

// ============================================================================
// 样式槽位
// ============================================================================

void Control::set_style_slot(core::StringView slot)
{
    style_slot_ = slot;
}

core::StringView Control::style_slot() const noexcept
{
    return core::StringView{ style_slot_.data(), style_slot_.size() };
}

// ============================================================================
// 视觉状态
// ============================================================================

ControlVisualState Control::visual_state() const noexcept
{
    return visual_state_;
}

void Control::update_visual_state()
{
    const ControlVisualState next = compute_visual_state();

    // VSM 路径：当 VSM 已安装时，通过状态名字符串驱动状态机
    if (cp_->vsm_) {
        const core::StringView state_name = compute_state_name();
        cp_->vsm_->go_to_state(state_name);
    }

    if (next == visual_state_) {
        return;
    }
    const ControlVisualState old = visual_state_;
    visual_state_ = next;
    on_visual_state_changed(old, next);
}

ControlVisualState Control::compute_visual_state() const
{
    return ControlVisualState::Normal;
}

core::StringView Control::compute_state_name() const noexcept
{
    // 默认实现：将 compute_visual_state() 返回的枚举映射为状态名字符串
    switch (compute_visual_state()) {
        case ControlVisualState::Normal:   return core::StringView{"Normal",   6};
        case ControlVisualState::Hovered:  return core::StringView{"Hovered",  7};
        case ControlVisualState::Pressed:  return core::StringView{"Pressed",  7};
        case ControlVisualState::Focused:  return core::StringView{"Focused",  7};
        case ControlVisualState::Disabled: return core::StringView{"Disabled", 8};
        default:                           return core::StringView{"Normal",   6};
    }
}

void Control::on_visual_state_changed(ControlVisualState /*old_state*/,
                                      ControlVisualState /*new_state*/)
{
    // 状态变化至少触发一次重绘，具体样式映射在 mine.ui.style 阶段接入
    invalidate_render();
}

// ============================================================================
// 内部子元素管理（set_inner_element / inner_element）
// ============================================================================

void Control::set_inner_element(UIElement* root) noexcept
{
    // 先移除旧内部元素（如有）
    if (cp_->inner_element_) {
        remove_child(cp_->inner_element_);
    }
    // 清除旧的所有权（若有）
    cp_->owned_inner_element_.reset();

    cp_->inner_element_ = root;
    // 将新内部元素加入视觉子树，并标记为命中穿透：
    // 内部实现元素（ContentPresenter 等）不应作为命中目标，
    // 确保鼠标事件派发给控件本身（等价 Qt WA_TransparentForMouseEvents）
    if (root) {
        root->set_hit_transparent(true);
        add_child(root);
    }
}

void Control::set_inner_element(core::OwnedPtr<UIElement> root) noexcept
{
    // 先移除旧内部元素（如有）
    if (cp_->inner_element_) {
        remove_child(cp_->inner_element_);
    }
    // 转移所有权到 Impl（元素生命周期由 Control 管理）
    cp_->owned_inner_element_ = std::move(root);
    cp_->inner_element_ = cp_->owned_inner_element_.get();

    // 将新内部元素加入视觉子树，并标记为命中穿透：
    // 内部实现元素（ContentPresenter 等）不应作为命中目标，
    // 确保鼠标事件派发给控件本身（等价 Qt WA_TransparentForMouseEvents）
    if (cp_->inner_element_) {
        cp_->inner_element_->set_hit_transparent(true);
        add_child(cp_->inner_element_);
    }
}

UIElement* Control::inner_element() const noexcept
{
    return cp_->inner_element_;
}

// ============================================================================
// VisualStateManager
// ============================================================================

void Control::set_visual_state_manager(style::VisualStateManager vsm) noexcept
{
    // 将 VSM 移动到堆上，通过 OwnedPtr 管理生命周期
    cp_->vsm_ = core::make_owned<style::VisualStateManager>(std::move(vsm));
}

style::VisualStateManager* Control::vsm() noexcept
{
    return cp_->vsm_.get();
}

const style::VisualStateManager* Control::vsm() const noexcept
{
    return cp_->vsm_.get();
}

// ============================================================================
// Measure 覆盖：委托给内部子元素
// ============================================================================

math::Size Control::measure_override(math::Size available)
{
    // 若内部元素已安装，委托其 measure() 并返回其期望尺寸
    if (cp_->inner_element_) {
        cp_->inner_element_->measure(available);
        return cp_->inner_element_->desired_size();
    }
    // 无内部元素：返回零尺寸（叶子控件应覆盖 measure_override 提供内容尺寸）
    return {0.0f, 0.0f};
}

// ============================================================================
// Arrange 覆盖：将内部子元素排列至内容区域
// ============================================================================

math::Size Control::arrange_override(math::Size final_size)
{
    if (cp_->inner_element_) {
        // bounds_rect() 已在 FrameworkElement::on_arrange 中被设置为内容矩形
        // 将该矩形传入内部元素，确保 ContentPresenter 等拥有正确的 bounds_rect
        cp_->inner_element_->arrange(bounds_rect());
    }
    return final_size;
}

}  // namespace mine::ui
