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

---

## 内部实现机制

### 双队列交换策略

`Dispatcher` 使用**双缓冲队列**避免长时间持锁：

```cpp
// 简化的内部结构（实际使用 PIMPL）
struct Dispatcher::Impl {
    std::mutex mutex_;
    std::vector<Function<void()>> incoming_;   // 生产者队列
    std::vector<Function<void()>> processing_; // 消费者队列
};
```

**关键流程**：

1. **post(fn)** - 生产者线程：
   ```cpp
   void post(Function<void()> fn) noexcept {
       std::lock_guard lock(mutex_);
       incoming_.push_back(std::move(fn));
   }
   ```
   - 时间：O(1) + 锁开销（~20-50 ns）
   - 线程安全：多个线程可并发 post

2. **dispatch()** - 消费者线程：
   ```cpp
   uint32_t dispatch() noexcept {
       uint32_t count = 0;
       while (true) {
           // 阶段 1：快速交换队列（持锁时间 < 1 µs）
           {
               std::lock_guard lock(mutex_);
               if (incoming_.empty()) break;
               std::swap(incoming_, processing_);
           }
           
           // 阶段 2：无锁执行回调（可能耗时）
           for (auto& fn : processing_) {
               fn();
               ++count;
           }
           processing_.clear();
       }
       return count;
   }
   ```
   - **优势**：回调执行期间不持锁，允许并发 post
   - **嵌套支持**：回调中 post 的任务在同次 dispatch 中被处理（循环检查 incoming_）

### PIMPL 实现细节

**动机**：
- 隐藏 `std::mutex` 和 `std::vector` 的实现细节（避免头文件依赖）
- 保持 ABI 稳定性（修改实现无需重新编译客户端代码）

**内存布局**：
- `Dispatcher` 对象：8 字节（`Pimpl<Impl>` 指针）
- `Impl` 堆分配：~40 字节（mutex） + 2 * 24 字节（vector） ≈ **88 字节**
- 每个排队任务：32 字节（Function SBO） + 可能的堆分配

### 嵌套 dispatch 的实现原理

```cpp
dispatcher.post([&]() noexcept {
    dispatcher.post(task2);  // 嵌套 post
    task1();
});

dispatcher.dispatch();
// 执行顺序：task1 → task2（两者在同次 dispatch 中完成）
```

**关键机制**：
- `dispatch()` 使用 `while (!incoming_.empty())` 循环
- 回调执行期间，嵌套 post 写入 `incoming_`
- 下一次循环迭代检测到 `incoming_` 非空，继续交换+执行
- 直到 `incoming_` 为空才返回

---

## 性能特征

### 操作开销

| 操作 | 时间复杂度 | 典型耗时 | 说明 |
|------|-----------|---------|------|
| `post(fn)` | O(1) + 锁 | 30-100 ns | 取决于锁竞争 |
| `dispatch()` | O(N) | N * 回调耗时 | N = 排队任务数 |
| `dispatch_one()` | O(1) + 锁 | ~50 ns + 回调耗时 | 单任务版本 |
| `pending_count()` | O(1) + 锁 | 20-50 ns | 仅读取 size() |

### 锁竞争分析

**低竞争场景**（推荐）：
- 单个/少数后台线程 → UI 线程
- `post` 频率 < 1000 次/秒
- **开销**：锁获取 ~20-30 ns，可忽略

**高竞争场景**（需优化）：
- 多个线程频繁 post（> 10,000 次/秒）
- **开销**：锁获取 50-200 ns，可能成为瓶颈
- **优化方案**：
  - 批量 post（合并多个小任务）
  - 使用无锁队列（需自行实现）

### 队列容量和扩展

- **初始容量**：`std::vector` 默认为空（无预分配）
- **扩展策略**：指数增长（1.5x 或 2x，取决于编译器）
- **内存峰值**：N * 32 字节（N = 历史最大排队数）

**示例**：100 个排队任务 → 至少 3.2 KB 内存

---

## 常见陷阱

### 1. 回调中访问已销毁对象

