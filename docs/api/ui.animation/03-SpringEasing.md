# SpringEasing - 弹簧物理缓动

## 概述

`SpringEasing` 是 MineUI 动画系统的弹簧物理缓动模拟器，基于阻尼弹簧的物理模型实现自然的弹性动画效果。与 `EasingFunction` 的无状态函数不同，`SpringEasing` 是有状态的模拟器，能够模拟真实的弹簧物理行为，包括速度、加速度和阻尼。

### 核心特性

- **物理模拟**：基于阻尼弹簧的二阶线性常微分方程
- **有状态**：保持当前位置、速度等状态
- **三种模式**：欠阻尼（振荡）、临界阻尼、过阻尼
- **动态重定向**：支持运行中修改目标值（retarget）
- **自动稳定**：检测并判断弹簧是否已稳定
- **初始速度**：支持设置初始速度，实现惯性效果

### 物理模型

弹簧系统的运动方程（二阶线性常微分方程）：

```
m·a = -k·x - c·v

其中：
  m - 质量（mass）
  k - 弹簧刚度（stiffness）
  c - 阻尼系数（damping）
  x - 相对位移
  v - 速度
  a - 加速度
```

### 三种工作模式

根据阻尼比 `ζ = c / (2·√(m·k))` 的值：

| 模式 | 条件 | 特点 | 适用场景 |
|------|------|------|----------|
| **欠阻尼** | ζ < 1 | 有振荡，超出目标后回弹 | 弹性交互、趣味动画 |
| **临界阻尼** | ζ = 1 | 最快到达目标，无振荡 | 快速响应、无回弹场景 |
| **过阻尼** | ζ > 1 | 慢速到达，无振荡 | 重物感、稳重效果 |

### 与 EasingFunction 的区别

| 特性 | EasingFunction | SpringEasing |
|------|----------------|--------------|
| **状态** | 无状态（纯函数） | 有状态（模拟器） |
| **输入** | 线性进度 (0~1) | 时间增量 (delta_time) |
| **物理性** | 数学曲线 | 物理模拟 |
| **重定向** | 不支持 | 支持运行中修改目标 |
| **初始速度** | 不支持 | 支持 |
| **复杂度** | 简单 | 复杂 |
| **适用场景** | 标准动画 | 交互式动画、拖拽 |

---

## 文件位置

```
src/mine/ui/animation/SpringEasing.h
```

---

## 类定义

```cpp
namespace mine::ui::animation {

/// @brief 弹簧物理缓动模拟器
/// @details 基于阻尼弹簧的物理模型，模拟真实的弹性行为。
///          使用二阶线性常微分方程求解。
class SpringEasing {
public:
    /// @brief 构造函数
    /// @param mass 质量（默认 1.0）
    /// @param stiffness 弹簧刚度（默认 100.0）
    /// @param damping 阻尼系数（默认 10.0）
    SpringEasing(float mass = 1.0f, float stiffness = 100.0f, float damping = 10.0f)
        : mass_(mass)
        , stiffness_(stiffness)
        , damping_(damping)
        , current_(0.0f)
        , target_(0.0f)
        , velocity_(0.0f)
    {
        // 参数验证
        assert(mass > 0.0f && "Mass must be positive");
        assert(stiffness > 0.0f && "Stiffness must be positive");
        assert(damping >= 0.0f && "Damping must be non-negative");
    }

    /// @brief 重置弹簧状态
    /// @param from 起始值
    /// @param to 目标值
    /// @param initial_velocity 初始速度（默认 0.0）
    void reset(float from, float to, float initial_velocity = 0.0f) {
        current_ = from;
        target_ = to;
        velocity_ = initial_velocity;
    }

    /// @brief 重定向到新目标（运行中修改目标）
    /// @param new_target 新目标值
    /// @details 保持当前位置和速度，仅修改目标值
    void retarget(float new_target) {
        target_ = new_target;
    }

    /// @brief 步进模拟（更新状态）
    /// @param dt 时间增量（秒）
    /// @return 当前值
    float step(float dt) {
        // 计算弹簧力
        float displacement = current_ - target_;
        float spring_force = -stiffness_ * displacement;

        // 计算阻尼力
        float damping_force = -damping_ * velocity_;

        // 计算加速度（F = ma => a = F/m）
        float acceleration = (spring_force + damping_force) / mass_;

        // 更新速度和位置（欧拉积分）
        velocity_ += acceleration * dt;
        current_ += velocity_ * dt;

        return current_;
    }

    /// @brief 是否已稳定（到达目标附近并静止）
    /// @param position_threshold 位置阈值（默认 0.001）
    /// @param velocity_threshold 速度阈值（默认 0.001）
    /// @return 如果已稳定返回 true
    bool is_settled(float position_threshold = 0.001f, 
                    float velocity_threshold = 0.001f) const {
        float displacement = std::abs(current_ - target_);
        float speed = std::abs(velocity_);
        return displacement < position_threshold && speed < velocity_threshold;
    }

    // ========== 访问器 ==========

    /// @brief 获取当前值
    float current() const { return current_; }

    /// @brief 获取目标值
    float target() const { return target_; }

    /// @brief 获取当前速度
    float velocity() const { return velocity_; }

    /// @brief 获取质量
    float mass() const { return mass_; }

    /// @brief 获取弹簧刚度
    float stiffness() const { return stiffness_; }

    /// @brief 获取阻尼系数
    float damping() const { return damping_; }

    /// @brief 计算阻尼比 ζ = c / (2·√(m·k))
    float damping_ratio() const {
        return damping_ / (2.0f * std::sqrt(mass_ * stiffness_));
    }

    // ========== 修改器 ==========

    /// @brief 设置质量
    void set_mass(float mass) {
        assert(mass > 0.0f && "Mass must be positive");
        mass_ = mass;
    }

    /// @brief 设置弹簧刚度
    void set_stiffness(float stiffness) {
        assert(stiffness > 0.0f && "Stiffness must be positive");
        stiffness_ = stiffness;
    }

    /// @brief 设置阻尼系数
    void set_damping(float damping) {
        assert(damping >= 0.0f && "Damping must be non-negative");
        damping_ = damping;
    }

private:
    // 弹簧参数
    float mass_;        // 质量
    float stiffness_;   // 刚度
    float damping_;     // 阻尼系数

    // 状态变量
    float current_;     // 当前值
    float target_;      // 目标值
    float velocity_;    // 当前速度
};

// ========== 推荐参数预设 ==========

/// @brief 快速弹入（欠阻尼，轻微振荡）
/// @details mass=1.0, stiffness=180, damping=12
///          阻尼比 ζ ≈ 0.45（欠阻尼）
inline SpringEasing create_quick_spring() {
    return SpringEasing(1.0f, 180.0f, 12.0f);
}

/// @brief 标准过渡（接近临界阻尼）
/// @details mass=1.0, stiffness=100, damping=18
///          阻尼比 ζ ≈ 0.90（接近临界阻尼）
inline SpringEasing create_standard_spring() {
    return SpringEasing(1.0f, 100.0f, 18.0f);
}

/// @brief 重物感（过阻尼）
/// @details mass=2.0, stiffness=80, damping=30
///          阻尼比 ζ ≈ 1.18（过阻尼）
inline SpringEasing create_heavy_spring() {
    return SpringEasing(2.0f, 80.0f, 30.0f);
}

/// @brief 无振荡快速（临界阻尼）
/// @details mass=1.0, stiffness=120, damping=21.9
///          阻尼比 ζ = 1.0（临界阻尼）
inline SpringEasing create_critical_spring() {
    return SpringEasing(1.0f, 120.0f, 21.9f);
}

}  // namespace mine::ui::animation
```

