# PropertyAnimation - 属性动画描述

## 概述

`PropertyAnimation` 是 MineUI 动画系统中用于描述单个依赖属性动画的核心数据结构。它封装了动画的所有必要信息：目标对象、属性、起始值、终止值、时长、缓动函数和当前状态。`PropertyAnimation` 由 `Storyboard` 创建和管理，不对外直接暴露，而是作为 Storyboard 的内部实现细节。

### 核心特性

- **完整描述**：包含动画所需的所有信息（目标、属性、from/to、时长、缓动）
- **状态追踪**：记录已流逝时间和完成状态
- **延迟解析**：from/to 值可延迟解析（通过 `from_is_resolved`/`to_is_resolved` 标志）
- **优先级管理**：`retain_p60` 控制动画完成后是否保持 `Animation(P60)` 优先级槽
- **类型安全**：使用 `Variant` 存储任意类型的属性值
- **轻量级**：POD 结构体，无虚函数，按值传递

### 生命周期

```
Storyboard::animate_dp()
    ↓
创建 PropertyAnimation（from 未解析）
    ↓
Storyboard::capture_from_values()
    ↓
解析 from 值（从当前属性值采样）
    ↓
Storyboard::resolve_and_begin()
    ↓
解析 to 值（从 StyleTrigger 或显式参数读取）
    ↓
Storyboard::tick(dt)
    ↓
推进动画（elapsed += dt，插值计算，写入 Animation 优先级）
    ↓
complete = true（elapsed >= duration）
    ↓
Storyboard::stop() 或 stop_not_retained()
    ↓
清除 Animation 优先级（根据 retain_p60）
```

---

## 文件位置

```
src/mine/ui/animation/PropertyAnimation.h
```

---

## 结构体定义

```cpp
namespace mine::ui::animation {

/// @brief 单个依赖属性动画的描述
/// @details 由 Storyboard 创建和管理，不对外直接暴露。
///          包含动画的所有必要信息：目标、属性、from/to 值、时长、缓动函数等。
struct PropertyAnimation {
    /// @brief 动画目标元素
    /// @note 通常是 Visual 或 Control 的实例
    DependencyObject* target{nullptr};

    /// @brief 目标属性
    /// @note 必须是该对象支持的依赖属性
    const DependencyProperty* prop{nullptr};

    /// @brief 起始值
    /// @note 可能在创建时未解析（from_is_resolved = false），
    ///       由 Storyboard::capture_from_values() 采样当前值后解析
    Variant from;

    /// @brief 起始值是否已解析
    /// @note false 表示 from 值尚未采样，需在状态切换前调用 capture_from_values()
    bool from_is_resolved{false};

    /// @brief 终止值
    /// @note 可能在创建时未解析（to_is_resolved = false），
    ///       由 Storyboard::resolve_and_begin() 从 StyleTrigger 读取后解析
    Variant to;

    /// @brief 终止值是否已解析
    /// @note false 表示 to 值尚未从 StyleTrigger 读取，需在状态切换后调用 resolve_and_begin()
    bool to_is_resolved{false};

    /// @brief 动画持续时长
    Duration duration;

    /// @brief 缓动函数
    /// @note 用于将线性进度映射为缓动进度
    EasingFn easing{nullptr};

    /// @brief 已流逝时间（秒）
    /// @note 由 Storyboard::tick(dt) 累加
    float elapsed{0.0f};

    /// @brief 是否已完成
    /// @note 当 elapsed >= duration.to_seconds() 时置为 true
    bool complete{false};

    /// @brief 动画完成后是否保持 Animation(P60) 槽
    /// @note true：完成后保留 Animation 优先级（终止值生效）
    ///       false：完成后清除 Animation 优先级（回退到 Local 或其他优先级的值）
    bool retain_p60{true};

    /// @brief 默认构造函数
    PropertyAnimation() = default;

    /// @brief 构造函数
    /// @param target 动画目标
    /// @param prop 目标属性
    /// @param duration 持续时长
    /// @param easing 缓动函数
    PropertyAnimation(
        DependencyObject* target,
        const DependencyProperty* prop,
        Duration duration,
        EasingFn easing
    )
        : target(target)
        , prop(prop)
        , duration(duration)
        , easing(easing) {}
};

} // namespace mine::ui::animation
```

