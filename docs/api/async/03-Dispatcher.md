# Dispatcher 详细接口文档

## 概述

`Dispatcher` 是 `mine.async` 模块的跨线程任务调度器，采用**多生产者单消费者（MPSC）**模型：任意线程可通过 `post()` 提交任务，目标线程（通常是 UI 线程）通过 `dispatch()` 批量处理。

### 核心特性

- **MPSC 模型**：多线程安全 post，单线程 dispatch
- **双队列交换**：`post()` 写入入队队列（加锁），`dispatch()` 交换到处理队列（无锁执行）
- **嵌套 dispatch**：回调中再次 `post` 的任务在同次 `dispatch()` 调用中被处理
- **PIMPL 封装**：内部使用 `std::mutex` + `std::vector<Function<void()>>`，不暴露在公共头

### 设计动机

UI 框架的核心线程安全约束：**所有 UI 操作必须在 UI 线程执行**。当后台线程（网络、IO、计算）需要更新界面时，必须将任务"投递"到 UI 线程。`Dispatcher` 提供了这一机制。

### 典型使用场景

| 场景 | 用法 |
|------|------|
| 后台线程 → UI 线程 | `dispatcher.post([]() noexcept { update_ui(); })` |
| Timer 回调调度 | `Timer::set_timeout_on(dispatcher, cb, 100)` |
| 网络响应处理 | HTTP 回调中 `dispatcher.post([data]() { render(data); })` |

---

## 类定义

```cpp
namespace mine::async {

class MINE_ASYNC_API Dispatcher {
public:
    Dispatcher() noexcept;
    Dispatcher(Dispatcher&& other) noexcept;
    Dispatcher& operator=(Dispatcher&& other) noexcept;
    ~Dispatcher() noexcept;

    Dispatcher(const Dispatcher&) = delete;

    void post(mine::core::Function<void()> fn) noexcept;

    uint32_t dispatch() noexcept;
    bool dispatch_one() noexcept;

    [[nodiscard]] uint32_t pending_count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

} // namespace mine::async
```

---

## 成员方法

### post

```cpp
void post(mine::core::Function<void()> fn) noexcept;
```

**描述**：向调度器提交任务（线程安全）。任务将在目标线程的下一次 `dispatch()` 调用中执行。

**参数**：
- `fn`：要执行的任务（无参数，无返回值，move-only）

**线程安全**：可在任意线程调用。

**时间复杂度**：O(1) + 锁开销。

---

### dispatch

```cpp
uint32_t dispatch() noexcept;
```

**描述**：在目标线程中处理**所有**排队任务。使用双队列交换避免长时间持锁。

**执行流程**：
1. 加锁，将 `incoming_` 交换到 `processing_`
2. 解锁，遍历 `processing_` 依次执行回调
3. 重复直到 `incoming_` 为空（支持嵌套 dispatch）

**返回值**：已处理的任务数量。

**线程安全**：仅可在目标线程调用。

**嵌套 dispatch 示例**：

```cpp
mine::async::Dispatcher d;

d.post([&]() noexcept {
    // 在回调中再次 post
    d.post([&]() noexcept { step2(); });
    step1();
});

d.dispatch();  // step1 和 step2 都在同次 dispatch 中执行
```

---

### dispatch_one

```cpp
bool dispatch_one() noexcept;
```

**描述**：处理**单个**排队任务。常用于需要精细控制执行时机的场景。

**返回值**：
- `true`：处理了一个任务
- `false`：队列为空

---

### pending_count

```cpp
[[nodiscard]] uint32_t pending_count() const noexcept;
```

**描述**：获取当前排队任务数（近似值，线程安全）。

### empty

```cpp
[[nodiscard]] bool empty() const noexcept;
```

**描述**：`pending_count() == 0` 的快捷方式。

---

## 线程模型

```
  工作线程 1 ──┐
  工作线程 2 ──┤  post(fn)
  工作线程 3 ──┘      ↓
              ┌─────────────────┐
              │  incoming_ 队列  │  (mutex 保护)
              └────────┬────────┘
                       │ dispatch() 交换
              ┌────────▼────────┐
              │ processing_ 队列 │  (无锁执行)
              └────────┬────────┘
                       │ 依次调用 fn()
              ┌────────▼────────┐
              │    UI 线程       │
              └─────────────────┘
```

---

## 与 Timer 的集成

`Dispatcher` 与 `Timer` 通过 `set_timeout_on` / `set_interval_on` 紧密集成：

```cpp
mine::async::Dispatcher ui_dispatcher;
mine::async::Timer timer;

// 1 秒后在 UI 线程执行回调
timer.set_timeout_on(ui_dispatcher, []() noexcept {
    show_notification("操作完成");
}, 1000);

// 主循环
while (running) {
    timer.tick();              // 检查定时器
    ui_dispatcher.dispatch();  // 处理排队任务
}
```

---

## 完整示例

```cpp
#include <mine/async/Async.h>
#include <thread>

void dispatcher_example() {
    mine::async::Dispatcher ui;

    // 模拟后台线程
    std::thread worker([&ui]() noexcept {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 投递到 UI 线程
        ui.post([]() noexcept {
            // 此 lambda 在 dispatch() 调用线程执行
            std::printf("UI 更新完成\n");
        });
    });

    // 主循环
    ui.dispatch();  // 阻塞直到队列为空（等待 worker 的 post）

    worker.join();
}
```