---

## 成员方法

### 构造函数

#### `SpringEasing(mass, stiffness, damping)`

创建弹簧缓动模拟器。

**参数：**
- `mass` - 质量（必须 > 0，默认 1.0）
- `stiffness` - 弹簧刚度（必须 > 0，默认 100.0）
- `damping` - 阻尼系数（必须 >= 0，默认 10.0）

**示例：**
```cpp
// 使用默认参数
SpringEasing spring;

// 自定义参数
SpringEasing custom_spring(1.0f, 180.0f, 12.0f);

// 使用预设工厂函数
auto quick = create_quick_spring();
auto standard = create_standard_spring();
```

---

### 状态控制方法

#### `reset(from, to, initial_velocity)`

重置弹簧状态到新的起点和目标。

**参数：**
- `from` - 起始值
- `to` - 目标值
- `initial_velocity` - 初始速度（默认 0.0）

**示例：**
```cpp
SpringEasing spring = create_quick_spring();

// 从 0 动画到 100
spring.reset(0.0f, 100.0f);

// 从 50 动画到 150，带初始速度（惯性）
spring.reset(50.0f, 150.0f, 200.0f);  // 初始速度 200
```

**使用场景：**
- 开始新动画
- 重置动画状态
- 设置初始惯性速度

---

#### `retarget(new_target)`

运行中修改目标值（保持当前位置和速度）。

**参数：**
- `new_target` - 新目标值

**示例：**
```cpp
SpringEasing spring = create_quick_spring();
spring.reset(0.0f, 100.0f);

// 模拟几帧
for (int i = 0; i < 10; ++i) {
    spring.step(0.016f);  // 16ms
}

// 运行中修改目标
spring.retarget(200.0f);  // 平滑过渡到新目标
```

**使用场景：**
- 拖拽交互（用户改变目标位置）
- 跟随动画（目标不断变化）
- 响应式布局（目标位置动态调整）

---

### 模拟方法

#### `step(dt)`

步进模拟，更新弹簧状态。

**参数：**
- `dt` - 时间增量（秒）

**返回值：**
- `float` - 更新后的当前值

**示例：**
```cpp
SpringEasing spring = create_quick_spring();
spring.reset(0.0f, 100.0f);

// 在更新循环中
void update(float delta_seconds) {
    float value = spring.step(delta_seconds);
    element->set_x(value);
    
    if (spring.is_settled()) {
        // 动画完成
        stop_animation();
    }
}
```

**注意事项：**
- `dt` 应该是帧间隔（如 0.016 秒 for 60fps）
- 不要传入累积时间
- 过大的 `dt` 可能导致数值不稳定

---

### 查询方法

#### `is_settled(position_threshold, velocity_threshold)`

检查弹簧是否已稳定（到达目标附近并静止）。

**参数：**
- `position_threshold` - 位置阈值（默认 0.001）
- `velocity_threshold` - 速度阈值（默认 0.001）

**返回值：**
- `bool` - 如果已稳定返回 `true`

**示例：**
```cpp
while (!spring.is_settled()) {
    float value = spring.step(delta_time);
    update_element(value);
}

// 自定义阈值（更宽松）
if (spring.is_settled(0.01f, 0.01f)) {
    // 认为已完成（允许更大误差）
}
```

