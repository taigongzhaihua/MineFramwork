# MouseButton 详细接口文档

## 概述

`MouseButton` 是 `mine.ui.input` 模块的**鼠标按键枚举**。

**核心特性：**
- **平台无关**：统一的鼠标按键枚举，跨 Windows、Linux、macOS
- **完整覆盖**：包含左键、右键、中键、侧键（X1、X2）
- **类型安全**：`enum class` 强类型枚举
- **WindowEvent 对齐**：值与 `WindowEvent::mouse_button` 字段约定一致

**继承关系：**
- 独立枚举类型，无继承关系

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/MouseButton.h
```

---

## 枚举定义

```cpp
enum class MouseButton : uint8_t {
    None   = 255, ///< 无特定按键（例如 MouseMove、MouseWheel）
    Left   = 0,   ///< 左键
    Right  = 1,   ///< 右键
    Middle = 2,   ///< 中键
    X1     = 3,   ///< 侧键 1
    X2     = 4,   ///< 侧键 2
};
```

---

## 枚举值

| 枚举值 | 数值 | 含义 |
|--------|------|------|
| `None` | 255 | 无特定按键（用于 MouseMove、MouseWheel 事件） |
| `Left` | 0 | 鼠标左键 |
| `Right` | 1 | 鼠标右键 |
| `Middle` | 2 | 鼠标中键（滚轮按下） |
| `X1` | 3 | 鼠标侧键 1（通常为"后退"按钮） |
| `X2` | 4 | 鼠标侧键 2（通常为"前进"按钮） |

**说明**：
- `None` 值为 255，用于表示无特定按键的鼠标事件（如 MouseMove、MouseWheel）
- 其他按键值 0-4 与 `WindowEvent::mouse_button` 字段约定一致，便于直接转换

---

## 使用场景

### 1. 鼠标按键按下事件

```cpp
#include <mine/ui/input/MouseButton.h>
#include <mine/ui/input/MouseEventArgs.h>

using namespace mine::ui::input;

void on_mouse_down(MouseEventArgs& args) {
    switch (args.button()) {
        case MouseButton::Left:
            std::cout << "鼠标左键按下" << std::endl;
            handle_left_click();
            break;
            
        case MouseButton::Right:
            std::cout << "鼠标右键按下" << std::endl;
            show_context_menu();
            break;
            
        case MouseButton::Middle:
            std::cout << "鼠标中键按下" << std::endl;
            handle_middle_click();
            break;
            
        case MouseButton::X1:
            std::cout << "鼠标侧键 1 按下（后退）" << std::endl;
            navigate_back();
            break;
            
        case MouseButton::X2:
            std::cout << "鼠标侧键 2 按下（前进）" << std::endl;
            navigate_forward();
            break;
            
        default:
            break;
    }
}
```

---

### 2. 鼠标按键释放事件

```cpp
void on_mouse_up(MouseEventArgs& args) {
    switch (args.button()) {
        case MouseButton::Left:
            std::cout << "鼠标左键释放" << std::endl;
            end_drag();
            break;
            
        case MouseButton::Right:
            std::cout << "鼠标右键释放" << std::endl;
            break;
            
        default:
            break;
    }
}
```

---

### 3. 双击事件

```cpp
void on_mouse_double_click(MouseEventArgs& args) {
    if (args.button() == MouseButton::Left) {
        std::cout << "鼠标左键双击" << std::endl;
        handle_double_click();
    }
}
```

---

### 4. 鼠标移动事件（无特定按键）

```cpp
void on_mouse_move(MouseEventArgs& args) {
    // MouseMove 事件的 button 通常为 MouseButton::None
    if (args.button() == MouseButton::None) {
        std::cout << "鼠标移动: (" << args.x() << ", " << args.y() << ")" << std::endl;
    }
}
```

---

### 5. 右键菜单

```cpp
void on_mouse_down(MouseEventArgs& args) {
    if (args.button() == MouseButton::Right) {
        std::cout << "显示右键菜单" << std::endl;
        show_context_menu(args.x(), args.y());
        args.set_handled(true);
    }
}
```

---

### 6. 拖拽操作（左键拖拽）

```cpp
class DragHandler {
public:
    void on_mouse_down(MouseEventArgs& args) {
        if (args.button() == MouseButton::Left) {
            m_dragging = true;
            m_start_x = args.x();
            m_start_y = args.y();
            std::cout << "开始拖拽" << std::endl;
        }
    }
    
