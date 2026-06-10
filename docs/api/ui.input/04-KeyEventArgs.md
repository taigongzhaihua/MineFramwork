# KeyEventArgs 详细接口文档

## 概述

`KeyEventArgs` 是 `mine.ui.input` 模块的**键盘路由事件参数类**。

**核心特性：**
- **路由事件参数**：继承自 `RoutedEventArgs`，支持事件冒泡和隧道
- **完整键盘信息**：包含虚拟键、硬件扫描码、修饰键状态、自动重复标志
- **快捷访问**：提供 `ctrl()`、`shift()`、`alt()` 快捷方法
- **栈分配**：在调用栈上分配，不可拷贝

**继承关系：**
```
RoutedEventArgs
    └── KeyEventArgs
```

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/KeyEventArgs.h
```

---

## 类定义

```cpp
class MINE_UI_INPUT_API KeyEventArgs : public RoutedEventArgs {
public:
    explicit KeyEventArgs(const RoutedEvent& event,
                          Key                key,
                          uint32_t           scan_code,
                          ModifierKeys       modifiers,
                          bool               is_repeat) noexcept;

    [[nodiscard]] Key key() const noexcept;
    [[nodiscard]] uint32_t scan_code() const noexcept;
    [[nodiscard]] ModifierKeys modifiers() const noexcept;
    [[nodiscard]] bool is_repeat() const noexcept;

    [[nodiscard]] bool ctrl() const noexcept;
    [[nodiscard]] bool shift() const noexcept;
    [[nodiscard]] bool alt() const noexcept;

private:
    Key          key_;
    uint32_t     scan_code_;
    ModifierKeys modifiers_;
    bool         is_repeat_;
};
```

---

## 构造函数

### KeyEventArgs

```cpp
explicit KeyEventArgs(const RoutedEvent& event,
                      Key                key,
                      uint32_t           scan_code,
                      ModifierKeys       modifiers,
                      bool               is_repeat) noexcept;
```

**描述**：构造键盘事件参数。

**参数**：
- `event`：路由事件描述符（如 `KeyDownEvent()`、`KeyUpEvent()`）
- `key`：平台无关虚拟键枚举（`Key`）
- `scan_code`：硬件扫描码
- `modifiers`：修饰键状态（`ModifierKeys` 位域）
- `is_repeat`：是否为键盘自动重复（按住不放产生的重复事件）

**特征**：
- `noexcept`：保证不抛出异常
- `explicit`：禁止隐式转换

**示例**：
```cpp
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/InputEvents.h>

using namespace mine::ui::input;

// 构造 KeyDown 事件参数
KeyEventArgs args(
    KeyDownEvent(),           // 事件描述符
    Key::A,                   // 虚拟键 A
    0x1E,                     // 硬件扫描码
    ModifierKeys::Ctrl,       // Ctrl 键按下
    false                     // 非自动重复
);

