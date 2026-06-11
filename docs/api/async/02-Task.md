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

### 基础协程

```cpp
mine::async::Task<int> simple_coroutine() {
    co_return 42;  // 直接返回值
}

// 调用
auto task = simple_coroutine();
auto result = task.get();      // Result<int>{ok_tag, 42}
```

### 链式 co_await

```cpp
mine::async::Task<std::string> fetch_user_name(int user_id) {
    // 模拟异步数据库查询
    co_return "Alice";
}

mine::async::Task<int> fetch_user_age(int user_id) {
    co_return 25;
}

mine::async::Task<std::string> get_user_info(int user_id) {
    auto name_result = co_await fetch_user_name(user_id);
    if (!name_result.is_ok()) {
        co_return "Unknown user";  // 错误处理
    }
    
    auto age_result = co_await fetch_user_age(user_id);
    if (!age_result.is_ok()) {
        co_return "Age unavailable";
    }
    
    co_return name_result.value() + ", age " + std::to_string(age_result.value());
}

// 使用
auto info_task = get_user_info(123);
auto info = info_task.get();  // "Alice, age 25"
```

### 错误传播

```cpp
mine::async::Task<int> may_fail(bool should_fail) {
    if (should_fail) {
        // 返回错误的Task
        co_return Task<int>::from_error(Status{StatusCode::InvalidArgument, "Failed"}).get().value();
        // 或者使用显式错误值（需业务约定）
    }
    co_return 100;
}

mine::async::Task<int> chain_with_error_handling() {
    auto r = co_await may_fail(true);
    if (!r.is_ok()) {
        // 错误恢复：返回默认值
        co_return -1;
    }
    co_return r.value() * 2;
}
```

### 并行协程组合（手动实现）

```cpp
mine::async::Task<int> compute1() { co_return 10; }
mine::async::Task<int> compute2() { co_return 20; }

mine::async::Task<int> parallel_sum() {
    // 启动两个任务（非阻塞）
    auto task1 = compute1();
    auto task2 = compute2();
    
    // 等待两个结果
    auto r1 = co_await task1;
    auto r2 = co_await task2;
    
    if (r1.is_ok() && r2.is_ok()) {
        co_return r1.value() + r2.value();  // 30
    }
    co_return -1;
}
```

---

## 性能特征

### 内存开销

**Task<T> 对象大小**：
- 仅包含 `Future<T> future_` 成员
- `sizeof(Task<T>)` = `sizeof(Future<T>)` = 8 字节（指针）

**协程帧大小**（取决于局部变量和捕获）：
| 协程特征 | 协程帧大小（约） |
|---------|-----------------|
| 简单 `co_return 42;` | ~64-128 字节 |
| 包含局部变量 | +局部变量大小 |
| 捕获外部变量（按值） | +捕获变量大小 |
| 多个 `co_await` | 每个挂起点增加 16-32 字节 |

**协程帧分配策略**：
- 编译器优化：小协程帧可能被内联到调用栈（HALO 优化）
- 大协程帧：堆分配（通过 `operator new`）

### 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| `Task<T>()` | O(1) | 空任务，无堆分配 |
| `from_value(v)` | O(1) + Promise分配 | 内部创建立即就绪的 Future |
| `map<U>(func)` | O(1) | 注册回调，延迟执行 transform |
| `co_await task` | O(1) + 阻塞 | 协程挂起/恢复开销 |
| `co_return value` | O(1) | 设置 Promise 值 |

### 协程开销

**挂起/恢复延迟**（Windows x64 / MSVC）：
- **挂起点（co_await）**：~20-50 ns（保存寄存器状态）
- **恢复（resume）**：~50-100 ns（恢复寄存器+跳转）
- **总往返延迟**：~70-150 ns

**与回调式对比**：
- **回调式 (`then`)**：~5-10 ns（直接函数调用）
- **协程式 (`co_await`)**：~70-150 ns（挂起/恢复开销）
- **权衡**：协程提供同步式代码可读性，开销增加 10-30倍（绝对值仍很小）

---

## 常见陷阱

### 1. 忘记 co_await 导致任务未执行