---

#### `current()` / `target()` / `velocity()`

获取当前状态。

**示例：**
```cpp
float pos = spring.current();      // 当前位置
float target = spring.target();    // 目标位置
float vel = spring.velocity();     // 当前速度

// 计算进度（0.0 ~ 1.0）
float progress = pos / target;

// 判断运动方向
if (vel > 0) {
    // 向正方向运动
} else if (vel < 0) {
    // 向负方向运动
}
```

---

#### `mass()` / `stiffness()` / `damping()`

获取弹簧参数。

**示例：**
```cpp
float m = spring.mass();
float k = spring.stiffness();
float c = spring.damping();

std::cout << "弹簧参数: m=" << m << ", k=" << k << ", c=" << c << "\n";
```

---

#### `damping_ratio()`

计算阻尼比 `ζ = c / (2·√(m·k))`。

**返回值：**
- `float` - 阻尼比

**示例：**
```cpp
float zeta = spring.damping_ratio();

if (zeta < 1.0f) {
    std::cout << "欠阻尼（有振荡）\n";
} else if (zeta == 1.0f) {
    std::cout << "临界阻尼（最快无振荡）\n";
} else {
    std::cout << "过阻尼（慢速无振荡）\n";
}
```

---

### 修改器方法

#### `set_mass()` / `set_stiffness()` / `set_damping()`

动态修改弹簧参数。

**示例：**
```cpp
// 动态调整弹簧刚度
spring.set_stiffness(200.0f);  // 更硬的弹簧

// 动态调整阻尼
spring.set_damping(20.0f);  // 更快稳定

// 动态调整质量
spring.set_mass(2.0f);  // 更重的物体
```

**注意事项：**
- 运行中修改参数会影响动画效果
- 修改后阻尼比会变化
- 确保参数合法（质量和刚度必须 > 0）

---

## 推荐参数预设

### 快速弹入（欠阻尼）

**参数：** `mass=1.0, stiffness=180, damping=12`

**特点：**
- 阻尼比 `ζ ≈ 0.45`（欠阻尼）
- 有轻微振荡（约 1-2 次）
- 快速到达目标

**适用场景：**
- 按钮点击反馈
- 弹出菜单
- 快速交互响应

**示例：**
```cpp
auto spring = create_quick_spring();
spring.reset(0.0f, 100.0f);
```

---

### 标准过渡（接近临界阻尼）

**参数：** `mass=1.0, stiffness=100, damping=18`

**特点：**
- 阻尼比 `ζ ≈ 0.90`（接近临界阻尼）
- 几乎无振荡
- 平滑快速过渡

**适用场景：**
- 页面元素进入
- 属性平滑过渡
- 标准动画效果

**示例：**
```cpp
auto spring = create_standard_spring();
spring.reset(0.0f, 100.0f);
```

---

### 重物感（过阻尼）

**参数：** `mass=2.0, stiffness=80, damping=30`

**特点：**
- 阻尼比 `ζ ≈ 1.18`（过阻尼）
- 无振荡
- 慢速稳重

**适用场景：**
- 大型元素移动
- 需要重物感的动画
- 稳重的界面效果

**示例：**
```cpp
auto spring = create_heavy_spring();
spring.reset(0.0f, 100.0f);
```

---

### 无振荡快速（临界阻尼）

**参数：** `mass=1.0, stiffness=120, damping=21.9`

**特点：**
- 阻尼比 `ζ = 1.0`（临界阻尼）
- 完全无振荡
- 最快到达目标

**适用场景：**
- 需要快速响应且不能振荡
- 精确定位
- 严肃 UI 场景

**示例：**
```cpp
auto spring = create_critical_spring();
spring.reset(0.0f, 100.0f);
```

---

## 使用场景

### 1. 快速弹入动画

按钮点击等快速反馈：

```cpp
class Button {
public:
    void on_press() {
        // 按下：缩小到 95%
        scale_spring_.reset(1.0f, 0.95f);
        is_animating_ = true;
    }
    
    void on_release() {
        // 释放：回弹到 100%
        scale_spring_.retarget(1.0f);
    }
    
    void update(float delta_seconds) {
        if (!is_animating_) return;
        
        float scale = scale_spring_.step(delta_seconds);
        set_scale(scale);
        
        if (scale_spring_.is_settled()) {
            is_animating_ = false;
        }
    }

private:
    SpringEasing scale_spring_ = create_quick_spring();
    bool is_animating_ = false;
};
```

---

### 2. 标准过渡动画

平滑的属性过渡：

```cpp
class Panel {
public:
    void slide_in() {
        // 从屏幕外滑入
        position_spring_.reset(-width_, 0.0f);
        is_animating_ = true;
    }
    
    void slide_out() {
        // 滑出屏幕
        position_spring_.retarget(-width_);
    }
    
    void update(float delta_seconds) {
        if (!is_animating_) return;
        
        float x = position_spring_.step(delta_seconds);
        set_x(x);
        
        if (position_spring_.is_settled()) {
            is_animating_ = false;
            if (x < 0) {
                // 已完全离开屏幕
                set_visible(false);
            }
        }
    }

private:
    SpringEasing position_spring_ = create_standard_spring();
    bool is_animating_ = false;
    float width_ = 300.0f;
};
```

---

### 3. 重物感动画

大型元素的移动：