    void on_mouse_move(MouseEventArgs& args) {
        if (m_dragging) {
            int dx = args.x() - m_start_x;
            int dy = args.y() - m_start_y;
            std::cout << "拖拽中: (" << dx << ", " << dy << ")" << std::endl;
            update_drag(dx, dy);
        }
    }
    
    void on_mouse_up(MouseEventArgs& args) {
        if (args.button() == MouseButton::Left && m_dragging) {
            m_dragging = false;
            std::cout << "结束拖拽" << std::endl;
        }
    }

private:
    bool m_dragging = false;
    int m_start_x = 0;
    int m_start_y = 0;
    
    void update_drag(int dx, int dy) {
        // 更新拖拽状态
    }
};
```

---

### 7. 中键滚动

```cpp
void on_mouse_down(MouseEventArgs& args) {
    if (args.button() == MouseButton::Middle) {
        std::cout << "启用中键滚动模式" << std::endl;
        enable_scroll_mode();
        args.set_handled(true);
    }
}
```

---

## 最佳实践

### 1. 使用枚举值而非硬编码数字

```cpp
// ✅ 推荐：使用枚举值
if (args.button() == MouseButton::Left) {
    handle_left_click();
}

// ❌ 不推荐：使用硬编码数字
if (static_cast<uint8_t>(args.button()) == 0) {  // ❌ 难以理解
    handle_left_click();
}
```

---

### 2. 使用 switch-case 处理多个按键

```cpp
// ✅ 推荐：使用 switch-case
switch (args.button()) {
    case MouseButton::Left:
        handle_left_click();
        break;
    case MouseButton::Right:
        show_context_menu();
        break;
    case MouseButton::Middle:
        handle_middle_click();
        break;
    default:
        break;
}

// ❌ 不推荐：使用多个 if-else（代码冗长）
if (args.button() == MouseButton::Left) {
    handle_left_click();
} else if (args.button() == MouseButton::Right) {
    show_context_menu();
} else if (args.button() == MouseButton::Middle) {
    handle_middle_click();
}
```

---

### 3. 区分鼠标按键和鼠标移动

```cpp
// ✅ 推荐：区分按键事件和移动事件
void on_mouse_move(MouseEventArgs& args) {
    // MouseMove 事件的 button 通常为 MouseButton::None
    if (args.button() == MouseButton::None) {
        update_cursor_position(args.x(), args.y());
    }
}

void on_mouse_down(MouseEventArgs& args) {
    // MouseDown 事件的 button 为具体按键
    if (args.button() == MouseButton::Left) {
        handle_left_click();
    }
}
```

---

### 4. 右键菜单处理

```cpp
// ✅ 推荐：右键菜单在 MouseDown 中处理
void on_mouse_down(MouseEventArgs& args) {
    if (args.button() == MouseButton::Right) {
        show_context_menu(args.x(), args.y());
        args.set_handled(true);  // 标记已处理，防止冒泡
    }
}

// ❌ 不推荐：在 MouseUp 中处理右键菜单（用户体验差）
void on_mouse_up(MouseEventArgs& args) {
    if (args.button() == MouseButton::Right) {
        show_context_menu(args.x(), args.y());  // ❌ 延迟显示
    }
}
```

---

## 常见陷阱

### 1. 混淆鼠标按键和键盘按键

```cpp
// ❌ 问题：混淆 MouseButton 和 Key
if (args.button() == Key::Enter) {  // ❌ 类型不匹配
    handle_click();
}

// ✅ 解决：使用 MouseButton 枚举
if (args.button() == MouseButton::Left) {
    handle_click();
}
```

---

### 2. 忘记检查按键类型

```cpp
// ❌ 问题：忘记检查按键类型
void on_mouse_down(MouseEventArgs& args) {
    handle_click();  // ❌ 所有按键都会触发
}

