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
- `T` 不可为 `void`（请使用哨兵类型如 `int` 或自定义 `Unit` 类型代替）
- `T` 必须可移动构造（`std::is_move_constructible_v<T>`）

---

## 内部实现机制

### 共享状态结构 (FutureSharedState<T>)

```cpp
namespace mine::async::detail {

template<typename T>
struct FutureSharedState {
    std::mutex              mutex;        // 保护所有可变状态
    std::condition_variable cv;           // 阻塞等待唤醒机制
    std::atomic<uint32_t>   ref_count{2}; // Promise + Future（初始 2）
    
    bool                    ready{false}; // 结果已设置（值或错误）
    bool                    broken{false};// Promise 析构时未设置值
    bool                    has_value{false}; // true: 持有值，false: 持有错误
    
    union {
        T                       value;    // 成功值（has_value == true）
        mine::core::Status      error;    // 错误状态（has_value == false）
    };
    
    mine::core::Function<void(mine::core::Result<T>)> callback; // on_ready 回调
};

} // namespace mine::async::detail
```

### 引用计数机制

**初始化**：`Promise` 构造时分配 `SharedState`，`ref_count = 2`（Promise 持有 1，Future 持有 1）。

**递增**：无——`Promise` / `Future` 不可拷贝。

**递减**：
1. `Promise` 析构 → `--ref_count`
2. `Future` 析构 → `--ref_count`
3. 当 `ref_count == 0` → `delete state_`

**Broken Promise 时序**：
```
t0: Promise 构造              ref_count = 2
t1: get_future()              Future 获得 state_ 指针
t2: Promise 析构（未 set_value）
    → 检测 !ready
    → broken = true
    → cv.notify_all()
    → --ref_count (= 1)
t3: Future::get() 返回
    → Result{err_tag, Status{StatusCode::Unknown}}
t4: Future 析构
    → --ref_count (= 0)
    → delete state_
```

### 线程同步协议

**写入路径 (`set_value` / `set_error`)**：
```cpp
void Promise<T>::set_value(T value) noexcept {
    std::lock_guard lock(state_->mutex);
    if (state_->ready) return; // 幂等：已设置则忽略
    
    ::new(&state_->value) T(std::move(value)); // placement new
    state_->has_value = true;
    state_->ready = true;
    
    auto cb = std::move(state_->callback); // 取出回调（避免持锁调用）
    state_->cv.notify_all();               // 唤醒所有等待者
    
    lock.unlock(); // 释放锁
    if (cb) cb(mine::core::Result<T>{ok_tag, state_->value}); // 执行回调
}
```

**读取路径 (`get` / `wait`)**：
```cpp
mine::core::Result<T> Future<T>::get() noexcept {
    if (!state_) return Result<T>{err_tag, Status{StatusCode::InvalidState}};
    
    std::unique_lock lock(state_->mutex);
    state_->cv.wait(lock, [this] {
        return state_->ready || state_->broken; // 等待谓词
    });
    
    if (state_->broken) {
        return Result<T>{err_tag, Status{StatusCode::Unknown}};
    }
    if (state_->has_value) {
        return Result<T>{ok_tag, state_->value};
    }
    return Result<T>{err_tag, state_->error};
}
```

**关键设计决策**：
1. **回调在设置线程执行**：`set_value` 调用回调前先释放锁，避免死锁。
2. **回调移出后执行**：`state_->callback` 被 `std::move` 移出，避免重复触发。
3. **condition_variable 谓词**：使用 lambda 谓词避免虚假唤醒。

---

- `T` 不可为引用类型（`static_assert(!std::is_reference_v<T>)`）
- `T` 不可为 `void`（请使用哨兵类型代替）

---

## Promise<T> 成员方法

### 默认构造

```cpp
Promise() noexcept;
```

**描述**：构造一个未完成的 Promise，内部分配共享状态（`ref_count = 2`）。

**堆分配**：一次 `new FutureSharedState<T>`，大小约 64-128 字节（取决于 `sizeof(T)`）。

**时间复杂度**：O(1) + 一次堆分配

**后置条件**：
- `state_ != nullptr`
- `future_obtained_ == false`
- `state_->ref_count == 2`
- `state_->ready == false`

**异常安全**：Basic guarantee（若 `new` 失败，对象不被构造）。项目禁用异常，OOM 触发全局错误处理。

---

### 移动构造 / 移动赋值

```cpp
Promise(Promise&& other) noexcept;
Promise& operator=(Promise&& other) noexcept;
```

**移动构造**：