```cpp
// ❌ 错误：ViewModel 生命周期短于回调
void bad_example() {
    ViewModel vm;
    
    std::thread([&dispatcher, &vm]() {
        dispatcher.post([&vm]() noexcept {
            vm.update();  // 悬垂引用：vm 可能已销毁
        });
    }).detach();
    
    // vm 在函数结束时销毁
}

// ✅ 正确：使用 shared_ptr 延长生命周期
void good_example() {
    auto vm = std::make_shared<ViewModel>();
    
    std::thread([dispatcher, vm]() {
        dispatcher.post([vm]() noexcept {
            vm->update();  // 安全：shared_ptr 保证生命周期
        });
    }).detach();
}
```

### 2. 回调抛出异常导致 dispatch 中断

```cpp
// ❌ 错误：未捕获异常
dispatcher.post([]() {
    throw std::runtime_error("错误");  // terminate!
});

// ✅ 正确：所有回调必须 noexcept
dispatcher.post([]() noexcept {
    try {
        risky_operation();
    } catch (...) {
        log_error("操作失败");
    }
});
```

**说明**：MineFramework 禁用异常（/EHs-c-），但第三方库可能抛出异常。始终用 `noexcept` 标记回调。

### 3. dispatch 线程与 UI 线程不一致

```cpp
// ❌ 错误：在工作线程调用 dispatch
mine::async::Dispatcher ui_dispatcher;

std::thread worker([&]() {
    ui_dispatcher.dispatch();  // 错误：应在 UI 线程调用
}).detach();

// ✅ 正确：仅在 UI 线程调用 dispatch
while (app_running) {
    ui_dispatcher.dispatch();  // UI 线程主循环
    process_events();
}
```

### 4. dispatch 后忘记清理 Dispatcher 导致回调泄漏

```cpp
// ⚠️ 注意：Dispatcher 析构时会销毁排队的回调
{
    mine::async::Dispatcher d;
    d.post(expensive_callback);
    // 如果这里没有 dispatch()，expensive_callback 在 d 析构时被丢弃
} // d 析构 → expensive_callback 的捕获变量被释放
```

### 5. 循环 post 导致无限递归

```cpp
// ❌ 错误：无限递归
mine::async::Dispatcher d;

std::function<void()> recursive_task;
recursive_task = [&]() noexcept {
    d.post(recursive_task);  // 无限递归：每次执行都 post 自己
};

d.post(recursive_task);
d.dispatch();  // 永不结束（直到栈溢出或队列耗尽内存）

// ✅ 正确：添加终止条件
int count = 0;
std::function<void()> limited_task;
limited_task = [&]() noexcept {
    if (++count < 10) {
        d.post(limited_task);
    }
};
```

### 6. 死锁：回调中等待 dispatch 结果

```cpp
// ❌ 死锁：dispatch 在等待回调完成，回调在等待另一任务
mine::async::Dispatcher d;

d.post([&]() noexcept {
    // 此回调在 dispatch 中执行，若再次调用 dispatch 会死锁（若使用递归锁）
    // 或阻塞（若嵌套 dispatch 不支持）
    d.dispatch();  // 危险！
});

d.dispatch();
```

**解决方案**：避免在回调中调用 `dispatch()`。

---

## 最佳实践

### 1. UI 线程集成模式

```cpp
class Application {
public:
    Application() : running_(true) {}

    void run() {
        while (running_) {
            // 1. 处理异步任务
            ui_dispatcher_.dispatch();
            
            // 2. 处理定时器
            timer_.tick();
            
            // 3. 处理窗口消息
            process_window_messages();
            
            // 4. 渲染帧
            render_frame();
        }
    }

    mine::async::Dispatcher& ui_dispatcher() { return ui_dispatcher_; }

private:
    mine::async::Dispatcher ui_dispatcher_;
    mine::async::Timer timer_;
    bool running_;
};
```

### 2. 批量 dispatch 优化

```cpp
// ✅ 推荐：定期批量 dispatch（减少 dispatch 调用开销）
while (app_running) {
    // 每帧统一 dispatch 一次（16.6 ms @ 60 FPS）
    uint32_t processed = dispatcher.dispatch();
    if (processed > 0) {
        std::printf("处理了 %u 个任务\n", processed);
    }
    
    render_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
```

### 3. 错误处理包装器