// 触发事件
EventManager::raise(*element, args);
```

---

## 成员方法

### key

```cpp
[[nodiscard]] Key key() const noexcept;
```

**描述**：获取触发事件的虚拟键。

**返回值**：
- `Key`：平台无关虚拟键枚举

**示例**：
```cpp
void on_key_down(KeyEventArgs& args) {
    Key k = args.key();
    if (k == Key::Enter) {
        std::cout << "回车键按下" << std::endl;
    }
}
```

---

### scan_code

```cpp
[[nodiscard]] uint32_t scan_code() const noexcept;
```

**描述**：获取硬件扫描码。

**返回值**：
- `uint32_t`：硬件扫描码（平台相关）

**说明**：
- 硬件扫描码是键盘硬件直接生成的原始码，平台相关
- 通常用于底层输入处理或键盘布局识别
- 大多数场景使用 `key()` 虚拟键即可

**示例**：
```cpp
void on_key_down(KeyEventArgs& args) {
    uint32_t scan = args.scan_code();
    std::cout << "硬件扫描码: 0x" << std::hex << scan << std::endl;
}
```

---

### modifiers

```cpp
[[nodiscard]] ModifierKeys modifiers() const noexcept;
```

**描述**：获取修饰键状态（Ctrl / Shift / Alt 位域）。

**返回值**：
- `ModifierKeys`：修饰键状态位域

**示例**：
```cpp
void on_key_down(KeyEventArgs& args) {
    ModifierKeys mods = args.modifiers();
    
    // 检查 Ctrl
    if (has_flag(mods, ModifierKeys::Ctrl)) {
        std::cout << "Ctrl 键按下" << std::endl;
    }
    
    // 精确匹配 Ctrl+Shift
    if (mods == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
        std::cout << "Ctrl+Shift 组合" << std::endl;
    }
}
```

---

### is_repeat

```cpp
[[nodiscard]] bool is_repeat() const noexcept;
```

**描述**：获取是否为键盘自动重复（按住不放产生的重复事件）。

**返回值**：
- `true`：自动重复事件（按住不放）
- `false`：首次按下

**示例**：
```cpp
void on_key_down(KeyEventArgs& args) {
    if (args.is_repeat()) {
        std::cout << "自动重复事件（按住不放）" << std::endl;
    } else {
        std::cout << "首次按下" << std::endl;
    }
}
```

---

### ctrl

```cpp
[[nodiscard]] bool ctrl() const noexcept;
```

**描述**：快捷访问：Ctrl 是否按下。

**返回值**：
- `true`：Ctrl 键按下
- `false`：Ctrl 键未按下

**等价于**：`has_flag(modifiers(), ModifierKeys::Ctrl)`

**示例**：
```cpp
void on_key_down(KeyEventArgs& args) {
    if (args.ctrl() && args.key() == Key::S) {
        std::cout << "Ctrl+S 保存" << std::endl;
        save_document();
    }
}
```

---

### shift

```cpp
[[nodiscard]] bool shift() const noexcept;
```

**描述**：快捷访问：Shift 是否按下。

**返回值**：
- `true`：Shift 键按下
- `false`：Shift 键未按下

**等价于**：`has_flag(modifiers(), ModifierKeys::Shift)`

**示例**：
```cpp
void on_key_down(KeyEventArgs& args) {
    if (args.shift() && args.key() == Key::Tab) {
        std::cout << "Shift+Tab 反向切换" << std::endl;
        focus_previous();
    }
}
```

---

### alt

```cpp
[[nodiscard]] bool alt() const noexcept;
```

**描述**：快捷访问：Alt 是否按下。

**返回值**：
- `true`：Alt 键按下
- `false`：Alt 键未按下

**等价于**：`has_flag(modifiers(), ModifierKeys::Alt)`

**示例**：
```cpp
void on_key_down(KeyEventArgs& args) {
    if (args.alt() && args.key() == Key::F4) {
        std::cout << "Alt+F4 关闭窗口" << std::endl;
        close_window();
    }
}
```

---

## 使用场景

### 1. 键盘事件处理

```cpp
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/InputEvents.h>

using namespace mine::ui::input;