**描述**：转移所有权，将 `other` 的 `state_` 指针转移到 `this`，`other` 变为空状态。

**后置条件**：
- `this->state_ == old_other.state_`
- `other.state_ == nullptr`
- `other.future_obtained_` 保持不变（历史记录）

**不触发 broken promise**：移动构造不递减引用计数，所有权直接转移。

---

**移动赋值**：

**描述**：先销毁当前状态（若有），再转移 `other` 的所有权。

**实现伪代码**：
```cpp
Promise& Promise<T>::operator=(Promise&& other) noexcept {
    if (this != &other) {
        // 销毁当前状态
        if (state_) {
            if (!state_->ready) {
                // 触发 broken promise
                std::lock_guard lock(state_->mutex);
                state_->broken = true;
                state_->cv.notify_all();
            }
            release_ref(); // --ref_count，可能 delete state_
        }
        
        // 转移所有权
        state_ = other.state_;
        future_obtained_ = other.future_obtained_;
        other.state_ = nullptr;
    }
    return *this;
}
```

**注意**：若赋值前 `this->state_` 持有未完成的 Promise，会触发 broken promise。

**边界情况**：
```cpp
Promise<int> p1;
auto f = p1.get_future();

Promise<int> p2;
p1 = std::move(p2);  // p1 的原 state 被标记 broken，f.get() 返回错误
```

---

### 析构函数

```cpp
~Promise() noexcept;
```

**描述**：若持有共享状态且未调用 `set_value` / `set_error`，自动标记 broken promise 并通知等待者。

**实现伪代码**：
```cpp
Promise<T>::~Promise() noexcept {
    if (!state_) return;
    
    {
        std::lock_guard lock(state_->mutex);
        if (!state_->ready) {
            state_->broken = true;
            state_->cv.notify_all(); // 唤醒所有阻塞在 get()/wait() 的线程
        }
    }
    
    release_ref(); // --ref_count，若归零则 delete state_
}
```

**生命周期约束**：Promise 必须比 Future 活得更久**或**在析构前调用 `set_value` / `set_error`。

---

### set_value

```cpp
void set_value(T value) noexcept;
```

**描述**：设置成功值，唤醒所有阻塞在 `Future::get()` / `Future::wait()` 的线程。

**幂等性**：仅首次调用生效，后续调用静默忽略（不触发断言，不修改状态）。

**参数**：
- `value`：成功值（将被移动到共享状态内部存储）

**线程安全**：可在任意线程调用。

**实现细节**：
1. 加锁 `mutex`
2. 检查 `ready`，若已设置则返回
3. 使用 placement new 构造 `state_->value`
4. 设置 `has_value = true`, `ready = true`
5. 移出 `callback`（若有）
6. 通知 `cv`
7. 解锁
8. 持锁外执行回调（避免死锁）

**回调执行时机**：在**设置值的线程**中同步执行，持锁外。

**示例**：

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();

// 线程 1
p.set_value(42);   // 唤醒等待者，触发回调
p.set_value(100);  // 幂等：静默忽略

