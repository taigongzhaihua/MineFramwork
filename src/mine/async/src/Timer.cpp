/**
 * @file Timer.cpp
 * @brief mine.async::Timer 实现。
 *
 * 轻量定时器管理器，内联存储最多 kMaxTimers 个定时器。
 * 使用单调时钟（std::chrono::steady_clock），不受系统时间调整影响。
 * 线程不安全（须在调用 tick() 的同一线程使用）。
 */

#include <mine/async/Timer.h>
#include <mine/async/Dispatcher.h>

#include <algorithm>
#include <chrono>
#include <memory>

#include <mine/core/Assert.h>

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// 内部类型与常量
// ──────────────────────────────────────────────────────────────────────────────

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Ms        = std::chrono::milliseconds;

static uint64_t timepoint_to_ms(TimePoint tp) noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<Ms>(tp.time_since_epoch()).count());
}

// ──────────────────────────────────────────────────────────────────────────────
// Timer::Entry
// ──────────────────────────────────────────────────────────────────────────────

struct Timer::Entry {
    TimerHandle handle     = kInvalidTimerHandle;
    bool        active     = false;
    bool        repeating  = false;
    uint64_t    interval_ms = 0;       // 间隔（毫秒）
    uint64_t    next_fire_ms = 0;      // 下次触发的绝对时间（毫秒）
    mine::core::Function<void()> callback;