class MyControl : public Visual {
protected:
    void on_key_down(KeyEventArgs& args) override {
        switch (args.key()) {
            case Key::Enter:
                std::cout << "回车键按下" << std::endl;
                handle_enter();
                args.set_handled(true);
                break;
                
            case Key::Escape:
                std::cout << "ESC 键按下" << std::endl;
                handle_escape();
                args.set_handled(true);
                break;
                
            default:
                Visual::on_key_down(args);  // 调用基类处理
                break;
        }
    }
};
```

---

### 2. 快捷键检测

```cpp
void on_key_down(KeyEventArgs& args) {
    // Ctrl+S 保存
    if (args.ctrl() && args.key() == Key::S) {
        std::cout << "Ctrl+S 保存" << std::endl;
        save_document();
        args.set_handled(true);
        return;
    }
    
    // Ctrl+Z 撤销
    if (args.ctrl() && args.key() == Key::Z) {
        std::cout << "Ctrl+Z 撤销" << std::endl;
        undo();
        args.set_handled(true);
        return;
    }
    
    // Ctrl+Shift+Z 重做
    if (args.ctrl() && args.shift() && args.key() == Key::Z) {
        std::cout << "Ctrl+Shift+Z 重做" << std::endl;
        redo();
        args.set_handled(true);
        return;
    }
}
```

---

### 3. 方向键导航

```cpp
void on_key_down(KeyEventArgs& args) {
    switch (args.key()) {
        case Key::Left:
            move_left();
            args.set_handled(true);
            break;
            
        case Key::Right:
            move_right();
            args.set_handled(true);
            break;
            
        case Key::Up:
            move_up();
            args.set_handled(true);
            break;
            
        case Key::Down:
            move_down();
            args.set_handled(true);
            break;
            
        default:
            break;
    }
}
```

---

### 4. 自动重复过滤

```cpp
void on_key_down(KeyEventArgs& args) {
    // 仅处理首次按下，忽略自动重复
    if (args.is_repeat()) {
        return;  // 忽略自动重复
    }
    
    // 处理首次按下
    std::cout << "首次按下: " << static_cast<int>(args.key()) << std::endl;
    handle_key_press(args.key());
}
```

---

### 5. 文本编辑快捷键

```cpp
void on_key_down(KeyEventArgs& args) {
    // Ctrl+X 剪切
    if (args.ctrl() && args.key() == Key::X) {
        cut();
        args.set_handled(true);
        return;
    }
    
    // Ctrl+C 复制
    if (args.ctrl() && args.key() == Key::C) {
        copy();
        args.set_handled(true);
        return;
    }
    
    // Ctrl+V 粘贴
    if (args.ctrl() && args.key() == Key::V) {
        paste();
        args.set_handled(true);
        return;
    }
    
    // Ctrl+A 全选
    if (args.ctrl() && args.key() == Key::A) {
        select_all();
        args.set_handled(true);
        return;
    }
}
```

---

### 6. 功能键处理

```cpp
void on_key_down(KeyEventArgs& args) {
    switch (args.key()) {
        case Key::F1:
            show_help();
            args.set_handled(true);
            break;
            
        case Key::F5:
            refresh();
            args.set_handled(true);
            break;
            
        case Key::F11:
            toggle_fullscreen();
            args.set_handled(true);
            break;
            
        default:
            break;
    }
}
```

---

### 7. KeyUp 事件处理

```cpp
void on_key_up(KeyEventArgs& args) {
    // 释放按键时的处理
    if (args.key() == Key::Space) {
        std::cout << "空格键释放" << std::endl;
        stop_action();
        args.set_handled(true);
    }
}
```

---

## 最佳实践

### 1. 使用快捷方法检查修饰键

```cpp
// ✅ 推荐：使用快捷方法
if (args.ctrl() && args.key() == Key::S) {
    save_document();
}

// ⚠️ 可用但冗长：使用 modifiers()
if (has_flag(args.modifiers(), ModifierKeys::Ctrl) && args.key() == Key::S) {
    save_document();
}
```

---

### 2. 先检查组合修饰键，再检查单个修饰键

```cpp
// ✅ 推荐：先检查组合修饰键
void on_key_down(KeyEventArgs& args) {
    // 先检查 Ctrl+Shift+Z
    if (args.ctrl() && args.shift() && args.key() == Key::Z) {
        redo();
        args.set_handled(true);
        return;
    }
    
    // 再检查 Ctrl+Z
    if (args.ctrl() && args.key() == Key::Z) {
        undo();
        args.set_handled(true);
        return;
    }
}