---

## 成员字段详解

### target

```cpp
DependencyObject* target{nullptr};
```

动画的目标对象，必须是 `DependencyObject` 或其子类（如 `Visual`、`Control`）。

- **类型**：`DependencyObject*`
- **可空**：否（动画必须有目标对象）
- **生命周期**：由 Storyboard 持有，Storyboard 不负责 target 的生命周期管理

**示例**：

```cpp
Button* btn = new Button();
PropertyAnimation anim;
anim.target = btn;  // 目标是按钮
anim.prop = Button::BackgroundProperty;
```

---

### prop

```cpp
const DependencyProperty* prop{nullptr};
```

要动画化的依赖属性。

- **类型**：`const DependencyProperty*`
- **可空**：否（动画必须有目标属性）
- **约束**：必须是 `target` 支持的依赖属性

**示例**：

```cpp
PropertyAnimation anim;
anim.target = btn;
anim.prop = Visual::OpacityProperty;  // 动画化 Opacity 属性
```

---

### from

```cpp
Variant from;
```

动画的起始值，存储在 `Variant` 中以支持任意类型。

- **类型**：`Variant`
- **延迟解析**：如果 `from_is_resolved = false`，则值未初始化，需调用 `Storyboard::capture_from_values()` 采样当前值
- **类型匹配**：必须与 `prop` 的类型兼容

**示例**：

```cpp
PropertyAnimation anim;
anim.from = Variant::from<float>(0.0f);  // 显式设置起始值
anim.from_is_resolved = true;

// 或延迟解析
anim.from_is_resolved = false;  // 稍后由 Storyboard 采样
```

---

### from_is_resolved

```cpp
bool from_is_resolved{false};
```

标志起始值是否已解析。

- **类型**：`bool`
- **默认值**：`false`
- **用途**：
  - `false`：起始值未采样，需在状态切换前调用 `Storyboard::capture_from_values()`
  - `true`：起始值已显式设置或已采样

**示例**：

```cpp
// 自动 from（延迟解析）
PropertyAnimation anim = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
// anim.from_is_resolved = false

// 显式 from
PropertyAnimation anim = storyboard.animate_dp_from_to(
    btn, OpacityProperty, Variant::from<float>(0.0f), Variant::from<float>(1.0f),
    duration, easing
);
// anim.from_is_resolved = true
```

---

### to

```cpp
Variant to;
```

动画的终止值，存储在 `Variant` 中以支持任意类型。

- **类型**：`Variant`
- **延迟解析**：如果 `to_is_resolved = false`，则值未初始化，需调用 `Storyboard::resolve_and_begin()` 从 StyleTrigger 读取
- **类型匹配**：必须与 `prop` 的类型兼容

**示例**：

```cpp
PropertyAnimation anim;
anim.to = Variant::from<float>(1.0f);  // 显式设置终止值
anim.to_is_resolved = true;

// 或延迟解析（从 StyleTrigger 读取）
anim.to_is_resolved = false;  // 稍后由 Storyboard 解析
```

---

### to_is_resolved

```cpp
bool to_is_resolved{false};
```

标志终止值是否已解析。

- **类型**：`bool`
- **默认值**：`false`
- **用途**：
  - `false`：终止值未读取，需在状态切换后调用 `Storyboard::resolve_and_begin()` 从 StyleTrigger 读取
  - `true`：终止值已显式设置

**示例**：

```cpp
// 自动 to（从 StyleTrigger 读取）
PropertyAnimation anim = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
// anim.to_is_resolved = false

// 显式 to
PropertyAnimation anim = storyboard.animate_dp_to(
    btn, OpacityProperty, Variant::from<float>(1.0f), duration, easing
);
// anim.to_is_resolved = true
```

---

### duration

```cpp
Duration duration;
```

动画的持续时长。

- **类型**：`Duration`
- **默认值**：`Duration::seconds(0.0f)`（零时长）
- **用途**：控制动画的推进速度

**示例**：

