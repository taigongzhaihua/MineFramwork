/**
 * @file Visual.cpp
 * @brief Visual 视觉基类实现。
 *
 * 实现要点：
 *   - 依赖属性 OpacityProperty / VisibilityProperty 静态注册
 *   - Impl 内联存储变换、裁剪、脏区、子节点列表、事件处理器列表
 *   - render_to_canvas：save → transform → clip → on_render → 递归子树 → restore
 *   - invalidate_render：设置脏标志并向上传播到根节点
 *   - 路由事件：按注册顺序调用处理器，handled=true 时提前退出
 */

#include <mine/ui/visual/Visual.h>
#include <mine/ui/visual/UIElement.h>
#include <mine/paint/Canvas.h>
#include <mine/core/Assert.h>
#include <mine/core/TypeId.h>
#include <mine/core/Variant.h>
#include <mine/containers/Vector.h>
#include <mine/math/Transform2D.h>
#include <mine/math/Rect.h>
#include <mine/math/RoundedRect.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutedEventArgs.h>

namespace mine::ui {

// ============================================================================
// Impl 内部实现结构体
// ============================================================================

/// 单个路由事件处理器注册项
struct HandlerEntry {
    const RoutedEvent*   event_ptr; ///< 目标事件描述符地址（用于匹配）
    RoutedEventHandlerFn fn;        ///< 处理器函数指针
    void*                user_data; ///< 用户自定义数据
};

struct Visual::Impl {
    /// 裁剪类型枚举（矩形裁剪与圆角矩形裁剪互斥）
    enum class ClipKind : uint8_t {
        None,        ///< 无裁剪
        Rect,        ///< 矩形裁剪（clip_rect_）
        RoundedRect, ///< 圆角矩形裁剪（clip_rounded_rect_）
    };

    /// 父节点（非拥有，原始指针）
    Visual* parent_ = nullptr;

    /// 子节点列表（非拥有，原始指针；顺序即渲染顺序，后绘制的在上层）
    mine::containers::Vector<Visual*> children_;

    /// 局部仿射变换（默认单位矩阵）
    math::Transform2D transform_{ math::Transform2D::identity() };

    /// 当前裁剪类型
    ClipKind clip_kind_ = ClipKind::None;

    /// 矩形裁剪区域（仅 clip_kind_ == Rect 时有效）
    math::Rect clip_rect_{};

    /// 圆角矩形裁剪区域（仅 clip_kind_ == RoundedRect 时有效）
    math::RoundedRect clip_rounded_rect_{};

    /// 渲染脏标志：新建时为 true，render_to_canvas 后清为 false
    bool is_render_dirty_ = true;

