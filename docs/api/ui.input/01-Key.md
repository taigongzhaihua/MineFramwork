# Key 详细接口文档

## 概述

`Key` 是 `mine.ui.input` 模块的**平台无关虚拟键枚举**。

**核心特性：**
- **平台无关**：统一的键盘键枚举，跨 Windows、Linux、macOS
- **Win32 对齐**：键值与 Win32 VK_ 常量对齐，便于直接转换
- **完整覆盖**：包含字母、数字、功能键、导航键、修饰键等所有常用按键
- **类型安全**：`enum class` 强类型枚举

**继承关系：**
- 独立枚举类型，无继承关系

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/Key.h
```

---

## 枚举定义

```cpp
enum class Key : uint32_t {
    None = 0,

    // 基本键
    Backspace  = 0x08,
    Tab        = 0x09,
    Enter      = 0x0D,
    Escape     = 0x1B,
    Space      = 0x20,

    // 导航键
    PageUp     = 0x21,
    PageDown   = 0x22,
    End        = 0x23,
    Home       = 0x24,
    Left       = 0x25,
    Up         = 0x26,
    Right      = 0x27,
    Down       = 0x28,
    Insert     = 0x2D,
    Delete     = 0x2E,
    PrintScreen = 0x2C,

    // 数字键 0-9（主键盘）
    D0 = 0x30, D1, D2, D3, D4, D5, D6, D7, D8, D9,

    // 字母键 A-Z
    A = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // 数字键盘
    NumPad0          = 0x60,
    NumPad1, NumPad2, NumPad3, NumPad4,
    NumPad5, NumPad6, NumPad7, NumPad8, NumPad9,
    NumPadMultiply   = 0x6A,
    NumPadAdd        = 0x6B,
    NumPadSeparator  = 0x6C,
    NumPadSubtract   = 0x6D,
    NumPadDecimal    = 0x6E,
    NumPadDivide     = 0x6F,

    // 功能键 F1-F12
    F1  = 0x70, F2,  F3,  F4,  F5,  F6,
    F7,  F8,  F9,  F10, F11, F12,

    // 锁定键
    CapsLock    = 0x14,
    NumLock     = 0x90,
    ScrollLock  = 0x91,

    // 系统键
    Pause       = 0x13,

