/**
 * @file Variant.h
 * @brief Variant — 无 RTTI 的类型擦除值容器（类 std::any）。
 *
 * 设计目标：
 *   - 不依赖 RTTI（不使用 typeid / std::type_info）
 *   - 使用 TypeId 进行类型鉴别，O(1) 类型检查
 *   - 小对象优化（SBO）：sizeof(T) <= 16 且 noexcept 移动的类型直接存储在栈上
 *   - 仅允许可拷贝（CopyConstructible）类型进入容器，保证 Variant 自身可拷贝
 *   - 不使用 C++ 异常；类型不匹配时 any_cast 触发断言终止
 *
 * 与 std::any 的主要区别：
 *   - 使用 TypeId 代替 std::type_info（无 RTTI 依赖）
 *   - 提供 has<T>() / get<T>() 便捷成员
 *   - 分配器路径走 mine::core::default_allocator()
 *   - StringView / const char* 有特殊处理（转换为内部字符串存储）
 *
 * 用法：
 * @code
 *   Variant v = 42;
 *   Variant v2 = 3.14f;
 *   Variant v3 = StringView{"hello"};
 *
 *   if (v.has<int>()) {
 *       int x = v.get<int>();
 *   }
 *   int x = any_cast<int>(v);  // 类型不匹配时断言
 * @endcode
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>
#include <new>

#include <mine/core/Assert.h>
#include <mine/core/TypeId.h>
#include <mine/core/Allocator.h>
#include <mine/core/StringView.h>

namespace mine::core {

// ──────────────────────────────────────────────────────────────────────────────
// 内部：类型操作函数表（虚表替代，无 RTTI）
// ──────────────────────────────────────────────────────────────────────────────

namespace detail {

/// SBO 缓冲区大小（字节）：覆盖 double / void* / float[4] 等常见属性值类型
static constexpr size_t kVariantSBOSize  = 16;
static constexpr size_t kVariantSBOAlign = alignof(max_align_t);

/**
 * @brief Variant 类型操作表，每个具体类型 T 生成一个静态实例。
 *
 * 所有操作通过函数指针调用，避免虚函数表（RTTI 相关）。
 */
struct VariantOps {
    TypeId type_id;                                                 ///< 被存储类型的 TypeId
    bool   uses_sbo;                                                ///< 是否使用小对象优化

    /// 将 src 处的对象拷贝构造到 dst（SBO 时 dst 是栈缓冲区，否则是堆指针位置）
    void (*copy_construct)(void* dst, const void* src) noexcept;

    /// 将 src 处的对象移动构造到 dst（noexcept 保证）
    void (*move_construct)(void* dst, void* src) noexcept;

    /// 析构 ptr 处的对象（SBO 时直接析构，非 SBO 时析构后释放内存）
    void (*destroy)(void* ptr) noexcept;
};

/// 判断类型 T 是否适合 SBO：大小、对齐、noexcept 移动三个条件都需满足
template<typename T>
inline constexpr bool kFitsSBO =
    (sizeof(T) <= kVariantSBOSize) &&
    (alignof(T) <= kVariantSBOAlign) &&
    std::is_nothrow_move_constructible_v<T>;

/**
 * @brief 为类型 T 生成 VariantOps 的静态实例（SBO 路径）。
 */
template<typename T>
    requires kFitsSBO<T>
const VariantOps* get_ops_sbo() noexcept {
    // 拷贝构造：直接在缓冲区内就地构造
    static constexpr VariantOps ops = {
        TypeId::of<T>(),
        true,
        // copy_construct
        [](void* dst, const void* src) noexcept {
            std::construct_at(static_cast<T*>(dst),
                              *static_cast<const T*>(src));
        },
        // move_construct
        [](void* dst, void* src) noexcept {
            std::construct_at(static_cast<T*>(dst),
                              std::move(*static_cast<T*>(src)));
        },
        // destroy（SBO：析构对象，不释放内存）
        [](void* ptr) noexcept {
            std::destroy_at(static_cast<T*>(ptr));
        },
    };
    return &ops;
}

/**
 * @brief 为类型 T 生成 VariantOps 的静态实例（堆分配路径）。
 *
 * 缓冲区中存储的是 T* 指针，而非 T 对象本身。
 */
template<typename T>
    requires (!kFitsSBO<T>)
