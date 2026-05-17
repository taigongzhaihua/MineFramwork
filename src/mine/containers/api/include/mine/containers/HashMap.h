/**
 * @file HashMap.h
 * @brief 基于开放寻址法（Robin Hood 探测）的哈希映射表。
 *
 * 特性：
 *   - Robin Hood 线性探测：减少平均探测距离，插入/查找性能优于简单线性探测
 *   - 负载因子阈值 0.75：超过时自动 2 倍扩容
 *   - 使用 mine::core::IAllocator 进行内存管理（可替换）
 *   - 不支持 C++ 异常，分配失败通过 MINE_CHECK 断言终止
 *   - Key/Value 类型必须可移动构造（MoveConstructible）
 *
 * 探测策略说明（Robin Hood）：
 *   每个 slot 记录其实际存储键的"理想位置"与当前位置的距离（PSL，Probe Sequence Length）。
 *   插入时若当前 slot 中的元素 PSL 小于待插入元素的 PSL，则"抢占"该 slot 并将原元素继续向后探测，
 *   从而均衡各元素的探测距离。
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>
#include <new>
#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>
#include <mine/containers/Hash.h>

namespace mine::containers {

namespace detail {

/// 哈希表 slot 状态
enum class SlotState : uint8_t {
    Empty   = 0,  ///< 空闲
    Occupied = 1, ///< 已占用
};

/// 哈希表内部 slot 结构（按缓存行对齐）
template<typename K, typename V>
struct HashSlot {
    alignas(alignof(K) > alignof(V) ? alignof(K) : alignof(V))
    unsigned char key_buf[sizeof(K)];     ///< Key 的原始存储（未初始化时为垃圾值）
    unsigned char val_buf[sizeof(V)];     ///< Value 的原始存储
    int32_t       psl   = -1;            ///< Probe Sequence Length，-1 表示 Empty
    // psl >= 0 表示该 slot 已被占用

    [[nodiscard]] K& key() noexcept {
        return *reinterpret_cast<K*>(key_buf);
    }
    [[nodiscard]] const K& key() const noexcept {
        return *reinterpret_cast<const K*>(key_buf);
    }
    [[nodiscard]] V& value() noexcept {
        return *reinterpret_cast<V*>(val_buf);
    }
    [[nodiscard]] const V& value() const noexcept {
        return *reinterpret_cast<const V*>(val_buf);
    }
    [[nodiscard]] bool empty() const noexcept { return psl < 0; }
};

} // namespace detail

/**
 * @brief Robin Hood 开放寻址哈希映射表。
 *
 * @tparam K     键类型（需有 Hash<K> 与 Equal<K> 特化）
 * @tparam V     值类型
 * @tparam THash 哈希函数类型，默认为 Hash<K>
 * @tparam TEqual 键相等比较类型，默认为 Equal<K>
 */
template<typename K, typename V,
         typename THash  = Hash<K>,
         typename TEqual = Equal<K>>
class HashMap {
public:
    using key_type       = K;
    using mapped_type    = V;
    using value_type     = void;   // 不提供 pair 视角，使用 key()/value() 分别访问
    using size_type      = size_t;
    using hasher         = THash;
    using key_equal      = TEqual;

private:
    using Slot = detail::HashSlot<K, V>;

public:
    // ── 迭代器 ────────────────────────────────────────────────────────────

    class iterator {
    public:
        iterator(Slot* ptr, Slot* end) noexcept : ptr_{ptr}, end_{end} {
            skip_empty();
        }

        [[nodiscard]] K& key()   noexcept { return ptr_->key(); }
        [[nodiscard]] V& value() noexcept { return ptr_->value(); }

        iterator& operator++() noexcept {
            ++ptr_;
            skip_empty();
            return *this;
        }
        [[nodiscard]] bool operator==(const iterator& o) const noexcept {
            return ptr_ == o.ptr_;
        }
        [[nodiscard]] bool operator!=(const iterator& o) const noexcept {
            return ptr_ != o.ptr_;
        }

