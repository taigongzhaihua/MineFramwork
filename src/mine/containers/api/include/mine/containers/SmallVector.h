/**
 * @file SmallVector.h
 * @brief 带内联缓冲区的小对象优化动态数组。
 *
 * SmallVector<T, N> 在元素数量不超过 N 时直接使用栈上的内联存储，
 * 避免堆分配；超过 N 后自动切换到堆分配，行为与 Vector<T> 相同。
 *
 * 适用场景：
 *   - 函数内短期存活的小集合（如命中测试候选列表、待刷新控件列表）
 *   - 大多数情况下元素数量在 N 以内，偶尔需要动态扩展
 *
 * 注意事项：
 *   - N 必须 > 0
 *   - 非平凡类型在内联存储中使用 placement new / 显式析构
 *   - SmallVector 不支持拷贝时跨实例共享内联存储，拷贝始终深拷贝
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
 * @brief 小对象优化的动态数组。
 *
 * @tparam T 元素类型
 * @tparam N 内联存储容量（元素数量，必须 > 0）
 */
template<typename T, size_t N>
    requires (N > 0)
class SmallVector {
    static_assert(N > 0, "SmallVector: N 必须大于 0");

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

    explicit SmallVector(mine::core::IAllocator* alloc
                         = mine::core::default_allocator()) noexcept
        : alloc_{alloc}
        , data_{inline_ptr()}
        , cap_{N}
    {}

    explicit SmallVector(size_type count,
                         mine::core::IAllocator* alloc
                         = mine::core::default_allocator())
        : SmallVector{alloc}
    {
        resize(count);
    }

    SmallVector(size_type count, const T& value,
                mine::core::IAllocator* alloc
                = mine::core::default_allocator())
        : SmallVector{alloc}
    {
        resize(count, value);
    }

    SmallVector(std::initializer_list<T> init,
                mine::core::IAllocator* alloc
                = mine::core::default_allocator())
        : SmallVector{alloc}
    {
        reserve(init.size());
        for (const auto& v : init) push_back(v);
    }

    template<typename InputIt>
        requires (!std::is_integral_v<InputIt>)
    SmallVector(InputIt first, InputIt last,
                mine::core::IAllocator* alloc
                = mine::core::default_allocator())
        : SmallVector{alloc}
    {
        for (; first != last; ++first) push_back(*first);
    }

    SmallVector(const SmallVector& other)
        : alloc_{other.alloc_}
        , data_{inline_ptr()}
        , cap_{N}
    {
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
    }

    SmallVector(SmallVector&& other) noexcept
        : alloc_{other.alloc_}
        , size_{other.size_}
    {
        if (other.is_heap()) {
            // 堆分配：直接转移指针
            data_       = other.data_;
            cap_        = other.cap_;
            other.data_ = other.inline_ptr();
            other.cap_  = N;
            other.size_ = 0;
        } else {
            // 内联存储：必须逐元素移动
            data_ = inline_ptr();
            cap_  = N;
            if constexpr (std::is_trivially_move_constructible_v<T>) {
                std::memcpy(inline_buf_, other.inline_buf_,
                            size_ * sizeof(T));
            } else {
                for (size_type i = 0; i < size_; ++i) {
                    ::new (data_ + i) T(std::move(other.data_[i]));
                    other.data_[i].~T();
                }
            }
            other.size_ = 0;
        }
    }

    ~SmallVector() noexcept {
        destroy_range(data_, size_);
        if (is_heap()) {
            alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
        }
    }

    // ── 赋值 ──────────────────────────────────────────────────────────────

