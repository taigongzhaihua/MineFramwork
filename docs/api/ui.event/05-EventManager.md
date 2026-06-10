# EventManager 详细接口文档

## 概述

`EventManager` 是 `mine.ui.event` 模块的**路由事件派发器**。

**核心特性：**
- **纯静态工具类**：不可实例化，仅提供静态方法
- **统一派发入口**：`raise()` 根据 `RoutingStrategy` 选择传播路径
- **三种传播算法**：
  - **Bubble**（冒泡）：从 `source` 沿可视化树向根部传播
  - **Tunnel**（隧道）：从根部沿可视化树向 `source` 传播（"Preview" 事件）
  - **Direct**（直接）：仅在 `source` 上触发，不传播
- **传播控制**：任意处理器将 `RoutedEventArgs::handled()` 设为 `true` 后，传播立即停止

---

## 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/EventManager.h
```

---

## 类定义

```cpp
class MINE_UI_EVENT_API EventManager {
public:
    EventManager()                               = delete;
    EventManager(const EventManager&)            = delete;
    EventManager& operator=(const EventManager&) = delete;

    static void raise(IRoutedEventTarget& source,
                      RoutedEventArgs&    args) noexcept;
};
```

**描述**：路由事件派发器（纯静态工具类，不可实例化）。

**特征**：
- 禁止实例化（构造函数删除）
- 禁止拷贝和赋值
- 仅提供静态方法

---

## 静态方法

### raise

```cpp
static void raise(IRoutedEventTarget& source,
                  RoutedEventArgs&    args) noexcept;
```

**描述**：派发路由事件。

**参数**：
- `source`：事件起源的目标元素（路由路径的叶节点）
- `args`：路由事件参数（内含事件描述符、源信息、Handled 标志）

**传播算法**：
- `RoutingStrategy::Bubble`：`source` → `parent` → ... → `root`
- `RoutingStrategy::Tunnel`：`root` → ... → `parent` → `source`
- `RoutingStrategy::Direct`：`source` only

**传播规则**：
- 任意处理器将 `args.handled()` 置为 `true` 后，传播立即停止（不再调用后续节点）
- Tunnel 事件收集完整路径后再反向派发，确保根部先处理

**示例**：
```cpp
// 注册事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 触发事件
RoutedEventArgs args{ClickEvent};
args.set_source(button_ptr);
args.set_original_source(button_ptr);
EventManager::raise(*button_element, args);
```

---

## 使用场景

### 1. 派发冒泡事件（Bubble）

```cpp
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutedEventArgs.h>

using namespace mine::ui;

// 注册冒泡事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 触发冒泡事件
void raise_click(Button* button) {
    RoutedEventArgs args{ClickEvent};
    args.set_source(button);
    args.set_original_source(button);
    EventManager::raise(*button, args);
}

// 可视化树：Window → StackPanel → Button
// 传播路径：Button → StackPanel → Window
```

---

### 2. 派发隧道事件（Tunnel）

```cpp
// 注册隧道事件
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);

// 触发隧道事件
void raise_preview_click(Button* button) {
    RoutedEventArgs args{PreviewClickEvent};
    args.set_source(button);
    args.set_original_source(button);
    EventManager::raise(*button, args);
}

// 可视化树：Window → StackPanel → Button
// 传播路径：Window → StackPanel → Button
```

---

### 3. 派发直接事件（Direct）

```cpp
// 注册直接事件
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

// 触发直接事件
void raise_loaded(Control* control) {
    RoutedEventArgs args{LoadedEvent};
    args.set_source(control);
    args.set_original_source(control);
    EventManager::raise(*control, args);
}

// 传播路径：Control only（不传播到父节点）
```

---

### 4. 配对派发 Tunnel + Bubble 事件

```cpp
// 注册隧道 + 冒泡事件
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 触发 Tunnel + Bubble 事件
void raise_click_with_preview(Button* button) {
    // 1. 触发 PreviewClick（Tunnel 阶段）
    {
        RoutedEventArgs args{PreviewClickEvent};
        args.set_source(button);
        args.set_original_source(button);
        EventManager::raise(*button, args);
        
        // 如果 PreviewClick 被标记为 handled，则不触发 Click
        if (args.handled()) {
            return;
        }
    }
    
    // 2. 触发 Click（Bubble 阶段）
    {
        RoutedEventArgs args{ClickEvent};
        args.set_source(button);
        args.set_original_source(button);
        EventManager::raise(*button, args);
    }
}

