# AnimationClock - 全局动画时钟

## 概述

`AnimationClock` 是 MineUI 动画系统的全局时钟管理器，负责集中驱动所有活跃动画的推进（tick）。它采用 Meyer's Singleton 模式实现，确保整个应用程序中只有一个动画时钟实例，从而协调所有控件的动画更新。

### 核心特性

- **单例模式**：全局唯一实例，Meyer's Singleton 实现（线程安全）
- **集中驱动**：统一管理所有活跃动画的 tick 回调
- **自动清理**：tick 回调返回 `false` 时自动移除
- **生命周期管理**：控件析构时必须调用 `unregister_animation()`
- **幂等注册**：重复注册同一 handle 不会创建重复回调
- **状态查询**：支持查询是否存在活跃动画
- **高性能**：轻量级设计，适合每帧调用

### 工作流程

```
应用层主循环
    ↓
AnimationClock::instance().tick_all(dt)
    ↓
遍历所有已注册的动画回调
    ↓ (每个回调)
TickFn(user_data, dt) → 返回 true 保持活跃
                      → 返回 false 自动移除
```

### 使用场景

- 应用程序主循环驱动所有动画
- 控件注册自己的动画 tick 回调
- 控件析构时注销动画回调
- 查询是否存在活跃动画（用于优化渲染）
- 定时器控制动画启停
- 多控件协调动画

---

## 文件位置

```
src/mine/ui/animation/AnimationClock.h
```

---

## 类定义

```cpp
namespace mine::ui::animation {

/// @brief 动画 tick 回调函数类型
/// @param user_data 用户数据（通常是控件的 this 指针）
/// @param dt 自上次 tick 以来的时间增量（秒）
/// @return 是否继续保持活跃（false 将自动移除）
using TickFn = bool (*)(void* user_data, float dt);

/// @brief 全局动画时钟（单例）
/// @details 负责集中驱动所有活跃动画的 tick 回调。
///          应用程序在主循环中调用 tick_all(dt) 推进所有动画。
///          控件在开始动画时调用 register_animation() 注册回调，
///          在析构时必须调用 unregister_animation() 注销回调。
class MINE_UI_ANIMATION_API AnimationClock {
public:
    /// @brief 获取全局唯一实例
    /// @return AnimationClock 单例引用
    static AnimationClock& instance() noexcept;

    /// @brief 注册动画 tick 回调
    /// @param handle 动画句柄（通常是控件的 this 指针，用于唯一标识）
    /// @param tick_fn tick 回调函数
    /// @note 重复注册同一 handle 不会创建重复回调（幂等操作）
    void register_animation(void* handle, TickFn tick_fn);

    /// @brief 注销动画 tick 回调
    /// @param handle 动画句柄（必须与注册时的 handle 相同）
    /// @note 控件析构时必须调用此方法，否则可能访问已释放的内存
    void unregister_animation(void* handle);

    /// @brief 推进所有活跃动画
    /// @param dt 自上次 tick 以来的时间增量（秒）
    /// @return 是否仍有动画在运行
    /// @note 应用程序在主循环中调用此方法
    bool tick_all(float dt);

    /// @brief 查询是否存在活跃动画
    /// @return 如果存在至少一个活跃动画返回 true
    bool has_active() const noexcept;

    // 禁止拷贝和移动
    AnimationClock(const AnimationClock&) = delete;
    AnimationClock& operator=(const AnimationClock&) = delete;
    AnimationClock(AnimationClock&&) = delete;
    AnimationClock& operator=(AnimationClock&&) = delete;

private:
    AnimationClock() = default;
    ~AnimationClock() = default;

    struct Impl;
    Impl* impl_{nullptr};
};

} // namespace mine::ui::animation
```

---

## 成员方法详解

### instance()

```cpp
static AnimationClock& instance() noexcept;
```

获取全局唯一的 `AnimationClock` 实例。

- **返回值**：`AnimationClock` 单例引用
- **线程安全**：Meyer's Singleton 实现，C++11 后保证线程安全
- **生命周期**：第一次调用时创建，程序结束时自动销毁

**示例**：

```cpp
// 获取全局实例
AnimationClock& clock = AnimationClock::instance();

// 直接链式调用
AnimationClock::instance().tick_all(dt);
```

---