```cpp
// 统一的错误处理包装
template<typename Fn>
mine::core::Function<void()> safe_dispatch_task(Fn&& fn) {
    return [fn = std::forward<Fn>(fn)]() noexcept {
        try {
            fn();
        } catch (const std::exception& e) {
            log_error("Dispatcher 回调异常: {}", e.what());
        } catch (...) {
            log_error("Dispatcher 回调未知异常");
        }
    };
}

// 使用
dispatcher.post(safe_dispatch_task([vm] {
    vm->risky_update();
}));
```

### 4. 生命周期管理：shared_ptr + weak_ptr

```cpp
class Component : public std::enable_shared_from_this<Component> {
public:
    void schedule_update() {
        // 使用 weak_ptr 避免循环引用
        std::weak_ptr<Component> weak_this = shared_from_this();
        
        dispatcher_.post([weak_this]() noexcept {
            if (auto self = weak_this.lock()) {
                self->do_update();  // 安全：检查对象是否存活
            }
        });
    }

private:
    void do_update() { /* ... */ }
    mine::async::Dispatcher& dispatcher_;
};
```

### 5. dispatch_one 用于时间受限场景

```cpp
// 场景：每帧处理最多 N 个任务，避免卡顿
void frame_loop() {
    const int max_tasks_per_frame = 5;
    
    for (int i = 0; i < max_tasks_per_frame; ++i) {
        if (!dispatcher.dispatch_one()) {
            break;  // 队列为空
        }
    }
    
    render_frame();  // 确保每帧渲染
}
```

---

## 高级应用场景

### 场景 1：MVVM 属性变更通知到 UI

```cpp
class ViewModel {
public:
    ViewModel(mine::async::Dispatcher& ui_dispatcher)
        : ui_dispatcher_(ui_dispatcher) {}

    void set_username(std::string_view name) {
        // 1. 在后台线程验证
        thread_pool_.enqueue([this, name = std::string(name)]() {
            auto validation_result = validate_username(name);
            
            // 2. 切回 UI 线程更新界面
            ui_dispatcher_.post([this, validation_result]() noexcept {
                if (validation_result.is_ok()) {
                    username_ = validation_result.value();
                    notify_property_changed("Username");
                } else {
                    error_message_ = validation_result.error().message();
                }
            });
        });
    }

private:
    mine::async::Dispatcher& ui_dispatcher_;
    mine::async::ThreadPool thread_pool_{4};
    std::string username_;
    std::string error_message_;
};
```

### 场景 2：动画帧调度器

```cpp
class AnimationScheduler {
public:
    AnimationScheduler(mine::async::Dispatcher& dispatcher)
        : dispatcher_(dispatcher) {}

    void start_animation(std::function<bool(double)> frame_callback) {
        auto start_time = std::chrono::steady_clock::now();
        
        schedule_next_frame(start_time, std::move(frame_callback));
    }

private:
    void schedule_next_frame(
        std::chrono::steady_clock::time_point start_time,
        std::function<bool(double)> callback
    ) {
        dispatcher_.post([this, start_time, callback = std::move(callback)]() noexcept {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start_time).count();
            
            // 执行动画帧回调
            bool continue_animation = callback(elapsed);
            
            if (continue_animation) {
                // 递归调度下一帧
                schedule_next_frame(start_time, std::move(callback));
            }
        });
    }

    mine::async::Dispatcher& dispatcher_;
};

// 使用
AnimationScheduler scheduler(ui_dispatcher);
scheduler.start_animation([](double t) -> bool {
    update_animation(t);
    return t < 2.0;  // 动画持续 2 秒
});
```

### 场景 3：HTTP 响应流水线

```cpp
class HttpClient {
public:
    void fetch_user_data(
        int user_id,
        mine::async::Dispatcher& ui_dispatcher,
        std::function<void(UserData)> ui_callback
    ) {
        // 1. 在 IO 线程发起请求
        io_thread_pool_.enqueue([=, &ui_dispatcher]() {
            auto response = http_get("/api/users/" + std::to_string(user_id));
            
            if (response.status == 200) {
                // 2. 在工作线程解析 JSON
                compute_thread_pool_.enqueue([=, &ui_dispatcher, body = response.body]() {
                    UserData user_data = parse_json(body);
                    
                    // 3. 切回 UI 线程调用回调
                    ui_dispatcher.post([ui_callback, user_data]() noexcept {
                        ui_callback(user_data);
                    });
                });
            }
        });
    }

private:
    mine::async::ThreadPool io_thread_pool_{2};
    mine::async::ThreadPool compute_thread_pool_{4};
};

// 使用
http_client.fetch_user_data(123, ui_dispatcher, [](UserData data) {
    display_user_profile(data);  // 安全：在 UI 线程执行
});
```

