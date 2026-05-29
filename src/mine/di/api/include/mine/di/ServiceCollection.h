/**
 * @file ServiceCollection.h
 * @brief 服务集合（依赖注入注册器）声明。
 *
 * 用法示例：
 * @code
 *   auto sp = ServiceCollection{}
 *       .add_singleton<ILogger, ConsoleLogger>()
 *       .add_scoped   <IUnitOfWork, SqliteUnitOfWork, IConfig>()
 *       .add_transient<LoginVM, LoginVM, IUnitOfWork, ILogger>()
 *       .add_instance <IConfig>(config_ptr)
 *       .build();
 *
 *   auto* vm = sp.resolve<LoginVM>();
 * @endcode
 */

#pragma once

#include <mine/core/Memory.h>
#include <mine/core/Pimpl.h>
#include <mine/core/TypeId.h>
#include <mine/containers/Vector.h>

#include <mine/di/Api.h>
#include <mine/di/Lifetime.h>
#include <mine/di/ServiceProvider.h>

namespace mine::di {

/**
 * @brief 服务注册容器，用于配置服务及其生命周期。
 *
 * build() 消耗当前 ServiceCollection（右值限定），
 * 返回不可变的 ServiceProvider。
 */
class MINE_DI_API ServiceCollection {
public:
    ServiceCollection();
    ~ServiceCollection();

    ServiceCollection(const ServiceCollection&)            = delete;
    ServiceCollection& operator=(const ServiceCollection&) = delete;
    ServiceCollection(ServiceCollection&&) noexcept;
    ServiceCollection& operator=(ServiceCollection&&) noexcept;

    // ── Singleton 注册 ────────────────────────────────────────────────────────

    /**
     * @brief 注册 Singleton 服务。
     *
     * 有两种用法：
     *   1. 显式依赖列表：`add_singleton<ISvc, Impl, IDep1, IDep2>()`
     *   2. MINE_INJECT 自动推导（Deps 为空且 TImpl 声明了 MINE_INJECT 宏）：
     *      `add_singleton<ISvc, Impl>()`
     *   3. 无依赖默认构造：`add_singleton<ISvc, Impl>()`（TImpl 无 MINE_INJECT 时）
     *
     * @tparam TService 服务接口类型
     * @tparam TImpl    服务实现类型
     * @tparam Deps...  依赖类型列表（T 或 T*，自动解析）
     */
    template<class TService, class TImpl, class... Deps>
    ServiceCollection& add_singleton() {
        if constexpr (sizeof...(Deps) == 0 && detail::HasMineInject<TImpl>) {
            // MINE_INJECT 版本：使用类内声明的静态工厂
            register_descriptor({
                core::TypeId::of<TService>(),
                Lifetime::Singleton,
                nullptr,
                [](ServiceProvider& p, void* ctx) noexcept -> detail::FactoryResult {
                    detail::FactoryResult r = TImpl::__mine_di_factory_fn(p, ctx);
                    if (r.impl_ptr == nullptr) return {};
                    return {
                        static_cast<void*>(
                            static_cast<TService*>(static_cast<TImpl*>(r.impl_ptr))),
                        r.impl_ptr
                    };
                },
                &TImpl::__mine_di_destroy_fn,
            });
        } else {
            // 显式 Deps 版本（Deps 为空时使用默认构造）
            register_descriptor({
                core::TypeId::of<TService>(),
                Lifetime::Singleton,
                nullptr,
                &detail::typed_factory_fn<TImpl, TService, Deps...>,
                &detail::typed_destroy_fn<TImpl>,
            });
        }
        return *this;
    }

    // ── Scoped 注册 ───────────────────────────────────────────────────────────

    /**
     * @brief 注册 Scoped 服务。
     *
     * @tparam TService 服务接口类型
     * @tparam TImpl    服务实现类型
     * @tparam Deps...  依赖类型列表（T 或 T*，自动解析）
     */
    template<class TService, class TImpl, class... Deps>
    ServiceCollection& add_scoped() {
        if constexpr (sizeof...(Deps) == 0 && detail::HasMineInject<TImpl>) {
            register_descriptor({
                core::TypeId::of<TService>(),
                Lifetime::Scoped,
                nullptr,
                [](ServiceProvider& p, void* ctx) noexcept -> detail::FactoryResult {
                    detail::FactoryResult r = TImpl::__mine_di_factory_fn(p, ctx);
                    if (r.impl_ptr == nullptr) return {};
                    return {
                        static_cast<void*>(
                            static_cast<TService*>(static_cast<TImpl*>(r.impl_ptr))),
                        r.impl_ptr
                    };
                },
                &TImpl::__mine_di_destroy_fn,
            });
        } else {
            register_descriptor({
                core::TypeId::of<TService>(),
                Lifetime::Scoped,
                nullptr,
                &detail::typed_factory_fn<TImpl, TService, Deps...>,
                &detail::typed_destroy_fn<TImpl>,
            });
        }
        return *this;
    }