### register_animation()

```cpp
void register_animation(void* handle, TickFn tick_fn);
```

注册动画 tick 回调，使该动画加入全局时钟的驱动列表。

- **参数**：
  - `handle`：动画句柄，通常使用控件的 `this` 指针作为唯一标识
  - `tick_fn`：tick 回调函数，签名为 `bool (*)(void*, float)`
- **幂等性**：重复注册同一 `handle` 不会创建重复回调
- **回调返回值**：
  - `true`：动画继续保持活跃，下次 `tick_all()` 时继续调用
  - `false`：动画完成，自动从时钟中移除

**示例**：

```cpp
class AnimatedButton : public Button {
public:
    void start_animation() {
        // 注册动画回调
        AnimationClock::instance().register_animation(
            this,  // 使用 this 作为唯一标识
            [](void* user_data, float dt) -> bool {
                auto* self = static_cast<AnimatedButton*>(user_data);
                return self->tick_animation(dt);
            }
        );
    }

    bool tick_animation(float dt) {
        elapsed_ += dt;
        if (elapsed_ >= duration_) {
            // 动画完成，返回 false 自动移除
            return false;
        }
        // 更新动画状态
        update_properties();
        return true;  // 继续运行
    }

private:
    float elapsed_{0.0f};
    float duration_{1.0f};
};
```

---

### unregister_animation()

```cpp
void unregister_animation(void* handle);
```

注销动画 tick 回调，从全局时钟中移除该动画。

- **参数**：
  - `handle`：动画句柄，必须与注册时的 `handle` 相同
- **必须调用**：控件析构时必须调用此方法，否则时钟会尝试调用已释放内存的回调
- **安全性**：如果 `handle` 未注册，调用此方法无副作用

**示例**：

```cpp
class AnimatedButton : public Button {
public:
    ~AnimatedButton() {
        // 析构时必须注销动画回调
        AnimationClock::instance().unregister_animation(this);
    }

    void stop_animation() {
        // 提前停止动画
        AnimationClock::instance().unregister_animation(this);
    }
};
```

**⚠️ 常见错误**：

```cpp
// ❌ 忘记在析构函数中注销
class AnimatedButton : public Button {
    ~AnimatedButton() {
        // 缺少 unregister_animation(this)
        // 导致悬挂指针，可能崩溃
    }
};

// ✅ 正确做法
class AnimatedButton : public Button {
    ~AnimatedButton() {
        AnimationClock::instance().unregister_animation(this);
    }
};
```

---

### tick_all()

```cpp
bool tick_all(float dt);
```

推进所有活跃动画，调用每个已注册的 tick 回调。

- **参数**：
  - `dt`：自上次 tick 以来的时间增量（秒）
- **返回值**：`true` 表示仍有动画在运行，`false` 表示所有动画已完成
- **自动清理**：回调返回 `false` 的动画会被自动移除
- **调用频率**：通常在应用程序主循环中每帧调用一次

**示例**：

```cpp
// 应用程序主循环
void App::main_loop() {
    auto last_time = std::chrono::high_resolution_clock::now();

    while (running_) {
        // 计算时间增量
        auto current_time = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        // 推进所有动画
        bool has_animation = AnimationClock::instance().tick_all(dt);

        // 处理事件
        process_events();

        // 渲染
        render();

        // 优化：如果无动画且无事件，可以降低帧率
        if (!has_animation && !has_events()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
}
```

---

### has_active()

```cpp
bool has_active() const noexcept;
```

查询是否存在至少一个活跃动画。

- **返回值**：`true` 表示存在活跃动画，`false` 表示所有动画已完成
- **用途**：用于优化渲染策略，无动画时可降低帧率

**示例**：

```cpp
void App::update() {
    if (AnimationClock::instance().has_active()) {
        // 有动画，保持高帧率
        render_frame();
    } else {
        // 无动画，降低帧率节省资源
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
```

---

## 使用场景

### 1. 控件动画注册

控件在开始动画时注册 tick 回调，在动画完成或析构时注销。

