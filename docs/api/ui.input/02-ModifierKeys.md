# ModifierKeys 详细接口文档

## 概述

`ModifierKeys` 是 `mine.ui.input` 模块的**修饰键位域枚举**。

**核心特性：**
- **位域枚举**：支持位运算符（`|`、`&`、`~`）组合多个修饰键
- **类型安全**：`enum class` 强类型枚举
- **高效存储**：`uint8_t` 类型，仅占 1 字节
- **快捷键检测**：用于检测 Ctrl+S、Ctrl+Shift+Z 等快捷键组合

**继承关系：**
- 独立枚举类型，无继承关系

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/ModifierKeys.h
```

---

## 枚举定义

```cpp
enum class ModifierKeys : uint8_t {
    None  = 0,
    Ctrl  = 1u << 0,  // 0x01
    Shift = 1u << 1,  // 0x02
    Alt   = 1u << 2,  // 0x04
};
```

---

## 枚举值

| 枚举值 | 位值 | 十六进制 | 含义 |
|--------|------|---------|------|
| `None` | 0 | 0x00 | 无修饰键 |
| `Ctrl` | 1 << 0 | 0x01 | Ctrl 键 |
| `Shift` | 1 << 1 | 0x02 | Shift 键 |
| `Alt` | 1 << 2 | 0x04 | Alt 键 |

---

## 位运算符

### operator| (位或)

```cpp
[[nodiscard]] inline ModifierKeys operator|(ModifierKeys a, ModifierKeys b) noexcept;
```

**描述**：组合两个修饰键。

**参数**：
- `a`：第一个修饰键
- `b`：第二个修饰键

**返回值**：
- 组合后的修饰键

**示例**：
```cpp
// Ctrl + Shift
ModifierKeys combined = ModifierKeys::Ctrl | ModifierKeys::Shift;

// Ctrl + Shift + Alt
ModifierKeys all = ModifierKeys::Ctrl | ModifierKeys::Shift | ModifierKeys::Alt;
```

---

### operator& (位与)

```cpp
[[nodiscard]] inline ModifierKeys operator&(ModifierKeys a, ModifierKeys b) noexcept;
```

**描述**：检查修饰键交集。

**参数**：
- `a`：第一个修饰键
- `b`：第二个修饰键

**返回值**：
- 交集修饰键

**示例**：
```cpp
ModifierKeys keys = ModifierKeys::Ctrl | ModifierKeys::Shift;

// 检查是否包含 Ctrl
if ((keys & ModifierKeys::Ctrl) != ModifierKeys::None) {
    std::cout << "包含 Ctrl" << std::endl;
}

// 检查是否包含 Alt
if ((keys & ModifierKeys::Alt) != ModifierKeys::None) {
    std::cout << "包含 Alt" << std::endl;  // 不会输出
}
```

---

### operator~ (位非)

```cpp
[[nodiscard]] inline ModifierKeys operator~(ModifierKeys a) noexcept;
```

**描述**：取反修饰键（较少使用）。

**参数**：
- `a`：修饰键

**返回值**：
- 取反后的修饰键

---

### operator|= (位或赋值)

```cpp
inline ModifierKeys& operator|=(ModifierKeys& a, ModifierKeys b) noexcept;
```

**描述**：追加修饰键。

**参数**：
- `a`：目标修饰键（引用）
- `b`：要追加的修饰键

**返回值**：
- 更新后的目标修饰键引用

**示例**：
```cpp
ModifierKeys keys = ModifierKeys::Ctrl;

// 追加 Shift
keys |= ModifierKeys::Shift;

// keys 现在为 Ctrl | Shift
```

---

### operator&= (位与赋值)

```cpp
inline ModifierKeys& operator&=(ModifierKeys& a, ModifierKeys b) noexcept;
```

**描述**：移除不在 `b` 中的修饰键（较少使用）。

**参数**：
- `a`：目标修饰键（引用）
- `b`：要保留的修饰键

**返回值**：
- 更新后的目标修饰键引用

---

## 工具函数

### has_flag

```cpp
[[nodiscard]] inline bool has_flag(ModifierKeys flags, ModifierKeys flag) noexcept;
```

**描述**：检查 `flags` 中是否包含指定标志位 `flag`。

**参数**：
- `flags`：修饰键集合
- `flag`：要检查的修饰键

**返回值**：
- `true`：包含指定修饰键
- `false`：不包含指定修饰键

**示例**：
```cpp
ModifierKeys keys = ModifierKeys::Ctrl | ModifierKeys::Shift;