```cpp
class Dialog {
public:
    void show() {
        // 从上方落下（带重物感）
        position_spring_.reset(-height_, 0.0f);
        opacity_spring_.reset(0.0f, 1.0f);
        is_animating_ = true;
    }
    
    void update(float delta_seconds) {
        if (!is_animating_) return;
        
        float y = position_spring_.step(delta_seconds);
        float opacity = opacity_spring_.step(delta_seconds);
        
        set_y(y);
        set_opacity(opacity);
        
        if (position_spring_.is_settled() && opacity_spring_.is_settled()) {
            is_animating_ = false;
        }
    }

private:
    SpringEasing position_spring_ = create_heavy_spring();
    SpringEasing opacity_spring_ = create_standard_spring();
    bool is_animating_ = false;
    float height_ = 400.0f;
};
```

---

### 4. 无振荡快速响应

需要精确定位的场景：

```cpp
class Slider {
public:
    void set_value(float value) {
        // 快速无振荡过渡到新值
        value_spring_.retarget(value);
    }
    
    void update(float delta_seconds) {
        float current = value_spring_.step(delta_seconds);
        update_thumb_position(current);
        
        if (value_spring_.is_settled()) {
            // 已到达精确位置
            on_value_changed(current);
        }
    }

private:
    SpringEasing value_spring_ = create_critical_spring();
};
```

---

### 5. 动态重定向（跟随动画）

目标不断变化的场景：

```cpp
class FollowCursor {
public:
    void on_mouse_move(float x, float y) {
        // 鼠标移动时重定向目标
        x_spring_.retarget(x);
        y_spring_.retarget(y);
    }
    
    void update(float delta_seconds) {
        float x = x_spring_.step(delta_seconds);
        float y = y_spring_.step(delta_seconds);
        
        element_->set_position(x, y);
    }

private:
    SpringEasing x_spring_ = create_standard_spring();
    SpringEasing y_spring_ = create_standard_spring();
};
```

---

### 6. 拖拽交互

带惯性的拖拽释放：

```cpp
class DraggableElement {
public:
    void on_drag_start(float x, float y) {
        is_dragging_ = true;
        position_spring_.reset(x, x, 0.0f);  // 无初始速度
    }
    
    void on_drag_move(float x) {
        if (is_dragging_) {
            // 拖拽时直接设置位置
            position_spring_.reset(x, x, 0.0f);
            set_x(x);
        }
    }
    
    void on_drag_end(float velocity_x) {
        is_dragging_ = false;
        
        // 释放时应用速度（惯性）
        float current = position_spring_.current();
        position_spring_.reset(current, snap_target_, velocity_x);
    }
    
    void update(float delta_seconds) {
        if (!is_dragging_) {
            float x = position_spring_.step(delta_seconds);
            set_x(x);
        }
    }

private:
    SpringEasing position_spring_ = create_quick_spring();
    bool is_dragging_ = false;
    float snap_target_ = 0.0f;
};
```

---

### 7. 滚动惯性模拟

模拟真实的滚动惯性：

```cpp
class ScrollView {
public:
    void on_scroll_gesture(float velocity) {
        // 设置初始速度（惯性）
        float current = scroll_offset_;
        float target = current + velocity * 0.5f;  // 速度转换为距离
        
        // 限制在有效范围内
        target = std::clamp(target, 0.0f, max_scroll_);
        
        scroll_spring_.reset(current, target, velocity);
    }
    
    void update(float delta_seconds) {
        float offset = scroll_spring_.step(delta_seconds);
        scroll_offset_ = offset;
        
        update_content_offset(offset);
        
        if (scroll_spring_.is_settled()) {
            on_scroll_end();
        }
    }

private:
    SpringEasing scroll_spring_ = create_standard_spring();
    float scroll_offset_ = 0.0f;
    float max_scroll_ = 1000.0f;
};
```

---

### 8. 多属性协同动画

多个属性使用独立的弹簧：

```cpp
class Card {
public:
    void expand() {
        // 位置、大小、透明度同时动画
        x_spring_.reset(start_x_, end_x_);
        y_spring_.reset(start_y_, end_y_);
        width_spring_.reset(small_width_, large_width_);
        height_spring_.reset(small_height_, large_height_);
        opacity_spring_.reset(0.8f, 1.0f);
    }
    
    void update(float delta_seconds) {
        float x = x_spring_.step(delta_seconds);
        float y = y_spring_.step(delta_seconds);
        float w = width_spring_.step(delta_seconds);
        float h = height_spring_.step(delta_seconds);
        float opacity = opacity_spring_.step(delta_seconds);
        
        set_bounds(x, y, w, h);
        set_opacity(opacity);
        
        // 所有弹簧都稳定后才算完成
        if (x_spring_.is_settled() && 
            y_spring_.is_settled() &&
            width_spring_.is_settled() &&
            height_spring_.is_settled() &&
            opacity_spring_.is_settled()) {
            on_expand_complete();
        }
    }

private:
    SpringEasing x_spring_ = create_standard_spring();
    SpringEasing y_spring_ = create_standard_spring();
    SpringEasing width_spring_ = create_standard_spring();
    SpringEasing height_spring_ = create_standard_spring();
    SpringEasing opacity_spring_ = create_quick_spring();
    
    float start_x_, start_y_, end_x_, end_y_;
    float small_width_, small_height_, large_width_, large_height_;
};
```

---

## 最佳实践

### ✅ 使用预设工厂函数