```cpp
class FadeInButton : public Button {
public:
    void start_fade_in() {
        opacity_ = 0.0f;
        AnimationClock::instance().register_animation(this, tick_callback);
    }

    ~FadeInButton() {
        AnimationClock::instance().unregister_animation(this);
    }

private:
    static bool tick_callback(void* user_data, float dt) {
        auto* self = static_cast<FadeInButton*>(user_data);
        self->opacity_ += dt * 2.0f;  // 0.5秒淡入
        if (self->opacity_ >= 1.0f) {
            self->opacity_ = 1.0f;
            return false;  // 动画完成
        }
        return true;  // 继续
    }

    float opacity_{1.0f};
};
```

---

### 2. 应用层驱动

应用程序在主循环中驱动所有动画。

```cpp
class Application {
public:
    void run() {
        auto last_time = std::chrono::high_resolution_clock::now();

        while (is_running()) {
            // 计算 dt
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_time).count();
            last_time = now;

            // 驱动所有动画
            AnimationClock::instance().tick_all(dt);

            // 渲染
            render_all_windows();

            // 处理事件
            dispatch_events();
        }
    }
};
```

---

### 3. 生命周期管理

确保控件析构时正确注销动画回调。

```cpp
class AnimatedControl : public Control {
public:
    void start_animation() {
        is_animating_ = true;
        AnimationClock::instance().register_animation(this, tick_fn);
    }

    void stop_animation() {
        if (is_animating_) {
            is_animating_ = false;
            AnimationClock::instance().unregister_animation(this);
        }
    }

    ~AnimatedControl() {
        // 析构时确保注销
        stop_animation();
    }

private:
    bool is_animating_{false};

    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<AnimatedControl*>(user_data);
        // 动画逻辑...
        return self->is_animating_;
    }
};
```

---

### 4. 防重入保护

使用幂等注册避免重复回调。

```cpp
class PulseButton : public Button {
public:
    void start_pulse() {
        // 幂等注册：重复调用不会创建重复回调
        AnimationClock::instance().register_animation(this, tick_fn);
        is_pulsing_ = true;
    }

    void stop_pulse() {
        is_pulsing_ = false;
        AnimationClock::instance().unregister_animation(this);
    }

    ~PulseButton() {
        stop_pulse();
    }

private:
    bool is_pulsing_{false};
    float phase_{0.0f};

    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<PulseButton*>(user_data);
        if (!self->is_pulsing_) return false;

        self->phase_ += dt * 2.0f;  // 每秒2个周期
        float scale = 1.0f + 0.1f * std::sin(self->phase_);
        self->set_scale(scale);
        return true;
    }
};
```

---

### 5. 查询状态优化

根据动画状态优化渲染策略。

```cpp
class Renderer {
public:
    void render_frame() {
        if (AnimationClock::instance().has_active()) {
            // 有动画，强制刷新
            invalidate_all();
            render_with_vsync();
        } else {
            // 无动画，仅按需渲染
            if (has_dirty_region()) {
                render_dirty_only();
            }
        }
    }

private:
    void render_with_vsync() { /* 60 FPS */ }
    void render_dirty_only() { /* 按需渲染 */ }
    bool has_dirty_region() const { return false; }
    void invalidate_all() { }
};
```

---

### 6. 定时器控制

使用定时器控制动画的启停。

```cpp
class AutoHidePanel : public Panel {
public:
    void on_mouse_enter() {
        // 鼠标进入，停止自动隐藏
        AnimationClock::instance().unregister_animation(this);
    }

    void on_mouse_leave() {
        // 鼠标离开，3秒后自动隐藏
        idle_time_ = 0.0f;
        AnimationClock::instance().register_animation(this, tick_fn);
    }

    ~AutoHidePanel() {
        AnimationClock::instance().unregister_animation(this);
    }

private:
    float idle_time_{0.0f};
    static constexpr float kAutoHideDelay = 3.0f;

    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<AutoHidePanel*>(user_data);
        self->idle_time_ += dt;
        if (self->idle_time_ >= kAutoHideDelay) {
            self->set_visible(false);
            return false;  // 完成
        }
        return true;
    }
};
```

---

### 7. 多控件协调

多个控件同时注册动画，由时钟统一驱动。

