# Storyboard - 属性动画时间线

## 概述

`Storyboard` 是 MineUI 动画系统的核心时间线容器，负责管理一组 `PropertyAnimation` 并协调其采样、推进和清除。它与 `VisualStateManager` 紧密集成，提供了声明式的属性动画编排能力，是实现状态切换动画的关键组件。

### 核心特性

- **动画容器**：管理一组 `PropertyAnimation`，支持并行动画化多个属性
- **5 步工作流程**：配置 → 采样 from → 写入 StyleTrigger → 解析 to → 推进动画
- **3 种配置模式**：自动 from、显式 to、显式 from/to
- **优先级协调**：通过 `Animation(P60)` 优先级覆盖 `StyleTrigger` 和 `Local` 值
- **中断支持**：支持动画中途停止并清除优先级
- **retain_p60 机制**：控制动画完成后是否保持终止值
- **VSM 集成**：与 `VisualStateManager` 无缝集成，支持状态切换动画

### 5 步工作流程

```
1. animate_dp*()         配置动画（添加 PropertyAnimation）
       ↓
2. capture_from_values() 采样起始值（from_is_resolved = true）
       ↓
3. （外部）写入新状态的 StyleTrigger
       ↓
4. resolve_and_begin()   解析终止值并开始动画（to_is_resolved = true）
       ↓
5. tick(dt)             推进动画（elapsed += dt，插值计算）
       ↓
   is_complete() → true  所有动画完成
       ↓
   stop() 或 stop_not_retained() 清除 Animation 优先级
```

### 与 VisualStateManager 集成

```cpp
// VisualStateManager 内部使用 Storyboard
class VisualState {
    Storyboard enter_storyboard_;  // 进入状态时的动画
};

// 状态切换流程
void VisualStateManager::go_to_state(VisualState* new_state) {
    // 1. 采样起始值（当前状态）
    new_state->enter_storyboard_.capture_from_values();

    // 2. 写入新状态的 StyleTrigger
    apply_style_trigger(new_state);

    // 3. 解析终止值并开始动画
    new_state->enter_storyboard_.resolve_and_begin();

    // 4. 推进动画（每帧调用）
    AnimationClock::instance().register_animation(this, tick_fn);
}

static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<VisualStateManager*>(user_data);
    return !self->current_state_->enter_storyboard_.tick(dt);  // 返回 false 停止
}
```

---

## 文件位置

```
src/mine/ui/animation/Storyboard.h
```

---

## 类定义

```cpp
namespace mine::ui::animation {

/// @brief 属性动画时间线容器
/// @details 管理一组 PropertyAnimation 并协调其采样、推进和清除。
///          通常由 VisualStateManager 内部使用，但也可直接使用。
class MINE_UI_ANIMATION_API Storyboard {
public:
    /// @brief 默认构造函数
    Storyboard() = default;

    /// @brief 析构函数
    ~Storyboard() = default;

    // ========== 配置阶段方法 ==========

    /// @brief 添加依赖属性动画（自动 from，to 从 StyleTrigger 读取）
    /// @param target 动画目标元素
    /// @param prop 目标属性
    /// @param duration 持续时长
    /// @param easing 缓动函数（可选，默认线性）
    /// @return 新创建的 PropertyAnimation 引用
    /// @note from 值将在 capture_from_values() 时采样
    ///       to 值将在 resolve_and_begin() 时从 StyleTrigger 读取
    PropertyAnimation& animate_dp(
        DependencyObject* target,
        const DependencyProperty* prop,
        Duration duration,
        EasingFn easing = nullptr
    );

    /// @brief 添加依赖属性动画（自动 from，显式 to）
    /// @param target 动画目标元素
    /// @param prop 目标属性
    /// @param to 终止值
    /// @param duration 持续时长
    /// @param easing 缓动函数（可选，默认线性）
    /// @return 新创建的 PropertyAnimation 引用
    /// @note from 值将在 capture_from_values() 时采样
    PropertyAnimation& animate_dp_to(
        DependencyObject* target,
        const DependencyProperty* prop,
        const Variant& to,
        Duration duration,
        EasingFn easing = nullptr
    );

    /// @brief 添加依赖属性动画（显式 from/to）
    /// @param target 动画目标元素
    /// @param prop 目标属性
    /// @param from 起始值
    /// @param to 终止值
    /// @param duration 持续时长
    /// @param easing 缓动函数（可选，默认线性）
    /// @return 新创建的 PropertyAnimation 引用
    PropertyAnimation& animate_dp_from_to(
        DependencyObject* target,
        const DependencyProperty* prop,
        const Variant& from,
        const Variant& to,
        Duration duration,
        EasingFn easing = nullptr
    );

    // ========== 生命周期方法 ==========

    /// @brief 采样所有动画的起始值
    /// @note 必须在写入新状态的 StyleTrigger 之前调用
    void capture_from_values();

    /// @brief 解析终止值并开始动画
    /// @note 必须在写入新状态的 StyleTrigger 之后调用
    void resolve_and_begin();

    /// @brief 推进所有动画
    /// @param dt 时间增量（秒）
    /// @return 是否所有动画已完成（true = 全部完成）
    bool tick(float dt);

    /// @brief 停止所有动画并清除 Animation 优先级
    /// @note 清除所有动画的 Animation(P60) 优先级槽
    void stop();

    /// @brief 仅停止 retain_p60=false 的动画
    /// @note 保留 retain_p60=true 的动画的 Animation 优先级
    void stop_not_retained();

    // ========== 查询方法 ==========

    /// @brief 查询是否所有动画已完成
    /// @return 如果所有动画都已完成返回 true
    bool is_complete() const;

    /// @brief 获取动画数量
    /// @return 当前 Storyboard 中的动画数量
    size_t animation_count() const;

    /// @brief 清空所有动画
    void clear();

private:
    std::vector<PropertyAnimation> animations_;
};

} // namespace mine::ui::animation
```