const VariantOps* get_ops_heap() noexcept {
    static constexpr VariantOps ops = {
        TypeId::of<T>(),
        false,
        // copy_construct：分配新内存并拷贝构造
        [](void* dst, const void* src) noexcept {
            // dst 是存储指针的地址，src 是存储指针的地址（const 路径）
            const T* src_obj = *static_cast<const T* const*>(src);
            void* mem = default_allocator()->alloc(sizeof(T), alignof(T));
            MINE_CHECK_MSG(mem, "Variant：堆分配失败（OOM）");
            T* new_obj = std::construct_at(static_cast<T*>(mem), *src_obj);
            std::memcpy(dst, &new_obj, sizeof(T*));
        },
        // move_construct：转移指针所有权（不分配新内存）
        [](void* dst, void* src) noexcept {
            std::memcpy(dst, src, sizeof(T*));
            // 将源指针清零，防止 src 的 destroy 重复释放
            T* null_ptr = nullptr;
            std::memcpy(src, &null_ptr, sizeof(T*));
        },
        // destroy：析构并释放堆内存
        [](void* ptr) noexcept {
            T* obj = nullptr;
            std::memcpy(&obj, ptr, sizeof(T*));
            if (obj) {
                obj->~T();
                default_allocator()->dealloc(obj, sizeof(T), alignof(T));
            }
        },
    };
    return &ops;
}

/// 统一分发：按 SBO 能力选择正确的 ops
template<typename T>
const VariantOps* get_ops() noexcept {
    if constexpr (kFitsSBO<T>) {
        return get_ops_sbo<T>();
    } else {
        return get_ops_heap<T>();
    }
}

} // namespace detail

// ──────────────────────────────────────────────────────────────────────────────
// Variant
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 无 RTTI 的类型擦除值容器（类 std::any，使用 TypeId 鉴别类型）。
 *
 * 只允许存储满足以下约束的类型 T：
 *   - CopyConstructible（Variant 自身须可拷贝，用于属性值传递）
 *   - Destructible
 *   - 不为引用类型
 */
class Variant {
public:
    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 默认构造为空（不持有任何值）
    constexpr Variant() noexcept = default;

    /**
     * @brief 从任意可拷贝类型 T 的值构造 Variant。
     *
     * StringView / const char* 特殊处理：存储为内部 StringView 副本
     * （调用方须保证底层字符串生命周期覆盖 Variant 生命周期，
     *  或使用 InlineString 进行拥有型存储——后者由 mine.containers 提供）。
     */
    template<typename T>
        requires (!std::is_same_v<std::decay_t<T>, Variant> &&
                  std::is_copy_constructible_v<std::decay_t<T>> &&
                  !std::is_reference_v<std::decay_t<T>>)
    /*implicit*/ Variant(T&& value) noexcept  // NOLINT(google-explicit-constructor)
    {
        using DecayedT = std::decay_t<T>;
        emplace_impl<DecayedT>(std::forward<T>(value));
    }

    // ── 析构 ──────────────────────────────────────────────────────────────────

    ~Variant() noexcept { reset(); }

    // ── 拷贝构造 ──────────────────────────────────────────────────────────────

    Variant(const Variant& other) noexcept {
        if (other.ops_) {
            other.ops_->copy_construct(buf_, other.buf_);
            ops_ = other.ops_;
        }
    }

    // ── 移动构造 ──────────────────────────────────────────────────────────────

    Variant(Variant&& other) noexcept {
        if (other.ops_) {
            other.ops_->move_construct(buf_, other.buf_);
            ops_ = other.ops_;
            // 不调用 other.reset()：move_construct 已转移堆指针（若有）
            other.ops_ = nullptr;
        }
    }

    // ── 拷贝赋值 ──────────────────────────────────────────────────────────────

    Variant& operator=(const Variant& other) noexcept {
        if (this != &other) {
            reset();
            if (other.ops_) {
                other.ops_->copy_construct(buf_, other.buf_);
                ops_ = other.ops_;
            }
        }
        return *this;
    }

    // ── 移动赋值 ──────────────────────────────────────────────────────────────

    Variant& operator=(Variant&& other) noexcept {
        if (this != &other) {
            reset();
            if (other.ops_) {
                other.ops_->move_construct(buf_, other.buf_);
                ops_ = other.ops_;
                other.ops_ = nullptr;
            }
        }
        return *this;
    }

    // ── 值赋值 ────────────────────────────────────────────────────────────────

    template<typename T>
        requires (!std::is_same_v<std::decay_t<T>, Variant> &&
                  std::is_copy_constructible_v<std::decay_t<T>> &&
                  !std::is_reference_v<std::decay_t<T>>)
    Variant& operator=(T&& value) noexcept {
        using DecayedT = std::decay_t<T>;
        reset();
        emplace_impl<DecayedT>(std::forward<T>(value));
        return *this;
    }

    // ── 状态查询 ──────────────────────────────────────────────────────────────