```cpp
PropertyAnimation anim;
anim.duration = Duration::milliseconds(300);  // 300 毫秒动画
```

---

### easing

```cpp
EasingFn easing{nullptr};
```

缓动函数，用于将线性进度映射为缓动进度。

- **类型**：`EasingFn`（函数指针：`float (*)(float)`）
- **默认值**：`nullptr`（表示线性插值）
- **用途**：实现加速、减速、弹跳等动画效果

**示例**：

```cpp
PropertyAnimation anim;
anim.easing = CubicEaseOut;  // 三次方缓出

// 或使用自定义 CubicBezier
anim.easing = [](float t) {
    return CubicBezier(0.4f, 0.0f, 0.2f, 1.0f)(t);
};
```

---

### elapsed

```cpp
float elapsed{0.0f};
```

已流逝的时间（秒），用于追踪动画进度。

- **类型**：`float`
- **默认值**：`0.0f`
- **更新**：由 `Storyboard::tick(dt)` 累加
- **完成判定**：当 `elapsed >= duration.to_seconds()` 时，动画完成

**示例**：

```cpp
void Storyboard::tick(float dt) {
    for (auto& anim : animations_) {
        if (anim.complete) continue;

        anim.elapsed += dt;
        if (anim.elapsed >= anim.duration.to_seconds()) {
            anim.elapsed = anim.duration.to_seconds();
            anim.complete = true;
        }

        // 计算插值进度
        float t = anim.elapsed / anim.duration.to_seconds();
        if (anim.easing) {
            t = anim.easing(t);
        }

        // 插值并写入 Animation 优先级
        Variant value = interpolate(anim.from, anim.to, t);
        anim.target->set_value_priority(anim.prop, value, Priority::Animation);
    }
}
```

---

### complete

```cpp
bool complete{false};
```

标志动画是否已完成。

- **类型**：`bool`
- **默认值**：`false`
- **完成条件**：`elapsed >= duration.to_seconds()`
- **用途**：用于跳过已完成的动画

**示例**：

```cpp
bool Storyboard::is_complete() const {
    return std::all_of(animations_.begin(), animations_.end(),
        [](const PropertyAnimation& anim) { return anim.complete; }
    );
}
```

---

### retain_p60

```cpp
bool retain_p60{true};
```

动画完成后是否保持 `Animation(P60)` 优先级槽。

- **类型**：`bool`
- **默认值**：`true`
- **用途**：
  - `true`：动画完成后，`Animation` 优先级保留终止值（终止值生效）
  - `false`：动画完成后，清除 `Animation` 优先级（回退到 `Local` 或其他优先级的值）

**示例**：

```cpp
// 场景 1：按钮按下动画（retain_p60 = true）
// 动画完成后保持按下状态的终止值
PropertyAnimation anim = storyboard.animate_dp(btn, ScaleProperty, duration, easing);
anim.retain_p60 = true;  // 动画完成后保持缩放值

// 场景 2：临时高亮动画（retain_p60 = false）
// 动画完成后清除 Animation 优先级，恢复到 Local 值
PropertyAnimation anim = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
anim.retain_p60 = false;  // 动画完成后恢复原始不透明度
```

---

## 使用场景

### 1. Storyboard 创建动画

`PropertyAnimation` 由 `Storyboard` 的 `animate_dp*` 方法创建。

```cpp
Storyboard storyboard;

// 自动 from，to 从 StyleTrigger 读取
auto& anim1 = storyboard.animate_dp(
    button, Visual::OpacityProperty,
    Duration::milliseconds(300), CubicEaseOut
);

// 显式 to
auto& anim2 = storyboard.animate_dp_to(
    button, Visual::ScaleProperty,
    Variant::from<float>(1.2f),
    Duration::milliseconds(200), BackEaseOut
);

// 显式 from/to
auto& anim3 = storyboard.animate_dp_from_to(
    button, Visual::RotationProperty,
    Variant::from<float>(0.0f), Variant::from<float>(360.0f),
    Duration::seconds(1.0f), Linear
);
```

---

### 2. from/to 解析流程

演示延迟解析的完整流程。