---

## 成员方法详解

### animate_dp()

```cpp
PropertyAnimation& animate_dp(
    DependencyObject* target,
    const DependencyProperty* prop,
    Duration duration,
    EasingFn easing = nullptr
);
```

添加依赖属性动画，from 值自动采样，to 值从 StyleTrigger 读取。

- **参数**：
  - `target`：动画目标元素（必须是 `DependencyObject` 或子类）
  - `prop`：目标属性（必须是 `target` 支持的依赖属性）
  - `duration`：动画持续时长
  - `easing`：缓动函数（可选，默认 `nullptr` 表示线性插值）
- **返回值**：新创建的 `PropertyAnimation` 引用
- **用途**：最常用的动画配置方式，适合 VisualStateManager 状态切换

**示例**：

```cpp
Storyboard storyboard;

// 自动 from（当前值）→ to（StyleTrigger 值）
storyboard.animate_dp(
    button, Visual::OpacityProperty,
    Duration::milliseconds(300), CubicEaseOut
);

storyboard.animate_dp(
    button, Visual::ScaleProperty,
    Duration::milliseconds(200), BackEaseOut
);

// 采样 from 值
storyboard.capture_from_values();

// 写入新状态 StyleTrigger
button->set_value_priority(Visual::OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);
button->set_value_priority(Visual::ScaleProperty, Variant::from<float>(1.1f), Priority::StyleTrigger);

// 解析 to 值并开始动画
storyboard.resolve_and_begin();

// 推进动画
while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}
```

---

### animate_dp_to()

```cpp
PropertyAnimation& animate_dp_to(
    DependencyObject* target,
    const DependencyProperty* prop,
    const Variant& to,
    Duration duration,
    EasingFn easing = nullptr
);
```

添加依赖属性动画，from 值自动采样，to 值显式指定。

- **参数**：
  - `target`：动画目标元素
  - `prop`：目标属性
  - `to`：显式终止值
  - `duration`：动画持续时长
  - `easing`：缓动函数（可选）
- **返回值**：新创建的 `PropertyAnimation` 引用
- **用途**：适合不依赖 StyleTrigger 的场景，直接指定终止值

**示例**：

```cpp
Storyboard storyboard;

// 自动 from（当前值）→ 显式 to（1.0）
storyboard.animate_dp_to(
    button, Visual::OpacityProperty,
    Variant::from<float>(1.0f),  // 显式 to
    Duration::milliseconds(300), CubicEaseOut
);

storyboard.capture_from_values();
storyboard.resolve_and_begin();

while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}
```

---

### animate_dp_from_to()

```cpp
PropertyAnimation& animate_dp_from_to(
    DependencyObject* target,
    const DependencyProperty* prop,
    const Variant& from,
    const Variant& to,
    Duration duration,
    EasingFn easing = nullptr
);
```

