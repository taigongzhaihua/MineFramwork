/**
 * @file Result.h
 * @brief Result<T, E> — 携带值或错误的判别联合类型。
 *
 * 设计原则：
 *   - 不使用 C++ 异常；错误通过返回值显式传递
 *   - 不依赖 RTTI
 *   - 标记 [[nodiscard]]，防止调用方忽略返回值
 *   - 成功/失败构造通过 OkTag / ErrTag 标签消除歧义
 *
 * 用法：
 * @code
 *   Result<int> divide(int a, int b) {
 *       if (b == 0)
 *           return {err_tag, Status{StatusCode::InvalidArg}};
 *       return {ok_tag, a / b};
 *   }
 *
 *   auto r = divide(10, 2);
 *   if (r.ok()) {
 *       use(r.value());
 *   }
 * @endcode
 */

#pragma once

#include <type_traits>
#include <utility>
#include <new>

#include <mine/core/Assert.h>
#include <mine/core/Status.h>

namespace mine::core {

// ──────────────────────────────────────────────────────────────────────────────
// 标签类型：消除成功/失败构造的歧义
// ──────────────────────────────────────────────────────────────────────────────

/// 构造成功值时使用的标签
struct OkTag  { explicit OkTag()  = default; };
/// 构造失败值时使用的标签
struct ErrTag { explicit ErrTag() = default; };

/// 全局成功标签常量，用于直接初始化 Result
inline constexpr OkTag  ok_tag{};
/// 全局失败标签常量，用于直接初始化 Result
inline constexpr ErrTag err_tag{};

// ──────────────────────────────────────────────────────────────────────────────
// Result<T, E>
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 携带成功值 T 或错误值 E 的判别联合。
 *
 * @tparam T 成功时持有的值类型，不得为 void 或引用（使用 Status 代替 Result<void>）
 * @tparam E 失败时持有的错误类型，默认为 Status
 *
 * 内存布局：使用无名联合 + bool 判别位，无额外堆分配。
 */
template<typename T, typename E = Status>
class [[nodiscard]] Result {
    static_assert(!std::is_reference_v<T>,  "Result<T> 不支持引用类型 T，请改用指针");
    static_assert(!std::is_void_v<T>,       "Result<void> 无意义，请改用 Status");
    static_assert(!std::is_reference_v<E>,  "Result<T,E> 不支持引用类型 E");

public:
    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 以成功标签 + 转发参数构造，就地初始化 T
    template<typename... Args>
        requires std::is_constructible_v<T, Args...>
    explicit Result(OkTag, Args&&... args) noexcept
        : has_value_{true}
    {
        std::construct_at(std::addressof(data_.value), std::forward<Args>(args)...);
    }

    /// 以失败标签 + 转发参数构造，就地初始化 E
    template<typename... Args>
        requires std::is_constructible_v<E, Args...>
    explicit Result(ErrTag, Args&&... args) noexcept
        : has_value_{false}
    {
        std::construct_at(std::addressof(data_.error), std::forward<Args>(args)...);
    }

    // ── 析构 ──────────────────────────────────────────────────────────────────

    ~Result() noexcept {
        if (has_value_) {
            std::destroy_at(std::addressof(data_.value));
        } else {
            std::destroy_at(std::addressof(data_.error));
        }
    }

    // ── 拷贝构造 ──────────────────────────────────────────────────────────────

    Result(const Result& other) noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E>)
        : has_value_{other.has_value_}
    {
        if (has_value_) {
            std::construct_at(std::addressof(data_.value), other.data_.value);
        } else {
            std::construct_at(std::addressof(data_.error), other.data_.error);
        }
    }

    // ── 移动构造 ──────────────────────────────────────────────────────────────