// 检查是否包含 Ctrl
if (has_flag(keys, ModifierKeys::Ctrl)) {
    std::cout << "包含 Ctrl" << std::endl;
}

// 检查是否包含 Alt
if (has_flag(keys, ModifierKeys::Alt)) {
    std::cout << "包含 Alt" << std::endl;  // 不会输出
}
```

---

## 使用场景

### 1. 快捷键检测（单个修饰键）

```cpp
#include <mine/ui/input/ModifierKeys.h>
#include <mine/ui/input/KeyEventArgs.h>

using namespace mine::ui::input;

void on_key_down(KeyEventArgs& args) {
    // Ctrl+S 保存
    if (args.key() == Key::S && has_flag(args.modifiers(), ModifierKeys::Ctrl)) {
        std::cout << "Ctrl+S 保存" << std::endl;
        save_document();
    }
    
    // Shift+Delete 剪切
    if (args.key() == Key::Delete && has_flag(args.modifiers(), ModifierKeys::Shift)) {
        std::cout << "Shift+Delete 剪切" << std::endl;
        cut();
    }
    
    // Alt+F4 关闭窗口
    if (args.key() == Key::F4 && has_flag(args.modifiers(), ModifierKeys::Alt)) {
        std::cout << "Alt+F4 关闭窗口" << std::endl;
        close_window();
    }
}
```

---

### 2. 快捷键检测（组合修饰键）

```cpp
void on_key_down(KeyEventArgs& args) {
    ModifierKeys required = ModifierKeys::Ctrl | ModifierKeys::Shift;
    
    // Ctrl+Shift+Z 重做
    if (args.key() == Key::Z && args.modifiers() == required) {
        std::cout << "Ctrl+Shift+Z 重做" << std::endl;
        redo();
    }
    
    // Ctrl+Shift+N 新窗口
    if (args.key() == Key::N && args.modifiers() == required) {
        std::cout << "Ctrl+Shift+N 新窗口" << std::endl;
        open_new_window();
    }
}
```

---

### 3. 检查是否包含某个修饰键（无论其他修饰键）

```cpp
void on_key_down(KeyEventArgs& args) {
    // 只要包含 Ctrl 键（无论是否有 Shift/Alt）
    if (has_flag(args.modifiers(), ModifierKeys::Ctrl)) {
        std::cout << "包含 Ctrl 键" << std::endl;
    }
    
    // 只要包含 Shift 键
    if (has_flag(args.modifiers(), ModifierKeys::Shift)) {
        std::cout << "包含 Shift 键" << std::endl;
    }
}
```

---

### 4. 组合修饰键

```cpp
// 组合 Ctrl + Shift
ModifierKeys combined = ModifierKeys::Ctrl | ModifierKeys::Shift;

// 组合 Ctrl + Alt
ModifierKeys ctrl_alt = ModifierKeys::Ctrl | ModifierKeys::Alt;

// 组合 Ctrl + Shift + Alt
ModifierKeys all = ModifierKeys::Ctrl | ModifierKeys::Shift | ModifierKeys::Alt;
```

---

### 5. 追加修饰键

```cpp
ModifierKeys keys = ModifierKeys::Ctrl;

// 追加 Shift
keys |= ModifierKeys::Shift;

// 追加 Alt
keys |= ModifierKeys::Alt;