    private:
        Slot* ptr_;
        Slot* end_;
        void skip_empty() noexcept {
            while (ptr_ != end_ && ptr_->empty()) ++ptr_;
        }
    };

    class const_iterator {
    public:
        const_iterator(const Slot* ptr, const Slot* end) noexcept
            : ptr_{ptr}, end_{end}
        {
            skip_empty();
        }
        /*implicit*/ const_iterator(const iterator& it) noexcept  // NOLINT(google-explicit-constructor)
            : ptr_{it.ptr_}, end_{it.end_}
        {}

        [[nodiscard]] const K& key()   const noexcept { return ptr_->key(); }
        [[nodiscard]] const V& value() const noexcept { return ptr_->value(); }

        const_iterator& operator++() noexcept {
            ++ptr_;
            skip_empty();
            return *this;
        }
        [[nodiscard]] bool operator==(const const_iterator& o) const noexcept {
            return ptr_ == o.ptr_;
        }
        [[nodiscard]] bool operator!=(const const_iterator& o) const noexcept {
            return ptr_ != o.ptr_;
        }

    private:
        const Slot* ptr_;
        const Slot* end_;
        void skip_empty() noexcept {
            while (ptr_ != end_ && ptr_->empty()) ++ptr_;
        }
    };

    // ── 构造 / 析构 ───────────────────────────────────────────────────────

    explicit HashMap(size_type init_cap = 0,
                     mine::core::IAllocator* alloc
                     = mine::core::default_allocator())
        : alloc_{alloc}
    {
        if (init_cap > 0) {
            // 将初始容量向上取到合适的桶数
            const size_type buckets = next_power_of_two(
                static_cast<size_type>(
                    static_cast<double>(init_cap) / kMaxLoadFactor + 1.0));
            alloc_slots(buckets);
        }
    }

    HashMap(const HashMap& other)
        : alloc_{other.alloc_}
        , hash_{other.hash_}
        , equal_{other.equal_}
    {
        if (other.bucket_count_ > 0) {
            alloc_slots(other.bucket_count_);
            // 将所有已占用 slot 逐个再插入
            for (size_type i = 0; i < other.bucket_count_; ++i) {
                if (!other.slots_[i].empty()) {
                    insert_slot(other.slots_[i].key(),
                                other.slots_[i].value());
                }
            }
        }
    }

    HashMap(HashMap&& other) noexcept
        : alloc_{other.alloc_}
        , slots_{other.slots_}
        , bucket_count_{other.bucket_count_}
        , size_{other.size_}
        , hash_{other.hash_}
        , equal_{other.equal_}
    {
        other.slots_        = nullptr;
        other.bucket_count_ = 0;
        other.size_         = 0;
    }

    ~HashMap() noexcept {
        clear();
        if (slots_) {
            alloc_->dealloc(slots_,
                            bucket_count_ * sizeof(Slot),
                            alignof(Slot));
        }
    }