添加依赖属性动画，from 和 to 值均显式指定。

- **参数**：
  - `target`：动画目标元素
  - `prop`：目标属性
  - `from`：显式起始值
  - `to`：显式终止值
  - `duration`：动画持续时长
  - `easing`：缓动函数（可选）
- **返回值**：新创建的 `PropertyAnimation` 引用
- **用途**：完全独立于依赖属性系统的动画，适合临时效果

**示例**：

```cpp
Storyboard storyboard;

// 显式 from（0.0）→ 显式 to（1.0）
storyboard.animate_dp_from_to(
    button, Visual::OpacityProperty,
    Variant::from<float>(0.0f),  // 显式 from
    Variant::from<float>(1.0f),  // 显式 to
    Duration::milliseconds(300), CubicEaseOut
);

// 无需 capture_from_values()，from 已显式指定
storyboard.resolve_and_begin();

while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}
```

---

### capture_from_values()

```cpp
void capture_from_values();
```

采样所有动画的起始值（from），必须在写入新状态的 StyleTrigger 之前调用。

- **用途**：采样 `from_is_resolved = false` 的动画的起始值
- **时机**：在状态切换前调用（新状态 StyleTrigger 写入前）
- **效果**：将每个动画的 `from` 设置为当前有效值，`from_is_resolved` 置为 `true`

**示例**：

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// 1. 采样起始值（状态切换前）
storyboard.capture_from_values();
// 此时 btn->get_value(OpacityProperty) 被采样到 anim.from

// 2. 写入新状态 StyleTrigger
btn->set_value_priority(OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);

// 3. 解析终止值
storyboard.resolve_and_begin();
```

---

### resolve_and_begin()

```cpp
void resolve_and_begin();
```

解析终止值（to）并开始动画，必须在写入新状态的 StyleTrigger 之后调用。

- **用途**：
  - 解析 `to_is_resolved = false` 的动画的终止值（从 StyleTrigger 读取）
  - 将所有动画的起始值写入 `Animation(P60)` 优先级
- **时机**：在状态切换后调用（新状态 StyleTrigger 写入后）
- **效果**：
  - `to_is_resolved` 置为 `true`
  - 写入起始值到 `Animation` 优先级（覆盖 StyleTrigger 和 Local）

**示例**：

```cpp
// 写入新状态 StyleTrigger
btn->set_value_priority(OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);

// 解析终止值并开始动画
storyboard.resolve_and_begin();
// 此时 btn->get_value(OpacityProperty) 读取到 0.5（StyleTrigger 值）
// anim.to = 0.5, to_is_resolved = true
// 同时写入 anim.from 到 Animation 优先级
```

---

### tick()

```cpp
bool tick(float dt);
```

推进所有动画，返回是否所有动画已完成。

- **参数**：
  - `dt`：时间增量（秒）
- **返回值**：`true` 表示所有动画已完成，`false` 表示仍有动画在运行
- **效果**：
  - 累加 `elapsed += dt`
  - 计算插值进度 `t = elapsed / duration`
  - 应用缓动函数 `t = easing(t)`
  - 插值计算 `value = interpolate(from, to, t)`
  - 写入 `Animation` 优先级 `set_value_priority(prop, value, Priority::Animation)`
  - 检查完成条件 `complete = (elapsed >= duration)`

**示例**：

```cpp
auto last_time = std::chrono::high_resolution_clock::now();

while (!storyboard.is_complete()) {
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - last_time).count();
    last_time = now;

    bool complete = storyboard.tick(dt);
    if (complete) {
        std::cout << "所有动画已完成\n";
        break;
    }

    render_frame();
}
```

---

### stop()

```cpp
void stop();
```

停止所有动画并清除所有 Animation 优先级。

- **用途**：中断动画或清理完成的动画
- **效果**：清除所有动画的 `Animation(P60)` 优先级槽，属性值回退到 `StyleTrigger` 或 `Local` 优先级

**示例**：

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);
storyboard.capture_from_values();
storyboard.resolve_and_begin();

// 动画中途停止
std::this_thread::sleep_for(std::chrono::milliseconds(150));
storyboard.stop();  // 清除所有 Animation 优先级

// 属性值回退到 StyleTrigger 或 Local
float opacity = btn->get_value(OpacityProperty).as<float>();
```

---

### stop_not_retained()

```cpp
void stop_not_retained();
```

仅停止 `retain_p60 = false` 的动画，保留 `retain_p60 = true` 的动画。

