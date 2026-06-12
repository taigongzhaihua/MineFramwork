# ThreadPool 详细接口文档

## 概述

`ThreadPool` 是 `mine.async` 模块的固定大小工作线程池，用于并行执行 CPU 密集型或 IO 阻塞型任务。构造时创建指定数量的工作线程，析构时自动汇合。

### 核心特性

- **固定线程数**：初始化时创建，不动态扩缩
- **单任务队列**：所有线程从同一队列取任务（简化实现）
- **Future 支持**：`enqueue(fn)` 返回 `Future<T>` 异步获取结果
- **解绑任务**：`enqueue_detached(fn)` 提交无返回值任务
- **轮询等待**：`wait_all()` 非持锁轮询，避免死锁

### 设计动机

MineFramework 的线程模型（见 `01-architecture.md`）定义了 IO/任务池为进程级线程池。`ThreadPool` 为上层模块（网络、数据库、MVVM 后台命令）提供可复用的工作线程。

### 典型使用场景

| 场景 | 用法 |
|------|------|
| 并行计算 | `pool.enqueue(heavy_compute)` → 返回 Future |
| 后台 IO | `pool.enqueue_detached(file_io_task)` |
| MVVM 命令 | `pool.enqueue(viewmodel_command)` |
| 批量任务 | 循环 enqueue → 最后 `pool.wait_all()` |

---

## 类定义

```cpp
namespace mine::async {

class MINE_ASYNC_API ThreadPool {
public:
    explicit ThreadPool(uint32_t num_threads = 0) noexcept;
    ThreadPool(ThreadPool&& other) noexcept;
    ThreadPool& operator=(ThreadPool&& other) noexcept;
    ~ThreadPool() noexcept;

    ThreadPool(const ThreadPool&) = delete;

    template<typename F>
    [[nodiscard]] auto enqueue(F&& fn) noexcept -> Future<decltype(fn())>;

    void enqueue_detached(mine::core::Function<void()> fn) noexcept;

    [[nodiscard]] uint32_t thread_count() const noexcept;
    [[nodiscard]] uint32_t pending_count() const noexcept;
    void wait_all() noexcept;

private:
    struct Impl;
    mine::core::Pimpl<Impl> p_;
};

} // namespace mine::async
```

---

## 构造函数

### ThreadPool(uint32_t)

```cpp
explicit ThreadPool(uint32_t num_threads = 0) noexcept;
```

**描述**：构造线程池并启动工作线程。

**参数**：
- `num_threads`：工作线程数。`0` 表示使用 `std::thread::hardware_concurrency()`，若获取失败则回退到 4。

**时间复杂度**：O(num_threads) — 创建线程。

**析构**：通知所有线程退出 → 等待汇合。析构时未完成的任务将被丢弃。

---

## 任务提交

### enqueue (模板)

```cpp
template<typename F>
[[nodiscard]] auto enqueue(F&& fn) noexcept -> Future<decltype(fn())>;
```

**描述**：提交有返回值的任务，返回 `Future<R>` 用于异步获取结果。

**类型推导**：`R = decltype(fn())`，由编译器自动推导。

**约束**：`fn` 的调用必须为 `noexcept`（项目禁用异常）。

**SBO**：内部使用 `mine::core::Function` 包装，≤ 32 字节捕获走 SBO 快速路径，> 32 字节自动堆分配。

**示例**：

```cpp
mine::async::ThreadPool pool(4);

// 返回 Future<int>
auto f1 = pool.enqueue([]() noexcept -> int {
    return heavy_compute();
});

// 获取结果（阻塞等待）
auto result = f1.get();
if (result.ok()) {
    use(result.value());
}
```

### enqueue_detached

```cpp
void enqueue_detached(mine::core::Function<void()> fn) noexcept;
```

**描述**：提交无返回值的"发射后不管"任务。

**与 `enqueue` 的区别**：不返回 Future，调用方无法获取结果或等待完成。

---

## 状态查询与控制

### thread_count

```cpp
[[nodiscard]] uint32_t thread_count() const noexcept;
```

**描述**：获取工作线程数。

### pending_count

```cpp
[[nodiscard]] uint32_t pending_count() const noexcept;
```

**描述**：获取当前队列中待处理任务数（近似值）。

### wait_all

```cpp
void wait_all() noexcept;
```

**描述**：阻塞等待所有已提交任务完成。

**实现**：轮询 `tasks_.empty() && active_count_ == 0`，不持锁等待（避免死锁）。

**注意**：`wait_all()` 返回后，其他线程仍可能正在提交新任务（这些不会被等待）。

---

## 线程模型

