/**
 * @file Function.h
 * @brief 移动语义函数包装器，带小缓冲区优化（SBO），不使用异常。
 *
 * 与 std::function 的差异：
 *   - 仅支持移动语义（不可拷贝），避免不必要的闭包复制
 *   - 所有接口均为 noexcept，符合项目禁用异常的要求
 *   - SBO 大小固定为 32 字节；超出时触发编译期 static_assert
 *   - 无堆分配，捕获列表必须 ≤ 32 字节且 noexcept 移动构造
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
 * @endcode
 */

#pragma once

#include <cstddef>
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
 * @brief 移动专用 SBO 函数包装器。
 *
 * @tparam R    可调用对象的返回类型
 * @tparam Args 可调用对象的参数类型列表
 *
 * 约束（编译期检查）：
 *   1. 被包装类型 F 的 sizeof(F) ≤ 32（超出则 static_assert 失败）
 *   2. alignof(F) ≤ alignof(max_align_t)
 *   3. F 必须为 noexcept 移动可构造（确保包装操作不抛异常）
 */
template<class R, class... Args>
class Function<R(Args...)> {
public:
    /// SBO 缓冲区大小（字节）：覆盖 4 个指针以内的捕获列表（64 位）
    static constexpr size_t kSBOSize  = 32;
    static constexpr size_t kSBOAlign = alignof(max_align_t);

private:
    /**
     * @brief 类型操作虚表（函数指针表，替代 virtual dispatch，节省一个指针）。
     *
     * 每个被包装类型 Decay 对应一个静态实例，其地址即作为类型标识。
     */
    struct Ops {
        /// 调用 self 指针处的可调用对象，转发所有参数
        R    (*invoke    )(void* self, Args...) noexcept;
        /// 析构 self 指针处的对象（不释放内存，SBO 时对象内联存储）
        void (*destroy   )(void* self) noexcept;
        /// 从 src 移动构造到 dst，并析构 src（用于 Function 的移动操作）
        void (*move_from )(void* dst, void* src) noexcept;
    };

    /// SBO 缓冲区：内联存储被包装对象
    alignas(kSBOAlign) mutable char buf_[kSBOSize]{};
    /// 当前存储类型的操作表指针；nullptr 表示空（未持有对象）
    const Ops* ops_ = nullptr;

public:
    // ── 构造 / 析构 ─────────────────────────────────────────────────────────

    /// 默认构造：空状态，operator bool() 返回 false
    Function() noexcept = default;

    /**
     * @brief 从任意可调用对象构造。
     *
     * 通过 SFINAE 排除 Function 自身，避免与移动构造冲突。
     *
     * @tparam F 可调用类型（lambda、函数指针等）
     * @param f  可调用对象（右值引用，将被移动/拷贝到 SBO 缓冲区）
     */
    template<
        class F,
        class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Function>>>
    Function(F&& f) noexcept {  // NOLINT(google-explicit-constructor)
        using Decay = std::decay_t<F>;

        // 编译期约束检查
        static_assert(sizeof(Decay) <= kSBOSize,
            "Function: 捕获列表过大，超过 32 字节 SBO 上限，请减少捕获变量数量");
        static_assert(alignof(Decay) <= kSBOAlign,
            "Function: 捕获列表的对齐要求超过 alignof(max_align_t)");
        static_assert(std::is_nothrow_move_constructible_v<Decay>,
            "Function: 被包装类型须为 noexcept 移动可构造，以确保 Function 整体 noexcept");

        // 为该 Decay 类型生成静态虚表（一个 Decay 类型仅有一份实例）
        static const Ops ops{
            // invoke：将缓冲区解释为 Decay 并转发调用
            [](void* self, Args... args) noexcept -> R {
                return (*static_cast<Decay*>(self))(std::forward<Args>(args)...);
            },
            // destroy：调用 Decay 析构函数（不释放内存，内联存储）
            [](void* self) noexcept {
                static_cast<Decay*>(self)->~Decay();
            },
            // move_from：从 src 移动构造到 dst，然后析构 src
            [](void* dst, void* src) noexcept {
                ::new(dst) Decay(std::move(*static_cast<Decay*>(src)));
                static_cast<Decay*>(src)->~Decay();
            }
        };

        // 将可调用对象移动/拷贝到 SBO 缓冲区
        ::new(buf_) Decay(std::forward<F>(f));
        ops_ = &ops;
    }

    ~Function() noexcept { reset(); }

    /// 移动构造：转移被包装对象的所有权，源变为空状态
    Function(Function&& o) noexcept {
        if (o.ops_) {
            o.ops_->move_from(buf_, o.buf_);
            ops_   = o.ops_;
            o.ops_ = nullptr;
        }
    }

    /// 移动赋值：先销毁当前对象，再转移所有权
    Function& operator=(Function&& o) noexcept {
        if (this != &o) {
            reset();
            new(this) Function(std::move(o));
        }
        return *this;
    }

    Function(const Function&)            = delete;
    Function& operator=(const Function&) = delete;

    // ── 操作 ────────────────────────────────────────────────────────────────

    /**
     * @brief 清空包装对象，析构被包装类型，恢复为空状态。
     */
    void reset() noexcept {
        if (ops_) {
            ops_->destroy(buf_);
            ops_ = nullptr;
        }
    }

    /**
     * @brief 检查是否持有有效的可调用对象。
     *
     * @return true  已持有可调用对象，可安全调用 operator()
     * @return false 空状态，调用 operator() 会触发断言
     */
    [[nodiscard]] explicit operator bool() const noexcept { return ops_ != nullptr; }

    /**
     * @brief 调用被包装的可调用对象。
     *
     * 空状态下调用触发 MINE_ASSERT。
     *
     * @param args 转发给被包装对象的参数
     * @return 被包装对象的返回值
     */
    R operator()(Args... args) const noexcept {
        MINE_ASSERT(ops_ != nullptr);
        return ops_->invoke(buf_, std::forward<Args>(args)...);
    }
};

} // namespace mine::core
