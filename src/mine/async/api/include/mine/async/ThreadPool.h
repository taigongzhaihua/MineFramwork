/**
 * @file ThreadPool.h
 * @brief 线程池 — 并行任务执行。
 *
 * 设计原则：
 *   - 固定线程数，初始化时创建，析构时自动汇合
 *   - 使用工作队列（work queue）+ 条件变量实现线程唤醒
 *   - 不依赖异常 / RTTI
 *   - PIMPL 封装平台线程句柄
 *
 * 典型用法：
 * @code
 *   mine::async::ThreadPool pool(4);  // 4 个工作线程
 *
 *   auto future = pool.enqueue([]() noexcept -> int {
 *       return heavy_computation();
 *   });
 *
 *   auto result = future.get();  // 阻塞等待结果
 *   pool.wait_all();             // 等待所有任务完成
 * @endcode
 *
 * 线程安全：所有公开方法均为线程安全。
 */

#pragma once

#include <cstdint>
#include <mine/async/Api.h>
#include <mine/async/Future.h>
#include <mine/core/Function.h>
#include <mine/core/Pimpl.h>
#include <mine/core/Result.h>

// PIMPL 类中的 Pimpl<T> 成员变量会触发 C4251（需要 DLL 接口），
// 因客户端无法直接访问 p_，该警告可安全禁用。
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4251)
#endif

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// ThreadPool
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 固定大小的工作线程池。
 *
 * 构造时创建指定数量的工作线程，析构时自动汇合所有线程。
 * 任务通过 enqueue() 提交，返回 Future<T> 用于获取结果。
 *
 * 特性：
 *   - 固定线程数（不动态扩缩）
 *   - 工作窃取（work-stealing）的简化单队列实现
 *   - 线程安全的 enqueue / wait_all
 */
class MINE_ASYNC_API ThreadPool {
public:
    /**
     * @brief 构造线程池。
     *
     * @param num_threads 工作线程数（0 表示使用硬件并发数）
     */
    explicit ThreadPool(uint32_t num_threads = 0) noexcept;

    /// 不可拷贝
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// 移动构造
    ThreadPool(ThreadPool&& other) noexcept;

    /// 移动赋值
    ThreadPool& operator=(ThreadPool&& other) noexcept;

    /**
     * @brief 析构：通知所有线程退出并等待汇合。
     *
     * 析构前未完成的任务将被丢弃（不执行）。若需等待任务完成，
     * 请在析构前调用 wait_all()。
     */
    ~ThreadPool() noexcept;

    // ── 任务提交 ──────────────────────────────────────────────────────────────

    /**
     * @brief 将任务提交到线程池，返回 Future<T> 获取结果。
     *
     * 任务函数在工作线程中执行，结果通过 Future<T> 异步获取。
     *
     * @tparam F 可调用对象类型
     * @param fn 要执行的任务（无参数，返回 T）
     * @return Future<T> 用于获取任务结果
     */
    template<typename F>
    [[nodiscard]] auto enqueue(F&& fn) noexcept -> Future<decltype(fn())>;

    /**
     * @brief 提交无返回值的任务。
     *
     * @param fn 要执行的任务（无参数，无返回值）
     */
    void enqueue_detached(mine::core::Function<void()> fn) noexcept;

    // ── 状态查询与控制 ────────────────────────────────────────────────────────

    /**
     * @brief 获取工作线程数。
     */
    [[nodiscard]] uint32_t thread_count() const noexcept;

    /**
     * @brief 获取当前队列中待处理任务数（近似值）。
     */
    [[nodiscard]] uint32_t pending_count() const noexcept;

    /**
     * @brief 阻塞等待所有已提交任务完成。
     *
     * 注意：wait_all() 期间其他线程仍可提交新任务，wait_all() 不会等待这些任务。
     */
    void wait_all() noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

// ──────────────────────────────────────────────────────────────────────────────
// ThreadPool::enqueue 模板实现
// ──────────────────────────────────────────────────────────────────────────────

template<typename F>
inline auto ThreadPool::enqueue(F&& fn) noexcept -> Future<decltype(fn())> {
    using R = decltype(fn());

    Promise<R> promise;
    auto future = promise.get_future();

    // 将任务包装为 void() 闭包：执行 fn，将结果写入 promise
    // 注意：由于 Function 不可拷贝，此处需要将 fn 移动到堆上
    // 实际实现在 .cpp 中通过类型擦除完成
    enqueue_detached(mine::core::Function<void()>([p = std::move(promise), f = std::forward<F>(fn)]() mutable noexcept {
        // 直接设置结果值（不抛异常）
        p.set_value(f());
    }));

    return future;
}

} // namespace mine::async

#ifdef _MSC_VER
#    pragma warning(pop)
#endif