```
         enqueue(fn) / enqueue_detached(fn)
                      │
              ┌───────▼────────┐
              │   任务队列       │  (mutex + cv)
              └───┬───┬───┬────┘
                  │   │   │
          ┌───────┘   │   └───────┐
          ▼           ▼           ▼
      Worker 1    Worker 2    Worker N
          │           │           │
          └───────────┴───────────┘
                      │
                 active_count_ (atomic)
```

- 工作线程通过 `cv.wait()` 在队列为空时阻塞
- `enqueue` 通过 `cv.notify_one()` 唤醒一个工作线程
- 析构时设置 `running_ = false` → `cv.notify_all()` → `join()`

---

## 完整示例

```cpp
#include <mine/async/Async.h>

void threadpool_example() {
    mine::async::ThreadPool pool(4);
    std::atomic<int> sum{0};

    // 批量提交 100 个任务
    for (int i = 0; i < 100; ++i) {
        pool.enqueue_detached([&sum, i]() noexcept {
            sum.fetch_add(i);
        });
    }

    // 等待全部完成
    pool.wait_all();

    std::printf("总和: %d\n", sum.load());  // 4950
}
```

---

## 内部实现机制

### 任务队列与工作线程

**内部结构**（简化）：

```cpp
struct ThreadPool::Impl {
    std::vector<std::thread> workers_;                 // 工作线程
    std::queue<Function<void()>> tasks_;               // 任务队列
    std::mutex mutex_;                                 // 保护 tasks_
    std::condition_variable cv_;                       // 唤醒工作线程
    std::atomic<uint32_t> active_count_{0};            // 正在执行的任务数
    std::atomic<bool> running_{true};                  // 运行标志
};
```

**工作线程主循环**：

```cpp
void worker_loop() {
    while (true) {
        Function<void()> task;
        
        // 阶段 1：从队列取任务（持锁）
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] {
                return !running_ || !tasks_.empty();
            });
            
            if (!running_ && tasks_.empty()) {
                break;  // 线程池关闭
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        // 阶段 2：执行任务（不持锁）
        active_count_.fetch_add(1);
        task();
        active_count_.fetch_sub(1);
    }
}
```

**enqueue 实现细节**：

```cpp
template<typename F>
auto enqueue(F&& fn) noexcept -> Future<decltype(fn())> {
    using R = decltype(fn());
    
    // 创建 Promise/Future 对
    Promise<R> promise;
    auto future = promise.get_future();
    
    // 包装任务：执行 fn 并设置 Promise
    auto task = [promise = std::move(promise), fn = std::forward<F>(fn)]() mutable noexcept {
        if constexpr (std::is_void_v<R>) {
            fn();
            promise.set_value();
        } else {
            promise.set_value(fn());
        }
    };
    
    // 提交到队列
    {
        std::lock_guard lock(mutex_);
        tasks_.emplace(std::move(task));
    }
    cv_.notify_one();
    
    return future;
}
```

### 析构与线程汇合

```cpp
~ThreadPool() noexcept {
    // 1. 通知所有线程退出
    running_.store(false);
    cv_.notify_all();
    
    // 2. 等待所有线程汇合
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // 3. 丢弃未执行的任务（tasks_ 自动清空）
}
```

**关键点**：
- 未执行的任务（队列中的任务）在析构时被丢弃
- 正在执行的任务会被允许完成
- `wait_all()` 必须在析构前显式调用（若需确保所有任务完成）

### wait_all 实现原理

```cpp
void wait_all() noexcept {
    while (true) {
        uint32_t pending;
        {
            std::lock_guard lock(mutex_);
            pending = tasks_.size();
        }
        
        if (pending == 0 && active_count_.load() == 0) {
            break;  // 队列为空且无活跃任务
        }
        
        std::this_thread::yield();  // 释放 CPU，避免自旋
    }
}
```

**为何不用 condition_variable**：
- 避免死锁：若调用线程也是工作线程，cv.wait() 会导致死锁
- 轮询开销可接受：通常 wait_all() 仅在批量任务完成时调用

---

## 性能特征

### 内存开销

| 组件 | 大小（约） | 说明 |
|------|-----------|------|
| `ThreadPool` 对象 | 8 字节 | Pimpl 指针 |
| `Impl` 结构 | ~120 字节 | workers_ + mutex + cv + atomics |
| 每个工作线程栈 | 1 MB | Windows 默认栈大小 |
| 每个排队任务 | 32+ 字节 | Function SBO + 可能的堆分配 |

**示例**：4 线程池 ≈ 4 MB（线程栈） + 120 字节（控制结构）

### 时间复杂度

