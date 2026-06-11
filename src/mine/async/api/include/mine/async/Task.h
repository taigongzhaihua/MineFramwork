/**
 * @file Task.h
 * @brief Task<T> — 异步任务抽象，支持 C++20 协程。
 *
 * 提供可组合的异步任务：
 *   - Task<T>::from_value(v)：立即就绪的任务
 *   - Task<T>::from_future(f)：包装 Future<T>
 *   - .then(continuation)：链式回调（结果就绪时调用）
 *   - .get()：阻塞等待结果
 *   - co_return / co_await：C++20 协程支持
 *
 * 设计原则：
 *   - 不依赖 C++ 异常；错误通过 Result<T> 显式传递
 *   - 不依赖 RTTI；无虚函数
 *   - 基于 Future<T> 实现，线程安全
 *   - 不可拷贝，可移动
 *
 * 协程用法：
 * @code
 *   // 声明一个异步协程
 *   mine::async::Task<int> compute_async() {
 *       co_return 42;
 *   }
 *
 *   // 等待另一个协程
 *   mine::async::Task<int> caller() {
 *       auto result = co_await compute_async();  // result 类型为 Result<int>
 *       if (result.ok()) {
 *           co_return result.value() * 2;
 *       }
 *       co_return 0;
 *   }
 *
 *   // 在非协程上下文中使用
 *   auto task = compute_async();
 *   task.then([](mine::core::Result<int> r) noexcept { ... });
 * @endcode
 */

#pragma once

#include <coroutine>
#include <utility>

#include <mine/async/Api.h>
#include <mine/async/Future.h>
#include <mine/core/Assert.h>
#include <mine/core/Function.h>
#include <mine/core/Result.h>
#include <mine/core/Status.h>

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// Task<T>
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 异步任务抽象，C++20 协程返回类型。
 *
 * Task<T> 同时是一个合法的 C++20 协程返回类型（定义了 promise_type）
 * 和一个可被 co_await 的 Awaitable 类型。
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

    // ──────────────────────────────────────────────────────────────────────────
    // C++20 协程支持：promise_type
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief C++20 协程 promise 类型。
     *
     * 编译器在遇到 co_return / co_await 时自动生成此类实例，
     * 管理协程状态并桥接 Future<T>。
     */
    class promise_type {
    public:
        /// 创建 Task 返回对象（编译器调用）
        [[nodiscard]] Task<T> get_return_object() noexcept {
            return Task<T>{promise_.get_future()};
        }

        /// 协程启动时立即执行（不挂起）
        [[nodiscard]] std::suspend_never initial_suspend() noexcept {
            return {};
        }

        /// 协程结束时立即销毁（不挂起）
        [[nodiscard]] std::suspend_never final_suspend() noexcept {
            return {};
        }

        /// co_return value; → 设置成功值
        template<typename U>
            requires std::is_constructible_v<T, U>
        void return_value(U&& value) noexcept {
            promise_.set_value(T{std::forward<U>(value)});
        }

        /// 异常路径（项目禁用异常，此路径不应到达）
        void unhandled_exception() noexcept {
            MINE_ASSERT_MSG(false, "协程中不应抛出异常（项目禁用异常）");
            promise_.set_error(mine::core::Status{mine::core::StatusCode::Unknown});
        }

    private:
        Promise<T> promise_;
    };

    // ──────────────────────────────────────────────────────────────────────────
    // C++20 协程支持：Awaitable（使 Task 可被 co_await）
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief co_await task 返回的 Awaitable 类型。
     *
     * co_await 一个 Task<T> 时，等待其 Future<T> 就绪，然后返回 Result<T>。
     */
    class Awaitable {
    public:
        explicit Awaitable(Future<T> future) noexcept
            : future_(std::move(future))
        {
        }

        /// 检查是否已就绪（无需挂起）
        [[nodiscard]] bool await_ready() const noexcept {
            return future_.is_ready();
        }

        /// 挂起协程，注册恢复回调
        void await_suspend(std::coroutine_handle<> h) noexcept {
            future_.on_ready(mine::core::Function<void(mine::core::Result<T>)>(
                [h](mine::core::Result<T> /*unused*/) mutable noexcept {
                    h.resume();
                }));
        }

        /// 恢复协程，返回结果
        [[nodiscard]] mine::core::Result<T> await_resume() noexcept {
            return future_.get();
        }

    private:
        Future<T> future_;
    };

    /**
     * @brief 使 Task<T> 可被 co_await。
     *
     * co_await task 返回 Result<T>，调用方需显式检查成功/失败。
     *
     * @return Awaitable 对象
     */
    [[nodiscard]] Awaitable operator co_await() noexcept {
        return Awaitable{std::move(future_)};
    }

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
    /// 内部构造（由工厂方法和 promise_type 使用）
    explicit Task(Future<T> future) noexcept
        : future_(std::move(future))
    {
    }

    Future<T> future_;
};

} // namespace mine::async
