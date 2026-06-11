# mine.async

职责：提供异步任务调度基础设施。

## 模块组成

| 类型 | 说明 |
|------|------|
| `Future<T>` / `Promise<T>` | 异步结果传递原语（线程安全） |
| `Task<T>` | 可组合的异步任务抽象 |
| `Dispatcher` | 跨线程任务调度器（MPSC） |
| `ThreadPool` | 固定大小的并行工作线程池 |
| `Timer` | 延迟/周期性定时器（轻量，内联存储） |

## 设计原则

- 不依赖 C++ 异常：所有错误通过 `Result<T>` 显式传递
- 不依赖 RTTI：无虚函数
- 线程安全：Future/Promise、Dispatcher、ThreadPool 均为线程安全
- Timer 为单线程使用（须在调用 tick() 的同一线程操作）

## 使用示例

```cpp
// Promise/Future：异步结果传递
mine::async::Promise<int> promise;
auto future = promise.get_future();
std::thread([p = std::move(promise)]() mutable {
    p.set_value(42);
}).detach();
auto result = future.get();  // 阻塞等待

// ThreadPool：并行任务
mine::async::ThreadPool pool(4);
auto f = pool.enqueue([]() noexcept { return heavy_work(); });
auto r = f.get();

// Dispatcher：跨线程调度
mine::async::Dispatcher ui_dispatcher;
ui_dispatcher.post([]() noexcept { update_ui(); });
ui_dispatcher.dispatch();  // 在 UI 线程调用

// Timer：延迟执行
mine::async::Timer timer;
timer.set_timeout([]() noexcept { on_timeout(); }, 1000);
timer.tick();  // 每帧调用
```

## 依赖

- `mine.core`：基本类型、Result、Function