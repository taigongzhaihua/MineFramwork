# IRoutedEventTarget 详细接口文档

## 概述

`IRoutedEventTarget` 是 `mine.ui.event` 模块的**路由事件目标接口**。

**核心特性：**
- **路由系统抽象**：使 `EventManager` 独立于具体 `UIElement` 实现
- **父节点遍历**：`parent_target()` 返回可视化树中的父节点
- **事件处理器触发**：`invoke_handlers()` 触发注册的事件处理器
- **轻量化设计**：事件处理器以函数指针 + 用户数据形式存储（避免 `std::function` 开销）

---

## 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/IRoutedEventTarget.h
```

---

## 类定义

```cpp
/// 路由事件处理器函数指针类型
using RoutedEventHandlerFn = void(*)(void* sender,
                                     RoutedEventArgs& args,
                                     void* user_data);

class IRoutedEventTarget {
public:
    virtual ~IRoutedEventTarget() = default;

    [[nodiscard]] virtual IRoutedEventTarget* parent_target() const noexcept = 0;

    virtual void invoke_handlers(const RoutedEvent& event,
                                  RoutedEventArgs&   args) noexcept = 0;
};
```

**描述**：路由事件目标接口。

**特征**：
- 纯虚接口（所有方法都是纯虚函数）
- 轻量化设计（事件处理器以函数指针 + 用户数据形式存储）
- 与具体 `UIElement` 解耦（在 `mine.ui.visual` 存在之前保持解耦）

---

## 类型别名

### RoutedEventHandlerFn

```cpp
using RoutedEventHandlerFn = void(*)(void* sender,
                                     RoutedEventArgs& args,
                                     void* user_data);
```

**描述**：路由事件处理器函数指针类型。

**参数**：
- `sender`：当前处理元素（与 `invoke_handlers` 的 `this` 对应）
- `args`：路由事件参数（可读写 `handled` 等状态）
- `user_data`：注册时传入的用户自定义数据

**示例**：
```cpp
void on_click(void* sender, RoutedEventArgs& args, void* user_data) {
    std::cout << "Button clicked" << std::endl;
}

// 注册事件处理器
button.add_handler(&ClickEvent, on_click, nullptr);
```

---

## 接口方法

### parent_target

```cpp
[[nodiscard]] virtual IRoutedEventTarget* parent_target() const noexcept = 0;
```

**描述**：返回可视化树中的父节点，若当前为根节点则返回 `nullptr`。

**返回值**：
- 父节点指针（若当前为根节点则返回 `nullptr`）

**使用场景**：
- `EventManager` 依赖此方法向上遍历路由路径（Bubble 策略）
- 或收集完整路径再反向遍历（Tunnel 策略）

**示例**：
```cpp
class Button : public IRoutedEventTarget {
public:
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;  // 返回父容器
    }

private:
    IRoutedEventTarget* parent_ = nullptr;
};
```

---

### invoke_handlers

```cpp
virtual void invoke_handlers(const RoutedEvent& event,
                              RoutedEventArgs&   args) noexcept = 0;
```

**描述**：触发此节点上注册到指定路由事件的所有处理器。

**参数**：
- `event`：当前派发的路由事件描述符
- `args`：路由事件参数（可被处理器修改 `handled` 状态等）

**实现要求**：
1. 遍历内部已注册的处理器列表
2. 对列表中匹配 `event` 的每个处理器调用回调
3. 每次调用后检查 `args.handled()`，若为 `true` 可提前停止本层处理

**示例**：
```cpp
class Button : public IRoutedEventTarget {
public:
    void invoke_handlers(const RoutedEvent& event,
                         RoutedEventArgs&   args) noexcept override {
        // 遍历已注册的处理器列表
        for (auto& handler : handlers_) {
            if (handler.event == &event) {
                // 调用处理器
                handler.fn(this, args, handler.user_data);
                
                // 检查 handled 状态
                if (args.handled()) {
                    break;  // 提前停止
                }
            }
        }
    }

private:
    struct HandlerEntry {
        const RoutedEvent*     event;
        RoutedEventHandlerFn  fn;
        void*                 user_data;
    };
    
    std::vector<HandlerEntry> handlers_;
};
```

---

## 使用场景

### 1. 实现路由事件目标（基本实现）

```cpp
#include <mine/ui/event/IRoutedEventTarget.h>
#include <mine/ui/event/RoutedEvent.h>
#include <vector>

using namespace mine::ui;