// 线程 2
auto r = f.get();  // Result<int>{ok_tag, 42}
```

---

### set_error

```cpp
void set_error(mine::core::Status error) noexcept;
```

**描述**：设置错误状态，唤醒所有等待者。

**幂等性**：同 `set_value`。

**参数**：
- `error`：错误状态（包含错误码和可选消息）

**与 broken promise 的区别**：
- `set_error`：显式错误（调用方知晓）
- `broken`：隐式错误（Promise 意外析构）

---

### get_future

```cpp
[[nodiscard]] Future<T> get_future() noexcept;
```

**描述**：获取关联的 `Future<T>`。每个 Promise 仅可获取一次。

**返回值**：
- 首次调用：有效的 `Future<T>`（共享同一 SharedState，`ref_count` 保持 2）
- 重复调用：空 Future（`valid() == false`，`state_ == nullptr`）

**线程安全**：否——必须在同一线程多次调用或使用外部同步。

**实现**：
```cpp
Future<T> Promise<T>::get_future() noexcept {
    if (future_obtained_) {
        return Future<T>{}; // 空 Future
    }
    future_obtained_ = true;
    return Future<T>{state_}; // 共享 state_ 指针
}
```

**为何不可多次调用**：`Future<T>` 的 `get()` 会消费结果值（移动），多个 Future 会导致 use-after-move。

---

## Future<T> 成员方法

### 默认构造

```cpp
Future() noexcept = default;
```

**描述**：构造一个空 Future（不持有共享状态）。

**后置条件**：
- `state_ == nullptr`
- `valid() == false`

**用途**：作为占位符或表示"无未来值"的状态。

---

### 移动构造 / 移动赋值

```cpp
Future(Future&& other) noexcept;
Future& operator=(Future&& other) noexcept;
```

**移动构造**：

**描述**：转移所有权，`other` 变为空状态。

**后置条件**：
- `this->state_ == old_other.state_`
- `other.state_ == nullptr`

**不触发引用计数变化**：所有权直接转移。

---

**移动赋值**：

**描述**：先释放当前状态（递减引用计数），再转移 `other` 的所有权。

**实现伪代码**：
```cpp
Future& Future<T>::operator=(Future&& other) noexcept {
    if (this != &other) {
        if (state_) {
            release_ref(); // --ref_count，可能 delete state_
        }
        state_ = other.state_;
        other.state_ = nullptr;
    }
    return *this;
}
```

---

### 析构函数

```cpp
~Future() noexcept;
```

**描述**：若持有共享状态，递减引用计数。若引用计数归零，销毁共享状态。

**不阻塞**：析构函数不会等待 Promise 设置值——即使 Future 被析构，Promise 仍可正常 `set_value`（只是无等待者）。

---

### valid

```cpp
[[nodiscard]] bool valid() const noexcept;
```

**描述**：检查 Future 是否持有有效共享状态。

**返回值**：
- `true`：持有共享状态（可调用 `get` / `wait` / `is_ready`）
- `false`：空 Future（默认构造或移动后）

**用途**：在调用 `get()` 前检查有效性，避免返回 `InvalidState` 错误。

**示例**：
```cpp
mine::async::Future<int> f; // 默认构造
assert(!f.valid());          // 空 Future

mine::async::Promise<int> p;
f = p.get_future();
assert(f.valid());           // 有效 Future

auto f2 = std::move(f);
assert(!f.valid());          // f 被移动后变为空
assert(f2.valid());          // f2 持有所有权
```

---

### is_ready

```cpp
[[nodiscard]] bool is_ready() const noexcept;
```

**描述**：非阻塞地检查结果是否已就绪（Promise 调用了 `set_value` / `set_error` 或被标记 broken）。

**返回值**：
- `true`：结果已就绪，调用 `get()` 不会阻塞
- `false`：结果未就绪，调用 `get()` 会阻塞当前线程

**线程安全**：可在任意线程调用。

**实现**：
```cpp
bool Future<T>::is_ready() const noexcept {
    if (!state_) return false;
    std::lock_guard lock(state_->mutex);
    return state_->ready || state_->broken;
}
```

**性能**：O(1)，仅加锁读取标志位。

**用途**：
1. 轮询模式（polling）：避免阻塞主线程
2. 多 Future 批量检查：找到第一个就绪的
3. 超时前试探：先 `is_ready()`，若未就绪再考虑超时等待

**示例**：
```cpp
mine::async::Promise<int> p;
auto f = p.get_future();

assert(!f.is_ready());  // 未设置值

p.set_value(42);
assert(f.is_ready());   // 已就绪
```

---

### wait

```cpp
void wait() noexcept;
```

**描述**：阻塞当前线程，直到结果就绪（`ready == true || broken == true`）。

**不消费结果**：`wait()` 后可继续调用 `get()` 获取结果。

**无返回值**：仅同步，不提供错误信息。

**线程安全**：可在任意线程调用（每个线程会阻塞在自己的 `cv.wait()`）。

**实现**：
```cpp
void Future<T>::wait() noexcept {
    if (!state_) return; // 空 Future 直接返回
    
    std::unique_lock lock(state_->mutex);
    state_->cv.wait(lock, [this] {
        return state_->ready || state_->broken;
    });
}
```

**虚假唤醒处理**：使用 lambda 谓词，condition_variable 自动处理虚假唤醒。

**多线程等待**：多个线程可同时 `wait()` 同一个 Future，`set_value` 会 `notify_all()` 唤醒所有等待者。

**示例**：
```cpp
mine::async::Promise<std::string> p;
auto f = p.get_future();

std::thread t([&p] {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    p.set_value("Hello");
});

