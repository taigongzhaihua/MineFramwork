/**
 * @file Span.h
 * @brief Span<T> — 非拥有的连续内存视图（数组视图）。
 *
 * 类似 std::span（C++20），但不引入 <span> 头文件，且对下标越界做断言检查。
 * Span<T> 允许通过元素写回（可变视图），Span<const T> 为只读视图。
 *
 * 公共头中不引入 STL 容器头。
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <iterator>

#include <mine/core/Assert.h>

namespace mine::core {

/**
 * @brief 非拥有的连续内存视图，持有 [data, data+size) 区间。
 *
 * @tparam T 元素类型（可为 const）
 *
 * 构造方式：
 * @code
 *   int arr[] = {1,2,3};
 *   Span<int>       s1{arr, 3};         // 从指针+长度
 *   Span<const int> s2{arr};            // 从数组引用（自动推导大小）
 *   Span<int>       s3 = s1.subspan(1); // 子视图
 * @endcode
 */
template<typename T>
class Span {
public:
    using element_type    = T;
    using value_type      = std::remove_cv_t<T>;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using iterator        = T*;
    using const_iterator  = const T*;

    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 构造空视图
    constexpr Span() noexcept = default;

    /// 从指针和元素数构造
    constexpr Span(T* data, size_type count) noexcept
        : data_{data}, size_{count}
    {
        MINE_ASSERT_MSG(data != nullptr || count == 0,
            "Span: 非零大小但指针为 nullptr");
    }

    /// 从 C 数组引用构造（自动推导元素个数）
    template<size_t N>
    constexpr /*implicit*/ Span(T (&arr)[N]) noexcept  // NOLINT(google-explicit-constructor)
        : data_{arr}, size_{N}
    {}

    /// 从具有 .data()/.size() 成员的容器构造（不引入 STL 头）
    /// 用于兼容 Vector<T>、SmallVector<T> 等自有容器
    template<typename Container>
        requires requires(Container& c) {
            { c.data() } -> std::convertible_to<T*>;
            { c.size() } -> std::convertible_to<size_t>;
        }
    constexpr /*implicit*/ Span(Container& c) noexcept  // NOLINT(google-explicit-constructor)
        : data_{c.data()}, size_{c.size()}
    {}

    /// 从 const 容器构造（当 T 为 const 时）
    template<typename Container>
        requires requires(const Container& c) {
            { c.data() } -> std::convertible_to<T*>;
            { c.size() } -> std::convertible_to<size_t>;
        }
    constexpr /*implicit*/ Span(const Container& c) noexcept  // NOLINT(google-explicit-constructor)
        : data_{c.data()}, size_{c.size()}
    {}

    // ── 容量 ──────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr T*        data()  const noexcept { return data_; }
    [[nodiscard]] constexpr size_type size()  const noexcept { return size_; }
    [[nodiscard]] constexpr bool      empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr size_type size_bytes() const noexcept {
        return size_ * sizeof(T);
    }

    // ── 元素访问 ──────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr T& operator[](size_type i) const noexcept {
        MINE_ASSERT(i < size_);
        return data_[i];
    }

    [[nodiscard]] constexpr T& front() const noexcept {
        MINE_ASSERT(!empty());
        return data_[0];
    }

    [[nodiscard]] constexpr T& back() const noexcept {
        MINE_ASSERT(!empty());
        return data_[size_ - 1];
    }

    // ── 迭代器 ────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr iterator begin() const noexcept { return data_; }
    [[nodiscard]] constexpr iterator end()   const noexcept { return data_ + size_; }

    // ── 子视图 ────────────────────────────────────────────────────────────────

    /**
     * @brief 返回前 count 个元素的子视图
     */
    [[nodiscard]] constexpr Span first(size_type count) const noexcept {
        MINE_ASSERT(count <= size_);
        return Span{data_, count};
    }

    /**
     * @brief 返回后 count 个元素的子视图
     */
    [[nodiscard]] constexpr Span last(size_type count) const noexcept {
        MINE_ASSERT(count <= size_);
        return Span{data_ + size_ - count, count};
    }

    /**
     * @brief 返回从 offset 开始、长度为 count 的子视图
     * @param offset 起始元素下标
     * @param count  元素数，默认到末尾
     */
    [[nodiscard]] constexpr Span subspan(
        size_type offset,
        size_type count = static_cast<size_type>(-1)) const noexcept
    {
        MINE_ASSERT(offset <= size_);
        size_type available = size_ - offset;
        size_type actual = (count == static_cast<size_type>(-1) || count > available)
                         ? available : count;
        return Span{data_ + offset, actual};
    }

    // ── 类型转换 ──────────────────────────────────────────────────────────────

    /// 隐式转换为 Span<const T>（允许可变视图赋给只读视图）
    constexpr /*implicit*/ operator Span<const T>() const noexcept  // NOLINT(google-explicit-constructor)
    {
        return Span<const T>{data_, size_};
    }

    /// 以字节视图形式返回（用于序列化/IO）
    [[nodiscard]] Span<const unsigned char> as_bytes() const noexcept {
        return Span<const unsigned char>{
            reinterpret_cast<const unsigned char*>(data_), size_bytes()};
    }

private:
    T*        data_{nullptr};
    size_type size_{0};
};

// ──────────────────────────────────────────────────────────────────────────────
// 推导指南
// ──────────────────────────────────────────────────────────────────────────────

template<typename T, size_t N>
Span(T (&)[N]) -> Span<T>;

template<typename Container>
Span(Container&) -> Span<typename Container::value_type>;

template<typename Container>
Span(const Container&) -> Span<const typename Container::value_type>;

} // namespace mine::core