// ❌ 不推荐：先检查单个修饰键（可能误触发）
void on_key_down(KeyEventArgs& args) {
    // ❌ Ctrl+Shift+Z 会被误识别为 Ctrl+Z
    if (args.ctrl() && args.key() == Key::Z) {
        undo();  // ❌ 错误触发
        return;
    }
}
```

---

### 3. 使用 set_handled 阻止事件冒泡

```cpp
// ✅ 推荐：处理完事件后调用 set_handled
void on_key_down(KeyEventArgs& args) {
    if (args.key() == Key::Enter) {
        handle_enter();
        args.set_handled(true);  // 阻止事件继续冒泡
        return;
    }
}

// ❌ 不推荐：忘记调用 set_handled（事件会继续冒泡）
void on_key_down(KeyEventArgs& args) {
    if (args.key() == Key::Enter) {
        handle_enter();
        // ❌ 忘记调用 set_handled，事件会继续冒泡
    }
}
```

---

### 4. 过滤自动重复事件

```cpp
// ✅ 推荐：根据需求过滤自动重复
void on_key_down(KeyEventArgs& args) {
    // 场景1：仅处理首次按下
    if (args.is_repeat()) {
        return;  // 忽略自动重复
    }
    handle_key_press(args.key());
    
    // 场景2：支持自动重复（如文本编辑）
    // 不检查 is_repeat()，允许自动重复触发
}
```

---

## 常见陷阱

### 1. 混淆 Key 枚举和字符

```cpp
// ❌ 问题：混淆 Key 枚举和字符
if (args.key() == 'A') {  // ❌ 类型不匹配
    handle_a();
}

// ✅ 解决：使用 Key 枚举
if (args.key() == Key::A) {
    handle_a();
}
```

---

### 2. 忘记检查修饰键

```cpp
// ❌ 问题：忘记检查修饰键
if (args.key() == Key::S) {
    save_document();  // ❌ 单独按 S 键也会保存
}

// ✅ 解决：检查修饰键
if (args.ctrl() && args.key() == Key::S) {
    save_document();
}
```

---

### 3. 快捷键处理顺序错误

```cpp
// ❌ 问题：先检查单个修饰键
if (args.ctrl() && args.key() == Key::Z) {
    undo();  // ❌ Ctrl+Shift+Z 会被误识别为撤销
    args.set_handled(true);
    return;
}
if (args.ctrl() && args.shift() && args.key() == Key::Z) {
    redo();  // ❌ 永远不会执行
}

// ✅ 解决：先检查组合修饰键
if (args.ctrl() && args.shift() && args.key() == Key::Z) {
    redo();  // 先检查组合
    args.set_handled(true);
    return;
}
if (args.ctrl() && args.key() == Key::Z) {
    undo();  // 再检查单个
}
```

---

### 4. 忘记调用 set_handled

```cpp
// ❌ 问题：忘记调用 set_handled
void on_key_down(KeyEventArgs& args) {
    if (args.key() == Key::Enter) {
        handle_enter();
        // ❌ 忘记调用 set_handled，事件会继续冒泡
    }
}

// ✅ 解决：调用 set_handled
void on_key_down(KeyEventArgs& args) {
    if (args.key() == Key::Enter) {
        handle_enter();
        args.set_handled(true);  // 阻止事件冒泡
    }
}
```

---

## 完整示例

```cpp
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/visual/Visual.h>
#include <iostream>

using namespace mine::ui::input;
using namespace mine::ui::visual;

// ────────────────────────────────────────────────────────────────────────────
// 文本编辑器控件
// ────────────────────────────────────────────────────────────────────────────

