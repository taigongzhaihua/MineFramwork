/**
 * @file ObservableCollection.h
 * @brief ObservableCollection<T> —— 支持变更通知的动态数组容器。
 *
 * 提供类似 Vector<T> 的访问/修改接口，每次结构变更（Add/Remove/Move/Replace/Clear）
 * 自动触发 INotifyCollectionChanged 通知，使 UI 列表控件能做增量刷新。
 *
 * 典型用法：
 * @code
 *   class TodoViewModel : public ViewModelBase {
 *   public:
 *       ObservableCollection<TodoItem*>& items() noexcept { return items_; }
 *
 *       void add_item(TodoItem* item) noexcept {
 *           items_.add(item);  // 自动触发 CollectionChangedAction::Add
 *       }
 *
 *       void clear_items() noexcept {
 *           items_.clear();    // 自动触发 CollectionChangedAction::Reset
 *       }
 *   private:
 *       ObservableCollection<TodoItem*> items_;
 *   };
 * @endcode
 *
 * @note T 必须满足 MoveConstructible 和 MoveAssignable；
 *       remove(const T& item) 额外要求 T 满足 EqualityComparable。
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include <mine/containers/Vector.h>
#include <mine/core/Assert.h>
#include <mine/mvvm/Api.h>
#include <mine/mvvm/CollectionChangedArgs.h>
#include <mine/mvvm/ObservableCollectionBase.h>

namespace mine::mvvm {

/**
 * @brief 带变更通知的动态数组容器。
 *
 * @tparam T 元素类型（MoveConstructible + MoveAssignable）
 */
template<typename T>
class ObservableCollection : public ObservableCollectionBase {
public:
    using value_type      = T;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = T*;
    using const_iterator  = const T*;
    using size_type       = size_t;

    // ── 构造 ──────────────────────────────────────────────────────────────

    ObservableCollection() = default;
    ~ObservableCollection() override = default;

    // 不可拷贝（继承自 ObservableCollectionBase）

    // ── 容量与访问 ────────────────────────────────────────────────────────

    /// 返回当前元素数量
    [[nodiscard]] size_type size() const noexcept { return items_.size(); }

    /// 集合是否为空
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }

    /**
     * @brief 按索引访问元素（带断言的越界检查）。
     *
     * @param index 元素索引（0-based）
     */
    reference at(size_type index) noexcept {
        MINE_CHECK_MSG(index < items_.size(), "ObservableCollection::at 索引越界");
        return items_[index];
    }

    const_reference at(size_type index) const noexcept {
        MINE_CHECK_MSG(index < items_.size(), "ObservableCollection::at 索引越界");
        return items_[index];
    }

    reference       operator[](size_type index) noexcept       { return items_[index]; }
    const_reference operator[](size_type index) const noexcept { return items_[index]; }

    // ── 迭代器 ────────────────────────────────────────────────────────────

    iterator       begin() noexcept       { return items_.begin(); }
    iterator       end()   noexcept       { return items_.end(); }
    const_iterator begin() const noexcept { return items_.begin(); }
    const_iterator end()   const noexcept { return items_.end(); }

    // ── 修改操作（均触发变更通知）────────────────────────────────────────

    /**
     * @brief 在集合末尾添加元素，触发 Add 通知。
     *
     * @param item 要添加的元素
     */
    void add(T item) {
        const int new_index = static_cast<int>(items_.size());
        items_.push_back(std::move(item));
        notify_subscribers(CollectionChangedArgs{
            CollectionChangedAction::Add,
            new_index,
            -1,
            1,
        });
    }

    /**
     * @brief 在指定位置插入元素，触发 Add 通知。
     *
     * @param index 插入位置（0 = 头部，size() = 尾部）
     * @param item  要插入的元素
     */
    void insert(size_type index, T item) {
        MINE_CHECK_MSG(index <= items_.size(), "ObservableCollection::insert 索引越界");
        items_.insert(items_.begin() + static_cast<ptrdiff_t>(index), std::move(item));
        notify_subscribers(CollectionChangedArgs{
            CollectionChangedAction::Add,
            static_cast<int>(index),
            -1,
            1,
        });
    }

    /**
     * @brief 移除指定索引处的元素，触发 Remove 通知。
     *
     * @param index 要移除的元素索引
     */
    void remove_at(size_type index) {
        MINE_CHECK_MSG(index < items_.size(), "ObservableCollection::remove_at 索引越界");
        const int old_index = static_cast<int>(index);
        items_.erase(items_.begin() + static_cast<ptrdiff_t>(index));
        notify_subscribers(CollectionChangedArgs{
            CollectionChangedAction::Remove,
            -1,
            old_index,
            1,
        });
    }

    /**
     * @brief 移除第一个与 item 相等的元素，触发 Remove 通知。
     *
     * 要求 T 满足 EqualityComparable（即支持 operator==）。
     *
     * @param item  要移除的目标值
     * @return true 找到并移除；false 未找到
     */
    bool remove(const T& item)
        requires requires(const T& a, const T& b) { { a == b } -> std::convertible_to<bool>; }
    {
        for (size_type i = 0; i < items_.size(); ++i) {
            if (items_[i] == item) {
                remove_at(i);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 将指定索引处的元素替换为新值，触发 Replace 通知。
     *
     * @param index 要替换的元素索引
     * @param item  新值
     */
    void replace(size_type index, T item) {
        MINE_CHECK_MSG(index < items_.size(), "ObservableCollection::replace 索引越界");
        items_[index] = std::move(item);
        notify_subscribers(CollectionChangedArgs{
            CollectionChangedAction::Replace,
            static_cast<int>(index),
            static_cast<int>(index),
            1,
        });
    }

    /**
     * @brief 将元素从 from_index 移动到 to_index，触发 Move 通知。
     *
     * 移动后，from_index 处元素会被移除，并在 to_index 处插入。
     * 注意：to_index 是移动前的目标位置。
     *
     * @param from_index 源位置
     * @param to_index   目标位置
     */
    void move(size_type from_index, size_type to_index) {
        MINE_CHECK_MSG(from_index < items_.size(), "ObservableCollection::move from 索引越界");
        MINE_CHECK_MSG(to_index < items_.size(),   "ObservableCollection::move to 索引越界");
        if (from_index == to_index) return;

        // 提取元素，在目标位置插入
        T temp = std::move(items_[from_index]);
        items_.erase(items_.begin() + static_cast<ptrdiff_t>(from_index));
        // 插入位置需要调整：若目标在源之后，索引因删除而减 1
        const size_type insert_pos = (to_index > from_index) ? to_index - 1 : to_index;
        items_.insert(items_.begin() + static_cast<ptrdiff_t>(insert_pos), std::move(temp));

        notify_subscribers(CollectionChangedArgs{
            CollectionChangedAction::Move,
            static_cast<int>(insert_pos),
            static_cast<int>(from_index),
            1,
        });
    }

    /**
     * @brief 清空集合所有元素，触发 Reset 通知。
     */
    void clear() {
        if (items_.empty()) return;
        items_.clear();
        notify_subscribers(CollectionChangedArgs{
            CollectionChangedAction::Reset,
            -1,
            -1,
            0,
        });
    }

private:
    /// 存储集合元素的底层动态数组
    mine::containers::Vector<T> items_;
};

} // namespace mine::mvvm