```cpp
// 步骤 1：创建 Storyboard，添加动画（from/to 未解析）
Storyboard storyboard;
auto& anim = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
// anim.from_is_resolved = false, anim.to_is_resolved = false

// 步骤 2：采样起始值（状态切换前）
storyboard.capture_from_values();
// anim.from = btn->get_value(OpacityProperty);  // 采样当前值
// anim.from_is_resolved = true;

// 步骤 3：写入新状态的 StyleTrigger
btn->set_value_priority(OpacityProperty, Variant::from<float>(0.5f), Priority::StyleTrigger);

// 步骤 4：解析终止值并开始动画（状态切换后）
storyboard.resolve_and_begin();
// anim.to = btn->get_value(OpacityProperty);  // 读取 StyleTrigger 值
// anim.to_is_resolved = true;
// anim.target->set_value_priority(anim.prop, anim.from, Priority::Animation);  // 写入起始值

// 步骤 5：推进动画
storyboard.tick(dt);
// anim.elapsed += dt;
// float t = anim.elapsed / anim.duration.to_seconds();
// Variant value = interpolate(anim.from, anim.to, t);
// anim.target->set_value_priority(anim.prop, value, Priority::Animation);
```

---

### 3. 优先级管理

动画通过 `Animation(P60)` 优先级覆盖其他值。

```cpp
// 初始状态：Local 优先级
button->set_value(OpacityProperty, Variant::from<float>(1.0f));  // Local
// 有效值：1.0

// 创建动画
Storyboard storyboard;
storyboard.animate_dp_from_to(
    button, OpacityProperty,
    Variant::from<float>(1.0f), Variant::from<float>(0.0f),
    Duration::milliseconds(300), Linear
);

storyboard.capture_from_values();
storyboard.resolve_and_begin();
// Animation 优先级写入起始值
// 优先级链：Animation(1.0) > Local(1.0)
// 有效值：1.0

storyboard.tick(0.15f);  // 中途
// Animation 优先级写入插值 0.5
// 优先级链：Animation(0.5) > Local(1.0)
// 有效值：0.5

storyboard.tick(0.15f);  // 完成
// Animation 优先级写入终止值 0.0
// 优先级链：Animation(0.0) > Local(1.0)
// 有效值：0.0

// retain_p60 = true：保持 Animation 优先级
// 优先级链：Animation(0.0) > Local(1.0)
// 有效值：0.0

// retain_p60 = false：清除 Animation 优先级
storyboard.stop_not_retained();
// 优先级链：Local(1.0)
// 有效值：1.0（恢复）
```

---

### 4. 完成检测

检测动画是否完成。

```cpp
void update_animations(Storyboard& storyboard, float dt) {
    storyboard.tick(dt);

    if (storyboard.is_complete()) {
        std::cout << "所有动画已完成\n";
        on_animation_complete();
    }
}
```

---

### 5. retain_p60 机制

控制动画完成后的行为。

```cpp
// 场景 1：按钮状态切换（retain_p60 = true）
// 从 Normal -> Pressed，动画完成后保持 Pressed 状态
Storyboard pressed_storyboard;
pressed_storyboard.animate_dp(btn, ScaleProperty, duration, easing);
// retain_p60 = true（默认）
// 动画完成后，Animation 优先级保留终止值 0.95

// 场景 2：临时反馈动画（retain_p60 = false）
// 点击时闪烁，动画完成后恢复原始状态
Storyboard flash_storyboard;
auto& anim = flash_storyboard.animate_dp_to(
    btn, OpacityProperty,
    Variant::from<float>(0.5f),
    Duration::milliseconds(100), Linear
);
anim.retain_p60 = false;  // 动画完成后清除 Animation 优先级
// 动画完成后，调用 stop_not_retained() 清除 Animation 优先级，恢复 Local 值
```

---

### 6. 类型转换与插值

不同类型的属性值需要不同的插值方法。

