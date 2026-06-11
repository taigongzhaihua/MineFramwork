/**
 * @file Function.h
 * @brief 移动语义函数包装器，SBO 优先 + 堆分配回退，不使用异常。
 *
 * 与 std::function 的差异：
 *   - 仅支持移动语义（不可拷贝），避免不必要的闭包复制
 *   - 所有接口均为 noexcept，符合项目禁用异常的要求
 *   - SBO 大小 32 字节；≤ 32 字节走内联快速路径
 *   - 超出 32 字节自动回退到堆分配（无编译期限制）
 *
 * 典型用途：
 *   - mine.ui.binding 的 getter/setter 闭包
 *   - mine.ui.event 的事件处理回调
 *   - 任意延迟执行的一次性任务
 *
 * 使用示例：
 * @code
 *   int x = 42;
 *   mine::core::Function<int()> fn = [x]() noexcept { return x; };
 *   int result = fn(); // result == 42
 *
 *   // 大捕获自动走堆分配
 *   LargeState s;
 *   mine::core::Function<void()> fn2 = [s]() noexcept { s.process(); };
 * @endcode
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

#include <mine/core/Assert.h>

namespace mine::core {

/// @cond INTERNAL
/// 主模板未定义，只使用特化形式 Function<R(Args...)>
template<class Sig>
class Function;
/// @endcond

/**
 * @brief 移动专用函数包装器，SBO 优先 + 堆分配回退。
 *
 * @tparam R    可调用对象的返回类型
 * @tparam Args 可调用对象的参数类型列表
 *
 * 约束：
 *   1. alignof(F) ≤ alignof(max_align_t)
 *   2. F 必须为 noexcept 移动可构造
 *   3. sizeof(F) ≤ 32 → SBO 快速路径
 *   4. sizeof(F)  > 32 → 自动堆分配（无上限）
 */
template<class R, class... Args>
class Function<R(Args...)> {
public:
    /// SBO 缓冲区大小（字节）：覆盖 4 个指针以内的捕获列表（64 位）
    static constexpr size_t kSBOSize  = 32;
    static constexpr size_t kSBOAlign = alignof(max_align_t);

private:
    // ── 类型操作虚表 ─────────────────────────────────────────────────────

    struct Ops {
        /// 调用：self 指向 Decay 对象（SBO）或 Decay* 首字节（堆）
        R    (*invoke       )(void* self, Args...) noexcept;
        /// 析构：SBO 调用 ~Decay()；堆 delete Decay*
        void (*destroy      )(void* self) noexcept;
        /// 移动：从 src 移动构造到 dst 并析构 src
        void (*move_from    )(void* dst, void* src) noexcept;
    };

    // ── 标记指针：ops_ 最低 bit 区分 SBO (0) / 堆 (1) ──────────────────

    static constexpr uintptr_t kHeapTag = 1u;

    [[nodiscard]] bool is_heap() const noexcept {
        return (reinterpret_cast<uintptr_t>(ops_) & kHeapTag) != 0;
    }

    [[nodiscard]] const Ops* real_ops() const noexcept {
        return reinterpret_cast<const Ops*>(
            reinterpret_cast<uintptr_t>(ops_) & ~kHeapTag);
    }

    static const Ops* tag_heap(const Ops* p) noexcept {
        return reinterpret_cast<const Ops*>(
            reinterpret_cast<uintptr_t>(p) | kHeapTag);
    }

    // ── 数据成员 ─────────────────────────────────────────────────────────

    /// SBO：内联存储对象；堆：首 8B 存储堆指针
    alignas(kSBOAlign) mutable char buf_[kSBOSize]{};
    /// 操作表指针（最低位=1 表示堆模式）；nullptr 表示空
    const Ops* ops_ = nullptr;

public:
    // ── 构造 / 析构 ─────────────────────────────────────────────────────

    Function() noexcept = default;

    /**
     * @brief 从任意可调用对象构造（自动选择 SBO 或堆）。
     */
    template<
        class F,
        class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Function>>>
    Function(F&& f) noexcept {  // NOLINT(google-explicit-constructor)
        using Decay = std::decay_t<F>;

        static_assert(alignof(Decay) <= kSBOAlign,
            "Function: 捕获列表对齐超限");
        static_assert(std::is_nothrow_move_constructible_v<Decay>,
            "Function: 被包装类型须为 noexcept 移动可构造");

        if constexpr (sizeof(Decay) <= kSBOSize) {
            // ── SBO 快速路径 ─────────────────────────────────────────────
            static const Ops ops{
                [](void* self, Args... args) noexcept -> R {
                    return (*static_cast<Decay*>(self))(
                        std::forward<Args>(args)...);
                },
                [](void* self) noexcept {
                    static_cast<Decay*>(self)->~Decay();
                },
                [](void* dst, void* src) noexcept {
                    ::new(dst) Decay(std::move(*static_cast<Decay*>(src)));
                    static_cast<Decay*>(src)->~Decay();
                }
            };
            ::new(buf_) Decay(std::forward<F>(f));
            ops_ = &ops;
        } else {
            // ── 堆分配回退路径（ops 均接收 buf_，内部自行解引用）───────
            static const Ops ops{
                [](void* self, Args... args) noexcept -> R {
                    void* ptr;
                    std::memcpy(&ptr, self, sizeof(ptr));
                    return (*static_cast<Decay*>(ptr))(
                        std::forward<Args>(args)...);
                },
                [](void* self) noexcept {
                    void* ptr;
                    std::memcpy(&ptr, self, sizeof(ptr));
                    delete static_cast<Decay*>(ptr);
                },
                [](void* dst, void* src) noexcept {
                    void* ptr;
                    std::memcpy(&ptr, src, sizeof(ptr));
                    std::memcpy(dst, &ptr, sizeof(ptr));
                    ptr = nullptr;
                    std::memcpy(src, &ptr, sizeof(ptr));
                }
            };
            auto* ptr = new Decay(std::forward<F>(f));
            std::memcpy(buf_, &ptr, sizeof(ptr));
            ops_ = tag_heap(&ops);
        }
    }

    ~Function() noexcept { reset(); }

    /// 移动构造
    Function(Function&& o) noexcept {
        if (o.ops_) {
            if (o.is_heap()) {
                std::memcpy(buf_, o.buf_, sizeof(void*));
                ops_ = o.ops_;
                void* nullp = nullptr;
                std::memcpy(o.buf_, &nullp, sizeof(nullp));
                o.ops_ = nullptr;
            } else {
                o.real_ops()->move_from(buf_, o.buf_);
                ops_   = o.ops_;
                o.ops_ = nullptr;
            }
        }
    }

    /// 移动赋值
    Function& operator=(Function&& o) noexcept {
        if (this != &o) {
            reset();
            new(this) Function(std::move(o));
        }
        return *this;
    }

    Function(const Function&)            = delete;
    Function& operator=(const Function&) = delete;

    // ── 操作 ────────────────────────────────────────────────────────────

    void reset() noexcept {
        if (ops_) {
            real_ops()->destroy(buf_);
            ops_ = nullptr;
        }
    }

    [[nodiscard]] explicit operator bool() const noexcept { return ops_ != nullptr; }

    R operator()(Args... args) const noexcept {
        MINE_ASSERT(ops_ != nullptr);
        return real_ops()->invoke(
            const_cast<char*>(buf_), std::forward<Args>(args)...);
    }
};

} // namespace mine::core
