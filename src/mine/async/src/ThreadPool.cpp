/**
 * @file ThreadPool.cpp
 * @brief mine.async::ThreadPool 实现。
 *
 * 实现固定大小的工作线程池，使用单任务队列 + 条件变量调度。
 */

#include <mine/async/ThreadPool.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <mine/core/Assert.h>

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// ThreadPool::Impl
// ──────────────────────────────────────────────────────────────────────────────

struct ThreadPool::Impl {
    /// 工作线程列表
    std::vector<std::thread> workers_;

    /// 任务队列
    std::vector<mine::core::Function<void()>> tasks_;

    /// 互斥锁：保护任务队列（mutable 允许在 const 方法中锁定）
    mutable std::mutex mtx_;

    /// 条件变量：工作线程等待任务
    std::condition_variable cv_;

    /// 运行标志
    std::atomic<bool> running_{true};

    /// 活跃任务计数
    std::atomic<uint32_t> active_count_{0};

    /// 线程数量
    uint32_t thread_count_{0};
};

// ──────────────────────────────────────────────────────────────────────────────
// 构造/析构
// ──────────────────────────────────────────────────────────────────────────────

ThreadPool::ThreadPool(uint32_t num_threads) noexcept
    : p_{mine::core::make_pimpl<Impl>()}
{
    // 确定线程数
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) {
            num_threads = 4;  // 回退值
        }
    }

    p_->thread_count_ = num_threads;

    // 工作线程主循环（lambda 定义在构造器内以访问私有 Impl）
    auto worker_loop = [](Impl* impl) noexcept {
        for (;;) {
            mine::core::Function<void()> task;

            {
                std::unique_lock<std::mutex> lock(impl->mtx_);
                impl->cv_.wait(lock, [impl]() noexcept {
                    return !impl->running_ || !impl->tasks_.empty();
                });

                if (!impl->running_ && impl->tasks_.empty()) {
                    return;
                }

                if (!impl->tasks_.empty()) {
                    task = std::move(impl->tasks_.front());
                    impl->tasks_.erase(impl->tasks_.begin());
                }
            }

            if (task) {
                impl->active_count_.fetch_add(1, std::memory_order_relaxed);
                task();
                impl->active_count_.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    };

    // 创建工作线程
    p_->workers_.reserve(num_threads);
    for (uint32_t i = 0; i < num_threads; ++i) {
        p_->workers_.emplace_back(worker_loop, p_.operator->());
    }
}

ThreadPool::ThreadPool(ThreadPool&& other) noexcept
    : p_{std::move(other.p_)}
{
}

ThreadPool& ThreadPool::operator=(ThreadPool&& other) noexcept {
    if (this != &other) {
        p_ = std::move(other.p_);
    }
    return *this;
}

ThreadPool::~ThreadPool() noexcept {
    if (!p_) return;

    // 通知所有线程退出
    {
        std::lock_guard<std::mutex> lock(p_->mtx_);
        p_->running_ = false;
    }
    p_->cv_.notify_all();

    // 等待所有线程汇合
    for (auto& worker : p_->workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// 任务提交
// ──────────────────────────────────────────────────────────────────────────────

void ThreadPool::enqueue_detached(mine::core::Function<void()> fn) noexcept {
    if (!fn || !p_) return;

    {
        std::lock_guard<std::mutex> lock(p_->mtx_);
        if (!p_->running_) return;
        p_->tasks_.push_back(std::move(fn));
    }
    p_->cv_.notify_one();
}

// ──────────────────────────────────────────────────────────────────────────────
// 状态查询与控制
// ──────────────────────────────────────────────────────────────────────────────

uint32_t ThreadPool::thread_count() const noexcept {
    return p_ ? p_->thread_count_ : 0;
}

uint32_t ThreadPool::pending_count() const noexcept {
    if (!p_) return 0;

    std::lock_guard<std::mutex> lock(p_->mtx_);
    return static_cast<uint32_t>(p_->tasks_.size());
}

void ThreadPool::wait_all() noexcept {
    if (!p_) return;

    // 轮询等待，不持有锁（避免与工作线程互斥锁死锁）
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(p_->mtx_);
            if (p_->tasks_.empty() && p_->active_count_.load(std::memory_order_relaxed) == 0) {
                return;
            }
        }
        // 短暂休眠避免忙等
        std::this_thread::yield();
    }
}

} // namespace mine::async