f.wait();            // 阻塞约 1 秒
auto r = f.get();    // Result<string>{ok_tag, "Hello"}
t.join();
```

---

### get

```cpp
[[nodiscard]] mine::core::Result<T> get() noexcept;
```

**描述**：阻塞等待并获取结果值。这是 Future 的主要消费方法。

**返回值**：
- 成功：`Result<T>{ok_tag, value}`（包含 Promise 设置的值）
- 错误：`Result<T>{err_tag, status}`（Promise 调用 `set_error` 或 broken promise）
- 无效：`Result<T>{err_tag, Status{StatusCode::InvalidState}}`（空 Future）

**阻塞行为**：若结果未就绪，阻塞当前线程直到：
1. `set_value` 被调用
2. `set_error` 被调用
3. Promise 析构（触发 broken）

**值消费语义**：
- `Result<T>` 通过**拷贝**返回（不消费共享状态中的值）
- 多次调用 `get()` 返回**相同**的结果（幂等读取）

**错误码说明**：

| 错误码 | 原因 |
|--------|------|
| `StatusCode::InvalidState` | 空 Future（`state_ == nullptr`） |
| `StatusCode::Unknown` | Broken promise（Promise 未设置值就析构） |
| 其他 | `Promise::set_error` 显式传递的错误 |

**实现关键点**：
```cpp
mine::core::Result<T> Future<T>::get() noexcept {
    if (!state_) {
        return Result<T>{err_tag, Status{StatusCode::InvalidState}};
    }
    
    std::unique_lock lock(state_->mutex);
    state_->cv.wait(lock, [this] {
        return state_->ready || state_->broken;
    });
    
    if (state_->broken) {
        return Result<T>{err_tag, Status{StatusCode::Unknown}};
    }
    if (state_->has_value) {
        return Result<T>{ok_tag, state_->value}; // 拷贝值
    }
    return Result<T>{err_tag, state_->error};
}
```

**示例 1：成功获取**

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();

std::thread t([&p] {
    p.set_value(42);
});

auto r = f.get();  // 阻塞直到 t 线程调用 set_value
assert(r.is_ok());
assert(r.value() == 42);

t.join();
```

**示例 2：Broken promise**

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();
// Promise 析构，未设置值

auto r = f.get();  // Result{err_tag, Status{StatusCode::Unknown}}
assert(r.is_err());
```

**示例 3：显式错误**

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();
p.set_error(Status{StatusCode::Cancelled, "User cancelled"});

auto r = f.get();
assert(r.is_err());
assert(r.error().code() == StatusCode::Cancelled);
```

---

### on_ready

```cpp
void on_ready(mine::core::Function<void(mine::core::Result<T>)> callback) noexcept;
```

**描述**：注册一个回调，当结果就绪时异步触发（非阻塞模式）。

**执行时机**：
- **若结果已就绪**：在**当前线程**（调用 `on_ready` 的线程）中同步执行回调
- **若结果未就绪**：在**调用 `set_value` / `set_error` 的线程**中执行回调（持锁外）

**线程安全**：可在任意线程调用（内部加锁）。

**回调参数**：`Result<T>`（与 `get()` 返回值语义相同）

**仅触发一次**：回调被移出共享状态后执行，不会重复触发。

**重复调用**：
- **未就绪状态**：仅最后一次 `on_ready` 的回调生效（覆盖前一个）
- **已就绪状态**：每次 `on_ready` 都会同步执行（无覆盖）

**实现伪代码**：
```cpp
void Future<T>::on_ready(Function<void(Result<T>)> callback) noexcept {
    if (!state_) {
        callback(Result<T>{err_tag, Status{StatusCode::InvalidState}});
        return;
    }
    
    std::unique_lock lock(state_->mutex);
    if (state_->ready || state_->broken) {
        // 已就绪：同步执行
        lock.unlock();
        Result<T> r = /* 构造结果 */;
        callback(std::move(r));
    } else {
        // 未就绪：保存回调（覆盖旧回调）
        state_->callback = std::move(callback);
    }
}
```

**陷阱警告**：
1. **回调中不可阻塞**：执行在 `set_value` 线程中，阻塞会延迟生产者
2. **回调中不可再次访问同一 Future**：可能死锁（持锁状态）
3. **闭包生命周期**：回调中捕获的变量需保证在执行时仍有效

**示例 1：未就绪时注册**

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();

f.on_ready([](auto r) {
    if (r.is_ok()) {
        std::cout << "Value: " << r.value() << "\n"; // 打印 "Value: 42"
    }
});

p.set_value(42); // 触发回调（在当前线程执行）
```

**示例 2：已就绪时注册**

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();
p.set_value(42);

f.on_ready([](auto r) {
    std::cout << "Already ready: " << r.value() << "\n"; // 立即执行
});
```

**示例 3：线程池集成**