class SimpleControl : public IRoutedEventTarget {
public:
    // ── 实现 IRoutedEventTarget 接口 ────────────────────────────────────
    
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;
    }
    
    void invoke_handlers(const RoutedEvent& event,
                         RoutedEventArgs&   args) noexcept override {
        // 遍历已注册的处理器列表
        for (auto& handler : handlers_) {
            if (handler.event == &event) {
                // 调用处理器
                handler.fn(this, args, handler.user_data);
                
                // 检查 handled 状态
                if (args.handled()) {
                    break;  // 提前停止
                }
            }
        }
    }
    
    // ── 注册事件处理器 ──────────────────────────────────────────────────
    
    void add_handler(const RoutedEvent*    event,
                     RoutedEventHandlerFn fn,
                     void*                user_data = nullptr) {
        handlers_.push_back({event, fn, user_data});
    }
    
    // ── 设置父节点 ──────────────────────────────────────────────────────
    
    void set_parent(IRoutedEventTarget* parent) {
        parent_ = parent;
    }

private:
    struct HandlerEntry {
        const RoutedEvent*     event;
        RoutedEventHandlerFn  fn;
        void*                 user_data;
    };
    
    IRoutedEventTarget*       parent_ = nullptr;
    std::vector<HandlerEntry> handlers_;
};
```

---

### 2. 使用路由事件目标（注册事件处理器）

```cpp
// 注册路由事件
const RoutedEvent& ClickEvent =
    register_event<SimpleControl>("Click", RoutingStrategy::Bubble);

// 创建控件
SimpleControl window;
SimpleControl panel;
SimpleControl button;

// 构建可视化树
panel.set_parent(&window);
button.set_parent(&panel);

// 定义事件处理器
void on_click(void* sender, RoutedEventArgs& args, void* user_data) {
    std::cout << "Button clicked" << std::endl;
}

// 注册事件处理器
button.add_handler(&ClickEvent, on_click, nullptr);
panel.add_handler(&ClickEvent, on_click, nullptr);
window.add_handler(&ClickEvent, on_click, nullptr);

// 触发事件
RoutedEventArgs args{ClickEvent};
EventManager::raise(button, args);

// 输出：
// Button clicked
// Button clicked（panel 处理）
// Button clicked（window 处理）
```

---

### 3. 自定义事件处理器（携带用户数据）

```cpp
// 用户数据结构
struct UserData {
    int count = 0;
};

// 事件处理器（携带用户数据）
void on_click_with_data(void* sender, RoutedEventArgs& args, void* user_data) {
    auto* data = static_cast<UserData*>(user_data);
    data->count++;
    std::cout << "Button clicked " << data->count << " times" << std::endl;
}

// 注册事件处理器（携带用户数据）
UserData data;
button.add_handler(&ClickEvent, on_click_with_data, &data);

// 触发事件
RoutedEventArgs args{ClickEvent};
EventManager::raise(button, args);
EventManager::raise(button, args);

// 输出：
// Button clicked 1 times
// Button clicked 2 times
```

---

### 4. 实现事件处理器提前停止（handled 标志）

```cpp
class SimpleControl : public IRoutedEventTarget {
public:
    void invoke_handlers(const RoutedEvent& event,
                         RoutedEventArgs&   args) noexcept override {
        for (auto& handler : handlers_) {
            if (handler.event == &event) {
                // 调用处理器
                handler.fn(this, args, handler.user_data);
                
                // ✅ 检查 handled 状态，提前停止
                if (args.handled()) {
                    std::cout << "Event handled, stop invoking" << std::endl;
                    break;
                }
            }
        }
    }
    
    // ...
};

// 事件处理器（设置 handled 标志）
void on_click_1(void* sender, RoutedEventArgs& args, void* user_data) {
    std::cout << "Handler 1" << std::endl;
    args.set_handled(true);  // 标记已处理
}

void on_click_2(void* sender, RoutedEventArgs& args, void* user_data) {
    std::cout << "Handler 2" << std::endl;  // ❌ 不会执行
}

// 注册事件处理器
button.add_handler(&ClickEvent, on_click_1, nullptr);
button.add_handler(&ClickEvent, on_click_2, nullptr);

// 触发事件
RoutedEventArgs args{ClickEvent};
EventManager::raise(button, args);

// 输出：
// Handler 1
// Event handled, stop invoking
```

---

## 最佳实践

### 1. 实现 parent_target 返回父节点

```cpp
// ✅ 推荐：返回父节点指针
class Control : public IRoutedEventTarget {
public:
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;
    }

private:
    IRoutedEventTarget* parent_ = nullptr;
};