- **用途**：清理临时效果动画，保留状态切换动画
- **效果**：仅清除 `retain_p60 = false` 的动画的 `Animation` 优先级

**示例**：

```cpp
Storyboard storyboard;

// 状态切换动画（retain_p60 = true）
auto& anim1 = storyboard.animate_dp(btn, ScaleProperty, duration, easing);
anim1.retain_p60 = true;

// 临时反馈动画（retain_p60 = false）
auto& anim2 = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
anim2.retain_p60 = false;

storyboard.capture_from_values();
storyboard.resolve_and_begin();

while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}

// 仅清除 anim2 的 Animation 优先级
storyboard.stop_not_retained();

// anim1 的 Scale 保持动画终止值
// anim2 的 Opacity 回退到 StyleTrigger 或 Local 值
```

---

### is_complete()

```cpp
bool is_complete() const;
```

查询是否所有动画已完成。

- **返回值**：`true` 表示所有动画都已完成
- **用途**：检查动画完成状态，决定是否停止 tick

**示例**：

```cpp
if (storyboard.is_complete()) {
    std::cout << "所有动画已完成\n";
    storyboard.stop();
}
```

---

### animation_count()

```cpp
size_t animation_count() const;
```

获取当前 Storyboard 中的动画数量。

- **返回值**：动画数量
- **用途**：调试、日志、统计

**示例**：

```cpp
std::cout << "Storyboard 包含 " << storyboard.animation_count() << " 个动画\n";
```

---

### clear()

```cpp
void clear();
```

清空所有动画（不清除 Animation 优先级）。

- **用途**：重置 Storyboard 以复用
- **效果**：清空 `animations_` 容器

**示例**：

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);
// ...使用完毕...
storyboard.clear();  // 清空动画列表
// 可以添加新的动画
storyboard.animate_dp(btn, ScaleProperty, duration, easing);
```

---

## 使用场景

### 1. 基础动画

最简单的单属性动画。

```cpp
Button* btn = new Button();
Storyboard storyboard;

// 添加动画：Opacity 从当前值淡出到 StyleTrigger 值
storyboard.animate_dp(
    btn, Visual::OpacityProperty,
    Duration::milliseconds(300), CubicEaseOut
);

// 采样起始值
storyboard.capture_from_values();

// 写入新状态 StyleTrigger
btn->set_value_priority(Visual::OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);

// 解析终止值并开始动画
storyboard.resolve_and_begin();

// 推进动画
while (!storyboard.is_complete()) {
    storyboard.tick(0.016f);  // 60 FPS
}

// 清理
storyboard.stop();
```

---

### 2. 显式 to 值

不依赖 StyleTrigger，直接指定终止值。

```cpp
Storyboard storyboard;

// 从当前值淡入到 1.0
storyboard.animate_dp_to(
    btn, Visual::OpacityProperty,
    Variant::from<float>(1.0f),  // 显式 to
    Duration::milliseconds(300), CubicEaseOut
);

storyboard.capture_from_values();
storyboard.resolve_and_begin();

while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}
```

---

### 3. 显式 from/to 值

完全独立于依赖属性系统的动画。

```cpp
Storyboard storyboard;

// 从 0.0 淡入到 1.0
storyboard.animate_dp_from_to(
    btn, Visual::OpacityProperty,
    Variant::from<float>(0.0f),  // 显式 from
    Variant::from<float>(1.0f),  // 显式 to
    Duration::milliseconds(300), CubicEaseOut
);

// 无需 capture_from_values()
storyboard.resolve_and_begin();

while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}
```

---

### 4. 并行动画（多属性）

同时动画化多个属性。

```cpp
Storyboard storyboard;

// 同时动画化 Opacity、Scale、Rotation
storyboard.animate_dp(btn, Visual::OpacityProperty, Duration::milliseconds(300), CubicEaseOut);
storyboard.animate_dp(btn, Visual::ScaleProperty, Duration::milliseconds(300), BackEaseOut);
storyboard.animate_dp(btn, Visual::RotationProperty, Duration::milliseconds(500), ElasticEaseOut);

storyboard.capture_from_values();

btn->set_value_priority(Visual::OpacityProperty, Variant::from<float>(0.8f), Priority::StyleTrigger);
btn->set_value_priority(Visual::ScaleProperty, Variant::from<float>(1.1f), Priority::StyleTrigger);
btn->set_value_priority(Visual::RotationProperty, Variant::from<float>(10.0f), Priority::StyleTrigger);

