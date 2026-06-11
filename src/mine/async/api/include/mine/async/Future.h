/**
 * @file Future.h
 * @brief Future<T> 与 Promise<T> — 异步结果传递原语。
 *
 * 设计原则：
 *   - 不依赖 C++ 异常；错误通过 Result<T> 显式传递
 *   - 不依赖 RTTI；无虚函数
 *   - 线程安全：内部使用引用计数共享状态 + mutex/condition_variable
 *   - 不可拷贝，可移动
 *
 * 典型用法：
 * @code
 *   // 生产者端
 *   mine::async::Promise<int> promise;
 *   auto future = promise.get_future();
 *   std::thread([p = std::move(promise)]() mutable {
 *       p.set_value(42);
 *   }).detach();
 *
 *   // 消费者端
 *   auto result = future.get();  // 阻塞等待，返回 Result<int>
 *   if (result.ok()) {
 *       use(result.value());
 *   }
 * @endcode
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

#include <mine/async/Api.h>
#include <mine/core/Assert.h>
#include <mine/core/Function.h>
#include <mine/core/Result.h>
#include <mine/core/Status.h>

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// 内部：Future/Promise 共享状态
// ──────────────────────────────────────────────────────────────────────────────

namespace detail {

/**
 * @brief Future/Promise 之间共享的控制块。
 *
 * 使用引用计数管理生命周期。同时被 Promise（写入端）和 Future（读取端）引用。
 * 最后一个引用释放时自动销毁。
 *
 * @tparam T 异步结果的值类型
 */
template<typename T>
struct FutureSharedState {
    /// 引用计数（构造时 = 2，Promise 和 Future 各持 1）
    std::atomic<int> ref_count{2};

    /// 互斥锁：保护 ready/broken 标志及值的读写
    std::mutex mtx;

    /// 条件变量：用于阻塞等待
    std::condition_variable cv;

    /// 结果是否就绪（set_value 或 set_error 已调用）
    bool ready{false};

    /// Promise 是否在未设置值的情况下析构
    bool broken{false};

    /// 是否为成功值（valid 仅当 ready && !broken 时有效）
    bool has_value{false};

    /// 存储：成功值 T 或错误 Status（使用 union 节省空间）
    union {
        T            value;
        mine::core::Status error;
    } storage{};

    /// 析构：若 ready && has_value，销毁 T；若 ready && !has_value，销毁 Status
    ~FutureSharedState() noexcept {
        if (ready && !broken) {
            if (has_value) {
                std::destroy_at(&storage.value);
            } else {
                std::destroy_at(&storage.error);
            }
        }
    }

    /// 增加引用计数
    void add_ref() noexcept {
        ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    /// 减少引用计数，若归零则 delete this
    void release() noexcept {
        if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }
};

} // namespace detail

// ──────────────────────────────────────────────────────────────────────────────
// 前向声明
// ──────────────────────────────────────────────────────────────────────────────

template<typename T>
class Future;

// ──────────────────────────────────────────────────────────────────────────────
// Promise<T>
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 异步结果的写入端（生产者）。
 *
 * 每个 Promise 关联唯一一个 Future。调用 set_value() 或 set_error() 后，
 * 关联的 Future 变为就绪状态。
 *
 * @tparam T 异步结果的值类型（不可为引用或 void）
 */
template<typename T>
class Promise {
    static_assert(!std::is_reference_v<T>, "Promise<T> 不支持引用类型 T");
    static_assert(!std::is_void_v<T>,      "Promise<void> 不支持，请使用 Promise<Empty>");

public:
    /// 构造一个未完成的 Promise
    Promise() noexcept
        : state_(new detail::FutureSharedState<T>())
    {
    }

    /// 移动构造
    Promise(Promise&& other) noexcept
        : state_(other.state_)
    {
        other.state_ = nullptr;
    }

    /// 移动赋值
    Promise& operator=(Promise&& other) noexcept {
        if (this != &other) {
            if (state_) {
                set_broken();
                state_->release();
            }
            state_ = other.state_;
            other.state_ = nullptr;
        }
        return *this;
    }

    /// 不可拷贝
    Promise(const Promise&)            = delete;
    Promise& operator=(const Promise&) = delete;

    /**
     * @brief 析构：若未设置值/错误，自动以 kBrokenPromise 错误完成关联的 Future。
     */
    ~Promise() noexcept {
        if (state_) {
            set_broken();
            state_->release();
        }
    }

    // ── 结果设置 ──────────────────────────────────────────────────────────────

    /**
     * @brief 设置成功值，唤醒所有等待者。
     *
     * 幂等：仅首次调用生效。
     */
    void set_value(T value) noexcept {
        if (!state_) return;

        {
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->ready) return;
            state_->ready = true;
            state_->has_value = true;
            std::construct_at(&state_->storage.value, std::move(value));
        }
        state_->cv.notify_all();
    }

    /**
     * @brief 设置错误状态，唤醒所有等待者。
     *
     * 幂等：仅首次调用生效。
     */
    void set_error(mine::core::Status error) noexcept {
        if (!state_) return;

        {
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->ready) return;
            state_->ready = true;
            state_->has_value = false;
            std::construct_at(&state_->storage.error, std::move(error));
        }
        state_->cv.notify_all();
    }

    // ── Future 访问 ───────────────────────────────────────────────────────────

    /**
     * @brief 获取关联的 Future<T>。
     *
     * 每个 Promise 仅可获取一次 Future；重复调用返回空 Future。
     */
    [[nodiscard]] Future<T> get_future() noexcept;

