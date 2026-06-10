# InputEvents 详细接口文档

## 概述

`InputEvents.h` 定义了 `mine.ui.input` 模块的**标准路由事件声明**。

**核心特性：**
- **WPF 风格事件对**：每个输入事件提供隧道（Preview）和冒泡事件对
- **三种路由策略**：隧道（根 → 目标）、冒泡（目标 → 根）、直接（仅目标）
- **Meyer's 单例**：事件在首次调用时注册，此后返回稳定引用
- **完整覆盖**：键盘、鼠标、字符输入、焦点事件

**事件策略说明：**
- **Tunnel（隧道）**：事件从根元素向目标元素传播（根 → 目标）
  - 命名：`PreviewXxx`
  - 用途：父元素提前拦截、验证输入
- **Bubble（冒泡）**：事件从目标元素向根元素传播（目标 → 根）
  - 命名：`Xxx`
  - 用途：目标元素处理、父元素后置处理
- **Direct（直接）**：事件仅派发到目标元素，不传播
  - 用途：焦点事件、合成事件

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/InputEvents.h
```

---

## 事件列表

### 键盘事件

| 事件函数 | 路由策略 | 参数类型 | 描述 |
|---------|---------|---------|------|
| `PreviewKeyDownEvent()` | Tunnel（隧道） | `KeyEventArgs` | 键盘按下（根 → 目标） |
| `KeyDownEvent()` | Bubble（冒泡） | `KeyEventArgs` | 键盘按下（目标 → 根） |
| `PreviewKeyUpEvent()` | Tunnel（隧道） | `KeyEventArgs` | 键盘释放（根 → 目标） |
| `KeyUpEvent()` | Bubble（冒泡） | `KeyEventArgs` | 键盘释放（目标 → 根） |

---

### 鼠标事件

| 事件函数 | 路由策略 | 参数类型 | 描述 |
|---------|---------|---------|------|
| `PreviewMouseDownEvent()` | Tunnel（隧道） | `MouseEventArgs` | 鼠标按下（根 → 目标） |
| `MouseDownEvent()` | Bubble（冒泡） | `MouseEventArgs` | 鼠标按下（目标 → 根） |
| `PreviewMouseUpEvent()` | Tunnel（隧道） | `MouseEventArgs` | 鼠标释放（根 → 目标） |
| `MouseUpEvent()` | Bubble（冒泡） | `MouseEventArgs` | 鼠标释放（目标 → 根） |
| `MouseMoveEvent()` | Bubble（冒泡） | `MouseEventArgs` | 鼠标移动（无 Preview 变体） |
| `MouseWheelEvent()` | Bubble（冒泡） | `MouseEventArgs` | 鼠标滚轮 |
| `MouseEnterEvent()` | Direct（直接） | `MouseEventArgs` | 鼠标进入元素（合成派发） |
| `MouseLeaveEvent()` | Direct（直接） | `MouseEventArgs` | 鼠标离开元素（合成派发） |

---

### 字符输入事件

| 事件函数 | 路由策略 | 参数类型 | 描述 |
|---------|---------|---------|------|
| `TextInputEvent()` | Bubble（冒泡） | `TextInputEventArgs` | 字符输入（向键盘焦点派发） |

---

### 焦点事件

| 事件函数 | 路由策略 | 参数类型 | 描述 |
|---------|---------|---------|------|
| `GotFocusEvent()` | Direct（直接） | `RoutedEventArgs` | 获得键盘焦点 |
| `LostFocusEvent()` | Direct（直接） | `RoutedEventArgs` | 失去键盘焦点 |

---

## 事件声明

### 键盘事件

#### PreviewKeyDownEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& PreviewKeyDownEvent();
```

**描述**：隧道策略键盘按下事件（根 → 目标）。

**路由策略**：Tunnel（隧道）

**参数类型**：`KeyEventArgs`

**使用场景**：
- 父元素提前拦截键盘输入
- 验证快捷键合法性
- 禁止特定按键

---

#### KeyDownEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& KeyDownEvent();
```

**描述**：冒泡策略键盘按下事件（目标 → 根）。

**路由策略**：Bubble（冒泡）

**参数类型**：`KeyEventArgs`

**使用场景**：
- 目标元素处理键盘输入
- 父元素后置处理

---

#### PreviewKeyUpEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& PreviewKeyUpEvent();
```

**描述**：隧道策略键盘释放事件（根 → 目标）。

**路由策略**：Tunnel（隧道）

**参数类型**：`KeyEventArgs`

---

#### KeyUpEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& KeyUpEvent();
```

**描述**：冒泡策略键盘释放事件（目标 → 根）。

**路由策略**：Bubble（冒泡）

**参数类型**：`KeyEventArgs`

---

### 鼠标事件

#### PreviewMouseDownEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& PreviewMouseDownEvent();
```

**描述**：隧道策略鼠标按下事件（根 → 目标）。

**路由策略**：Tunnel（隧道）

**参数类型**：`MouseEventArgs`

**使用场景**：
- 父元素提前拦截鼠标点击
- 验证点击区域合法性

---

#### MouseDownEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& MouseDownEvent();
```

**描述**：冒泡策略鼠标按下事件（目标 → 根）。

**路由策略**：Bubble（冒泡）

**参数类型**：`MouseEventArgs`

**使用场景**：
- 目标元素处理鼠标点击
- 父元素后置处理

---

#### PreviewMouseUpEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& PreviewMouseUpEvent();
```

**描述**：隧道策略鼠标释放事件（根 → 目标）。

**路由策略**：Tunnel（隧道）

**参数类型**：`MouseEventArgs`

---

#### MouseUpEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& MouseUpEvent();
```

**描述**：冒泡策略鼠标释放事件（目标 → 根）。

**路由策略**：Bubble（冒泡）

**参数类型**：`MouseEventArgs`

---

#### MouseMoveEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& MouseMoveEvent();
```

**描述**：冒泡策略鼠标移动事件（无 Preview 变体，直接派发）。

**路由策略**：Bubble（冒泡）

**参数类型**：`MouseEventArgs`

**说明**：
- 无 Preview 变体（性能考虑）
- 高频事件（鼠标移动时频繁触发）

---

#### MouseWheelEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& MouseWheelEvent();
```

**描述**：冒泡策略鼠标滚轮事件。

**路由策略**：Bubble（冒泡）

**参数类型**：`MouseEventArgs`

---

#### MouseEnterEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& MouseEnterEvent();
```

**描述**：直接策略鼠标进入元素事件（合成派发）。

**路由策略**：Direct（直接）

**参数类型**：`MouseEventArgs`

**说明**：
- 由 `InputRouter` 在悬停目标切换时合成派发
- 仅派发到目标元素，不传播

---

#### MouseLeaveEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& MouseLeaveEvent();
```

**描述**：直接策略鼠标离开元素事件（合成派发）。

**路由策略**：Direct（直接）

**参数类型**：`MouseEventArgs`

**说明**：
- 由 `InputRouter` 在悬停目标切换时合成派发
- 仅派发到目标元素，不传播

---

### 字符输入事件

#### TextInputEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& TextInputEvent();
```

**描述**：冒泡策略字符输入事件。

**路由策略**：Bubble（冒泡）

**参数类型**：`TextInputEventArgs`

**说明**：
- 由 `InputRouter` 收到平台 `WindowEventKind::Char` 时向键盘焦点派发
- 仅包含可打印字符（≥ 32）
- 控制字符（< 32）通过 `KeyDownEvent` 处理

---

### 焦点事件

#### GotFocusEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& GotFocusEvent();
```

**描述**：直接策略获得键盘焦点事件。

**路由策略**：Direct（直接）

**参数类型**：`RoutedEventArgs`

**说明**：
- 由 `InputRouter::set_keyboard_focus` 向新焦点元素派发
- 仅派发到新焦点元素，不传播

---

#### LostFocusEvent

```cpp
MINE_UI_INPUT_API const RoutedEvent& LostFocusEvent();
```

**描述**：直接策略失去键盘焦点事件。

**路由策略**：Direct（直接）

**参数类型**：`RoutedEventArgs`

**说明**：
- 由 `InputRouter::set_keyboard_focus` 向旧焦点元素派发
- 仅派发到旧焦点元素，不传播

---

## 使用场景

### 1. 注册事件处理器（虚方法）

```cpp
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/KeyEventArgs.h>

using namespace mine::ui::input;

class MyControl : public Visual {
protected:
    // 重写虚方法处理键盘事件
    void on_key_down(KeyEventArgs& args) override {
        if (args.key() == Key::Enter) {
            std::cout << "回车键按下" << std::endl;
            args.set_handled(true);
        } else {
            Visual::on_key_down(args);
        }
    }
    