// 可视化树：Window → StackPanel → Button
// 传播路径：
//   Tunnel 阶段：Window.PreviewClick → StackPanel.PreviewClick → Button.PreviewClick
//   Bubble 阶段：Button.Click → StackPanel.Click → Window.Click
```

---

### 5. 事件传播控制（Handled 标志）

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

// 触发事件
RoutedEventArgs args{PreviewClickEvent};
args.set_source(button);
args.set_original_source(button);
EventManager::raise(*button, args);

// 输出：
// Window.PreviewClick（检查并标记 handled = true）
// （StackPanel.PreviewClick 和 Button.PreviewClick 不会执行）
```

---

## 最佳实践

### 1. 同时设置 source 和 original_source

```cpp
// ✅ 推荐：同时设置 source 和 original_source
RoutedEventArgs args{ClickEvent};
args.set_source(button);
args.set_original_source(button);  // ✅ 设置 original_source
EventManager::raise(*button, args);

// ❌ 不推荐：忘记设置 original_source
RoutedEventArgs args{ClickEvent};
args.set_source(button);
// ❌ 忘记设置 original_source
EventManager::raise(*button, args);
```

---

### 2. 检查 Handled 标志后决定是否继续传播

```cpp
// ✅ 推荐：检查 Handled 标志后决定是否继续传播
void raise_click_with_preview(Button* button) {
    // 1. 触发 PreviewClick（Tunnel 阶段）
    RoutedEventArgs preview_args{PreviewClickEvent};
    preview_args.set_source(button);
    preview_args.set_original_source(button);
    EventManager::raise(*button, preview_args);
    
    // 检查 Handled 标志
    if (preview_args.handled()) {
        return;  // ✅ PreviewClick 已阻止，不触发 Click
    }
    
    // 2. 触发 Click（Bubble 阶段）
    RoutedEventArgs click_args{ClickEvent};
    click_args.set_source(button);
    click_args.set_original_source(button);
    EventManager::raise(*button, click_args);
}

// ❌ 不推荐：不检查 Handled 标志
void raise_click_with_preview(Button* button) {
    RoutedEventArgs preview_args{PreviewClickEvent};
    EventManager::raise(*button, preview_args);
    
    // ❌ 不检查 Handled 标志，总是触发 Click
    RoutedEventArgs click_args{ClickEvent};
    EventManager::raise(*button, click_args);
}
```

---

### 3. 使用 Tunnel 事件拦截（在事件到达目标之前）

```cpp
// ✅ 推荐：使用 Tunnel 事件拦截
window.add_handler(PreviewKeyDownEvent, [](RoutedEventArgs& args) {
    auto& key_args = static_cast<KeyEventArgs&>(args);
    if (is_invalid_key(key_args.key_code())) {
        args.set_handled(true);  // ✅ 在事件到达目标之前拦截
    }
});

// ❌ 不推荐：使用 Bubble 事件拦截（事件已经到达目标）
window.add_handler(KeyDownEvent, [](RoutedEventArgs& args) {
    args.set_handled(true);  // ❌ 事件已经到达目标，拦截意义不大
});
```

---

### 4. 不在事件处理器中调用 raise（避免递归）

```cpp
// ❌ 危险：在事件处理器中调用 raise（可能导致递归）
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // ❌ 在事件处理器中再次触发事件（递归！）
    EventManager::raise(*button, args);
});

// ✅ 安全：在事件处理器外部调用 raise
void raise_click(Button* button) {
    RoutedEventArgs args{ClickEvent};
    EventManager::raise(*button, args);
}
```

---

## 常见陷阱

### 1. 忘记设置 original_source

```cpp
// ❌ 问题：忘记设置 original_source
RoutedEventArgs args{ClickEvent};
args.set_source(button);
// ❌ 忘记设置 original_source
EventManager::raise(*button, args);

// 在事件处理器中：
void on_click(RoutedEventArgs& args) {
    void* origin = args.original_source();  // nullptr！
}

// ✅ 解决：同时设置 source 和 original_source
RoutedEventArgs args{ClickEvent};
args.set_source(button);
args.set_original_source(button);  // ✅ 设置 original_source
EventManager::raise(*button, args);
```

---

### 2. Tunnel 事件忘记检查 Handled 标志

