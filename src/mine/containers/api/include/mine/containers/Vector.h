/**
 * @file Vector.h
 * @brief 动态数组容器，语义上等价于 std::vector 的无异常子集。
 *
 * 特性：
 *   - 使用 mine::core::IAllocator 进行堆内存管理（可替换）
 *   - 容量增长策略：2 倍增长，最小 8 个元素
 *   - 分配失败时通过 MINE_CHECK 断言终止，不抛异常
 *   - 支持平凡类型（memcpy/memmove 优化）与非平凡类型（placement new/析构）
 *   - 迭代器为裸指针，与标准算法兼容
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <new>
#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>
#include <mine/core/Span.h>

namespace mine::containers {

/**
 * @brief 动态数组容器。
 *
 * 模板参数 T 的约束：
 *   - 可析构（Destructible）
 *   - 可移动构造（MoveConstructible）
 *
 * 对于拷贝操作，还要求 T 可拷贝构造（CopyConstructible）。
 */
template<typename T>
class Vector {
public:
    // ── 类型别名 ──────────────────────────────────────────────────────────
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = T*;
    using const_iterator  = const T*;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    // ── 构造 / 析构 ───────────────────────────────────────────────────────

    /// 默认构造：使用全局默认分配器，空容器
    explicit Vector(mine::core::IAllocator* alloc
                    = mine::core::default_allocator()) noexcept
        : alloc_{alloc}
    {}

    /// 创建含 count 个值初始化元素的容器
    explicit Vector(size_type count,
                    mine::core::IAllocator* alloc
                    = mine::core::default_allocator())
        : alloc_{alloc}
    {
        resize(count);
    }

    /// 创建含 count 个拷贝自 value 的元素的容器
    Vector(size_type count, const T& value,
           mine::core::IAllocator* alloc
           = mine::core::default_allocator())
        : alloc_{alloc}
    {
        resize(count, value);
    }

    /// 从初始化列表构造
    Vector(std::initializer_list<T> init,
           mine::core::IAllocator* alloc
           = mine::core::default_allocator())
        : alloc_{alloc}
    {
        reserve(init.size());
        for (const auto& v : init) {
            push_back(v);
        }
    }

    /// 从迭代器区间构造（[first, last)）
    template<typename InputIt>
        requires (!std::is_integral_v<InputIt>)
    Vector(InputIt first, InputIt last,
           mine::core::IAllocator* alloc
           = mine::core::default_allocator())
        : alloc_{alloc}
    {
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    /// 拷贝构造（深拷贝每个元素）
    Vector(const Vector& other)
        : alloc_{other.alloc_}
    {
        reserve(other.size_);
        if constexpr (std::is_trivially_copy_constructible_v<T>) {
            // 平凡类型直接 memcpy
            std::memcpy(data_, other.data_, other.size_ * sizeof(T));
            size_ = other.size_;
        } else {
            for (size_type i = 0; i < other.size_; ++i) {
                ::new (data_ + i) T(other.data_[i]);
                ++size_;
            }
        }
    }

    /// 移动构造（O(1) 转移所有权）
    Vector(Vector&& other) noexcept
        : alloc_{other.alloc_}
        , data_{other.data_}
        , size_{other.size_}
        , cap_{other.cap_}
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.cap_  = 0;
    }

    ~Vector() noexcept {
        destroy_range(data_, size_);
        if (data_) {
            alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
        }
    }

    // ── 赋值 ──────────────────────────────────────────────────────────────