    /// 路由事件处理器列表
    mine::containers::Vector<HandlerEntry> handlers_;
};

// ============================================================================
// 依赖属性静态注册
// ============================================================================

/// opacity 属性变更回调：钳制到 [0,1] 并在子类 on_property_changed 中处理
static void s_opacity_changed(DependencyObject*        obj,
                               const DependencyProperty& /*prop*/,
                               const core::Variant&      /*old_v*/,
                               const core::Variant&      /*new_v*/)
{
    // 变更通知已通过 affects_render=true 触发 invalidate_render()；
    // 若需额外钳制，可在 Visual::on_property_changed 中处理。
    (void)obj;
}

const DependencyProperty& Visual::OpacityProperty =
    register_property(
        "Opacity",
        core::TypeId::of<Visual>(),
        core::TypeId::of<float>(),
        core::Variant{ 1.0f },
        PropertyMetadata{
            .affects_render = true,
            .changed        = s_opacity_changed
        });

const DependencyProperty& Visual::VisibilityProperty =
    register_property(
        "Visibility",
        core::TypeId::of<Visual>(),
        core::TypeId::of<Visibility>(),
        core::Variant{ Visibility::Visible },
        PropertyMetadata{
            .affects_measure = true,
            .affects_arrange = true,
            .affects_render  = true
        });

// ============================================================================
// 生命周期
// ============================================================================

Visual::Visual()
    : p_{ core::make_pimpl<Impl>() }
{}

Visual::~Visual()
{
    // 从父节点的子列表中移除自身，防止悬空指针
    if (p_->parent_ != nullptr) {
        Visual* old_parent = p_->parent_;
        old_parent->remove_child(this);
        // 注意：remove_child 内部已将 parent_ 置为 nullptr 并调用 on_parent_changed
    }
    // 将所有子节点的父指针置为 nullptr（子节点不被析构，由调用方管理生命周期）
    for (Visual* child : p_->children_) {
        Visual* old_p = child->p_->parent_;
        child->p_->parent_ = nullptr;
        child->on_parent_changed(old_p, nullptr);
    }
}

// ============================================================================
// 视觉树管理
// ============================================================================

Visual* Visual::parent() const noexcept
{
    return p_->parent_;
}

uint32_t Visual::child_count() const noexcept
{
    return static_cast<uint32_t>(p_->children_.size());
}

Visual* Visual::child_at(uint32_t index) const noexcept
{
    MINE_ASSERT_MSG(index < p_->children_.size(), "Visual::child_at: 索引越界");
    return p_->children_[index];
}

void Visual::add_child(Visual* child)
{
    MINE_ASSERT_MSG(child != nullptr, "Visual::add_child: child 不能为 nullptr");
    MINE_ASSERT_MSG(child != this,    "Visual::add_child: 不能将自身添加为子节点");
    MINE_ASSERT_MSG(child->p_->parent_ == nullptr,
                    "Visual::add_child: child 已经有父节点，请先从父节点移除");

    child->p_->parent_ = this;
    p_->children_.push_back(child);
    // 通知子节点父节点已变更（old=nullptr → new=this）
    child->on_parent_changed(nullptr, this);
    // 添加子节点后子树已变化，触发渲染失效
    invalidate_render();
}

void Visual::remove_child(Visual* child)
{
    MINE_ASSERT_MSG(child != nullptr, "Visual::remove_child: child 不能为 nullptr");

    for (auto it = p_->children_.begin(); it != p_->children_.end(); ++it) {
        if (*it == child) {
            Visual* old_parent = child->p_->parent_;
            child->p_->parent_ = nullptr;
            p_->children_.erase(it);
            // 通知子节点父节点已变更（old=this → new=nullptr）
            child->on_parent_changed(old_parent, nullptr);
            invalidate_render();
            return;
        }
    }
    // child 不在列表中，静默忽略
}

void Visual::remove_all_children()
{
    // 先收集所有子节点，再逐个置空并通知（避免遍历时修改容器）
    containers::Vector<Visual*> children_copy{ p_->children_ };
    for (Visual* child : children_copy) {
        Visual* old_parent = child->p_->parent_;
        child->p_->parent_ = nullptr;
        child->on_parent_changed(old_parent, nullptr);
    }
    p_->children_.clear();
    invalidate_render();
}

// ============================================================================
// 父节点变更钩子
// ============================================================================

void Visual::on_parent_changed(Visual* /*old_parent*/, Visual* /*new_parent*/) noexcept
{
    // 默认空实现，子类（如 UserControl）可覆盖以响应"加入/离开视觉树"事件
}

// ============================================================================
// 局部变换
// ============================================================================

const math::Transform2D& Visual::render_transform() const noexcept
{
    return p_->transform_;
}

void Visual::set_render_transform(const math::Transform2D& t)
{
    p_->transform_ = t;
    invalidate_render();
}

// ============================================================================
// 矩形裁剪
// ============================================================================

bool Visual::has_clip_rect() const noexcept
{
    return p_->clip_kind_ == Impl::ClipKind::Rect;
}

math::Rect Visual::clip_rect() const noexcept
{
    return p_->clip_rect_;
}

void Visual::set_clip_rect(math::Rect rect)
{
    p_->clip_kind_ = Impl::ClipKind::Rect;
    p_->clip_rect_ = rect;
    invalidate_render();
}

void Visual::clear_clip_rect()
{
    if (p_->clip_kind_ == Impl::ClipKind::Rect) {
        p_->clip_kind_ = Impl::ClipKind::None;
        invalidate_render();
    }
}

// ============================================================================
// 圆角矩形裁剪
// ============================================================================

bool Visual::has_clip_rounded_rect() const noexcept
{
    return p_->clip_kind_ == Impl::ClipKind::RoundedRect;
}

math::RoundedRect Visual::clip_rounded_rect() const noexcept
{
    return p_->clip_rounded_rect_;
}

void Visual::set_clip_rounded_rect(math::RoundedRect rrect)
{
    p_->clip_kind_          = Impl::ClipKind::RoundedRect;
    p_->clip_rounded_rect_  = rrect;
    invalidate_render();
}

void Visual::clear_clip_rounded_rect()
{
    if (p_->clip_kind_ == Impl::ClipKind::RoundedRect) {
        p_->clip_kind_ = Impl::ClipKind::None;
        invalidate_render();
    }
}

// ============================================================================
// 快捷属性访问器（依赖属性）
// ============================================================================

float Visual::opacity() const
{
    return get_value(OpacityProperty).get<float>();
}

void Visual::set_opacity(float v)
{
    // 钳制到合法范围
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    set_value(OpacityProperty, core::Variant{ v });
}

Visibility Visual::visibility() const
{
    return get_value(VisibilityProperty).get<Visibility>();
}

void Visual::set_visibility(Visibility v)
{
    set_value(VisibilityProperty, core::Variant{ v });
}

// ============================================================================
// 脏区管理
// ============================================================================

bool Visual::is_render_dirty() const noexcept
{
    return p_->is_render_dirty_;
}

void Visual::invalidate_render()
{
    // 向上传播脏标志，直到已标记脏的祖先节点或根节点
    Visual* node = this;
    while (node != nullptr && !node->p_->is_render_dirty_) {
        node->p_->is_render_dirty_ = true;
        node = node->p_->parent_;
    }
    // 若当前节点本身尚未标记，补充标记
    if (node == this) {
        p_->is_render_dirty_ = true;
    }
}

// ============================================================================
// 渲染管线
// ============================================================================

void Visual::render_to_canvas(paint::Canvas& canvas)
{
    // Collapsed 不参与渲染
    if (visibility() == Visibility::Collapsed) {
        return;
    }

    canvas.save();

    // 应用局部变换（单位矩阵时 Canvas 内部仍处理，性能可接受）
    canvas.transform(p_->transform_);

    // 应用裁剪（圆角矩形裁剪与矩形裁剪互斥）
    if (p_->clip_kind_ == Impl::ClipKind::RoundedRect) {
        canvas.clip_rounded_rect(p_->clip_rounded_rect_);
    } else if (p_->clip_kind_ == Impl::ClipKind::Rect) {
        canvas.clip_rect(p_->clip_rect_);
    }

    // 渲染自身（Hidden 时跳过 on_render，但子节点依然可以渲染）
    if (visibility() == Visibility::Visible) {
        on_render(canvas);
    }

    // 递归渲染子节点（按添加顺序，后面的在视觉上层）
    for (Visual* child : p_->children_) {
        child->render_to_canvas(canvas);
    }

    canvas.restore();

    // 清除渲染脏标志
    p_->is_render_dirty_ = false;
}

void Visual::on_render(paint::Canvas& /*canvas*/)
{
    // 默认空实现：Visual 基类不绘制任何内容
}

UIElement* Visual::as_element() noexcept
{
    // 默认实现：普通 Visual 不是 UIElement
    return nullptr;
}

// ============================================================================
// 属性变更通知
// ============================================================================

void Visual::on_property_changed(const DependencyProperty& prop,
                                  const core::Variant&      old_value,
                                  const core::Variant&      new_value)
{
    // 调用基类通知（触发订阅者回调）
    DependencyObject::on_property_changed(prop, old_value, new_value);

    // 继承属性（inherits = true）：以 Inherited 优先级向下传播到所有直接子节点。
    // 子节点收到 set_value 后，其 on_property_changed 会递归继续向下传播，
    // 形成整棵子树的自动传播，无需手动遍历深层节点。
    // 如果子节点有本地（Local 及以上优先级）值，生效值不会被 Inherited 覆盖，
    // 但 Inherited 槽仍会被写入，以备本地值被 clear 后回退。
    if (prop.metadata().inherits) {
        const uint32_t n = child_count();
        for (uint32_t i = 0; i < n; ++i) {
            child_at(i)->set_value(prop, new_value, ValuePriority::Inherited);
        }
    }
}

// ============================================================================
// 路由事件处理器管理
// ============================================================================

void Visual::add_handler(const RoutedEvent&   event,
                          RoutedEventHandlerFn fn,
                          void*                user_data)
{
    MINE_ASSERT_MSG(fn != nullptr, "Visual::add_handler: fn 不能为 nullptr");
    p_->handlers_.push_back(HandlerEntry{ &event, fn, user_data });
}

void Visual::remove_handler(const RoutedEvent&   event,
                             RoutedEventHandlerFn fn,
                             void*                user_data)
{
    for (auto it = p_->handlers_.begin(); it != p_->handlers_.end(); ++it) {
        if (it->event_ptr == &event && it->fn == fn && it->user_data == user_data) {
            p_->handlers_.erase(it);
            return;
        }
    }
    // 未找到匹配项，静默忽略
}

// ============================================================================
// IRoutedEventTarget 接口实现
// ============================================================================

IRoutedEventTarget* Visual::parent_target() const noexcept
{
    // Visual 继承 IRoutedEventTarget，父节点转型合法
    return p_->parent_;
}

void Visual::invoke_handlers(const RoutedEvent& event,
                              RoutedEventArgs&   args) noexcept
{
    for (const HandlerEntry& entry : p_->handlers_) {
        if (entry.event_ptr == &event) {
            // 调用处理器：sender 传 this（void* 形式，与 RoutedEventHandlerFn 签名匹配）
            entry.fn(this, args, entry.user_data);
            // handled=true 时停止本层后续处理器的调用
            if (args.handled()) {
                break;
            }
        }
    }
}

} // namespace mine::ui
