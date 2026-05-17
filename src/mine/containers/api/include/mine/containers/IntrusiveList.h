/**
 * @file IntrusiveList.h
 * @brief 侵入式双向链表。
 *
 * 侵入式链表要求节点类型 T 继承自 IntrusiveListNode（或持有其成员），
 * 链表本身不管理节点的内存分配——节点由外部（通常是对象本身）管理。
 *
 * 适用场景：
 *   - 对象需要同时存在于多个链表中（每个链表使用不同的 Hook 成员）
 *   - 需要 O(1) 从链表中移除自身（前提是持有指向节点的指针）
 *   - 避免为链表包装分配额外的节点内存（相比 std::list）
 *
 * 使用方法：
 * @code
 *   struct Widget : public IntrusiveListNode<Widget> {
 *       int id;
 *   };
 *
 *   IntrusiveList<Widget> list;
 *   Widget w1, w2;
 *   list.push_back(w1);
 *   list.push_back(w2);
 *   for (Widget& w : list) { /* ... *\/ }
 * @endcode
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <mine/core/Assert.h>

namespace mine::containers {

// ── 前向声明 ──────────────────────────────────────────────────────────────

template<typename T>
class IntrusiveList;

// ──────────────────────────────────────────────────────────────────────────────
// IntrusiveListNode：链表节点 Hook
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 侵入式链表节点基类。
 *
 * T 是持有此 Hook 的宿主类型（CRTP 模式），用于类型安全地转换指针。
 * 节点不持有宿主的所有权，生命周期完全由外部管理。
 */
template<typename T>
class IntrusiveListNode {
    friend class IntrusiveList<T>;

public:
    IntrusiveListNode() noexcept = default;

    // 节点不可拷贝/移动（防止意外地将 Hook 复制到另一个对象）
    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;
    IntrusiveListNode(IntrusiveListNode&&) = delete;
    IntrusiveListNode& operator=(IntrusiveListNode&&) = delete;

    ~IntrusiveListNode() noexcept {
        // 析构时自动从链表中脱离（若尚在链表中）
        if (is_linked()) {
            unlink();
        }
    }

    /// 判断此节点是否已被链入某个链表
    [[nodiscard]] bool is_linked() const noexcept {
        return prev_ != nullptr || next_ != nullptr;
    }

    /// 将自身从所在链表中移除（O(1)）
    void unlink() noexcept;

private:
    IntrusiveListNode* prev_ = nullptr;
    IntrusiveListNode* next_ = nullptr;

