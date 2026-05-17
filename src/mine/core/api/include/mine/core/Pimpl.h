/**
 * @file Pimpl.h
 * @brief Pimpl<T> — PIMPL（指向实现的指针）惯用法辅助类。
 *
 * 动机：
 *   跨 DLL 边界的类必须使用 PIMPL 模式，以确保：
 *   1. 公共头不暴露 STL 容器、第三方类型等不稳定的实现细节
 *   2. 修改实现文件不破坏 ABI（调用方无需重新编译）
 *   3. 编译时间减少（包含公共头的编译单元无需看到实现细节）
 *
 * 用法（头文件）：
 * @code
 *   // Visual.h
 *   class MINE_API Visual {
 *   public:
 *       Visual();
 *       ~Visual();
 *       void invalidate();
 *   private:
 *       struct Impl;
 *       mine::core::Pimpl<Impl> p_;
 *   };
 * @endcode
 *
 * 用法（实现文件）：
 * @code
 *   // Visual.cpp
 *   #include <vector>  // 实现细节可自由引入 STL
 *   struct Visual::Impl {
 *       std::vector<Child*> children;
 *       bool dirty{true};
 *   };
 *   Visual::Visual() : p_{mine::core::make_pimpl<Impl>()} {}
 *   Visual::~Visual() = default;  // Pimpl 析构自动处理
 * @endcode
 */

#pragma once

#include <type_traits>
#include <utility>

#include <mine/core/Assert.h>
#include <mine/core/Memory.h>

namespace mine::core {

/**
 * @brief PIMPL 辅助类，持有指向前向声明类型 Impl 的 OwnedPtr。
 *
 * @tparam Impl 实现类型（前向声明即可，无需完整定义）
 *
 * 特性：
 *   - 默认构造为空（需在 .cpp 中通过 make_pimpl<Impl>() 初始化）
 *   - 不可拷贝（实现对象通常不支持拷贝）
 *   - 可移动（通过 OwnedPtr 移动语义）
 *   - operator-> / operator* 提供对实现对象的直接访问
 *   - 析构时自动释放实现对象（需 Impl 在析构点为完整类型）
 */
template<typename Impl>
class Pimpl {
public:
    // ── 构造 ──────────────────────────────────────────────────────────────────

    /// 构造空的 Pimpl（impl() 为 nullptr，访问前须通过 make_pimpl 初始化）
    constexpr Pimpl() noexcept = default;

    /// 从已有 OwnedPtr<Impl> 构造（通常由 make_pimpl 产生）
    explicit Pimpl(OwnedPtr<Impl> ptr) noexcept : ptr_{std::move(ptr)} {}

    // ── 析构（需 Impl 在此调用点为完整类型，否则链接错误）────────────────────

    ~Pimpl() noexcept = default;

    // ── 移动语义 ──────────────────────────────────────────────────────────────

    Pimpl(Pimpl&&) noexcept            = default;
    Pimpl& operator=(Pimpl&&) noexcept = default;

    Pimpl(const Pimpl&)            = delete;
    Pimpl& operator=(const Pimpl&) = delete;

    // ── 访问 ──────────────────────────────────────────────────────────────────

    [[nodiscard]] Impl* operator->() noexcept {
        MINE_ASSERT_MSG(ptr_, "Pimpl 未初始化，请先调用 make_pimpl<Impl>()");
        return ptr_.get();
    }

    [[nodiscard]] const Impl* operator->() const noexcept {
        MINE_ASSERT_MSG(ptr_, "Pimpl 未初始化");
        return ptr_.get();
    }

    [[nodiscard]] Impl& operator*() noexcept {
        MINE_ASSERT(ptr_);
        return *ptr_;
    }

    [[nodiscard]] const Impl& operator*() const noexcept {
        MINE_ASSERT(ptr_);
        return *ptr_;
    }

    [[nodiscard]] Impl* get() noexcept          { return ptr_.get(); }
    [[nodiscard]] const Impl* get() const noexcept { return ptr_.get(); }

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(ptr_);
    }

    /// 释放 Impl，Pimpl 变为空
    void reset() noexcept { ptr_.reset(); }

private:
    OwnedPtr<Impl> ptr_;
};

// ──────────────────────────────────────────────────────────────────────────────
// 工厂函数
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 在堆上构造 Impl 并返回封装它的 Pimpl<Impl>。
 *
 * 须在 Impl 完整定义可见的 .cpp 文件中调用（通常在构造函数实现里）。
 *
 * 示例：
 * @code
 *   Visual::Visual() : p_{mine::core::make_pimpl<Visual::Impl>()} {}
 * @endcode
 */
template<typename Impl, typename... Args>
    requires std::is_constructible_v<Impl, Args...>
[[nodiscard]] Pimpl<Impl> make_pimpl(Args&&... args) noexcept {
    return Pimpl<Impl>{make_owned<Impl>(std::forward<Args>(args)...)};
}

} // namespace mine::core
