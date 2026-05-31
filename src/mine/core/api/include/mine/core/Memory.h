/**
 * @file Memory.h
 * @brief 内存管理工具：MINE_NEW/MINE_DELETE 宏与 OwnedPtr<T> ABI 安全智能指针。
 *
 * 设计原则：
 *   - 所有堆对象创建经由 MINE_NEW，确保走统一分配器路径
 *   - OwnedPtr<T> 将删除器编码为函数指针（非模板参数），
 *     从而保证跨 DLL 边界传递时析构函数调用在正确侧发生
 *   - 禁止在公共 API 中使用 std::unique_ptr（其删除器类型是模板参数，
 *     会导致调用方实例化析构逻辑）
 *
 * 用法：
 * @code
 *   // 创建与释放（手动管理）
 *   auto* btn = MINE_NEW(Button, parent, "OK");
 *   MINE_DELETE(btn);
 *
 *   // RAII 管理
 *   OwnedPtr<Button> btn = make_owned<Button>(parent, "OK");
 *   btn->set_text("确定");
 *   // 超出作用域时自动释放
 *
 *   // 跨 DLL 传递（安全）
 *   OwnedPtr<IDevice> create_device();  // DLL 导出函数
 * @endcode
 */

#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>

namespace mine::core {

// ──────────────────────────────────────────────────────────────────────────────
// 内部工具：使用分配器构造/析构对象
// ──────────────────────────────────────────────────────────────────────────────

namespace detail {

/// 通过默认分配器分配内存，然后就地调用构造函数
template<typename T, typename... Args>
[[nodiscard]] T* construct_new(IAllocator* alloc, Args&&... args) noexcept {
    void* mem = alloc->alloc(sizeof(T), alignof(T));
    MINE_CHECK_MSG(mem != nullptr, "内存分配失败（OOM）");
    return ::new (mem) T(std::forward<Args>(args)...);
}

/// 调用析构函数，然后通过分配器释放内存
template<typename T>
void destroy_delete(IAllocator* alloc, T* ptr) noexcept {
    if (ptr) {
        ptr->~T();
        alloc->dealloc(ptr, sizeof(T), alignof(T));
    }
}

/// OwnedPtr 使用的类型擦除删除器签名
/// 首个参数是对象指针，第二个是分配器指针（编码到闭包函数中）
using DeleterFn = void (*)(void*) noexcept;

/// 为类型 T 生成一个固定的删除器函数（捕获分配器指针）
/// 返回一个可安全跨 DLL 传递的函数指针
template<typename T>
void typed_deleter(void* ptr) noexcept {
    destroy_delete<T>(default_allocator(), static_cast<T*>(ptr));
}

} // namespace detail

// ──────────────────────────────────────────────────────────────────────────────
// OwnedPtr<T>：ABI 安全的独占所有权智能指针
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 独占所有权的智能指针，删除器以函数指针而非模板参数存储。
 *
 * 与 std::unique_ptr<T> 的关键区别：
 *   - 删除器是 `void(*)(void*) noexcept` 函数指针，编码在对象内部
 *   - 无论调用方使用哪个编译器/标准库版本，析构始终在创建侧 DLL 中发生
 *   - 可隐式转换为 OwnedPtr<Base>（协变指针，需基类指针可安全转换）
 *
 * @tparam T 被管理对象的类型（可为抽象基类）
 */
template<typename T>
class OwnedPtr {
public:
    using element_type = T;
    using deleter_type = detail::DeleterFn;

    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 构造空指针
    constexpr OwnedPtr() noexcept = default;
    constexpr OwnedPtr(std::nullptr_t) noexcept {} // NOLINT(google-explicit-constructor)

    /**
     * @brief 从裸指针和删除器构造。
     *
     * 通常不直接使用此构造，而是通过 make_owned<T>() 工厂函数。
     * @param ptr     指向堆上已构造对象的指针
     * @param deleter 析构函数（不得为 nullptr，除非 ptr 也为 nullptr）
     */
    explicit OwnedPtr(T* ptr, detail::DeleterFn deleter) noexcept
        : ptr_{ptr}, deleter_{deleter}
    {
        MINE_ASSERT_MSG(ptr == nullptr || deleter != nullptr,
            "OwnedPtr：非空指针须提供删除器");
    }

    // ── 析构 ──────────────────────────────────────────────────────────────────

    ~OwnedPtr() noexcept { reset(); }

    // ── 移动语义（独占所有权，不可拷贝）──────────────────────────────────────

    OwnedPtr(OwnedPtr&& other) noexcept
        : ptr_{other.ptr_}, deleter_{other.deleter_}
    {
        other.ptr_     = nullptr;
        other.deleter_ = nullptr;
    }

    OwnedPtr& operator=(OwnedPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_           = other.ptr_;
            deleter_       = other.deleter_;
            other.ptr_     = nullptr;
            other.deleter_ = nullptr;
        }
        return *this;
    }

