# Timer 详细接口文档

## 概述

`Timer` 是 `mine.async` 模块的轻量定时器管理器，在同一线程内管理延迟和周期性定时器。使用单调时钟（`std::chrono::steady_clock`）计时，内部以固定大小数组存储定时器条目，无堆分配。

### 核心特性

- **内联存储**：最多 32 个同时活跃定时器，固定数组无堆分配
- **单调时钟**：基于 `steady_clock`，不受系统时间调整影响
- **不创建线程**：依赖外部调用 `tick()` 驱动
- **Dispatcher 集成**：`set_timeout_on` / `set_interval_on` 自动投递到目标线程
- **回调安全**：触发时先取出回调再执行，避免回调中修改条目导致迭代器失效

### 设计动机

动画系统（`mine.ui.animation.AnimationClock`）和通用定时需求（延迟执行、超时检测、心跳）都需要轻量定时器。`Timer` 提供了一个不依赖操作系统定时器 API 的纯用户态实现。

### 典型使用场景

| 场景 | 用法 |
|------|------|
| 动画驱动 | `timer.set_interval(tick_animation, 16)` — 每 16ms |
| 延迟执行 | `timer.set_timeout(on_timeout, 1000)` — 1 秒后 |
| 超时检测 | `timer.set_timeout(on_timeout, 5000)` — 5 秒超时 |
| UI 线程回调 | `timer.set_timeout_on(dispatcher, update_ui, 100)` |

---

## 类定义

```cpp
namespace mine::async {

using TimerHandle = uint32_t;
inline constexpr TimerHandle kInvalidTimerHandle = 0u;

class MINE_ASYNC_API Timer {
public:
    static constexpr uint32_t kMaxTimers = 32u;

    Timer() noexcept;
    ~Timer() noexcept;

    Timer(const Timer&)            = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&)                 = delete;
    Timer& operator=(Timer&&)      = delete;

    // 基本定时器
    [[nodiscard]] TimerHandle set_timeout(
        mine::core::Function<void()> callback, uint32_t delay_ms) noexcept;
    [[nodiscard]] TimerHandle set_interval(
        mine::core::Function<void()> callback, uint32_t interval_ms) noexcept;

    // Dispatcher 集成
    [[nodiscard]] TimerHandle set_timeout_on(
        Dispatcher& dispatcher,
        mine::core::Function<void()> callback, uint32_t delay_ms) noexcept;
    [[nodiscard]] TimerHandle set_interval_on(
        Dispatcher& dispatcher,
        mine::core::Function<void()> callback, uint32_t interval_ms) noexcept;

    // 管理
    void clear(TimerHandle handle) noexcept;
    void clear_all() noexcept;
    [[nodiscard]] bool is_active(TimerHandle handle) const noexcept;

    // 驱动
    uint32_t tick() noexcept;
    [[nodiscard]] uint32_t active_count() const noexcept;

private:
    struct Entry;
    Entry* entries_;
    uint32_t capacity_;
    uint32_t active_count_;
    uint64_t last_tick_time_;
};

} // namespace mine::async
```

---

## 定时器设置

### set_timeout

```cpp
[[nodiscard]] TimerHandle set_timeout(
    mine::core::Function<void()> callback,
    uint32_t delay_ms) noexcept;
```

**描述**：设置一次性延迟定时器。到期后触发回调一次，然后自动清除。

**参数**：
- `callback`：到期时调用的回调（`void() noexcept`）
- `delay_ms`：延迟毫秒数（必须 > 0）

**返回值**：
- 有效 `TimerHandle`（非 0）：成功
- `kInvalidTimerHandle`（0）：无空闲槽位或参数无效

**示例**：
```cpp
mine::async::Timer timer;
auto h = timer.set_timeout([]() noexcept {
    std::printf("1 秒后执行\n");
}, 1000);
```

### set_interval

```cpp
[[nodiscard]] TimerHandle set_interval(
    mine::core::Function<void()> callback,
    uint32_t interval_ms) noexcept;
```

**描述**：设置周期性定时器。每隔 `interval_ms` 触发一次，直到被 `clear()` 取消。

**回调重用**：触发时将回调移出→执行→移回，保持周期性可用。

---

### set_timeout_on

```cpp
[[nodiscard]] TimerHandle set_timeout_on(
    Dispatcher& dispatcher,
    mine::core::Function<void()> callback,
    uint32_t delay_ms) noexcept;
```

**描述**：设置一次性定时器，回调通过 `Dispatcher::post` 在目标线程执行。

