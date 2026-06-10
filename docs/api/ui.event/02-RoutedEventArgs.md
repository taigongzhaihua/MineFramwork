# RoutedEventArgs 详细接口文档

## 概述

`RoutedEventArgs` 是 `mine.ui.event` 模块的**路由事件参数基类**。

**核心特性：**
- **事件描述符**：绑定到 `RoutedEvent`（标识所属路由事件）
- **事件源追踪**：`source`（当前传播层）和 `original_source`（最初触发）
- **传播控制**：`handled` 标志（中止事件传播）
- **多态基类**：所有具体事件参数类（`KeyEventArgs`、`MouseEventArgs` 等）的基类

---

## 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/RoutedEventArgs.h
```

---

## 类定义

```cpp
class MINE_UI_EVENT_API RoutedEventArgs {
public:
    explicit RoutedEventArgs(const RoutedEvent& event) noexcept;
    virtual ~RoutedEventArgs() = default;

    // 禁止拷贝（防止多态切片）
    RoutedEventArgs(const RoutedEventArgs&)            = delete;
    RoutedEventArgs& operator=(const RoutedEventArgs&) = delete;

    // 事件描述符
    [[nodiscard]] const RoutedEvent& routed_event() const noexcept;

    // 事件源
    [[nodiscard]] void* source() const noexcept;
    [[nodiscard]] void* original_source() const noexcept;
    void set_source(void* src) noexcept;
    void set_original_source(void* src) noexcept;

    // 传播控制
    [[nodiscard]] bool handled() const noexcept;
    void set_handled(bool handled) noexcept;

private:
    const RoutedEvent* event_;
    void*              source_          = nullptr;
    void*              original_source_ = nullptr;
    bool               handled_         = false;
};
```

**描述**：路由事件参数基类。

**特征**：
- 禁止拷贝（防止多态切片）
- 虚析构函数（支持多态）
- 引用语义（通过引用传递）

---

## 构造函数

### RoutedEventArgs

```cpp
explicit RoutedEventArgs(const RoutedEvent& event) noexcept;
```

**描述**：构造事件参数，绑定到指定路由事件。

**参数**：
- `event`：此参数对应的路由事件描述符

**注意**：
- 通常在调用栈上构造
- 不应在事件处理器完成后持有 `RoutedEventArgs` 的指针/引用

**示例**：
```cpp
RoutedEventArgs args{Button::ClickEvent};
args.set_source(button_ptr);
EventManager::raise(*button_element, args);
```

---

## 事件描述符

### routed_event

```cpp
[[nodiscard]] const RoutedEvent& routed_event() const noexcept;
```

**描述**：返回触发此参数对象的路由事件描述符。

**返回值**：
- 路由事件描述符引用

**示例**：
```cpp
void on_click(RoutedEventArgs& args) {
    const RoutedEvent& event = args.routed_event();
    std::cout << "Event name: " << event.name() << std::endl;
}
```

---

## 事件源

### source

```cpp
[[nodiscard]] void* source() const noexcept;
```

**描述**：当前传播层的事件源（随路由路径变化）。

**返回值**：
- 当前传播层的事件源指针

**语义**：
- 在 WPF 语义中，`source` 在每个路由步骤中指向当前处理元素
- 而 `original_source` 始终保持最初触发事件的元素不变
- 当前 M1.1 阶段使用 `void*`；`mine.ui.visual` 就绪后类型会明确化

**示例**：
```cpp
可视化树：Window → StackPanel → Button