// ✅ 解决：检查按键类型
void on_mouse_down(MouseEventArgs& args) {
    if (args.button() == MouseButton::Left) {
        handle_click();
    }
}
```

---

### 3. 硬编码按键值

```cpp
// ❌ 问题：硬编码按键值
if (static_cast<uint8_t>(args.button()) == 0) {  // ❌ 难以理解
    handle_left_click();
}

// ✅ 解决：使用枚举值
if (args.button() == MouseButton::Left) {
    handle_left_click();
}
```

---

### 4. 混淆 None 和其他按键

```cpp
// ⚠️ 注意：MouseMove 事件的 button 为 MouseButton::None

// ❌ 问题：在 MouseMove 中检查具体按键
void on_mouse_move(MouseEventArgs& args) {
    if (args.button() == MouseButton::Left) {  // ❌ 永远不会触发
        update_drag();
    }
}

// ✅ 解决：使用拖拽状态标志
void on_mouse_down(MouseEventArgs& args) {
    if (args.button() == MouseButton::Left) {
        m_dragging = true;
    }
}

void on_mouse_move(MouseEventArgs& args) {
    if (m_dragging) {  // ✅ 使用状态标志
        update_drag();
    }
}

void on_mouse_up(MouseEventArgs& args) {
    if (args.button() == MouseButton::Left) {
        m_dragging = false;
    }
}
```

---

## 完整示例

```cpp
#include <mine/ui/input/MouseButton.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <iostream>

using namespace mine::ui::input;

// ────────────────────────────────────────────────────────────────────────────
// 鼠标事件处理器
// ────────────────────────────────────────────────────────────────────────────