**推荐：**
```cpp
// ✅ 使用预设参数
auto spring = create_quick_spring();

// ✅ 或根据场景选择
auto button_spring = create_quick_spring();
auto panel_spring = create_standard_spring();
auto dialog_spring = create_heavy_spring();
```

**不推荐：**
```cpp
// ❌ 随意设置参数（不知道效果）
SpringEasing spring(1.5f, 73.2f, 8.9f);
```

**原因：** 预设参数经过调试，效果良好。

---

### ✅ 在更新循环中调用 step()

**推荐：**
```cpp
// ✅ 每帧调用一次 step()
void update(float delta_seconds) {
    float value = spring.step(delta_seconds);
    element->set_x(value);
}
```

**不推荐：**
```cpp
// ❌ 每帧调用多次（错误）
void update(float delta_seconds) {
    spring.step(delta_seconds);
    spring.step(delta_seconds);  // 错误！
    float value = spring.current();
}
```

**原因：** `step()` 会修改状态，每帧只能调用一次。

---

### ✅ 使用 is_settled() 检测完成

**推荐：**
```cpp
// ✅ 使用 is_settled() 判断完成
if (spring.is_settled()) {
    stop_animation();
}

// ✅ 自定义阈值（更宽松）
if (spring.is_settled(0.01f, 0.01f)) {
    stop_animation();
}
```

**不推荐：**
```cpp
// ❌ 手动比较（不准确）
if (std::abs(spring.current() - spring.target()) < 0.01f) {
    stop_animation();  // 忽略了速度！
}
```

**原因：** `is_settled()` 同时检查位置和速度。

---

### ✅ 使用 retarget() 实现平滑过渡

**推荐：**
```cpp
// ✅ 使用 retarget()（保持速度）
spring.retarget(new_target);
```

**不推荐：**
```cpp
// ❌ 使用 reset()（丢失速度，产生跳跃）
spring.reset(spring.current(), new_target);
```

**原因：** `retarget()` 保持当前速度，过渡更平滑。

---

### ✅ 设置初始速度实现惯性

**推荐：**
```cpp
// ✅ 拖拽释放时设置初始速度
void on_drag_end(float velocity) {
    spring.reset(current_pos, target_pos, velocity);
}
```

**不推荐：**
```cpp
// ❌ 忽略速度（无惯性感）
void on_drag_end(float velocity) {
    spring.reset(current_pos, target_pos);  // 缺少 velocity
}
```

**原因：** 初始速度提供惯性效果，更自然。

---

## 常见陷阱

### ❌ dt 参数错误

**错误示例：**
```cpp
// ❌ 传入累积时间（错误）
float total_time = 0.0f;
void update(float delta) {
    total_time += delta;
    float value = spring.step(total_time);  // 错误！
}

// ❌ dt 过大（数值不稳定）
spring.step(1.0f);  // 1秒步长太大
```

**正确做法：**
```cpp
// ✅ 传入帧间隔
void update(float delta_seconds) {
    float value = spring.step(delta_seconds);  // 每帧的时间增量
}

// ✅ 限制最大 dt（避免极端情况）
void update(float delta_seconds) {
    float dt = std::min(delta_seconds, 0.033f);  // 限制为 30fps
    float value = spring.step(dt);
}
```

**解决方案：**
- 传入每帧的时间增量（如 0.016 秒）
- 不要传入累积时间
- 限制最大 dt 避免数值不稳定

---

### ❌ 忘记检查 is_settled()

**错误示例：**
```cpp
// ❌ 永远循环（弹簧永不精确到达目标）
while (spring.current() != spring.target()) {
    spring.step(0.016f);
}
```

**正确做法：**
```cpp
// ✅ 使用 is_settled() 判断
while (!spring.is_settled()) {
    spring.step(0.016f);
}

// ✅ 或在更新循环中检查
void update(float dt) {
    if (!spring.is_settled()) {
        float value = spring.step(dt);
        update_element(value);
    }
}
```

**解决方案：**
- 弹簧永远不会精确到达目标（浮点精度）
- 使用 `is_settled()` 判断"足够接近"

---

### ❌ 过度使用欠阻尼

**错误示例：**
```cpp
// ❌ 所有动画都用欠阻尼（振荡过多）
SpringEasing spring(1.0f, 200.0f, 5.0f);  // ζ ≈ 0.18，振荡强烈
```

**正确做法：**
```cpp
// ✅ 大部分场景用临界阻尼或接近临界阻尼
auto spring1 = create_standard_spring();   // ζ ≈ 0.90
auto spring2 = create_critical_spring();   // ζ = 1.0

// ✅ 仅在需要强调时使用欠阻尼
auto button_spring = create_quick_spring();  // ζ ≈ 0.45
```

**解决方案：**
- 振荡过多会让 UI 显得不专业
- 大部分场景推荐 ζ ≥ 0.9

---

### ❌ 参数不合法

**错误示例：**
```cpp
// ❌ 负质量或零刚度
SpringEasing spring(-1.0f, 0.0f, 10.0f);  // 非法参数！
```

**正确做法：**
```cpp
// ✅ 确保参数合法
assert(mass > 0.0f);
assert(stiffness > 0.0f);
assert(damping >= 0.0f);

SpringEasing spring(1.0f, 100.0f, 10.0f);
```

**解决方案：**
- 质量必须 > 0
- 刚度必须 > 0
- 阻尼必须 >= 0

---