storyboard.resolve_and_begin();

// 单次 tick 推进所有动画
while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}
```

---

### 5. 动画中断

中途停止动画并清除优先级。

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, Duration::seconds(2.0f), Linear);

storyboard.capture_from_values();
storyboard.resolve_and_begin();

// 动画 1 秒后中断
for (int i = 0; i < 60; ++i) {  // 60 帧 ≈ 1 秒
    storyboard.tick(0.016f);
}

// 停止动画
storyboard.stop();
std::cout << "动画已中断\n";

// Opacity 回退到 StyleTrigger 或 Local 值
```

---

### 6. retain_p60 机制

演示动画完成后的行为差异。

```cpp
// 场景 1：状态切换（retain_p60 = true）
Storyboard storyboard1;
auto& anim1 = storyboard1.animate_dp(btn, ScaleProperty, duration, easing);
anim1.retain_p60 = true;  // 保持终止值

storyboard1.capture_from_values();
btn->set_value_priority(ScaleProperty, Variant::from<float>(1.1f), Priority::StyleTrigger);
storyboard1.resolve_and_begin();

while (!storyboard1.is_complete()) {
    storyboard1.tick(dt);
}

// Animation 优先级保留终止值 1.1
float scale = btn->get_value(ScaleProperty).as<float>();  // 1.1

// 场景 2：临时效果（retain_p60 = false）
Storyboard storyboard2;
auto& anim2 = storyboard2.animate_dp(btn, OpacityProperty, duration, easing);
anim2.retain_p60 = false;  // 恢复原值

storyboard2.capture_from_values();
btn->set_value_priority(OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);
storyboard2.resolve_and_begin();

while (!storyboard2.is_complete()) {
    storyboard2.tick(dt);
}

storyboard2.stop_not_retained();

// Animation 优先级清除，回退到 Local 值
float opacity = btn->get_value(OpacityProperty).as<float>();  // Local 值
```

---

### 7. VisualStateManager 集成

演示 Storyboard 在 VSM 中的使用。

```cpp
class VisualState {
public:
    std::string name;
    Storyboard enter_storyboard;  // 进入状态时的动画

    void configure_animations(Button* btn) {
        if (name == "Pressed") {
            // 按下状态：缩小到 0.95
            enter_storyboard.animate_dp(
                btn, Visual::ScaleProperty,
                Duration::milliseconds(100), CubicEaseOut
            );
        } else if (name == "Hover") {
            // 悬停状态：放大到 1.05
            enter_storyboard.animate_dp(
                btn, Visual::ScaleProperty,
                Duration::milliseconds(200), BackEaseOut
            );
        }
    }
};

class VisualStateManager {
public:
    void go_to_state(VisualState* new_state) {
        if (current_state_) {
            // 停止当前状态的动画
            current_state_->enter_storyboard.stop();
        }

        current_state_ = new_state;

        // 1. 采样起始值
        new_state->enter_storyboard.capture_from_values();

        // 2. 写入新状态 StyleTrigger
        apply_style_trigger(new_state);

        // 3. 解析终止值并开始动画
        new_state->enter_storyboard.resolve_and_begin();

        // 4. 注册到 AnimationClock
        AnimationClock::instance().register_animation(this, tick_fn);
    }

private:
    VisualState* current_state_{nullptr};

    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<VisualStateManager*>(user_data);
        return !self->current_state_->enter_storyboard.tick(dt);
    }

    void apply_style_trigger(VisualState* state) {
        // 写入 StyleTrigger...
    }
};
```

---

## 最佳实践

### ✅ 推荐做法

#### 1. 遵循标准 5 步流程

```cpp
// ✅ 完整的 5 步流程
Storyboard storyboard;

// 1. 配置动画
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// 2. 采样起始值
storyboard.capture_from_values();

// 3. 写入新状态 StyleTrigger
btn->set_value_priority(OpacityProperty, value, Priority::StyleTrigger);

// 4. 解析终止值并开始动画
storyboard.resolve_and_begin();

// 5. 推进动画
while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}
```

#### 2. 动画完成后清理

```cpp
// ✅ 动画完成后调用 stop() 清理
if (storyboard.is_complete()) {
    storyboard.stop();  // 清除 Animation 优先级
}
```

#### 3. 根据场景选择配置方法