    Result(Result&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
        : has_value_{other.has_value_}
    {
        if (has_value_) {
            std::construct_at(std::addressof(data_.value), std::move(other.data_.value));
        } else {
            std::construct_at(std::addressof(data_.error), std::move(other.data_.error));
        }
    }

    // ── 赋值 ──────────────────────────────────────────────────────────────────

    Result& operator=(const Result& other) noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E>)
    {
        if (this != &other) {
            // 先析构当前持有的对象
            this->~Result();
            // 然后以拷贝构造语义重建
            has_value_ = other.has_value_;
            if (has_value_) {
                std::construct_at(std::addressof(data_.value), other.data_.value);
            } else {
                std::construct_at(std::addressof(data_.error), other.data_.error);
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
    {
        if (this != &other) {
            this->~Result();
            has_value_ = other.has_value_;
            if (has_value_) {
                std::construct_at(std::addressof(data_.value), std::move(other.data_.value));
            } else {
                std::construct_at(std::addressof(data_.error), std::move(other.data_.error));
            }
        }
        return *this;
    }

    // ── 查询 ──────────────────────────────────────────────────────────────────

    /// 是否持有成功值
    [[nodiscard]] bool ok() const noexcept { return has_value_; }

    /// 显式布尔转换：持有成功值时为 true
    explicit operator bool() const noexcept { return has_value_; }

    // ── 值访问 ────────────────────────────────────────────────────────────────

    /// 获取成功值引用（未成功时触发断言）
    [[nodiscard]] T& value() & noexcept {
        MINE_ASSERT_MSG(has_value_, "在失败的 Result 上调用 value()");
        return data_.value;
    }
    [[nodiscard]] const T& value() const& noexcept {
        MINE_ASSERT_MSG(has_value_, "在失败的 Result 上调用 value()");
        return data_.value;
    }
    [[nodiscard]] T&& value() && noexcept {
        MINE_ASSERT_MSG(has_value_, "在失败的 Result 上调用 value()");
        return std::move(data_.value);
    }

    /// 解引用运算符（等价于 value()）
    [[nodiscard]] T& operator*() & noexcept         { return value(); }
    [[nodiscard]] const T& operator*() const& noexcept { return value(); }
    [[nodiscard]] T* operator->() noexcept          { return &value(); }
    [[nodiscard]] const T* operator->() const noexcept { return &value(); }

    /// 获取错误值引用（成功时触发断言）
    [[nodiscard]] E& error() & noexcept {
        MINE_ASSERT_MSG(!has_value_, "在成功的 Result 上调用 error()");
        return data_.error;
    }
    [[nodiscard]] const E& error() const& noexcept {
        MINE_ASSERT_MSG(!has_value_, "在成功的 Result 上调用 error()");
        return data_.error;
    }

    /// 带默认值的安全取值：若成功则返回值，否则返回 default_val
    template<typename U = T>
        requires std::is_convertible_v<U, T>
    [[nodiscard]] T value_or(U&& default_val) const& noexcept {
        if (has_value_) return data_.value;
        return static_cast<T>(std::forward<U>(default_val));
    }

private:
    // 无名联合：T 与 E 共享同一块存储，由 has_value_ 判别哪个有效
    union Data {
        T value;
        E error;
        /// 联合的构造与析构由 Result 手动管理，此处均为空操作
        Data() noexcept {}
        ~Data() noexcept {}
    } data_;

    bool has_value_{false};
};

// ──────────────────────────────────────────────────────────────────────────────
// 工厂函数
// ──────────────────────────────────────────────────────────────────────────────

/// 构造成功的 Result<T>，就地转发构造参数
template<typename T, typename... Args>
    requires std::is_constructible_v<T, Args...>
[[nodiscard]] Result<T> make_ok(Args&&... args) noexcept {
    return Result<T>{ok_tag, std::forward<Args>(args)...};
}

/// 构造失败的 Result<T, E>
template<typename T, typename E = Status>
[[nodiscard]] Result<T, E> make_err(E error) noexcept {
    return Result<T, E>{err_tag, std::move(error)};
}

/// 以 StatusCode 快速构造失败的 Result<T>
template<typename T>
[[nodiscard]] Result<T> make_err(StatusCode code) noexcept {
    return Result<T>{err_tag, Status{code}};
}

} // namespace mine::core
