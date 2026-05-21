/**
 * @file ResourceDictionary.cpp
 * @brief ResourceDictionary 树形资源字典实现。
 *
 * 设计要点：
 *   - 本层资源存储：Vector<Entry>（线性表，典型 10-50 条目，线性查找足够）
 *   - 合并字典：SmallVector<const ResourceDictionary*, 4>（弱引用，不拥有）
 *   - 父字典：ResourceDictionary*（弱引用，不拥有，析构时自动取消订阅）
 *   - 特定 key 订阅：Vector<KeySub>（Function 为 move-only，用 Vector 支持移动语义）
 *   - resource_changed 广播订阅：SmallVector<ChangedSub, 4>
 *   - ID 分配：单调递增 uint32_t，0 为无效值
 *
 * 移除策略（swap-and-pop）：
 *   找到目标元素后，将最后一个元素移动到该位置，然后 pop_back()。
 *   顺序不保证，但 O(1) 移除不影响正确性。
 */

#include <mine/ui/style/ResourceDictionary.h>

#include <mine/containers/InlineString.h>
#include <mine/containers/SmallVector.h>
#include <mine/containers/Vector.h>
#include <mine/core/Assert.h>

namespace mine::ui::style {

// ────────────────────────────────────────────────────────────────────────────
// Impl：PIMPL 实现结构体
// ────────────────────────────────────────────────────────────────────────────

struct ResourceDictionary::Impl {
    // ── 本层资源条目 ─────────────────────────────────────────────────────

    struct Entry {
        containers::InlineString key;
        core::Variant            value;
    };
    /// 本层资源线性表（InlineString key + Variant value）
    containers::Vector<Entry> local_;

    // ── 合并字典 ─────────────────────────────────────────────────────────

    /// 合并进来的字典弱引用列表（按 merge() 调用顺序排列）
    containers::SmallVector<const ResourceDictionary*, 4> merged_;

    // ── 父字典 ────────────────────────────────────────────────────────────

    /// 父字典弱引用（不拥有生命周期）
    ResourceDictionary* parent_     = nullptr;
    /// 在父字典 resource_changed 上的订阅 ID（用于析构/重设时取消）
    HandlerId           parent_sid_ = kInvalidHandlerId;

    // ── 特定 key 订阅 ─────────────────────────────────────────────────────

    struct KeySub {
        HandlerId                                  id;
        containers::InlineString                   key;
        core::Function<void(const core::Variant&)> cb;
    };
    /// DynamicResource 特定 key 订阅列表（Function 为 move-only，用 Vector）
    containers::Vector<KeySub> key_subs_;

    // ── resource_changed 广播订阅 ─────────────────────────────────────────

    struct ChangedSub {
        HandlerId                              id;
        core::Function<void(core::StringView)> cb;
    };
    /// resource_changed 广播订阅列表（典型 0-4 个子字典订阅）
    containers::SmallVector<ChangedSub, 4> changed_subs_;

    // ── ID 分配 ───────────────────────────────────────────────────────────

    uint32_t next_id_ = 1;  ///< 下一个可用 ID（从 1 开始，0 = kInvalidHandlerId）

    HandlerId alloc_id() noexcept { return static_cast<HandlerId>(next_id_++); }

    // ── 内部辅助：查找本层条目 ────────────────────────────────────────────

    [[nodiscard]] const Entry* find_entry(core::StringView key) const noexcept {
        for (const auto& e : local_) {
            if (e.key == key) return &e;
        }
        return nullptr;
    }

    [[nodiscard]] Entry* find_entry_mut(core::StringView key) noexcept {
        for (auto& e : local_) {
            if (e.key == key) return &e;
        }
        return nullptr;
    }

    // ── 内部辅助：触发特定 key 的订阅回调 ────────────────────────────────

    void fire_key_subs(core::StringView key, const core::Variant& value) noexcept {
        for (auto& sub : key_subs_) {
            if (sub.key == key) {
                sub.cb(value);
            }
        }
    }

    // ── 内部辅助：触发 resource_changed 广播 ─────────────────────────────