### ❌ 混淆 reset() 和 retarget()

**错误示例：**
```cpp
// ❌ 应该用 retarget()，却用了 reset()
void on_target_change(float new_target) {
    spring.reset(spring.current(), new_target);  // 丢失速度！
}
```

**正确做法：**
```cpp
// ✅ 运行中修改目标用 retarget()
void on_target_change(float new_target) {
    spring.retarget(new_target);  // 保持速度
}

// ✅ 开始新动画用 reset()
void start_animation(float from, float to) {
    spring.reset(from, to);
}
```

**解决方案：**
- `reset()` 用于开始新动画（清空速度）
- `retarget()` 用于运行中修改目标（保持速度）

---

## 完整示例

### 示例 1：弹簧参数调试器

实现一个工具，可视化不同参数的弹簧行为：

```cpp
#include <mine/ui/animation/SpringEasing.h>
#include <mine/ui/animation/Duration.h>
#include <iostream>
#include <vector>
#include <iomanip>

using namespace mine::ui::animation;

/// @brief 弹簧参数调试器
class SpringDebugger {
public:
    /// @brief 模拟弹簧运动
    static void simulate(SpringEasing& spring, float from, float to, float duration_seconds) {
        spring.reset(from, to);
        
        std::cout << "=== 弹簧模拟 ===\n";
        std::cout << "参数: m=" << spring.mass() 
                  << ", k=" << spring.stiffness() 
                  << ", c=" << spring.damping() << "\n";
        std::cout << "阻尼比 ζ = " << spring.damping_ratio() << "\n";
        
        if (spring.damping_ratio() < 1.0f) {
            std::cout << "模式: 欠阻尼（有振荡）\n";
        } else if (spring.damping_ratio() == 1.0f) {
            std::cout << "模式: 临界阻尼（最快无振荡）\n";
        } else {
            std::cout << "模式: 过阻尼（慢速无振荡）\n";
        }
        std::cout << "\n";
        
        // 表头
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Time  |  Value  | Velocity | Progress\n";
        std::cout << "--------+---------+----------+----------\n";
        
        // 模拟
        float time = 0.0f;
        float dt = 0.016f;  // 60fps
        int frame = 0;
        
        while (time < duration_seconds && !spring.is_settled()) {
            float value = spring.step(dt);
            float velocity = spring.velocity();
            float progress = (value - from) / (to - from);
            
            // 每 5 帧打印一次
            if (frame % 5 == 0) {
                std::cout << " " << std::setw(6) << time << "s | "
                          << std::setw(7) << value << " | "
                          << std::setw(8) << velocity << " | "
                          << std::setw(7) << (progress * 100.0f) << "%\n";
            }
            
            time += dt;
            frame++;
        }
        
        std::cout << "\n完成时间: " << time << "s\n";
        std::cout << "最终值: " << spring.current() << "\n";
        std::cout << "误差: " << std::abs(spring.current() - to) << "\n\n";
    }
    
    /// @brief 比较不同预设
    static void compare_presets() {
        std::cout << "=== 预设参数比较 ===\n\n";
        
        struct Preset {
            std::string name;
            SpringEasing spring;
        };
        
        std::vector<Preset> presets = {
            {"快速弹入", create_quick_spring()},
            {"标准过渡", create_standard_spring()},
            {"重物感", create_heavy_spring()},
            {"临界阻尼", create_critical_spring()},
        };
        
        for (auto& preset : presets) {
            std::cout << preset.name << ":\n";
            std::cout << "  m=" << preset.spring.mass()
                      << ", k=" << preset.spring.stiffness()
                      << ", c=" << preset.spring.damping() << "\n";
            std::cout << "  阻尼比 ζ = " << preset.spring.damping_ratio() << "\n";
            
            // 模拟到稳定
            preset.spring.reset(0.0f, 100.0f);
            float time = 0.0f;
            float dt = 0.016f;
            
            while (!preset.spring.is_settled() && time < 2.0f) {
                preset.spring.step(dt);
                time += dt;
            }
            
            std::cout << "  完成时间: " << time << "s\n\n";
        }
    }
    
    /// @brief 可视化弹簧曲线
    static void visualize(SpringEasing& spring, float from, float to) {
        spring.reset(from, to);
        
        std::cout << "=== 弹簧曲线可视化 ===\n\n";
        
        constexpr int kHeight = 20;
        constexpr int kWidth = 60;
        
        // 采样
        std::vector<float> samples;
        float time = 0.0f;
        float dt = 0.016f;
        
        for (int i = 0; i < kWidth; ++i) {
            float value = spring.step(dt);
            samples.push_back(value);
            time += dt;
            
            if (spring.is_settled()) break;
        }
        
        // 找出范围（可能超出 [from, to]）
        float min_val = *std::min_element(samples.begin(), samples.end());
        float max_val = *std::max_element(samples.begin(), samples.end());
        
        // 扩展范围（考虑振荡）
        min_val = std::min(min_val, from);
        max_val = std::max(max_val, to);
        float range = max_val - min_val;
        
        // 绘制
        for (int row = kHeight; row >= 0; --row) {
            float y = min_val + (range * row / kHeight);
            
            // Y 轴标签
            if (row == kHeight) {
                std::cout << std::setw(6) << std::fixed << std::setprecision(1) << max_val << " ┤";
            } else if (row == 0) {
                std::cout << std::setw(6) << min_val << " └";
            } else {
                std::cout << "       │";
            }
            
            // 绘制曲线
            for (size_t col = 0; col < samples.size(); ++col) {
                float value = samples[col];
                float normalized = (value - min_val) / range;
                float expected = static_cast<float>(row) / kHeight;
                
                if (std::abs(normalized - expected) < 0.05f) {
                    std::cout << "●";
                } else if (row == 0) {
                    std::cout << "─";
                } else {
                    std::cout << " ";
                }
            }
            
            std::cout << "\n";
        }
        
        std::cout << "       0.0s" << std::string(kWidth - 8, ' ') << time << "s\n";
    }
};

int main() {
    // 1. 模拟快速弹入
    auto quick = create_quick_spring();
    SpringDebugger::simulate(quick, 0.0f, 100.0f, 1.0f);
    
    // 2. 比较预设
    SpringDebugger::compare_presets();
    
    // 3. 可视化
    auto spring = create_quick_spring();
    SpringDebugger::visualize(spring, 0.0f, 100.0f);
    
    return 0;
}
```

