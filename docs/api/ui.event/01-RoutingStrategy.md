# RoutingStrategy 详细接口文档

## 概述

`RoutingStrategy` 是 `mine.ui.event` 模块的**路由事件传播策略枚举**。

**核心特性：**
- **Bubble**：冒泡（从事件源向可视化树根部逐级传播）
- **Tunnel**：隧道/Preview（从可视化树根部向事件源逐级传播）
- **Direct**：直接（仅在事件源上派发，不传播）

---

## 文件位置

```
src/mine/ui/event/api/include/mine/ui/event/RoutingStrategy.h
```

---

## 枚举定义

```cpp
enum class RoutingStrategy : uint8_t {
    /// 冒泡：从事件源向可视化树根部逐级传播
    Bubble,
    /// 隧道（Preview）：从可视化树根部向事件源逐级传播
    Tunnel,
    /// 直接：仅在事件源上派发，不向树上/下传播
    Direct,
};
```

**描述**：路由事件传播策略。

**特征**：
- 与 WPF 路由事件模型对齐
- 支持三种传播方式

---

## 枚举值说明

### Bubble

**描述**：冒泡：从事件源向可视化树根部逐级传播。

**行为**：
- 事件首先在源元素上触发
- 然后向父元素传播
- 继续向祖先元素传播
- 直到到达可视化树根部

**典型场景**：
- Click 事件（按钮点击冒泡到父容器）
- MouseDown/MouseUp 事件
- KeyDown/KeyUp 事件
- TextChanged 事件

**示例**：
```
可视化树：Window → StackPanel → Button

用户点击 Button：
1. Button.Click 触发
2. StackPanel.Click 触发
3. Window.Click 触发
```

---

### Tunnel

**描述**：隧道（Preview）：从可视化树根部向事件源逐级传播。

**行为**：
- 事件从可视化树根部开始
- 向下传播到父元素
- 继续向下传播到子元素
- 直到到达事件源元素

**典型场景**：
- PreviewClick 事件（在 Click 之前触发）
- PreviewMouseDown/PreviewMouseUp 事件
- PreviewKeyDown/PreviewKeyUp 事件
- PreviewTextChanged 事件

**示例**：
```
可视化树：Window → StackPanel → Button

用户点击 Button：
1. Window.PreviewClick 触发（Tunnel 阶段）
2. StackPanel.PreviewClick 触发（Tunnel 阶段）
3. Button.PreviewClick 触发（Tunnel 阶段）
4. Button.Click 触发（Bubble 阶段）
5. StackPanel.Click 触发（Bubble 阶段）
6. Window.Click 触发（Bubble 阶段）
```

---

### Direct

**描述**：直接：仅在事件源上派发，不向树上/下传播。

**行为**：
- 事件仅在源元素上触发
- 不向父元素传播
- 不向子元素传播

**典型场景**：
- Loaded 事件（控件加载完成）
- Unloaded 事件（控件卸载）
- GotFocus 事件（控件获得焦点）
- LostFocus 事件（控件失去焦点）

**示例**：
```
可视化树：Window → StackPanel → Button

Button.Loaded 触发：
1. Button.Loaded 触发（仅在 Button 上）
2. 不传播到 StackPanel 或 Window
```

---

## 传播顺序对比

### Tunnel + Bubble 组合（典型模式）

```
可视化树：Window → StackPanel → Button

用户点击 Button：

Tunnel 阶段（从根部向事件源）：
1. Window.PreviewClick
2. StackPanel.PreviewClick
3. Button.PreviewClick

Bubble 阶段（从事件源向根部）：
4. Button.Click
5. StackPanel.Click
6. Window.Click
```

---

### 仅 Bubble

```
可视化树：Window → StackPanel → Button

用户点击 Button：
1. Button.Click
2. StackPanel.Click
3. Window.Click
```

---

### 仅 Direct

```
可视化树：Window → StackPanel → Button

Button 加载完成：
1. Button.Loaded（仅此一个）
```

---

## 使用场景

### 1. 注册冒泡路由事件（最常用）

```cpp
#include <mine/ui/event/RoutingStrategy.h>
#include <mine/ui/event/RoutedEvent.h>

using namespace mine::ui;

// 注册一个冒泡路由事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 订阅事件
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // 处理点击事件
    std::cout << "Button clicked" << std::endl;
});
```

---

### 2. 注册隧道预览事件（配对使用）

