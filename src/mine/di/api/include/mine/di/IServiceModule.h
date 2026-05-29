/**
 * @file IServiceModule.h
 * @brief 服务模块接口，用于将相关服务批量注册封装为模块。
 *
 * 使用场景：
 * @code
 *   class DataModule : public IServiceModule {
 *   public:
 *       void configure(ServiceCollection& sc) override {
 *           sc.add_singleton<IDatabase, SqliteDatabase>()
 *             .add_scoped  <IUnitOfWork, SqliteUnitOfWork, IDatabase>();
 *       }
 *   };
 *
 *   auto sp = ServiceCollection{}
 *       .add_module<DataModule>()
 *       .add_module<UIModule>()
 *       .build();
 * @endcode
 */

#pragma once

#include <mine/di/Api.h>

namespace mine::di {

class ServiceCollection;

/**
 * @brief 服务模块接口。
 *
 * 实现此接口的类代表一组相关服务的注册逻辑，
 * 通过 ServiceCollection::add_module<Module>() 加载。
 */
class MINE_DI_API IServiceModule {
public:
    virtual ~IServiceModule() noexcept = default;

    /**
     * @brief 将模块管理的服务注册到 ServiceCollection。
     *
     * @param sc 目标服务集合
     */
    virtual void configure(ServiceCollection& sc) = 0;
};

} // namespace mine::di