    SmallVector& operator=(const SmallVector& other) {
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

    SmallVector& operator=(SmallVector&& other) noexcept {
        if (this == &other) return *this;
        clear();
        if (is_heap()) {
            alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
            data_ = inline_ptr();
            cap_  = N;
        }
        alloc_ = other.alloc_;
        size_  = other.size_;
        if (other.is_heap()) {
            data_       = other.data_;
            cap_        = other.cap_;
            other.data_ = other.inline_ptr();
            other.cap_  = N;
            other.size_ = 0;
        } else {
            if constexpr (std::is_trivially_move_constructible_v<T>) {
                std::memcpy(inline_buf_, other.inline_buf_,
                            size_ * sizeof(T));
            } else {
                for (size_type i = 0; i < size_; ++i) {
                    ::new (data_ + i) T(std::move(other.data_[i]));
                    other.data_[i].~T();
                }
            }
            other.size_ = 0;
        }
        return *this;
    }

    SmallVector& operator=(std::initializer_list<T> init) {
        clear();
        reserve(init.size());
        for (const auto& v : init) push_back(v);
        return *this;
    }

    // ── 元素访问 ──────────────────────────────────────────────────────────

    [[nodiscard]] reference operator[](size_type idx) noexcept {
        MINE_ASSERT_MSG(idx < size_, "SmallVector: 下标越界");
        return data_[idx];
    }
    [[nodiscard]] const_reference operator[](size_type idx) const noexcept {
        MINE_ASSERT_MSG(idx < size_, "SmallVector: 下标越界");
        return data_[idx];
    }
    [[nodiscard]] reference at(size_type idx) noexcept {
        MINE_CHECK_MSG(idx < size_, "SmallVector::at: 下标越界");
        return data_[idx];
    }
    [[nodiscard]] const_reference at(size_type idx) const noexcept {
        MINE_CHECK_MSG(idx < size_, "SmallVector::at: 下标越界");
        return data_[idx];
    }
    [[nodiscard]] reference       front() noexcept {
        MINE_ASSERT_MSG(size_ > 0, "SmallVector::front: 容器为空");
        return data_[0];
    }
    [[nodiscard]] const_reference front() const noexcept {
        MINE_ASSERT_MSG(size_ > 0, "SmallVector::front: 容器为空");
        return data_[0];
    }
    [[nodiscard]] reference       back() noexcept {
        MINE_ASSERT_MSG(size_ > 0, "SmallVector::back: 容器为空");
        return data_[size_ - 1];
    }
    [[nodiscard]] const_reference back() const noexcept {
        MINE_ASSERT_MSG(size_ > 0, "SmallVector::back: 容器为空");
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

    /// 是否正在使用内联存储（未发生堆分配）
    [[nodiscard]] bool is_small() const noexcept { return !is_heap(); }

    void reserve(size_type new_cap) {
        if (new_cap <= cap_) return;
        grow_to(new_cap);
    }

    void shrink_to_fit() {
        if (size_ == cap_) return;
        if (size_ == 0 && is_heap()) {
            alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
            data_ = inline_ptr();
            cap_  = N;
            return;
        }
        if (size_ <= N && is_heap()) {
            // 可以退回内联存储
            T* old_data = data_;
            size_type old_cap = cap_;
            data_ = inline_ptr();
            cap_  = N;
            if constexpr (std::is_trivially_move_constructible_v<T>) {
                std::memcpy(data_, old_data, size_ * sizeof(T));
            } else {
                for (size_type i = 0; i < size_; ++i) {
                    ::new (data_ + i) T(std::move(old_data[i]));
                    old_data[i].~T();
                }
            }
            alloc_->dealloc(old_data, old_cap * sizeof(T), alignof(T));
        } else if (is_heap()) {
            relocate(size_);
        }
    }

    // ── 修改器 ────────────────────────────────────────────────────────────

    void clear() noexcept {
        destroy_range(data_, size_);
        size_ = 0;
    }

    void push_back(const T& value) {
        ensure_capacity();
        ::new (data_ + size_) T(value);
        ++size_;
    }

    void push_back(T&& value) {
        ensure_capacity();
        ::new (data_ + size_) T(std::move(value));
        ++size_;
    }

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        ensure_capacity();
        ::new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back() noexcept {
        MINE_ASSERT_MSG(size_ > 0, "SmallVector::pop_back: 容器为空");
        --size_;
        data_[size_].~T();
    }

    void resize(size_type new_size) {
        if (new_size > size_) {
            reserve(new_size);
            if constexpr (std::is_trivially_default_constructible_v<T>) {
                std::memset(data_ + size_, 0,
                            (new_size - size_) * sizeof(T));
            } else {
                for (size_type i = size_; i < new_size; ++i)
                    ::new (data_ + i) T();
            }
        } else {
            destroy_range(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void resize(size_type new_size, const T& value) {
        if (new_size > size_) {
            reserve(new_size);
            for (size_type i = size_; i < new_size; ++i)
                ::new (data_ + i) T(value);
        } else {
            destroy_range(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void swap(SmallVector& other) noexcept {
        // 两个都在堆：直接交换指针
        if (is_heap() && other.is_heap()) {
            auto* td = data_;    data_  = other.data_;  other.data_ = td;
            auto  tc = cap_;     cap_   = other.cap_;   other.cap_  = tc;
            auto  ts = size_;    size_  = other.size_;  other.size_ = ts;
            auto* ta = alloc_;   alloc_ = other.alloc_; other.alloc_= ta;
            return;
        }
        // 通用路径：逐元素交换（可能涉及内联缓冲区，不能简单 swap 指针）
        SmallVector tmp{std::move(other)};
        other = std::move(*this);
        *this = std::move(tmp);
    }

    // ── 转换 ──────────────────────────────────────────────────────────────

    [[nodiscard]] operator mine::core::Span<T>() noexcept {
        return {data_, size_};
    }
    [[nodiscard]] operator mine::core::Span<const T>() const noexcept {
        return {data_, size_};
    }
    [[nodiscard]] mine::core::Span<const T> as_span() const noexcept {
        return {data_, size_};
    }

    [[nodiscard]] mine::core::IAllocator* allocator() const noexcept {
        return alloc_;
    }

    // ── 相等比较 ──────────────────────────────────────────────────────────

    [[nodiscard]] bool operator==(const SmallVector& other) const noexcept {
        if (size_ != other.size_) return false;
        if constexpr (std::is_trivially_copyable_v<T>) {
            return std::memcmp(data_, other.data_, size_ * sizeof(T)) == 0;
        } else {
            for (size_type i = 0; i < size_; ++i)
                if (!(data_[i] == other.data_[i])) return false;
            return true;
        }
    }
    [[nodiscard]] bool operator!=(const SmallVector& other) const noexcept {
        return !(*this == other);
    }

private:
    mine::core::IAllocator* alloc_ = mine::core::default_allocator();
    T*        data_  = nullptr;   ///< 当前活动缓冲区（内联或堆）
    size_type size_  = 0;
    size_type cap_   = N;

    /// 内联缓冲区（以 max_align_t 对齐，存储 N 个 T 的原始字节）
    alignas(alignof(T)) unsigned char inline_buf_[sizeof(T) * N];

    // ── 内部辅助 ──────────────────────────────────────────────────────────

    [[nodiscard]] T* inline_ptr() noexcept {
        return reinterpret_cast<T*>(inline_buf_);
    }
    [[nodiscard]] const T* inline_ptr() const noexcept {
        return reinterpret_cast<const T*>(inline_buf_);
    }

    /// 判断当前使用堆分配（data_ 不指向内联缓冲区）
    [[nodiscard]] bool is_heap() const noexcept {
        return data_ != inline_ptr();
    }

    void ensure_capacity(size_type extra = 1) {
        if (size_ + extra > cap_) {
            size_type new_cap = cap_ < 8 ? 8 : cap_ * 2;
            while (new_cap < size_ + extra) new_cap *= 2;
            grow_to(new_cap);
        }
    }

    void grow_to(size_type new_cap) {
        MINE_ASSERT(new_cap >= size_);
        if (new_cap <= N && !is_heap()) return;  // 已在内联范围内
        relocate(new_cap);
    }

    void relocate(size_type new_cap) {
        void* raw = alloc_->alloc(new_cap * sizeof(T), alignof(T));
        MINE_CHECK_MSG(raw != nullptr, "SmallVector: 内存分配失败");
        T* new_data = static_cast<T*>(raw);
        if constexpr (std::is_trivially_move_constructible_v<T>) {
            if (size_ > 0) std::memcpy(new_data, data_, size_ * sizeof(T));
        } else {
            for (size_type i = 0; i < size_; ++i) {
                ::new (new_data + i) T(std::move(data_[i]));
                data_[i].~T();
            }
        }
        if (is_heap()) {
            alloc_->dealloc(data_, cap_ * sizeof(T), alignof(T));
        }
        data_ = new_data;
        cap_  = new_cap;
    }

    static void destroy_range(T* ptr, size_type count) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < count; ++i) ptr[i].~T();
        }
    }
};

// ── 非成员交换 ─────────────────────────────────────────────────────────────

template<typename T, size_t N>
void swap(SmallVector<T, N>& a, SmallVector<T, N>& b) noexcept {
    a.swap(b);
}

} // namespace mine::containers
