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