    // 重写虚方法处理鼠标事件
    void on_mouse_down(MouseEventArgs& args) override {
        if (args.button() == MouseButton::Left) {
            std::cout << "鼠标左键按下" << std::endl;
            args.set_handled(true);
        }
    }
};
```

---

### 2. 注册事件处理器（add_handler）

```cpp
class MyControl : public Visual {
public:
    MyControl() {
        // 注册 KeyDown 事件处理器
        add_handler(KeyDownEvent(), &MyControl::on_key_down_handler, this);
        
        // 注册 MouseDown 事件处理器
        add_handler(MouseDownEvent(), &MyControl::on_mouse_down_handler, this);
    }

private:
    void on_key_down_handler(RoutedEventArgs& args) {
        auto& key_args = static_cast<KeyEventArgs&>(args);
        std::cout << "KeyDown 处理器" << std::endl;
    }
    
    void on_mouse_down_handler(RoutedEventArgs& args) {
        auto& mouse_args = static_cast<MouseEventArgs&>(args);
        std::cout << "MouseDown 处理器" << std::endl;
    }
};
```

---

### 3. Preview 事件拦截

```cpp
class ParentControl : public Visual {
protected:
    void on_preview_key_down(KeyEventArgs& args) override {
        // 隧道阶段拦截（根 → 目标）
        if (args.key() == Key::Escape) {
            std::cout << "父元素拦截 Escape 键" << std::endl;
            args.set_handled(true);  // 阻止继续传播到子元素
            return;
        }
        
        Visual::on_preview_key_down(args);
    }
};
```

---

### 4. 焦点事件处理

```cpp
class MyControl : public Visual {
protected:
    void on_got_focus(RoutedEventArgs& args) override {
        std::cout << "获得键盘焦点" << std::endl;
        m_has_focus = true;
        Visual::on_got_focus(args);
    }
    
    void on_lost_focus(RoutedEventArgs& args) override {
        std::cout << "失去键盘焦点" << std::endl;
        m_has_focus = false;
        Visual::on_lost_focus(args);
    }

private:
    bool m_has_focus = false;
};
```

---

### 5. 鼠标进入/离开事件

```cpp
class MyControl : public Visual {
protected:
    void on_mouse_enter(MouseEventArgs& args) override {
        std::cout << "鼠标进入元素" << std::endl;
        m_is_hovered = true;
        Visual::on_mouse_enter(args);
    }
    
    void on_mouse_leave(MouseEventArgs& args) override {
        std::cout << "鼠标离开元素" << std::endl;
        m_is_hovered = false;
        Visual::on_mouse_leave(args);
    }

private:
    bool m_is_hovered = false;
};
```

---

### 6. 字符输入事件

```cpp
class TextBox : public Visual {
public:
    TextBox() {
        // 注册 TextInput 事件处理器
        add_handler(TextInputEvent(), &TextBox::on_text_input_handler, this);
    }

private:
    void on_text_input_handler(RoutedEventArgs& args) {
        auto& text_args = static_cast<TextInputEventArgs&>(args);
        uint32_t codepoint = text_args.char_utf32();
        
        std::cout << "字符输入: U+" << std::hex << codepoint << std::endl;
        insert_char(codepoint);
        
        args.set_handled(true);
    }
    
    void insert_char(uint32_t codepoint) {
        // 插入字符到文本缓冲区
    }
};
```

---

## 最佳实践

### 1. 区分隧道和冒泡事件

```cpp
// ✅ 推荐：区分 Preview（隧道）和普通（冒泡）事件

// Preview 事件：父元素提前拦截
void on_preview_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Escape) {
        // 父元素拦截 Escape 键
        args.set_handled(true);
        return;
    }
}

// 普通事件：目标元素处理
void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        // 目标元素处理 Enter 键
        args.set_handled(true);
    }
}
```

---

### 2. 使用 set_handled 控制事件传播

```cpp
// ✅ 推荐：使用 set_handled 控制事件传播

void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        handle_enter();
        args.set_handled(true);  // 阻止事件继续传播
        return;
    }
    
    // 未处理的事件继续传播
    Visual::on_key_down(args);
}
```

---

### 3. Preview 事件用于验证输入

```cpp
// ✅ 推荐：Preview 事件用于验证输入

class NumericTextBox : public Visual {
protected:
    void on_preview_text_input(TextInputEventArgs& args) override {
        uint32_t codepoint = args.char_utf32();
        
        // 仅允许数字字符
        if (codepoint < '0' || codepoint > '9') {
            std::cout << "非数字字符被拦截" << std::endl;
            args.set_handled(true);  // 拦截非数字字符
            return;
        }
        
        Visual::on_preview_text_input(args);
    }
};
```

---

### 4. 焦点事件更新状态

```cpp
// ✅ 推荐：焦点事件更新 UI 状态

