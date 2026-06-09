# mine.ui.event —— 路由事件系统模块

## 模块概述

`mine.ui.event` 提供类似 WPF/UWP 的**路由事件**（Routed Event）系统：事件沿可视化树传播，支持冒泡（Bubble）、隧道（Tunnel）、直接（Direct）三种策略。

| 核心类型 | 职责 |
|---------|------|
| `RoutedEvent` | 全局唯一的事件描述符（名称、路由策略） |
| `RoutedEventArgs` | 事件参数基类（含 `handled` 标记） |
| `IRoutedEventTarget` | 事件目标接口（`add_handler` / `invoke_handlers`） |
| `EventManager` | 事件派发引擎（按策略沿树传播） |
| `ICommand` / `RelayCommand` | MVVM 命令抽象 |

**依赖**：`mine.core`。

---

## 1. 路由策略 —— `RoutingStrategy`

**文件**：`<mine/ui/event/RoutingStrategy.h>`

```cpp
namespace mine::ui {

enum class RoutingStrategy : uint8_t {
    Direct = 0,  // 仅在目标元素触发，不沿树传播
    Bubble = 1,  // 从目标向根部逐级冒泡
    Tunnel = 2,  // 从根部向目标逐级隧道（Preview 事件专用）
};

} // namespace mine::ui
```

---

## 2. 事件描述符 —— `RoutedEvent`

**文件**：`<mine/ui/event/RoutedEvent.h>`

全局唯一的事件定义，通过 `register_event<T>()` 模板注册。

```cpp
namespace mine::ui {

class RoutedEvent {
public:
    // 不可拷贝/移动（全局唯一）
    RoutedEvent(const RoutedEvent&) = delete;
    RoutedEvent& operator=(const RoutedEvent&) = delete;

    StringView      name()     const noexcept;  // 事件名称
    RoutingStrategy strategy() const noexcept;  // 路由策略
    TypeId          owner_type() const noexcept; // 所有者类型

    bool operator==(const RoutedEvent&) const noexcept;
    bool operator!=(const RoutedEvent&) const noexcept;
};

// 注册路由事件
template<typename TOwner>
const RoutedEvent& register_event(StringView name, RoutingStrategy strategy);

} // namespace mine::ui
```

### 使用示例

```cpp
// 头文件声明
class Button : public Control {
public:
    static const RoutedEvent& ClickEvent();
};

// 实现文件注册
const RoutedEvent& Button::ClickEvent() {
    static const RoutedEvent& ev =
        register_event<Button>("Click", RoutingStrategy::Bubble);
    return ev;
}
```

---

## 3. 事件参数 —— `RoutedEventArgs`

**文件**：`<mine/ui/event/RoutedEventArgs.h>`

```cpp
namespace mine::ui {

class RoutedEventArgs {
public:
    explicit RoutedEventArgs(const RoutedEvent& ev) noexcept;

    const RoutedEvent& routed_event() const noexcept;

    // 标记事件已处理（停止后续传播）
    bool handled() const noexcept;
    void set_handled(bool value) noexcept;

    // 事件源
    IRoutedEventTarget* original_source() const noexcept;  // 命中测试的原始目标
    IRoutedEventTarget* source()         const noexcept;  // 当前触发节点
    void set_original_source(IRoutedEventTarget* s) noexcept;
    void set_source(IRoutedEventTarget* s) noexcept;
};

} // namespace mine::ui
```

---

## 4. 事件目标接口 —— `IRoutedEventTarget`

**文件**：`<mine/ui/event/IRoutedEventTarget.h>`

可视化树中每个节点需实现此接口以参与事件路由。

```cpp
namespace mine::ui {

using RoutedEventHandlerFn = void (*)(void*            sender,
                                     RoutedEventArgs& args,
                                     void*            user_data);

class IRoutedEventTarget {
public:
    virtual ~IRoutedEventTarget() = default;

    // 返回父节点（nullptr = 根节点）
    virtual IRoutedEventTarget* parent_target() const noexcept = 0;

    // 触发本节点上注册到指定事件的所有处理器
    virtual void invoke_handlers(const RoutedEvent& event,
                                  RoutedEventArgs&   args) noexcept = 0;
};

} // namespace mine::ui
```

---

