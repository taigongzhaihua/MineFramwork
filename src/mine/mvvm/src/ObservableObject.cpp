/**
 * @file ObservableObject.cpp
 * @brief ObservableObject 实现 —— 属性变更通知基类。
 */

#include <mine/mvvm/ObservableObject.h>

#include <mine/containers/Vector.h>
#include <mine/core/Pimpl.h>

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

} // namespace mine::mvvm