| 操作 | 时间复杂度 | 典型耗时 | 说明 |
|------|-----------|---------|------|
| `enqueue(fn)` | O(1) + 锁 | 100-500 ns | 创建 Promise + 队列插入 + 通知 |
| `enqueue_detached(fn)` | O(1) + 锁 | 50-200 ns | 仅队列插入 + 通知 |
| `wait_all()` | O(N * T) | N * 任务耗时 | N = 任务数，T = 平均耗时 |
| 析构 | O(M) | M * 线程汇合 | M = 工作线程数 |

### 任务调度延迟

**测量**：从 enqueue 到任务开始执行的延迟

| 场景 | 延迟 | 说明 |
|------|------|------|
| 有空闲线程 | 1-10 µs | cv.notify_one() 唤醒开销 |
| 所有线程繁忙 | 等待下一任务完成 | 取决于任务耗时 |
| 高竞争（多线程 enqueue） | 10-100 µs | 锁竞争开销 |

### 扩展性分析

**CPU 密集型任务**：
- **最优线程数** = CPU 核心数（避免上下文切换）
- **示例**：8 核 CPU → `ThreadPool pool(8)`

**IO 密集型任务**：
- **最优线程数** = 2-4 * CPU 核心数（线程阻塞时允许更多并发）
- **示例**：8 核 CPU → `ThreadPool pool(16-32)`

---

## 常见陷阱

### 1. 忘记 wait_all 导致任务未完成

```cpp
// ❌ 错误：提交任务后立即析构
void bad_example() {
    mine::async::ThreadPool pool(4);
    
    for (int i = 0; i < 100; ++i) {
        pool.enqueue_detached([=] { process(i); });
    }
    
    // pool 析构 → 未执行的任务被丢弃
}

// ✅ 正确：显式等待完成
void good_example() {
    mine::async::ThreadPool pool(4);
    
    for (int i = 0; i < 100; ++i) {
        pool.enqueue_detached([=] { process(i); });
    }
    
    pool.wait_all();  // 确保所有任务完成
}
```

### 2. Future 未检查导致错误被忽略

```cpp
// ❌ 错误：未检查 Future 结果
mine::async::ThreadPool pool(4);
auto f = pool.enqueue([] { return risky_operation(); });
// Future 未调用 get() → 错误被忽略

// ✅ 正确：检查结果
auto result = f.get();
if (!result.is_ok()) {
    handle_error(result.error());
}
```

### 3. 捕获引用导致悬垂

```cpp
// ❌ 错误：按引用捕获局部变量
void dangerous() {
    std::vector<int> data = {1, 2, 3};
    
    pool.enqueue_detached([&data]() noexcept {
        process(data);  // 悬垂引用：data 可能已销毁
    });
    
    // data 在函数结束时销毁
}

// ✅ 正确：按值捕获
void safe() {
    std::vector<int> data = {1, 2, 3};
    
    pool.enqueue_detached([data]() noexcept {
        process(data);  // 安全：拷贝
    });
}

// ✅ 更好：使用 shared_ptr 避免拷贝大对象
void better() {
    auto data = std::make_shared<std::vector<int>>(1000000);
    
    pool.enqueue_detached([data]() noexcept {
        process(*data);  // 安全 + 高效
    });
}
```

### 4. 线程池大小设置不当

```cpp
// ❌ 错误：线程过多导致上下文切换开销
mine::async::ThreadPool pool(100);  // 可能远超 CPU 核心数

// ✅ 正确：基于硬件并发度
uint32_t num_threads = std::thread::hardware_concurrency();
mine::async::ThreadPool pool(num_threads);  // 或 0（自动检测）
```

### 5. wait_all 在工作线程中调用导致死锁

```cpp
// ❌ 死锁：工作线程等待自己
mine::async::ThreadPool pool(4);

pool.enqueue([&pool]() noexcept {
    pool.wait_all();  // 死锁：等待包括自己的所有任务
});
```

### 6. enqueue 返回的 Future 被立即丢弃

```cpp
// ⚠️ 警告：Future 析构不影响任务执行，但无法获取结果
pool.enqueue([] { return compute(); });  // Future 被丢弃
// 任务仍然执行，但结果无法获取

// 若不需要结果，使用 enqueue_detached
pool.enqueue_detached([] { side_effect_only(); });
```

---

## 最佳实践

### 1. 使用 RAII 确保任务完成

