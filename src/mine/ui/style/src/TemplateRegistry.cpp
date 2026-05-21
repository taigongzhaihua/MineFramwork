/**
 * @file TemplateRegistry.cpp
 * @brief TemplateRegistry 控件模板注册表实现。
 *
 * 存储策略：
 *   使用 Vector<OwnedPtr<ControlTemplate>> 实现引用地址稳定存储。
 *   Vector 扩容时只移动 OwnedPtr（内含裸指针），ControlTemplate 对象
 *   保持在堆上的原始地址，因此 register_template() 返回的引用永久有效。
 */

#include <mine/ui/style/TemplateRegistry.h>
#include <mine/core/Memory.h>
#include <mine/containers/Vector.h>
#include <mine/containers/InlineString.h>

namespace mine::ui::style {

// ============================================================================
// Impl 内部实现结构体
// ============================================================================

struct TemplateRegistry::Impl {
    /// 已注册模板列表（OwnedPtr 保证堆地址稳定，Vector 扩容不影响引用有效性）
    containers::Vector<core::OwnedPtr<ControlTemplate>> entries;
};

// ============================================================================
// 生命周期
// ============================================================================

TemplateRegistry::TemplateRegistry()
    : p_{ core::make_pimpl<Impl>() }
{}

// ============================================================================
// 单例访问
// ============================================================================

TemplateRegistry& TemplateRegistry::instance() noexcept
{
    // 函数内局部静态变量，C++11 保证线程安全的单次初始化
    static TemplateRegistry s_instance;
    return s_instance;
}

// ============================================================================
// 注册
// ============================================================================

const ControlTemplate& TemplateRegistry::register_template(
    core::StringView         name,
    core::TypeId             target_type,
    ControlTemplate::BuildFn build_fn) noexcept
{
    // 在堆上构造 ControlTemplate，确保引用地址稳定
    auto owned = core::make_owned<ControlTemplate>();
    owned->set_name(name).set_target_type(target_type);
    owned->build_fn_ = build_fn;

    p_->entries.push_back(std::move(owned));

    // 返回对堆上对象的常量引用（地址在 OwnedPtr 生命周期内永久稳定）
    return *p_->entries.back();
}

// ============================================================================
// 查找
// ============================================================================

const ControlTemplate* TemplateRegistry::find(core::StringView name) const noexcept
{
    // 线性扫描，从后往前以返回最后注册的同名模板
    const auto& entries = p_->entries;
    for (size_t i = entries.size(); i > 0; --i) {
        const auto& entry = entries[i - 1];
        if (entry && entry->name() == name) {
            return entry.get();
        }
    }
    return nullptr;
}

const ControlTemplate* TemplateRegistry::find_default(core::TypeId type) const noexcept
{
    // 线性扫描，按目标类型查找；从后往前以返回最后注册的匹配模板
    const auto& entries = p_->entries;
    for (size_t i = entries.size(); i > 0; --i) {
        const auto& entry = entries[i - 1];
        if (entry && entry->target_type() == type) {
            return entry.get();
        }
    }
    return nullptr;
}

}  // namespace mine::ui::style