**生命周期**：调用方须保证 `Dispatcher` 生命周期长于本定时器。

### set_interval_on

```cpp
[[nodiscard]] TimerHandle set_interval_on(
    Dispatcher& dispatcher,
    mine::core::Function<void()> callback,
    uint32_t interval_ms) noexcept;
```

**描述**：周期性版本。使用 `std::shared_ptr<Function<void()>>` 共享回调，每次触发创建新闭包投递。

---

## 定时器管理

### clear

```cpp
void clear(TimerHandle handle) noexcept;
```

**描述**：取消指定定时器。幂等：无效句柄静默忽略。

### clear_all

```cpp
void clear_all() noexcept;
```

**描述**：取消所有定时器。

### is_active

```cpp
[[nodiscard]] bool is_active(TimerHandle handle) const noexcept;
```

**描述**：检查指定定时器是否活跃（尚未触发且未被取消）。

---

## 驱动方法

### tick

```cpp
uint32_t tick() noexcept;
```

**描述**：推进定时器，触发所有到期回调。必须在拥有线程中定期调用。

**实现细节**：
1. 获取当前时间 `now_ms`
2. 遍历所有活跃条目，检查 `now_ms >= next_fire_ms`
3. 一次性定时器：移出回调→清除条目→执行回调
4. 周期性定时器：移出回调→更新下次触发时间→执行回调→移回回调

**返回值**：本次触发的定时器数量。

### active_count

```cpp
[[nodiscard]] uint32_t active_count() const noexcept;
```

**描述**：获取当前活跃定时器数量。

---

## 线程安全

`Timer` **不提供内部线程安全**。所有方法必须在调用 `tick()` 的同一线程使用。

| 方法 | 线程约束 |
|------|---------|
| `set_timeout` / `set_interval` | 与 `tick()` 同线程 |
| `set_timeout_on` / `set_interval_on` | 与 `tick()` 同线程 |
| `clear` / `clear_all` | 与 `tick()` 同线程 |
| `tick` | 拥有线程 |

---

## 与 Dispatcher 集成示例

```cpp
mine::async::Dispatcher ui_dispatcher;
mine::async::Timer timer;

// 一次性：1 秒后在 UI 线程更新
timer.set_timeout_on(ui_dispatcher, []() noexcept {
    show_notification("完成");
}, 1000);

// 周期性：每 500ms 更新动画
timer.set_interval_on(ui_dispatcher, []() noexcept {
    advance_animation_frame();
}, 500);

// 主循环
while (running) {
    timer.tick();
    ui_dispatcher.dispatch();
    render();
}
```

---

## 完整示例

```cpp
#include <mine/async/Async.h>

void timer_example() {
    mine::async::Timer timer;
    int counter = 0;

    // 周期性定时器
    auto h = timer.set_interval([&]() noexcept {
        ++counter;
        std::printf("tick %d\n", counter);
    }, 100);

    // 模拟 10 次 tick
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        timer.tick();
    }

    timer.clear(h);
    std::printf("最终计数: %d\n", counter);  // 约 10
}
```

---

## 内部实现机制

### Entry 数据结构

```cpp
struct Timer::Entry {
    mine::core::Function<void()> callback;   // 回调函数
    Dispatcher* dispatcher;                  // 可选：目标调度器
    uint64_t next_fire_ms;                   // 下次触发时间（单调时钟）
    uint32_t interval_ms;                    // 间隔（0 = 一次性）
    TimerHandle handle;                      // 句柄
    bool active;                             // 活跃标志
};
```

**存储方式**：
- 固定数组：`Entry entries_[32]`（栈分配，无堆开销）
- 空闲槽位：`active == false`
- 句柄生成：递增计数器（非 0，避免与 kInvalidTimerHandle 冲突）

### tick 实现细节

```cpp
uint32_t tick() noexcept {
    uint64_t now_ms = get_steady_time_ms();  // steady_clock::now()
    uint32_t fired_count = 0;
    
    for (uint32_t i = 0; i < capacity_; ++i) {
        Entry& entry = entries_[i];
        if (!entry.active) continue;
        
        if (now_ms >= entry.next_fire_ms) {
            // 取出回调（避免回调中修改 entries_ 导致迭代器失效）
            auto callback = std::move(entry.callback);
            
            if (entry.interval_ms == 0) {
                // 一次性定时器：清除
                entry.active = false;
                --active_count_;
            } else {
                // 周期性定时器：更新下次触发时间
                entry.next_fire_ms = now_ms + entry.interval_ms;
                
                // 回调移动走了，需要克隆（for set_interval）
                // 实际实现中 set_interval 使用 shared_ptr<Function> 共享
            }
            
            // 执行回调
            if (entry.dispatcher) {
                // 通过 Dispatcher 投递
                entry.dispatcher->post(std::move(callback));
            } else {
                // 直接执行
                callback();
            }
            
            ++fired_count;
        }
    }
    
    last_tick_time_ = now_ms;
    return fired_count;
}
```

