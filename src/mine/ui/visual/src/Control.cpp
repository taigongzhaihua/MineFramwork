/**
 * @file Control.cpp
 * @brief Control 控件基类实现。
 *
 * 任务 #12：ControlTemplate + TemplateRegistry 支持。
 * 添加 set_template_root / find_template_child / bind_template 实现。
 */

#include <mine/ui/visual/Control.h>
#include <mine/ui/property/ValuePriority.h>
#include <mine/containers/SmallVector.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/style/TemplateRegistry.h>
#include <mine/core/Memory.h>

namespace mine::ui {

// ============================================================================
// Control::Impl 私有实现结构体
// ============================================================================

struct Control::Impl {
    /// 模板根元素（非拥有指针，生命周期由 owned_template_root_ 或外部管理）
    UIElement* template_root_{nullptr};

    /// 模板根元素的所有权（当通过 OwnedPtr 重载设置时有效，否则为 nullptr）
    core::OwnedPtr<UIElement> owned_template_root_{nullptr};

    /// 模板属性绑定记录（宿主 host_prop → 子元素 child_prop 单向同步）
    struct TemplateBinding {
        DependencyObject*         child;       ///< 模板子元素
        const DependencyProperty* child_prop;  ///< 子元素目标属性
        const DependencyProperty* host_prop;   ///< 宿主控件源属性
    };
    containers::SmallVector<TemplateBinding, 8> bindings_;

    /// 属性变更订阅 token（0 表示尚未订阅；首次 bind_template 时创建）
    uint32_t binding_sub_token_{0};

    /// VisualStateManager 实例（堆分配，影视为可空 nullp——未安装时为 nullptr）
    core::OwnedPtr<style::VisualStateManager> vsm_;
};

// ============================================================================
// 文件内部辅助函数
// ============================================================================

namespace {

/// 在以 node 为根的子树中按 template_name 深度优先搜索
UIElement* find_child_by_name(UIElement& node, core::StringView name)
{
    // 检查当前节点
    if (node.template_name() == name) {
        return &node;
    }
    // 递归检查所有子节点
    const uint32_t count = node.child_count();
    for (uint32_t i = 0; i < count; ++i) {
        Visual* v = node.child_at(i);
        UIElement* child = v->as_element();
        if (child) {
            UIElement* found = find_child_by_name(*child, name);
            if (found) return found;
        }
    }
    return nullptr;
}

}  // namespace

/// 属性变更订阅静态回调：遍历绑定列表，同步匹配的子元素属性
void Control::on_host_prop_changed(DependencyObject* /*sender*/,
                                   const DependencyProperty& prop,
                                   const core::Variant&      /*old_v*/,
                                   const core::Variant&      new_v,
                                   void*                     user_data) noexcept
{
    // user_data = Control::Impl*（堆分配，地址永久稳定）
    auto* impl = static_cast<Control::Impl*>(user_data);
    for (auto& binding : impl->bindings_) {
        if (binding.host_prop == &prop) {
            // 以 TemplateBind(40) 优先级写入，低于 Local(50)，不覆盖本地值
            binding.child->set_value(*binding.child_prop,
                                     new_v,
                                     ValuePriority::TemplateBind);
        }
    }
}

// ============================================================================
// 生命周期
// ============================================================================

Control::Control()
    : cp_{ core::make_pimpl<Impl>() }
{}

Control::~Control()
{
    // 取消属性变更订阅，防止 Impl 析构后回调访问已释放内存
    if (cp_ && cp_->binding_sub_token_ != 0) {
        unsubscribe_property_changed(cp_->binding_sub_token_);
        cp_->binding_sub_token_ = 0;
    }
}

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

void Control::set_template_slot(core::StringView slot)
{
	template_slot_ = slot;
}

core::StringView Control::template_slot() const noexcept
{
	return core::StringView{ template_slot_.data(), template_slot_.size() };
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
	// 状态变化至少触发一次重绘，具体样式映射在 mine.ui.style 阶段接入。
	invalidate_render();
}

// ============================================================================
// 控件模板
// ============================================================================

void Control::set_template_root(UIElement* root) noexcept
{
    // 先移除旧模板根（如有）
    if (cp_->template_root_) {
        remove_child(cp_->template_root_);
    }
    // 清除旧的所有权（若有）
    cp_->owned_template_root_.reset();

    cp_->template_root_ = root;
    // 将新模板根加入视觉子树
    if (root) {
        add_child(root);
    }
}

void Control::set_template_root(core::OwnedPtr<UIElement> root) noexcept
{
    // 先移除旧模板根（如有）
    if (cp_->template_root_) {
        remove_child(cp_->template_root_);
    }
    // 转移所有权到 Impl（元素生命周期由 Control 管理）
    cp_->owned_template_root_ = std::move(root);
    cp_->template_root_ = cp_->owned_template_root_.get();

    // 将新模板根加入视觉子树
    if (cp_->template_root_) {
        add_child(cp_->template_root_);
    }
}

UIElement* Control::find_template_child(core::StringView name) const noexcept
{
    if (!cp_->template_root_) {
        return nullptr;
    }
    return find_child_by_name(*cp_->template_root_, name);
}

void Control::bind_template(DependencyObject&         child,
                            const DependencyProperty& child_prop,
                            const DependencyProperty& host_prop) noexcept
{
    // 1. 立即同步当前宿主属性值到子元素（首次同步）
    child.set_value(child_prop,
                    get_value(host_prop),
                    ValuePriority::TemplateBind);

    // 2. 记录绑定（后续变更通过订阅回调传播）
    cp_->bindings_.push_back(Control::Impl::TemplateBinding{
        &child, &child_prop, &host_prop
    });

    // 3. 首次 bind_template 时订阅宿主控件的属性变更（仅订阅一次）
    if (cp_->binding_sub_token_ == 0) {
        cp_->binding_sub_token_ = subscribe_property_changed(
            &on_host_prop_changed,
            cp_.get()  // user_data = Impl*（堆分配，地址永久稳定）
        );
    }
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
// 模板根访问器（protected，供子类在 on_measure/on_render 中使用）
// ============================================================================

UIElement* Control::template_root() const noexcept
{
    return cp_->template_root_;
}

// ============================================================================
// 自动模板构建的 on_measure 实现
// ============================================================================

void Control::on_measure(math::Size available_size)
{
    // 若模板槽位非空且模板根尚未构建，从 TemplateRegistry 查找并构建模板
    if (!cp_->template_root_ && !template_slot_.empty()) {
        const core::StringView slot{ template_slot_.data(), template_slot_.size() };
        const style::ControlTemplate* tmpl =
            style::TemplateRegistry::instance().find(slot);
        if (tmpl && tmpl->build_fn_) {
            // build_fn_ 内部会调用 set_template_root 将根元素加入视觉子树
            tmpl->build_fn_(*this);
        }
    }
    // 若模板根已构建，测量模板根并采用其期望尺寸
    if (cp_->template_root_) {
        cp_->template_root_->measure(available_size);
        set_desired_size(cp_->template_root_->desired_size());
        return;
    }
    // 无模板：默认零尺寸
    UIElement::on_measure(available_size);
}

}  // namespace mine::ui

