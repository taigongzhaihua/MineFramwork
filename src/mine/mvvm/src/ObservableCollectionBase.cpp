/**
 * @file ObservableCollectionBase.cpp
 * @brief ObservableCollectionBase 实现 —— 集合变更通知订阅者管理。
 */

#include <mine/mvvm/ObservableCollectionBase.h>

#include <mine/containers/Vector.h>
#include <mine/core/Pimpl.h>

namespace mine::mvvm {

// ── Pimpl 实现 ─────────────────────────────────────────────────────────────

/**
 * @brief ObservableCollectionBase 私有实现，管理集合变更订阅者列表。
 */
struct ObservableCollectionBase::Impl {
    /**
     * @brief 订阅者记录（函数指针 + 用户数据 + 令牌）。
     */
    struct Subscriber {
        INotifyCollectionChanged::ChangedFn fn;
        void*    user_data;
        uint32_t token;
    };

    /// 当前所有订阅者
    mine::containers::Vector<Subscriber> subscribers;

    /// 下一个分配的令牌值（从 1 开始，0 为无效令牌）
    uint32_t next_token{ 1 };
};

// ── 构造 / 析构 ───────────────────────────────────────────────────────────

ObservableCollectionBase::ObservableCollectionBase()
    : p_{ mine::core::make_pimpl<Impl>() }
{}

ObservableCollectionBase::~ObservableCollectionBase() = default;

// ── INotifyCollectionChanged 实现 ─────────────────────────────────────────

uint32_t ObservableCollectionBase::subscribe_collection_changed(
    ChangedFn fn, void* user_data) noexcept
{
    const uint32_t token = p_->next_token++;
    p_->subscribers.push_back(Impl::Subscriber{ fn, user_data, token });
    return token;
}

void ObservableCollectionBase::unsubscribe_collection_changed(uint32_t token) noexcept
{
    auto& subs = p_->subscribers;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].token == token) {
            // swap-with-back 移除，O(1)
            if (i + 1 < subs.size()) {
                subs[i] = std::move(subs[subs.size() - 1]);
            }
            subs.pop_back();
            return;
        }
    }
}

// ── notify_subscribers ────────────────────────────────────────────────────

void ObservableCollectionBase::notify_subscribers(
    const CollectionChangedArgs& args) noexcept
{
    const size_t count = p_->subscribers.size();
    for (size_t i = 0; i < count; ++i) {
        const auto& sub = p_->subscribers[i];
        sub.fn(this, args, sub.user_data);
    }
}

} // namespace mine::mvvm
