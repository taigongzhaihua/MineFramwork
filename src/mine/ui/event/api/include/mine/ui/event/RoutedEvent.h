/**
 * @file RoutedEvent.h
 * @brief 路由事件描述符及全局注册机制。
 *
 * RoutedEvent 是路由事件系统的核心描述符，与 DependencyProperty 设计类似：
 *   - 全局唯一：同一所有者类型 + 名称只能注册一次
 *   - 地址稳定：描述符由 RoutedEventRegistry 管理生命周期，返回常量引用
 *   - 作为 RoutedEventArgs 的标识符，在路由过程中标识事件类型
 *
 * 典型注册方式（在 .cpp 文件的静态初始化期注册）：
 * @code
 *   // 在类定义（.h）中声明
 *   class Button {
 *   public:
 *       static const mine::ui::RoutedEvent& ClickEvent;
 *   };
 *
 *   // 在 .cpp 中注册
 *   const mine::ui::RoutedEvent& Button::ClickEvent =
 *       mine::ui::register_event<Button>("Click", mine::ui::RoutingStrategy::Bubble);
 * @endcode
 */

#pragma once

#include <cstddef>
#include <mine/core/StringView.h>
#include <mine/core/TypeId.h>
#include <mine/ui/event/Api.h>
#include <mine/ui/event/RoutingStrategy.h>

namespace mine::ui {

/**
 * @brief 路由事件描述符。
 *
 * 每个路由事件对应一个全局唯一的 RoutedEvent 实例，由 register_event() 创建。
 * 用户通过返回的常量引用标识事件类型，不应自行构造此对象。
 *
 * 实例由 RoutedEventRegistry 统一管理，在进程生命周期内保持有效。
 */
class MINE_UI_EVENT_API RoutedEvent {
public:
    /// 事件名称（通常为 "Click"、"KeyDown" 等）
    [[nodiscard]] core::StringView  name()        const noexcept;
    /// 事件所属类型（注册者的类型 ID）
    [[nodiscard]] core::TypeId      owner_type()  const noexcept;
    /// 事件传播策略
    [[nodiscard]] RoutingStrategy   strategy()    const noexcept;

    // 禁止拷贝和赋值（描述符由全局注册表管理，不可复制）
    RoutedEvent(const RoutedEvent&)            = delete;
    RoutedEvent& operator=(const RoutedEvent&) = delete;

private:
    /// 仅由 register_event 内部构造
    RoutedEvent(const char*   name,
                core::TypeId  owner_type,
                RoutingStrategy strategy) noexcept;

    // 允许注册表和注册函数访问私有构造
    friend class RoutedEventRegistry;
    friend MINE_UI_EVENT_API const RoutedEvent& register_event(
        core::StringView name,
        core::TypeId     owner_type,
        RoutingStrategy  strategy);

    // ── 描述符数据 ─────────────────────────────────────────────────────────

    /// 事件名称（指向注册表内部字符串存储，进程期间有效）
    const char*     name_;
    /// 事件所属类型
    core::TypeId    owner_type_;
    /// 传播策略
    RoutingStrategy strategy_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 注册 API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 注册路由事件描述符（底层接口）。
 *
 * 相同 name + owner_type 组合只能注册一次。重复注册时断言失败（Debug 模式）。
 *
 * @param name       事件名称（建议为字符串字面量，确保地址稳定）
 * @param owner_type 所有者类型 ID
 * @param strategy   传播策略
 * @return 对新注册的 RoutedEvent 描述符的常量引用（地址永久有效）
 */
MINE_UI_EVENT_API
const RoutedEvent& register_event(core::StringView name,
                                   core::TypeId     owner_type,
                                   RoutingStrategy  strategy);

/**
 * @brief 注册路由事件描述符（类型安全便捷模板）。
 *
 * @tparam TOwner 事件所属类（注册者类型）
 * @param name     事件名称
 * @param strategy 传播策略
 * @return 对新注册的 RoutedEvent 描述符的常量引用
 */
template<typename TOwner>
const RoutedEvent& register_event(core::StringView name,
                                   RoutingStrategy  strategy) {
    return register_event(name, core::TypeId::of<TOwner>(), strategy);
}

} // namespace mine::ui