```cpp
class ScopedThreadPool {
public:
    explicit ScopedThreadPool(uint32_t threads) : pool_(threads) {}
    
    ~ScopedThreadPool() {
        pool_.wait_all();  // 析构时自动等待
    }
    
    mine::async::ThreadPool& get() { return pool_; }

private:
    mine::async::ThreadPool pool_;
};

// 使用
void process_batch() {
    ScopedThreadPool pool(4);
    
    for (auto& item : items) {
        pool.get().enqueue_detached([&item] { process(item); });
    }
    
    // 函数结束时自动等待所有任务完成
}
```

### 2. 批量收集 Future 结果

```cpp
std::vector<mine::async::Future<int>> futures;

for (int i = 0; i < 10; ++i) {
    futures.push_back(pool.enqueue([i] { return compute(i); }));
}

// 收集所有结果
std::vector<int> results;
for (auto& future : futures) {
    auto r = future.get();
    if (r.is_ok()) {
        results.push_back(r.value());
    }
}
```

### 3. 限流：避免任务队列过长

```cpp
void throttled_enqueue(mine::async::ThreadPool& pool, Function<void()> task) {
    // 等待队列长度降到阈值以下
    while (pool.pending_count() > 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    pool.enqueue_detached(std::move(task));
}
```

### 4. 异常安全包装（针对第三方库）

```cpp
template<typename F>
auto safe_enqueue(mine::async::ThreadPool& pool, F&& fn) {
    return pool.enqueue([fn = std::forward<F>(fn)]() noexcept -> decltype(fn()) {
        try {
            return fn();
        } catch (const std::exception& e) {
            log_error("ThreadPool 任务异常: {}", e.what());
            if constexpr (!std::is_void_v<decltype(fn())>) {
                return {};  // 返回默认值
            }
        }
    });
}
```

### 5. 分离 CPU 密集和 IO 密集任务

```cpp
class TaskExecutor {
public:
    TaskExecutor()
        : cpu_pool_(std::thread::hardware_concurrency()),  // CPU 核心数
          io_pool_(4 * std::thread::hardware_concurrency())  // 4 倍核心数
    {}

    mine::async::ThreadPool& cpu_pool() { return cpu_pool_; }
    mine::async::ThreadPool& io_pool() { return io_pool_; }

private:
    mine::async::ThreadPool cpu_pool_;
    mine::async::ThreadPool io_pool_;
};

// 使用
executor.cpu_pool().enqueue([] { return heavy_compute(); });
executor.io_pool().enqueue([] { return database_query(); });
```

---

## 高级应用场景

### 场景 1：Map-Reduce 并行计算

```cpp
// Map: 并行转换
template<typename T, typename Fn>
std::vector<decltype(std::declval<Fn>()(std::declval<T>()))> parallel_map(
    mine::async::ThreadPool& pool,
    const std::vector<T>& input,
    Fn transform
) {
    using R = decltype(transform(input[0]));
    std::vector<mine::async::Future<R>> futures;
    
    for (const auto& item : input) {
        futures.push_back(pool.enqueue([&item, transform] {
            return transform(item);
        }));
    }
    
    std::vector<R> results;
    for (auto& f : futures) {
        auto r = f.get();
        if (r.is_ok()) {
            results.push_back(r.value());
        }
    }
    
    return results;
}

// Reduce: 合并结果
int parallel_sum(mine::async::ThreadPool& pool, const std::vector<int>& data) {
    std::atomic<int> sum{0};
    
    for (int value : data) {
        pool.enqueue_detached([&sum, value]() noexcept {
            sum.fetch_add(value);
        });
    }
    
    pool.wait_all();
    return sum.load();
}

// 使用
mine::async::ThreadPool pool(8);
std::vector<int> input = {1, 2, 3, ..., 1000};

// Map: 平方
auto squares = parallel_map(pool, input, [](int x) { return x * x; });

// Reduce: 求和
int total = parallel_sum(pool, squares);
```

### 场景 2：递归并行（分治算法）

```cpp
// 并行快速排序
void parallel_quicksort(
    mine::async::ThreadPool& pool,
    std::vector<int>& data,
    int left,
    int right,
    int depth = 0
) {
    if (left >= right) return;
    
    int pivot = partition(data, left, right);
    
    // 递归深度 < 4 时继续并行，否则转串行（避免过度并行）
    if (depth < 4) {
        auto f1 = pool.enqueue([&, left, pivot, depth] {
            parallel_quicksort(pool, data, left, pivot - 1, depth + 1);
        });
        
        auto f2 = pool.enqueue([&, pivot, right, depth] {
            parallel_quicksort(pool, data, pivot + 1, right, depth + 1);
        });
        
        f1.get();
        f2.get();
    } else {
        // 串行执行（避免线程池过载）
        parallel_quicksort(pool, data, left, pivot - 1, depth + 1);
        parallel_quicksort(pool, data, pivot + 1, right, depth + 1);
    }
}
```

