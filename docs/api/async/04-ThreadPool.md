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