    /// 重置条目为空闲状态
    void reset() noexcept {
        handle      = kInvalidTimerHandle;
        active      = false;
        repeating   = false;
        interval_ms = 0;
        next_fire_ms = 0;
        callback    = {};
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 构造/析构
// ──────────────────────────────────────────────────────────────────────────────

Timer::Timer() noexcept
    : entries_(new Entry[kMaxTimers]{})  // 值初始化所有条目
    , capacity_(kMaxTimers)
    , active_count_(0)
    , last_tick_time_(timepoint_to_ms(Clock::now()))
{
}

Timer::~Timer() noexcept {
    // 清空回调（避免析构顺序问题）
    for (uint32_t i = 0; i < capacity_; ++i) {
        entries_[i].callback = {};
    }
    delete[] entries_;
    entries_ = nullptr;
}

// ──────────────────────────────────────────────────────────────────────────────
// 定时器设置
// ──────────────────────────────────────────────────────────────────────────────

TimerHandle Timer::set_timeout(
    mine::core::Function<void()> callback,
    uint32_t delay_ms) noexcept
{
    if (!callback || delay_ms == 0) {
        return kInvalidTimerHandle;
    }

    // 查找空闲槽位
    for (uint32_t i = 0; i < capacity_; ++i) {
        if (!entries_[i].active) {
            static TimerHandle s_next_handle = 1;

            entries_[i].handle       = s_next_handle++;
            entries_[i].active       = true;
            entries_[i].repeating    = false;
            entries_[i].interval_ms  = delay_ms;
            entries_[i].next_fire_ms = timepoint_to_ms(Clock::now()) + delay_ms;
            entries_[i].callback     = std::move(callback);
            ++active_count_;

            // 防止句柄回绕到 0
            if (s_next_handle == 0) {
                s_next_handle = 1;
            }

            return entries_[i].handle;
        }
    }

    // 无空闲槽位
    return kInvalidTimerHandle;
}

TimerHandle Timer::set_interval(
    mine::core::Function<void()> callback,
    uint32_t interval_ms) noexcept
{
    if (!callback || interval_ms == 0) {
        return kInvalidTimerHandle;
    }

    // 查找空闲槽位
    for (uint32_t i = 0; i < capacity_; ++i) {
        if (!entries_[i].active) {
            static TimerHandle s_next_handle = 1;

            entries_[i].handle       = s_next_handle++;
            entries_[i].active       = true;
            entries_[i].repeating    = true;
            entries_[i].interval_ms  = interval_ms;
            entries_[i].next_fire_ms = timepoint_to_ms(Clock::now()) + interval_ms;
            entries_[i].callback     = std::move(callback);
            ++active_count_;

            if (s_next_handle == 0) {
                s_next_handle = 1;
            }

            return entries_[i].handle;
        }
    }

    return kInvalidTimerHandle;
}

// ──────────────────────────────────────────────────────────────────────────────
// Dispatcher 集成
// ──────────────────────────────────────────────────────────────────────────────

namespace {

/// 堆分配的 Dispatcher 回调包装器，避免 Function SBO 溢出
struct DispatcherCallback {
    Dispatcher*                 dispatcher;
    mine::core::Function<void()> callback;

    void fire() noexcept {
        if (dispatcher && callback) {
            dispatcher->post(std::move(callback));
        }
    }
};

} // namespace

TimerHandle Timer::set_timeout_on(
    Dispatcher& dispatcher,
    mine::core::Function<void()> callback,
    uint32_t delay_ms) noexcept
{
    if (!callback) return kInvalidTimerHandle;

    // 堆分配包装器以绕过 Function 的 SBO 限制
    auto* wrapper = new DispatcherCallback{&dispatcher, std::move(callback)};

    return set_timeout(
        mine::core::Function<void()>([wrapper]() noexcept {
            wrapper->fire();
            delete wrapper;
        }),
        delay_ms);
}

TimerHandle Timer::set_interval_on(
    Dispatcher& dispatcher,
    mine::core::Function<void()> callback,
    uint32_t interval_ms) noexcept
{
    if (!callback) return kInvalidTimerHandle;

    // 使用 shared_ptr 共享回调所有权：
    // - 定时器每次触发时通过 shared_ptr 访问原始回调（不消耗）
    // - 每次创建新闭包投递到 Dispatcher（仅捕获 shared_ptr，适配 SBO）
    auto shared_cb = std::make_shared<mine::core::Function<void()>>(std::move(callback));

    return set_interval(
        mine::core::Function<void()>([&dispatcher, shared_cb]() noexcept {
            dispatcher.post(mine::core::Function<void()>([shared_cb]() noexcept {
                if (*shared_cb) {
                    (*shared_cb)();
                }
            }));
        }),
        interval_ms);
}


void Timer::clear(TimerHandle handle) noexcept {
    if (handle == kInvalidTimerHandle) return;

    for (uint32_t i = 0; i < capacity_; ++i) {
        if (entries_[i].active && entries_[i].handle == handle) {
            entries_[i].reset();
            --active_count_;
            return;
        }
    }
}

void Timer::clear_all() noexcept {
    for (uint32_t i = 0; i < capacity_; ++i) {
        entries_[i].reset();
    }
    active_count_ = 0;
}

bool Timer::is_active(TimerHandle handle) const noexcept {
    if (handle == kInvalidTimerHandle) return false;

    for (uint32_t i = 0; i < capacity_; ++i) {
        if (entries_[i].active && entries_[i].handle == handle) {
            return true;
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
// 驱动
// ──────────────────────────────────────────────────────────────────────────────

uint32_t Timer::tick() noexcept {
    // 获取当前时间
    auto now = Clock::now();
    uint64_t now_ms = timepoint_to_ms(now);
    last_tick_time_ = now_ms;

    uint32_t fired_count = 0;

    // 触发到期定时器
    for (uint32_t i = 0; i < capacity_; ++i) {
        if (!entries_[i].active) continue;
        if (now_ms < entries_[i].next_fire_ms) continue;

        if (entries_[i].repeating) {
            // 周期性定时器：先取出回调，更新下次触发时间，再触发回调
            auto cb = std::move(entries_[i].callback);
            entries_[i].next_fire_ms = now_ms + entries_[i].interval_ms;

            if (cb) {
                cb();
                ++fired_count;
            }

            // 将回调移回条目
            entries_[i].callback = std::move(cb);
        } else {
            // 一次性定时器：取出回调，清理条目，再触发
            auto cb = std::move(entries_[i].callback);
            entries_[i].reset();
            --active_count_;

            if (cb) {
                cb();
                ++fired_count;
            }
        }
    }

    return fired_count;
}

uint32_t Timer::active_count() const noexcept {
    return active_count_;
}

} // namespace mine::async