```cpp
// ✅ VSM 状态切换：animate_dp()（自动 from/to）
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// ✅ 直接指定终止值：animate_dp_to()
storyboard.animate_dp_to(btn, OpacityProperty, Variant::from<float>(1.0f), duration, easing);

// ✅ 完全独立动画：animate_dp_from_to()
storyboard.animate_dp_from_to(
    btn, OpacityProperty,
    Variant::from<float>(0.0f), Variant::from<float>(1.0f),
    duration, easing
);
```

#### 4. 正确使用 retain_p60

```cpp
// ✅ 状态切换：retain_p60 = true
auto& anim1 = storyboard.animate_dp(btn, ScaleProperty, duration, easing);
anim1.retain_p60 = true;

// ✅ 临时效果：retain_p60 = false
auto& anim2 = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
anim2.retain_p60 = false;
```

---

### ❌ 避免的做法

#### 1. 跳过 capture_from_values()

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// ❌ 跳过 capture_from_values()
// storyboard.capture_from_values();  // 忘记调用

storyboard.resolve_and_begin();  // from 值未采样，动画从未定义值开始
```

#### 2. 在 capture_from_values() 之前写入 StyleTrigger

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// ❌ 先写入 StyleTrigger
btn->set_value_priority(OpacityProperty, value, Priority::StyleTrigger);

// ❌ 再采样 from
storyboard.capture_from_values();  // 采样到的是 StyleTrigger 值，而非原始值
```

#### 3. 重复调用 resolve_and_begin()

```cpp
storyboard.capture_from_values();
storyboard.resolve_and_begin();  // 第一次

// ❌ 重复调用
storyboard.resolve_and_begin();  // 第二次：重复写入 Animation 优先级
```

#### 4. 忘记调用 stop() 清理

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}

// ❌ 忘记 stop()
// Animation 优先级槽一直占用
```

---

## 常见陷阱

### ❌ 陷阱 1：from 值采样时机错误

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// ❌ 先写入新状态
btn->set_value_priority(OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);

// ❌ 再采样 from
storyboard.capture_from_values();  // 采样到的是 0.5，而非原始值

storyboard.resolve_and_begin();
// 动画从 0.5 → 0.5（无效果）
```

**✅ 解决方案**：

```cpp
storyboard.capture_from_values();  // 先采样原始值
btn->set_value_priority(OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);  // 再写入
storyboard.resolve_and_begin();
```

---

### ❌ 陷阱 2：多个 Storyboard 竞争同一属性

```cpp
Storyboard storyboard1, storyboard2;

// 两个 Storyboard 同时动画化 Opacity
storyboard1.animate_dp(btn, OpacityProperty, duration, easing);
storyboard2.animate_dp(btn, OpacityProperty, duration, easing);

storyboard1.capture_from_values();
storyboard2.capture_from_values();

storyboard1.resolve_and_begin();
storyboard2.resolve_and_begin();  // ❌ 覆盖 storyboard1 的 Animation 优先级

// 两个 Storyboard 同时 tick
storyboard1.tick(dt);  // 写入 value1 到 Animation 优先级
storyboard2.tick(dt);  // 覆盖为 value2
// 动画冲突！
```

**✅ 解决方案**：

同一时刻只允许一个 Storyboard 动画化同一属性。

```cpp
// 停止旧动画
storyboard1.stop();

// 开始新动画
storyboard2.capture_from_values();
storyboard2.resolve_and_begin();
```

---

### ❌ 陷阱 3：tick() 返回值处理错误

```cpp
// ❌ 忽略 tick() 返回值
void update_animation() {
    storyboard.tick(dt);  // 忽略返回值
    // 动画完成后继续 tick，浪费 CPU
}

// ✅ 正确处理返回值
void update_animation() {
    bool complete = storyboard.tick(dt);
    if (complete) {
        AnimationClock::instance().unregister_animation(this);  // 停止 tick
    }
}
```

---

### ❌ 陷阱 4：retain_p60 = false 但不调用 stop_not_retained()

```cpp
Storyboard storyboard;
auto& anim = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
anim.retain_p60 = false;

while (!storyboard.is_complete()) {
    storyboard.tick(dt);
}

// ❌ 忘记调用 stop_not_retained()
// Animation 优先级未清除，retain_p60 = false 无效
```

**✅ 解决方案**：