// ❌ 不推荐：返回 nullptr（无法路由）
class Control : public IRoutedEventTarget {
public:
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return nullptr;  // ❌ 无法路由到父节点
    }
};
```

---

### 2. 实现 invoke_handlers 检查 handled 标志

```cpp
// ✅ 推荐：检查 handled 标志，提前停止
void invoke_handlers(const RoutedEvent& event,
                     RoutedEventArgs&   args) noexcept override {
    for (auto& handler : handlers_) {
        if (handler.event == &event) {
            handler.fn(this, args, handler.user_data);
            
            if (args.handled()) {
                break;  // ✅ 提前停止
            }
        }
    }
}

// ❌ 不推荐：不检查 handled 标志（浪费性能）
void invoke_handlers(const RoutedEvent& event,
                     RoutedEventArgs&   args) noexcept override {
    for (auto& handler : handlers_) {
        if (handler.event == &event) {
            handler.fn(this, args, handler.user_data);
            // ❌ 不检查 handled 标志，继续调用后续处理器
        }
    }
}
```

---

### 3. 使用函数指针而非 std::function（性能优化）

```cpp
// ✅ 推荐：使用函数指针 + 用户数据（避免 std::function 开销）
using RoutedEventHandlerFn = void(*)(void* sender,
                                     RoutedEventArgs& args,
                                     void* user_data);

struct HandlerEntry {
    const RoutedEvent*     event;
    RoutedEventHandlerFn  fn;
    void*                 user_data;
};

// ❌ 不推荐：使用 std::function（性能开销）
struct HandlerEntry {
    const RoutedEvent*                              event;
    std::function<void(void*, RoutedEventArgs&)>   fn;
};
```

---

### 4. 避免循环依赖（父子节点）

```cpp
// ❌ 危险：循环依赖
control_a.set_parent(&control_b);
control_b.set_parent(&control_a);  // ❌ 循环依赖！

// ✅ 安全：单向依赖
control_a.set_parent(&control_b);
```

---

## 常见陷阱

### 1. parent_target 返回 nullptr 导致无法路由

```cpp
// ❌ 问题：parent_target 返回 nullptr
class Control : public IRoutedEventTarget {
public:
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return nullptr;  // ❌ 无法路由到父节点
    }
};

// 可视化树：window → panel → button
// 但 button.parent_target() 返回 nullptr
// 导致 Bubble 事件无法传播到 panel 和 window

// ✅ 解决：返回父节点指针
class Control : public IRoutedEventTarget {
public:
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;  // ✅ 返回父节点
    }

private:
    IRoutedEventTarget* parent_ = nullptr;
};
```

---

### 2. invoke_handlers 不检查 handled 标志

```cpp
// ❌ 问题：不检查 handled 标志
void invoke_handlers(const RoutedEvent& event,
                     RoutedEventArgs&   args) noexcept override {
    for (auto& handler : handlers_) {
        if (handler.event == &event) {
            handler.fn(this, args, handler.user_data);
            // ❌ 不检查 handled 标志，继续调用后续处理器
        }
    }
}

// ✅ 解决：检查 handled 标志，提前停止
void invoke_handlers(const RoutedEvent& event,
                     RoutedEventArgs&   args) noexcept override {
    for (auto& handler : handlers_) {
        if (handler.event == &event) {
            handler.fn(this, args, handler.user_data);
            
            if (args.handled()) {
                break;  // ✅ 提前停止
            }
        }
    }
}
```

---

### 3. 循环依赖（父子节点）

```cpp
// ❌ 问题：循环依赖
control_a.set_parent(&control_b);
control_b.set_parent(&control_a);  // ❌ 循环依赖！

// EventManager::raise() 可能会陷入无限循环

// ✅ 解决：单向依赖
control_a.set_parent(&control_b);
```

---

### 4. 忘记实现 invoke_handlers

```cpp
// ❌ 问题：忘记实现 invoke_handlers
class Control : public IRoutedEventTarget {
public:
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;
    }
    
    // ❌ 忘记实现 invoke_handlers
    // void invoke_handlers(...) { ... }

private:
    IRoutedEventTarget* parent_ = nullptr;
};

// 编译错误：Cannot instantiate abstract class

// ✅ 解决：实现 invoke_handlers
class Control : public IRoutedEventTarget {
public:
    void invoke_handlers(const RoutedEvent& event,
                         RoutedEventArgs&   args) noexcept override {
        // 实现逻辑
    }
};
```

---

## 完整示例

```cpp
#include <mine/ui/event/IRoutedEventTarget.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/EventManager.h>
#include <vector>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 实现路由事件目标
// ────────────────────────────────────────────────────────────────────────────

class SimpleControl : public IRoutedEventTarget {
public:
    explicit SimpleControl(const char* name) : name_(name) {}
    
