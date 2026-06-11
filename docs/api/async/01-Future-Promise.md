# Future<T> / Promise<T> 详细接口文档

## 概述

`Future<T>` 与 `Promise<T>` 是 `mine.async` 模块的核心异步结果传递原语，采用生产者-消费者模型：`Promise<T>` 为写入端，`Future<T>` 为读取端。两者通过引用计数的共享状态进行线程安全通信。

### 核心特性

- **线程安全**：内部使用 `std::mutex` + `std::condition_variable` 保护共享状态
- **引用计数**：Promise 和 Future 共享同一控制块，最后一个释放者负责销毁
- **Broken Promise 检测**：Promise 析构时若未设置值，Future 收到错误状态
- **幂等写入**：`set_value` / `set_error` 仅首次调用生效
- **零异常**：所有错误通过 `Result<T>` 显式传递
- **仅移动**：不可拷贝，支持移动语义

### 设计动机

C++ 标准库的 `std::future` / `std::promise` 存在以下问题：

1. **异常依赖**：`std::future::get()` 可能抛出 `std::future_error`
2. **类型限制**：`std::promise<void>` 需要特化处理
3. **ABI 不稳定**：不同编译器/标准库版本的布局可能不同

MineFramework 的实现：
- 错误通过 `Result<T>` 返回值传递，不使用异常
- 使用自定义引用计数共享状态，无 STL 智能指针依赖
- 公共头暴露为模板类，内联实现，无额外链接依赖

### 典型使用场景

| 场景 | 用法 |
|------|------|
| 线程间值传递 | `Promise<int> p; auto f = p.get_future();` 生产者线程 `p.set_value(42)` |
| 异步任务结果 | `ThreadPool::enqueue` 内部使用 Promise/Future 返回结果 |
| 协程桥接 | `Task<T>::promise_type` 内部使用 Promise 存储协程返回值 |
| 一次性通知 | 事件发生通知，等待者通过 `future.wait()` 阻塞 |

---

## 类定义

### Promise<T>

```cpp
namespace mine::async {

template<typename T>
class Promise {
public:
    Promise() noexcept;
    Promise(Promise&& other) noexcept;
    Promise& operator=(Promise&& other) noexcept;
    ~Promise() noexcept;

    Promise(const Promise&)            = delete;
    Promise& operator=(const Promise&) = delete;

    void set_value(T value) noexcept;
    void set_error(mine::core::Status error) noexcept;
    [[nodiscard]] Future<T> get_future() noexcept;

private:
    detail::FutureSharedState<T>* state_ = nullptr;
    bool future_obtained_ = false;
};

} // namespace mine::async
```

### Future<T>

```cpp
namespace mine::async {

template<typename T>
class Future {
public:
    Future() noexcept = default;
    Future(Future&& other) noexcept;
    Future& operator=(Future&& other) noexcept;
    ~Future() noexcept;

    Future(const Future&)            = delete;
    Future& operator=(const Future&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] mine::core::Result<T> get() noexcept;
    void wait() noexcept;
    void on_ready(mine::core::Function<void(mine::core::Result<T>)> callback) noexcept;

private:
    detail::FutureSharedState<T>* state_ = nullptr;
};

} // namespace mine::async
```

### 模板约束

- `T` 不可为引用类型（`static_assert(!std::is_reference_v<T>)`）
- `T` 不可为 `void`（请使用哨兵类型代替）

---

## Promise<T> 成员方法

### 默认构造

```cpp
Promise() noexcept;
```

**描述**：构造一个未完成的 Promise，内部分配共享状态（`ref_count = 2`）。

**时间复杂度**：O(1) + 一次堆分配

**后置条件**：`state_ != nullptr`，`future_obtained_ == false`

---

### 移动构造 / 移动赋值

```cpp
Promise(Promise&& other) noexcept;
Promise& operator=(Promise&& other) noexcept;
```

**描述**：转移所有权。移动后源 Promise 变为空状态。

**注意**：若移动赋值时当前 Promise 已有共享状态且未设置值，会触发 broken promise。

---

### 析构函数

