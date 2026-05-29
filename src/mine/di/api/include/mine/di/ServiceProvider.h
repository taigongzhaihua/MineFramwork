/**
 * @file ServiceProvider.h
 * @brief 服务提供者及相关内部类型声明。
 *
 * 包含：
 *   - detail 命名空间：工厂/析构函数类型、服务描述符、实例缓存条目
 *   - ServiceProvider：服务解析容器（由 ServiceCollection::build() 创建）
 *   - Scope：作用域容器（由 ServiceProvider::create_scope() 创建）
 */

#pragma once

#include <type_traits>

#include <mine/containers/Vector.h>
#include <mine/core/Assert.h>
#include <mine/core/Memory.h>
#include <mine/core/Pimpl.h>
#include <mine/core/TypeId.h>

#include <mine/di/Api.h>
#include <mine/di/Lifetime.h>

namespace mine::di {

// ── 前向声明 ─────────────────────────────────────────────────────────────────

class ServiceProvider;
class Scope;

// ── 内部实现细节（detail 命名空间）──────────────────────────────────────────

namespace detail {

/**
 * @brief 工厂函数返回值。
 *
 * 同时持有服务接口指针（service_ptr）和实现类指针（impl_ptr），
 * 以确保在多重继承场景下也能正确析构。
 */
struct FactoryResult {
    void* service_ptr{ nullptr };  ///< 指向 TService 子对象（返回给调用者）
    void* impl_ptr{ nullptr };     ///< 指向 TImpl 原始对象（传给析构函数）
};

/// 工厂函数类型：接受 provider 引用和可选上下文指针，返回 FactoryResult
using FactoryFn = FactoryResult(*)(ServiceProvider& provider, void* context) noexcept;

/// 析构函数类型：接受 impl_ptr（TImpl* 以 void* 存储）
using DestroyFn = void(*)(void* impl_ptr) noexcept;

/**
 * @brief 服务描述符（注册时创建，不可变）。
 */
struct ServiceDescriptor {
    core::TypeId service_type{};  ///< 服务接口类型标识（TService 的 TypeId）
    Lifetime     lifetime{};      ///< 服务生命周期
    void*        context{};       ///< 附加上下文（如预注册实例指针）
    FactoryFn    factory{};       ///< 创建实例的工厂函数
    DestroyFn    destroy{};       ///< 销毁实例的析构函数（nullptr = 不拥有所有权）
};

/**
 * @brief 已创建实例的缓存条目（用于 singleton/scoped/transient 缓存）。
 */
struct InstanceEntry {
    core::TypeId service_type{};  ///< 服务类型标识
    void*        service_ptr{};   ///< 指向 TService 的指针（返回给调用者）
    void*        impl_ptr{};      ///< 指向 TImpl 的指针（用于正确析构）
    DestroyFn    destroy{};       ///< nullptr 表示不拥有（add_instance 场景）
};

/**
 * @brief 预注册实例的工厂函数（直接返回 context 中存储的实例指针）。
 */
inline FactoryResult instance_factory_fn(ServiceProvider& /*provider*/, void* ctx) noexcept {
    return {ctx, ctx};
}

/**
 * @brief 类型化析构函数模板（通过 MINE_DELETE 销毁 TImpl 实例）。
 *
 * @tparam TImpl 实现类类型
 */
template<class TImpl>
void typed_destroy_fn(void* impl_ptr) noexcept {
    MINE_DELETE(static_cast<TImpl*>(impl_ptr));
}

/**
 * @brief 解析依赖类型 Dep 的辅助函数。
 *
 * 支持 Dep 为 T 或 T*（均解析为 T*），使得：
 *   - add_singleton<ISvc, Impl, IDep>() 和
 *   - MINE_INJECT(Impl, IDep*)
 * 均可正确工作。
 *
 * @tparam Dep 依赖类型（T 或 T*）
 * @param provider 当前解析上下文
 * @return 解析到的服务指针（T*）
 */
template<class Dep>
auto resolve_dep(ServiceProvider& provider) noexcept;

/**
 * @brief 通用工厂函数模板（explicit Deps 版本）。
 *
 * 创建 TImpl 实例（通过解析所有 Deps...），并返回上转型到 TService* 的结果。
 *
 * @tparam TImpl   实现类
 * @tparam TService 服务接口类
 * @tparam Deps... 依赖类型列表（T 或 T*，均可）
 */
template<class TImpl, class TService, class... Deps>
FactoryResult typed_factory_fn(ServiceProvider& provider, void* /*ctx*/) noexcept;

} // namespace detail

// ── ServiceProvider 声明 ─────────────────────────────────────────────────────

/**
 * @brief 服务提供者（依赖注入容器）。
 *
 * 由 ServiceCollection::build() 生成，不可拷贝，仅可移动。
 * 负责按注册的 Lifetime 解析和管理服务实例的生命周期。
 *
 * 线程安全：非线程安全，单线程使用。
 */
class MINE_DI_API ServiceProvider {
public:
    ~ServiceProvider();