```cpp
// ❌ 错误：未 co_await，函数立即返回 Task，不等待结果
mine::async::Task<int> bad_example() {
    auto task = compute();  // task 立即返回
    // 缺少 co_await，compute() 未被等待
    co_return 42;           // 直接返回，compute() 结果丢失
}

// ✅ 正确：显式 co_await
mine::async::Task<int> good_example() {
    auto task = compute();
    auto r = co_await task;  // 等待结果
    co_return r.is_ok() ? r.value() : -1;
}
```

### 2. co_return 错误类型导致编译失败

```cpp
// ❌ 错误：co_return Result<T> 而非 T
mine::async::Task<int> wrong_return() {
    auto r = co_await compute();
    co_return r;  // 类型错误：r 是 Result<int>，不是 int
}

// ✅ 正确：co_return T 值
mine::async::Task<int> correct_return() {
    auto r = co_await compute();
    co_return r.is_ok() ? r.value() : -1;
}
```

### 3. Task 析构时协程帧泄漏（理论上不会）

```cpp
// MineFramework 的 Task 不会泄漏协程帧
// final_suspend 返回 suspend_never → 协程帧自动销毁

// 但若自定义协程类型返回 suspend_always，需手动 destroy：
// ⚠️ 假设某自定义协程类 CustomTask
CustomTask task = custom_coroutine();
// task 析构时，若 final_suspend=suspend_always，协程帧泄漏
// 需手动调用 handle.destroy() → MineFramework 的 Task 无此问题
```

### 4. 协程中捕获悬垂引用

```cpp
// ❌ 错误：按引用捕获局部变量
mine::async::Task<int> dangerous(int value) {
    auto func = [&value]() -> mine::async::Task<int> {  // 捕获引用
        co_await some_async_op();
        co_return value;  // 悬垂引用：value 可能已被销毁
    };
    co_return (co_await func()).value();
}

// ✅ 正确：按值捕获
auto func = [value]() -> mine::async::Task<int> {  // 捕获值
    co_await some_async_op();
    co_return value;  // 安全
};
```

### 5. 混用回调式和协程式导致混乱

```cpp
// ⚠️ 不推荐：混用 then 和 co_await
mine::async::Task<int> confusing() {
    auto task = compute();
    task.then([](auto r) {
        // 回调在其他线程执行
    });
    auto r = co_await task;  // 协程挂起等待
    // 回调和协程同时等待同一任务 → 语义不清晰
    co_return r.value();
}

// ✅ 推荐：统一使用协程式
mine::async::Task<int> clear_coroutine() {
    auto r = co_await compute();
    process(r);
    co_return r.value();
}

// ✅ 或统一使用回调式
void clear_callback() {
    compute().then([](auto r) {
        process(r);
    });
}
```

### 6. Task 生命周期过短导致协程未完成

```cpp
// ❌ 错误：Task 立即析构，协程未执行完
void fire_and_forget_bad() {
    auto task = long_running_coroutine();  // 返回 Task<int>
    // task 在函数结束时析构 → 协程被取消（Future 析构）
}

// ✅ 正确：保持 Task 存活直到完成
void fire_and_forget_good() {
    auto task = long_running_coroutine();
    task.then([](auto r) {
        // 回调确保任务完成
    });
    // 或将 task 移动到其他生命周期更长的位置
}
```

---

## 最佳实践

### 1. 优先使用协程式（提升可读性）

```cpp
// ✅ 推荐：协程式代码类似同步代码，易读易维护
mine::async::Task<std::string> fetch_user_profile(int id) {
    auto user_result = co_await database.fetch_user(id);
    if (!user_result.is_ok()) {
        co_return "User not found";
    }
    
    auto orders_result = co_await database.fetch_orders(user_result.value().id);
    if (!orders_result.is_ok()) {
        co_return "Orders unavailable";
    }
    
    co_return format_profile(user_result.value(), orders_result.value());
}

// ❌ 不推荐：回调式（回调地狱）
void fetch_user_profile_callback(int id, std::function<void(std::string)> callback) {
    database.fetch_user(id).then([=](auto user_result) {
        if (!user_result.is_ok()) {
            callback("User not found");
            return;
        }
        database.fetch_orders(user_result.value().id).then([=](auto orders_result) {
            if (!orders_result.is_ok()) {
                callback("Orders unavailable");
                return;
            }
            callback(format_profile(user_result.value(), orders_result.value()));
        });
    });
}
```