// keys 现在为 Ctrl | Shift | Alt
```

---

### 6. 检查修饰键组合

```cpp
void on_key_down(KeyEventArgs& args) {
    ModifierKeys mods = args.modifiers();
    
    // 精确匹配 Ctrl + Shift（不能有 Alt）
    if (mods == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
        std::cout << "精确匹配 Ctrl+Shift" << std::endl;
    }
    
    // 包含 Ctrl 和 Shift（可能还有 Alt）
    if (has_flag(mods, ModifierKeys::Ctrl) && 
        has_flag(mods, ModifierKeys::Shift)) {
        std::cout << "包含 Ctrl 和 Shift" << std::endl;
    }
}
```

---

### 7. 文本编辑快捷键

```cpp
void on_key_down(KeyEventArgs& args) {
    Key key = args.key();
    ModifierKeys mods = args.modifiers();
    
    // Ctrl+Z 撤销
    if (key == Key::Z && mods == ModifierKeys::Ctrl) {
        undo();
    }
    
    // Ctrl+Shift+Z 重做
    if (key == Key::Z && mods == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
        redo();
    }
    
    // Ctrl+X 剪切
    if (key == Key::X && mods == ModifierKeys::Ctrl) {
        cut();
    }
    
    // Ctrl+C 复制
    if (key == Key::C && mods == ModifierKeys::Ctrl) {
        copy();
    }
    
    // Ctrl+V 粘贴
    if (key == Key::V && mods == ModifierKeys::Ctrl) {
        paste();
    }
}
```

---

## 最佳实践

### 1. 使用 has_flag 而非位运算

```cpp
// ✅ 推荐：使用 has_flag
if (has_flag(args.modifiers(), ModifierKeys::Ctrl)) {
    handle_ctrl();
}

// ❌ 不推荐：手动位运算（不够清晰）
if ((args.modifiers() & ModifierKeys::Ctrl) != ModifierKeys::None) {
    handle_ctrl();
}
```

---

### 2. 精确匹配使用 ==，包含检查使用 has_flag

```cpp
// ✅ 推荐：精确匹配使用 ==
if (args.modifiers() == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
    // 精确匹配 Ctrl+Shift（不能有 Alt）
}

// ✅ 推荐：包含检查使用 has_flag
if (has_flag(args.modifiers(), ModifierKeys::Ctrl)) {
    // 包含 Ctrl（可能还有 Shift/Alt）
}

// ❌ 不推荐：混淆精确匹配和包含检查
```

---

### 3. 组合修饰键使用 | 运算符

```cpp
// ✅ 推荐：使用 | 组合
ModifierKeys combined = ModifierKeys::Ctrl | ModifierKeys::Shift;

// ❌ 不推荐：手动位运算
ModifierKeys combined = static_cast<ModifierKeys>(0x03);  // ❌ 不清晰
```

---

### 4. 快捷键处理顺序：先检查组合修饰键，再检查单个修饰键

```cpp
// ✅ 推荐：先检查组合修饰键
void on_key_down(KeyEventArgs& args) {
    // 先检查 Ctrl+Shift+Z（组合）
    if (args.key() == Key::Z && 
        args.modifiers() == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
        redo();
        return;
    }
    
    // 再检查 Ctrl+Z（单个）
    if (args.key() == Key::Z && args.modifiers() == ModifierKeys::Ctrl) {
        undo();
        return;
    }
}

// ❌ 不推荐：先检查单个修饰键（可能误触发）
void on_key_down(KeyEventArgs& args) {
    // ❌ Ctrl+Shift+Z 会被误识别为 Ctrl+Z
    if (has_flag(args.modifiers(), ModifierKeys::Ctrl) && args.key() == Key::Z) {
        undo();  // ❌ 错误触发
    }
}
```

---

## 常见陷阱

### 1. 混淆精确匹配和包含检查

```cpp
// ❌ 问题：使用 has_flag 进行精确匹配
if (has_flag(args.modifiers(), ModifierKeys::Ctrl) && args.key() == Key::Z) {
    undo();
}
// ❌ Ctrl+Shift+Z 也会触发撤销（错误）

// ✅ 解决：使用 == 进行精确匹配
if (args.modifiers() == ModifierKeys::Ctrl && args.key() == Key::Z) {
    undo();
}
```

---

### 2. 忘记检查修饰键

```cpp
// ❌ 问题：忘记检查修饰键
if (args.key() == Key::S) {
    save_document();
}
// ❌ 单独按 S 键也会保存（错误）

// ✅ 解决：检查修饰键
if (args.key() == Key::S && args.modifiers() == ModifierKeys::Ctrl) {
    save_document();
}
```

---

### 3. 快捷键处理顺序错误

```cpp
// ❌ 问题：先检查单个修饰键
if (args.key() == Key::Z && has_flag(args.modifiers(), ModifierKeys::Ctrl)) {
    undo();  // ❌ Ctrl+Shift+Z 会被误识别为撤销
    return;
}
if (args.key() == Key::Z && 
    args.modifiers() == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
    redo();  // ❌ 永远不会执行
}