```cpp
mine::async::Promise<int> p;
auto f = p.get_future();

mine::async::ThreadPool pool(4);
f.on_ready([&pool](auto r) {
    if (r.is_ok()) {
        pool.enqueue([v = r.value()] {
            // 在线程池中处理结果
            process(v);
        });
    }
});

p.set_value(100); // 回调在当前线程执行，但进一步调度到线程池
```

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

---

## 性能特征

### 内存开销

**FutureSharedState<T> 结构大小**（Windows x64 / MSVC）：

| 组件 | 大小 | 说明 |
|------|------|------|
| `std::mutex` | 80 字节 | Windows SRWLOCK 包装 |
| `std::condition_variable` | 72 字节 | Windows CONDITION_VARIABLE 包装 |
| `std::atomic<uint32_t>` | 4 字节 | 引用计数 |
| `bool` × 3 | 3 字节 | ready / broken / has_value |
| padding | 5 字节 | 对齐 |
| `union { T value; Status error; }` | `max(sizeof(T), 16)` | 值或错误 |
| `Function<void(Result<T>)>` | 40 字节 | on_ready 回调 |
| **总计** | **~204 + sizeof(T)** | 取 max(sizeof(T), 16) |

**示例大小**：
- `FutureSharedState<int>`：约 220 字节
- `FutureSharedState<std::string>`：约 232 字节（std::string 通常 32 字节）
- `FutureSharedState<std::vector<int>>`：约 216 字节（vector 24 字节）

### 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| `Promise()` | O(1) + 堆分配 | 分配共享状态 |
| `Promise::set_value` | O(1) + 回调 | 加锁、placement new、唤醒 |
| `Future::get()` | O(1) + 阻塞 | 等待+拷贝值 |
| `Future::wait()` | O(1) + 阻塞 | 仅等待 |
| `Future::is_ready()` | O(1) | 轻量锁检查 |
| `Future::on_ready()` | O(1) | 加锁注册回调 |

### 同步开销

**关键路径延迟**（Windows x64，无竞争）：

1. **set_value → get 唤醒**：
   - 持锁时间：~50-100 ns（placement new + 设置标志）
   - 唤醒延迟：~1-5 µs（线程调度器介入）
   - **总延迟：1-5 µs**

2. **is_ready() 轮询**：
   - 无竞争：~10-20 ns（快速锁获取）
   - 有竞争：~50-200 ns（锁争用）

3. **on_ready 回调执行**：
   - 回调在 `set_value` 线程中**同步**执行
   - 回调耗时直接影响生产者线程

### 可扩展性

**多消费者阻塞**：
- `notify_all()` 唤醒所有等待线程
- N 个线程同时 `wait()`，唤醒开销：O(N) × 线程调度延迟
- **最佳实践**：单一消费者，若需多播请使用 `on_ready` + 线程池分发

**多生产者竞争**：
- `set_value` 是幂等的，首次成功者生效
- 多线程同时调用 `set_value`：mutex 串行化，仅首个写入
- **建议**：单一生产者模型（一个 Promise 对应一个生产任务）

---

## 常见陷阱

### 1. Broken Promise 未检测

```cpp
// ❌ 错误：Promise 析构前未设置值
void bad_example() {
    mine::async::Promise<int> p;
    auto f = p.get_future();
    // Promise 在函数结束时析构，触发 broken
} // Future 持有者收到 StatusCode::Unknown 错误

// ✅ 正确：显式设置值或错误
void good_example() {
    mine::async::Promise<int> p;
    auto f = p.get_future();
    p.set_value(42); // 或 p.set_error(...)
}
```

### 2. on_ready 回调阻塞

```cpp
// ❌ 错误：回调中阻塞会延迟生产者
f.on_ready([&](auto r) {
    std::this_thread::sleep_for(std::chrono::seconds(10)); // 阻塞 10 秒
    // set_value 的线程会等待回调完成
});
p.set_value(42); // 被阻塞 10 秒

// ✅ 正确：回调中立即调度到其他线程
f.on_ready([&pool](auto r) {
    pool.enqueue([r = std::move(r)] {
        // 在线程池中处理，不阻塞生产者
        process_long_running(r);
    });
});
p.set_value(42); // 立即返回
```

### 3. 多次 get_future 调用

```cpp
// ❌ 错误：第二次调用返回空 Future
mine::async::Promise<int> p;
auto f1 = p.get_future(); // 有效
auto f2 = p.get_future(); // 空 Future（f2.valid() == false）

// ✅ 正确：仅调用一次，通过移动传递所有权
auto f = p.get_future();
auto f2 = std::move(f); // 转移所有权
```