    /// 协变转换构造：OwnedPtr<Derived> → OwnedPtr<Base>（需 Derived* 可安全赋值给 T*）
    template<typename U>
        requires std::is_convertible_v<U*, T*>
    OwnedPtr(OwnedPtr<U>&& other) noexcept
        : ptr_{other.get()}, deleter_{other.get_deleter()}
    {
        (void)other.release();
    }

    /// 协变赋值：OwnedPtr<Derived> → OwnedPtr<Base>
    template<typename U>
        requires std::is_convertible_v<U*, T*>
    OwnedPtr& operator=(OwnedPtr<U>&& other) noexcept {
        reset();
        ptr_     = other.get();
        deleter_ = other.get_deleter();
        (void)other.release();
        return *this;
    }

    OwnedPtr(const OwnedPtr&)            = delete;
    OwnedPtr& operator=(const OwnedPtr&) = delete;

    // ── 赋值 nullptr ──────────────────────────────────────────────────────────

    OwnedPtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    // ── 访问 ──────────────────────────────────────────────────────────────────

    [[nodiscard]] T* get() const noexcept { return ptr_; }

    [[nodiscard]] T* operator->() const noexcept {
        MINE_ASSERT(ptr_ != nullptr);
        return ptr_;
    }

    [[nodiscard]] T& operator*() const noexcept {
        MINE_ASSERT(ptr_ != nullptr);
        return *ptr_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    // ── 所有权转移 ────────────────────────────────────────────────────────────

    /**
     * @brief 放弃所有权，返回裸指针。调用方负责后续析构。
     */
    [[nodiscard]] T* release() noexcept {
        T* tmp     = ptr_;
        ptr_       = nullptr;
        deleter_   = nullptr;
        return tmp;
    }

    /**
     * @brief 重置：析构当前持有的对象（若有），变为空指针。
     */
    void reset() noexcept {
        if (ptr_ && deleter_) {
            deleter_(ptr_);
            ptr_     = nullptr;
            deleter_ = nullptr;
        }
    }

    /**
     * @brief 重置为新的指针+删除器组合。
     */
    void reset(T* ptr, detail::DeleterFn deleter) noexcept {
        reset();
        ptr_     = ptr;
        deleter_ = deleter;
    }

    // ── 获取删除器 ────────────────────────────────────────────────────────────

    [[nodiscard]] detail::DeleterFn get_deleter() const noexcept {
        return deleter_;
    }

private:
    T*               ptr_{nullptr};
    detail::DeleterFn deleter_{nullptr};
};

// ── 比较运算符 ────────────────────────────────────────────────────────────────

template<typename T, typename U>
[[nodiscard]] bool operator==(const OwnedPtr<T>& a, const OwnedPtr<U>& b) noexcept {
    return a.get() == b.get();
}
template<typename T>
[[nodiscard]] bool operator==(const OwnedPtr<T>& p, std::nullptr_t) noexcept {
    return p.get() == nullptr;
}
template<typename T>
[[nodiscard]] bool operator!=(const OwnedPtr<T>& p, std::nullptr_t) noexcept {
    return p.get() != nullptr;
}

// ──────────────────────────────────────────────────────────────────────────────
// 工厂函数
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 通过默认分配器创建 OwnedPtr<T>，转发构造参数。
 *
 * 等价于 `std::make_unique<T>(...)`，但使用统一分配器并返回 OwnedPtr。
 *
 * @tparam T   目标类型（可为抽象基类，但须为具体类才能构造）
 * @tparam Args 构造参数类型包
 */
template<typename T, typename... Args>
    requires std::is_constructible_v<T, Args...>
[[nodiscard]] OwnedPtr<T> make_owned(Args&&... args) noexcept {
    T* ptr = detail::construct_new<T>(default_allocator(), std::forward<Args>(args)...);
    return OwnedPtr<T>{ptr, &detail::typed_deleter<T>};
}

} // namespace mine::core

// ──────────────────────────────────────────────────────────────────────────────
// MINE_NEW / MINE_DELETE：全局分配宏（统一分配器路径）
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @def MINE_NEW(T, ...)
 * @brief 通过默认分配器分配并构造类型 T 的对象，返回 T*。
 *
 * @warning 返回的裸指针所有权须由调用方管理（推荐配合 OwnedPtr 或 MINE_DELETE）。
 *
 * 示例：
 * @code
 *   Button* btn = MINE_NEW(Button, parent, "OK");
 *   MINE_DELETE(btn);
 * @endcode
 */
#define MINE_NEW(T, ...) \
    (::mine::core::detail::construct_new<T>(::mine::core::default_allocator(), ##__VA_ARGS__))

/**
 * @def MINE_DELETE(ptr)
 * @brief 调用析构函数并通过默认分配器释放由 MINE_NEW 分配的对象。
 * @param ptr 类型化指针（须为 MINE_NEW 分配的对象，且不得为 nullptr 以外的野指针）
 */
#define MINE_DELETE(ptr) \
    (::mine::core::detail::destroy_delete(::mine::core::default_allocator(), (ptr)))
