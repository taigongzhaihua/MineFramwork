# mine.ui.animation —— 动画与缓动模块

## 模块概述

`mine.ui.animation` 提供属性动画系统，由 `Storyboard` 管理动画时间线，`AnimationClock` 驱动逐帧推进，配合 `EasingFunction` 实现非线性缓动。

| 核心类型 | 职责 |
|---------|------|
| `Storyboard` | 动画时间线容器，管理多个属性的 from→to 插值 |
| `AnimationClock` | 全局时钟，注册后每帧 `tick_all(dt)` 推进所有活跃动画 |
| `Duration` | 动画时长（毫秒） |
| `EasingFunction` | 缓动函数（QuadEaseOut、QuadEaseIn 等） |

**依赖**：`mine.core`、`mine.ui.property`（通过 `DependencyObject::set_value` 写入 `Animation` 优先级）。

---

## 1. Storyboard —— 动画时间线

**文件**：`<mine/ui/animation/Storyboard.h>`

管理一组属性的 from→to 动画，由 `VisualStateManager` 创建和驱动。

### 生命周期阶段

| 阶段 | 方法 | 说明 |
|------|------|------|
| 注册 | `animate_dp(owner, prop, duration, easing)` | 注册要驱动的属性及参数 |
| 采样 | `capture_from_values()` | 读取当前生效值作为 from |
| 解析 | `resolve_and_begin()` | 读取 StyleTrigger/最高优先级值作为 to，开始动画 |
| 推进 | `tick(dt)` → `bool` | 每帧推进，返回 true=已完成 |
| 停止 | `stop()` / `stop_not_retained()` | 清除 Animation(P60) 槽 |

### 类摘要

```cpp
namespace mine::ui::animation {

class Storyboard {
public:
    Storyboard() noexcept;

    // 不可拷贝，可移动
    Storyboard(Storyboard&&) noexcept;
    Storyboard& operator=(Storyboard&&) noexcept;

    // ── 注册动画目标 ─────────────────────────────────────────────────
    // 将 owner.prop 的一个动画注册到此 Storyboard
    // duration: 动画时长（毫秒）
    // easing:   缓动函数类型（默认 Linear）
    void animate_dp(DependencyObject&    owner,
                    const DependencyProperty& prop,
                    Duration             duration,
                    EasingFunction       easing = EasingFunction::Linear);

    // ── 生命周期（由 VSM 调用）──────────────────────────────────────

    void capture_from_values() noexcept;      // 采样当前生效值作为 from
    void resolve_and_begin() noexcept;        // 解析 to 值，写入 Animation=from
    bool tick(float dt) noexcept;             // 推进 dt 秒，返回 true=已完成
    void stop() noexcept;                     // 清除所有受管属性 P60 槽
    void stop_not_retained() noexcept;        // 仅清除 retain_p60=false 的属性

    bool is_active() const noexcept;
};

} // namespace mine::ui::animation
```

### 内部流程（由 VSM 驱动）

```
1. configure(sb)           → 注册 animate_dp
2. capture_from_values()   → 从 P60/P30/P20 读当前可见值作为 from
3. apply_state(P30)        → VSM 写入新状态的 StyleTrigger 值
4. resolve_and_begin()     → 读 P30 作为 to，写 P60=from，启动动画
5. tick(dt) × N            → 每帧插值 from→to，写 P60
6. tick() == true          → 动画完成
   - retain_p60=true       → 保持 P60=to（覆盖 Local 显示目标颜色）
   - retain_p60=false      → P60 被 stop_not_retained 清除，回退到低优先级值
```

---

## 2. AnimationClock —— 全局动画时钟

**文件**：`<mine/ui/animation/AnimationClock.h>`

```cpp
namespace mine::ui::animation {

class AnimationClock {
public:
    static AnimationClock& instance() noexcept;      // 全局单例

    // 注册动画回调（幂等：同一 handle 重复注册仅刷新 fn）
    void register_animation(void* handle,
                            bool (*tick_fn)(void* handle, float dt) noexcept) noexcept;

    // 注销动画回调
    void unregister_animation(void* handle) noexcept;

    // 主循环每帧调用：推进所有注册的动画
    void tick_all(float dt) noexcept;
};

} // namespace mine::ui::animation
```

### 使用示例

```cpp
// 在 on_visual_state_changed 中注册（幂等）
AnimationClock::instance().register_animation(this, &Button::anim_tick_callback);

// 在析构函数中注销
AnimationClock::instance().unregister_animation(this);

// tick 回调
static bool anim_tick_callback(void* handle, float dt) noexcept {
    auto* self = static_cast<Button*>(handle);
    bool any_active = false;
    if (auto* v = self->vsm()) {
        if (v->tick_animations(dt)) any_active = true;
    }
    return any_active;  // false → AnimationClock 自动注销
}
```

---

## 3. Duration —— 动画时长

**文件**：`<mine/ui/animation/Duration.h>`

```cpp
namespace mine::ui::animation {

class Duration {
public:
    constexpr Duration() noexcept;
    static constexpr Duration milliseconds(float ms) noexcept;
    static constexpr Duration seconds(float s) noexcept;

    float as_milliseconds() const noexcept;
    float as_seconds()      const noexcept;
};

} // namespace mine::ui::animation
```

### 常用时长

| 场景 | 时长 | 缓动 |
|------|------|------|
| Hover 入场 | 120ms | QuadEaseOut |
| Hover 出场 | 100ms | QuadEaseOut |
| Press 入场 | 60ms | QuadEaseIn |
| Checked 切换 | 120ms | QuadEaseOut |

---

## 4. EasingFunction —— 缓动函数

**文件**：`<mine/ui/animation/EasingFunction.h>`

```cpp
namespace mine::ui::animation {

enum class EasingFunction : uint8_t {
    Linear       = 0,  // f(t) = t
    QuadEaseOut  = 1,  // f(t) = 1 - (1-t)²   （快速启动，缓停）
    QuadEaseIn   = 2,  // f(t) = t²            （缓启动，快速停止）
    QuadEaseInOut = 3, // 两端缓，中间快
};

// 直接计算缓动值
float apply_easing(EasingFunction easing, float t) noexcept;
// t ∈ [0, 1]，返回 ∈ [0, 1]

} // namespace mine::ui::animation
```

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.ui.property` | Storyboard 通过 `set_value(Animation)` 写入 P60 |
| `mine.ui.style` | VSM 在 `go_to_state` 中创建并驱动 Storyboard |