```cpp
// 浮点数插值
Variant interpolate_float(const Variant& from, const Variant& to, float t) {
    float f = from.as<float>();
    float t_val = to.as<float>();
    return Variant::from<float>(f + (t_val - f) * t);
}

// 颜色插值
Variant interpolate_color(const Variant& from, const Variant& to, float t) {
    Color c1 = from.as<Color>();
    Color c2 = to.as<Color>();
    return Variant::from<Color>({
        static_cast<uint8_t>(c1.r + (c2.r - c1.r) * t),
        static_cast<uint8_t>(c1.g + (c2.g - c1.g) * t),
        static_cast<uint8_t>(c1.b + (c2.b - c1.b) * t),
        static_cast<uint8_t>(c1.a + (c2.a - c1.a) * t)
    });
}

// Storyboard::tick() 中根据属性类型选择插值方法
void Storyboard::tick(float dt) {
    for (auto& anim : animations_) {
        // ...计算 t...
        
        Variant value;
        if (anim.prop->type() == PropertyType::Float) {
            value = interpolate_float(anim.from, anim.to, t);
        } else if (anim.prop->type() == PropertyType::Color) {
            value = interpolate_color(anim.from, anim.to, t);
        }
        // ...其他类型...

        anim.target->set_value_priority(anim.prop, value, Priority::Animation);
    }
}
```

---

### 7. 多属性并行动画

一个 Storyboard 可以包含多个 PropertyAnimation，并行动画化多个属性。

```cpp
Storyboard storyboard;

// 同时动画化 Opacity 和 Scale
storyboard.animate_dp(btn, OpacityProperty, Duration::milliseconds(300), CubicEaseOut);
storyboard.animate_dp(btn, ScaleProperty, Duration::milliseconds(300), BackEaseOut);

storyboard.capture_from_values();
storyboard.resolve_and_begin();

// 单次 tick 推进所有动画
storyboard.tick(dt);

// 所有动画完成后返回 true
if (storyboard.is_complete()) {
    std::cout << "Opacity 和 Scale 动画均已完成\n";
}
```

---

## 最佳实践

### ✅ 推荐做法

#### 1. 使用 Storyboard 创建和管理

```cpp
// ✅ 使用 Storyboard 的 animate_dp* 方法创建
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// ❌ 不要手动创建 PropertyAnimation
PropertyAnimation anim;
anim.target = btn;
anim.prop = OpacityProperty;
// 容易遗漏字段，且无法正确集成到 Storyboard 生命周期
```

#### 2. 遵循标准生命周期

```cpp
// ✅ 按顺序调用 Storyboard 方法
storyboard.capture_from_values();      // 1. 采样起始值
// ...写入新状态 StyleTrigger...
storyboard.resolve_and_begin();        // 2. 解析终止值并开始
while (!storyboard.is_complete()) {
    storyboard.tick(dt);               // 3. 推进动画
}
storyboard.stop();                     // 4. 清理
```

#### 3. 根据场景设置 retain_p60

```cpp
// ✅ 状态切换：retain_p60 = true（保持终止值）
auto& anim1 = storyboard.animate_dp(btn, ScaleProperty, duration, easing);
anim1.retain_p60 = true;

// ✅ 临时效果：retain_p60 = false（恢复原值）
auto& anim2 = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
anim2.retain_p60 = false;
```

#### 4. 检查完成状态

```cpp
// ✅ 使用 is_complete() 检测所有动画完成
if (storyboard.is_complete()) {
    on_animation_complete();
}

// ❌ 不要手动遍历检查
bool all_complete = true;
for (const auto& anim : storyboard.animations_) {
    if (!anim.complete) {
        all_complete = false;
        break;
    }
}
```

---

### ❌ 避免的做法

#### 1. 直接修改 PropertyAnimation 字段

```cpp
// ❌ 不要在 Storyboard 外部修改 PropertyAnimation
auto& anim = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
anim.elapsed = 0.5f;  // 错误！破坏内部状态

// ✅ 使用 Storyboard 提供的方法
storyboard.stop();  // 停止并重置
```

#### 2. 忘记调用 capture_from_values()

```cpp
// ❌ 直接调用 resolve_and_begin()，from 值未采样
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);
// 忘记 capture_from_values()
storyboard.resolve_and_begin();  // from 值未初始化，动画从未定义值开始

// ✅ 正确流程
storyboard.capture_from_values();  // 先采样 from
storyboard.resolve_and_begin();    // 再解析 to
```

