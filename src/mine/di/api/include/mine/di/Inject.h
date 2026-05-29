/**
 * @file Inject.h
 * @brief MINE_INJECT 宏定义，用于在类内声明可注入构造函数。
 *
 * 使用场景：
 * @code
 *   class LoginVM : public ViewModelBase {
 *       MINE_INJECT(LoginVM, IUnitOfWork*, ILogger*);
 *   public:
 *       LoginVM(IUnitOfWork* uow, ILogger* log);
 *   };
 *
 *   // 注册（自动识别依赖）：
 *   sc.add_singleton<LoginVM, LoginVM>();
 *
 *   // 或注册为接口实现：
 *   sc.add_singleton<IViewModel, LoginVM>();
 * @endcode
 *
 * 宏展开后在类内生成两个静态成员：
 *   - __mine_di_factory_fn：工厂函数，由 ServiceCollection 调用
 *   - __mine_di_destroy_fn：析构函数，由 ServiceProvider 调用
 */

#pragma once

#include <mine/core/Memory.h>
#include <mine/di/ServiceProvider.h>

/**
 * @def MINE_INJECT(Class, ...)
 * @brief 在类内声明可注入构造函数。
 *
 * @param Class   当前类名
 * @param ...     依赖类型列表（指针形式，如 IConfig*, ILogger*）
 *
 * 注意：
 *   1. 宏必须放在类的 private 或 public 区段内均可（生成 public static 函数）
 *   2. 依赖类型须为指针（T*），容器会自动去掉 * 进行 resolve
 *   3. 声明顺序须与构造函数参数顺序完全一致
 */
#define MINE_INJECT(Class, ...)                                                           \
    static ::mine::di::detail::FactoryResult __mine_di_factory_fn(                       \
            ::mine::di::ServiceProvider& __provider, void* /*__ctx*/) noexcept {         \
        Class* __impl = ::mine::di::detail::make_injected_instance<Class, ##__VA_ARGS__> \
            (__provider);                                                                 \
        if (__impl == nullptr) return {};                                                 \
        return {__impl, __impl};                                                          \
    }                                                                                     \
    static void __mine_di_destroy_fn(void* __ptr) noexcept {                             \
        MINE_DELETE(static_cast<Class*>(__ptr));                                          \
    }

namespace mine::di::detail {

/**
 * @brief MINE_INJECT 宏内部使用的实例创建辅助模板。
 *
 * 将 Deps... 中的每个类型（T 或 T*）从 provider 解析后传入 TImpl 构造函数。
 *
 * @tparam TImpl  要创建的实现类
 * @tparam Deps.. 依赖类型列表
 */
template<class TImpl, class... Deps>
TImpl* make_injected_instance(ServiceProvider& provider) noexcept {
    return MINE_NEW(TImpl, resolve_dep<Deps>(provider)...);
}

} // namespace mine::di::detail