### set_interval_on 的共享回调机制

**问题**：周期性定时器需要在每次触发后保留回调，但 `Function<void()>` 是 move-only。

**解决方案**：使用 `std::shared_ptr<Function<void()>>` 包装回调：

```cpp
TimerHandle set_interval_on(Dispatcher& dispatcher, Function<void()> callback, uint32_t interval_ms) {
    // 包装为共享指针
    auto shared_cb = std::make_shared<Function<void()>>(std::move(callback));
    
    // 每次触发时创建新闭包
    auto timer_cb = [&dispatcher, shared_cb]() noexcept {
        dispatcher.post([shared_cb]() noexcept {
            (*shared_cb)();  // 解引用并调用
        });
    };
    
    return set_interval(std::move(timer_cb), interval_ms);
}
```

### 单调时钟保证

**为何使用 steady_clock**：
- **系统时间调整免疫**：用户修改系统时间不影响定时器精度
- **不可倒流**：`now()` 单调递增，避免时间戳比较错误

**时间戳获取**：
```cpp
uint64_t get_steady_time_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
}
```

---

## 性能特征

### 内存开销

| 组件 | 大小（约） | 说明 |
|------|-----------|------|
| `Timer` 对象 | 32 + 32*80 ≈ **2.6 KB** | 固定数组 + 元数据 |
| 每个 Entry | ~80 字节 | Function(32B) + 时间戳(8B) + 指针(8B) + 其他 |
| 最大同时定时器 | 32 个 | 编译期常量 |

**优势**：
- 无堆分配（对比：`std::map<id, Entry>` 每条目约 48 字节堆开销）
- Cache 友好（连续内存，遍历效率高）

### 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| `set_timeout` | O(N) | 线性查找空闲槽位（N=32） |
| `set_interval` | O(N) | 同上 |
| `tick()` | O(N) | 遍历所有条目检查到期 |
| `clear(handle)` | O(N) | 线性查找句柄 |
| `is_active(handle)` | O(N) | 同上 |

**N = 32（常量）**，实际开销极小（~100-500 ns）。

### 定时精度

**理论精度**：取决于 `tick()` 调用频率

| tick 间隔 | 精度（最坏情况） | 说明 |
|----------|-----------------|------|
| 16 ms (60 FPS) | ±16 ms | 动画场景 |
| 1 ms | ±1 ms | 高精度场景 |
| 100 ms | ±100 ms | 低频率场景 |

**示例**：设置 100ms 超时，若 tick 每 16ms 调用一次，实际触发时间在 96-112ms 之间。

**系统定时器对比**：
- `Timer` 精度：取决于 tick 频率（用户态）
- `std::this_thread::sleep_for`：取决于操作系统调度器（内核态，通常 1-15ms）
- 高精度定时器（`timeBeginPeriod`）：1ms（需特权）

---

## 常见陷阱

### 1. 忘记调用 tick 导致定时器不触发

```cpp
// ❌ 错误：设置定时器后未调用 tick
mine::async::Timer timer;
timer.set_timeout([]() { std::printf("永不触发\n"); }, 1000);
// 缺少 timer.tick()

// ✅ 正确：主循环中定期 tick
while (running) {
    timer.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
```

### 2. tick 间隔过长导致精度下降

```cpp
// ❌ 精度差：tick 每秒一次，100ms 定时器误差 ±1000ms
while (running) {
    timer.tick();
    std::this_thread::sleep_for(std::chrono::seconds(1));  // 太长
}

// ✅ 精度好：tick 频率 >> 定时器间隔
while (running) {
    timer.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 适中
}
```

### 3. 回调中修改定时器导致未定义行为

```cpp
// ⚠️ 危险：回调中清除其他定时器（可能影响迭代）
mine::async::Timer timer;
TimerHandle h1, h2;

h1 = timer.set_timeout([&]() noexcept {
    timer.clear(h2);  // 危险：修改正在遍历的数据结构
}, 100);

h2 = timer.set_timeout([]() noexcept { /* ... */ }, 200);

// 实际上 MineFramework 的实现是安全的（先移出回调再执行）
// 但不推荐在回调中频繁修改定时器
```