#### 3. from/to 类型不匹配

```cpp
// ❌ from/to 类型与属性类型不匹配
auto& anim = storyboard.animate_dp_from_to(
    btn, OpacityProperty,  // 属性类型是 float
    Variant::from<int>(0),  // ❌ from 类型是 int
    Variant::from<int>(1),  // ❌ to 类型是 int
    duration, easing
);
// 插值时可能崩溃或产生错误结果

// ✅ 确保类型匹配
auto& anim = storyboard.animate_dp_from_to(
    btn, OpacityProperty,
    Variant::from<float>(0.0f),  // ✅ float
    Variant::from<float>(1.0f),  // ✅ float
    duration, easing
);
```

#### 4. 忘记清理 Animation 优先级

```cpp
// ❌ 动画完成后不调用 stop()
Storyboard storyboard;
// ...动画完成...
// 忘记调用 stop()，Animation 优先级槽一直占用

// ✅ 动画完成后调用 stop()
if (storyboard.is_complete()) {
    storyboard.stop();  // 清理 Animation 优先级
}
```

---

## 常见陷阱

### ❌ 陷阱 1：from/to 值未解析就开始动画

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

// ❌ 跳过 capture_from_values() 和 resolve_and_begin()
storyboard.tick(dt);  // from/to 未初始化，动画行为未定义
```

**✅ 解决方案**：

```cpp
storyboard.capture_from_values();
storyboard.resolve_and_begin();
storyboard.tick(dt);  // ✅ from/to 已解析
```

---

### ❌ 陷阱 2：elapsed 超出 duration 后继续累加

```cpp
void Storyboard::tick(float dt) {
    for (auto& anim : animations_) {
        // ❌ 没有检查 complete 标志
        anim.elapsed += dt;
        // elapsed 会无限累加，t > 1.0 导致插值异常
    }
}
```

**✅ 解决方案**：

```cpp
void Storyboard::tick(float dt) {
    for (auto& anim : animations_) {
        if (anim.complete) continue;  // ✅ 跳过已完成的动画

        anim.elapsed += dt;
        if (anim.elapsed >= anim.duration.to_seconds()) {
            anim.elapsed = anim.duration.to_seconds();  // ✅ 限制最大值
            anim.complete = true;
        }
    }
}
```

---

### ❌ 陷阱 3：多次调用 resolve_and_begin()

```cpp
Storyboard storyboard;
storyboard.animate_dp(btn, OpacityProperty, duration, easing);

storyboard.capture_from_values();
storyboard.resolve_and_begin();  // 第一次：正确

// ❌ 重复调用
storyboard.resolve_and_begin();  // 第二次：重复写入 Animation 优先级
```

**✅ 解决方案**：

每个 Storyboard 只调用一次 `resolve_and_begin()`。

---

### ❌ 陷阱 4：retain_p60 = false 但未调用 stop_not_retained()

```cpp
Storyboard storyboard;
auto& anim = storyboard.animate_dp(btn, OpacityProperty, duration, easing);
anim.retain_p60 = false;

// 动画完成
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

### 示例 1：PropertyAnimation 演示

