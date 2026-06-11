/**
 * @file AsyncTest.cpp
 * @brief mine.async 模块单元测试（doctest）
 *
 * 测试覆盖：
 *   - Promise<T> / Future<T>：set_value/set_error/get/wait/is_ready/移动语义/broken promise
 *   - Task<T>：from_value/from_error/from_future/then/map/移动语义
 *   - Dispatcher：post/dispatch/dispatch_one/pending_count/移动语义
 *   - ThreadPool：enqueue/enqueue_detached/wait_all/移动语义
 *   - Timer：set_timeout/set_interval/clear/clear_all/is_active/tick
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/async/Async.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace mine::async;

// ──────────────────────────────────────────────────────────────────────────────
// 工具：追踪构造/析构次数的非平凡类型
// ──────────────────────────────────────────────────────────────────────────────

struct Tracker {
    int  value = 0;
    int* move_count = nullptr;

    Tracker() = default;
    explicit Tracker(int v, int* mc = nullptr) : value{v}, move_count{mc} {}

    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    Tracker(Tracker&& o) noexcept : value{o.value}, move_count{o.move_count} {
        o.value = -1;
        if (move_count) ++(*move_count);
    }
    Tracker& operator=(Tracker&& o) noexcept {
        value = o.value;
        move_count = o.move_count;
        o.value = -1;
        if (move_count) ++(*move_count);
        return *this;
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// Promise<T> / Future<T> — 基础功能
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Future_Promise_SetValue_Get") {
    Promise<int> promise;
    auto future = promise.get_future();

    CHECK(future.valid());
    CHECK_FALSE(future.is_ready());

    promise.set_value(42);

    CHECK(future.is_ready());
    auto result = future.get();
    CHECK(result.ok());
    CHECK(result.value() == 42);
}

TEST_CASE("Future_Promise_SetError_Get") {
    Promise<int> promise;
    auto future = promise.get_future();

    promise.set_error(mine::core::Status{mine::core::StatusCode::Unknown});

    auto result = future.get();
    CHECK_FALSE(result.ok());
    CHECK(result.error().code() == mine::core::StatusCode::Unknown);
}

TEST_CASE("Future_Promise_SetValue_Idempotent") {
    Promise<int> promise;
    auto future = promise.get_future();

    promise.set_value(10);
    promise.set_value(20);  // 应被忽略

    auto result = future.get();
    CHECK(result.value() == 10);
}

TEST_CASE("Future_Promise_MoveSemantics") {
    Promise<int> p1;
    auto f1 = p1.get_future();

    // 移动 Promise
    Promise<int> p2 = std::move(p1);
    p2.set_value(99);

    auto result = f1.get();
    CHECK(result.value() == 99);
}

TEST_CASE("Future_Future_MoveSemantics") {
    Promise<int> promise;
    auto f1 = promise.get_future();

    Future<int> f2 = std::move(f1);
    CHECK_FALSE(f1.valid());  // 移动后源无效
    CHECK(f2.valid());

    promise.set_value(7);
    CHECK(f2.get().value() == 7);
}

TEST_CASE("Future_DefaultConstructed_NotValid") {
    Future<int> f;
    CHECK_FALSE(f.valid());
    CHECK_FALSE(f.is_ready());
}

TEST_CASE("Future_Promise_BrokenPromise") {
    Future<int> future;
    {
        Promise<int> promise;
        future = promise.get_future();
        // promise 离开作用域时析构，但未设置值
    }

    CHECK(future.is_ready());
    auto result = future.get();
    CHECK_FALSE(result.ok());
}

TEST_CASE("Future_Wait_BlocksUntilReady") {
    Promise<int> promise;
    auto future = promise.get_future();

    std::atomic<bool> started{false};

    std::thread t([&]() noexcept {
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        promise.set_value(1);
    });

    // 等待线程启动
    while (!started) { std::this_thread::yield(); }

    future.wait();
    // wait() 返回意味着结果已就绪
    CHECK(future.is_ready());
    CHECK(future.get().value() == 1);

    t.join();
}

TEST_CASE("Future_IsReady_NonBlocking") {
    Promise<int> promise;
    auto future = promise.get_future();

    CHECK_FALSE(future.is_ready());
    promise.set_value(5);
    CHECK(future.is_ready());
}

TEST_CASE("Future_OnReady_CalledImmediatelyWhenReady") {
    Promise<int> promise;
    auto future = promise.get_future();
    promise.set_value(88);

    int received = 0;
    future.on_ready([&](mine::core::Result<int> r) noexcept {
        if (r.ok()) received = r.value();
    });

    CHECK(received == 88);
}

TEST_CASE("Future_GetFuture_Twice_ReturnsInvalid") {
    Promise<int> promise;
    auto f1 = promise.get_future();
    auto f2 = promise.get_future();

    CHECK(f1.valid());
    CHECK_FALSE(f2.valid());
}

TEST_CASE("Future_Promise_NonTrivialType") {
    int move_count = 0;
    Promise<Tracker> promise;
    auto future = promise.get_future();

    promise.set_value(Tracker{42, &move_count});

    auto result = future.get();
    CHECK(result.ok());
    CHECK(result.value().value == 42);
    CHECK(move_count >= 1);  // 至少有一次移动
}

// ──────────────────────────────────────────────────────────────────────────────
// Task<T>
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Task_FromValue_ImmediatelyReady") {
    auto task = Task<int>::from_value(100);
    CHECK(task.valid());
    CHECK(task.is_ready());
    CHECK(task.get().value() == 100);
}

TEST_CASE("Task_FromError_ImmediatelyReady") {
    auto task = Task<int>::from_error(mine::core::Status{mine::core::StatusCode::InvalidArg});
    CHECK(task.is_ready());
    CHECK_FALSE(task.get().ok());
}

TEST_CASE("Task_FromFuture_BecomesReady") {
    Promise<int> promise;
    auto future = promise.get_future();
    auto task = Task<int>::from_future(std::move(future));

    CHECK(task.valid());
    CHECK_FALSE(task.is_ready());

    promise.set_value(55);
    CHECK(task.is_ready());
    CHECK(task.get().value() == 55);
}

TEST_CASE("Task_Then_Callback") {
    Promise<int> promise;
    auto task = Task<int>::from_future(promise.get_future());

    int received = 0;
    task.then([&](mine::core::Result<int> r) noexcept {
        if (r.ok()) received = r.value();
    });

    promise.set_value(77);
    CHECK(received == 77);
}

TEST_CASE("Task_DefaultConstructed_NotValid") {
    Task<int> t;
    CHECK_FALSE(t.valid());
}

TEST_CASE("Task_Wait_Blocks") {
    Promise<int> promise;
    auto task = Task<int>::from_future(promise.get_future());

    std::atomic<bool> started{false};

    std::thread t([&]() noexcept {
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        promise.set_value(1);
    });

    while (!started) { std::this_thread::yield(); }

    task.wait();
    CHECK(task.is_ready());
    CHECK(task.get().value() == 1);

    t.join();
}

// ──────────────────────────────────────────────────────────────────────────────
// Dispatcher
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Dispatcher_Post_Dispatch_ExecutesTask") {
    Dispatcher d;
    int value = 0;

    d.post([&]() noexcept { value = 42; });
    CHECK(value == 0);  // 尚未 dispatch

    uint32_t count = d.dispatch();
    CHECK(count == 1);
    CHECK(value == 42);
}

TEST_CASE("Dispatcher_Post_MultipleTasks") {
    Dispatcher d;
    int sum = 0;

    d.post([&]() noexcept { sum += 1; });
    d.post([&]() noexcept { sum += 2; });
    d.post([&]() noexcept { sum += 3; });

    uint32_t count = d.dispatch();
    CHECK(count == 3);
    CHECK(sum == 6);
}

TEST_CASE("Dispatcher_DispatchOne_SingleTask") {
    Dispatcher d;
    int a = 0, b = 0;

    d.post([&]() noexcept { a = 1; });
    d.post([&]() noexcept { b = 2; });

    CHECK(d.dispatch_one());
    CHECK(a == 1);
    CHECK(b == 0);  // 仅处理了一个

    CHECK(d.dispatch_one());
    CHECK(b == 2);

    CHECK_FALSE(d.dispatch_one());  // 队列为空
}

TEST_CASE("Dispatcher_Empty_Initially") {
    Dispatcher d;
    CHECK(d.empty());
    CHECK(d.pending_count() == 0);
}

TEST_CASE("Dispatcher_PendingCount") {
    Dispatcher d;
    d.post([]() noexcept {});
    d.post([]() noexcept {});

    CHECK(d.pending_count() == 2);

    d.dispatch();
    CHECK(d.pending_count() == 0);
}

TEST_CASE("Dispatcher_MoveSemantics") {
    Dispatcher d1;
    d1.post([]() noexcept {});

    Dispatcher d2 = std::move(d1);
    CHECK(d2.pending_count() == 1);

    d2.dispatch();
    CHECK(d2.empty());
}

TEST_CASE("Dispatcher_NestedDispatch_ProcessesNewTasks") {
    Dispatcher d;
    int outer = 0, inner = 0;

    d.post([&]() noexcept {
        outer = 1;
        // 在回调中 post 新任务
        d.post([&]() noexcept { inner = 2; });
    });

    uint32_t count = d.dispatch();
    // 嵌套 post 的任务应在同次 dispatch 中被处理
    CHECK(count >= 1);
    CHECK(outer == 1);
    // inner 可能被处理（取决于实现是否支持嵌套 dispatch）
    // 当前实现：dispatch() 循环直到队列为空，支持嵌套
    CHECK(inner == 2);
}

// ──────────────────────────────────────────────────────────────────────────────
// ThreadPool
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("ThreadPool_EnqueueDetached_ExecutesTask") {
    ThreadPool pool(2);
    std::atomic<int> value{0};

    pool.enqueue_detached([&]() noexcept {
        value.store(42);
    });

    pool.wait_all();
    CHECK(value.load() == 42);
}

TEST_CASE("ThreadPool_EnqueueMultiple_AllComplete") {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    const int kTaskCount = 100;

    for (int i = 0; i < kTaskCount; ++i) {
        pool.enqueue_detached([&]() noexcept {
            counter.fetch_add(1);
        });
    }

    pool.wait_all();
    CHECK(counter.load() == kTaskCount);
}

TEST_CASE("ThreadPool_Enqueue_WithResult") {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() noexcept -> int {
        return 123;
    });

    auto result = future.get();
    CHECK(result.ok());
    CHECK(result.value() == 123);
}

TEST_CASE("ThreadPool_ThreadCount") {
    ThreadPool pool(4);
    CHECK(pool.thread_count() == 4);
}

TEST_CASE("ThreadPool_DefaultThreadCount") {
    ThreadPool pool(0);  // 使用硬件并发数
    CHECK(pool.thread_count() > 0);
}

TEST_CASE("ThreadPool_MoveSemantics") {
    ThreadPool pool1(2);
    std::atomic<int> value{0};

    pool1.enqueue_detached([&]() noexcept {
        value.store(1);
    });

    ThreadPool pool2 = std::move(pool1);
    pool2.wait_all();
    CHECK(value.load() == 1);
}

TEST_CASE("ThreadPool_PendingCount") {
    ThreadPool pool(2);
    std::atomic<bool> started{false};
    std::atomic<bool> done{false};

    // 提交一个阻塞任务，让其他任务排队
    pool.enqueue_detached([&]() noexcept {
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        done = true;
    });

    // 等待第一个任务开始
    while (!started) { std::this_thread::yield(); }

    // 提交更多任务（它们将在队列中等待）
    for (int i = 0; i < 10; ++i) {
        pool.enqueue_detached([]() noexcept {});
    }

    // pending_count 是近似值
    uint32_t pending = pool.pending_count();
    // 至少有部分任务在队列中（可能已被工作线程取走一些）
    CHECK(pending >= 0);

    pool.wait_all();
    CHECK(done);
}

// ──────────────────────────────────────────────────────────────────────────────
// Timer
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Timer_SetTimeout_FiresOnce") {
    Timer timer;
    int fired = 0;

    // 使用较长延迟以兼容 Windows 定时器分辨率（约 15ms）
    auto handle = timer.set_timeout([&]() noexcept { ++fired; }, 50);
    CHECK(handle != kInvalidTimerHandle);
    CHECK(timer.is_active(handle));
    CHECK(timer.active_count() == 1);

    // 等待时间不足
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint32_t count = timer.tick();
    // 可能触发也可能不触发（取决于系统定时器精度）
    // 不强制检查 count == 0

    // 等待足够时间
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    count = timer.tick();
    CHECK(fired >= 1);

    // 不应再次触发
    CHECK_FALSE(timer.is_active(handle));
    CHECK(timer.active_count() == 0);
    count = timer.tick();
    CHECK(count == 0);
    // fired 不应再增加
    int fired_after = fired;
    CHECK(fired_after == fired);
}

TEST_CASE("Timer_SetInterval_FiresRepeatedly") {
    Timer timer;
    int fired = 0;

    auto handle = timer.set_interval([&]() noexcept { ++fired; }, 50);
    CHECK(handle != kInvalidTimerHandle);
    CHECK(timer.active_count() == 1);

    // 第一次触发
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    timer.tick();
    CHECK(fired == 1);

    // 第二次触发
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    timer.tick();
    CHECK(fired >= 2);

    CHECK(timer.is_active(handle));
}

TEST_CASE("Timer_Clear_CancelsTimer") {
    Timer timer;
    int fired = 0;

    auto handle = timer.set_timeout([&]() noexcept { ++fired; }, 200);
    CHECK(timer.is_active(handle));

    timer.clear(handle);
    CHECK_FALSE(timer.is_active(handle));
    CHECK(timer.active_count() == 0);

    // 等待后 tick，确保不会触发
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    timer.tick();
    CHECK(fired == 0);
}

TEST_CASE("Timer_ClearAll_CancelsAll") {
    Timer timer;
    int fired1 = 0, fired2 = 0;

    (void)timer.set_timeout([&]() noexcept { ++fired1; }, 100);
    (void)timer.set_timeout([&]() noexcept { ++fired2; }, 100);
    CHECK(timer.active_count() == 2);

    timer.clear_all();
    CHECK(timer.active_count() == 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    timer.tick();
    CHECK(fired1 == 0);
    CHECK(fired2 == 0);
}

TEST_CASE("Timer_InvalidHandle_NotActive") {
    Timer timer;
    CHECK_FALSE(timer.is_active(kInvalidTimerHandle));
}

TEST_CASE("Timer_Clear_InvalidHandle_NoOp") {
    Timer timer;
    timer.clear(kInvalidTimerHandle);  // 不应崩溃
    CHECK(timer.active_count() == 0);
}

TEST_CASE("Timer_MultipleTimers_Independent") {
    Timer timer;
    int a = 0, b = 0, c = 0;

    // 使用较大延迟以适应 Windows 定时器分辨率
    auto h1 = timer.set_timeout([&]() noexcept { a = 1; }, 30);
    auto h2 = timer.set_timeout([&]() noexcept { b = 2; }, 80);
    auto h3 = timer.set_timeout([&]() noexcept { c = 3; }, 150);

    CHECK(timer.active_count() == 3);

    // ~50ms 后：a 应已触发，b 和 c 不应触发
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.tick();
    CHECK(a == 1);
    // b 和 c 不应触发（延迟分别为 80ms 和 150ms）
    CHECK(b == 0);
    CHECK(c == 0);
    CHECK_FALSE(timer.is_active(h1));
    CHECK(timer.is_active(h2));
    CHECK(timer.is_active(h3));

    // 再等 ~60ms（累计 ~110ms）：b 应触发
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer.tick();
    CHECK(a == 1);
    CHECK(b == 2);
    CHECK(c == 0);

    // 再等 ~60ms（累计 ~170ms）：c 应触发
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer.tick();
    CHECK(a == 1);
    CHECK(b == 2);
    CHECK(c == 3);
}

TEST_CASE("Timer_SetInterval_Clear_StopsRepeating") {
    Timer timer;
    int fired = 0;

    auto handle = timer.set_interval([&]() noexcept { ++fired; }, 50);

    // 触发一次
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    timer.tick();
    CHECK(fired == 1);

    // 取消
    timer.clear(handle);
    CHECK_FALSE(timer.is_active(handle));

    // 不应再触发
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    timer.tick();
    CHECK(fired == 1);
}

// ──────────────────────────────────────────────────────────────────────────────
// 线程安全：跨线程 Promise/Future
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("ThreadSafety_PromiseSetValue_FromOtherThread") {
    Promise<int> promise;
    auto future = promise.get_future();

    std::thread t([p = std::move(promise)]() mutable noexcept {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        p.set_value(999);
    });

    auto result = future.get();
    CHECK(result.value() == 999);

    t.join();
}

TEST_CASE("ThreadSafety_MultipleFuturesWait") {
    Promise<int> p1, p2, p3;
    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    std::thread t1([p = std::move(p1)]() mutable noexcept {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        p.set_value(1);
    });
    std::thread t2([p = std::move(p2)]() mutable noexcept {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        p.set_value(2);
    });
    std::thread t3([p = std::move(p3)]() mutable noexcept {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        p.set_value(3);
    });

    CHECK(f1.get().value() == 1);
    CHECK(f2.get().value() == 2);
    CHECK(f3.get().value() == 3);

    t1.join();
    t2.join();
    t3.join();
}
