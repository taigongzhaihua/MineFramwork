/**
 * @file Dispatcher.h
 * @brief 工作调度器 — 将任务排队到指定线程执行。
 *
 * 设计原则：
 *   - 线程安全：任意线程可 post()，由目标线程调用 dispatch() 处理
 *   - 无虚函数、无异常、无 RTTI
 *   - PIMPL 封装内部队列与同步原语
 *
 * 典型用法：
 * @code
 *   // UI 线程创建 Dispatcher
 *   mine::async::Dispatcher ui_dispatcher;
 *
 *   // 工作线程 post 任务
 *   worker_thread([&ui_dispatcher]() {
 *       auto data = fetch_data();
 *       ui_dispatcher.post([d = std::move(data)]() noexcept {
 *           update_ui(d);
 *       });
 *   });
 *
 *   // UI 线程主循环
 *   while (running) {
 *       ui_dispatcher.dispatch();  // 处理所有排队任务
 *       render_frame();
 *   }
 * @endcode
 *
 * 注意：dispatch() 必须在目标线程中调用。Dispatcher 不创建线程。
 */

#pragma once

#include <cstdint>
#include <mine/async/Api.h>
#include <mine/core/Function.h>
#include <mine/core/Pimpl.h>

// PIMPL 类中的 Pimpl<T> 成员变量会触发 C4251（需要 DLL 接口），
// 因客户端无法直接访问 p_，该警告可安全禁用。
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4251)
#endif

namespace mine::async {

// ──────────────────────────────────────────────────────────────────────────────
// Dispatcher
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 线程安全的任务调度器。
 *
 * 允许任意线程向目标线程（通常是 UI 线程）提交任务，
 * 目标线程通过调用 dispatch() 或 dispatch_one() 处理任务。
 *
 * 特性：
 *   - 多生产者单消费者（MPSC）模型
 *   - post() 可在任意线程调用
 *   - dispatch() 须在目标线程调用
 *   - 支持嵌套 dispatch（dispatch 回调中再次 post 的任务在同次 dispatch 中处理）
 */
class MINE_ASYNC_API Dispatcher {
public:
    /// 构造空的 Dispatcher
    Dispatcher() noexcept;

    /// 移动构造
    Dispatcher(Dispatcher&& other) noexcept;

    /// 移动赋值
    Dispatcher& operator=(Dispatcher&& other) noexcept;

    /// 不可拷贝
    Dispatcher(const Dispatcher&)            = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    /// 析构（丢弃未处理的任务）
    ~Dispatcher() noexcept;

    // ── 任务提交 ──────────────────────────────────────────────────────────────

    /**
     * @brief 向调度器提交任务（线程安全）。
     *
     * 任务将在目标线程的下一次 dispatch() 调用中执行。
     *
     * @param fn 要执行的任务（无参数，无返回值）
     */
    void post(mine::core::Function<void()> fn) noexcept;

    // ── 任务处理 ──────────────────────────────────────────────────────────────

    /**
     * @brief 处理所有排队任务（在目标线程调用）。
     *
     * 持续处理直到队列为空。处理期间新 post 的任务也会在同次
     * dispatch() 调用中被处理。
     *
     * @return 已处理的任务数量
     */
    uint32_t dispatch() noexcept;

    /**
     * @brief 处理单个排队任务（在目标线程调用）。
     *
     * @return true  处理了一个任务
     * @return false 队列为空，无任务可处理
     */
    bool dispatch_one() noexcept;

    // ── 状态查询 ──────────────────────────────────────────────────────────────

    /**
     * @brief 获取当前排队任务数（近似值，线程安全）。
     */
    [[nodiscard]] uint32_t pending_count() const noexcept;

    /**
     * @brief 检查队列是否为空（近似值）。
     */
    [[nodiscard]] bool empty() const noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

} // namespace mine::async

#ifdef _MSC_VER
#    pragma warning(pop)
#endif
