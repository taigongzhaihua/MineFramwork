/**
 * @file RoutedEvent.cpp
 * @brief RoutedEvent 全局注册表与描述符实现。
 *
 * RoutedEventRegistry 是 Meyer's 单例，设计与 DependencyPropertyRegistry 对称：
 *   - register_event() 在静态初始化阶段（单线程）调用，无须加锁
 *   - 注册表拥有所有 RoutedEvent 描述符的生命周期（分配于默认分配器）
 *   - 注册后的描述符地址在进程期间永远有效（地址稳定）
 *   - (name, owner_type) 重复注册触发断言
 *
 * 字符串存储：
 *   name 以 StringView 形式接收，但内部存储为 const char*，
 *   指向注册表内部分配的字符串副本，确保地址稳定且不依赖调用方字符串的生命周期。
 */

#include <cstring>  // strlen / memcpy

#include <mine/ui/event/RoutedEvent.h>

#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>
#include <mine/containers/Vector.h>

namespace mine::ui {

// ────────────────────────────────────────────────────────────────────────────
// 内部：全局注册表
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief 路由事件全局注册表（Meyer's 单例）。
 *
 * RoutedEvent 实例由注册表统一分配和管理，程序退出时析构。
 * 事件描述符以指针形式存储在 props_ 向量中，外部持有其常量引用。
 */
class RoutedEventRegistry {
public:
    /// Meyer's 单例：首次调用时构造，程序退出时析构
    static RoutedEventRegistry& instance() noexcept {
        static RoutedEventRegistry s_instance;
        return s_instance;
    }

    /**
     * @brief 注册新的路由事件描述符。
     *
     * @param name       事件名称（将在注册表内复制一份）
     * @param owner_type 所有者类型 ID
     * @param strategy   传播策略
     * @return 对新注册描述符的常量引用（地址在进程期间稳定）
     */
    const RoutedEvent& add(core::StringView name,
                            core::TypeId    owner_type,
                            RoutingStrategy strategy) {
        // 检查重复注册
        for (RoutedEvent* e : events_) {
            if (e->owner_type() == owner_type && e->name() == name) {
                MINE_ASSERT_MSG(false,
                    "RoutedEvent 重复注册：同一所有者类型下已存在同名路由事件");
                return *e;
            }
        }

        auto* alloc = core::default_allocator();

        // 在注册表内分配并复制名称字符串，确保地址稳定
        const size_t name_len    = name.size();
        char*        name_copy   = static_cast<char*>(
            alloc->alloc(name_len + 1u, alignof(char)));
        MINE_CHECK(name_copy != nullptr);
        // 手动复制字符串内容（StringView 不一定以 '\0' 结尾）
        if (name_len > 0u) {
            std::memcpy(name_copy, name.data(), name_len);
        }
        name_copy[name_len] = '\0';

        // 分配 RoutedEvent 描述符对象
        void* mem = alloc->alloc(sizeof(RoutedEvent), alignof(RoutedEvent));
        MINE_CHECK(mem != nullptr);

        // placement new 构造（传入内部拷贝的字符串指针）
        RoutedEvent* event = new (mem) RoutedEvent(name_copy, owner_type, strategy);
        events_.push_back(event);
        return *event;
    }

    ~RoutedEventRegistry() {
        auto* alloc = core::default_allocator();
        // 析构并释放所有已注册的事件描述符
        for (RoutedEvent* e : events_) {
            // 先记录名称信息，再析构（析构后 name() 可能无效）
            const size_t name_len = e->name().size();
            const char*  name_ptr = e->name().data();
            e->~RoutedEvent();
            alloc->dealloc(e, sizeof(RoutedEvent), alignof(RoutedEvent));
            alloc->dealloc(const_cast<char*>(name_ptr), name_len + 1u, alignof(char));
        }
    }

private:
    RoutedEventRegistry() = default;

    /// 所有已注册路由事件描述符（注册表拥有生命周期）
    containers::Vector<RoutedEvent*> events_;
};

// ────────────────────────────────────────────────────────────────────────────
// RoutedEvent 实现
// ────────────────────────────────────────────────────────────────────────────

RoutedEvent::RoutedEvent(const char*     name,
                          core::TypeId    owner_type,
                          RoutingStrategy strategy) noexcept
    : name_       {name}
    , owner_type_ {owner_type}
    , strategy_   {strategy}
{}

core::StringView RoutedEvent::name() const noexcept {
    return core::StringView{name_};
}

core::TypeId RoutedEvent::owner_type() const noexcept {
    return owner_type_;
}

RoutingStrategy RoutedEvent::strategy() const noexcept {
    return strategy_;
}

// ────────────────────────────────────────────────────────────────────────────
// 注册 API 实现
// ────────────────────────────────────────────────────────────────────────────

const RoutedEvent& register_event(core::StringView name,
                                   core::TypeId     owner_type,
                                   RoutingStrategy  strategy) {
    return RoutedEventRegistry::instance().add(name, owner_type, strategy);
}

} // namespace mine::ui