    void fire_changed(core::StringView key) noexcept {
        for (auto& sub : changed_subs_) {
            sub.cb(key);
        }
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 构造 / 析构 / 移动
// ────────────────────────────────────────────────────────────────────────────

ResourceDictionary::ResourceDictionary()
    : p_{core::make_pimpl<Impl>()}
{}

ResourceDictionary::~ResourceDictionary() {
    // 析构时断开父字典连接，避免父字典持有悬空的回调引用
    if (p_ && p_->parent_ && p_->parent_sid_ != kInvalidHandlerId) {
        p_->parent_->off_resource_changed(p_->parent_sid_);
        p_->parent_sid_ = kInvalidHandlerId;
        p_->parent_      = nullptr;
    }
}

ResourceDictionary::ResourceDictionary(ResourceDictionary&&) noexcept  = default;
ResourceDictionary& ResourceDictionary::operator=(ResourceDictionary&&) noexcept = default;

// ────────────────────────────────────────────────────────────────────────────
// 写入
// ────────────────────────────────────────────────────────────────────────────

void ResourceDictionary::set(core::StringView key, core::Variant value) {
    // 静态写入：仅更新存储，不触发任何通知
    auto* entry = p_->find_entry_mut(key);
    if (entry) {
        entry->value = std::move(value);
    } else {
        p_->local_.push_back(
            Impl::Entry{containers::InlineString{key.data(), key.size()}, std::move(value)});
    }
}

void ResourceDictionary::set_dynamic(core::StringView key, core::Variant value) {
    // 动态写入：更新存储后触发订阅者和广播
    auto* entry = p_->find_entry_mut(key);
    if (entry) {
        entry->value = value;  // 保留副本用于回调（不移动）
    } else {
        p_->local_.push_back(
            Impl::Entry{containers::InlineString{key.data(), key.size()}, value});
        entry = &p_->local_.back();
    }
    // 触发特定 key 的 subscribe() 回调
    p_->fire_key_subs(key, entry->value);
    // 广播 resource_changed
    p_->fire_changed(key);
}

// ────────────────────────────────────────────────────────────────────────────
// 合并
// ────────────────────────────────────────────────────────────────────────────

void ResourceDictionary::merge(const ResourceDictionary& other) {
    p_->merged_.push_back(&other);
}

void ResourceDictionary::clear_merged() noexcept {
    p_->merged_.clear();
}

// ────────────────────────────────────────────────────────────────────────────
// 查找
// ────────────────────────────────────────────────────────────────────────────

core::Variant ResourceDictionary::find(core::StringView key) const noexcept {
    // 1. 查本层
    const auto* e = p_->find_entry(key);
    if (e) return e->value;

    // 2. 查合并层（按 merge() 调用顺序，仅查各自本层，不递归其父）
    for (const auto* merged : p_->merged_) {
        auto v = merged->find_local(key);
        if (v.has_value()) return v;
    }

    // 3. 递归查父层
    if (p_->parent_) {
        return p_->parent_->find(key);
    }

    return {};  // 空 Variant，调用方通过 has_value() 判断
}

core::Variant ResourceDictionary::find_local(core::StringView key) const noexcept {
    const auto* e = p_->find_entry(key);
    return e ? e->value : core::Variant{};
}

// ────────────────────────────────────────────────────────────────────────────
// DynamicResource 订阅
// ────────────────────────────────────────────────────────────────────────────

HandlerId ResourceDictionary::subscribe(core::StringView key,
                                         core::Function<void(const core::Variant&)> callback) {
    const HandlerId id = p_->alloc_id();
    p_->key_subs_.push_back(Impl::KeySub{
        id,
        containers::InlineString{key.data(), key.size()},
        std::move(callback)});
    return id;
}

void ResourceDictionary::unsubscribe(HandlerId id) noexcept {
    auto& subs = p_->key_subs_;
    for (decltype(subs.size()) i = 0; i < subs.size(); ++i) {
        if (subs[i].id == id) {
            // swap-and-pop：将末尾元素移动到当前位置（保持容量，O(1) 移除）
            if (i + 1 < subs.size()) {
                subs[i] = std::move(subs[subs.size() - 1]);
            }
            subs.pop_back();
            return;
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
// 父字典连接
// ────────────────────────────────────────────────────────────────────────────

void ResourceDictionary::set_parent(ResourceDictionary* parent) noexcept {
    // 断开旧父字典的 resource_changed 订阅
    if (p_->parent_ && p_->parent_sid_ != kInvalidHandlerId) {
        p_->parent_->off_resource_changed(p_->parent_sid_);
        p_->parent_sid_ = kInvalidHandlerId;
    }

    p_->parent_ = parent;

    if (parent) {
        // 订阅父字典的 resource_changed，以便将父层变更向下传播给本层订阅者
        p_->parent_sid_ = parent->on_resource_changed(
            [this](core::StringView key) noexcept {
                // 若本层未覆盖该 key，则触发本层对该 key 的 subscribe() 订阅者
                if (!p_->find_entry(key)) {
                    core::Variant value = find(key);
                    p_->fire_key_subs(key, value);
                }
                // 将 resource_changed 信号传播给本层的 on_resource_changed 订阅者
                p_->fire_changed(key);
            });
    }
}

ResourceDictionary* ResourceDictionary::parent() const noexcept {
    return p_->parent_;
}

// ────────────────────────────────────────────────────────────────────────────
// resource_changed 广播接口
// ────────────────────────────────────────────────────────────────────────────

HandlerId ResourceDictionary::on_resource_changed(
    core::Function<void(core::StringView)> callback) {
    const HandlerId id = p_->alloc_id();
    p_->changed_subs_.push_back(Impl::ChangedSub{id, std::move(callback)});
    return id;
}

void ResourceDictionary::off_resource_changed(HandlerId id) noexcept {
    auto& subs = p_->changed_subs_;
    for (decltype(subs.size()) i = 0; i < subs.size(); ++i) {
        if (subs[i].id == id) {
            // swap-and-pop：移除的顺序不影响广播正确性
            if (i + 1 < subs.size()) {
                subs[i] = std::move(subs[subs.size() - 1]);
            }
            subs.pop_back();
            return;
        }
    }
}

void ResourceDictionary::notify_resource_changed(core::StringView key) {
    p_->fire_changed(key);
}

}  // namespace mine::ui::style