```cpp
if (storyboard.is_complete()) {
    storyboard.stop_not_retained();  // ✅ 清除 retain_p60 = false 的动画
}
```

---

## 完整示例

### 示例 1：按钮状态动画

```cpp
/// @brief 按钮状态管理器（带动画）
class AnimatedButton {
public:
    AnimatedButton() {
        button_ = new Button();
        setup_animations();
    }

    ~AnimatedButton() {
        delete button_;
    }

    void go_to_normal() { transition_to_state("Normal"); }
    void go_to_hover() { transition_to_state("Hover"); }
    void go_to_pressed() { transition_to_state("Pressed"); }

private:
    Button* button_;
    std::string current_state_{"Normal"};
    Storyboard storyboard_;

    void setup_animations() {
        // 初始状态
        button_->set_value(Visual::ScaleProperty, Variant::from<float>(1.0f));
        button_->set_value(Visual::OpacityProperty, Variant::from<float>(1.0f));
    }

    void transition_to_state(const std::string& new_state) {
        if (current_state_ == new_state) return;

        std::cout << "状态切换: " << current_state_ << " → " << new_state << "\n";
        current_state_ = new_state;

        // 停止旧动画
        storyboard_.stop();
        storyboard_.clear();

        // 配置新动画
        if (new_state == "Normal") {
            storyboard_.animate_dp(
                button_, Visual::ScaleProperty,
                Duration::milliseconds(200), CubicEaseOut
            );
            storyboard_.animate_dp(
                button_, Visual::OpacityProperty,
                Duration::milliseconds(150), Linear
            );
        } else if (new_state == "Hover") {
            storyboard_.animate_dp(
                button_, Visual::ScaleProperty,
                Duration::milliseconds(150), BackEaseOut
            );
        } else if (new_state == "Pressed") {
            storyboard_.animate_dp(
                button_, Visual::ScaleProperty,
                Duration::milliseconds(100), CubicEaseOut
            );
        }

        // 采样起始值
        storyboard_.capture_from_values();

        // 写入新状态 StyleTrigger
        if (new_state == "Normal") {
            button_->set_value_priority(Visual::ScaleProperty, Variant::from<float>(1.0f), Priority::StyleTrigger);
            button_->set_value_priority(Visual::OpacityProperty, Variant::from<float>(1.0f), Priority::StyleTrigger);
        } else if (new_state == "Hover") {
            button_->set_value_priority(Visual::ScaleProperty, Variant::from<float>(1.05f), Priority::StyleTrigger);
        } else if (new_state == "Pressed") {
            button_->set_value_priority(Visual::ScaleProperty, Variant::from<float>(0.95f), Priority::StyleTrigger);
        }

        // 解析终止值并开始动画
        storyboard_.resolve_and_begin();

        // 注册到 AnimationClock
        AnimationClock::instance().register_animation(this, tick_callback);
    }

    static bool tick_callback(void* user_data, float dt) {
        auto* self = static_cast<AnimatedButton*>(user_data);
        return !self->storyboard_.tick(dt);  // 返回 false 继续
    }
};

/// @brief 演示状态切换
void demonstrate_button_states() {
    AnimatedButton btn;

    btn.go_to_hover();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    btn.go_to_pressed();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    btn.go_to_normal();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "所有状态切换完成\n";
}
```

---

### 示例 2：多属性并行动画

```cpp
/// @brief 复杂动画组合：淡入 + 缩放 + 旋转
void demonstrate_complex_animation() {
    Panel* panel = new Panel();

    // 初始状态：不可见、缩小、无旋转
    panel->set_value(Visual::OpacityProperty, Variant::from<float>(0.0f));
    panel->set_value(Visual::ScaleProperty, Variant::from<float>(0.5f));
    panel->set_value(Visual::RotationProperty, Variant::from<float>(0.0f));

    Storyboard storyboard;

    // 添加 3 个并行动画
    storyboard.animate_dp_to(
        panel, Visual::OpacityProperty,
        Variant::from<float>(1.0f),
        Duration::milliseconds(500), CubicEaseOut
    );

    storyboard.animate_dp_to(
        panel, Visual::ScaleProperty,
        Variant::from<float>(1.0f),
        Duration::milliseconds(400), BackEaseOut
    );

    storyboard.animate_dp_to(
        panel, Visual::RotationProperty,
        Variant::from<float>(360.0f),
        Duration::milliseconds(600), CubicEaseInOut
    );

    // 开始动画
    storyboard.capture_from_values();
    storyboard.resolve_and_begin();

    std::cout << "开始复杂动画（Opacity + Scale + Rotation）\n";
    std::cout << "动画数量: " << storyboard.animation_count() << "\n";

    // 推进动画
    auto last_time = std::chrono::high_resolution_clock::now();
    int frame = 0;

    while (!storyboard.is_complete()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        storyboard.tick(dt);
        frame++;

        if (frame % 10 == 0) {
            std::cout << "  Frame " << frame
                      << ", Opacity: " << panel->get_value(Visual::OpacityProperty).as<float>()
                      << ", Scale: " << panel->get_value(Visual::ScaleProperty).as<float>()
                      << ", Rotation: " << panel->get_value(Visual::RotationProperty).as<float>()
                      << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // 60 FPS
    }

    std::cout << "复杂动画完成\n";
    storyboard.stop();

    delete panel;
}
```