    Vector& operator=(const Vector& other) {
        if (this == &other) return *this;
        clear();
        reserve(other.size_);
        if constexpr (std::is_trivially_copy_constructible_v<T>) {
            std::memcpy(data_, other.data_, other.size_ * sizeof(T));
            size_ = other.size_;
        } else {
            for (size_type i = 0; i < other.size_; ++i) {
                ::new (data_ + i) T(other.data_[i]);
                ++size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this == &other) return *this;
        destroy_range(data_, size_);
        if (data_) {
            alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
        }
        alloc_ = other.alloc_;
        data_  = other.data_;
        size_  = other.size_;
        cap_   = other.cap_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.cap_  = 0;
        return *this;
    }

    Vector& operator=(std::initializer_list<T> init) {
        clear();
        reserve(init.size());
        for (const auto& v : init) {
            push_back(v);
        }
        return *this;
    }

    // ── 元素访问 ──────────────────────────────────────────────────────────

    /// 下标访问（Debug 模式下范围检查）
    [[nodiscard]] reference operator[](size_type idx) noexcept {
        MINE_ASSERT_MSG(idx < size_, "Vector: 下标越界");
        return data_[idx];
    }
    [[nodiscard]] const_reference operator[](size_type idx) const noexcept {
        MINE_ASSERT_MSG(idx < size_, "Vector: 下标越界");
        return data_[idx];
    }

    /// 带始终有效范围检查的访问
    [[nodiscard]] reference at(size_type idx) noexcept {
        MINE_CHECK_MSG(idx < size_, "Vector::at: 下标越界");
        return data_[idx];
    }
    [[nodiscard]] const_reference at(size_type idx) const noexcept {
        MINE_CHECK_MSG(idx < size_, "Vector::at: 下标越界");
        return data_[idx];
    }

    [[nodiscard]] reference       front() noexcept {
        MINE_ASSERT_MSG(size_ > 0, "Vector::front: 容器为空");
        return data_[0];
    }
    [[nodiscard]] const_reference front() const noexcept {
        MINE_ASSERT_MSG(size_ > 0, "Vector::front: 容器为空");
        return data_[0];
    }
    [[nodiscard]] reference       back() noexcept {
        MINE_ASSERT_MSG(size_ > 0, "Vector::back: 容器为空");
        return data_[size_ - 1];
    }
    [[nodiscard]] const_reference back() const noexcept {
        MINE_ASSERT_MSG(size_ > 0, "Vector::back: 容器为空");
        return data_[size_ - 1];
    }

    [[nodiscard]] pointer       data() noexcept       { return data_; }
    [[nodiscard]] const_pointer data() const noexcept { return data_; }

    // ── 迭代器 ────────────────────────────────────────────────────────────

    [[nodiscard]] iterator       begin() noexcept        { return data_; }
    [[nodiscard]] const_iterator begin() const noexcept  { return data_; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data_; }
    [[nodiscard]] iterator       end() noexcept          { return data_ + size_; }
    [[nodiscard]] const_iterator end() const noexcept    { return data_ + size_; }
    [[nodiscard]] const_iterator cend() const noexcept   { return data_ + size_; }

    // ── 容量 ──────────────────────────────────────────────────────────────

    [[nodiscard]] bool      empty()    const noexcept { return size_ == 0; }
    [[nodiscard]] size_type size()     const noexcept { return size_; }
    [[nodiscard]] size_type capacity() const noexcept { return cap_; }

    /// 预分配至少 new_cap 个元素的容量（不缩减）
    void reserve(size_type new_cap) {
        if (new_cap <= cap_) return;
        grow_to(new_cap);
    }

    /// 释放多余容量（收缩到当前 size）
    void shrink_to_fit() {
        if (size_ == cap_) return;
        if (size_ == 0) {
            if (data_) {
                alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
                data_ = nullptr;
                cap_  = 0;
            }
            return;
        }
        relocate(size_);
    }

    // ── 修改器 ────────────────────────────────────────────────────────────

    /// 清空所有元素（不释放内存）
    void clear() noexcept {
        destroy_range(data_, size_);
        size_ = 0;
    }

    /// 尾部追加元素（拷贝），仅当 T 可拷贝构造时可用，避免 MSVC 即时模板实例化错误
    void push_back(const T& value) requires std::is_copy_constructible_v<T> {
        ensure_capacity();
        ::new (data_ + size_) T(value);
        ++size_;
    }

    /// 尾部追加元素（移动）
    void push_back(T&& value) {
        ensure_capacity();
        ::new (data_ + size_) T(std::move(value));
        ++size_;
    }

    /// 尾部原位构造元素，返回对新元素的引用
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        ensure_capacity();
        ::new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    /// 移除尾部元素
    void pop_back() noexcept {
        MINE_ASSERT_MSG(size_ > 0, "Vector::pop_back: 容器为空");
        --size_;
        data_[size_].~T();
    }

    /// 调整大小：增大时值初始化，减小时析构多余元素
    void resize(size_type new_size) {
        if (new_size > size_) {
            reserve(new_size);
            if constexpr (std::is_trivially_default_constructible_v<T>) {
                std::memset(data_ + size_, 0,
                            (new_size - size_) * sizeof(T));
            } else {
                for (size_type i = size_; i < new_size; ++i) {
                    ::new (data_ + i) T();
                }
            }
        } else {
            destroy_range(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    /// 调整大小：增大时用 value 填充
    void resize(size_type new_size, const T& value) {
        if (new_size > size_) {
            reserve(new_size);
            for (size_type i = size_; i < new_size; ++i) {
                ::new (data_ + i) T(value);
            }
        } else {
            destroy_range(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    /**
     * @brief 在 pos 处插入元素（拷贝），返回指向新元素的迭代器。
     * @param pos 插入位置，必须是 [begin(), end()] 内的有效迭代器
     */
    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }

    /// 在 pos 处插入元素（移动）
    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, std::move(value));
    }

    /// 在 pos 处插入 count 个 value 的拷贝
    iterator insert(const_iterator pos, size_type count, const T& value) {
        if (count == 0) return const_cast<iterator>(pos);
        const size_type idx = static_cast<size_type>(pos - data_);
        MINE_ASSERT_MSG(idx <= size_, "Vector::insert: 位置无效");
        ensure_capacity(count);
        // 将 [idx, size_) 区间元素向后移动 count 位
        shift_right(idx, count);
        for (size_type i = 0; i < count; ++i) {
            ::new (data_ + idx + i) T(value);
        }
        size_ += count;
        return data_ + idx;
    }

    /// 在 pos 处原位构造元素
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        const size_type idx = static_cast<size_type>(pos - data_);
        MINE_ASSERT_MSG(idx <= size_, "Vector::emplace: 位置无效");
        ensure_capacity();
        shift_right(idx, 1);
        ::new (data_ + idx) T(std::forward<Args>(args)...);
        ++size_;
        return data_ + idx;
    }

    /**
     * @brief 移除 pos 处的元素，返回指向下一个元素的迭代器。
     * @param pos 必须是 [begin(), end()) 内的有效迭代器
     */
    iterator erase(const_iterator pos) noexcept {
        const size_type idx = static_cast<size_type>(pos - data_);
        MINE_ASSERT_MSG(idx < size_, "Vector::erase: 位置无效");
        data_[idx].~T();
        shift_left(idx + 1, 1);
        --size_;
        return data_ + idx;
    }

    /// 移除 [first, last) 区间的元素
    iterator erase(const_iterator first, const_iterator last) noexcept {
        const size_type f = static_cast<size_type>(first - data_);
        const size_type l = static_cast<size_type>(last - data_);
        MINE_ASSERT_MSG(f <= l && l <= size_, "Vector::erase: 范围无效");
        const size_type count = l - f;
        if (count == 0) return data_ + f;
        destroy_range(data_ + f, count);
        shift_left(l, count);
        size_ -= count;
        return data_ + f;
    }

    /// 交换两个容器的内容（O(1)）
    void swap(Vector& other) noexcept {
        auto* tmp_alloc = alloc_;  alloc_ = other.alloc_;  other.alloc_ = tmp_alloc;
        auto* tmp_data  = data_;   data_  = other.data_;   other.data_  = tmp_data;
        auto  tmp_size  = size_;   size_  = other.size_;   other.size_  = tmp_size;
        auto  tmp_cap   = cap_;    cap_   = other.cap_;    other.cap_   = tmp_cap;
    }

    // ── 转换 ──────────────────────────────────────────────────────────────

    /// 隐式转换为 Span<T>（非拥有视图）
    [[nodiscard]] operator mine::core::Span<T>() noexcept {
        return mine::core::Span<T>{data_, size_};
    }
    [[nodiscard]] operator mine::core::Span<const T>() const noexcept {
        return mine::core::Span<const T>{data_, size_};
    }

    /// 获取只读 span 视图
    [[nodiscard]] mine::core::Span<const T> as_span() const noexcept {
        return {data_, size_};
    }

    // ── 分配器访问 ────────────────────────────────────────────────────────

    [[nodiscard]] mine::core::IAllocator* allocator() const noexcept {
        return alloc_;
    }

    // ── 相等比较 ──────────────────────────────────────────────────────────

    [[nodiscard]] bool operator==(const Vector& other) const noexcept {
        if (size_ != other.size_) return false;
        if constexpr (std::is_trivially_copyable_v<T>) {
            return std::memcmp(data_, other.data_, size_ * sizeof(T)) == 0;
        } else {
            for (size_type i = 0; i < size_; ++i) {
                if (!(data_[i] == other.data_[i])) return false;
            }
            return true;
        }
    }

    [[nodiscard]] bool operator!=(const Vector& other) const noexcept {
        return !(*this == other);
    }

private:
    mine::core::IAllocator* alloc_ = mine::core::default_allocator();
    T*        data_  = nullptr;   ///< 元素缓冲区指针
    size_type size_  = 0;         ///< 当前元素数量
    size_type cap_   = 0;         ///< 已分配容量

    // ── 内部辅助 ──────────────────────────────────────────────────────────

    /// 确保至少还有 extra 个空位，必要时扩容
    void ensure_capacity(size_type extra = 1) {
        if (size_ + extra > cap_) {
            // 2 倍增长，最小容量 8
            size_type new_cap = cap_ == 0 ? 8 : cap_ * 2;
            while (new_cap < size_ + extra) new_cap *= 2;
            grow_to(new_cap);
        }
    }

    /// 扩容到精确的 new_cap（分配并移动现有元素）
    void grow_to(size_type new_cap) {
        MINE_ASSERT(new_cap >= size_);
        relocate(new_cap);
    }

    /// 重新分配缓冲区到 new_cap 并移动元素
    void relocate(size_type new_cap) {
        void* raw = alloc_->alloc(new_cap * sizeof(T), alignof(T));
        MINE_CHECK_MSG(raw != nullptr, "Vector: 内存分配失败");
        T* new_data = static_cast<T*>(raw);
        if constexpr (std::is_trivially_move_constructible_v<T>) {
            // 平凡移动：直接 memcpy（更快）
            if (data_ && size_ > 0) {
                std::memcpy(new_data, data_, size_ * sizeof(T));
            }
        } else {
            // 非平凡类型：placement new + 析构原始元素
            for (size_type i = 0; i < size_; ++i) {
                ::new (new_data + i) T(std::move(data_[i]));
                data_[i].~T();
            }
        }
        if (data_) {
            alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
        }
        data_ = new_data;
        cap_  = new_cap;
    }

    /// 析构 [ptr, ptr+count) 内的元素
    static void destroy_range(T* ptr, size_type count) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < count; ++i) {
                ptr[i].~T();
            }
        }
    }