// ✅ 解决：先检查组合修饰键
if (args.key() == Key::Z && 
    args.modifiers() == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
    redo();  // 先检查组合
    return;
}
if (args.key() == Key::Z && args.modifiers() == ModifierKeys::Ctrl) {
    undo();  // 再检查单个
}
```

---

### 4. 硬编码修饰键值

```cpp
// ❌ 问题：硬编码修饰键值
if (static_cast<uint8_t>(args.modifiers()) == 0x01) {  // ❌ 不清晰
    handle_ctrl();
}

// ✅ 解决：使用枚举值
if (args.modifiers() == ModifierKeys::Ctrl) {
    handle_ctrl();
}
```

---

## 完整示例

```cpp
#include <mine/ui/input/ModifierKeys.h>
#include <mine/ui/input/Key.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <iostream>

using namespace mine::ui::input;

// ────────────────────────────────────────────────────────────────────────────
// 快捷键处理器
// ────────────────────────────────────────────────────────────────────────────

class ShortcutHandler {
public:
    void on_key_down(KeyEventArgs& args) {
        Key key = args.key();
        ModifierKeys mods = args.modifiers();
        
        // ════════════════════════════════════════════════════════════════════
        // 1. 组合修饰键快捷键（先检查）
        // ════════════════════════════════════════════════════════════════════
        
        // Ctrl+Shift+Z 重做
        if (key == Key::Z && mods == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
            std::cout << "快捷键: Ctrl+Shift+Z (重做)" << std::endl;
            redo();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+Shift+N 新窗口
        if (key == Key::N && mods == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
            std::cout << "快捷键: Ctrl+Shift+N (新窗口)" << std::endl;
            open_new_window();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+Shift+T 重新打开关闭的标签
        if (key == Key::T && mods == (ModifierKeys::Ctrl | ModifierKeys::Shift)) {
            std::cout << "快捷键: Ctrl+Shift+T (重新打开标签)" << std::endl;
            reopen_tab();
            args.set_handled(true);
            return;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 2. 单个修饰键快捷键
        // ════════════════════════════════════════════════════════════════════
        
        // Ctrl+S 保存
        if (key == Key::S && mods == ModifierKeys::Ctrl) {
            std::cout << "快捷键: Ctrl+S (保存)" << std::endl;
            save_document();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+Z 撤销
        if (key == Key::Z && mods == ModifierKeys::Ctrl) {
            std::cout << "快捷键: Ctrl+Z (撤销)" << std::endl;
            undo();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+X 剪切
        if (key == Key::X && mods == ModifierKeys::Ctrl) {
            std::cout << "快捷键: Ctrl+X (剪切)" << std::endl;
            cut();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+C 复制
        if (key == Key::C && mods == ModifierKeys::Ctrl) {
            std::cout << "快捷键: Ctrl+C (复制)" << std::endl;
            copy();
            args.set_handled(true);
            return;
        }
        
        // Ctrl+V 粘贴
        if (key == Key::V && mods == ModifierKeys::Ctrl) {
            std::cout << "快捷键: Ctrl+V (粘贴)" << std::endl;
            paste();
            args.set_handled(true);
            return;
        }
        
        // Shift+Delete 剪切
        if (key == Key::Delete && mods == ModifierKeys::Shift) {
            std::cout << "快捷键: Shift+Delete (剪切)" << std::endl;
            cut();
            args.set_handled(true);
            return;
        }
        
        // Alt+F4 关闭窗口
        if (key == Key::F4 && mods == ModifierKeys::Alt) {
            std::cout << "快捷键: Alt+F4 (关闭窗口)" << std::endl;
            close_window();
            args.set_handled(true);
            return;
        }
        
        // ════════════════════════════════════════════════════════════════════
        // 3. 包含检查（适用于辅助功能）
        // ════════════════════════════════════════════════════════════════════
        
        // 包含 Ctrl 的方向键导航（加速移动）
        if (has_flag(mods, ModifierKeys::Ctrl)) {
            switch (key) {
                case Key::Left:
                    std::cout << "Ctrl+Left (快速左移)" << std::endl;
                    fast_move_left();
                    args.set_handled(true);
                    return;
                    
                case Key::Right:
                    std::cout << "Ctrl+Right (快速右移)" << std::endl;
                    fast_move_right();
                    args.set_handled(true);
                    return;
                    
                default:
                    break;
            }
        }
    }

private:
    void save_document() { std::cout << "  执行: 保存文档" << std::endl; }
    void undo() { std::cout << "  执行: 撤销" << std::endl; }
    void redo() { std::cout << "  执行: 重做" << std::endl; }
    void cut() { std::cout << "  执行: 剪切" << std::endl; }
    void copy() { std::cout << "  执行: 复制" << std::endl; }
    void paste() { std::cout << "  执行: 粘贴" << std::endl; }
    void open_new_window() { std::cout << "  执行: 打开新窗口" << std::endl; }
    void reopen_tab() { std::cout << "  执行: 重新打开标签" << std::endl; }
    void close_window() { std::cout << "  执行: 关闭窗口" << std::endl; }
    void fast_move_left() { std::cout << "  执行: 快速左移" << std::endl; }
    void fast_move_right() { std::cout << "  执行: 快速右移" << std::endl; }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    ShortcutHandler handler;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 单个修饰键快捷键
    // ════════════════════════════════════════════════════════════════════════
    
    KeyEventArgs args1(Key::S, ModifierKeys::Ctrl);
    handler.on_key_down(args1);
    // 输出：快捷键: Ctrl+S (保存)
    //       执行: 保存文档
    
    KeyEventArgs args2(Key::Z, ModifierKeys::Ctrl);
    handler.on_key_down(args2);
    // 输出：快捷键: Ctrl+Z (撤销)
    //       执行: 撤销
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 组合修饰键快捷键
    // ════════════════════════════════════════════════════════════════════════
    
    KeyEventArgs args3(Key::Z, ModifierKeys::Ctrl | ModifierKeys::Shift);
    handler.on_key_down(args3);
    // 输出：快捷键: Ctrl+Shift+Z (重做)
    //       执行: 重做
    
    KeyEventArgs args4(Key::N, ModifierKeys::Ctrl | ModifierKeys::Shift);
    handler.on_key_down(args4);
    // 输出：快捷键: Ctrl+Shift+N (新窗口)
    //       执行: 打开新窗口
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 包含检查示例
    // ════════════════════════════════════════════════════════════════════════
    
    ModifierKeys keys = ModifierKeys::Ctrl | ModifierKeys::Shift;
    
    if (has_flag(keys, ModifierKeys::Ctrl)) {
        std::cout << "包含 Ctrl" << std::endl;
    }
    
    if (has_flag(keys, ModifierKeys::Shift)) {
        std::cout << "包含 Shift" << std::endl;
    }
    
    if (has_flag(keys, ModifierKeys::Alt)) {
        std::cout << "包含 Alt" << std::endl;  // 不会输出
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 组合修饰键示例
    // ════════════════════════════════════════════════════════════════════════
    
    ModifierKeys combined = ModifierKeys::None;
    combined |= ModifierKeys::Ctrl;
    combined |= ModifierKeys::Shift;
    
    std::cout << "组合后的修饰键值: " 
              << static_cast<int>(combined) << std::endl;
    // 输出：组合后的修饰键值: 3 (0x01 | 0x02)
    
    return 0;
}
```

---

## 总结

`ModifierKeys` 是 `mine.ui.input` 模块的修饰键位域枚举，具备：

1. **位域枚举**：支持位运算符（`|`、`&`、`~`）组合多个修饰键
2. **类型安全**：`enum class` 强类型枚举
3. **高效存储**：`uint8_t` 类型，仅占 1 字节
4. **快捷键检测**：用于检测 Ctrl+S、Ctrl+Shift+Z 等快捷键组合
5. **工具函数**：`has_flag` 检查是否包含指定修饰键

**使用建议**：
- 使用 **has_flag** 而非手动位运算
- **精确匹配使用 ==**，包含检查使用 **has_flag**
- 组合修饰键使用 **| 运算符**
- 快捷键处理顺序：**先检查组合修饰键，再检查单个修饰键**
- 避免混淆精确匹配和包含检查
- 避免忘记检查修饰键
- 避免硬编码修饰键值
