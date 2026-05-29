/**
 * @file Lifetime.h
 * @brief 依赖注入服务生命周期枚举定义。
 */

#pragma once

#include <cstdint>

namespace mine::di {

/**
 * @brief 服务实例的生命周期。
 *
 * - Singleton：整个 ServiceProvider 生命周期内只创建一个实例，
 *              适合全局单例（如日志、配置、数据库连接池）。
 * - Scoped：   每个 Scope（窗口/页面级别）内只创建一个实例，
 *              不同 Scope 之间相互隔离，适合请求/页面级别状态。
 * - Transient：每次 resolve 都创建新实例，实例由容器跟踪，
 *              在 ServiceProvider 或 Scope 析构时统一释放。
 */
enum class Lifetime : uint8_t {
    Singleton,  ///< 进程级唯一实例
    Scoped,     ///< 作用域内唯一实例
    Transient,  ///< 每次创建新实例
};

} // namespace mine::di