```cpp
class ParticleSystem : public Control {
public:
    void add_particle(const Particle& p) {
        particles_.push_back(p);
        if (!is_active_) {
            is_active_ = true;
            AnimationClock::instance().register_animation(this, tick_fn);
        }
    }

    ~ParticleSystem() {
        AnimationClock::instance().unregister_animation(this);
    }

private:
    std::vector<Particle> particles_;
    bool is_active_{false};

    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<ParticleSystem*>(user_data);
        
        // 更新所有粒子
        for (auto& p : self->particles_) {
            p.position += p.velocity * dt;
            p.lifetime -= dt;
        }

        // 移除已死亡粒子
        self->particles_.erase(
            std::remove_if(self->particles_.begin(), self->particles_.end(),
                [](const Particle& p) { return p.lifetime <= 0.0f; }),
            self->particles_.end()
        );

        // 无粒子时停止动画
        if (self->particles_.empty()) {
            self->is_active_ = false;
            return false;
        }
        return true;
    }

    struct Particle {
        Vector2 position;
        Vector2 velocity;
        float lifetime;
    };
};
```

---

## 最佳实践

### ✅ 推荐做法

#### 1. 析构时必须注销

```cpp
class AnimatedWidget : public Widget {
public:
    ~AnimatedWidget() {
        // ✅ 析构时必须注销，防止悬挂指针
        AnimationClock::instance().unregister_animation(this);
    }
};
```

#### 2. 使用 RAII 管理生命周期

```cpp
class AnimationGuard {
public:
    AnimationGuard(void* handle, TickFn fn)
        : handle_(handle) {
        AnimationClock::instance().register_animation(handle_, fn);
    }

    ~AnimationGuard() {
        AnimationClock::instance().unregister_animation(handle_);
    }

private:
    void* handle_;
};

class Widget : public Control {
public:
    void start_animation() {
        // ✅ RAII 自动管理生命周期
        guard_ = std::make_unique<AnimationGuard>(this, tick_fn);
    }

private:
    std::unique_ptr<AnimationGuard> guard_;
    static bool tick_fn(void* user_data, float dt) { return false; }
};
```

#### 3. 动画完成返回 false

```cpp
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    self->elapsed_ += dt;
    if (self->elapsed_ >= self->duration_) {
        // ✅ 动画完成，返回 false 自动移除
        return false;
    }
    return true;
}
```

#### 4. 使用 has_active() 优化渲染

```cpp
void render_loop() {
    if (AnimationClock::instance().has_active()) {
        // ✅ 有动画，保持高帧率
        render_frame();
    } else {
        // ✅ 无动画,降低帧率节省 CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
```

---

### ❌ 避免的做法

#### 1. 忘记注销导致悬挂指针

```cpp
class BadWidget : public Widget {
public:
    void start_animation() {
        AnimationClock::instance().register_animation(this, tick_fn);
    }

    // ❌ 忘记在析构函数中注销
    ~BadWidget() {
        // 导致 AnimationClock 持有悬挂指针
        // 下次 tick_all() 时可能崩溃
    }

private:
    static bool tick_fn(void* user_data, float dt) {
        // ❌ user_data 可能已被释放
        auto* self = static_cast<BadWidget*>(user_data);
        self->do_something();  // 崩溃!
        return true;
    }
};

// ✅ 正确做法
class GoodWidget : public Widget {
public:
    ~GoodWidget() {
        AnimationClock::instance().unregister_animation(this);
    }
};
```

#### 2. 动画完成后忘记返回 false

```cpp
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    self->elapsed_ += dt;
    if (self->elapsed_ >= self->duration_) {
        // ❌ 忘记返回 false，动画永远不会停止
        return true;  // 错误！
    }
    return true;
}

// ✅ 正确做法
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    self->elapsed_ += dt;
    if (self->elapsed_ >= self->duration_) {
        return false;  // ✅ 动画完成，自动移除
    }
    return true;
}
```

#### 3. 在 tick 回调中修改动画列表

```cpp
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    
    // ❌ 不要在 tick 回调中注册或注销其他动画
    // 可能导致迭代器失效
    AnimationClock::instance().register_animation(
        self->child_, other_tick_fn
    );
    
    return true;
}

// ✅ 正确做法：返回后再注册
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    if (should_start_child_animation) {
        self->pending_child_animation_ = true;
        return false;  // 先结束当前动画
    }
    return true;
}

void on_animation_complete() {
    if (pending_child_animation_) {
        AnimationClock::instance().register_animation(child_, child_tick_fn);
    }
}
```