    // ── 实现 IRoutedEventTarget 接口 ────────────────────────────────────
    
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override {
        return parent_;
    }
    
    void invoke_handlers(const RoutedEvent& event,
                         RoutedEventArgs&   args) noexcept override {
        std::cout << "[" << name_ << "] Invoking handlers..." << std::endl;
        
        for (auto& handler : handlers_) {
            if (handler.event == &event) {
                handler.fn(this, args, handler.user_data);
                
                if (args.handled()) {
                    std::cout << "[" << name_ << "] Event handled, stop invoking" 
                              << std::endl;
                    break;
                }
            }
        }
    }
    
    // ── 注册事件处理器 ──────────────────────────────────────────────────
    
    void add_handler(const RoutedEvent*    event,
                     RoutedEventHandlerFn fn,
                     void*                user_data = nullptr) {
        handlers_.push_back({event, fn, user_data});
    }
    
    // ── 设置父节点 ──────────────────────────────────────────────────────
    
    void set_parent(IRoutedEventTarget* parent) {
        parent_ = parent;
    }
    
    // ── 获取名称 ────────────────────────────────────────────────────────
    
    [[nodiscard]] const char* name() const noexcept {
        return name_;
    }

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

// ────────────────────────────────────────────────────────────────────────────
// 事件处理器
// ────────────────────────────────────────────────────────────────────────────

void on_click(void* sender, RoutedEventArgs& args, void* user_data) {
    auto* control = static_cast<SimpleControl*>(sender);
    std::cout << "[" << control->name() << "] Click handled" << std::endl;
    
    // 可选：标记已处理
    // args.set_handled(true);
}

void on_click_with_block(void* sender, RoutedEventArgs& args, void* user_data) {
    auto* control = static_cast<SimpleControl*>(sender);
    std::cout << "[" << control->name() << "] Click handled and blocked" << std::endl;
    
    // 标记已处理，阻止后续传播
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
    
    // 场景 1：正常传播
    {
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "场景 1：正常传播" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        // 注册事件处理器
        button.add_handler(&ClickEvent, on_click, nullptr);
        panel.add_handler(&ClickEvent, on_click, nullptr);
        window.add_handler(&ClickEvent, on_click, nullptr);
        
        // 触发事件
        RoutedEventArgs args{ClickEvent};
        args.set_source(&button);
        args.set_original_source(&button);
        EventManager::raise(button, args);
        
        // 输出：
        // [Button] Invoking handlers...
        // [Button] Click handled
        // [Panel] Invoking handlers...
        // [Panel] Click handled
        // [Window] Invoking handlers...
        // [Window] Click handled
    }
    
    // 场景 2：事件拦截（在 Panel 阻止传播）
    {
        std::cout << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "场景 2：事件拦截（在 Panel 阻止传播）" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        // 清除之前的处理器
        SimpleControl button2("Button2");
        SimpleControl panel2("Panel2");
        SimpleControl window2("Window2");
        panel2.set_parent(&window2);
        button2.set_parent(&panel2);
        
        // 注册事件处理器（Panel 阻止传播）
        button2.add_handler(&ClickEvent, on_click, nullptr);
        panel2.add_handler(&ClickEvent, on_click_with_block, nullptr);  // 阻止传播
        window2.add_handler(&ClickEvent, on_click, nullptr);
        
        // 触发事件
        RoutedEventArgs args{ClickEvent};
        args.set_source(&button2);
        args.set_original_source(&button2);
        EventManager::raise(button2, args);
        
        // 输出：
        // [Button2] Invoking handlers...
        // [Button2] Click handled
        // [Panel2] Invoking handlers...
        // [Panel2] Click handled and blocked
        // （Window2 不会执行，因为 Panel2 已阻止传播）
    }
    
    return 0;
}
```

---

## 总结

`IRoutedEventTarget` 是 `mine.ui.event` 模块的路由事件目标接口，具备：

1. **路由系统抽象**：使 `EventManager` 独立于具体 `UIElement` 实现
2. **父节点遍历**：`parent_target()` 返回可视化树中的父节点
3. **事件处理器触发**：`invoke_handlers()` 触发注册的事件处理器
4. **轻量化设计**：事件处理器以函数指针 + 用户数据形式存储（避免 `std::function` 开销）

**使用建议**：
- 实现 `parent_target` 返回父节点（支持路由）
- 实现 `invoke_handlers` 检查 `handled` 标志（提前停止）
- 使用函数指针而非 `std::function`（性能优化）
- 避免循环依赖（父子节点单向依赖）
- 实现所有纯虚函数（否则无法实例化）