## 5. 事件派发引擎 —— `EventManager`

**文件**：`<mine/ui/event/EventManager.h>`

```cpp
namespace mine::ui {

class EventManager {
public:
    // 从 source 开始按事件策略沿树传播
    static void raise(IRoutedEventTarget& source,
                      RoutedEventArgs&    args) noexcept;
};

} // namespace mine::ui
```

### 传播行为

| 策略 | 行为 |
|------|------|
| `Direct` | 仅在 `source` 上调用 `invoke_handlers`，不传播 |
| `Bubble` | `source → parent → ... → root`，遇 `handled=true` 停止 |
| `Tunnel` | `root → ... → parent → source`，遇 `handled=true` 停止 |

---

## 6. 处理器注册 —— `add_handler` / `remove_handler`

这些方法定义在 `Visual` 基类（`mine.ui.visual`）中，但属于事件系统的一部分：

```cpp
// Visual 中（继承 IRoutedEventTarget）
void add_handler(const RoutedEvent& event,
                 RoutedEventHandlerFn fn,
                 void* user_data = nullptr);

void remove_handler(const RoutedEvent& event,
                    RoutedEventHandlerFn fn,
                    void* user_data = nullptr);
```

### 使用示例

```cpp
// 在构造函数中注册
add_handler(input::MouseEnterEvent(), &Button::on_mouse_enter_router, this);
add_handler(input::MouseLeaveEvent(), &Button::on_mouse_leave_router, this);

// 静态路由函数
static void on_mouse_enter_router(void* sender, RoutedEventArgs& args, void* user_data) {
    auto* self = static_cast<Button*>(user_data);
    self->is_hovered_ = true;
    self->update_visual_state();
}
```

> **注意**：`MouseEnter`/`MouseLeave` 使用 `RoutingStrategy::Direct`，仅在目标元素触发；`MouseDown`/`MouseUp` 使用 `Bubble`，可被父元素拦截。

---

## 7. 命令 —— `ICommand` / `RelayCommand`

**文件**：`<mine/ui/event/ICommand.h>`、`<mine/ui/event/RelayCommand.h>`

MVVM 模式的命令抽象。

```cpp
namespace mine::ui {

class ICommand {
public:
    virtual ~ICommand() = default;

    virtual void execute(const Variant& parameter) = 0;
    virtual bool can_execute(const Variant& parameter) const = 0;

    // 订阅 can_execute 变更（返回 token 用于取消）
    using CanExecuteChangedFn = void (*)(ICommand* sender, void* user_data);
    virtual uint64_t subscribe_can_execute_changed(
        CanExecuteChangedFn fn, void* user_data = nullptr) = 0;
    virtual void unsubscribe_can_execute_changed(uint64_t token) = 0;
};

// 便捷实现：将 execute/can_execute 委托给函数对象
class RelayCommand : public ICommand {
public:
    using ExecuteFn = void (*)(const Variant& parameter, void* user_data);
    using CanExecuteFn = bool (*)(const Variant& parameter, void* user_data);

    RelayCommand(ExecuteFn exec, CanExecuteFn can_exec = nullptr,
                 void* user_data = nullptr) noexcept;

    void execute(const Variant& parameter) override;
    bool can_execute(const Variant& parameter) const override;
    // ...
};

} // namespace mine::ui
```

---

## 8. 典型事件流（以鼠标按下为例）

```
平台 WindowEvent (WM_LBUTTONDOWN)
  → InputRouter::dispatch_mouse_event()
    → UIElement::hit_test(pos)                    // 确定命中目标
    → EventManager::raise(target, PreviewMouseDown) // 隧道
    → EventManager::raise(target, MouseDown)        // 冒泡
      → target.invoke_handlers(MouseDown, args)
        → Button::on_mouse_down_router()          // Button 的处理器
          → is_pressed_ = true
          → update_visual_state()                 // 驱动 VSM
```

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.ui.input` | 定义具体事件（`MouseEnterEvent()` 等），`InputRouter` 将平台事件转为路由事件 |
| `mine.ui.visual` | `Visual` 实现 `IRoutedEventTarget`，提供 `add_handler`/`invoke_handlers` |
| `mine.ui.controls` | 控件注册事件处理器并定义行为 |