private:
    friend class Future<T>;

    /// 在未设置值/错误时标记为 broken promise
    void set_broken() noexcept {
        if (!state_) return;

        {
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->ready) return;
            state_->ready = true;
            state_->broken = true;
        }
        state_->cv.notify_all();
    }

    detail::FutureSharedState<T>* state_ = nullptr;
    bool future_obtained_ = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// Future<T>
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 异步结果的读取端（消费者）。
 *
 * 通过 Promise<T>::get_future() 获取。可阻塞等待结果或注册回调。
 *
 * @tparam T 异步结果的值类型
 */
template<typename T>
class Future {
    static_assert(!std::is_reference_v<T>, "Future<T> 不支持引用类型 T");
    static_assert(!std::is_void_v<T>,      "Future<void> 不支持");

    friend class Promise<T>;

public:
    /// 构造空 Future（未关联任何 Promise）
    Future() noexcept = default;

    /// 移动构造
    Future(Future&& other) noexcept
        : state_(other.state_)
    {
        other.state_ = nullptr;
    }

    /// 移动赋值
    Future& operator=(Future&& other) noexcept {
        if (this != &other) {
            if (state_) {
                state_->release();
            }
            state_ = other.state_;
            other.state_ = nullptr;
        }
        return *this;
    }

    /// 不可拷贝
    Future(const Future&)            = delete;
    Future& operator=(const Future&) = delete;

    /// 析构
    ~Future() noexcept {
        if (state_) {
            state_->release();
        }
    }

    // ── 状态查询 ──────────────────────────────────────────────────────────────

    /**
     * @brief 检查 Future 是否有效（关联了 Promise）。
     */
    [[nodiscard]] bool valid() const noexcept {
        return state_ != nullptr;
    }

    /**
     * @brief 非阻塞检查结果是否就绪。
     */
    [[nodiscard]] bool is_ready() const noexcept {
        if (!state_) return false;
        std::lock_guard<std::mutex> lock(state_->mtx);
        return state_->ready;
    }

    // ── 结果获取 ──────────────────────────────────────────────────────────────

    /**
     * @brief 阻塞等待并获取结果。
     *
     * @return Result<T> 成功值或错误状态（含 broken promise）
     */
    [[nodiscard]] mine::core::Result<T> get() noexcept {
        if (!state_) {
            return mine::core::Result<T>{mine::core::err_tag,
                                         mine::core::Status{mine::core::StatusCode::InvalidState}};
        }

        {
            std::unique_lock<std::mutex> lock(state_->mtx);
            state_->cv.wait(lock, [this]() noexcept { return state_->ready; });
        }

        return take_result();
    }

    /**
     * @brief 阻塞等待结果就绪（不获取值）。
     */
    void wait() noexcept {
        if (!state_) return;

        std::unique_lock<std::mutex> lock(state_->mtx);
        state_->cv.wait(lock, [this]() noexcept { return state_->ready; });
    }

    // ── 回调注册 ──────────────────────────────────────────────────────────────

    /**
     * @brief 注册结果就绪回调。
     *
     * 若结果已就绪，回调立即在当前线程同步调用。
     * 回调签名：void(mine::core::Result<T>) noexcept
     */
    void on_ready(mine::core::Function<void(mine::core::Result<T>)> callback) noexcept {
        if (!state_ || !callback) return;

        {
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->ready) {
                auto result = extract_result();
                callback(std::move(result));
                return;
            }
        }

        // 慢路径：阻塞等待后回调
        auto result = get();
        callback(std::move(result));
    }

private:
    /// 由 Promise::get_future() 调用
    explicit Future(detail::FutureSharedState<T>* state) noexcept
        : state_(state)
    {
        if (state_) {
            state_->add_ref();
        }
    }

    /// 从共享状态中取出结果（必须在 ready 状态下调用）
    mine::core::Result<T> take_result() noexcept {
        MINE_ASSERT(state_ && state_->ready);

        if (state_->broken) {
            return mine::core::Result<T>{mine::core::err_tag,
                    mine::core::Status{mine::core::StatusCode::Unknown}};
        }

        if (state_->has_value) {
            T val = std::move(state_->storage.value);
            return mine::core::Result<T>{mine::core::ok_tag, std::move(val)};
        } else {
            mine::core::Status err = state_->storage.error;
            return mine::core::Result<T>{mine::core::err_tag, std::move(err)};
        }
    }

    /// 从共享状态中提取结果（拷贝，不修改状态，用于回调场景）
    mine::core::Result<T> extract_result() const noexcept {
        MINE_ASSERT(state_ && state_->ready);

        if (state_->broken) {
            return mine::core::Result<T>{mine::core::err_tag,
                    mine::core::Status{mine::core::StatusCode::Unknown}};
        }

        if (state_->has_value) {
            return mine::core::Result<T>{mine::core::ok_tag, state_->storage.value};
        } else {
            return mine::core::Result<T>{mine::core::err_tag, state_->storage.error};
        }
    }

    detail::FutureSharedState<T>* state_ = nullptr;
};

// ──────────────────────────────────────────────────────────────────────────────
// Promise<T>::get_future() 实现（须在 Future<T> 完整定义后）
// ──────────────────────────────────────────────────────────────────────────────

template<typename T>
inline Future<T> Promise<T>::get_future() noexcept {
    if (!state_ || future_obtained_) {
        return Future<T>{};
    }

    future_obtained_ = true;
    // Promise 和 Future 共享同一个 SharedState（引用计数管理生命周期）
    return Future<T>{state_};
}

} // namespace mine::async