### 4. 回调中捕获悬垂引用

```cpp
// ❌ 错误：lambda 捕获局部变量引用
void dangerous() {
    int local_value = 42;
    mine::async::Promise<int> p;
    auto f = p.get_future();
    
    f.on_ready([&local_value](auto r) { // 捕获引用
        std::cout << local_value; // 悬垂引用！
    });
    
    // 函数返回，local_value 被销毁
} // 回调执行时访问已销毁的 local_value

// ✅ 正确：按值捕获或使用 shared_ptr
f.on_ready([local_value](auto r) { // 按值捕获
    std::cout << local_value; // 安全
});
```

### 5. Future 析构前未等待结果

```cpp
// ⚠️ 潜在问题：Promise 设置值后无人消费
{
    mine::async::Promise<int> p;
    auto f = p.get_future();
    p.set_value(42);
    // f 析构，结果被丢弃（合法但可能非预期）
}

// ✅ 明确意图：若不需要结果，使用 on_ready 或明确忽略
auto f = p.get_future();
f.on_ready([](auto) {}); // 明确表示"不关心结果"
```

### 6. 死锁：回调中访问同一 Future

```cpp
// ❌ 死锁：回调持锁期间尝试访问 Future
f.on_ready([&f](auto r) {
    if (f.is_ready()) { // 尝试加锁 → 死锁（回调在持锁状态执行）
        // ...
    }
});

// ✅ 正确：回调中不访问原 Future
f.on_ready([](auto r) {
    // 仅使用 r，不访问 f
    process(r);
});
```

---

## 最佳实践

### 1. RAII 管理 Promise 生命周期

```cpp
class AsyncTask {
public:
    AsyncTask() : promise_(), future_(promise_.get_future()) {}
    
    mine::async::Future<int>& get_future() { return future_; }
    
    void execute() {
        try_execute_impl();
        if (!finished_) {
            promise_.set_error(Status{StatusCode::Internal, "Task incomplete"});
        }
    }
    
private:
    void try_execute_impl() {
        // 业务逻辑
        promise_.set_value(42);
        finished_ = true;
    }
    
    mine::async::Promise<int> promise_;
    mine::async::Future<int> future_;
    bool finished_ = false;
};
```

### 2. 超时等待模式（需结合其他机制）

```cpp
// Future 不直接支持超时，需配合 Timer 实现
auto f = p.get_future();

mine::async::Timer timer;
std::atomic<bool> timeout{false};

// 启动超时定时器
auto timeout_id = timer.set_timeout(std::chrono::seconds(5), [&timeout, &f] {
    if (!f.is_ready()) {
        timeout = true;
        // 无法取消 Future，但可以标记超时
    }
});

// 轮询或阻塞等待
while (!f.is_ready() && !timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

if (timeout) {
    // 处理超时
} else {
    auto r = f.get();
    timer.clear_timeout(timeout_id);
}
```

### 3. Promise 异常安全包装

```cpp
template<typename T, typename Func>
void execute_with_promise(mine::async::Promise<T> p, Func&& func) noexcept {
    try {
        auto result = func();
        p.set_value(std::move(result));
    } catch (const std::exception& e) {
        p.set_error(Status{StatusCode::Internal, e.what()});
    } catch (...) {
        p.set_error(Status{StatusCode::Unknown, "Unknown exception"});
    }
}

// 使用（注：项目禁用异常，此为示意）
mine::async::Promise<int> p;
execute_with_promise(std::move(p), [] { return compute(); });
```

### 4. 批量等待多个 Future

```cpp
template<typename T>
std::vector<mine::core::Result<T>> wait_all(std::vector<mine::async::Future<T>>& futures) {
    std::vector<mine::core::Result<T>> results;
    results.reserve(futures.size());
    
    for (auto& f : futures) {
        f.wait();
        results.push_back(f.get());
    }
    
    return results;
}

// 使用
std::vector<mine::async::Future<int>> futures;
// ... 填充 futures
auto results = wait_all(futures);
```

### 5. Future 链式转换（手动实现）

```cpp
template<typename T, typename U, typename Func>
mine::async::Future<U> then(mine::async::Future<T> f, Func&& func) {
    mine::async::Promise<U> p;
    auto result_future = p.get_future();
    
    f.on_ready([p = std::move(p), func = std::forward<Func>(func)](auto r) mutable {
        if (r.is_ok()) {
            auto transformed = func(r.value());
            p.set_value(std::move(transformed));
        } else {
            p.set_error(r.error());
        }
    });
    
    return result_future;
}

// 使用
auto f1 = get_int_future();
auto f2 = then(std::move(f1), [](int x) { return x * 2; });
auto r = f2.get(); // r.value() == 原值 * 2
```