    // 修饰键
    LeftShift   = 0xA0,
    RightShift  = 0xA1,
    LeftCtrl    = 0xA2,
    RightCtrl   = 0xA3,
    LeftAlt     = 0xA4,
    RightAlt    = 0xA5,
};
```

---

## 枚举值分类

### 基本键

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `None` | 0x00 | 无按键 |
| `Backspace` | 0x08 | 退格键 |
| `Tab` | 0x09 | Tab 键 |
| `Enter` | 0x0D | 回车键 |
| `Escape` | 0x1B | ESC 键 |
| `Space` | 0x20 | 空格键 |

---

### 导航键

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `PageUp` | 0x21 | PageUp 键 |
| `PageDown` | 0x22 | PageDown 键 |
| `End` | 0x23 | End 键 |
| `Home` | 0x24 | Home 键 |
| `Left` | 0x25 | 左方向键 |
| `Up` | 0x26 | 上方向键 |
| `Right` | 0x27 | 右方向键 |
| `Down` | 0x28 | 下方向键 |
| `Insert` | 0x2D | Insert 键 |
| `Delete` | 0x2E | Delete 键 |
| `PrintScreen` | 0x2C | 截屏键 |

---

### 数字键（主键盘）

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `D0` - `D9` | 0x30 - 0x39 | 数字键 0-9 |

**示例**：
- `Key::D0` = 0x30（数字键 0）
- `Key::D1` = 0x31（数字键 1）
- `Key::D9` = 0x39（数字键 9）

---

### 字母键

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `A` - `Z` | 0x41 - 0x5A | 字母键 A-Z |

**示例**：
- `Key::A` = 0x41（字母键 A）
- `Key::B` = 0x42（字母键 B）
- `Key::Z` = 0x5A（字母键 Z）

---

### 数字键盘

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `NumPad0` - `NumPad9` | 0x60 - 0x69 | 数字键盘 0-9 |
| `NumPadMultiply` | 0x6A | 数字键盘 * |
| `NumPadAdd` | 0x6B | 数字键盘 + |
| `NumPadSeparator` | 0x6C | 数字键盘分隔符 |
| `NumPadSubtract` | 0x6D | 数字键盘 - |
| `NumPadDecimal` | 0x6E | 数字键盘小数点 |
| `NumPadDivide` | 0x6F | 数字键盘 / |

---

### 功能键

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `F1` - `F12` | 0x70 - 0x7B | 功能键 F1-F12 |

**示例**：
- `Key::F1` = 0x70（功能键 F1）
- `Key::F12` = 0x7B（功能键 F12）

---

### 锁定键

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `CapsLock` | 0x14 | 大写锁定键 |
| `NumLock` | 0x90 | 数字锁定键 |
| `ScrollLock` | 0x91 | 滚动锁定键 |

---

### 系统键

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `Pause` | 0x13 | 暂停键 |

---

### 修饰键

| 枚举值 | 十六进制值 | 含义 |
|--------|-----------|------|
| `LeftShift` | 0xA0 | 左 Shift 键 |
| `RightShift` | 0xA1 | 右 Shift 键 |
| `LeftCtrl` | 0xA2 | 左 Ctrl 键 |
| `RightCtrl` | 0xA3 | 右 Ctrl 键 |
| `LeftAlt` | 0xA4 | 左 Alt 键 |
| `RightAlt` | 0xA5 | 右 Alt 键 |

---

## 工具函数

### key_from_win32_vk

```cpp
[[nodiscard]] inline Key key_from_win32_vk(uint32_t vk) noexcept;
```

**描述**：将 Win32 VK 虚拟键码转换为平台无关的 `Key` 枚举。

**参数**：
- `vk`：Win32 VK 虚拟键码（如 `VK_RETURN`、`VK_ESCAPE`）

**返回值**：
- 对应的 `Key` 枚举值

**特征**：
- 对于大多数按键，Win32 VK 值与 `Key` 枚举值直接对应，无需查表
- 直接进行类型转换（`static_cast<Key>(vk)`）

**示例**：
```cpp
#include <mine/ui/input/Key.h>
#include <Windows.h>

using namespace mine::ui::input;