    ServiceProvider(const ServiceProvider&)            = delete;
    ServiceProvider& operator=(const ServiceProvider&) = delete;
    ServiceProvider(ServiceProvider&&) noexcept;
    ServiceProvider& operator=(ServiceProvider&&) noexcept;

    /**
     * @brief 解析服务（必须已注册，否则断言失败）。
     *
     * @tparam T 服务接口类型
     * @return T* 服务实例（由容器管理生命周期，调用者不拥有）
     */
    template<class T>
    T* resolve() {
        void* ptr = resolve_impl(core::TypeId::of<T>());
        MINE_CHECK_MSG(ptr != nullptr, "mine.di: 服务未注册");
        return static_cast<T*>(ptr);
    }

    /**
     * @brief 尝试解析服务（未注册时返回 nullptr，不断言）。
     *
     * @tparam T 服务接口类型
     * @return T* 服务实例，或 nullptr（若未注册）
     */
    template<class T>
    T* try_resolve() noexcept {
        return static_cast<T*>(try_resolve_impl(core::TypeId::of<T>()));
    }

    /**
     * @brief 创建子作用域（Scoped 服务在子作用域中隔离）。
     *
     * @return Scope 新作用域（Scope 析构时释放其内的所有 Scoped/Transient 服务）
     */
    Scope create_scope();

private:
    friend class ServiceCollection;
    friend class Scope;  // Scope 需要访问描述符和解析实现

    /// 由 ServiceCollection::build() 调用
    explicit ServiceProvider(mine::containers::Vector<detail::ServiceDescriptor> descriptors);

    /// 解析服务（内部实现，非模板）
    void* resolve_impl(core::TypeId type);
    /// 尝试解析服务（内部实现，非模板）
    void* try_resolve_impl(core::TypeId type) noexcept;

    struct Impl;
    core::Pimpl<Impl> p_;
};

// ── Scope 声明 ───────────────────────────────────────────────────────────────

/**
 * @brief 服务作用域。
 *
 * 由 ServiceProvider::create_scope() 创建。
 * Singleton 解析委托给根 ServiceProvider；
 * Scoped 服务在同一 Scope 内共享；
 * Transient 服务每次创建新实例并在 Scope 析构时释放。
 */
class MINE_DI_API Scope {
public:
    ~Scope();

    Scope(const Scope&)            = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) noexcept;
    Scope& operator=(Scope&&) noexcept;

    /**
     * @brief 解析服务（必须已注册，否则断言失败）。
     */
    template<class T>
    T* resolve() {
        void* ptr = resolve_impl(core::TypeId::of<T>());
        MINE_CHECK_MSG(ptr != nullptr, "mine.di: 服务未注册");
        return static_cast<T*>(ptr);
    }

    /**
     * @brief 尝试解析服务（未注册时返回 nullptr，不断言）。
     */
    template<class T>
    T* try_resolve() noexcept {
        return static_cast<T*>(try_resolve_impl(core::TypeId::of<T>()));
    }

private:
    friend class ServiceProvider;

    /// 由 ServiceProvider::create_scope() 调用
    explicit Scope(ServiceProvider* root);

    void* resolve_impl(core::TypeId type);
    void* try_resolve_impl(core::TypeId type) noexcept;

    struct Impl;
    core::Pimpl<Impl> p_;
};

// ── detail 模板实现（放在 ServiceProvider/Scope 完整声明之后）──────────────

namespace detail {

template<class Dep>
auto resolve_dep(ServiceProvider& provider) noexcept {
    using T = std::remove_pointer_t<Dep>;
    return provider.resolve<T>();
}

template<class TImpl, class TService, class... Deps>
FactoryResult typed_factory_fn(ServiceProvider& provider, void* /*ctx*/) noexcept {
    // 解析所有依赖并构造 TImpl
    TImpl* impl = MINE_NEW(TImpl, resolve_dep<Deps>(provider)...);
    if (impl == nullptr) return {};
    // 上转型到 TService*（正确处理多重继承指针偏移）
    return {static_cast<TService*>(impl), impl};
}

/**
 * @brief 检测类型 T 是否声明了 MINE_INJECT 宏。
 *
 * MINE_INJECT 在类内生成 static 工厂函数 __mine_di_factory_fn。
 */
template<class T>
concept HasMineInject = requires(ServiceProvider& p, void* ctx) {
    { T::__mine_di_factory_fn(p, ctx) } -> std::same_as<FactoryResult>;
};

} // namespace detail

} // namespace mine::di