```cpp
/// @brief 演示 PropertyAnimation 的创建和管理
void demonstrate_property_animation() {
    // 创建控件
    Button* btn = new Button();
    btn->set_value(Visual::OpacityProperty, Variant::from<float>(1.0f));

    // 创建 Storyboard
    Storyboard storyboard;

    // 添加动画：Opacity 从 1.0 淡出到 0.0
    auto& anim = storyboard.animate_dp_from_to(
        btn, Visual::OpacityProperty,
        Variant::from<float>(1.0f),  // from
        Variant::from<float>(0.0f),  // to
        Duration::milliseconds(500),
        CubicEaseOut
    );

    std::cout << "动画创建完成\n";
    std::cout << "  target: " << anim.target << "\n";
    std::cout << "  prop: " << anim.prop->name() << "\n";
    std::cout << "  from_is_resolved: " << anim.from_is_resolved << "\n";
    std::cout << "  to_is_resolved: " << anim.to_is_resolved << "\n";
    std::cout << "  duration: " << anim.duration.to_milliseconds() << " ms\n";
    std::cout << "  retain_p60: " << anim.retain_p60 << "\n";

    // 开始动画
    storyboard.capture_from_values();
    storyboard.resolve_and_begin();

    std::cout << "\n动画开始\n";

    // 推进动画
    float dt = 0.016f;  // 60 FPS
    int frame = 0;
    while (!storyboard.is_complete()) {
        storyboard.tick(dt);
        frame++;

        if (frame % 10 == 0) {  // 每 10 帧打印一次
            float opacity = btn->get_value(Visual::OpacityProperty).as<float>();
            std::cout << "  Frame " << frame
                      << ", elapsed: " << anim.elapsed << "s"
                      << ", opacity: " << opacity << "\n";
        }
    }

    std::cout << "\n动画完成\n";
    std::cout << "  complete: " << anim.complete << "\n";
    std::cout << "  final opacity: "
              << btn->get_value(Visual::OpacityProperty).as<float>() << "\n";

    // 清理
    storyboard.stop();
    delete btn;
}
```

---

### 示例 2：插值计算演示

```cpp
/// @brief 演示属性动画的插值计算
class AnimationInterpolator {
public:
    /// @brief 浮点数线性插值
    static Variant interpolate_float(const Variant& from, const Variant& to, float t) {
        float f = from.as<float>();
        float t_val = to.as<float>();
        float result = f + (t_val - f) * t;
        return Variant::from<float>(result);
    }

    /// @brief 颜色插值
    static Variant interpolate_color(const Variant& from, const Variant& to, float t) {
        Color c1 = from.as<Color>();
        Color c2 = to.as<Color>();
        return Variant::from<Color>({
            static_cast<uint8_t>(c1.r + (c2.r - c1.r) * t),
            static_cast<uint8_t>(c1.g + (c2.g - c1.g) * t),
            static_cast<uint8_t>(c1.b + (c2.b - c1.b) * t),
            static_cast<uint8_t>(c1.a + (c2.a - c1.a) * t)
        });
    }

    /// @brief 矩形插值
    static Variant interpolate_rect(const Variant& from, const Variant& to, float t) {
        Rect r1 = from.as<Rect>();
        Rect r2 = to.as<Rect>();
        return Variant::from<Rect>({
            r1.x + (r2.x - r1.x) * t,
            r1.y + (r2.y - r1.y) * t,
            r1.width + (r2.width - r1.width) * t,
            r1.height + (r2.height - r1.height) * t
        });
    }

    /// @brief 根据属性类型选择插值方法
    static Variant interpolate(
        const PropertyAnimation& anim,
        float t
    ) {
        if (anim.easing) {
            t = anim.easing(t);  // 应用缓动函数
        }

        if (anim.prop->type() == PropertyType::Float) {
            return interpolate_float(anim.from, anim.to, t);
        } else if (anim.prop->type() == PropertyType::Color) {
            return interpolate_color(anim.from, anim.to, t);
        } else if (anim.prop->type() == PropertyType::Rect) {
            return interpolate_rect(anim.from, anim.to, t);
        }

        // 不支持的类型：返回终止值
        return anim.to;
    }
};

/// @brief 演示插值计算
void demonstrate_interpolation() {
    // 创建动画
    PropertyAnimation anim;
    anim.from = Variant::from<float>(0.0f);
    anim.to = Variant::from<float>(100.0f);
    anim.duration = Duration::seconds(1.0f);
    anim.easing = CubicEaseInOut;

    std::cout << "插值演示（CubicEaseInOut）\n";
    for (float t = 0.0f; t <= 1.0f; t += 0.2f) {
        Variant value = AnimationInterpolator::interpolate(anim, t);
        std::cout << "  t=" << t << " → value=" << value.as<float>() << "\n";
    }
}
```

---

### 示例 3：优先级切换演示