```cpp
// 注册隧道预览事件（在冒泡事件之前触发）
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);

// 注册冒泡事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 订阅预览事件（可用于事件拦截/验证）
window.add_handler(PreviewClickEvent, [](RoutedEventArgs& args) {
    // 在 Click 之前拦截事件
    if (should_block_click) {
        args.set_handled(true);  // 阻止后续传播
    }
});

// 订阅冒泡事件
button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
    // 如果 PreviewClick 未阻止，则执行正常逻辑
    std::cout << "Button clicked" << std::endl;
});
```

---

### 3. 注册直接路由事件（不传播）

```cpp
// 注册直接路由事件
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

// 订阅事件
button.add_handler(LoadedEvent, [](RoutedEventArgs& args) {
    // 仅在 button 加载完成时触发
    std::cout << "Button loaded" << std::endl;
});
```

---

## 典型事件分类

### 冒泡事件（Bubble）

```cpp
// 鼠标事件
RoutedEvent MouseDownEvent("MouseDown", RoutingStrategy::Bubble);
RoutedEvent MouseUpEvent("MouseUp", RoutingStrategy::Bubble);
RoutedEvent MouseMoveEvent("MouseMove", RoutingStrategy::Bubble);
RoutedEvent ClickEvent("Click", RoutingStrategy::Bubble);

// 键盘事件
RoutedEvent KeyDownEvent("KeyDown", RoutingStrategy::Bubble);
RoutedEvent KeyUpEvent("KeyUp", RoutingStrategy::Bubble);

// 输入事件
RoutedEvent TextChangedEvent("TextChanged", RoutingStrategy::Bubble);
```

---

### 隧道事件（Tunnel/Preview）

```cpp
// 鼠标预览事件
RoutedEvent PreviewMouseDownEvent("PreviewMouseDown", RoutingStrategy::Tunnel);
RoutedEvent PreviewMouseUpEvent("PreviewMouseUp", RoutingStrategy::Tunnel);
RoutedEvent PreviewClickEvent("PreviewClick", RoutingStrategy::Tunnel);

// 键盘预览事件
RoutedEvent PreviewKeyDownEvent("PreviewKeyDown", RoutingStrategy::Tunnel);
RoutedEvent PreviewKeyUpEvent("PreviewKeyUp", RoutingStrategy::Tunnel);

// 输入预览事件
RoutedEvent PreviewTextChangedEvent("PreviewTextChanged", RoutingStrategy::Tunnel);
```

---

### 直接事件（Direct）

```cpp
// 生命周期事件
RoutedEvent LoadedEvent("Loaded", RoutingStrategy::Direct);
RoutedEvent UnloadedEvent("Unloaded", RoutingStrategy::Direct);

// 焦点事件
RoutedEvent GotFocusEvent("GotFocus", RoutingStrategy::Direct);
RoutedEvent LostFocusEvent("LostFocus", RoutingStrategy::Direct);

// 布局事件
RoutedEvent SizeChangedEvent("SizeChanged", RoutingStrategy::Direct);
```

---

## 最佳实践

### 1. 配对使用 Tunnel + Bubble

```cpp
// ✅ 推荐：配对使用 PreviewXxx + Xxx
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 用途：PreviewClick 可用于事件拦截/验证
```

---

### 2. 生命周期事件使用 Direct

```cpp
// ✅ 推荐：生命周期事件使用 Direct（不需要传播）
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

// ❌ 不推荐：生命周期事件使用 Bubble（不必要的传播）
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Bubble);
```

---

### 3. 用户交互事件使用 Bubble

```cpp
// ✅ 推荐：用户交互事件使用 Bubble（允许父容器处理）
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 用途：父容器可以统一处理子控件的点击事件
```

---

### 4. 事件验证/拦截使用 Tunnel

```cpp
// ✅ 推荐：事件验证使用 PreviewXxx（Tunnel）
window.add_handler(PreviewKeyDownEvent, [](RoutedEventArgs& args) {
    // 在 KeyDown 之前验证输入
    if (is_invalid_key(args.key())) {
        args.set_handled(true);  // 阻止后续传播
    }
});

// 用途：在事件到达目标元素之前进行验证/拦截
```

---

## 常见陷阱

### 1. 直接事件期望传播

```cpp
// ❌ 问题：直接事件不会传播
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

// 父容器订阅子控件的 Loaded 事件
parent.add_handler(LoadedEvent, [](RoutedEventArgs& args) {
    // ❌ 不会触发（Direct 事件不传播）
});

// ✅ 解决：直接在子控件上订阅
child.add_handler(LoadedEvent, [](RoutedEventArgs& args) {
    // ✅ 会触发
});
```