---

## 高级应用场景

### 场景 1：HTTP 请求异步化

```cpp
class HttpClient {
public:
    mine::async::Future<std::string> fetch_async(const std::string& url) {
        mine::async::Promise<std::string> promise;
        auto future = promise.get_future();
        
        // 启动异步 I/O 线程
        io_thread_pool_.enqueue([url, p = std::move(promise)]() mutable {
            auto [status, body] = http_sync_request(url);
            if (status == 200) {
                p.set_value(std::move(body));
            } else {
                p.set_error(Status{StatusCode::Network, "HTTP " + std::to_string(status)});
            }
        });
        
        return future;
    }
    
private:
    mine::async::ThreadPool io_thread_pool_{4};
};

// 使用
HttpClient client;
auto f = client.fetch_async("https://example.com");

f.on_ready([](auto r) {
    if (r.is_ok()) {
        std::cout << "Response: " << r.value() << "\n";
    } else {
        std::cerr << "Error: " << r.error().message() << "\n";
    }
});
```

### 场景 2：数据库查询流水线

```cpp
struct User { int id; std::string name; };
struct Order { int user_id; double amount; };

class Database {
public:
    mine::async::Future<User> fetch_user(int user_id) {
        mine::async::Promise<User> p;
        auto f = p.get_future();
        
        query_pool_.enqueue([user_id, p = std::move(p)]() mutable {
            // 模拟数据库查询
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            p.set_value(User{user_id, "Alice"});
        });
        
        return f;
    }
    
    mine::async::Future<std::vector<Order>> fetch_orders(int user_id) {
        mine::async::Promise<std::vector<Order>> p;
        auto f = p.get_future();
        
        query_pool_.enqueue([user_id, p = std::move(p)]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            p.set_value(std::vector<Order>{{user_id, 99.99}, {user_id, 49.50}});
        });
        
        return f;
    }
    
private:
    mine::async::ThreadPool query_pool_{8};
};

// 使用：并行查询用户和订单
Database db;
auto user_future = db.fetch_user(123);
auto orders_future = db.fetch_orders(123);

// 等待两个结果
user_future.wait();
orders_future.wait();

auto user = user_future.get();
auto orders = orders_future.get();

if (user.is_ok() && orders.is_ok()) {
    std::cout << "User: " << user.value().name << "\n";
    std::cout << "Orders: " << orders.value().size() << "\n";
}
```

### 场景 3：UI 线程与后台任务通信

```cpp
class BackgroundProcessor {
public:
    mine::async::Future<std::vector<int>> process_large_dataset(const std::vector<int>& data) {
        mine::async::Promise<std::vector<int>> p;
        auto f = p.get_future();
        
        // 后台线程处理
        worker_pool_.enqueue([data, p = std::move(p)]() mutable {
            std::vector<int> result;
            result.reserve(data.size());
            
            // 模拟耗时计算
            for (int v : data) {
                result.push_back(v * v);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            p.set_value(std::move(result));
        });
        
        return f;
    }
    
private:
    mine::async::ThreadPool worker_pool_{std::thread::hardware_concurrency()};
};

// UI 线程使用
BackgroundProcessor processor;
auto data = std::vector<int>{1, 2, 3, 4, 5};

auto future = processor.process_large_dataset(data);

// 注册回调，在 UI Dispatcher 中更新界面
future.on_ready([&ui_dispatcher](auto r) {
    ui_dispatcher.dispatch([r = std::move(r)]() {
        if (r.is_ok()) {
            update_ui_with_results(r.value());
        } else {
            show_error_dialog(r.error().message());
        }
    });
});
```

### 场景 4：多阶段异步流水线

