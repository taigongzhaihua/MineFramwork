# RoutedEvent 详细接口文档

## 概述

`RoutedEvent` 是 `mine.ui.event` 模块的**路由事件描述符**。

**核心特性：**
- **全局唯一**：同一所有者类型 + 名称只能注册一次
- **地址稳定**：描述符由 `RoutedEventRegistry` 管理生命周期，返回常量引用
- **标识符**：作为 `RoutedEventArgs` 的标识符，在路由过程中标识事件类型
- **类型安全注册**：`register_event<TOwner>()` 提供类型安全的注册接口

---

## 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/RoutedEvent.h
```

---

## 类定义

```cpp
class MINE_UI_EVENT_API RoutedEvent {
public:
    [[nodiscard]] core::StringView  name()        const noexcept;
    [[nodiscard]] core::TypeId      owner_type()  const noexcept;
    [[nodiscard]] RoutingStrategy   strategy()    const noexcept;

    // 禁止拷贝和赋值
    RoutedEvent(const RoutedEvent&)            = delete;
    RoutedEvent& operator=(const RoutedEvent&) = delete;

private:
    RoutedEvent(const char*   name,
                core::TypeId  owner_type,
                RoutingStrategy strategy) noexcept;

    const char*     name_;
    core::TypeId    owner_type_;
    RoutingStrategy strategy_;
};
```

**描述**：路由事件描述符。

**特征**：
- 禁止拷贝和赋值（描述符由全局注册表管理，不可复制）
- 私有构造函数（仅由 `register_event` 内部构造）
- 地址稳定（进程生命周期内保持有效）

---

## 成员方法

### name

```cpp
[[nodiscard]] core::StringView name() const noexcept;
```

**描述**：返回事件名称。

**返回值**：
- 事件名称（通常为 `"Click"`、`"KeyDown"` 等）

**示例**：
```cpp
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

std::cout << "Event name: " << ClickEvent.name() << std::endl;
// 输出：Event name: Click
```

---

### owner_type

```cpp
[[nodiscard]] core::TypeId owner_type() const noexcept;
```

**描述**：返回事件所属类型。

**返回值**：
- 事件所属类型（注册者的类型 ID）

**示例**：
```cpp
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

core::TypeId type = ClickEvent.owner_type();
std::cout << "Owner type: " << type.name() << std::endl;
// 输出：Owner type: Button
```

---

### strategy

```cpp
[[nodiscard]] RoutingStrategy strategy() const noexcept;
```

**描述**：返回事件传播策略。

**返回值**：
- 事件传播策略（`Bubble`/`Tunnel`/`Direct`）

**示例**：
```cpp
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

RoutingStrategy strategy = ClickEvent.strategy();
if (strategy == RoutingStrategy::Bubble) {
    std::cout << "Bubble event" << std::endl;
}
```

---

## 注册 API

### register_event（底层接口）

```cpp
MINE_UI_EVENT_API
const RoutedEvent& register_event(core::StringView name,
                                   core::TypeId     owner_type,
                                   RoutingStrategy  strategy);
```

**描述**：注册路由事件描述符（底层接口）。

**参数**：
- `name`：事件名称（建议为字符串字面量，确保地址稳定）
- `owner_type`：所有者类型 ID
- `strategy`：传播策略

**返回值**：
- 对新注册的 `RoutedEvent` 描述符的常量引用（地址永久有效）

**注意**：
- 相同 `name` + `owner_type` 组合只能注册一次
- 重复注册时断言失败（Debug 模式）

**示例**：
```cpp
const RoutedEvent& ClickEvent =
    register_event("Click", core::TypeId::of<Button>(), RoutingStrategy::Bubble);
```

---

### register_event（类型安全模板）

```cpp
template<typename TOwner>
const RoutedEvent& register_event(core::StringView name,
                                   RoutingStrategy  strategy);
```

**描述**：注册路由事件描述符（类型安全便捷模板）。

**模板参数**：
- `TOwner`：事件所属类（注册者类型）

**参数**：
- `name`：事件名称
- `strategy`：传播策略

**返回值**：
- 对新注册的 `RoutedEvent` 描述符的常量引用

**示例**：
```cpp
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);
```

---

## 使用场景

### 1. 注册路由事件（典型模式）

```cpp
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutingStrategy.h>

using namespace mine::ui;

// ── 在类定义（.h）中声明 ────────────────────────────────────────────────
class Button {
public:
    static const RoutedEvent& ClickEvent;
};

// ── 在 .cpp 中注册 ──────────────────────────────────────────────────────
const RoutedEvent& Button::ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);
```

---

### 2. 注册冒泡路由事件

```cpp
// 注册冒泡路由事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

const RoutedEvent& MouseDownEvent =
    register_event<Control>("MouseDown", RoutingStrategy::Bubble);

const RoutedEvent& KeyDownEvent =
    register_event<Control>("KeyDown", RoutingStrategy::Bubble);