    HashMap& operator=(const HashMap& other) {
        if (this == &other) return *this;
        clear();
        for (size_type i = 0; i < other.bucket_count_; ++i) {
            if (!other.slots_[i].empty()) {
                insert_slot(other.slots_[i].key(), other.slots_[i].value());
            }
        }
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this == &other) return *this;
        this->~HashMap();
        new (this) HashMap(std::move(other));
        return *this;
    }

    // ── 容量 ──────────────────────────────────────────────────────────────

    [[nodiscard]] bool      empty()        const noexcept { return size_ == 0; }
    [[nodiscard]] size_type size()         const noexcept { return size_; }
    [[nodiscard]] size_type bucket_count() const noexcept { return bucket_count_; }

    // ── 迭代器访问 ────────────────────────────────────────────────────────

    [[nodiscard]] iterator begin() noexcept {
        return {slots_, slots_ + bucket_count_};
    }
    [[nodiscard]] iterator end() noexcept {
        return {slots_ + bucket_count_, slots_ + bucket_count_};
    }
    [[nodiscard]] const_iterator begin() const noexcept {
        return {slots_, slots_ + bucket_count_};
    }
    [[nodiscard]] const_iterator end() const noexcept {
        return {slots_ + bucket_count_, slots_ + bucket_count_};
    }
    [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] const_iterator cend()   const noexcept { return end(); }

    // ── 查找 ──────────────────────────────────────────────────────────────

    /**
     * @brief 查找键，返回指向值的指针；未找到则返回 nullptr。
     */
    [[nodiscard]] V* find(const K& key) noexcept {
        if (!slots_) return nullptr;
        const size_type idx = find_index(key);
        return idx == npos ? nullptr : &slots_[idx].value();
    }

    [[nodiscard]] const V* find(const K& key) const noexcept {
        if (!slots_) return nullptr;
        const size_type idx = find_index(key);
        return idx == npos ? nullptr : &slots_[idx].value();
    }

    [[nodiscard]] bool contains(const K& key) const noexcept {
        return find(key) != nullptr;
    }

    // ── 修改器 ────────────────────────────────────────────────────────────

    /**
     * @brief 插入键值对（若键已存在则不覆盖）。
     * @return 指向现有或新插入值的指针，以及是否发生了插入
     */
    struct InsertResult {
        V*   value;     ///< 指向键对应值的指针（始终非空）
        bool inserted;  ///< true = 新插入；false = 键已存在
    };

    InsertResult insert(const K& key, const V& value) {
        return emplace(key, value);
    }

    InsertResult insert(const K& key, V&& value) {
        return emplace(key, std::move(value));
    }

    InsertResult insert(K&& key, V&& value) {
        return emplace(std::move(key), std::move(value));
    }

    /**
     * @brief 在表中原位构造值。
     */
    template<typename KArg, typename... VArgs>
    InsertResult emplace(KArg&& key, VArgs&&... vargs) {
        ensure_capacity();
        return insert_impl(std::forward<KArg>(key),
                           std::forward<VArgs>(vargs)...);
    }

    /**
     * @brief 插入或更新：若键存在则用新值覆盖，否则插入。
     * @return 指向值的指针（始终非空）
     */
    template<typename KArg, typename... VArgs>
    V* insert_or_assign(KArg&& key, VArgs&&... vargs) {
        ensure_capacity();
        auto [ptr, ins] = insert_impl(std::forward<KArg>(key),
                                      std::forward<VArgs>(vargs)...);
        if (!ins) {
            // 键已存在：更新值
            ptr->~V();
            ::new (ptr) V(std::forward<VArgs>(vargs)...);
        }
        return ptr;
    }

    /**
     * @brief 通过键访问值（若键不存在则值初始化插入）。
     */
    V& operator[](const K& key) {
        ensure_capacity();
        auto [ptr, _] = insert_impl(key);
        return *ptr;
    }

    V& operator[](K&& key) {
        ensure_capacity();
        auto [ptr, _] = insert_impl(std::move(key));
        return *ptr;
    }

    /**
     * @brief 移除指定键。
     * @return true 表示成功删除，false 表示键不存在
     */
    bool erase(const K& key) noexcept {
        if (!slots_) return false;
        const size_type idx = find_index(key);
        if (idx == npos) return false;
        erase_at(idx);
        return true;
    }

    void clear() noexcept {
        if (!slots_) return;
        for (size_type i = 0; i < bucket_count_; ++i) {
            if (!slots_[i].empty()) {
                slots_[i].key().~K();
                slots_[i].value().~V();
                slots_[i].psl = -1;
            }
        }
        size_ = 0;
    }

    void swap(HashMap& other) noexcept {
        auto* ta = alloc_;         alloc_  = other.alloc_;        other.alloc_  = ta;
        auto* ts = slots_;         slots_  = other.slots_;        other.slots_  = ts;
        auto  tb = bucket_count_;  bucket_count_ = other.bucket_count_;
                                                   other.bucket_count_ = tb;
        auto  tsz = size_;         size_  = other.size_;          other.size_  = tsz;
    }

    [[nodiscard]] mine::core::IAllocator* allocator() const noexcept {
        return alloc_;
    }