    [[nodiscard]] T* as_item() noexcept {
        return static_cast<T*>(this);
    }
    [[nodiscard]] const T* as_item() const noexcept {
        return static_cast<const T*>(this);
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// IntrusiveList：侵入式双向链表容器
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 侵入式双向链表。
 *
 * 使用哨兵节点（sentinel）简化边界处理：
 *   head_.next_ -> 第一个元素
 *   tail_.prev_ -> 最后一个元素
 *   空链表：head_.next_ == &tail_, tail_.prev_ == &head_
 *
 * @tparam T 节点宿主类型，必须继承自 IntrusiveListNode<T>
 */
template<typename T>
class IntrusiveList {
    static_assert(std::is_base_of_v<IntrusiveListNode<T>, T>,
        "IntrusiveList<T>: T 必须继承自 IntrusiveListNode<T>");

    using Node = IntrusiveListNode<T>;

public:
    // ── 迭代器 ────────────────────────────────────────────────────────────

    class iterator {
        friend class IntrusiveList;
    public:
        iterator(Node* n, Node* sentinel_tail) noexcept
            : node_{n}, tail_{sentinel_tail}
        {}

        [[nodiscard]] T& operator*()  noexcept { return *node_->as_item(); }
        [[nodiscard]] T* operator->() noexcept { return  node_->as_item(); }

        iterator& operator++() noexcept {
            node_ = node_->next_;
            return *this;
        }
        iterator operator++(int) noexcept {
            iterator tmp{*this};
            ++*this;
            return tmp;
        }
        iterator& operator--() noexcept {
            node_ = node_->prev_;
            return *this;
        }

        [[nodiscard]] bool operator==(const iterator& o) const noexcept {
            return node_ == o.node_;
        }
        [[nodiscard]] bool operator!=(const iterator& o) const noexcept {
            return node_ != o.node_;
        }

    private:
        Node* node_;
        Node* tail_;  // 哨兵尾节点（用于 end() 比较）
    };

    class const_iterator {
        friend class IntrusiveList;
    public:
        const_iterator(const Node* n, const Node* sentinel_tail) noexcept
            : node_{n}, tail_{sentinel_tail}
        {}
        /*implicit*/ const_iterator(const iterator& it) noexcept  // NOLINT(google-explicit-constructor)
            : node_{it.node_}, tail_{it.tail_}
        {}

        [[nodiscard]] const T& operator*()  const noexcept { return *node_->as_item(); }
        [[nodiscard]] const T* operator->() const noexcept { return  node_->as_item(); }

        const_iterator& operator++() noexcept {
            node_ = node_->next_;
            return *this;
        }
        const_iterator operator++(int) noexcept {
            const_iterator tmp{*this};
            ++*this;
            return tmp;
        }
        const_iterator& operator--() noexcept {
            node_ = node_->prev_;
            return *this;
        }

        [[nodiscard]] bool operator==(const const_iterator& o) const noexcept {
            return node_ == o.node_;
        }
        [[nodiscard]] bool operator!=(const const_iterator& o) const noexcept {
            return node_ != o.node_;
        }

    private:
        const Node* node_;
        const Node* tail_;
    };

    // ── 构造 / 析构 ───────────────────────────────────────────────────────

    IntrusiveList() noexcept {
        // 初始化哨兵：head_ <-> tail_
        head_.next_ = &tail_;
        tail_.prev_ = &head_;
    }

    // 链表不可拷贝（节点不能被多个链表同时持有）
    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    // 链表可移动
    IntrusiveList(IntrusiveList&& other) noexcept {
        head_.next_ = &tail_;
        tail_.prev_ = &head_;
        splice_from(other);
    }

    IntrusiveList& operator=(IntrusiveList&& other) noexcept {
        if (this == &other) return *this;
        clear();
        splice_from(other);
        return *this;
    }

    ~IntrusiveList() noexcept {
        // 将所有节点逐个 unlink（节点析构时也会 unlink，但这里提前处理）
        clear();
    }

    // ── 元素访问 ──────────────────────────────────────────────────────────

    [[nodiscard]] T& front() noexcept {
        MINE_ASSERT_MSG(!empty(), "IntrusiveList::front: 链表为空");
        return *head_.next_->as_item();
    }
    [[nodiscard]] const T& front() const noexcept {
        MINE_ASSERT_MSG(!empty(), "IntrusiveList::front: 链表为空");
        return *head_.next_->as_item();
    }
    [[nodiscard]] T& back() noexcept {
        MINE_ASSERT_MSG(!empty(), "IntrusiveList::back: 链表为空");
        return *tail_.prev_->as_item();
    }
    [[nodiscard]] const T& back() const noexcept {
        MINE_ASSERT_MSG(!empty(), "IntrusiveList::back: 链表为空");
        return *tail_.prev_->as_item();
    }

    // ── 迭代器访问 ────────────────────────────────────────────────────────

    [[nodiscard]] iterator begin() noexcept {
        return {head_.next_, &tail_};
    }
    [[nodiscard]] iterator end() noexcept {
        return {&tail_, &tail_};
    }
    [[nodiscard]] const_iterator begin() const noexcept {
        return {head_.next_, &tail_};
    }
    [[nodiscard]] const_iterator end() const noexcept {
        return {&tail_, &tail_};
    }
    [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] const_iterator cend()   const noexcept { return end(); }

    // ── 容量 ──────────────────────────────────────────────────────────────

    [[nodiscard]] bool empty() const noexcept {
        return head_.next_ == &tail_;
    }

    /// 返回链表长度（O(n)，尽量避免频繁调用）
    [[nodiscard]] size_t size() const noexcept {
        size_t count = 0;
        const Node* n = head_.next_;
        while (n != &tail_) {
            ++count;
            n = n->next_;
        }
        return count;
    }

    // ── 修改器 ────────────────────────────────────────────────────────────

    /// 在链表头部插入节点（O(1)）
    void push_front(T& item) noexcept {
        MINE_ASSERT_MSG(!item.is_linked(), "IntrusiveList::push_front: 节点已在链表中");
        insert_before(*head_.next_, item);
    }

    /// 在链表尾部插入节点（O(1)）
    void push_back(T& item) noexcept {
        MINE_ASSERT_MSG(!item.is_linked(), "IntrusiveList::push_back: 节点已在链表中");
        insert_before(tail_, item);
    }

    /// 移除并返回头部节点引用（O(1)）
    T& pop_front() noexcept {
        MINE_ASSERT_MSG(!empty(), "IntrusiveList::pop_front: 链表为空");
        T& item = *head_.next_->as_item();
        item.unlink();
        return item;
    }

    /// 移除并返回尾部节点引用（O(1)）
    T& pop_back() noexcept {
        MINE_ASSERT_MSG(!empty(), "IntrusiveList::pop_back: 链表为空");
        T& item = *tail_.prev_->as_item();
        item.unlink();
        return item;
    }

    /**
     * @brief 在 pos 指向的节点之前插入 item（O(1)）。
     * @param pos  插入位置（pos 必须是此链表的有效迭代器）
     * @param item 要插入的节点（不得已在任何链表中）
     * @return 指向新插入节点的迭代器
     */
    iterator insert(iterator pos, T& item) noexcept {
        MINE_ASSERT_MSG(!item.is_linked(), "IntrusiveList::insert: 节点已在链表中");
        insert_before(*pos.node_, item);
        return {static_cast<Node*>(&item), &tail_};
    }

    /**
     * @brief 移除 pos 处的节点（O(1)），返回指向下一个节点的迭代器。
     *
     * 注意：被移除的节点由外部管理内存，此函数仅修改链接关系。
     */
    iterator erase(iterator pos) noexcept {
        Node* next = pos.node_->next_;
        pos.node_->as_item()->unlink();
        return {next, &tail_};
    }

    /// 将链表中所有节点逐个 unlink（不释放内存）
    void clear() noexcept {
        while (!empty()) {
            head_.next_->as_item()->unlink();
        }
    }

private:
    Node head_;  ///< 哨兵头节点（不存储数据）
    Node tail_;  ///< 哨兵尾节点（不存储数据）

    /// 在 before 节点之前插入 item
    void insert_before(Node& before, T& item) noexcept {
        Node& node = static_cast<Node&>(item);
        node.prev_ = before.prev_;
        node.next_ = &before;
        before.prev_->next_ = &node;
        before.prev_ = &node;
    }

    /// 将 other 的所有节点接管到 this 的尾部（other 变为空链表）
    void splice_from(IntrusiveList& other) noexcept {
        if (other.empty()) return;
        // 连接 other 的节点段到 this 的 tail_ 之前
        Node* other_first = other.head_.next_;
        Node* other_last  = other.tail_.prev_;
        other_first->prev_ = tail_.prev_;
        other_last->next_  = &tail_;
        tail_.prev_->next_ = other_first;
        tail_.prev_        = other_last;
        // 重置 other 为空
        other.head_.next_ = &other.tail_;
        other.tail_.prev_ = &other.head_;
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// IntrusiveListNode::unlink 定义（需要 IntrusiveList 已完整）
// ──────────────────────────────────────────────────────────────────────────────

template<typename T>
void IntrusiveListNode<T>::unlink() noexcept {
    if (prev_) prev_->next_ = next_;
    if (next_) next_->prev_ = prev_;
    prev_ = nullptr;
    next_ = nullptr;
}

} // namespace mine::containers