```

---

### 3. 注册隧道预览事件（配对使用）

```cpp
// 注册隧道预览事件
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);

// 注册冒泡事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);
```

---

### 4. 注册直接路由事件

```cpp
// 注册直接路由事件
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

const RoutedEvent& UnloadedEvent =
    register_event<Control>("Unloaded", RoutingStrategy::Direct);

const RoutedEvent& GotFocusEvent =
    register_event<Control>("GotFocus", RoutingStrategy::Direct);
```

---

### 5. 使用事件描述符

```cpp
// 注册事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 触发事件
RoutedEventArgs args{ClickEvent};
args.set_source(button_ptr);
args.set_original_source(button_ptr);
EventManager::raise(*button_element, args);

// 订阅事件
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // 获取事件信息
    const RoutedEvent& event = args.routed_event();
    std::cout << "Event name: " << event.name() << std::endl;
    std::cout << "Event strategy: " 
              << (event.strategy() == RoutingStrategy::Bubble ? "Bubble" : "Other")
              << std::endl;
});
```

---

## 最佳实践

### 1. 在 .cpp 文件静态初始化期注册

```cpp
// ✅ 推荐：在 .cpp 文件静态初始化期注册
// Button.h
class Button {
public:
    static const RoutedEvent& ClickEvent;
};

// Button.cpp
const RoutedEvent& Button::ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// ❌ 不推荐：在运行时动态注册
void init_button() {
    const RoutedEvent& ClickEvent =
        register_event<Button>("Click", RoutingStrategy::Bubble);
}
```

---

### 2. 使用类型安全模板注册

```cpp
// ✅ 推荐：使用类型安全模板注册
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// ❌ 不推荐：使用底层接口注册（不够类型安全）
const RoutedEvent& ClickEvent =
    register_event("Click", core::TypeId::of<Button>(), RoutingStrategy::Bubble);
```

---

### 3. 使用字符串字面量作为事件名称

```cpp
// ✅ 推荐：使用字符串字面量（地址稳定）
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// ❌ 不推荐：使用动态字符串（地址不稳定）
std::string event_name = "Click";
const RoutedEvent& ClickEvent =
    register_event<Button>(event_name.c_str(), RoutingStrategy::Bubble);
```

---

### 4. 配对注册 PreviewXxx + Xxx 事件

```cpp
// ✅ 推荐：配对注册 PreviewXxx + Xxx
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 用途：PreviewClick 可用于事件拦截/验证
```

---

## 常见陷阱

### 1. 重复注册相同事件

```cpp
// ❌ 问题：重复注册相同事件
const RoutedEvent& ClickEvent1 =
    register_event<Button>("Click", RoutingStrategy::Bubble);

const RoutedEvent& ClickEvent2 =
    register_event<Button>("Click", RoutingStrategy::Bubble);  // ❌ 断言失败！

// ✅ 解决：同一 name + owner_type 组合只能注册一次
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);
```

---

### 2. 使用动态字符串作为事件名称

```cpp
// ❌ 问题：使用动态字符串（地址不稳定）
std::string event_name = "Click";
const RoutedEvent& ClickEvent =
    register_event<Button>(event_name.c_str(), RoutingStrategy::Bubble);

// event_name 销毁后，ClickEvent.name() 指向无效内存

// ✅ 解决：使用字符串字面量（地址稳定）
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);
```

---

### 3. 拷贝事件描述符

```cpp
// ❌ 问题：拷贝事件描述符
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

RoutedEvent copy = ClickEvent;  // ❌ 编译错误：禁止拷贝

// ✅ 解决：使用常量引用
const RoutedEvent& event_ref = ClickEvent;
```

---

### 4. 忘记声明为静态成员

```cpp
// ❌ 问题：忘记声明为静态成员（每个实例都会注册）
class Button {
public:
    const RoutedEvent& ClickEvent =
        register_event<Button>("Click", RoutingStrategy::Bubble);
    // ❌ 每次创建 Button 实例都会注册，导致重复注册错误
};

// ✅ 解决：声明为静态成员
class Button {
public:
    static const RoutedEvent& ClickEvent;
};

const RoutedEvent& Button::ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);
```

---

## 完整示例

```cpp
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/event/EventManager.h>
#include <mine/ui/event/IRoutedEventTarget.h>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 定义控件类
// ────────────────────────────────────────────────────────────────────────────

class Button {
public:
    // 声明静态路由事件
    static const RoutedEvent& ClickEvent;
    static const RoutedEvent& PreviewClickEvent;
};

class Control {
public:
    // 声明静态路由事件
    static const RoutedEvent& LoadedEvent;
    static const RoutedEvent& KeyDownEvent;
};

// ────────────────────────────────────────────────────────────────────────────
// 注册路由事件（.cpp 文件静态初始化期）
// ────────────────────────────────────────────────────────────────────────────

// 冒泡事件
const RoutedEvent& Button::ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 隧道预览事件
const RoutedEvent& Button::PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);

