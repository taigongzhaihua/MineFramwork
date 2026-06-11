/**
 * @file Dispatcher.cpp
 * @brief mine.async::Dispatcher 实现。
 *
 * 内部使用 mutex + condition_variable + 双队列交换实现 MPSC 调度器。
 */

#include <mine/async/Dispatcher.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// Dispatcher::Impl
// ──────────────────────────────────────────────────────────────────────────────

struct Dispatcher::Impl {
    /// 互斥锁：保护入队操作（mutable 允许在 const 方法中锁定）
    mutable std::mutex mtx;

    /// 条件变量：dispatch() 可等待新任务
    std::condition_variable cv;

    /// 入队队列（生产者写入，dispatch 时交换到处理队列）
    std::vector<mine::core::Function<void()>> incoming_;

    /// 处理队列（dispatch 时使用，不需要锁）
    std::vector<mine::core::Function<void()>> processing_;

    /// 销毁标志
    bool shutdown_{false};
};

// ──────────────────────────────────────────────────────────────────────────────
// 构造/析构
// ──────────────────────────────────────────────────────────────────────────────

Dispatcher::Dispatcher() noexcept
    : p_{mine::core::make_pimpl<Impl>()}
{
}

Dispatcher::Dispatcher(Dispatcher&& other) noexcept
    : p_{std::move(other.p_)}
{
}

Dispatcher& Dispatcher::operator=(Dispatcher&& other) noexcept {
    if (this != &other) {
        p_ = std::move(other.p_);
    }
    return *this;
}

Dispatcher::~Dispatcher() noexcept {
    // 标记关闭，丢弃未处理的任务
    if (p_) {
        std::lock_guard<std::mutex> lock(p_->mtx);
        p_->shutdown_ = true;
        p_->incoming_.clear();
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// 任务提交
// ──────────────────────────────────────────────────────────────────────────────

void Dispatcher::post(mine::core::Function<void()> fn) noexcept {
    if (!fn || !p_) return;

    {
        std::lock_guard<std::mutex> lock(p_->mtx);
        if (p_->shutdown_) return;
        p_->incoming_.push_back(std::move(fn));
    }
    p_->cv.notify_one();
}

// ──────────────────────────────────────────────────────────────────────────────
// 任务处理
// ──────────────────────────────────────────────────────────────────────────────

uint32_t Dispatcher::dispatch() noexcept {
    if (!p_) return 0;

    uint32_t count = 0;

    // 持续处理直到队列为空
    for (;;) {
        // 交换队列
        {
            std::lock_guard<std::mutex> lock(p_->mtx);
            if (p_->incoming_.empty()) {
                break;  // 无更多任务
            }
            p_->processing_.swap(p_->incoming_);
        }

        // 处理所有已交换的任务
        for (auto& fn : p_->processing_) {
            if (fn) {
                fn();
                ++count;
            }
        }
        p_->processing_.clear();
    }

    return count;
}

bool Dispatcher::dispatch_one() noexcept {
    if (!p_) return false;

    mine::core::Function<void()> fn;

    {
        std::lock_guard<std::mutex> lock(p_->mtx);
        if (p_->incoming_.empty()) {
            return false;
        }
        fn = std::move(p_->incoming_.front());
        p_->incoming_.erase(p_->incoming_.begin());
    }

    if (fn) {
        fn();
        return true;
    }

    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
// 状态查询
// ──────────────────────────────────────────────────────────────────────────────

uint32_t Dispatcher::pending_count() const noexcept {
    if (!p_) return 0;

    std::lock_guard<std::mutex> lock(p_->mtx);
    return static_cast<uint32_t>(p_->incoming_.size());
}

bool Dispatcher::empty() const noexcept {
    return pending_count() == 0;
}

} // namespace mine::async
