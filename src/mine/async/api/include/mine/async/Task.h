/**
 * @file Task.h
 * @brief Task<T> — 异步任务抽象。
 *
 * 提供可组合的异步任务：
 *   - Task<T>::from_value(v)：立即就绪的任务
 *   - Task<T>::from_future(f)：包装 Future<T>
 *   - .then(continuation)：链式回调（结果就绪时调用）
 *   - .get()：阻塞等待结果
 *
 * 设计原则：
 *   - 不依赖 C++ 异常；错误通过 Result<T> 显式传递
 *   - 不依赖 RTTI；无虚函数
 *   - 基于 Future<T> 实现，线程安全
 *   - 不可拷贝，可移动
 *
 * 典型用法：
 * @code
 *   Promise<int> promise;
 *   auto task = Task<int>::from_future(promise.get_future());
 *
 *   task.then([](mine::core::Result<int> r) noexcept {
 *       if (r.ok()) update_ui(r.value());
 *   });
 *
 *   // ... 在其他线程
 *   promise.set_value(42);
 *
 *   // 或直接阻塞等待
 *   auto result = task.get();
 * @endcode
 */

#pragma once

#include <utility>

#include <mine/async/Api.h>
#include <mine/async/Future.h>
#include <mine/core/Function.h>
#include <mine/core/Result.h>
#include <mine/core/Status.h>

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// Task<T>
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 异步任务抽象。
 *
 * Task<T> 代表一个将在未来完成并产生 Result<T> 的异步操作。
 * 基于 Future<T> 实现，可通过 then() 注册完成回调，或通过 get() 阻塞等待结果。
 *
 * @tparam T 任务结果值类型
 */
template<typename T>
class Task {
    static_assert(!std::is_reference_v<T>, "Task<T> 不支持引用类型 T");
    static_assert(!std::is_void_v<T>,      "Task<void> 不支持");

public:
    /// 构造空的 Task（未关联任何异步操作）
    Task() noexcept = default;

    /// 移动构造（默认，Future<T> 已支持移动）
    Task(Task&& other) noexcept = default;

    /// 移动赋值（默认）
    Task& operator=(Task&& other) noexcept = default;

    /// 不可拷贝
    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;

    /// 析构（默认）
    ~Task() noexcept = default;

    // ── 工厂方法 ──────────────────────────────────────────────────────────────

    /**
     * @brief 从已完成的值构造 Task（立即就绪）。
     */
    [[nodiscard]] static Task<T> from_value(T value) noexcept {
        Promise<T> p;
        auto f = p.get_future();
        p.set_value(std::move(value));
        return Task<T>{std::move(f)};
    }

    /**
     * @brief 从错误状态构造 Task（立即就绪）。
     */
    [[nodiscard]] static Task<T> from_error(mine::core::Status error) noexcept {
        Promise<T> p;
        auto f = p.get_future();
        p.set_error(std::move(error));
        return Task<T>{std::move(f)};
    }

    /**
     * @brief 从 Future<T> 构造 Task。
     */
    [[nodiscard]] static Task<T> from_future(Future<T> future) noexcept {
        return Task<T>{std::move(future)};
    }

    // ── 状态查询 ──────────────────────────────────────────────────────────────

    /**
     * @brief 检查是否有效（关联了异步操作）。
     */
    [[nodiscard]] bool valid() const noexcept {
        return future_.valid();
    }

    /**
     * @brief 非阻塞检查任务是否已完成。
     */
    [[nodiscard]] bool is_ready() const noexcept {
        return future_.is_ready();
    }

    // ── 结果获取 ──────────────────────────────────────────────────────────────

    /**
     * @brief 阻塞等待并获取结果。
     */
    [[nodiscard]] mine::core::Result<T> get() noexcept {
        return future_.get();
    }

    /**
     * @brief 阻塞等待任务完成（不获取值）。
     */
    void wait() noexcept {
        future_.wait();
    }

    // ── 组合 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 注册完成回调。
     *
     * 回调签名：void(mine::core::Result<T>) noexcept
     * 若任务已完成，回调立即在当前线程同步调用。
     *
     * @return *this 引用，支持链式调用
     */
    Task<T>& then(mine::core::Function<void(mine::core::Result<T>)> callback) noexcept {
        future_.on_ready(std::move(callback));
        return *this;
    }

private:
    /// 内部构造（由工厂方法使用）
    explicit Task(Future<T> future) noexcept
        : future_(std::move(future))
    {
    }

    Future<T> future_;
};

} // namespace mine::async