// 直接事件
const RoutedEvent& Control::LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

// 冒泡事件
const RoutedEvent& Control::KeyDownEvent =
    register_event<Control>("KeyDown", RoutingStrategy::Bubble);

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // ── 查询事件信息 ────────────────────────────────────────────────────
    {
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "查询事件信息" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        std::cout << "Button.ClickEvent:" << std::endl;
        std::cout << "  Name: " << Button::ClickEvent.name() << std::endl;
        std::cout << "  Owner: " << Button::ClickEvent.owner_type().name() << std::endl;
        std::cout << "  Strategy: "
                  << (Button::ClickEvent.strategy() == RoutingStrategy::Bubble
                          ? "Bubble" : "Other")
                  << std::endl;
        
        std::cout << "Button.PreviewClickEvent:" << std::endl;
        std::cout << "  Name: " << Button::PreviewClickEvent.name() << std::endl;
        std::cout << "  Owner: " << Button::PreviewClickEvent.owner_type().name() << std::endl;
        std::cout << "  Strategy: "
                  << (Button::PreviewClickEvent.strategy() == RoutingStrategy::Tunnel
                          ? "Tunnel" : "Other")
                  << std::endl;
        
        std::cout << "Control.LoadedEvent:" << std::endl;
        std::cout << "  Name: " << Control::LoadedEvent.name() << std::endl;
        std::cout << "  Owner: " << Control::LoadedEvent.owner_type().name() << std::endl;
        std::cout << "  Strategy: "
                  << (Control::LoadedEvent.strategy() == RoutingStrategy::Direct
                          ? "Direct" : "Other")
                  << std::endl;
    }
    
    // ── 使用事件描述符 ──────────────────────────────────────────────────
    {
        std::cout << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "使用事件描述符" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        // 创建事件参数
        RoutedEventArgs args{Button::ClickEvent};
        
        // 查询事件信息
        const RoutedEvent& event = args.routed_event();
        std::cout << "Event from args:" << std::endl;
        std::cout << "  Name: " << event.name() << std::endl;
        std::cout << "  Owner: " << event.owner_type().name() << std::endl;
        std::cout << "  Strategy: "
                  << (event.strategy() == RoutingStrategy::Bubble
                          ? "Bubble" : "Other")
                  << std::endl;
    }
    
    // ── 事件注册模式对比 ────────────────────────────────────────────────
    {
        std::cout << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        std::cout << "事件注册模式对比" << std::endl;
        std::cout << "════════════════════════════════════════════════" << std::endl;
        
        std::cout << "冒泡事件（Bubble）：" << std::endl;
        std::cout << "  - Button.ClickEvent" << std::endl;
        std::cout << "  - Control.KeyDownEvent" << std::endl;
        
        std::cout << "隧道事件（Tunnel）：" << std::endl;
        std::cout << "  - Button.PreviewClickEvent" << std::endl;
        
        std::cout << "直接事件（Direct）：" << std::endl;
        std::cout << "  - Control.LoadedEvent" << std::endl;
    }
    
    return 0;
}
```

**输出：**
```
════════════════════════════════════════════════
查询事件信息
════════════════════════════════════════════════
Button.ClickEvent:
  Name: Click
  Owner: Button
  Strategy: Bubble
Button.PreviewClickEvent:
  Name: PreviewClick
  Owner: Button
  Strategy: Tunnel
Control.LoadedEvent:
  Name: Loaded
  Owner: Control
  Strategy: Direct

════════════════════════════════════════════════
使用事件描述符
════════════════════════════════════════════════
Event from args:
  Name: Click
  Owner: Button
  Strategy: Bubble

════════════════════════════════════════════════
事件注册模式对比
════════════════════════════════════════════════
冒泡事件（Bubble）：
  - Button.ClickEvent
  - Control.KeyDownEvent
隧道事件（Tunnel）：
  - Button.PreviewClickEvent
直接事件（Direct）：
  - Control.LoadedEvent
```

---

## 总结

`RoutedEvent` 是 `mine.ui.event` 模块的路由事件描述符，具备：

1. **全局唯一**：同一所有者类型 + 名称只能注册一次
2. **地址稳定**：描述符由 `RoutedEventRegistry` 管理生命周期，返回常量引用
3. **标识符**：作为 `RoutedEventArgs` 的标识符，在路由过程中标识事件类型
4. **类型安全注册**：`register_event<TOwner>()` 提供类型安全的注册接口

**使用建议**：
- 在 `.cpp` 文件静态初始化期注册（避免运行时动态注册）
- 使用类型安全模板注册（`register_event<TOwner>()`）
- 使用字符串字面量作为事件名称（地址稳定）
- 配对注册 `PreviewXxx` + `Xxx` 事件（用于事件拦截/验证）
- 声明为静态成员（避免每个实例都注册）
- 使用常量引用（禁止拷贝）