// Win32 消息处理
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        // 将 Win32 VK 转换为 Key 枚举
        Key key = key_from_win32_vk(static_cast<uint32_t>(wParam));
        
        if (key == Key::Enter) {
            std::cout << "回车键按下" << std::endl;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
```

---

## 使用场景

### 1. 键盘事件处理

```cpp
#include <mine/ui/input/Key.h>
#include <mine/ui/input/KeyEventArgs.h>

using namespace mine::ui::input;

void on_key_down(KeyEventArgs& args) {
    switch (args.key()) {
        case Key::Enter:
            std::cout << "回车键按下" << std::endl;
            break;
        case Key::Escape:
            std::cout << "ESC 键按下" << std::endl;
            break;
        case Key::Space:
            std::cout << "空格键按下" << std::endl;
            break;
        default:
            break;
    }
}
```

---

### 2. 快捷键检测

```cpp
void on_key_down(KeyEventArgs& args) {
    // Ctrl+S 保存
    if (args.key() == Key::S && args.modifiers().has(ModifierKeys::Ctrl)) {
        std::cout << "Ctrl+S 保存" << std::endl;
        save_document();
    }
    
    // Ctrl+Z 撤销
    if (args.key() == Key::Z && args.modifiers().has(ModifierKeys::Ctrl)) {
        std::cout << "Ctrl+Z 撤销" << std::endl;
        undo();
    }
    
    // Ctrl+Shift+Z 重做
    if (args.key() == Key::Z && 
        args.modifiers().has(ModifierKeys::Ctrl | ModifierKeys::Shift)) {
        std::cout << "Ctrl+Shift+Z 重做" << std::endl;
        redo();
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
            break;
        case Key::Right:
            move_right();
            break;
        case Key::Up:
            move_up();
            break;
        case Key::Down:
            move_down();
            break;
        default:
            break;
    }
}
```

---

### 4. 功能键处理

```cpp
void on_key_down(KeyEventArgs& args) {
    switch (args.key()) {
        case Key::F1:
            show_help();
            break;
        case Key::F5:
            refresh();
            break;
        case Key::F11:
            toggle_fullscreen();
            break;
        default:
            break;
    }
}
```

---

### 5. 文本编辑快捷键

```cpp
void on_key_down(KeyEventArgs& args) {
    // Backspace 删除前一个字符
    if (args.key() == Key::Backspace) {
        delete_previous_char();
    }
    
    // Delete 删除后一个字符
    if (args.key() == Key::Delete) {
        delete_next_char();
    }
    
    // Home 移动到行首
    if (args.key() == Key::Home) {
        move_to_line_start();
    }
    
    // End 移动到行尾
    if (args.key() == Key::End) {
        move_to_line_end();
    }
}
```

---

### 6. 数字键盘输入

```cpp
void on_key_down(KeyEventArgs& args) {
    // 判断是否为数字键盘按键
    if (args.key() >= Key::NumPad0 && args.key() <= Key::NumPad9) {
        int digit = static_cast<int>(args.key()) - static_cast<int>(Key::NumPad0);
        std::cout << "数字键盘输入: " << digit << std::endl;
    }
    
    // 数字键盘运算符
    switch (args.key()) {
        case Key::NumPadAdd:
            std::cout << "数字键盘 +" << std::endl;
            break;
        case Key::NumPadSubtract:
            std::cout << "数字键盘 -" << std::endl;
            break;
        case Key::NumPadMultiply:
            std::cout << "数字键盘 *" << std::endl;
            break;
        case Key::NumPadDivide:
            std::cout << "数字键盘 /" << std::endl;
            break;
        default:
            break;
    }
}
```

---

### 7. 字母键输入

```cpp
void on_key_down(KeyEventArgs& args) {
    // 判断是否为字母键
    if (args.key() >= Key::A && args.key() <= Key::Z) {
        char letter = 'A' + (static_cast<int>(args.key()) - static_cast<int>(Key::A));
        std::cout << "字母键输入: " << letter << std::endl;
    }
}
```

---

## 最佳实践

### 1. 使用枚举值而非硬编码数字

```cpp
// ✅ 推荐：使用枚举值
if (args.key() == Key::Enter) {
    handle_enter();
}

// ❌ 不推荐：使用硬编码数字
if (static_cast<uint32_t>(args.key()) == 0x0D) {  // ❌ 难以理解
    handle_enter();
}
```

---

### 2. 使用 switch-case 处理多个按键

```cpp
// ✅ 推荐：使用 switch-case
switch (args.key()) {
    case Key::Enter:
        handle_enter();
        break;
    case Key::Escape:
        handle_escape();
        break;
    case Key::Space:
        handle_space();
        break;
    default:
        break;
}

// ❌ 不推荐：使用多个 if-else（代码冗长）
if (args.key() == Key::Enter) {
    handle_enter();
} else if (args.key() == Key::Escape) {
    handle_escape();
} else if (args.key() == Key::Space) {
    handle_space();
}
```

---

### 3. 区分主键盘和数字键盘

```cpp
// ✅ 推荐：区分主键盘数字键和数字键盘
if (args.key() >= Key::D0 && args.key() <= Key::D9) {
    std::cout << "主键盘数字键" << std::endl;
}
if (args.key() >= Key::NumPad0 && args.key() <= Key::NumPad9) {
    std::cout << "数字键盘数字键" << std::endl;
}

// ❌ 不推荐：混淆主键盘和数字键盘
// 用户可能期望不同行为
```

---

### 4. 区分左右修饰键

```cpp
// ✅ 推荐：区分左右修饰键（精确控制）
if (args.key() == Key::LeftShift) {
    std::cout << "左 Shift 键" << std::endl;
}

// ⚠️ 注意：大多数场景使用 ModifierKeys 即可
if (args.modifiers().has(ModifierKeys::Shift)) {
    std::cout << "Shift 键（左或右）" << std::endl;
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

### 2. 忘记区分主键盘和数字键盘

```cpp
// ❌ 问题：仅检查主键盘数字键
if (args.key() >= Key::D0 && args.key() <= Key::D9) {
    handle_digit();
}
// ❌ 数字键盘输入不会触发

// ✅ 解决：同时检查主键盘和数字键盘
if ((args.key() >= Key::D0 && args.key() <= Key::D9) ||
    (args.key() >= Key::NumPad0 && args.key() <= Key::NumPad9)) {
    handle_digit();
}
```

---

### 3. 硬编码键值

```cpp
// ❌ 问题：硬编码键值
if (static_cast<uint32_t>(args.key()) == 0x0D) {  // ❌ 难以理解
    handle_enter();
}

// ✅ 解决：使用枚举值
if (args.key() == Key::Enter) {
    handle_enter();
}
```

---

### 4. 混淆 Enter 和 NumPadEnter

```cpp
// ⚠️ 注意：Key 枚举中只有 Enter（主键盘回车键）
// 数字键盘回车键在某些系统中与 Enter 相同

// ✅ 推荐：统一处理 Enter
if (args.key() == Key::Enter) {
    handle_enter();
}
```

---

## 完整示例

```cpp
#include <mine/ui/input/Key.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/ModifierKeys.h>
#include <iostream>
#include <string>

using namespace mine::ui::input;

// ────────────────────────────────────────────────────────────────────────────
// 键盘事件处理器
// ────────────────────────────────────────────────────────────────────────────

class KeyboardHandler {
public:
    void on_key_down(KeyEventArgs& args) {
        Key key = args.key();
        ModifierKeys modifiers = args.modifiers();
        
        // ════════════════════════════════════════════════════════════════════
        // 1. 快捷键处理
        // ════════════════════════════════════════════════════════════════════
        
        // Ctrl+S 保存
        if (key == Key::S && modifiers.has(ModifierKeys::Ctrl)) {
            std::cout << "快捷键: Ctrl+S (保存)" << std::endl;
            save_document();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+Z 撤销
        if (key == Key::Z && modifiers.has(ModifierKeys::Ctrl)) {
            std::cout << "快捷键: Ctrl+Z (撤销)" << std::endl;
            undo();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+Shift+Z 重做
        if (key == Key::Z && 
            modifiers.has(ModifierKeys::Ctrl | ModifierKeys::Shift)) {
            std::cout << "快捷键: Ctrl+Shift+Z (重做)" << std::endl;
            redo();
            args.set_handled(true);
            return;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 2. 功能键处理
        // ════════════════════════════════════════════════════════════════════
        switch (key) {
            case Key::F1:
                std::cout << "功能键: F1 (帮助)" << std::endl;
                show_help();
                args.set_handled(true);
                return;
                
            case Key::F5:
                std::cout << "功能键: F5 (刷新)" << std::endl;
                refresh();
                args.set_handled(true);
                return;
                
            case Key::F11:
                std::cout << "功能键: F11 (全屏切换)" << std::endl;
                toggle_fullscreen();
                args.set_handled(true);
                return;
                
            default:
                break;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 3. 基本键处理
        // ════════════════════════════════════════════════════════════════════
        switch (key) {
            case Key::Enter:
                std::cout << "基本键: Enter (回车)" << std::endl;
                handle_enter();
                args.set_handled(true);
                break;
                
            case Key::Escape:
                std::cout << "基本键: Escape (取消)" << std::endl;
                handle_escape();
                args.set_handled(true);
                break;
                
            case Key::Backspace:
                std::cout << "基本键: Backspace (删除前一个字符)" << std::endl;
                delete_previous_char();
                args.set_handled(true);
                break;
                
            case Key::Delete:
                std::cout << "基本键: Delete (删除后一个字符)" << std::endl;
                delete_next_char();
                args.set_handled(true);
                break;
                
            case Key::Tab:
                std::cout << "基本键: Tab (制表符)" << std::endl;
                handle_tab();
                args.set_handled(true);
                break;
                
            default:
                break;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 4. 方向键导航
        // ════════════════════════════════════════════════════════════════════
        switch (key) {
            case Key::Left:
                std::cout << "导航键: Left (左)" << std::endl;
                move_left();
                args.set_handled(true);
                break;
                
            case Key::Right:
                std::cout << "导航键: Right (右)" << std::endl;
                move_right();
                args.set_handled(true);
                break;
                
            case Key::Up:
                std::cout << "导航键: Up (上)" << std::endl;
                move_up();
                args.set_handled(true);
                break;
                
            case Key::Down:
                std::cout << "导航键: Down (下)" << std::endl;
                move_down();
                args.set_handled(true);
                break;
                
            case Key::Home:
                std::cout << "导航键: Home (行首)" << std::endl;
                move_to_line_start();
                args.set_handled(true);
                break;
                
            case Key::End:
                std::cout << "导航键: End (行尾)" << std::endl;
                move_to_line_end();
                args.set_handled(true);
                break;
                
            default:
                break;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 5. 字母键处理
        // ════════════════════════════════════════════════════════════════════
        if (key >= Key::A && key <= Key::Z) {
            char letter = 'A' + (static_cast<int>(key) - static_cast<int>(Key::A));
            std::cout << "字母键: " << letter << std::endl;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 6. 数字键处理
        // ════════════════════════════════════════════════════════════════════
        if (key >= Key::D0 && key <= Key::D9) {
            int digit = static_cast<int>(key) - static_cast<int>(Key::D0);
            std::cout << "主键盘数字键: " << digit << std::endl;
        }
        
        if (key >= Key::NumPad0 && key <= Key::NumPad9) {
            int digit = static_cast<int>(key) - static_cast<int>(Key::NumPad0);
            std::cout << "数字键盘数字键: " << digit << std::endl;
        }
    }

private:
    void save_document() { std::cout << "  执行: 保存文档" << std::endl; }
    void undo() { std::cout << "  执行: 撤销" << std::endl; }
    void redo() { std::cout << "  执行: 重做" << std::endl; }
    void show_help() { std::cout << "  执行: 显示帮助" << std::endl; }
    void refresh() { std::cout << "  执行: 刷新" << std::endl; }
    void toggle_fullscreen() { std::cout << "  执行: 全屏切换" << std::endl; }
    void handle_enter() { std::cout << "  执行: 处理回车" << std::endl; }
    void handle_escape() { std::cout << "  执行: 处理取消" << std::endl; }
    void delete_previous_char() { std::cout << "  执行: 删除前一个字符" << std::endl; }
    void delete_next_char() { std::cout << "  执行: 删除后一个字符" << std::endl; }
    void handle_tab() { std::cout << "  执行: 处理制表符" << std::endl; }
    void move_left() { std::cout << "  执行: 左移" << std::endl; }
    void move_right() { std::cout << "  执行: 右移" << std::endl; }
    void move_up() { std::cout << "  执行: 上移" << std::endl; }
    void move_down() { std::cout << "  执行: 下移" << std::endl; }
    void move_to_line_start() { std::cout << "  执行: 移动到行首" << std::endl; }
    void move_to_line_end() { std::cout << "  执行: 移动到行尾" << std::endl; }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    KeyboardHandler handler;
    
    // 模拟键盘事件
    KeyEventArgs args1(Key::Enter, ModifierKeys::None);
    handler.on_key_down(args1);
    // 输出：基本键: Enter (回车)
    //       执行: 处理回车
    
    KeyEventArgs args2(Key::S, ModifierKeys::Ctrl);
    handler.on_key_down(args2);
    // 输出：快捷键: Ctrl+S (保存)
    //       执行: 保存文档
    
    KeyEventArgs args3(Key::F1, ModifierKeys::None);
    handler.on_key_down(args3);
    // 输出：功能键: F1 (帮助)
    //       执行: 显示帮助
    
    KeyEventArgs args4(Key::Left, ModifierKeys::None);
    handler.on_key_down(args4);
    // 输出：导航键: Left (左)
    //       执行: 左移
    
    return 0;
}
```

---

## 总结

`Key` 是 `mine.ui.input` 模块的平台无关虚拟键枚举，具备：

1. **平台无关**：统一的键盘键枚举，跨 Windows、Linux、macOS
2. **Win32 对齐**：键值与 Win32 VK_ 常量对齐，便于直接转换
3. **完整覆盖**：包含字母、数字、功能键、导航键、修饰键等所有常用按键
4. **类型安全**：`enum class` 强类型枚举
5. **工具函数**：`key_from_win32_vk` 用于从 Win32 VK 码转换

**使用建议**：
- 使用枚举值而非硬编码数字
- 使用 **switch-case** 处理多个按键
- 区分主键盘和数字键盘
- 区分左右修饰键（需要时）
- 避免混淆 Key 枚举和字符
- 同时检查主键盘和数字键盘数字键