用户点击 Button：
1. Button.Click：source = Button, original_source = Button
2. StackPanel.Click：source = StackPanel, original_source = Button
3. Window.Click：source = Window, original_source = Button
```

---

### original_source

```cpp
[[nodiscard]] void* original_source() const noexcept;
```

**描述**：最初触发事件的元素（在整个路由过程中不变）。

**返回值**：
- 最初触发事件的元素指针

**示例**：
```cpp
void on_click(RoutedEventArgs& args) {
    void* current = args.source();  // 当前传播层
    void* origin = args.original_source();  // 最初触发
    
    std::cout << "Current source: " << current << std::endl;
    std::cout << "Original source: " << origin << std::endl;
}
```

---

### set_source

```cpp
void set_source(void* src) noexcept;
```

**描述**：设置当前传播层的事件源。

**参数**：
- `src`：事件源指针

**使用场景**：
- 通常由 `EventManager` 在路由过程中调用
- 用户代码很少需要手动调用

**示例**：
```cpp
RoutedEventArgs args{Button::ClickEvent};
args.set_source(button_ptr);
```

---

### set_original_source

```cpp
void set_original_source(void* src) noexcept;
```

**描述**：设置原始事件源（通常仅在 `raise()` 入口处设置一次）。

**参数**：
- `src`：原始事件源指针

**使用场景**：
- 通常由 `EventManager::raise()` 在入口处调用一次
- 用户代码很少需要手动调用

**示例**：
```cpp
RoutedEventArgs args{Button::ClickEvent};
args.set_original_source(button_ptr);
EventManager::raise(*button_element, args);
```

---

## 传播控制

### handled

```cpp
[[nodiscard]] bool handled() const noexcept;
```

**描述**：返回事件的 Handled 状态。

**返回值**：
- `true`：事件已被处理，阻止后续传播
- `false`：事件未被处理，继续传播

**示例**：
```cpp
void on_click(RoutedEventArgs& args) {
    if (args.handled()) {
        std::cout << "Event already handled" << std::endl;
        return;
    }
    // 处理事件
}
```

---

### set_handled

```cpp
void set_handled(bool handled) noexcept;
```

**描述**：设置事件的 Handled 状态。

**参数**：
- `handled`：
  - `true`：标记已处理并停止传播
  - `false`：恢复传播（罕见）

**使用场景**：
- 事件处理器可将此值设为 `true` 以中止事件继续在路由路径上传播
- `EventManager` 在每个路由步骤开始前检查此标志

**示例**：
```cpp
void on_click(RoutedEventArgs& args) {
    // 处理事件
    do_something();
    
    // 标记已处理，阻止后续传播
    args.set_handled(true);
}
```

---

## 使用场景

### 1. 基本事件参数

```cpp
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/EventManager.h>

using namespace mine::ui;

// 注册路由事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 触发事件
void raise_click(Button* button) {
    RoutedEventArgs args{ClickEvent};
    args.set_source(button);
    args.set_original_source(button);
    EventManager::raise(*button, args);
}

// 处理事件
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    std::cout << "Button clicked" << std::endl;
});
```

---

### 2. 自定义事件参数（继承 RoutedEventArgs）

```cpp
#include <mine/ui/event/RoutedEventArgs.h>

// 自定义键盘事件参数
class KeyEventArgs : public RoutedEventArgs {
public:
    KeyEventArgs(const RoutedEvent& event, int key_code)
        : RoutedEventArgs(event), key_code_(key_code) {}

    [[nodiscard]] int key_code() const noexcept { return key_code_; }

private:
    int key_code_;
};

// 注册键盘事件
const RoutedEvent& KeyDownEvent =
    register_event<Control>("KeyDown", RoutingStrategy::Bubble);

// 触发键盘事件
void raise_key_down(Control* control, int key_code) {
    KeyEventArgs args{KeyDownEvent, key_code};
    args.set_source(control);
    args.set_original_source(control);
    EventManager::raise(*control, args);
}

// 处理键盘事件
control.add_handler(KeyDownEvent, [](RoutedEventArgs& args) {
    // 向下转型到 KeyEventArgs
    auto& key_args = static_cast<KeyEventArgs&>(args);
    std::cout << "Key pressed: " << key_args.key_code() << std::endl;
});
```

---

### 3. 事件传播控制（Handled 标志）

```cpp
// 场景：在父容器拦截子控件的点击事件

// 可视化树：Window → StackPanel → Button

// 父容器订阅 PreviewClick（Tunnel 阶段）
window.add_handler(PreviewClickEvent, [](RoutedEventArgs& args) {
    // 在 Click 之前拦截事件
    if (should_block_click) {
        args.set_handled(true);  // 阻止后续传播
        std::cout << "Click blocked by Window" << std::endl;
    }
});

// Button 订阅 Click（Bubble 阶段）
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // 如果 PreviewClick 已标记 handled = true，则不会执行到这里
    std::cout << "Button clicked" << std::endl;
});
```

---

### 4. 事件源追踪（source vs original_source）

```cpp
// 可视化树：Window → StackPanel → Button

// Button 触发 Click 事件
button.raise_event(ClickEvent);

// StackPanel 处理 Click 事件
panel.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    void* current = args.source();  // StackPanel
    void* origin = args.original_source();  // Button
    
    std::cout << "Current source: " << current << std::endl;  // StackPanel
    std::cout << "Original source: " << origin << std::endl;  // Button
});

// Window 处理 Click 事件
window.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    void* current = args.source();  // Window
    void* origin = args.original_source();  // Button
    
    std::cout << "Current source: " << current << std::endl;  // Window
    std::cout << "Original source: " << origin << std::endl;  // Button
});
```

---

## 最佳实践

### 1. 禁止拷贝（防止多态切片）

```cpp
// ✅ 推荐：通过引用传递
void handle_event(RoutedEventArgs& args) {
    // 处理事件
}

