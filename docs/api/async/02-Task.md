# Task<T> 详细接口文档

## 概述

`Task<T>` 是 `mine.async` 模块的可组合异步任务抽象，同时支持**回调式**（`then` / `map`）和 **C++20 协程式**（`co_return` / `co_await`）两种编程范式。

### 核心特性

- **双范式**：`then()` 回调链 + `co_await` 协程，同一类型两种用法
- **基于 Future<T>**：内部委托给 `Future<T>` 实现，天然线程安全
- **C++20 协程**：实现 `promise_type` + `operator co_await`，可直接作为协程返回类型
- **错误显式**：`co_await task` 返回 `Result<T>`，不抛异常，调用方显式检查
- **Monadic Map**：`map<U>(transform)` 变换结果类型，支持链式组合

### 设计动机

上层模块（`mvvm`、`net`、`data`）需要一种统一的异步抽象：
- MVVM 的命令执行需要在后台线程运行，结果回到 UI 线程
- 网络请求是天然异步的，需要链式组合（请求 → 解析 → 更新 UI）
- 数据库操作可以在协程中顺序书写，避免回调地狱

### 典型使用场景

| 场景 | 方式 | 示例 |
|------|------|------|
| 简单异步 | 回调式 | `task.then([](Result<int> r) { ... })` |
| 链式变换 | `map` | `task.map<int>([](auto r) { return r.value() * 2; })` |
| 协程组合 | `co_await` | `auto r = co_await compute();` |
| 立即值 | 工厂 | `Task<int>::from_value(42)` |

---

## 类定义

```cpp
namespace mine::async {

template<typename T>
class Task {
public:
    // ── C++20 协程支持 ────────────────────────────────────────────
    class promise_type;       // 协程 promise（编译器自动使用）
    class Awaitable;          // co_await 返回类型

    // ── 构造 ──────────────────────────────────────────────────────
    Task() noexcept = default;
    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;
    ~Task() noexcept = default;
    Task(const Task&) = delete;

    // ── 工厂 ──────────────────────────────────────────────────────
    [[nodiscard]] static Task<T> from_value(T value) noexcept;
    [[nodiscard]] static Task<T> from_error(mine::core::Status error) noexcept;
    [[nodiscard]] static Task<T> from_future(Future<T> future) noexcept;

    // ── 查询 ──────────────────────────────────────────────────────
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;

    // ── 结果 ──────────────────────────────────────────────────────
    [[nodiscard]] mine::core::Result<T> get() noexcept;
    void wait() noexcept;

    // ── 组合 ──────────────────────────────────────────────────────
    Task<T>& then(mine::core::Function<void(mine::core::Result<T>)> callback) noexcept;
    template<typename U>
    [[nodiscard]] Task<U> map(mine::core::Function<U(mine::core::Result<T>)> transform) noexcept;

    // ── 协程 ──────────────────────────────────────────────────────
    [[nodiscard]] Awaitable operator co_await() noexcept;

private:
    Future<T> future_;
};

} // namespace mine::async
```

---

## C++20 协程支持

### promise_type

```cpp
class Task<T>::promise_type {
public:
    [[nodiscard]] Task<T> get_return_object() noexcept;
    [[nodiscard]] std::suspend_never initial_suspend() noexcept;
    [[nodiscard]] std::suspend_never final_suspend() noexcept;

    template<typename U> requires std::is_constructible_v<T, U>
    void return_value(U&& value) noexcept;

    void unhandled_exception() noexcept;
};
```

**编译器行为**：
1. 遇到 `co_return value;` → 调用 `promise_type::return_value(value)`
2. 协程函数返回 → 调用 `get_return_object()` 获取 `Task<T>`
3. `initial_suspend` 返回 `suspend_never` → 协程立即执行（惰性启动）
4. `final_suspend` 返回 `suspend_never` → 协程帧自动销毁

**异常处理**：项目禁用异常，`unhandled_exception()` 触发断言并设置错误状态。

### operator co_await

```cpp
[[nodiscard]] Awaitable operator co_await() noexcept;
```

**返回值**：`Awaitable` 对象，实现协程挂起/恢复协议。

**语义**：`co_await task` 返回 `Result<T>`——调用方显式检查错误：

```cpp
mine::async::Task<int> chain() {
    auto r = co_await compute();   // r: Result<int>
    if (r.ok()) {
        co_return r.value() * 2;
    }
    co_return -1;                  // 错误回退值
}
```

> **设计决策**：返回 `Result<T>` 而非 `T`。
> 在无异常环境下，协程无法自动传播错误，必须由调用方显式处理。
> 这与 `Future<T>::get()` 返回 `Result<T>` 保持一致。

---

## 工厂方法

### from_value

```cpp
[[nodiscard]] static Task<T> from_value(T value) noexcept;
```

**描述**：从已完成的值创建立即就绪的 Task。

**示例**：
```cpp
auto task = Task<int>::from_value(100);
CHECK(task.is_ready());        // true
CHECK(task.get().value() == 100);
```

### from_error

```cpp
[[nodiscard]] static Task<T> from_error(mine::core::Status error) noexcept;
```

**描述**：从错误状态创建立即就绪的 Task。

### from_future

```cpp
[[nodiscard]] static Task<T> from_future(Future<T> future) noexcept;
```

**描述**：包装已有 Future<T> 为 Task<T>。常用于将 Promise/Future 操作纳入 Task 组合链。

---

## 组合方法

### then

```cpp
Task<T>& then(mine::core::Function<void(mine::core::Result<T>)> callback) noexcept;
```

**描述**：注册完成回调，支持链式调用。

**回调时机**：
- 若任务已完成：立即在当前线程同步调用
- 若任务未完成：在设置结果值的线程中调用

**返回值**：`*this`，支持链式调用。

### map

```cpp
template<typename U>
[[nodiscard]] Task<U> map(mine::core::Function<U(mine::core::Result<T>)> transform) noexcept;
```

**描述**：Monadic map——变换结果类型，返回新的 `Task<U>`。

**示例**：
```cpp
auto task = Task<int>::from_value(10);
auto mapped = task.map<int>([](Result<int> r) noexcept -> int {
    return r.ok() ? r.value() * 2 : -1;
});
CHECK(mapped.get().value() == 20);
```

---

## 协程使用示例

```cpp
// 基础协程
mine::async::Task<int> compute_answer() {
    co_return 42;
}

// 链式协程
mine::async::Task<int> double_answer() {
    auto r = co_await compute_answer();   // Result<int>
    if (r.ok()) {
        co_return r.value() * 2;
    }
    co_return -1;
}

// 组合协程与回调
void use_task() {
    auto task = double_answer();
    task.then([](mine::core::Result<int> r) noexcept {
        if (r.ok()) {
            update_ui(r.value());  // 84
        }
    });
}
```