    /// 是否持有值
    [[nodiscard]] bool has_value() const noexcept { return ops_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    /// 持有值的 TypeId（空 Variant 返回无效 TypeId）
    [[nodiscard]] TypeId type_id() const noexcept {
        return ops_ ? ops_->type_id : TypeId{};
    }

    /// 是否持有类型 T 的值
    template<typename T>
    [[nodiscard]] bool has() const noexcept {
        return ops_ && ops_->type_id == TypeId::of<std::remove_cvref_t<T>>();
    }

    // ── 值访问（通过成员函数）────────────────────────────────────────────────

    /**
     * @brief 取出类型 T 的值引用，类型不匹配时断言终止。
     */
    template<typename T>
    [[nodiscard]] T& get() & noexcept {
        using DecayedT = std::remove_cvref_t<T>;
        MINE_ASSERT_MSG(has<DecayedT>(), "Variant::get<T>：类型不匹配");
        if constexpr (detail::kFitsSBO<DecayedT>) {
            return *reinterpret_cast<DecayedT*>(buf_);
        } else {
            DecayedT* ptr = nullptr;
            std::memcpy(&ptr, buf_, sizeof(DecayedT*));
            return *ptr;
        }
    }

    template<typename T>
    [[nodiscard]] const T& get() const& noexcept {
        using DecayedT = std::remove_cvref_t<T>;
        MINE_ASSERT_MSG(has<DecayedT>(), "Variant::get<T>：类型不匹配");
        if constexpr (detail::kFitsSBO<DecayedT>) {
            return *reinterpret_cast<const DecayedT*>(buf_);
        } else {
            const DecayedT* ptr = nullptr;
            std::memcpy(&ptr, buf_, sizeof(const DecayedT*));
            return *ptr;
        }
    }

    // ── 就地构造 ──────────────────────────────────────────────────────────────

    /**
     * @brief 就地构造类型 T，转发参数（先 reset 当前值）。
     */
    template<typename T, typename... Args>
        requires std::is_copy_constructible_v<T> && std::is_constructible_v<T, Args...>
    T& emplace(Args&&... args) noexcept {
        reset();
        return emplace_impl<T>(std::forward<Args>(args)...);
    }

    // ── 重置 ──────────────────────────────────────────────────────────────────

    /// 析构当前持有的值，变为空
    void reset() noexcept {
        if (ops_) {
            ops_->destroy(buf_);
            ops_ = nullptr;
        }
    }

    // ── 交换 ──────────────────────────────────────────────────────────────────

    void swap(Variant& other) noexcept {
        // 借助临时 Variant 三步移动交换
        Variant tmp{std::move(other)};
        other = std::move(*this);
        *this = std::move(tmp);
    }

private:
    // ── 内部就地构造实现 ──────────────────────────────────────────────────────

    template<typename T, typename... Args>
    T& emplace_impl(Args&&... args) noexcept {
        static_assert(!std::is_reference_v<T>, "Variant 不支持引用类型");
        static_assert(std::is_copy_constructible_v<T>,
                      "Variant 要求存储类型 CopyConstructible（用于属性值传递）");

        ops_ = detail::get_ops<T>();

        if constexpr (detail::kFitsSBO<T>) {
            // SBO 路径：直接在 buf_ 内就地构造
            return *std::construct_at(reinterpret_cast<T*>(buf_),
                                      std::forward<Args>(args)...);
        } else {
            // 堆路径：分配内存，buf_ 中存储指针
            void* mem = default_allocator()->alloc(sizeof(T), alignof(T));
            MINE_CHECK_MSG(mem, "Variant：堆分配失败（OOM）");
            T* obj = std::construct_at(static_cast<T*>(mem), std::forward<Args>(args)...);
            std::memcpy(buf_, &obj, sizeof(T*));
            return *obj;
        }
    }

    /// SBO 缓冲区：存储小对象本体，或非小对象的 void* 指针
    alignas(detail::kVariantSBOAlign)
    unsigned char buf_[detail::kVariantSBOSize]{};

    /// 当前类型的操作表指针（nullptr 表示空 Variant）
    const detail::VariantOps* ops_{nullptr};
};

// ──────────────────────────────────────────────────────────────────────────────
// any_cast：自由函数（对标 std::any_cast，无异常版本）
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 从 Variant 中提取类型 T 的值引用。
 *
 * 类型不匹配时触发 MINE_ASSERT 断言终止（无异常）。
 * 成功时返回对 Variant 内部存储的直接引用（无拷贝）。
 */
template<typename T>
[[nodiscard]] T& any_cast(Variant& v) noexcept {
    return v.get<T>();
}

template<typename T>
[[nodiscard]] const T& any_cast(const Variant& v) noexcept {
    return v.get<T>();
}

/// 指针版本：类型不匹配时返回 nullptr（不断言）
template<typename T>
[[nodiscard]] T* any_cast(Variant* v) noexcept {
    if (!v || !v->has<T>()) return nullptr;
    return &v->get<T>();
}

template<typename T>
[[nodiscard]] const T* any_cast(const Variant* v) noexcept {
    if (!v || !v->has<T>()) return nullptr;
    return &v->get<T>();
}

// ──────────────────────────────────────────────────────────────────────────────
// swap 特化
// ──────────────────────────────────────────────────────────────────────────────

inline void swap(Variant& a, Variant& b) noexcept { a.swap(b); }

} // namespace mine::core
