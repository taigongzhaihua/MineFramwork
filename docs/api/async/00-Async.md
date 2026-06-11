# mine.async 模块 API 文档

## 模块概述

`mine.async` 是 MineFramework L0 基础层的异步任务调度模块，提供协程任务、执行器与异步调度基础设施。依赖仅 `mine.core`。

### 模块组成

| 文档 | 类型 | 说明 |
|------|------|------|
| [01-Future-Promise.md](01-Future-Promise.md) | `Future<T>` / `Promise<T>` | 异步结果传递原语（线程安全） |
| [02-Task.md](02-Task.md) | `Task<T>` | 可组合异步任务 + C++20 协程支持 |
| [03-Dispatcher.md](03-Dispatcher.md) | `Dispatcher` | 跨线程任务调度器（MPSC） |
| [04-ThreadPool.md](04-ThreadPool.md) | `ThreadPool` | 固定大小工作线程池 |
| [05-Timer.md](05-Timer.md) | `Timer` | 轻量定时器管理器 |

### 依赖关系

```
mine.async
  └── mine.core (Result<T>, Status, Function<R(Args...)>, Pimpl<T>, Assert)
```

### 命名空间

所有类型位于 `mine::async` 命名空间。

### 设计约束

- **禁用异常**：所有错误通过 `Result<T>` 显式传递
- **禁用 RTTI**：无虚函数，使用函数指针表替代
- **PIMPL**：非模板类使用 PIMPL 隔离实现
- **C++20 协程**：`Task<T>` 支持 `co_return` / `co_await`
- **SBO → 堆回退**：`Function` 的 SBO 为 32 字节，超出自动堆分配

### 线程模型

```
┌──────────────────────────────────────────────────┐
│  UI 线程                                          │
│  Dispatcher::dispatch() + Timer::tick()           │
│  依赖属性写访问必须在此线程                         │
└────────────┬─────────────────────────────────────┘
             │ post()
┌────────────▼─────────────────────────────────────┐
│  IO/任务池 (ThreadPool)                           │
│  网络 IO、文件 IO、数据库访问、CPU 密集计算        │
│  通过 Future<T> / Task<T> 返回结果到 UI 线程       │
└──────────────────────────────────────────────────┘
```

### 快速开始

```cpp
#include <mine/async/Async.h>

// Promise/Future：线程间传值
mine::async::Promise<int> p;
auto f = p.get_future();
std::thread([p = std::move(p)]() mutable { p.set_value(42); }).detach();
auto r = f.get();  // Result<int>{ok_tag, 42}

// ThreadPool：并行任务
mine::async::ThreadPool pool(4);
auto future = pool.enqueue([]() noexcept { return heavy_work(); });
auto result = future.get();

// Dispatcher：UI 线程调度
mine::async::Dispatcher ui;
ui.post([]() noexcept { update_label("完成"); });
ui.dispatch();

// Timer：延迟/周期执行
mine::async::Timer timer;
timer.set_timeout([]() noexcept { on_timeout(); }, 1000);
timer.tick();  // 每帧调用

// C++20 协程
mine::async::Task<int> compute() {
    co_return 42;
}
mine::async::Task<int> chain() {
    auto r = co_await compute();  // Result<int>
    co_return r.ok() ? r.value() * 2 : -1;
}
```

### 测试覆盖

48 个单元测试，覆盖所有公开 API 的正常路径、错误路径、移动语义和线程安全场景。