class MouseHandler {
public:
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标按键按下
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_down(MouseEventArgs& args) {
        MouseButton button = args.button();
        
        switch (button) {
            case MouseButton::Left:
                std::cout << "鼠标左键按下: (" << args.x() << ", " << args.y() << ")" << std::endl;
                m_dragging = true;
                m_start_x = args.x();
                m_start_y = args.y();
                args.set_handled(true);
                break;
                
            case MouseButton::Right:
                std::cout << "鼠标右键按下: (" << args.x() << ", " << args.y() << ")" << std::endl;
                show_context_menu(args.x(), args.y());
                args.set_handled(true);
                break;
                
            case MouseButton::Middle:
                std::cout << "鼠标中键按下: (" << args.x() << ", " << args.y() << ")" << std::endl;
                enable_scroll_mode();
                args.set_handled(true);
                break;
                
            case MouseButton::X1:
                std::cout << "鼠标侧键 1 按下（后退）" << std::endl;
                navigate_back();
                args.set_handled(true);
                break;
                
            case MouseButton::X2:
                std::cout << "鼠标侧键 2 按下（前进）" << std::endl;
                navigate_forward();
                args.set_handled(true);
                break;
                
            default:
                break;
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标按键释放
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_up(MouseEventArgs& args) {
        MouseButton button = args.button();
        
        switch (button) {
            case MouseButton::Left:
                std::cout << "鼠标左键释放: (" << args.x() << ", " << args.y() << ")" << std::endl;
                if (m_dragging) {
                    m_dragging = false;
                    end_drag();
                }
                args.set_handled(true);
                break;
                
            case MouseButton::Right:
                std::cout << "鼠标右键释放: (" << args.x() << ", " << args.y() << ")" << std::endl;
                args.set_handled(true);
                break;
                
            default:
                break;
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标移动
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_move(MouseEventArgs& args) {
        // MouseMove 事件的 button 通常为 MouseButton::None
        if (args.button() == MouseButton::None) {
            std::cout << "鼠标移动: (" << args.x() << ", " << args.y() << ")" << std::endl;
            
            if (m_dragging) {
                int dx = args.x() - m_start_x;
                int dy = args.y() - m_start_y;
                std::cout << "  拖拽偏移: (" << dx << ", " << dy << ")" << std::endl;
                update_drag(dx, dy);
            }
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标双击
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_double_click(MouseEventArgs& args) {
        if (args.button() == MouseButton::Left) {
            std::cout << "鼠标左键双击: (" << args.x() << ", " << args.y() << ")" << std::endl;
            handle_double_click();
            args.set_handled(true);
        }
    }

private:
    bool m_dragging = false;
    int m_start_x = 0;
    int m_start_y = 0;
    
    void show_context_menu(int x, int y) {
        std::cout << "  执行: 显示右键菜单 at (" << x << ", " << y << ")" << std::endl;
    }
    
    void enable_scroll_mode() {
        std::cout << "  执行: 启用中键滚动模式" << std::endl;
    }
    
    void navigate_back() {
        std::cout << "  执行: 后退导航" << std::endl;
    }
    
    void navigate_forward() {
        std::cout << "  执行: 前进导航" << std::endl;
    }
    
    void end_drag() {
        std::cout << "  执行: 结束拖拽" << std::endl;
    }
    
    void update_drag(int dx, int dy) {
        std::cout << "  执行: 更新拖拽状态 (" << dx << ", " << dy << ")" << std::endl;
    }
    
    void handle_double_click() {
        std::cout << "  执行: 处理双击" << std::endl;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    MouseHandler handler;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 左键点击
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args1(MouseButton::Left, 100, 200);
    handler.on_mouse_down(args1);
    // 输出：鼠标左键按下: (100, 200)
    
    MouseEventArgs args2(MouseButton::Left, 100, 200);
    handler.on_mouse_up(args2);
    // 输出：鼠标左键释放: (100, 200)
    //       执行: 结束拖拽
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 右键菜单
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args3(MouseButton::Right, 150, 250);
    handler.on_mouse_down(args3);
    // 输出：鼠标右键按下: (150, 250)
    //       执行: 显示右键菜单 at (150, 250)
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 中键滚动
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args4(MouseButton::Middle, 200, 300);
    handler.on_mouse_down(args4);
    // 输出：鼠标中键按下: (200, 300)
    //       执行: 启用中键滚动模式
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 侧键导航
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args5(MouseButton::X1, 0, 0);
    handler.on_mouse_down(args5);
    // 输出：鼠标侧键 1 按下（后退）
    //       执行: 后退导航
    
    MouseEventArgs args6(MouseButton::X2, 0, 0);
    handler.on_mouse_down(args6);
    // 输出：鼠标侧键 2 按下（前进）
    //       执行: 前进导航
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 鼠标移动
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args7(MouseButton::None, 120, 220);
    handler.on_mouse_move(args7);
    // 输出：鼠标移动: (120, 220)
    
    // ════════════════════════════════════════════════════════════════════════
    // 6. 双击
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args8(MouseButton::Left, 100, 200);
    handler.on_mouse_double_click(args8);
    // 输出：鼠标左键双击: (100, 200)
    //       执行: 处理双击
    
    return 0;
}
```

---

## 总结

`MouseButton` 是 `mine.ui.input` 模块的鼠标按键枚举，具备：

1. **平台无关**：统一的鼠标按键枚举，跨 Windows、Linux、macOS
2. **完整覆盖**：包含左键、右键、中键、侧键（X1、X2）
3. **类型安全**：`enum class` 强类型枚举
4. **WindowEvent 对齐**：值与 `WindowEvent::mouse_button` 字段约定一致（0=左键, 1=右键, 2=中键, 3=X1, 4=X2）
5. **None 值**：用于表示无特定按键的鼠标事件（如 MouseMove、MouseWheel）

**使用建议**：
- 使用枚举值而非硬编码数字
- 使用 **switch-case** 处理多个按键
- 区分鼠标按键和鼠标移动（MouseMove 的 button 为 None）
- **右键菜单在 MouseDown 中处理**（用户体验更好）
- 使用状态标志处理拖拽操作（不要依赖 MouseMove 的 button）
- 避免混淆 MouseButton 和 Key 枚举
- 避免硬编码按键值