```cpp
/// @brief 演示动画优先级与 retain_p60 的交互
void demonstrate_priority_switching() {
    Button* btn = new Button();

    // 初始状态：Local 优先级
    btn->set_value(Visual::OpacityProperty, Variant::from<float>(1.0f));
    std::cout << "初始 Opacity（Local）: "
              << btn->get_value(Visual::OpacityProperty).as<float>() << "\n";

    // 场景 1：retain_p60 = true（保持终止值）
    {
        std::cout << "\n--- 场景 1: retain_p60 = true ---\n";

        Storyboard storyboard;
        auto& anim = storyboard.animate_dp_from_to(
            btn, Visual::OpacityProperty,
            Variant::from<float>(1.0f),
            Variant::from<float>(0.3f),
            Duration::milliseconds(100), Linear
        );
        anim.retain_p60 = true;

        storyboard.capture_from_values();
        storyboard.resolve_and_begin();

        // 推进动画至完成
        while (!storyboard.is_complete()) {
            storyboard.tick(0.016f);
        }

        std::cout << "动画完成后 Opacity（Animation + Local）: "
                  << btn->get_value(Visual::OpacityProperty).as<float>() << "\n";
        // 输出: 0.3（Animation 优先级保留）

        storyboard.stop();
        std::cout << "调用 stop() 后 Opacity（Local）: "
                  << btn->get_value(Visual::OpacityProperty).as<float>() << "\n";
        // 输出: 1.0（恢复 Local 值）
    }

    // 场景 2：retain_p60 = false（恢复原值）
    {
        std::cout << "\n--- 场景 2: retain_p60 = false ---\n";

        btn->set_value(Visual::OpacityProperty, Variant::from<float>(1.0f));

        Storyboard storyboard;
        auto& anim = storyboard.animate_dp_from_to(
            btn, Visual::OpacityProperty,
            Variant::from<float>(1.0f),
            Variant::from<float>(0.3f),
            Duration::milliseconds(100), Linear
        );
        anim.retain_p60 = false;

        storyboard.capture_from_values();
        storyboard.resolve_and_begin();

        // 推进动画至完成
        while (!storyboard.is_complete()) {
            storyboard.tick(0.016f);
        }

        std::cout << "动画完成后 Opacity（Animation + Local）: "
                  << btn->get_value(Visual::OpacityProperty).as<float>() << "\n";
        // 输出: 0.3（Animation 优先级仍保留）

        storyboard.stop_not_retained();
        std::cout << "调用 stop_not_retained() 后 Opacity（Local）: "
                  << btn->get_value(Visual::OpacityProperty).as<float>() << "\n";
        // 输出: 1.0（清除 Animation 优先级，恢复 Local 值）
    }

    delete btn;
}
```

---

## 总结

`PropertyAnimation` 是 MineUI 动画系统中描述单个属性动画的核心数据结构，封装了动画的所有必要信息。主要特性包括：

1. **完整描述**：包含目标、属性、from/to、时长、缓动、状态
2. **延迟解析**：支持 from/to 值的延迟解析（通过 `from_is_resolved`/`to_is_resolved` 标志）
3. **优先级管理**：通过 `Animation(P60)` 优先级覆盖其他值
4. **状态追踪**：`elapsed` 和 `complete` 字段追踪动画进度
5. **retain_p60**：控制动画完成后是否保持终止值

**关键要点**：

- ✅ 使用 `Storyboard` 的 `animate_dp*` 方法创建
- ✅ 遵循标准生命周期：`capture_from_values()` → `resolve_and_begin()` → `tick()` → `stop()`
- ✅ 根据场景设置 `retain_p60`（状态切换用 true，临时效果用 false）
- ✅ 使用 `is_complete()` 检测完成状态
- ❌ 不要手动修改 `PropertyAnimation` 字段
- ❌ 不要跳过 `capture_from_values()` 或 `resolve_and_begin()`
- ❌ 不要让 `elapsed` 超出 `duration` 后继续累加
- ❌ 不要忘记调用 `stop()` 或 `stop_not_retained()` 清理

`PropertyAnimation` 是 `Storyboard` 的内部实现细节，与 `Duration`、`EasingFunction`、`AnimationClock` 等组件协同工作，构成完整的 UI 动画系统。