// ❌ 不推荐：通过值传递（会导致多态切片）
void handle_event(RoutedEventArgs args) {
    // ❌ 丢失派生类信息（KeyEventArgs → RoutedEventArgs）
}
```

---

### 2. 不持有事件参数的指针/引用

```cpp
// ❌ 危险：持有事件参数的指针/引用
RoutedEventArgs* stored_args = nullptr;

button.add_handler(ClickEvent, [&stored_args](RoutedEventArgs& args) {
    stored_args = &args;  // ❌ 危险：args 在栈上，作用域结束后失效
});

// ✅ 安全：仅在事件处理器中使用
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // ✅ 仅在此作用域内使用 args
    do_something(args);
});
```

---

### 3. 自定义事件参数继承 RoutedEventArgs

```cpp
// ✅ 推荐：自定义事件参数继承 RoutedEventArgs
class MouseEventArgs : public RoutedEventArgs {
public:
    MouseEventArgs(const RoutedEvent& event, int x, int y)
        : RoutedEventArgs(event), x_(x), y_(y) {}

    [[nodiscard]] int x() const noexcept { return x_; }
    [[nodiscard]] int y() const noexcept { return y_; }

private:
    int x_, y_;
};

// ❌ 不推荐：自定义事件参数不继承 RoutedEventArgs
struct MouseEventData {
    int x, y;
};
```

---

### 4. 使用 Handled 标志阻止传播

```cpp
// ✅ 推荐：在 PreviewXxx 事件中使用 Handled 阻止传播
window.add_handler(PreviewKeyDownEvent, [](RoutedEventArgs& args) {
    auto& key_args = static_cast<KeyEventArgs&>(args);
    if (is_invalid_key(key_args.key_code())) {
        args.set_handled(true);  // ✅ 阻止后续传播
    }
});

// ❌ 不推荐：在 Xxx 事件中使用 Handled（已经传播到此）
button.add_handler(KeyDownEvent, [](RoutedEventArgs& args) {
    args.set_handled(true);  // ❌ 已经传播到此，阻止意义不大
});
```

---

## 常见陷阱

### 1. 多态切片

```cpp
// ❌ 问题：通过值传递导致多态切片
void handle_event(RoutedEventArgs args) {  // ❌ 值传递
    // 丢失派生类信息
}

KeyEventArgs key_args{KeyDownEvent, VK_RETURN};
handle_event(key_args);  // ❌ 切片为 RoutedEventArgs，丢失 key_code

// ✅ 解决：通过引用传递
void handle_event(RoutedEventArgs& args) {  // ✅ 引用传递
    // 保留派生类信息
}
```

---

### 2. 持有事件参数的指针/引用

```cpp
// ❌ 问题：持有事件参数的指针/引用
RoutedEventArgs* stored_args = nullptr;

button.add_handler(ClickEvent, [&stored_args](RoutedEventArgs& args) {
    stored_args = &args;  // ❌ args 在栈上，作用域结束后失效
});

// 稍后使用
stored_args->set_handled(true);  // ❌ 野指针！

// ✅ 解决：仅在事件处理器中使用
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // ✅ 仅在此作用域内使用 args
    args.set_handled(true);
});
```

---

### 3. 忘记设置 original_source

```cpp
// ❌ 问题：忘记设置 original_source
RoutedEventArgs args{ClickEvent};
args.set_source(button);
// ❌ 忘记设置 original_source
EventManager::raise(*button, args);

// ✅ 解决：同时设置 source 和 original_source
RoutedEventArgs args{ClickEvent};
args.set_source(button);
args.set_original_source(button);  // ✅ 设置 original_source
EventManager::raise(*button, args);
```

---

### 4. 误用 Handled 标志

```cpp
// ❌ 问题：在 Bubble 阶段设置 Handled（已经传播到此）
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    args.set_handled(true);  // ❌ 已经传播到 button，阻止意义不大
});

// ✅ 解决：在 Tunnel 阶段设置 Handled（阻止后续传播）
window.add_handler(PreviewClickEvent, [](RoutedEventArgs& args) {
    args.set_handled(true);  // ✅ 阻止 Click 事件传播到 button
});
```

---

## 完整示例

```cpp
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Window.h>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 自定义事件参数
// ────────────────────────────────────────────────────────────────────────────

class MouseEventArgs : public RoutedEventArgs {
public:
    MouseEventArgs(const RoutedEvent& event, int x, int y)
        : RoutedEventArgs(event), x_(x), y_(y) {}

    [[nodiscard]] int x() const noexcept { return x_; }
    [[nodiscard]] int y() const noexcept { return y_; }

private:
    int x_, y_;
};

// ────────────────────────────────────────────────────────────────────────────
// 注册路由事件
// ────────────────────────────────────────────────────────────────────────────