#### 4. 不处理 dt 过大的情况

```cpp
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    
    // ❌ 如果 dt 过大（如窗口最小化后恢复），动画可能跳变
    self->position_ += self->velocity_ * dt;
    
    return true;
}

// ✅ 正确做法：限制最大 dt
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    
    // 限制最大 dt 为 0.1 秒（防止跳变）
    dt = std::min(dt, 0.1f);
    self->position_ += self->velocity_ * dt;
    
    return true;
}
```

---

## 常见陷阱

### ❌ 陷阱 1：忘记注销导致内存访问错误

```cpp
class AnimatedButton : public Button {
public:
    void start_animation() {
        AnimationClock::instance().register_animation(this, tick_fn);
    }

    // ❌ 忘记注销
    ~AnimatedButton() {
        // 未调用 unregister_animation(this)
    }

private:
    static bool tick_fn(void* user_data, float dt) {
        // ❌ this 指针可能已被释放
        auto* self = static_cast<AnimatedButton*>(user_data);
        return self->update(dt);  // 崩溃！
    }
};
```

**✅ 解决方案**：

```cpp
class AnimatedButton : public Button {
public:
    ~AnimatedButton() {
        // ✅ 析构时注销
        AnimationClock::instance().unregister_animation(this);
    }
};
```

---

### ❌ 陷阱 2：动画完成后继续返回 true

```cpp
class FadeOut : public Control {
    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<FadeOut*>(user_data);
        self->opacity_ -= dt;
        if (self->opacity_ <= 0.0f) {
            self->opacity_ = 0.0f;
            // ❌ 应该返回 false，但返回了 true
            return true;  // 导致 tick 回调永远不会停止
        }
        return true;
    }

    float opacity_{1.0f};
};
```

**✅ 解决方案**：

```cpp
class FadeOut : public Control {
    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<FadeOut*>(user_data);
        self->opacity_ -= dt;
        if (self->opacity_ <= 0.0f) {
            self->opacity_ = 0.0f;
            return false;  // ✅ 动画完成，自动移除
        }
        return true;
    }

    float opacity_{1.0f};
};
```

---

### ❌ 陷阱 3：在 tick 回调中递归调用 tick_all()

```cpp
static bool tick_fn(void* user_data, float dt) {
    auto* self = static_cast<Widget*>(user_data);
    
    // ❌ 不要在 tick 回调中调用 tick_all()
    AnimationClock::instance().tick_all(dt);  // 可能导致死循环
    
    return true;
}
```

**✅ 解决方案**：

不要在 tick 回调中调用 `tick_all()`，`tick_all()` 只应在应用程序主循环中调用。

---

### ❌ 陷阱 4：使用栈对象的地址作为 handle

```cpp
void start_temporary_animation() {
    Widget temp_widget;
    
    // ❌ temp_widget 是栈对象，函数返回后会被销毁
    AnimationClock::instance().register_animation(&temp_widget, tick_fn);
    
    // 函数返回后，temp_widget 被销毁，但 tick 回调仍在
}  // ❌ 悬挂指针！
```

**✅ 解决方案**：

只使用堆对象（或生命周期足够长的对象）的地址作为 `handle`。

```cpp
class WidgetManager {
    void start_animation() {
        // ✅ widget_ 是成员变量，生命周期由 WidgetManager 管理
        AnimationClock::instance().register_animation(&widget_, tick_fn);
    }

    ~WidgetManager() {
        AnimationClock::instance().unregister_animation(&widget_);
    }

private:
    Widget widget_;
};
```

---

## 完整示例

### 示例 1：动画控件基类