---

### 示例 3：动画中断演示

```cpp
/// @brief 演示动画中断与恢复
void demonstrate_animation_interruption() {
    Button* btn = new Button();
    btn->set_value(Visual::OpacityProperty, Variant::from<float>(1.0f));

    Storyboard storyboard;

    // 长动画：2 秒从 1.0 → 0.0
    storyboard.animate_dp_from_to(
        btn, Visual::OpacityProperty,
        Variant::from<float>(1.0f),
        Variant::from<float>(0.0f),
        Duration::seconds(2.0f), Linear
    );

    storyboard.capture_from_values();
    storyboard.resolve_and_begin();

    std::cout << "开始 2 秒淡出动画\n";

    // 动画 1 秒后中断
    for (int i = 0; i < 60; ++i) {  // 60 帧 ≈ 1 秒
        storyboard.tick(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    float mid_opacity = btn->get_value(Visual::OpacityProperty).as<float>();
    std::cout << "1 秒后中断，当前 Opacity: " << mid_opacity << "\n";

    // 停止动画
    storyboard.stop();

    float final_opacity = btn->get_value(Visual::OpacityProperty).as<float>();
    std::cout << "stop() 后 Opacity: " << final_opacity << "\n";
    // 输出: 1.0（回退到 Local 值）

    // 重新开始新动画（从当前值 → 0.5）
    storyboard.clear();
    storyboard.animate_dp_to(
        btn, Visual::OpacityProperty,
        Variant::from<float>(0.5f),
        Duration::milliseconds(300), CubicEaseOut
    );

    storyboard.capture_from_values();
    storyboard.resolve_and_begin();

    std::cout << "重新开始新动画（300ms → 0.5）\n";

    while (!storyboard.is_complete()) {
        storyboard.tick(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "新动画完成，最终 Opacity: "
              << btn->get_value(Visual::OpacityProperty).as<float>() << "\n";

    storyboard.stop();
    delete btn;
}
```

---

## 总结

`Storyboard` 是 MineUI 动画系统的核心时间线容器，负责管理属性动画的完整生命周期。主要特性包括：

1. **5 步工作流程**：配置 → 采样 from → 写入 StyleTrigger → 解析 to → 推进动画
2. **3 种配置模式**：`animate_dp()`、`animate_dp_to()`、`animate_dp_from_to()`
3. **优先级协调**：通过 `Animation(P60)` 优先级覆盖 `StyleTrigger` 和 `Local` 值
4. **中断支持**：`stop()` 和 `stop_not_retained()` 清除动画优先级
5. **VSM 集成**：与 `VisualStateManager` 无缝集成，支持状态切换动画

**关键要点**：

- ✅ 遵循标准 5 步流程：`animate_dp*()` → `capture_from_values()` → 写入 StyleTrigger → `resolve_and_begin()` → `tick()`
- ✅ 动画完成后调用 `stop()` 或 `stop_not_retained()` 清理
- ✅ 根据场景选择配置方法和 `retain_p60` 设置
- ✅ 处理 `tick()` 返回值，避免无效 tick
- ❌ 不要跳过 `capture_from_values()` 或在错误时机调用
- ❌ 不要让多个 Storyboard 竞争同一属性
- ❌ 不要重复调用 `resolve_and_begin()`
- ❌ 不要忘记清理 Animation 优先级

`Storyboard` 与 `PropertyAnimation`、`Duration`、`EasingFunction`、`AnimationClock` 等组件协同工作，构成完整的 UI 动画系统。