---

### 2. Tunnel 事件传播顺序误解

```cpp
// ❌ 误解：Tunnel 事件从子元素向父元素传播
// ✅ 正确：Tunnel 事件从父元素向子元素传播（从根部到事件源）

可视化树：Window → StackPanel → Button

用户点击 Button：
1. Window.PreviewClick（Tunnel 从根部开始）
2. StackPanel.PreviewClick
3. Button.PreviewClick
4. Button.Click（Bubble 从事件源开始）
5. StackPanel.Click
6. Window.Click
```

---

### 3. 忘记配对使用 Tunnel + Bubble

```cpp
// ❌ 问题：仅注册 Bubble 事件，无法在事件到达目标之前拦截
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// ✅ 解决：配对注册 PreviewClick + Click
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);
```

---

## 完整示例

```cpp
#include <mine/ui/event/RoutingStrategy.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Window.h>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 注册路由事件
// ────────────────────────────────────────────────────────────────────────────

// 冒泡事件
const RoutedEvent& ClickEvent =
    register_event<Button>("Click", RoutingStrategy::Bubble);

// 隧道预览事件
const RoutedEvent& PreviewClickEvent =
    register_event<Button>("PreviewClick", RoutingStrategy::Tunnel);

// 直接事件
const RoutedEvent& LoadedEvent =
    register_event<Control>("Loaded", RoutingStrategy::Direct);

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    Window window;
    StackPanel panel;
    Button button;
    
    window.add_child(&panel);
    panel.add_child(&button);
    
    // ── Tunnel 阶段：PreviewClick（从根部向事件源） ────────────────────
    window.add_handler(PreviewClickEvent, [](RoutedEventArgs& args) {
        std::cout << "1. Window.PreviewClick (Tunnel)" << std::endl;
        // 可在此拦截事件
        if (should_block) {
            args.set_handled(true);
        }
    });
    
    panel.add_handler(PreviewClickEvent, [](RoutedEventArgs& args) {
        std::cout << "2. StackPanel.PreviewClick (Tunnel)" << std::endl;
    });
    
    button.add_handler(PreviewClickEvent, [](RoutedEventArgs& args) {
        std::cout << "3. Button.PreviewClick (Tunnel)" << std::endl;
    });
    
    // ── Bubble 阶段：Click（从事件源向根部） ────────────────────────────
    button.add_handler(ClickEvent, [](RoutedEventArgs& args) {
        std::cout << "4. Button.Click (Bubble)" << std::endl;
    });
    
    panel.add_handler(ClickEvent, [](RoutedEventArgs& args) {
        std::cout << "5. StackPanel.Click (Bubble)" << std::endl;
    });
    
    window.add_handler(ClickEvent, [](RoutedEventArgs& args) {
        std::cout << "6. Window.Click (Bubble)" << std::endl;
    });
    
    // ── Direct 阶段：Loaded（仅在事件源） ──────────────────────────────
    button.add_handler(LoadedEvent, [](RoutedEventArgs& args) {
        std::cout << "Button.Loaded (Direct, 仅在 Button)" << std::endl;
    });
    
    // 触发事件
    button.raise_event(ClickEvent);  // 触发完整的 Tunnel + Bubble 流程
    button.raise_event(LoadedEvent);  // 仅在 button 上触发
    
    return 0;
}
```

---

## 总结

`RoutingStrategy` 是 `mine.ui.event` 模块的路由事件传播策略枚举，具备：

1. **Bubble**：冒泡（从事件源向可视化树根部逐级传播，最常用）
2. **Tunnel**：隧道/Preview（从可视化树根部向事件源逐级传播，用于事件拦截）
3. **Direct**：直接（仅在事件源上派发，不传播，用于生命周期事件）

**使用建议**：
- 配对使用 Tunnel + Bubble（PreviewXxx + Xxx）
- 生命周期事件使用 Direct（Loaded/Unloaded/GotFocus/LostFocus）
- 用户交互事件使用 Bubble（Click/MouseDown/KeyDown）
- 事件验证/拦截使用 Tunnel（PreviewKeyDown 可拦截非法输入）
- Tunnel 事件从根部向事件源传播，Bubble 事件从事件源向根部传播
- Direct 事件不传播，仅在事件源上触发
