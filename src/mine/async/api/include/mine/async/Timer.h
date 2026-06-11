/**
 * @file Timer.h
 * @brief 定时器 — 延迟执行与周期性回调。
 *
 * 设计原则：
 *   - 不创建线程；依赖外部驱动调用 tick()
 *   - 使用单调时钟，不受系统时间调整影响
 *   - 回调通过函数指针，无堆分配（内联存储少量定时器）
 *   - 线程不安全（须在调用 tick() 的同一线程使用）
 *
 * 典型用法：
 * @code
 *   mine::async::Timer timer;
 *
 *   // 延迟 1000ms 执行一次
 *   auto h1 = timer.set_timeout([]() noexcept {
 *       do_something();
 *   }, 1000);
 *
 *   // 每 500ms 执行一次
 *   auto h2 = timer.set_interval([]() noexcept {
 *       update_animation();
 *   }, 500);
 *
 *   // 在主循环中驱动
 *   while (running) {
 *       timer.tick();  // 检查并触发到期定时器
 *   }
 * @endcode
 */

#pragma once

#include <cstdint>
#include <mine/async/Api.h>
#include <mine/core/Function.h>

namespace mine::async {

// 前向声明
class Dispatcher;

// ──────────────────────────────────────────────────────────────────────────────
// TimerHandle：定时器句柄
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 定时器唯一标识符。
 *
 * 0 表示无效句柄。通过 Timer::set_timeout() / set_interval() 获取。
 */
using TimerHandle = uint32_t;

/// 无效定时器句柄常量
inline constexpr TimerHandle kInvalidTimerHandle = 0u;

// ──────────────────────────────────────────────────────────────────────────────
// Timer
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 轻量定时器管理器。
 *
 * 在一个线程内管理延迟和周期性定时器。不创建线程，
 * 必须通过 tick() 驱动。所有方法仅可在调用 tick() 的线程中使用。
 *
 * 特性：
 *   - 内联存储（最多同时活跃 kMaxTimers 个定时器，无堆分配）
 *   - 使用单调时钟（不受系统时间调整影响）
 *   - 回调执行期间不可安全修改自身（回调中操作其他定时器是安全的）
 */
class MINE_ASYNC_API Timer {
public:
    /// 最大同时活跃定时器数
    static constexpr uint32_t kMaxTimers = 32u;

    /// 构造空定时器管理器
    Timer() noexcept;

    /// 不可拷贝、不可移动（内部定时器持有可能的回调引用）
    Timer(const Timer&)            = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&)                 = delete;
    Timer& operator=(Timer&&)      = delete;

    /// 析构（丢弃所有活跃定时器，不触发回调）
    ~Timer() noexcept;

    // ── 定时器设置 ────────────────────────────────────────────────────────────

    /**
     * @brief 设置一次性延迟定时器。
     *
     * 在指定延迟后触发回调一次，然后自动清除。
     *
     * @param callback  到期时调用的回调（无参数，无返回值）
     * @param delay_ms  延迟毫秒数（必须 > 0）
     * @return TimerHandle 用于取消定时器的句柄（kInvalidTimerHandle 表示失败）
     */
    [[nodiscard]] TimerHandle set_timeout(
        mine::core::Function<void()> callback,
        uint32_t delay_ms) noexcept;

    /**
     * @brief 设置周期性定时器。
     *
     * 每隔指定间隔触发回调，直到被 clear() 取消。
     *
     * @param callback    到期时调用的回调
     * @param interval_ms 间隔毫秒数（必须 > 0）
     * @return TimerHandle 用于取消定时器的句柄（kInvalidTimerHandle 表示失败）
     */
    [[nodiscard]] TimerHandle set_interval(
        mine::core::Function<void()> callback,
        uint32_t interval_ms) noexcept;

    /**
     * @brief 设置一次性延迟定时器，回调自动投递到指定 Dispatcher。
     *
     * 定时器到期时，回调通过 dispatcher.post() 在目标线程执行。
     * 调用方须保证 Dispatcher 生命周期长于本定时器。
     *
     * @param dispatcher 目标调度器（引用，必须长于定时器生命周期）
     * @param callback   到期时调用的回调
     * @param delay_ms   延迟毫秒数（必须 > 0）
     * @return TimerHandle
     */
    [[nodiscard]] TimerHandle set_timeout_on(
        Dispatcher& dispatcher,
        mine::core::Function<void()> callback,
        uint32_t delay_ms) noexcept;

    /**
     * @brief 设置周期性定时器，回调自动投递到指定 Dispatcher。
     *
     * 同 set_timeout_on，但周期性触发直到被取消。
     */
    [[nodiscard]] TimerHandle set_interval_on(
        Dispatcher& dispatcher,
        mine::core::Function<void()> callback,
        uint32_t interval_ms) noexcept;

    // ── 定时器管理 ────────────────────────────────────────────────────────────

    /**
     * @brief 取消指定定时器。
     *
     * 幂等：无效句柄或已触发的定时器静默忽略。
     *
     * @param handle 要取消的定时器句柄
     */
    void clear(TimerHandle handle) noexcept;

    /**
     * @brief 取消所有定时器。
     */
    void clear_all() noexcept;

    /**
     * @brief 检查指定定时器是否活跃。
     *
     * @param handle 定时器句柄
     * @return true  定时器活跃（尚未触发且未被取消）
     * @return false 定时器无效或已触发/已取消
     */
    [[nodiscard]] bool is_active(TimerHandle handle) const noexcept;

    // ── 驱动 ──────────────────────────────────────────────────────────────────

    /**
     * @brief 推进定时器，触发所有到期回调。
     *
     * 必须在拥有线程中定期调用（通常每帧一次）。
     * 回调在 tick() 调用期间同步执行。
     *
     * @return 本次触发的定时器数量
     */
    uint32_t tick() noexcept;

    /**
     * @brief 获取活跃定时器数量。
     */
    [[nodiscard]] uint32_t active_count() const noexcept;

private:
    struct Entry;
    Entry* entries_;            ///< 定时器条目数组（固定大小，无堆分配）
    uint32_t capacity_;         ///< 数组容量（kMaxTimers）
    uint32_t active_count_;     ///< 当前活跃定时器数
    uint64_t last_tick_time_;   ///< 上次 tick() 的单调时间戳
};

} // namespace mine::async