    // ── Transient 注册 ────────────────────────────────────────────────────────

    /**
     * @brief 注册 Transient 服务。
     *
     * @tparam TService 服务接口类型
     * @tparam TImpl    服务实现类型
     * @tparam Deps...  依赖类型列表（T 或 T*，自动解析）
     */
    template<class TService, class TImpl, class... Deps>
    ServiceCollection& add_transient() {
        if constexpr (sizeof...(Deps) == 0 && detail::HasMineInject<TImpl>) {
            register_descriptor({
                core::TypeId::of<TService>(),
                Lifetime::Transient,
                nullptr,
                [](ServiceProvider& p, void* ctx) noexcept -> detail::FactoryResult {
                    detail::FactoryResult r = TImpl::__mine_di_factory_fn(p, ctx);
                    if (r.impl_ptr == nullptr) return {};
                    return {
                        static_cast<void*>(
                            static_cast<TService*>(static_cast<TImpl*>(r.impl_ptr))),
                        r.impl_ptr
                    };
                },
                &TImpl::__mine_di_destroy_fn,
            });
        } else {
            register_descriptor({
                core::TypeId::of<TService>(),
                Lifetime::Transient,
                nullptr,
                &detail::typed_factory_fn<TImpl, TService, Deps...>,
                &detail::typed_destroy_fn<TImpl>,
            });
        }
        return *this;
    }

    // ── 预注册实例 ────────────────────────────────────────────────────────────

    /**
     * @brief 注册已有实例（不转移所有权，容器不负责销毁）。
     *
     * 实例的生命周期由调用方管理，必须在 ServiceProvider 销毁之前保持有效。
     *
     * @tparam TService 服务接口类型
     * @param  instance 已构造的服务实例指针（非 nullptr）
     */
    template<class TService>
    ServiceCollection& add_instance(TService* instance) {
        MINE_CHECK_MSG(instance != nullptr, "mine.di: add_instance 实例不能为 nullptr");
        register_descriptor({
            core::TypeId::of<TService>(),
            Lifetime::Singleton,
            static_cast<void*>(instance),
            &detail::instance_factory_fn,
            nullptr,  // 不拥有所有权，不负责销毁
        });
        return *this;
    }

    // ── 用户自定义工厂 ────────────────────────────────────────────────────────

    /**
     * @brief 注册用户自定义工厂函数（Singleton 生命周期）。
     *
     * 工厂函数由调用方提供，返回新创建的 TService* 实例；
     * 容器将在销毁时通过 MINE_DELETE 释放。
     *
     * @tparam TService 服务接口类型
     * @param  factory  用户工厂函数（noexcept，由调用方保证返回非 nullptr）
     */
    template<class TService>
    ServiceCollection& add_factory(TService*(*factory)(ServiceProvider&) noexcept,
                                   Lifetime lifetime = Lifetime::Singleton) {
        // 用 context 存储用户工厂函数指针（函数指针 <-> void* 转换符合标准）
        using UserFn = TService*(*)(ServiceProvider&) noexcept;
        register_descriptor({
            core::TypeId::of<TService>(),
            lifetime,
            reinterpret_cast<void*>(factory),
            [](ServiceProvider& p, void* ctx) noexcept -> detail::FactoryResult {
                auto fn = reinterpret_cast<UserFn>(ctx);
                TService* svc = fn(p);
                return {svc, svc};
            },
            [](void* ptr) noexcept {
                MINE_DELETE(static_cast<TService*>(ptr));
            },
        });
        return *this;
    }

    // ── IServiceModule 支持 ───────────────────────────────────────────────────

    /**
     * @brief 注册服务模块（批量注册）。
     *
     * @tparam Module 实现了 IServiceModule 接口的模块类型
     */
    template<class Module>
    ServiceCollection& add_module() {
        Module{}.configure(*this);
        return *this;
    }

    // ── 构建 ─────────────────────────────────────────────────────────────────

    /**
     * @brief 消耗当前 ServiceCollection 并构建 ServiceProvider。
     *
     * 调用后当前对象处于无效（已移动）状态，不得继续使用。
     *
     * @return ServiceProvider 构建完成的服务提供者
     */
    ServiceProvider build();

private:
    /// 注册服务描述符（追加到内部列表末尾）
    void register_descriptor(detail::ServiceDescriptor desc);

    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::di