### 场景 4：多线程日志收集器

```cpp
class ThreadSafeLogger {
public:
    ThreadSafeLogger() {
        // 日志写入线程
        log_thread_ = std::thread([this]() {
            while (running_) {
                log_dispatcher_.dispatch();  // 批量写入日志
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    ~ThreadSafeLogger() {
        running_ = false;
        if (log_thread_.joinable()) log_thread_.join();
    }

    void log(std::string_view message) {
        // 任意线程可调用
        log_dispatcher_.post([this, msg = std::string(message)]() noexcept {
            write_to_file(msg);
        });
    }

private:
    void write_to_file(const std::string& msg) {
        // 串行写入，避免文件锁竞争
        std::fprintf(log_file_, "%s\n", msg.c_str());
    }

    mine::async::Dispatcher log_dispatcher_;
    std::thread log_thread_;
    std::atomic<bool> running_{true};
    FILE* log_file_ = std::fopen("app.log", "a");
};
```

---

## 调试技巧

### 1. 监控队列积压

```cpp
void monitor_dispatcher(mine::async::Dispatcher& d) {
    uint32_t pending = d.pending_count();
    if (pending > 100) {
        std::fprintf(stderr, "警告：Dispatcher 积压 %u 个任务\n", pending);
    }
}

// 在主循环中定期检查
while (app_running) {
    monitor_dispatcher(ui_dispatcher);
    ui_dispatcher.dispatch();
}
```

### 2. 回调执行追踪

```cpp
// 包装器：记录回调执行时间
mine::core::Function<void()> traced_task(std::string name, std::function<void()> fn) {
    return [name = std::move(name), fn = std::move(fn)]() noexcept {
        auto start = std::chrono::steady_clock::now();
        fn();
        auto end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration<double, std::milli>(end - start).count();
        std::printf("[TRACE] %s 执行耗时: %.2f ms\n", name.c_str(), duration);
    };
}

// 使用
dispatcher.post(traced_task("更新用户界面", [&]() {
    update_ui();
}));
```

### 3. Visual Studio 断点调试

在 Visual Studio 中调试 Dispatcher：
- **断点位置**：`Dispatcher::post` / `Dispatcher::dispatch` 入口
- **监视变量**：`p_->incoming_.size()`, `p_->processing_.size()`
- **调用堆栈**：查看回调调用链（F9 在回调中设置断点）

---

## 与其他调度模型对比

| 特性 | Dispatcher (MPSC) | ThreadPool | std::async | Qt QMetaObject::invokeMethod |
|------|-------------------|------------|-----------|------------------------------|
| 线程模型 | 多生产者→单消费者 | 多任务→多工作线程 | 单任务→单线程 | 信号槽跨线程调用 |
| 典型用途 | UI 线程集成 | CPU 密集计算 | 一次性异步任务 | Qt UI 更新 |
| 队列管理 | 手动 dispatch | 自动调度 | 自动执行 | 自动排队 |
| 开销 | 极低（~50 ns） | 中（线程池） | 高（线程创建） | 中（Qt 事件循环） |
| 嵌套支持 | ✅ | ❌ | ❌ | ✅ |

---

## 扩展阅读

- [05-Timer.md](05-Timer.md)：定时器如何使用 Dispatcher 调度回调
- [04-ThreadPool.md](04-ThreadPool.md)：CPU/IO 任务执行器
- [02-Task.md](02-Task.md)：Task 如何与 Dispatcher 结合实现 UI 线程协程
- [../core/07-Function.md](../core/07-Function.md)：Function<void()> 回调包装器实现
- [../ui.app/00-AppLoop.md](../ui.app/00-AppLoop.md)：应用主循环中的 Dispatcher 集成

---

**最后更新**：2026-06-12  
**适用版本**：MineFramework M0.1+