```cpp
// 模拟：下载图片 → 解码 → 应用滤镜 → 保存
class ImagePipeline {
public:
    mine::async::Future<std::string> process(const std::string& url) {
        // 阶段 1：下载
        auto download_future = http_client_.fetch_async(url);
        
        // 阶段 2：解码（手动实现 then）
        mine::async::Promise<Image> decode_promise;
        auto decode_future = decode_promise.get_future();
        
        download_future.on_ready([p = std::move(decode_promise)](auto r) mutable {
            if (r.is_ok()) {
                auto img = decode_image(r.value());
                p.set_value(std::move(img));
            } else {
                p.set_error(r.error());
            }
        });
        
        // 阶段 3：应用滤镜
        mine::async::Promise<Image> filter_promise;
        auto filter_future = filter_promise.get_future();
        
        decode_future.on_ready([p = std::move(filter_promise)](auto r) mutable {
            if (r.is_ok()) {
                auto filtered = apply_filter(r.value());
                p.set_value(std::move(filtered));
            } else {
                p.set_error(r.error());
            }
        });
        
        // 阶段 4：保存
        mine::async::Promise<std::string> save_promise;
        auto save_future = save_promise.get_future();
        
        filter_future.on_ready([p = std::move(save_promise)](auto r) mutable {
            if (r.is_ok()) {
                auto path = save_image(r.value());
                p.set_value(path);
            } else {
                p.set_error(r.error());
            }
        });
        
        return save_future;
    }
    
private:
    HttpClient http_client_;
    
    Image decode_image(const std::string& data) { /* ... */ return {}; }
    Image apply_filter(const Image& img) { /* ... */ return {}; }
    std::string save_image(const Image& img) { /* ... */ return "output.png"; }
};

// 使用
ImagePipeline pipeline;
auto result_future = pipeline.process("https://example.com/image.jpg");

result_future.on_ready([](auto r) {
    if (r.is_ok()) {
        std::cout << "Image saved to: " << r.value() << "\n";
    } else {
        std::cerr << "Pipeline failed: " << r.error().message() << "\n";
    }
});
```

---

## 与标准库对比

| 特性 | mine::async::Future | std::future |
|------|---------------------|-------------|
| 异常处理 | Result<T> 返回值 | get() 抛出异常 |
| 零异常保证 | ✅ | ❌ |
| 回调支持 | ✅ on_ready() | ❌（C++20 无） |
| Broken promise | StatusCode::Unknown | std::future_error::broken_promise |
| 是否阻塞析构 | ❌ | ✅（std::async 返回的 future） |
| 线程安全保证 | 明确文档化 | 部分未定义 |
| ABI 稳定性 | ✅（模板内联） | ❌（依赖 STL 实现） |
| void 类型支持 | 需哨兵类型 | ✅（特化） |

---

## 调试技巧

### 1. 检测 Broken Promise

```cpp
auto f = p.get_future();

f.on_ready([](auto r) {
    if (r.is_err() && r.error().code() == StatusCode::Unknown) {
        // Broken promise 发生
        std::cerr << "Warning: Promise destroyed without setting value!\n";
        // 打印堆栈跟踪（需调试器辅助）
    }
});
```

### 2. 记录 Future 状态转换

```cpp
class TrackedPromise {
public:
    TrackedPromise(std::string name) : name_(std::move(name)), promise_() {
        std::cout << "[" << name_ << "] Promise created\n";
    }
    
    ~TrackedPromise() {
        std::cout << "[" << name_ << "] Promise destroyed\n";
    }
    
    void set_value(int v) {
        std::cout << "[" << name_ << "] set_value(" << v << ")\n";
        promise_.set_value(v);
    }
    
    mine::async::Future<int> get_future() {
        std::cout << "[" << name_ << "] get_future()\n";
        return promise_.get_future();
    }
    
private:
    std::string name_;
    mine::async::Promise<int> promise_;
};
```

### 3. 检测悬垂回调

```cpp
// 使用 AddressSanitizer (ASan) 检测 use-after-free
// 编译时添加 /fsanitize=address (MSVC) 或 -fsanitize=address (GCC/Clang)

void test_dangling_callback() {
    mine::async::Promise<int> p;
    auto f = p.get_future();
    
    {
        int local = 42;
        f.on_ready([&local](auto r) {  // ASan 会检测到 local 已被销毁
            std::cout << local;
        });
    } // local 销毁
    
    p.set_value(100); // 触发回调 → ASan 报告 heap-use-after-free
}
```

---

## 扩展阅读

- [00-Async.md](00-Async.md)：mine.async 模块概述
- [02-Task.md](02-Task.md)：Task<T> 协程式异步模型
- [03-Dispatcher.md](03-Dispatcher.md)：MPSC 调度器与线程模型
- [04-ThreadPool.md](04-ThreadPool.md)：线程池任务提交
- [../core/07-Function.md](../core/07-Function.md)：Function<R(Args...)> 类型擦除包装器
- [../core/06-Result.md](../core/06-Result.md)：Result<T> 错误处理模式

---

**最后更新**：2026-06-12  
**适用版本**：MineFramework M0.1+