private:
    mine::core::IAllocator* alloc_ = mine::core::default_allocator();
    Slot*     slots_        = nullptr;
    size_type bucket_count_ = 0;
    size_type size_         = 0;
    THash     hash_{};
    TEqual    equal_{};

    /// 最大负载因子（超过此值时触发 rehash）
    static constexpr double kMaxLoadFactor = 0.75;
    static constexpr size_type npos = size_type(-1);

    // ── 内部辅助 ──────────────────────────────────────────────────────────

    /// 将 v 向上取到最近的 2 的幂（v 为 0 时返回 1）
    static size_type next_power_of_two(size_type v) noexcept {
        if (v == 0) return 1;
        --v;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        if constexpr (sizeof(size_type) == 8) v |= v >> 32;
        return v + 1;
    }

    /// 分配 count 个 slot（已初始化为 psl=-1）
    void alloc_slots(size_type count) {
        void* raw = alloc_->alloc(count * sizeof(Slot), alignof(Slot));
        MINE_CHECK_MSG(raw != nullptr, "HashMap: 内存分配失败");
        slots_ = static_cast<Slot*>(raw);
        bucket_count_ = count;
        // 将所有 psl 初始化为 -1（Empty 状态）
        for (size_type i = 0; i < count; ++i) {
            slots_[i].psl = -1;
        }
    }

    /// 计算键的起始桶索引
    [[nodiscard]] size_type bucket_for(const K& key) const noexcept {
        return static_cast<size_type>(hash_(key)) & (bucket_count_ - 1);
    }

    /// 查找键的 slot 索引，未找到返回 npos
    [[nodiscard]] size_type find_index(const K& key) const noexcept {
        if (!slots_ || bucket_count_ == 0) return npos;
        size_type idx = bucket_for(key);
        int32_t   psl = 0;
        while (true) {
            const Slot& s = slots_[idx];
            if (s.empty() || psl > s.psl) return npos;
            if (s.psl == psl && equal_(s.key(), key)) return idx;
            idx = (idx + 1) & (bucket_count_ - 1);
            ++psl;
        }
    }

    /// 确保负载因子 < kMaxLoadFactor，必要时扩容
    void ensure_capacity() {
        if (!slots_) {
            alloc_slots(8);
            return;
        }
        if (static_cast<double>(size_ + 1) >
            static_cast<double>(bucket_count_) * kMaxLoadFactor)
        {
            rehash(bucket_count_ * 2);
        }
    }

    /// 将现有所有元素重新哈希到大小为 new_bucket_count 的新表
    void rehash(size_type new_bucket_count) {
        Slot*     old_slots  = slots_;
        size_type old_count  = bucket_count_;

        alloc_slots(new_bucket_count);
        size_ = 0;

        for (size_type i = 0; i < old_count; ++i) {
            if (!old_slots[i].empty()) {
                insert_slot(std::move(old_slots[i].key()),
                            std::move(old_slots[i].value()));
                old_slots[i].key().~K();
                old_slots[i].value().~V();
            }
        }

        alloc_->dealloc(old_slots, old_count * sizeof(Slot), alignof(Slot));
    }

    /// 将已有 K/V 插入当前表（用于 rehash 路径）
    void insert_slot(K&& key, V&& value) {
        insert_impl(std::move(key), std::move(value));
    }
    void insert_slot(const K& key, const V& value) {
        insert_impl(key, value);
    }

    /**
     * @brief Robin Hood 插入实现。
     *
     * @return { 指向最终 Value 的指针, 是否为新插入 }
     */
    template<typename KArg, typename... VArgs>
    InsertResult insert_impl(KArg&& key_arg, VArgs&&... vargs) {
        // 先查重：若键已存在直接返回
        const size_type existing = find_index(
            static_cast<const K&>(
                // 对于 K&& 的情况需要 const ref 查找；转换无 UB
                reinterpret_cast<const K&>(key_arg)));
        if (existing != npos) {
            return {&slots_[existing].value(), false};
        }

        // 构造待插入的临时 K/V（放在局部 slot 中，避免额外堆分配）
        alignas(alignof(Slot)) unsigned char tmp_slot_buf[sizeof(Slot)];
        Slot& tmp = *reinterpret_cast<Slot*>(tmp_slot_buf);
        ::new (&tmp.key_buf) K(std::forward<KArg>(key_arg));
        ::new (&tmp.val_buf) V(std::forward<VArgs>(vargs)...);
        tmp.psl = 0;

        size_type idx = bucket_for(tmp.key());
        V* result_ptr = nullptr;

        while (true) {
            Slot& cur = slots_[idx];
            if (cur.empty()) {
                // 找到空位，直接放入
                cur.psl = tmp.psl;
                ::new (&cur.key_buf) K(std::move(tmp.key()));
                ::new (&cur.val_buf) V(std::move(tmp.value()));
                tmp.key().~K();
                tmp.value().~V();
                if (!result_ptr) result_ptr = &cur.value();
                ++size_;
                return {result_ptr, true};
            }
            if (tmp.psl > cur.psl) {
                // Robin Hood 抢占：交换 tmp 与 cur
                if (!result_ptr) result_ptr = &cur.value();
                swap_slots(tmp, cur);
            }
            ++tmp.psl;
            idx = (idx + 1) & (bucket_count_ - 1);
        }
    }

    /// 交换两个 slot 的 K/V 与 psl
    static void swap_slots(Slot& a, Slot& b) noexcept {
        // 按字节交换（避免对未初始化内存调用 swap）
        unsigned char tmp_k[sizeof(K)];
        unsigned char tmp_v[sizeof(V)];
        std::memcpy(tmp_k, a.key_buf, sizeof(K));
        std::memcpy(tmp_v, a.val_buf, sizeof(V));
        std::memcpy(a.key_buf, b.key_buf, sizeof(K));
        std::memcpy(a.val_buf, b.val_buf, sizeof(V));
        std::memcpy(b.key_buf, tmp_k, sizeof(K));
        std::memcpy(b.val_buf, tmp_v, sizeof(V));
        int32_t t = a.psl; a.psl = b.psl; b.psl = t;
    }

    /// 移除 idx 处的元素（后向移位填补空洞）
    void erase_at(size_type idx) noexcept {
        slots_[idx].key().~K();
        slots_[idx].value().~V();
        slots_[idx].psl = -1;
        --size_;

        // 后向移位（Backward Shift Deletion）：将后续 psl>0 的元素向前移
        while (true) {
            const size_type next = (idx + 1) & (bucket_count_ - 1);
            Slot& nxt = slots_[next];
            if (nxt.empty() || nxt.psl == 0) break;
            // 将 nxt 移到 idx（减少 PSL）
            ::new (&slots_[idx].key_buf) K(std::move(nxt.key()));
            ::new (&slots_[idx].val_buf) V(std::move(nxt.value()));
            slots_[idx].psl = nxt.psl - 1;
            nxt.key().~K();
            nxt.value().~V();
            nxt.psl = -1;
            idx = next;
        }
    }
};

template<typename K, typename V, typename TH, typename TE>
void swap(HashMap<K, V, TH, TE>& a, HashMap<K, V, TH, TE>& b) noexcept {
    a.swap(b);
}

} // namespace mine::containers
