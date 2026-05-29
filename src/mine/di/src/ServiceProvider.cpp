/**
 * @file ServiceProvider.cpp
 * @brief ServiceProvider 与 Scope 的实现。
 */

#include <mine/di/ServiceProvider.h>

#include <mine/containers/Vector.h>
#include <mine/core/Assert.h>
#include <mine/core/Memory.h>
#include <mine/core/Pimpl.h>
#include <mine/core/TypeId.h>

namespace mine::di {

// ─────────────────────────────────────────────────────────────────────────────
// ServiceProvider::Impl
// ─────────────────────────────────────────────────────────────────────────────

struct ServiceProvider::Impl {
    /// 所有已注册的服务描述符（注册顺序即查找顺序，最后注册的优先）
    mine::containers::Vector<detail::ServiceDescriptor> descriptors;

    /// 单例实例缓存（按首次解析时的顺序存储）
    mine::containers::Vector<detail::InstanceEntry> singleton_cache;

    /// Transient 实例跟踪（用于在 Provider 销毁时统一释放）
    mine::containers::Vector<detail::InstanceEntry> transient_cache;

    /**
     * @brief 按服务类型查找描述符（逆序：最后注册的优先）。
     *
     * @param type 服务类型标识
     * @return 指向找到的描述符的指针，未找到返回 nullptr
     */
    const detail::ServiceDescriptor* find_descriptor(core::TypeId type) const noexcept {
        for (int i = static_cast<int>(descriptors.size()) - 1; i >= 0; --i) {
            if (descriptors[i].service_type == type) {
                return &descriptors[i];
            }
        }
        return nullptr;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ServiceProvider 构造/析构
// ─────────────────────────────────────────────────────────────────────────────

ServiceProvider::ServiceProvider(
    mine::containers::Vector<detail::ServiceDescriptor> descriptors)
    : p_{ core::make_pimpl<Impl>() }
{
    p_->descriptors = std::move(descriptors);
}

ServiceProvider::~ServiceProvider() {
    if (!p_) return;

    // 逆序释放 Transient 实例（后创建的先销毁）
    for (int i = static_cast<int>(p_->transient_cache.size()) - 1; i >= 0; --i) {
        auto& e = p_->transient_cache[i];
        if (e.destroy != nullptr) {
            e.destroy(e.impl_ptr);
        }
    }

    // 逆序释放 Singleton 实例
    for (int i = static_cast<int>(p_->singleton_cache.size()) - 1; i >= 0; --i) {
        auto& e = p_->singleton_cache[i];
        if (e.destroy != nullptr) {
            e.destroy(e.impl_ptr);
        }
    }
}

ServiceProvider::ServiceProvider(ServiceProvider&&) noexcept = default;
ServiceProvider& ServiceProvider::operator=(ServiceProvider&&) noexcept = default;

// ─────────────────────────────────────────────────────────────────────────────
// ServiceProvider::resolve_impl / try_resolve_impl
// ─────────────────────────────────────────────────────────────────────────────

void* ServiceProvider::resolve_impl(core::TypeId type) {
    void* ptr = try_resolve_impl(type);
    MINE_CHECK_MSG(ptr != nullptr, "mine.di: 服务未注册，请检查 ServiceCollection 配置");
    return ptr;
}

void* ServiceProvider::try_resolve_impl(core::TypeId type) noexcept {
    // 先查 Singleton 缓存（包含以 Singleton 生命周期存储的 Scoped 服务）
    for (const auto& e : p_->singleton_cache) {
        if (e.service_type == type) return e.service_ptr;
    }

    // 查找描述符
    const detail::ServiceDescriptor* desc = p_->find_descriptor(type);
    if (desc == nullptr) return nullptr;

    detail::FactoryResult result;

    switch (desc->lifetime) {
        case Lifetime::Singleton:
        case Lifetime::Scoped: {
            // 在根 Provider 中，Scoped 等同于 Singleton
            result = desc->factory(*this, desc->context);
            if (result.service_ptr == nullptr) return nullptr;
            p_->singleton_cache.push_back({
                type,
                result.service_ptr,
                result.impl_ptr,
                desc->destroy,
            });
            return result.service_ptr;
        }
        case Lifetime::Transient: {
            result = desc->factory(*this, desc->context);
            if (result.service_ptr == nullptr) return nullptr;
            // 跟踪 Transient 实例以便在 Provider 销毁时释放
            p_->transient_cache.push_back({
                type,
                result.service_ptr,
                result.impl_ptr,
                desc->destroy,
            });
            return result.service_ptr;
        }
    }

    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// ServiceProvider::create_scope
// ─────────────────────────────────────────────────────────────────────────────

Scope ServiceProvider::create_scope() {
    return Scope{ this };
}

// ─────────────────────────────────────────────────────────────────────────────
// Scope::Impl
// ─────────────────────────────────────────────────────────────────────────────

struct Scope::Impl {
    ServiceProvider* root{ nullptr };  ///< 非拥有指针，指向根 Provider

    /// Scoped 实例缓存（当前 Scope 内的单例）
    mine::containers::Vector<detail::InstanceEntry> scoped_cache;

    /// Transient 实例跟踪（在 Scope 析构时释放）
    mine::containers::Vector<detail::InstanceEntry> transient_cache;
};

// ─────────────────────────────────────────────────────────────────────────────
// Scope 构造/析构
// ─────────────────────────────────────────────────────────────────────────────

Scope::Scope(ServiceProvider* root)
    : p_{ core::make_pimpl<Impl>() }
{
    MINE_CHECK_MSG(root != nullptr, "mine.di: Scope 的根 Provider 不能为 nullptr");
    p_->root = root;
}

Scope::~Scope() {
    if (!p_) return;

    // 逆序释放 Transient 实例
    for (int i = static_cast<int>(p_->transient_cache.size()) - 1; i >= 0; --i) {
        auto& e = p_->transient_cache[i];
        if (e.destroy != nullptr) {
            e.destroy(e.impl_ptr);
        }
    }

    // 逆序释放 Scoped 实例
    for (int i = static_cast<int>(p_->scoped_cache.size()) - 1; i >= 0; --i) {
        auto& e = p_->scoped_cache[i];
        if (e.destroy != nullptr) {
            e.destroy(e.impl_ptr);
        }
    }
}

Scope::Scope(Scope&&) noexcept = default;
Scope& Scope::operator=(Scope&&) noexcept = default;

// ─────────────────────────────────────────────────────────────────────────────
// Scope::resolve_impl / try_resolve_impl
// ─────────────────────────────────────────────────────────────────────────────

void* Scope::resolve_impl(core::TypeId type) {
    void* ptr = try_resolve_impl(type);
    MINE_CHECK_MSG(ptr != nullptr, "mine.di: 服务未注册，请检查 ServiceCollection 配置");
    return ptr;
}

void* Scope::try_resolve_impl(core::TypeId type) noexcept {
    ServiceProvider* root = p_->root;

    // 先在根 Provider 中查找描述符以确定生命周期
    const detail::ServiceDescriptor* desc = root->p_->find_descriptor(type);
    if (desc == nullptr) return nullptr;

    switch (desc->lifetime) {
        case Lifetime::Singleton: {
            // Singleton：委托给根 Provider（共享缓存）
            return root->try_resolve_impl(type);
        }
        case Lifetime::Scoped: {
            // 先查当前 Scope 的缓存
            for (const auto& e : p_->scoped_cache) {
                if (e.service_type == type) return e.service_ptr;
            }
            // 未命中：通过根 Provider 调用工厂（工厂本身使用根 Provider 解析依赖）
            detail::FactoryResult result = desc->factory(*root, desc->context);
            if (result.service_ptr == nullptr) return nullptr;
            p_->scoped_cache.push_back({
                type,
                result.service_ptr,
                result.impl_ptr,
                desc->destroy,
            });
            return result.service_ptr;
        }
        case Lifetime::Transient: {
            detail::FactoryResult result = desc->factory(*root, desc->context);
            if (result.service_ptr == nullptr) return nullptr;
            p_->transient_cache.push_back({
                type,
                result.service_ptr,
                result.impl_ptr,
                desc->destroy,
            });
            return result.service_ptr;
        }
    }

    return nullptr;
}

} // namespace mine::di