```cpp
/// @brief 可动画控件基类
/// @details 封装了 AnimationClock 的注册/注销逻辑
class AnimatableControl : public Control {
public:
    virtual ~AnimatableControl() {
        // 析构时自动注销
        stop_animation();
    }

protected:
    /// @brief 开始动画
    /// @note 子类调用此方法启动动画
    void start_animation() {
        if (!is_animating_) {
            is_animating_ = true;
            AnimationClock::instance().register_animation(this, &AnimatableControl::static_tick);
        }
    }

    /// @brief 停止动画
    void stop_animation() {
        if (is_animating_) {
            is_animating_ = false;
            AnimationClock::instance().unregister_animation(this);
        }
    }

    /// @brief 动画 tick 虚函数（子类重写）
    /// @param dt 时间增量（秒）
    /// @return 是否继续运行
    virtual bool tick_animation(float dt) = 0;

private:
    bool is_animating_{false};

    static bool static_tick(void* user_data, float dt) {
        auto* self = static_cast<AnimatableControl*>(user_data);
        if (!self->is_animating_) return false;
        return self->tick_animation(dt);
    }
};

/// @brief 淡入淡出按钮
class FadeButton : public AnimatableControl {
public:
    void fade_in() {
        target_opacity_ = 1.0f;
        start_animation();
    }

    void fade_out() {
        target_opacity_ = 0.0f;
        start_animation();
    }

protected:
    bool tick_animation(float dt) override {
        float speed = 2.0f;  // 每秒变化量
        if (std::abs(opacity_ - target_opacity_) < 0.01f) {
            opacity_ = target_opacity_;
            return false;  // 完成
        }

        if (opacity_ < target_opacity_) {
            opacity_ += speed * dt;
        } else {
            opacity_ -= speed * dt;
        }

        set_opacity(opacity_);
        return true;  // 继续
    }

private:
    float opacity_{1.0f};
    float target_opacity_{1.0f};
};

/// @brief 旋转控件
class RotatingWidget : public AnimatableControl {
public:
    void start_rotating(float speed_rad_per_sec) {
        rotation_speed_ = speed_rad_per_sec;
        start_animation();
    }

    void stop_rotating() {
        stop_animation();
    }

protected:
    bool tick_animation(float dt) override {
        rotation_angle_ += rotation_speed_ * dt;
        if (rotation_angle_ >= 2.0f * 3.14159f) {
            rotation_angle_ -= 2.0f * 3.14159f;
        }
        set_rotation(rotation_angle_);
        return true;  // 持续旋转
    }

private:
    float rotation_angle_{0.0f};
    float rotation_speed_{1.0f};
};
```

---

### 示例 2：应用层驱动器

```cpp
/// @brief 应用程序动画驱动器
class AnimationDriver {
public:
    AnimationDriver() {
        last_tick_time_ = std::chrono::high_resolution_clock::now();
    }

    /// @brief 在主循环中调用
    /// @return 是否有动画在运行
    bool update() {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last_tick_time_).count();
        last_tick_time_ = now;

        // 限制最大 dt（防止窗口最小化后恢复时跳变）
        dt = std::min(dt, 0.1f);

        // 驱动所有动画
        bool has_animation = AnimationClock::instance().tick_all(dt);

        // 更新统计信息
        update_statistics(dt, has_animation);

        return has_animation;
    }

    /// @brief 获取当前 FPS
    float get_fps() const {
        return fps_;
    }

    /// @brief 获取活跃动画数量
    bool has_active_animations() const {
        return AnimationClock::instance().has_active();
    }

private:
    void update_statistics(float dt, bool has_animation) {
        frame_count_++;
        frame_time_sum_ += dt;

        if (frame_time_sum_ >= 1.0f) {
            fps_ = frame_count_ / frame_time_sum_;
            frame_count_ = 0;
            frame_time_sum_ = 0.0f;
        }

        if (has_animation) {
            animated_frames_++;
        }
    }

    std::chrono::high_resolution_clock::time_point last_tick_time_;
    float fps_{0.0f};
    int frame_count_{0};
    int animated_frames_{0};
    float frame_time_sum_{0.0f};
};

/// @brief 应用程序主循环
class Application {
public:
    void run() {
        AnimationDriver driver;

        while (is_running_) {
            // 驱动动画
            bool has_animation = driver.update();

            // 处理事件
            process_events();

            // 渲染
            if (has_animation || has_dirty_regions()) {
                render_frame();
            }

            // 优化：无动画且无脏区时降低帧率
            if (!has_animation && !has_dirty_regions()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            // 显示 FPS（调试用）
            if (show_debug_info_) {
                std::cout << "FPS: " << driver.get_fps()
                          << ", Has Animation: " << has_animation << "\n";
            }
        }
    }

private:
    bool is_running_{true};
    bool show_debug_info_{false};

    void process_events() { }
    void render_frame() { }
    bool has_dirty_regions() const { return false; }
};
```

---

### 示例 3：多控件动画演示