### 4. 超过 32 个定时器导致添加失败

```cpp
// ❌ 错误：未检查返回值
mine::async::Timer timer;
for (int i = 0; i < 50; ++i) {
    auto h = timer.set_timeout([]() {}, 1000);
    // h 在 i >= 32 时为 kInvalidTimerHandle（0）
}

// ✅ 正确：检查容量或返回值
if (timer.active_count() < Timer::kMaxTimers) {
    timer.set_timeout(callback, delay);
} else {
    handle_timer_pool_full();
}
```

### 5. Dispatcher 生命周期短于 Timer

```cpp
// ❌ 错误：Dispatcher 先析构
void bad_example() {
    mine::async::Timer timer;
    {
        mine::async::Dispatcher dispatcher;
        timer.set_timeout_on(dispatcher, []() {}, 1000);
    } // dispatcher 析构
    
    timer.tick();  // 悬垂引用：dispatcher 已销毁
}

// ✅ 正确：确保 Dispatcher 生命周期
mine::async::Dispatcher dispatcher;  // 外层
mine::async::Timer timer;

timer.set_timeout_on(dispatcher, []() {}, 1000);
timer.tick();
```

### 6. set_interval 回调耗时过长导致积压

```cpp
// ❌ 问题：回调耗时 500ms，但间隔仅 100ms
timer.set_interval([]() noexcept {
    expensive_operation();  // 500 ms
}, 100);

// 结果：每次 tick 触发多个积压任务，导致雪崩

// ✅ 解决方案：使用 ThreadPool 异步执行
timer.set_interval([&pool]() noexcept {
    pool.enqueue_detached([] { expensive_operation(); });
}, 100);
```

---

## 最佳实践

### 1. 主循环集成模式

```cpp
class Application {
public:
    void run() {
        while (running_) {
            // 1. 驱动定时器
            timer_.tick();
            
            // 2. 处理异步任务
            ui_dispatcher_.dispatch();
            
            // 3. 渲染帧
            render_frame();
            
            // 4. 帧率限制
            frame_limiter_.sleep_until_next_frame();
        }
    }

private:
    mine::async::Timer timer_;
    mine::async::Dispatcher ui_dispatcher_;
    bool running_ = true;
};
```

### 2. 自动取消：RAII 包装器

```cpp
class ScopedTimer {
public:
    ScopedTimer(mine::async::Timer& timer, Function<void()> callback, uint32_t delay_ms)
        : timer_(timer), handle_(timer.set_timeout(std::move(callback), delay_ms))
    {}
    
    ~ScopedTimer() {
        if (handle_ != kInvalidTimerHandle) {
            timer_.clear(handle_);
        }
    }
    
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&& other) noexcept
        : timer_(other.timer_), handle_(other.handle_)
    {
        other.handle_ = kInvalidTimerHandle;
    }

private:
    mine::async::Timer& timer_;
    TimerHandle handle_;
};

// 使用
{
    ScopedTimer guard(timer, []() { timeout_action(); }, 5000);
    // 5 秒内未完成操作 → 触发超时
    if (operation_completed()) {
        // 提前完成，guard 析构自动取消定时器
    }
} // 离开作用域自动取消
```

### 3. 重试机制（指数退避）

```cpp
void retry_with_backoff(
    mine::async::Timer& timer,
    Function<bool()> operation,
    uint32_t max_retries = 5
) {
    uint32_t attempt = 0;
    uint32_t delay_ms = 100;
    
    std::function<void()> retry_fn;
    retry_fn = [&, attempt, delay_ms]() mutable noexcept {
        if (operation()) {
            return;  // 成功
        }
        
        if (++attempt >= max_retries) {
            log_error("重试失败：达到最大次数");
            return;
        }
        
        // 指数退避：100ms, 200ms, 400ms, 800ms, 1600ms
        delay_ms *= 2;
        timer.set_timeout([retry_fn]() { retry_fn(); }, delay_ms);
    };
    
    retry_fn();  // 首次尝试
}
```

### 4. 防抖（Debounce）