const RoutedEvent& MouseDownEvent =
    register_event<Control>("MouseDown", RoutingStrategy::Bubble);

const RoutedEvent& PreviewMouseDownEvent =
    register_event<Control>("PreviewMouseDown", RoutingStrategy::Tunnel);

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    Window window;
    StackPanel panel;
    Button button;
    
    window.add_child(&panel);
    panel.add_child(&button);
    
    // ── PreviewMouseDown（Tunnel 阶段）：事件拦截 ────────────────────────
    window.add_handler(PreviewMouseDownEvent, [](RoutedEventArgs& args) {
        auto& mouse_args = static_cast<MouseEventArgs&>(args);
        std::cout << "Window.PreviewMouseDown at (" 
                  << mouse_args.x() << ", " << mouse_args.y() << ")" << std::endl;
        
        // 在特定区域阻止事件传播
        if (mouse_args.x() < 100) {
            args.set_handled(true);
            std::cout << "Event blocked by Window" << std::endl;
        }
    });
    
    panel.add_handler(PreviewMouseDownEvent, [](RoutedEventArgs& args) {
        if (args.handled()) {
            std::cout << "StackPanel.PreviewMouseDown: Event already handled" << std::endl;
            return;
        }
        std::cout << "StackPanel.PreviewMouseDown" << std::endl;
    });
    
    // ── MouseDown（Bubble 阶段）：正常处理 ──────────────────────────────
    button.add_handler(MouseDownEvent, [](RoutedEventArgs& args) {
        auto& mouse_args = static_cast<MouseEventArgs&>(args);
        std::cout << "Button.MouseDown at (" 
                  << mouse_args.x() << ", " << mouse_args.y() << ")" << std::endl;
        
        // 检查事件源
        void* current = args.source();
        void* origin = args.original_source();
        std::cout << "Current source: " << current << " (Button)" << std::endl;
        std::cout << "Original source: " << origin << " (Button)" << std::endl;
    });
    
    panel.add_handler(MouseDownEvent, [](RoutedEventArgs& args) {
        void* current = args.source();
        void* origin = args.original_source();
        std::cout << "StackPanel.MouseDown" << std::endl;
        std::cout << "Current source: " << current << " (StackPanel)" << std::endl;
        std::cout << "Original source: " << origin << " (Button)" << std::endl;
    });
    
    window.add_handler(MouseDownEvent, [](RoutedEventArgs& args) {
        void* current = args.source();
        void* origin = args.original_source();
        std::cout << "Window.MouseDown" << std::endl;
        std::cout << "Current source: " << current << " (Window)" << std::endl;
        std::cout << "Original source: " << origin << " (Button)" << std::endl;
    });
    
    // ── 触发事件 ────────────────────────────────────────────────────────
    
    // 场景 1：正常传播（x >= 100）
    {
        MouseEventArgs args{MouseDownEvent, 200, 100};
        args.set_source(&button);
        args.set_original_source(&button);
        EventManager::raise(button, args);
        
        // 输出：
        // Window.PreviewMouseDown at (200, 100)
        // StackPanel.PreviewMouseDown
        // Button.MouseDown at (200, 100)
        // Current source: Button
        // Original source: Button
        // StackPanel.MouseDown
        // Current source: StackPanel
        // Original source: Button
        // Window.MouseDown
        // Current source: Window
        // Original source: Button
    }
    
    // 场景 2：事件拦截（x < 100）
    {
        MouseEventArgs args{MouseDownEvent, 50, 100};
        args.set_source(&button);
        args.set_original_source(&button);
        EventManager::raise(button, args);
        
        // 输出：
        // Window.PreviewMouseDown at (50, 100)
        // Event blocked by Window
        // StackPanel.PreviewMouseDown: Event already handled
        // （MouseDown 阶段不会执行）
    }
    
    return 0;
}
```

---

## 总结

`RoutedEventArgs` 是 `mine.ui.event` 模块的路由事件参数基类，具备：

1. **事件描述符**：绑定到 `RoutedEvent`（标识所属路由事件）
2. **事件源追踪**：`source`（当前传播层）和 `original_source`（最初触发）
3. **传播控制**：`handled` 标志（中止事件传播）
4. **多态基类**：禁止拷贝，支持派生类（`KeyEventArgs`、`MouseEventArgs` 等）

**使用建议**：
- 通过引用传递（防止多态切片）
- 不持有事件参数的指针/引用（栈上对象，作用域结束后失效）
- 自定义事件参数继承 `RoutedEventArgs`
- 使用 `Handled` 标志阻止传播（通常在 PreviewXxx 事件中）
- 同时设置 `source` 和 `original_source`（用于事件源追踪）