---

### 示例 2：拖拽交互演示

实现带惯性的拖拽效果：

```cpp
#include <mine/ui/animation/SpringEasing.h>
#include <iostream>
#include <cmath>

using namespace mine::ui::animation;

/// @brief 可拖拽元素（模拟）
class DraggableBox {
public:
    DraggableBox() : spring_(create_quick_spring()) {
        spring_.reset(0.0f, 0.0f);  // 初始位置
    }
    
    void on_drag_start(float x) {
        is_dragging_ = true;
        drag_start_x_ = x;
        drag_start_time_ = 0.0f;
        last_drag_x_ = x;
        
        std::cout << "[拖拽开始] x=" << x << "\n";
    }
    
    void on_drag_move(float x, float time) {
        if (!is_dragging_) return;
        
        // 计算速度（用于释放时的惯性）
        float dt = time - drag_start_time_;
        if (dt > 0.001f) {
            drag_velocity_ = (x - last_drag_x_) / dt;
        }
        
        // 拖拽时直接设置位置（不使用弹簧）
        spring_.reset(x, x, 0.0f);
        position_ = x;
        
        last_drag_x_ = x;
        drag_start_time_ = time;
        
        std::cout << "[拖拽中] x=" << x << ", velocity=" << drag_velocity_ << "\n";
    }
    
    void on_drag_end(float time) {
        if (!is_dragging_) return;
        
        is_dragging_ = false;
        
        // 计算吸附目标（每 100 单位一个）
        float snap_target = std::round(position_ / 100.0f) * 100.0f;
        
        // 释放时应用惯性速度
        spring_.reset(position_, snap_target, drag_velocity_);
        
        std::cout << "[拖拽结束] 吸附到 " << snap_target 
                  << ", 初始速度=" << drag_velocity_ << "\n";
    }
    
    void update(float dt) {
        if (is_dragging_) return;
        
        // 弹簧模拟
        position_ = spring_.step(dt);
        
        if (spring_.is_settled()) {
            std::cout << "[已稳定] x=" << position_ << "\n";
        }
    }
    
    float position() const { return position_; }
    bool is_animating() const { return !spring_.is_settled(); }

private:
    SpringEasing spring_;
    float position_ = 0.0f;
    bool is_dragging_ = false;
    
    float drag_start_x_ = 0.0f;
    float drag_start_time_ = 0.0f;
    float last_drag_x_ = 0.0f;
    float drag_velocity_ = 0.0f;
};

int main() {
    DraggableBox box;
    
    std::cout << "=== 拖拽交互演示 ===\n\n";
    
    // 模拟拖拽序列
    float time = 0.0f;
    float dt = 0.016f;  // 60fps
    
    // 1. 开始拖拽
    box.on_drag_start(0.0f);
    
    // 2. 拖拽移动（快速向右）
    for (int i = 0; i < 20; ++i) {
        float x = i * 10.0f;  // 快速移动
        box.on_drag_move(x, time);
        time += dt;
    }
    
    // 3. 释放（带惯性）
    box.on_drag_end(time);
    std::cout << "\n";
    
    // 4. 弹簧模拟（吸附到目标）
    int frame = 0;
    while (box.is_animating() && frame < 120) {
        box.update(dt);
        
        // 每 10 帧打印一次
        if (frame % 10 == 0) {
            std::cout << "  [帧 " << frame << "] x=" << box.position() << "\n";
        }
        
        time += dt;
        frame++;
    }
    
    std::cout << "\n最终位置: " << box.position() << "\n";
    
    return 0;
}
```

---

### 示例 3：滚动惯性模拟

实现真实的滚动惯性效果：