```cpp
class Debouncer {
public:
    Debouncer(mine::async::Timer& timer, uint32_t delay_ms)
        : timer_(timer), delay_ms_(delay_ms)
    {}

    void trigger(Function<void()> callback) {
        // 取消旧定时器
        if (handle_ != kInvalidTimerHandle) {
            timer_.clear(handle_);
        }
        
        // 设置新定时器
        handle_ = timer_.set_timeout(std::move(callback), delay_ms_);
    }

private:
    mine::async::Timer& timer_;
    uint32_t delay_ms_;
    TimerHandle handle_ = kInvalidTimerHandle;
};

// 使用：搜索框输入防抖
Debouncer search_debouncer(timer, 300);

on_text_changed([&](std::string_view text) {
    search_debouncer.trigger([text = std::string(text)] {
        perform_search(text);  // 仅在停止输入 300ms 后执行
    });
});
```

### 5. 节流（Throttle）

```cpp
class Throttler {
public:
    Throttler(mine::async::Timer& timer, uint32_t interval_ms)
        : timer_(timer), interval_ms_(interval_ms)
    {}

    void trigger(Function<void()> callback) {
        if (handle_ != kInvalidTimerHandle) {
            return;  // 节流中，忽略
        }
        
        // 立即执行
        callback();
        
        // 设置冷却定时器
        handle_ = timer_.set_timeout([this]() noexcept {
            handle_ = kInvalidTimerHandle;  // 冷却结束
        }, interval_ms_);
    }

private:
    mine::async::Timer& timer_;
    uint32_t interval_ms_;
    TimerHandle handle_ = kInvalidTimerHandle;
};

// 使用：窗口 resize 节流
Throttler resize_throttler(timer, 100);

on_window_resize([&](Size new_size) {
    resize_throttler.trigger([new_size] {
        relayout_ui(new_size);  // 最多每 100ms 执行一次
    });
});
```

---

## 高级应用场景

### 场景 1：动画系统集成

```cpp
class AnimationClock {
public:
    AnimationClock(mine::async::Timer& timer)
        : timer_(timer)
    {
        // 60 FPS 动画帧
        timer_handle_ = timer_.set_interval([this]() noexcept {
            tick_animations();
        }, 16);  // ~60 FPS
    }

    ~AnimationClock() {
        timer_.clear(timer_handle_);
    }

    void add_animation(Animation* anim) {
        animations_.push_back(anim);
    }

private:
    void tick_animations() {
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = animations_.begin(); it != animations_.end(); ) {
            if ((*it)->update(now)) {
                ++it;  // 继续动画
            } else {
                it = animations_.erase(it);  // 完成，移除
            }
        }
    }

    mine::async::Timer& timer_;
    TimerHandle timer_handle_;
    std::vector<Animation*> animations_;
};
```

### 场景 2：心跳检测（健康检查）

```cpp
class HeartbeatMonitor {
public:
    HeartbeatMonitor(mine::async::Timer& timer, uint32_t timeout_ms)
        : timer_(timer), timeout_ms_(timeout_ms)
    {
        reset_watchdog();
    }

    void reset_watchdog() {
        if (watchdog_handle_ != kInvalidTimerHandle) {
            timer_.clear(watchdog_handle_);
        }
        
        watchdog_handle_ = timer_.set_timeout([this]() noexcept {
            on_timeout();
        }, timeout_ms_);
    }

    void on_heartbeat_received() {
        reset_watchdog();  // 收到心跳，重置定时器
    }

private:
    void on_timeout() {
        std::fprintf(stderr, "错误：心跳超时\n");
        reconnect();
    }

    mine::async::Timer& timer_;
    uint32_t timeout_ms_;
    TimerHandle watchdog_handle_ = kInvalidTimerHandle;
};

// 使用
HeartbeatMonitor monitor(timer, 5000);  // 5 秒超时

// 网络线程收到心跳包
on_network_packet([&](Packet packet) {
    if (packet.type == PacketType::Heartbeat) {
        monitor.on_heartbeat_received();
    }
});
```

### 场景 3：延迟初始化（Lazy Loading）

```cpp
class LazyResourceLoader {
public:
    LazyResourceLoader(mine::async::Timer& timer, Dispatcher& ui_dispatcher)
        : timer_(timer), ui_dispatcher_(ui_dispatcher)
    {}

    void schedule_load(std::string_view resource_id, uint32_t delay_ms) {
        timer_.set_timeout_on(ui_dispatcher_, [this, id = std::string(resource_id)]() noexcept {
            // 后台线程加载资源
            thread_pool_.enqueue([this, id]() {
                auto resource = load_resource(id);
                
                // 加载完成，回到 UI 线程
                ui_dispatcher_.post([this, id, resource]() noexcept {
                    on_resource_loaded(id, resource);
                });
            });
        }, delay_ms);
    }

private:
    mine::async::Timer& timer_;
    mine::async::Dispatcher& ui_dispatcher_;
    mine::async::ThreadPool thread_pool_{2};
};

// 使用：延迟 2 秒加载非关键资源
loader.schedule_load("background_music.ogg", 2000);
```