```cpp
~Promise() noexcept;
```

**描述**：若持有共享状态且未调用 `set_value` / `set_error`，自动标记 broken promise 并通知等待者。

**效果**：
1. 若 `state_ != nullptr` 且未就绪：设置 `broken = true`，通知 `cv`
2. 递减引用计数，若归零则销毁共享状态

---

### set_value

```cpp
void set_value(T value) noexcept;
```

**描述**：设置成功值，唤醒所有阻塞在 `Future::get()` / `Future::wait()` 的线程。

**幂等性**：仅首次调用生效，后续调用静默忽略。

**参数**：
- `value`：成功值（将被移动到共享状态内部存储）

**线程安全**：可在任意线程调用。

**示例**：

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();
p.set_value(42);
auto r = f.get();         // Result<int>{ok_tag, 42}
p.set_value(100);         // 幂等：静默忽略
```

---

### set_error

```cpp
void set_error(mine::core::Status error) noexcept;
```

**描述**：设置错误状态，唤醒所有等待者。

**幂等性**：同 `set_value`。

---

### get_future

```cpp
[[nodiscard]] Future<T> get_future() noexcept;
```

**描述**：获取关联的 `Future<T>`。每个 Promise 仅可获取一次。

**返回值**：
- 首次调用：有效的 `Future<T>`（共享同一 SharedState）
- 重复调用：空 Future（`valid() == false`）

---

## Future<T> 成员方法

### valid

```cpp
[[nodiscard]] bool valid() const noexcept;
```

**描述**：检查是否关联有效的共享状态。

**返回值**：`true` 表示可安全调用 `get()` / `wait()` 等方法。

---

### is_ready

```cpp
[[nodiscard]] bool is_ready() const noexcept;
```

**描述**：非阻塞检查结果是否就绪。

**返回值**：`true` 表示 `get()` 不会阻塞。

**线程安全**：是。

---

### get

```cpp
[[nodiscard]] mine::core::Result<T> get() noexcept;
```

**描述**：阻塞等待并获取结果。

**返回值**：
- 成功：`Result<T>{ok_tag, value}`
- set_error：`Result<T>{err_tag, error}`
- broken promise：`Result<T>{err_tag, Status{StatusCode::Unknown}}`
- 无效 Future：`Result<T>{err_tag, Status{StatusCode::InvalidState}}`

**阻塞行为**：若结果未就绪，阻塞当前线程直到 `set_value` / `set_error` 被调用或 Promise 析构。

---

### wait

```cpp
void wait() noexcept;
```

**描述**：阻塞等待结果就绪，但不获取值。

**与 `get()` 的区别**：不消费结果，可多次调用。

---

### on_ready

```cpp
void on_ready(mine::core::Function<void(mine::core::Result<T>)> callback) noexcept;
```

**描述**：注册结果就绪回调。若结果已就绪，回调在当前线程同步调用；否则在设置值/错误的线程中调用。

**参数**：
- `callback`：回调函数，签名 `void(Result<T>) noexcept`

---

## 线程安全

| 方法 | 线程安全 | 说明 |
|------|---------|------|
| `Promise::set_value` | ✅ | 可跨线程调用 |
| `Promise::set_error` | ✅ | 可跨线程调用 |
| `Future::get` | ✅ | 阻塞安全 |
| `Future::wait` | ✅ | 阻塞安全 |
| `Future::is_ready` | ✅ | 非阻塞 |
| `Future::on_ready` | ✅ | 回调在设置线程同步执行 |

内部使用 `std::mutex` 保护 `ready` / `broken` / `has_value` 标志及值的读写。

---

## 完整示例

```cpp
#include <mine/async/Async.h>
#include <thread>

void producer_consumer_example() {
    mine::async::Promise<int> promise;
    auto future = promise.get_future();

    // 生产者线程
    std::thread producer([p = std::move(promise)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        p.set_value(42);
    });

    // 消费者：阻塞等待
    auto result = future.get();
    if (result.ok()) {
        std::printf("结果: %d\n", result.value());  // 42
    }

    producer.join();
}
```