    /**
     * @brief 将 [idx, size_) 的元素向右移动 n 个位置，在 [idx, idx+n) 留出空位。
     *
     * 调用前必须已通过 ensure_capacity 保证 size_ + n <= cap_。
     * 空出的位置未初始化，调用方负责在其中构造元素。
     */
    void shift_right(size_type idx, size_type n) noexcept {
        const size_type count = size_ - idx;  // 需要移动的元素数量
        if (count == 0) return;

        if constexpr (std::is_trivially_move_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            // 平凡类型：memmove 整块移动
            std::memmove(data_ + idx + n, data_ + idx, count * sizeof(T));
        } else {
            // 从尾部向前逐个移动（避免覆盖）
            for (size_type i = count; i > 0; --i) {
                const size_type src = idx + i - 1;
                const size_type dst = src + n;
                if (dst >= size_) {
                    // 目标位置尚未构造，用 placement new 移动
                    ::new (data_ + dst) T(std::move(data_[src]));
                } else {
                    // 目标位置已构造，用移动赋值
                    data_[dst] = std::move(data_[src]);
                }
                if (src < size_) {
                    data_[src].~T();  // 析构已移走的源元素
                }
            }
        }
    }

    /**
     * @brief 将 [idx, size_) 的元素向左移动 n 个位置，填补 [idx-n, idx) 的空位。
     *
     * 调用前 [idx-n, idx) 区间的元素必须已被析构。
     */
    void shift_left(size_type idx, size_type n) noexcept {
        const size_type count = size_ - idx;  // 需要移动的元素数量
        if (count == 0) return;

        if constexpr (std::is_trivially_move_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            std::memmove(data_ + idx - n, data_ + idx, count * sizeof(T));
        } else {
            for (size_type i = 0; i < count; ++i) {
                const size_type src = idx + i;
                const size_type dst = src - n;
                if (dst >= size_ - n) {
                    // 目标位置尚未构造
                    ::new (data_ + dst) T(std::move(data_[src]));
                } else {
                    data_[dst] = std::move(data_[src]);
                }
                data_[src].~T();
            }
        }
    }
};

// ── 非成员交换 ─────────────────────────────────────────────────────────────

template<typename T>
void swap(Vector<T>& a, Vector<T>& b) noexcept {
    a.swap(b);
}

} // namespace mine::containers