### 场景 4：多阶段任务编排

```cpp
class MultiStageTask {
public:
    MultiStageTask(mine::async::Timer& timer) : timer_(timer) {}

    void start() {
        // 阶段 1：立即执行
        stage1();
        
        // 阶段 2：100ms 后
        timer_.set_timeout([this]() noexcept {
            stage2();
            
            // 阶段 3：再过 200ms
            timer_.set_timeout([this]() noexcept {
                stage3();
                
                // 阶段 4：最后 500ms
                timer_.set_timeout([this]() noexcept {
                    stage4_final();
                }, 500);
            }, 200);
        }, 100);
    }

private:
    void stage1() { std::printf("阶段 1: 初始化\n"); }
    void stage2() { std::printf("阶段 2: 预热\n"); }
    void stage3() { std::printf("阶段 3: 主任务\n"); }
    void stage4_final() { std::printf("阶段 4: 完成\n"); }

    mine::async::Timer& timer_;
};
```

---

## 调试技巧

### 1. 定时器状态监控

```cpp
void monitor_timer(const mine::async::Timer& timer) {
    std::printf("活跃定时器数: %u / %u\n",
        timer.active_count(),
        mine::async::Timer::kMaxTimers);
}

// 定期打印
timer.set_interval([&timer]() noexcept {
    monitor_timer(timer);
}, 1000);
```

### 2. 定时器触发追踪

```cpp
TimerHandle traced_timeout(
    mine::async::Timer& timer,
    std::string name,
    Function<void()> callback,
    uint32_t delay_ms
) {
    return timer.set_timeout([name = std::move(name), cb = std::move(callback)]() noexcept {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        std::printf("[TIMER] %s 触发于 %llu\n", name.c_str(), now);
        cb();
    }, delay_ms);
}

// 使用
traced_timeout(timer, "用户操作超时", []() { handle_timeout(); }, 5000);
```

### 3. tick 频率分析

```cpp
class TickProfiler {
public:
    void on_tick() {
        auto now = std::chrono::steady_clock::now();
        if (last_tick_.time_since_epoch().count() > 0) {
            auto delta = std::chrono::duration<double, std::milli>(now - last_tick_).count();
            std::printf("tick 间隔: %.2f ms\n", delta);
        }
        last_tick_ = now;
    }

private:
    std::chrono::steady_clock::time_point last_tick_;
};

// 使用
TickProfiler profiler;
while (running) {
    profiler.on_tick();
    timer.tick();
}
```

---

## 与其他定时器对比

| 特性 | Timer | std::async + sleep_for | platform::Timer (OS) | std::chrono::steady_clock |
|------|-------|------------------------|----------------------|--------------------------|
| 线程模型 | 单线程 tick 驱动 | 每定时器一线程 | 内核回调 | 仅时间戳 |
| 内存开销 | 2.6 KB（固定） | 1 MB/线程 | ~100 字节/定时器 | 0 |
| 精度 | 取决于 tick 频率 | 取决于 OS 调度器 | 高（1-15ms） | N/A（需手动轮询） |
| 最大定时器数 | 32（编译期） | 受限于线程数 | 通常无限制 | N/A |
| 典型开销 | ~100 ns/tick | ~10 µs/定时器 | ~1 µs/触发 | ~50 ns（读时间戳） |
| 适用场景 | UI 动画、游戏循环 | 后台一次性延迟 | 系统级定时 | 性能计时 |

---

## 扩展阅读

- [03-Dispatcher.md](03-Dispatcher.md)：set_timeout_on 如何与 Dispatcher 集成
- [04-ThreadPool.md](04-ThreadPool.md)：定时器回调中提交后台任务
- [../ui.animation/00-AnimationClock.md](../ui.animation/00-AnimationClock.md)：基于 Timer 的动画驱动
- [../core/07-Function.md](../core/07-Function.md)：回调包装器实现
- [../platform/00-PlatformTimer.md](../platform/00-PlatformTimer.md)：操作系统定时器对比

---

**最后更新**：2026-06-12  
**适用版本**：MineFramework M0.1+