```cpp
// ❌ 问题：Tunnel 事件标记 Handled，但仍触发 Bubble 事件
void raise_click_with_preview(Button* button) {
    // 1. 触发 PreviewClick（Tunnel 阶段）
    RoutedEventArgs preview_args{PreviewClickEvent};
    EventManager::raise(*button, preview_args);
    
    // ❌ 不检查 Handled 标志，总是触发 Click
    RoutedEventArgs click_args{ClickEvent};
    EventManager::raise(*button, click_args);
}

// ✅ 解决：检查 Handled 标志后决定是否继续传播
void raise_click_with_preview(Button* button) {
    RoutedEventArgs preview_args{PreviewClickEvent};
    EventManager::raise(*button, preview_args);
    
    if (preview_args.handled()) {
        return;  // ✅ PreviewClick 已阻止，不触发 Click
    }
    
    RoutedEventArgs click_args{ClickEvent};
    EventManager::raise(*button, click_args);
}
```

---

### 3. 在事件处理器中调用 raise（递归）

```cpp
// ❌ 问题：在事件处理器中调用 raise（递归）
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // ❌ 在事件处理器中再次触发事件（递归！）
    EventManager::raise(*button, args);
});

// 触发事件
RoutedEventArgs args{ClickEvent};
EventManager::raise(*button, args);

// 结果：无限递归，栈溢出

// ✅ 解决：不在事件处理器中调用 raise
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // ✅ 仅处理事件，不再次触发
    std::cout << "Button clicked" << std::endl;
});
```

---

### 4. 误用 Direct 事件期望传播

```cpp
// ❌ 问题：Direct 事件期望传播到父节点
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

// 父容器订阅子控件的 Loaded 事件
parent.add_handler(LoadedEvent, [](RoutedEventArgs& args) {
    // ❌ 不会触发（Direct 事件不传播）
});

// 触发事件
RoutedEventArgs args{LoadedEvent};
EventManager::raise(*child, args);

// ✅ 解决：直接在子控件上订阅
child.add_handler(LoadedEvent, [](RoutedEventArgs& args) {
    // ✅ 会触发
});
```

---

## 完整示例