### 场景 3：生产者-消费者管道

```cpp
class Pipeline {
public:
    Pipeline(mine::async::ThreadPool& pool) : pool_(pool) {}

    void add_item(int item) {
        pool_.enqueue_detached([this, item]() noexcept {
            // 阶段 1：预处理
            auto processed = preprocess(item);
            
            // 阶段 2：主处理
            pool_.enqueue_detached([this, processed]() noexcept {
                auto result = main_process(processed);
                
                // 阶段 3：后处理
                pool_.enqueue_detached([this, result]() noexcept {
                    postprocess(result);
                });
            });
        });
    }

private:
    mine::async::ThreadPool& pool_;
};
```

### 场景 4：文件批量处理（进度报告）

```cpp
struct ProcessResult {
    int succeeded;
    int failed;
};

ProcessResult process_files_parallel(
    mine::async::ThreadPool& pool,
    const std::vector<std::filesystem::path>& files,
    std::function<void(int, int)> progress_callback
) {
    std::atomic<int> succeeded{0};
    std::atomic<int> failed{0};
    std::atomic<int> completed{0};
    
    int total = static_cast<int>(files.size());
    
    for (const auto& file : files) {
        pool.enqueue_detached([&, file]() noexcept {
            if (process_file(file)) {
                succeeded.fetch_add(1);
            } else {
                failed.fetch_add(1);
            }
            
            int done = completed.fetch_add(1) + 1;
            progress_callback(done, total);
        });
    }
    
    pool.wait_all();
    
    return ProcessResult{succeeded.load(), failed.load()};
}

// 使用
auto result = process_files_parallel(pool, file_list, [](int done, int total) {
    std::printf("进度: %d/%d\n", done, total);
});
std::printf("成功: %d, 失败: %d\n", result.succeeded, result.failed);
```

---

## 调试技巧

### 1. 监控线程池状态

```cpp
void monitor_pool(const mine::async::ThreadPool& pool) {
    std::printf("线程数: %u, 排队任务: %u\n",
        pool.thread_count(),
        pool.pending_count());
}

// 定期打印
while (processing) {
    monitor_pool(pool);
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

### 2. 任务执行追踪

```cpp
template<typename F>
auto traced_enqueue(mine::async::ThreadPool& pool, std::string name, F&& fn) {
    return pool.enqueue([name = std::move(name), fn = std::forward<F>(fn)]() noexcept {
        auto start = std::chrono::steady_clock::now();
        std::printf("[START] %s\n", name.c_str());
        
        auto result = fn();
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start).count();
        std::printf("[END] %s (%.2f ms)\n", name.c_str(), duration);
        
        return result;
    });
}

// 使用
traced_enqueue(pool, "重计算", [] { return heavy_compute(); });
```

### 3. 死锁检测

```cpp
// 超时版 wait_all（避免无限等待）
bool wait_all_with_timeout(mine::async::ThreadPool& pool, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        if (pool.pending_count() == 0) {
            return true;  // 完成
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();
        
        if (elapsed > timeout_ms) {
            return false;  // 超时
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 使用
if (!wait_all_with_timeout(pool, 5000)) {
    std::fprintf(stderr, "警告：线程池 5 秒内未完成所有任务\n");
}
```

---

## 与其他执行器对比

| 特性 | ThreadPool | std::async | Dispatcher | OpenMP |
|------|-----------|-----------|------------|--------|
| 线程管理 | 固定线程池 | 动态创建 | 单线程MPSC | 编译器管理 |
| 任务调度 | FIFO 队列 | 立即执行 | 手动 dispatch | 自动（并行循环） |
| 结果获取 | Future<T> | std::future<T> | 回调 | 无（共享变量） |
| 开销 | 低（复用线程） | 高（创建线程） | 极低 | 中 |
| 适用场景 | 通用并行任务 | 一次性任务 | UI 更新 | 数据并行 |

---

## 扩展阅读

- [01-Future-Promise.md](01-Future-Promise.md)：enqueue 返回的 Future<T> 详细说明
- [02-Task.md](02-Task.md)：Task 如何与 ThreadPool 结合实现协程式后台任务
- [03-Dispatcher.md](03-Dispatcher.md)：将 ThreadPool 结果传回 UI 线程
- [../core/07-Function.md](../core/07-Function.md)：Function<R(Args...)> 回调包装器
- [../mvvm/00-MVVM.md](../mvvm/00-MVVM.md)：MVVM 命令如何使用 ThreadPool

---

**最后更新**：2026-06-12  
**适用版本**：MineFramework M0.1+