class TextEditor : public Visual {
protected:
    // ════════════════════════════════════════════════════════════════════════
    // KeyDown 事件处理
    // ════════════════════════════════════════════════════════════════════════
    void on_key_down(KeyEventArgs& args) override {
        Key key = args.key();
        
        // ════════════════════════════════════════════════════════════════════
        // 1. 过滤自动重复（可选）
        // ════════════════════════════════════════════════════════════════════
        if (args.is_repeat()) {
            // 某些操作不希望自动重复（如保存）
            if (args.ctrl() && key == Key::S) {
                return;  // 忽略自动重复
            }
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 2. 组合修饰键快捷键（先检查）
        // ════════════════════════════════════════════════════════════════════
        
        // Ctrl+Shift+Z 重做
        if (args.ctrl() && args.shift() && key == Key::Z) {
            std::cout << "Ctrl+Shift+Z 重做" << std::endl;
            redo();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+Shift+N 新窗口
        if (args.ctrl() && args.shift() && key == Key::N) {
            std::cout << "Ctrl+Shift+N 新窗口" << std::endl;
            open_new_window();
            args.set_handled(true);
            return;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 3. 单个修饰键快捷键
        // ════════════════════════════════════════════════════════════════════
        
        // Ctrl+S 保存
        if (args.ctrl() && key == Key::S) {
            std::cout << "Ctrl+S 保存" << std::endl;
            save_document();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+Z 撤销
        if (args.ctrl() && key == Key::Z) {
            std::cout << "Ctrl+Z 撤销" << std::endl;
            undo();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+X 剪切
        if (args.ctrl() && key == Key::X) {
            std::cout << "Ctrl+X 剪切" << std::endl;
            cut();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+C 复制
        if (args.ctrl() && key == Key::C) {
            std::cout << "Ctrl+C 复制" << std::endl;
            copy();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+V 粘贴
        if (args.ctrl() && key == Key::V) {
            std::cout << "Ctrl+V 粘贴" << std::endl;
            paste();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+A 全选
        if (args.ctrl() && key == Key::A) {
            std::cout << "Ctrl+A 全选" << std::endl;
            select_all();
            args.set_handled(true);
            return;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 4. 功能键
        // ════════════════════════════════════════════════════════════════════
        switch (key) {
            case Key::F1:
                std::cout << "F1 帮助" << std::endl;
                show_help();
                args.set_handled(true);
                return;
                
            case Key::F5:
                std::cout << "F5 刷新" << std::endl;
                refresh();
                args.set_handled(true);
                return;
                
            default:
                break;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 5. 基本键
        // ════════════════════════════════════════════════════════════════════
        switch (key) {
            case Key::Enter:
                std::cout << "Enter 回车" << std::endl;
                insert_newline();
                args.set_handled(true);
                return;
                
            case Key::Backspace:
                std::cout << "Backspace 删除前一个字符" << std::endl;
                delete_previous_char();
                args.set_handled(true);
                return;
                
            case Key::Delete:
                std::cout << "Delete 删除后一个字符" << std::endl;
                delete_next_char();
                args.set_handled(true);
                return;
                
            case Key::Tab:
                std::cout << "Tab 制表符" << std::endl;
                insert_tab();
                args.set_handled(true);
                return;
                
            default:
                break;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 6. 方向键导航
        // ════════════════════════════════════════════════════════════════════
        switch (key) {
            case Key::Left:
                move_left();
                args.set_handled(true);
                return;
                
            case Key::Right:
                move_right();
                args.set_handled(true);
                return;
                
            case Key::Up:
                move_up();
                args.set_handled(true);
                return;
                
            case Key::Down:
                move_down();
                args.set_handled(true);
                return;
                
            case Key::Home:
                move_to_line_start();
                args.set_handled(true);
                return;
                
            case Key::End:
                move_to_line_end();
                args.set_handled(true);
                return;
                
            default:
                break;
        }
        
        // 未处理的按键，调用基类处理
        Visual::on_key_down(args);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // KeyUp 事件处理
    // ════════════════════════════════════════════════════════════════════════
    void on_key_up(KeyEventArgs& args) override {
        std::cout << "KeyUp: " << static_cast<int>(args.key()) << std::endl;
        Visual::on_key_up(args);
    }

private:
    void save_document() { std::cout << "  执行: 保存文档" << std::endl; }
    void undo() { std::cout << "  执行: 撤销" << std::endl; }
    void redo() { std::cout << "  执行: 重做" << std::endl; }
    void cut() { std::cout << "  执行: 剪切" << std::endl; }
    void copy() { std::cout << "  执行: 复制" << std::endl; }
    void paste() { std::cout << "  执行: 粘贴" << std::endl; }
    void select_all() { std::cout << "  执行: 全选" << std::endl; }
    void show_help() { std::cout << "  执行: 显示帮助" << std::endl; }
    void refresh() { std::cout << "  执行: 刷新" << std::endl; }
    void insert_newline() { std::cout << "  执行: 插入换行" << std::endl; }
    void delete_previous_char() { std::cout << "  执行: 删除前一个字符" << std::endl; }
    void delete_next_char() { std::cout << "  执行: 删除后一个字符" << std::endl; }
    void insert_tab() { std::cout << "  执行: 插入制表符" << std::endl; }
    void move_left() { std::cout << "  执行: 左移" << std::endl; }
    void move_right() { std::cout << "  执行: 右移" << std::endl; }
    void move_up() { std::cout << "  执行: 上移" << std::endl; }
    void move_down() { std::cout << "  执行: 下移" << std::endl; }
    void move_to_line_start() { std::cout << "  执行: 移动到行首" << std::endl; }
    void move_to_line_end() { std::cout << "  执行: 移动到行尾" << std::endl; }
    void open_new_window() { std::cout << "  执行: 打开新窗口" << std::endl; }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    TextEditor editor;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 单个修饰键快捷键
    // ════════════════════════════════════════════════════════════════════════
    
    KeyEventArgs args1(
        KeyDownEvent(),
        Key::S,
        0,
        ModifierKeys::Ctrl,
        false
    );
    editor.on_key_down(args1);
    // 输出：Ctrl+S 保存
    //       执行: 保存文档
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 组合修饰键快捷键
    // ════════════════════════════════════════════════════════════════════════
    
    KeyEventArgs args2(
        KeyDownEvent(),
        Key::Z,
        0,
        ModifierKeys::Ctrl | ModifierKeys::Shift,
        false
    );
    editor.on_key_down(args2);
    // 输出：Ctrl+Shift+Z 重做
    //       执行: 重做
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 自动重复事件
    // ════════════════════════════════════════════════════════════════════════
    
    KeyEventArgs args3(
        KeyDownEvent(),
        Key::A,
        0,
        ModifierKeys::None,
        true  // 自动重复
    );
    editor.on_key_down(args3);
    // 输出：（根据 is_repeat() 处理）
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 方向键导航
    // ════════════════════════════════════════════════════════════════════════
    
    KeyEventArgs args4(
        KeyDownEvent(),
        Key::Left,
        0,
        ModifierKeys::None,
        false
    );
    editor.on_key_down(args4);
    // 输出：执行: 左移
    
    return 0;
}
```

---

## 总结

`KeyEventArgs` 是 `mine.ui.input` 模块的键盘路由事件参数类，具备：

1. **路由事件参数**：继承自 `RoutedEventArgs`，支持事件冒泡和隧道
2. **完整键盘信息**：包含虚拟键、硬件扫描码、修饰键状态、自动重复标志
3. **快捷访问**：提供 `ctrl()`、`shift()`、`alt()` 快捷方法
4. **栈分配**：在调用栈上分配，不可拷贝

**使用建议**：
- 使用 **快捷方法**（`ctrl()`、`shift()`、`alt()`）检查修饰键
- **先检查组合修饰键，再检查单个修饰键**
- 使用 **set_handled** 阻止事件冒泡
- 根据需求**过滤自动重复事件**
- 避免混淆 Key 枚举和字符
- 避免忘记检查修饰键
- 避免快捷键处理顺序错误
- 避免忘记调用 set_handled
