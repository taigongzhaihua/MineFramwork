/**
 * @file ObservableObject.cpp
 * @brief ObservableObject 实现 —— 属性变更通知基类。
 */

#include <mine/mvvm/ObservableObject.h>

#include <mine/containers/Vector.h>
#include <mine/core/Function.h>
#include <mine/core/Pimpl.h>
#include <mine/core/Variant.h>

namespace mine::mvvm {

// ── Pimpl 实现 ─────────────────────────────────────────────────────────────

/**
 * @brief ObservableObject 私有实现，管理属性变更订阅者列表。
 */
struct ObservableObject::Impl {
    /**
     * @brief 订阅者记录（函数指针 + 用户数据 + 令牌）。
     */
    struct Subscriber {
        INotifyPropertyChanged::ChangedFn fn;
        void*    user_data;
        uint32_t token;
    };

    /// 当前所有订阅者
    mine::containers::Vector<Subscriber> subscribers;

    /// 下一个分配的令牌值（从 1 开始，0 为无效令牌）
    uint32_t next_token{ 1 };

    // ── 属性反射表 ────────────────────────────────────────────────────────

    /**
     * @brief 属性 getter 条目：属性名 + 返回当前值的无参函数。
     *
     * 由 MINE_OBSERVABLE 宏在成员初始化时自动注入，无需手动维护。
     * 属性名使用 #Name 字符串字面量（静态生命周期），StringView 引用安全。
     */
    struct PropertyEntry {
        mine::core::StringView                       name;
        mine::core::Function<mine::core::Variant()>  getter;
    };

    /// 属性名 → getter 的线性查找表（属性数量通常 < 20，线性查找优于哈希）
    mine::containers::Vector<PropertyEntry> property_getters;
};

// ── 构造 / 析构 ───────────────────────────────────────────────────────────

ObservableObject::ObservableObject()
    : p_{ mine::core::make_pimpl<Impl>() }
{}

ObservableObject::~ObservableObject() = default;

// ── INotifyPropertyChanged 实现 ───────────────────────────────────────────

uint32_t ObservableObject::subscribe_property_changed(
    ChangedFn fn, void* user_data) noexcept
{
    const uint32_t token = p_->next_token++;
    p_->subscribers.push_back(Impl::Subscriber{ fn, user_data, token });
    return token;
}

void ObservableObject::unsubscribe_property_changed(uint32_t token) noexcept
{
    auto& subs = p_->subscribers;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].token == token) {
            // swap-with-back 移除，O(1)，不保证顺序（通知顺序无要求）
            if (i + 1 < subs.size()) {
                subs[i] = std::move(subs[subs.size() - 1]);
            }
            subs.pop_back();
            return;
        }
    }
}

// ── raise ─────────────────────────────────────────────────────────────────

void ObservableObject::raise(mine::core::StringView name) noexcept
{
    // 广播给所有订阅者
    // 注意：回调可能修改订阅者列表（可重入）；
    // 此处采用快照大小迭代（不处理复杂的可重入情形，与框架约定单线程调用）
    const size_t count = p_->subscribers.size();
    for (size_t i = 0; i < count; ++i) {
        const auto& sub = p_->subscribers[i];
        sub.fn(this, name, sub.user_data);
    }
}

// ── 属性反射接口实现 ──────────────────────────────────────────────────────

void ObservableObject::register_property_getter(
    mine::core::StringView                       name,
    mine::core::Function<mine::core::Variant()>  getter) noexcept
{
    // 如果同名条目已存在则更新（理论上 MINE_OBSERVABLE 宏不会重复注册）
    for (auto& entry : p_->property_getters) {
        if (entry.name == name) {
            entry.getter = std::move(getter);
            return;
        }
    }
    p_->property_getters.push_back(Impl::PropertyEntry{ name, std::move(getter) });
}

mine::core::Variant ObservableObject::get_property(mine::core::StringView name) const noexcept
{
    for (const auto& entry : p_->property_getters) {
        if (entry.name == name) {
            return entry.getter();
        }
    }
    return mine::core::Variant{};
}

} // namespace mine::mvvm