```cpp
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/event/IRoutedEventTarget.h>
#include <vector>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 实现路由事件目标
// ────────────────────────────────────────────────────────────────────────────

class SimpleControl : public IRoutedEventTarget {
public:
    explicit SimpleControl(const char* name) : name_(name) {}
    
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;
    }
    
    void invoke_handlers(const RoutedEvent& event,
                         RoutedEventArgs&   args) noexcept override {
        for (auto& handler : handlers_) {
            if (handler.event == &event) {
                handler.fn(this, args, handler.user_data);
                if (args.handled()) break;
            }
        }
    }
    
    void add_handler(const RoutedEvent*    event,
                     RoutedEventHandlerFn fn,
                     void*                user_data = nullptr) {
        handlers_.push_back({event, fn, user_data});
    }
    
    void set_parent(IRoutedEventTarget* parent) { parent_ = parent; }
    [[nodiscard]] const char* name() const noexcept { return name_; }

private:
    struct HandlerEntry {
        const RoutedEvent*     event;
        RoutedEventHandlerFn  fn;
        void*                 user_data;
    };
    
    const char*               name_;
    IRoutedEventTarget*       parent_ = nullptr;
    std::vector<HandlerEntry> handlers_;
};

// ────────────────────────────────────────────────────────────────────────────
// 注册路由事件
// ────────────────────────────────────────────────────────────────────────────

const RoutedEvent& ClickEvent =
    register_event<SimpleControl>("Click", RoutingStrategy::Bubble);

const RoutedEvent& PreviewClickEvent =
    register_event<SimpleControl>("PreviewClick", RoutingStrategy::Tunnel);

const RoutedEvent& LoadedEvent =
    register_event<SimpleControl>("Loaded", RoutingStrategy::Direct);

// ────────────────────────────────────────────────────────────────────────────
// 事件处理器
// ────────────────────────────────────────────────────────────────────────────

void on_event(void* sender, RoutedEventArgs& args, void* user_data) {
    auto* control = static_cast<SimpleControl*>(sender);
    const char* event_name = args.routed_event().name();
    std::cout << "[" << control->name() << "] " << event_name << " handled" << std::endl;
}

void on_event_with_block(void* sender, RoutedEventArgs& args, void* user_data) {
    auto* control = static_cast<SimpleControl*>(sender);
    const char* event_name = args.routed_event().name();
    std::cout << "[" << control->name() << "] " << event_name 
              << " handled and blocked" << std::endl;
    args.set_handled(true);
}

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // 创建控件
    SimpleControl window("Window");
    SimpleControl panel("Panel");
    SimpleControl button("Button");
    
    // 构建可视化树：Window → Panel → Button
    panel.set_parent(&window);
    button.set_parent(&panel);
    
    // ── 场景 1：冒泡事件（Bubble）────────────────────────────────────────
    {
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "场景 1：冒泡事件（Bubble）" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        button.add_handler(&ClickEvent, on_event, nullptr);
        panel.add_handler(&ClickEvent, on_event, nullptr);
        window.add_handler(&ClickEvent, on_event, nullptr);
        
        RoutedEventArgs args{ClickEvent};
        args.set_source(&button);
        args.set_original_source(&button);
        EventManager::raise(button, args);
        
        // 输出：
        // [Button] Click handled
        // [Panel] Click handled
        // [Window] Click handled
    }
    
    // ── 场景 2：隧道事件（Tunnel）───────────────────────────────────────
    {
        std::cout << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "场景 2：隧道事件（Tunnel）" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        SimpleControl window2("Window2");
        SimpleControl panel2("Panel2");
        SimpleControl button2("Button2");
        panel2.set_parent(&window2);
        button2.set_parent(&panel2);
        
        window2.add_handler(&PreviewClickEvent, on_event, nullptr);
        panel2.add_handler(&PreviewClickEvent, on_event, nullptr);
        button2.add_handler(&PreviewClickEvent, on_event, nullptr);
        
        RoutedEventArgs args{PreviewClickEvent};
        args.set_source(&button2);
        args.set_original_source(&button2);
        EventManager::raise(button2, args);
        
        // 输出：
        // [Window2] PreviewClick handled
        // [Panel2] PreviewClick handled
        // [Button2] PreviewClick handled
    }
    
    // ── 场景 3：直接事件（Direct）───────────────────────────────────────
    {
        std::cout << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "场景 3：直接事件（Direct）" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        SimpleControl window3("Window3");
        SimpleControl panel3("Panel3");
        SimpleControl button3("Button3");
        panel3.set_parent(&window3);
        button3.set_parent(&panel3);
        
        button3.add_handler(&LoadedEvent, on_event, nullptr);
        panel3.add_handler(&LoadedEvent, on_event, nullptr);
        window3.add_handler(&LoadedEvent, on_event, nullptr);
        
        RoutedEventArgs args{LoadedEvent};
        args.set_source(&button3);
        args.set_original_source(&button3);
        EventManager::raise(button3, args);
        
        // 输出：
        // [Button3] Loaded handled
        // （Panel3 和 Window3 不会执行，Direct 事件不传播）
    }
    
    // ── 场景 4：事件拦截（Handled 标志）─────────────────────────────────
    {
        std::cout << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "场景 4：事件拦截（Handled 标志）" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        SimpleControl window4("Window4");
        SimpleControl panel4("Panel4");
        SimpleControl button4("Button4");
        panel4.set_parent(&window4);
        button4.set_parent(&panel4);
        
        window4.add_handler(&PreviewClickEvent, on_event_with_block, nullptr);
        panel4.add_handler(&PreviewClickEvent, on_event, nullptr);
        button4.add_handler(&PreviewClickEvent, on_event, nullptr);
        
        RoutedEventArgs args{PreviewClickEvent};
        args.set_source(&button4);
        args.set_original_source(&button4);
        EventManager::raise(button4, args);
        
        // 输出：
        // [Window4] PreviewClick handled and blocked
        // （Panel4 和 Button4 不会执行，Window4 已阻止传播）
    }
    
    return 0;
}
```

---

## 总结

`EventManager` 是 `mine.ui.event` 模块的路由事件派发器，具备：

1. **纯静态工具类**：不可实例化，仅提供静态方法
2. **统一派发入口**：`raise()` 根据 `RoutingStrategy` 选择传播路径
3. **三种传播算法**：
   - **Bubble**（冒泡）：从 `source` 向根部传播
   - **Tunnel**（隧道）：从根部向 `source` 传播
   - **Direct**（直接）：仅在 `source` 上触发
4. **传播控制**：任意处理器标记 `handled = true` 后，传播立即停止

**使用建议**：
- 同时设置 `source` 和 `original_source`（用于事件源追踪）
- 检查 `Handled` 标志后决定是否继续传播（Tunnel + Bubble 配对使用）
- 使用 Tunnel 事件拦截（在事件到达目标之前）
- 不在事件处理器中调用 `raise`（避免递归）
- Direct 事件直接在目标上订阅（不期望传播）