```cpp
#include <mine/ui/animation/SpringEasing.h>
#include <iostream>
#include <algorithm>

using namespace mine::ui::animation;

/// @brief 滚动视图（模拟）
class ScrollView {
public:
    ScrollView(float content_height, float view_height)
        : content_height_(content_height)
        , view_height_(view_height)
        , max_scroll_(std::max(0.0f, content_height - view_height))
        , scroll_spring_(create_standard_spring())
    {
        scroll_spring_.reset(0.0f, 0.0f);
    }
    
    void on_scroll_gesture(float delta, float velocity) {
        // 当前位置
        float current = scroll_spring_.current();
        
        // 计算目标位置（根据速度预测）
        float predicted_offset = current + velocity * 0.3f;  // 速度 -> 距离
        
        // 限制在有效范围内
        float target = std::clamp(predicted_offset, 0.0f, max_scroll_);
        
        // 应用弹簧（带初始速度）
        scroll_spring_.reset(current, target, velocity);
        
        std::cout << "[滚动手势] 当前=" << current 
                  << ", 目标=" << target 
                  << ", 速度=" << velocity << "\n";
    }
    
    void on_scroll_to(float offset) {
        // 直接滚动到指定位置（无惯性）
        float target = std::clamp(offset, 0.0f, max_scroll_);
        scroll_spring_.reset(scroll_spring_.current(), target);
        
        std::cout << "[滚动到] 目标=" << target << "\n";
    }
    
    void update(float dt) {
        if (scroll_spring_.is_settled()) return;
        
        float offset = scroll_spring_.step(dt);
        
        // 检查是否超出边界（需要回弹）
        if (offset < 0.0f) {
            // 超出顶部，回弹
            scroll_spring_.retarget(0.0f);
        } else if (offset > max_scroll_) {
            // 超出底部，回弹
            scroll_spring_.retarget(max_scroll_);
        }
        
        scroll_offset_ = std::clamp(offset, -50.0f, max_scroll_ + 50.0f);  // 允许轻微超出
    }
    
    float scroll_offset() const { return scroll_offset_; }
    bool is_scrolling() const { return !scroll_spring_.is_settled(); }

private:
    float content_height_;
    float view_height_;
    float max_scroll_;
    float scroll_offset_ = 0.0f;
    SpringEasing scroll_spring_;
};

int main() {
    std::cout << "=== 滚动惯性模拟 ===\n\n";
    
    // 创建滚动视图（内容高度 1000，视图高度 600）
    ScrollView scroll_view(1000.0f, 600.0f);
    
    float time = 0.0f;
    float dt = 0.016f;
    
    // 1. 快速向下滑动（大速度）
    std::cout << "场景1：快速向下滑动\n";
    scroll_view.on_scroll_gesture(0.0f, 800.0f);  // 速度 800
    
    int frame = 0;
    while (scroll_view.is_scrolling() && frame < 60) {
        scroll_view.update(dt);
        
        if (frame % 10 == 0) {
            std::cout << "  [帧 " << frame << "] offset=" 
                      << scroll_view.scroll_offset() << "\n";
        }
        
        time += dt;
        frame++;
    }
    std::cout << "最终偏移: " << scroll_view.scroll_offset() << "\n\n";
    
    // 2. 滚动到顶部
    std::cout << "场景2：滚动到顶部\n";
    scroll_view.on_scroll_to(0.0f);
    
    frame = 0;
    while (scroll_view.is_scrolling() && frame < 60) {
        scroll_view.update(dt);
        
        if (frame % 10 == 0) {
            std::cout << "  [帧 " << frame << "] offset=" 
                      << scroll_view.scroll_offset() << "\n";
        }
        
        time += dt;
        frame++;
    }
    std::cout << "最终偏移: " << scroll_view.scroll_offset() << "\n\n";
    
    // 3. 超出边界后回弹
    std::cout << "场景3：超出边界回弹\n";
    scroll_view.on_scroll_gesture(0.0f, -500.0f);  // 向上（负速度）
    
    frame = 0;
    while (scroll_view.is_scrolling() && frame < 60) {
        scroll_view.update(dt);
        
        if (frame % 10 == 0) {
            std::cout << "  [帧 " << frame << "] offset=" 
                      << scroll_view.scroll_offset() << "\n";
        }
        
        time += dt;
        frame++;
    }
    std::cout << "最终偏移: " << scroll_view.scroll_offset() << "\n";
    
    return 0;
}
```

---

## 总结

`SpringEasing` 是 MineUI 动画系统的弹簧物理缓动模拟器，提供了：

1. **物理模拟**：基于真实的阻尼弹簧物理模型
2. **有状态**：保持位置、速度等状态，支持动态交互
3. **三种模式**：欠阻尼、临界阻尼、过阻尼满足不同需求
4. **动态重定向**：支持运行中修改目标（`retarget()`）
5. **初始速度**：支持设置初始速度，实现惯性效果

**关键要点：**

- **使用预设工厂函数**：`create_quick_spring()` 等提供调试好的参数
- **每帧调用一次 step()**：传入帧间隔（delta_time）
- **使用 is_settled() 判断完成**：同时检查位置和速度
- **retarget() vs reset()**：运行中修改目标用 `retarget()`，开始新动画用 `reset()`
- **设置初始速度**：拖拽释放时设置初始速度实现惯性

**使用场景指南：**

| 场景 | 推荐预设 | 特点 |
|------|----------|------|
| 按钮反馈 | `create_quick_spring()` | 快速弹入，轻微振荡 |
| 标准过渡 | `create_standard_spring()` | 平滑快速，几乎无振荡 |
| 大型元素 | `create_heavy_spring()` | 重物感，慢速稳重 |
| 精确定位 | `create_critical_spring()` | 最快无振荡 |
| 拖拽交互 | `create_quick_spring()` + 初始速度 | 惯性效果自然 |
| 滚动惯性 | `create_standard_spring()` + 初始速度 | 真实滚动感 |

**与 EasingFunction 的选择：**

- **EasingFunction**：标准动画、简单过渡、固定时长
- **SpringEasing**：交互式动画、拖拽、需要重定向、惯性效果

`SpringEasing` 为交互式动画提供了物理级别的真实感，特别适合需要动态响应用户输入的场景。