void on_got_focus(RoutedEventArgs& args) override {
    m_has_focus = true;
    update_visual_state();  // 更新 UI（如高亮边框）
    Visual::on_got_focus(args);
}

void on_lost_focus(RoutedEventArgs& args) override {
    m_has_focus = false;
    update_visual_state();  // 更新 UI（取消高亮）
    Visual::on_lost_focus(args);
}
```

---

## 常见陷阱

### 1. 混淆 Preview 和普通事件

```cpp
// ❌ 问题：在 Preview 事件中做目标元素处理
void on_preview_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        handle_enter();  // ❌ 应该在 on_key_down 中处理
        args.set_handled(true);
    }
}

// ✅ 解决：Preview 用于拦截，普通事件用于处理
void on_preview_key_down(KeyEventArgs& args) override {
    // Preview 仅用于拦截/验证
}

void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        handle_enter();  // ✅ 在普通事件中处理
        args.set_handled(true);
    }
}
```

---

### 2. 忘记调用基类处理

```cpp
// ❌ 问题：忘记调用基类处理
void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        handle_enter();
        args.set_handled(true);
    }
    // ❌ 忘记调用 Visual::on_key_down(args)
}

// ✅ 解决：调用基类处理（未处理的事件）
void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        handle_enter();
        args.set_handled(true);
        return;
    }
    
    Visual::on_key_down(args);  // ✅ 调用基类
}
```

---

### 3. 混淆 Direct 和 Bubble 事件

```cpp
// ⚠️ 注意：Direct 事件（如 GotFocus）不传播

// ❌ 问题：期望 GotFocus 事件冒泡
class ParentControl : public Visual {
protected:
    void on_got_focus(RoutedEventArgs& args) override {
        // ❌ 不会被子元素的 GotFocus 触发（Direct 策略）
        std::cout << "父元素收到 GotFocus" << std::endl;
    }
};

// ✅ 解决：使用其他机制监听子元素焦点变化
```

---

### 4. 忘记 set_handled 阻止传播

```cpp
// ❌ 问题：忘记 set_handled
void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        handle_enter();
        // ❌ 忘记 set_handled，事件继续传播到父元素
    }
}

// ✅ 解决：调用 set_handled
void on_key_down(KeyEventArgs& args) override {
    if (args.key() == Key::Enter) {
        handle_enter();
        args.set_handled(true);  // ✅ 阻止传播
    }
}
```

---

## 完整示例

```cpp
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/TextInputEventArgs.h>
#include <mine/ui/visual/Visual.h>
#include <iostream>

using namespace mine::ui::input;
using namespace mine::ui::visual;

// ────────────────────────────────────────────────────────────────────────────
// 父控件（拦截层）
// ────────────────────────────────────────────────────────────────────────────

class ParentControl : public Visual {
protected:
    // ════════════════════════════════════════════════════════════════════════
    // Preview 事件（隧道阶段，根 → 目标）
    // ════════════════════════════════════════════════════════════════════════
    