```cpp
/// @brief 粒子（用于粒子系统）
struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float lifetime;
    float max_lifetime;
};

/// @brief 粒子系统
class ParticleSystem : public Control {
public:
    ~ParticleSystem() {
        AnimationClock::instance().unregister_animation(this);
    }

    /// @brief 发射粒子
    void emit(int count) {
        for (int i = 0; i < count; ++i) {
            Particle p;
            p.position = {0.0f, 0.0f};
            p.velocity = random_velocity();
            p.color = random_color();
            p.lifetime = 2.0f;
            p.max_lifetime = 2.0f;
            particles_.push_back(p);
        }

        // 开始动画（如果尚未启动）
        if (!is_active_) {
            is_active_ = true;
            AnimationClock::instance().register_animation(this, tick_fn);
        }
    }

    void render() {
        for (const auto& p : particles_) {
            float alpha = p.lifetime / p.max_lifetime;
            render_particle(p.position, p.color, alpha);
        }
    }

private:
    std::vector<Particle> particles_;
    bool is_active_{false};

    static bool tick_fn(void* user_data, float dt) {
        auto* self = static_cast<ParticleSystem*>(user_data);

        // 更新所有粒子
        for (auto& p : self->particles_) {
            p.position += p.velocity * dt;
            p.velocity.y += 9.8f * dt;  // 重力
            p.lifetime -= dt;
        }

        // 移除死亡粒子
        self->particles_.erase(
            std::remove_if(self->particles_.begin(), self->particles_.end(),
                [](const Particle& p) { return p.lifetime <= 0.0f; }),
            self->particles_.end()
        );

        // 无粒子时停止动画
        if (self->particles_.empty()) {
            self->is_active_ = false;
            return false;
        }

        return true;
    }

    Vector2 random_velocity() {
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float speed = 50.0f + (rand() % 100);
        return {speed * std::cos(angle), speed * std::sin(angle)};
    }

    Color random_color() {
        return {
            static_cast<uint8_t>(rand() % 256),
            static_cast<uint8_t>(rand() % 256),
            static_cast<uint8_t>(rand() % 256),
            255
        };
    }

    void render_particle(const Vector2& pos, const Color& color, float alpha) {
        // 渲染逻辑...
    }

    struct Vector2 { float x, y; };
    struct Color { uint8_t r, g, b, a; };
    Vector2 operator+(const Vector2& a, const Vector2& b) {
        return {a.x + b.x, a.y + b.y};
    }
};

/// @brief 演示应用
class ParticleDemo {
public:
    void run() {
        ParticleSystem particles;

        // 每秒发射粒子
        float emit_timer = 0.0f;
        auto last_time = std::chrono::high_resolution_clock::now();

        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_time).count();
            last_time = now;

            emit_timer += dt;
            if (emit_timer >= 0.1f) {
                particles.emit(10);
                emit_timer = 0.0f;
            }

            // 驱动动画
            AnimationClock::instance().tick_all(dt);

            // 渲染
            particles.render();

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
};
```

---

## 总结

`AnimationClock` 是 MineUI 动画系统的核心驱动器，采用单例模式集中管理所有活跃动画。主要特性包括：

1. **单例模式**：全局唯一实例，Meyer's Singleton 线程安全实现
2. **集中驱动**：统一 `tick_all()` 推进所有动画，简化应用层逻辑
3. **自动清理**：回调返回 `false` 时自动移除，减少手动管理
4. **生命周期管理**：控件析构时必须调用 `unregister_animation()`
5. **幂等注册**：重复注册同一 handle 不会创建重复回调
6. **状态查询**：`has_active()` 用于优化渲染策略

**关键要点**：

- ✅ 控件析构时必须调用 `unregister_animation(this)`
- ✅ 动画完成时回调必须返回 `false`
- ✅ 应用层在主循环中调用 `tick_all(dt)` 驱动所有动画
- ✅ 使用 `has_active()` 优化渲染策略
- ❌ 不要在 tick 回调中递归调用 `tick_all()`
- ❌ 不要在 tick 回调中修改动画列表
- ❌ 不要使用栈对象地址作为 handle

`AnimationClock` 与 `Storyboard`、`VisualStateManager` 等组件协同工作，构成完整的 UI 动画系统。