### 2. 使用 from_value/from_error 包装同步结果

```cpp
// ✅ 统一接口：同步和异步函数返回相同类型
mine::async::Task<int> may_be_async(bool use_async) {
    if (use_async) {
        co_return (co_await expensive_async_compute()).value();
    } else {
        // 同步计算，包装为 Task
        co_return 42;  // 或 Task<int>::from_value(42).get().value()
    }
}
```

### 3. 错误集中处理模式

```cpp
mine::async::Task<int> with_error_handling() {
    auto r1 = co_await step1();
    auto r2 = co_await step2();
    auto r3 = co_await step3();
    
    // 集中错误检查
    if (!r1.is_ok()) co_return -1;
    if (!r2.is_ok()) co_return -2;
    if (!r3.is_ok()) co_return -3;
    
    co_return r1.value() + r2.value() + r3.value();
}
```

### 4. 使用 map 链式变换（Functional 风格）

```cpp
auto result_task = fetch_data()
    .map<int>([](Result<std::string> r) -> int {
        return r.is_ok() ? std::stoi(r.value()) : 0;
    })
    .map<std::string>([](Result<int> r) -> std::string {
        return r.is_ok() ? "Result: " + std::to_string(r.value()) : "Error";
    });

auto final_result = result_task.get();
```

### 5. 并行任务批量等待

```cpp
mine::async::Task<std::vector<int>> parallel_fetch_all(const std::vector<int>& ids) {
    std::vector<Task<int>> tasks;
    for (int id : ids) {
        tasks.push_back(fetch_item(id));
    }
    
    std::vector<int> results;
    for (auto& task : tasks) {
        auto r = co_await task;
        if (r.is_ok()) {
            results.push_back(r.value());
        }
    }
    
    co_return results;
}
```

---

## 高级应用场景

### 场景 1：MVVM 命令异步执行

```cpp
class ViewModel {
public:
    mine::async::Task<void> on_save_button_clicked() {
        // 1. 在后台线程验证数据
        auto validation_result = co_await thread_pool.enqueue_task([this] {
            return validate_data();
        });
        
        if (!validation_result.is_ok()) {
            dispatcher.dispatch([this] {
                error_message = "Validation failed";
            });
            co_return;
        }
        
        // 2. 异步保存到数据库
        auto save_result = co_await database.save_user(current_user);
        
        // 3. 回到 UI 线程更新界面
        dispatcher.dispatch([this, save_result] {
            if (save_result.is_ok()) {
                status_message = "Saved successfully";
            } else {
                status_message = "Save failed: " + save_result.error().message();
            }
        });
    }
    
private:
    mine::async::ThreadPool thread_pool{4};
    mine::async::Dispatcher dispatcher;
    mine::data::Database database;
    User current_user;
    std::string error_message;
    std::string status_message;
};
```

### 场景 2：HTTP 请求流水线

```cpp
struct HttpResponse { int status; std::string body; };

mine::async::Task<HttpResponse> http_get(const std::string& url);
mine::async::Task<json> parse_json(const std::string& body);

mine::async::Task<std::vector<User>> fetch_users_from_api() {
    // 1. 发起 HTTP 请求
    auto http_result = co_await http_get("https://api.example.com/users");
    if (!http_result.is_ok() || http_result.value().status != 200) {
        co_return std::vector<User>{};  // 空列表
    }
    
    // 2. 解析 JSON
    auto json_result = co_await parse_json(http_result.value().body);
    if (!json_result.is_ok()) {
        co_return std::vector<User>{};
    }
    
    // 3. 转换为领域对象
    std::vector<User> users;
    for (const auto& item : json_result.value()["users"]) {
        users.push_back(User{item["id"], item["name"]});
    }
    
    co_return users;
}
```

### 场景 3：数据库事务（协程式）