    void on_preview_key_down(KeyEventArgs& args) override {
        std::cout << "[Parent PreviewKeyDown] Key: " 
                  << static_cast<int>(args.key()) << std::endl;
        
        // 拦截 Escape 键
        if (args.key() == Key::Escape) {
            std::cout << "  父元素拦截 Escape 键" << std::endl;
            args.set_handled(true);
            return;
        }
        
        Visual::on_preview_key_down(args);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 普通事件（冒泡阶段，目标 → 根）
    // ════════════════════════════════════════════════════════════════════════
    
    void on_key_down(KeyEventArgs& args) override {
        std::cout << "[Parent KeyDown] Key: " 
                  << static_cast<int>(args.key()) << std::endl;
        
        Visual::on_key_down(args);
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 子控件（目标元素）
// ────────────────────────────────────────────────────────────────────────────

class ChildControl : public Visual {
public:
    ChildControl() {
        // 注册 TextInput 事件处理器
        add_handler(TextInputEvent(), &ChildControl::on_text_input_handler, this);
    }

protected:
    // ════════════════════════════════════════════════════════════════════════
    // Preview 事件（隧道阶段）
    // ════════════════════════════════════════════════════════════════════════
    
    void on_preview_key_down(KeyEventArgs& args) override {
        std::cout << "  [Child PreviewKeyDown] Key: " 
                  << static_cast<int>(args.key()) << std::endl;
        
        Visual::on_preview_key_down(args);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 普通事件（冒泡阶段）
    // ════════════════════════════════════════════════════════════════════════
    
    void on_key_down(KeyEventArgs& args) override {
        std::cout << "  [Child KeyDown] Key: " 
                  << static_cast<int>(args.key()) << std::endl;
        
        if (args.key() == Key::Enter) {
            std::cout << "    子元素处理 Enter 键" << std::endl;
            args.set_handled(true);
            return;
        }
        
        Visual::on_key_down(args);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标事件
    // ════════════════════════════════════════════════════════════════════════
    
    void on_mouse_enter(MouseEventArgs& args) override {
        std::cout << "  [Child MouseEnter]" << std::endl;
        Visual::on_mouse_enter(args);
    }
    
    void on_mouse_leave(MouseEventArgs& args) override {
        std::cout << "  [Child MouseLeave]" << std::endl;
        Visual::on_mouse_leave(args);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 焦点事件
    // ════════════════════════════════════════════════════════════════════════
    
    void on_got_focus(RoutedEventArgs& args) override {
        std::cout << "  [Child GotFocus]" << std::endl;
        Visual::on_got_focus(args);
    }
    
    void on_lost_focus(RoutedEventArgs& args) override {
        std::cout << "  [Child LostFocus]" << std::endl;
        Visual::on_lost_focus(args);
    }

private:
    void on_text_input_handler(RoutedEventArgs& args) {
        auto& text_args = static_cast<TextInputEventArgs&>(args);
        uint32_t codepoint = text_args.char_utf32();
        
        std::cout << "  [Child TextInput] U+" << std::hex << codepoint 
                  << std::dec << std::endl;
        
        args.set_handled(true);
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    ParentControl parent;
    ChildControl child;
    
    // 建立父子关系
    parent.add_child(&child);
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 键盘事件传播（Enter 键）
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "=== 键盘事件传播（Enter 键） ===" << std::endl;
    KeyEventArgs args1(KeyDownEvent(), Key::Enter, 0, ModifierKeys::None, false);
    EventManager::raise(child, args1);
    // 输出：
    // [Parent PreviewKeyDown] Key: 13 (隧道阶段，父→子)
    //   [Child PreviewKeyDown] Key: 13 (到达子元素)
    //   [Child KeyDown] Key: 13 (冒泡阶段，子→父)
    //     子元素处理 Enter 键
    // （事件被 set_handled，不继续传播到父元素）
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 键盘事件拦截（Escape 键）
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 键盘事件拦截（Escape 键） ===" << std::endl;
    KeyEventArgs args2(KeyDownEvent(), Key::Escape, 0, ModifierKeys::None, false);
    EventManager::raise(child, args2);
    // 输出：
    // [Parent PreviewKeyDown] Key: 27 (隧道阶段)
    //   父元素拦截 Escape 键
    // （事件被父元素拦截，不到达子元素）
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 焦点事件（Direct 策略，不传播）
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 焦点事件 ===" << std::endl;
    RoutedEventArgs args3(GotFocusEvent());
    EventManager::raise(child, args3);
    // 输出：
    //   [Child GotFocus] (仅派发到子元素，不传播)
    
    return 0;
}
```

---

## 总结

`InputEvents.h` 定义了 `mine.ui.input` 模块的标准路由事件声明，具备：

1. **WPF 风格事件对**：每个输入事件提供隧道（Preview）和冒泡事件对
2. **三种路由策略**：
   - **Tunnel（隧道）**：根 → 目标（PreviewXxx）
   - **Bubble（冒泡）**：目标 → 根（Xxx）
   - **Direct（直接）**：仅目标（GotFocus、LostFocus、MouseEnter、MouseLeave）
3. **Meyer's 单例**：事件在首次调用时注册，此后返回稳定引用
4. **完整覆盖**：键盘、鼠标、字符输入、焦点事件

**事件列表**：
- 键盘：PreviewKeyDown、KeyDown、PreviewKeyUp、KeyUp
- 鼠标：PreviewMouseDown、MouseDown、PreviewMouseUp、MouseUp、MouseMove、MouseWheel、MouseEnter、MouseLeave
- 字符输入：TextInput
- 焦点：GotFocus、LostFocus

**使用建议**：
- **区分隧道和冒泡事件**（Preview 用于拦截，普通事件用于处理）
- 使用 **set_handled** 控制事件传播
- **Preview 事件用于验证输入**
- **焦点事件更新 UI 状态**
- 避免混淆 Preview 和普通事件
- 避免忘记调用基类处理
- 避免混淆 Direct 和 Bubble 事件
- 避免忘记 set_handled 阻止传播