```cpp
mine::async::Task<bool> transfer_money(int from_id, int to_id, double amount) {
    // 1. 开始事务
    auto tx_result = co_await database.begin_transaction();
    if (!tx_result.is_ok()) {
        co_return false;
    }
    
    auto tx = tx_result.value();
    
    // 2. 扣款
    auto debit_result = co_await tx.execute_query(
        "UPDATE accounts SET balance = balance - ? WHERE id = ?",
        {amount, from_id}
    );
    
    if (!debit_result.is_ok()) {
        co_await tx.rollback();
        co_return false;
    }
    
    // 3. 入款
    auto credit_result = co_await tx.execute_query(
        "UPDATE accounts SET balance = balance + ? WHERE id = ?",
        {amount, to_id}
    );
    
    if (!credit_result.is_ok()) {
        co_await tx.rollback();
        co_return false;
    }
    
    // 4. 提交事务
    auto commit_result = co_await tx.commit();
    co_return commit_result.is_ok();
}
```

### 场景 4：文件批量上传（进度报告）

```cpp
struct UploadProgress { int completed; int total; };

mine::async::Task<std::vector<std::string>> upload_files(
    const std::vector<std::filesystem::path>& files,
    std::function<void(UploadProgress)> progress_callback
) {
    std::vector<std::string> uploaded_urls;
    int completed = 0;
    
    for (const auto& file : files) {
        // 异步上传单个文件
        auto upload_result = co_await upload_single_file(file);
        
        if (upload_result.is_ok()) {
            uploaded_urls.push_back(upload_result.value());
        }
        
        // 更新进度
        ++completed;
        progress_callback(UploadProgress{completed, static_cast<int>(files.size())});
    }
    
    co_return uploaded_urls;
}

// 使用
auto task = upload_files(file_list, [](UploadProgress p) {
    std::cout << p.completed << "/" << p.total << " files uploaded\n";
});

auto urls = task.get();
```

---

## 协程调试技巧

### 1. 使用命名协程帧（便于调试器识别）

```cpp
// 添加调试友好的名称
mine::async::Task<int> [[maybe_unused]] fetch_user_by_id_DEBUG(int id) {
    auto result = co_await database.fetch_user(id);
    co_return result.is_ok() ? result.value().age : -1;
}
```

### 2. 打印协程状态

```cpp
mine::async::Task<int> debug_coroutine() {
    std::cout << "[DEBUG] Coroutine started\n";
    
    auto r1 = co_await step1();
    std::cout << "[DEBUG] step1 completed: " << (r1.is_ok() ? "OK" : "ERROR") << "\n";
    
    auto r2 = co_await step2();
    std::cout << "[DEBUG] step2 completed: " << (r2.is_ok() ? "OK" : "ERROR") << "\n";
    
    co_return r1.value() + r2.value();
}
```

### 3. Visual Studio 协程调试窗口

在 Visual Studio 中调试协程时：
- **调用堆栈窗口**：显示挂起的协程帧
- **局部变量窗口**：显示协程局部变量（包括捕获的变量）
- **反汇编窗口**：查看协程挂起点的机器码

---

## 与其他异步模型对比

| 特性 | Task<T> (回调) | Task<T> (协程) | Future<T> | JavaScript Promise |
|------|----------------|----------------|-----------|-------------------|
| 链式组合 | ✅ `then` / `map` | ✅ `co_await` | ❌ | ✅ `.then()` |
| 错误传播 | 手动检查 Result | 手动检查 Result | Result<T> | 自动（throw） |
| 代码可读性 | 中（回调嵌套） | 高（同步风格） | 低（阻塞等待） | 高 |
| 性能开销 | 低（~10ns） | 中（~100ns） | 低 | N/A |
| 学习曲线 | 低 | 中（需理解协程） | 低 | 低 |

---

## 扩展阅读

- [01-Future-Promise.md](01-Future-Promise.md)：Future<T>/Promise<T> 底层机制
- [03-Dispatcher.md](03-Dispatcher.md)：MPSC 调度器与 UI 线程集成
- [04-ThreadPool.md](04-ThreadPool.md)：线程池任务提交
- [../core/07-Function.md](../core/07-Function.md)：Function<R(Args...)> 回调包装
- [../core/06-Result.md](../core/06-Result.md)：Result<T> 错误处理模式

---

**最后更新**：2026-06-12  
**适用版本**：MineFramework M0.1+


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
