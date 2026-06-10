# MouseEventArgs 详细接口文档

## 概述

`MouseEventArgs` 是 `mine.ui.input` 模块的**鼠标路由事件参数类**。

**核心特性：**
- **路由事件参数**：继承自 `RoutedEventArgs`，支持事件冒泡和隧道
- **完整鼠标信息**：包含按键、鼠标位置、修饰键状态、滚轮增量
- **逻辑坐标**：鼠标位置使用逻辑坐标（相对于根元素）
- **快捷访问**：提供 `ctrl()`、`shift()`、`alt()` 快捷方法
- **栈分配**：在调用栈上分配，不可拷贝

**继承关系：**
```
RoutedEventArgs
    └── MouseEventArgs
```

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/MouseEventArgs.h
```

---

## 类定义

```cpp
class MINE_UI_INPUT_API MouseEventArgs : public RoutedEventArgs {
public:
    explicit MouseEventArgs(const RoutedEvent& event,
                            MouseButton        button,
                            math::Point        position,
                            ModifierKeys       modifiers,
                            float              wheel_delta = 0.0f) noexcept;

    [[nodiscard]] MouseButton button() const noexcept;
    [[nodiscard]] math::Point position() const noexcept;
    [[nodiscard]] ModifierKeys modifiers() const noexcept;
    [[nodiscard]] float wheel_delta() const noexcept;

    [[nodiscard]] bool ctrl() const noexcept;
    [[nodiscard]] bool shift() const noexcept;
    [[nodiscard]] bool alt() const noexcept;

private:
    MouseButton  button_;
    math::Point  position_;
    ModifierKeys modifiers_;
    float        wheel_delta_;
};
```

---

## 构造函数

### MouseEventArgs

```cpp
explicit MouseEventArgs(const RoutedEvent& event,
                        MouseButton        button,
                        math::Point        position,
                        ModifierKeys       modifiers,
                        float              wheel_delta = 0.0f) noexcept;
```

**描述**：构造鼠标事件参数。

**参数**：
- `event`：路由事件描述符（如 `MouseDownEvent()`、`MouseUpEvent()`、`MouseMoveEvent()`、`MouseWheelEvent()`）
- `button`：触发事件的鼠标按键（MouseMove/MouseWheel 传 `MouseButton::None`）
- `position`：鼠标位置（相对于根元素的逻辑坐标，单位：逻辑像素）
- `modifiers`：修饰键状态（`ModifierKeys` 位域）
- `wheel_delta`：滚轮增量（正值向上，一格约 120.0f；非滚轮事件传 0）

**特征**：
- `noexcept`：保证不抛出异常
- `explicit`：禁止隐式转换

**示例（鼠标按下）**：
```cpp
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/InputEvents.h>

using namespace mine::ui::input;

// 构造 MouseDown 事件参数
MouseEventArgs args(
    MouseDownEvent(),         // 事件描述符
    MouseButton::Left,        // 鼠标左键
    {100.0f, 200.0f},         // 鼠标位置
    ModifierKeys::None,       // 无修饰键
    0.0f                      // 无滚轮增量
);

// 触发事件
EventManager::raise(*element, args);
```

**示例（滚轮事件）**：
```cpp
// 构造 MouseWheel 事件参数
MouseEventArgs args(
    MouseWheelEvent(),        // 事件描述符
    MouseButton::None,        // 无特定按键
    {150.0f, 250.0f},         // 鼠标位置
    ModifierKeys::None,       // 无修饰键
    120.0f                    // 滚轮向上一格
);

// 触发事件
EventManager::raise(*element, args);
```

---

## 成员方法

### button

```cpp
[[nodiscard]] MouseButton button() const noexcept;
```

**描述**：获取触发事件的鼠标按键。

**返回值**：
- `MouseButton`：鼠标按键枚举
  - MouseDown/MouseUp 事件：具体按键（Left、Right、Middle、X1、X2）
  - MouseMove/MouseWheel 事件：`MouseButton::None`

**示例**：
```cpp
void on_mouse_down(MouseEventArgs& args) {
    MouseButton btn = args.button();
    if (btn == MouseButton::Left) {
        std::cout << "鼠标左键按下" << std::endl;
    }
}
```

---

### position

```cpp
[[nodiscard]] math::Point position() const noexcept;
```

**描述**：获取鼠标位置（逻辑坐标）。

**返回值**：
- `math::Point`：鼠标位置（相对于根元素的逻辑坐标，单位：逻辑像素）

**说明**：
- 逻辑坐标：与 DPI 无关的坐标，1 逻辑像素 = 1/96 英寸
- 相对于根元素：坐标原点为根元素左上角

**示例**：
```cpp
void on_mouse_move(MouseEventArgs& args) {
    math::Point pos = args.position();
    std::cout << "鼠标位置: (" << pos.x << ", " << pos.y << ")" << std::endl;
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
void on_mouse_down(MouseEventArgs& args) {
    ModifierKeys mods = args.modifiers();
    
    // 检查 Ctrl
    if (has_flag(mods, ModifierKeys::Ctrl)) {
        std::cout << "Ctrl 键按下" << std::endl;
    }
}
```

---

### wheel_delta

```cpp
[[nodiscard]] float wheel_delta() const noexcept;
```

**描述**：获取滚轮增量。

**返回值**：
- `float`：滚轮增量
  - 正值：向上滚动
  - 负值：向下滚动
  - 一格约 120.0f
  - 非滚轮事件返回 0.0f

**示例**：
```cpp
void on_mouse_wheel(MouseEventArgs& args) {
    float delta = args.wheel_delta();
    if (delta > 0) {
        std::cout << "向上滚动: " << delta << std::endl;
        zoom_in();
    } else if (delta < 0) {
        std::cout << "向下滚动: " << delta << std::endl;
        zoom_out();
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
void on_mouse_down(MouseEventArgs& args) {
    if (args.ctrl() && args.button() == MouseButton::Left) {
        std::cout << "Ctrl+左键点击" << std::endl;
        multi_select();
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
void on_mouse_down(MouseEventArgs& args) {
    if (args.shift() && args.button() == MouseButton::Left) {
        std::cout << "Shift+左键点击" << std::endl;
        range_select();
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
void on_mouse_down(MouseEventArgs& args) {
    if (args.alt() && args.button() == MouseButton::Left) {
        std::cout << "Alt+左键点击" << std::endl;
        special_action();
    }
}
```

---

## 使用场景

### 1. 鼠标按键事件

```cpp
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/InputEvents.h>

using namespace mine::ui::input;

class MyControl : public Visual {
protected:
    void on_mouse_down(MouseEventArgs& args) override {
        switch (args.button()) {
            case MouseButton::Left:
                std::cout << "鼠标左键按下: (" 
                          << args.position().x << ", " 
                          << args.position().y << ")" << std::endl;
                handle_left_click(args.position());
                args.set_handled(true);
                break;
                
            case MouseButton::Right:
                std::cout << "鼠标右键按下" << std::endl;
                show_context_menu(args.position());
                args.set_handled(true);
                break;
                
            default:
                Visual::on_mouse_down(args);
                break;
        }
    }
    
    void on_mouse_up(MouseEventArgs& args) override {
        if (args.button() == MouseButton::Left) {
            std::cout << "鼠标左键释放" << std::endl;
            end_drag();
            args.set_handled(true);
        }
    }
};
```

---

### 2. 鼠标移动事件

```cpp
void on_mouse_move(MouseEventArgs& args) override {
    math::Point pos = args.position();
    std::cout << "鼠标移动: (" << pos.x << ", " << pos.y << ")" << std::endl;
    
    if (m_dragging) {
        update_drag(pos);
    }
}
```

---

### 3. 滚轮事件

```cpp
void on_mouse_wheel(MouseEventArgs& args) override {
    float delta = args.wheel_delta();
    
    if (delta > 0) {
        std::cout << "向上滚动: " << delta << std::endl;
        zoom_in();
    } else if (delta < 0) {
        std::cout << "向下滚动: " << delta << std::endl;
        zoom_out();
    }
    
    args.set_handled(true);
}
```

---

### 4. 拖拽操作

```cpp
class DragControl : public Visual {
protected:
    void on_mouse_down(MouseEventArgs& args) override {
        if (args.button() == MouseButton::Left) {
            m_dragging = true;
            m_start_pos = args.position();
            std::cout << "开始拖拽: (" << m_start_pos.x << ", " << m_start_pos.y << ")" << std::endl;
            args.set_handled(true);
        }
    }
    
    void on_mouse_move(MouseEventArgs& args) override {
        if (m_dragging) {
            math::Point pos = args.position();
            float dx = pos.x - m_start_pos.x;
            float dy = pos.y - m_start_pos.y;
            std::cout << "拖拽中: (" << dx << ", " << dy << ")" << std::endl;
            update_drag(dx, dy);
        }
    }
    
    void on_mouse_up(MouseEventArgs& args) override {
        if (args.button() == MouseButton::Left && m_dragging) {
            m_dragging = false;
            std::cout << "结束拖拽" << std::endl;
            args.set_handled(true);
        }
    }

private:
    bool m_dragging = false;
    math::Point m_start_pos;
    
    void update_drag(float dx, float dy) {
        // 更新拖拽状态
    }
};
```

---

### 5. 修饰键组合

```cpp
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        if (args.ctrl()) {
            std::cout << "Ctrl+左键点击（多选）" << std::endl;
            multi_select(args.position());
        } else if (args.shift()) {
            std::cout << "Shift+左键点击（范围选择）" << std::endl;
            range_select(args.position());
        } else {
            std::cout << "左键点击（单选）" << std::endl;
            single_select(args.position());
        }
        args.set_handled(true);
    }
}
```

---

### 6. 右键菜单

```cpp
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Right) {
        math::Point pos = args.position();
        std::cout << "显示右键菜单: (" << pos.x << ", " << pos.y << ")" << std::endl;
        show_context_menu(pos);
        args.set_handled(true);
    }
}
```

---

### 7. 双击事件

```cpp
void on_mouse_double_click(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        std::cout << "鼠标左键双击" << std::endl;
        handle_double_click(args.position());
        args.set_handled(true);
    }
}
```

---

## 最佳实践

### 1. 使用快捷方法检查修饰键

```cpp
// ✅ 推荐：使用快捷方法
if (args.ctrl() && args.button() == MouseButton::Left) {
    multi_select(args.position());
}

// ⚠️ 可用但冗长：使用 modifiers()
if (has_flag(args.modifiers(), ModifierKeys::Ctrl) && 
    args.button() == MouseButton::Left) {
    multi_select(args.position());
}
```

---

### 2. 区分鼠标按键和移动事件

```cpp
// ✅ 推荐：区分按键事件和移动事件
void on_mouse_down(MouseEventArgs& args) override {
    // args.button() 为具体按键
    if (args.button() == MouseButton::Left) {
        m_dragging = true;
    }
}

void on_mouse_move(MouseEventArgs& args) override {
    // args.button() 为 MouseButton::None
    if (m_dragging) {
        update_drag(args.position());
    }
}
```

---

### 3. 使用 set_handled 阻止事件冒泡

```cpp
// ✅ 推荐：处理完事件后调用 set_handled
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        handle_click(args.position());
        args.set_handled(true);  // 阻止事件继续冒泡
    }
}

// ❌ 不推荐：忘记调用 set_handled（事件会继续冒泡）
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        handle_click(args.position());
        // ❌ 忘记调用 set_handled
    }
}
```

---

### 4. 滚轮增量归一化

```cpp
// ✅ 推荐：归一化滚轮增量（一格约 120.0f）
void on_mouse_wheel(MouseEventArgs& args) override {
    float delta = args.wheel_delta();
    float normalized_delta = delta / 120.0f;  // 归一化为格数
    
    if (normalized_delta > 0) {
        zoom_in(normalized_delta);
    } else if (normalized_delta < 0) {
        zoom_out(-normalized_delta);
    }
}
```

---

## 常见陷阱

### 1. 混淆逻辑坐标和物理像素

```cpp
// ⚠️ 注意：position() 返回逻辑坐标（DPI 无关）

// ❌ 问题：将逻辑坐标当作物理像素
void on_mouse_down(MouseEventArgs& args) override {
    math::Point pos = args.position();
    // ❌ 如果 DPI 为 200%，物理像素坐标应为 pos * 2
    draw_at_physical_pixel(pos.x, pos.y);  // ❌ 位置错误
}

// ✅ 解决：根据 DPI 转换坐标
void on_mouse_down(MouseEventArgs& args) override {
    math::Point logical_pos = args.position();
    float dpi_scale = get_dpi_scale();
    math::Point physical_pos = {
        logical_pos.x * dpi_scale,
        logical_pos.y * dpi_scale
    };
    draw_at_physical_pixel(physical_pos.x, physical_pos.y);
}
```

---

### 2. 忘记检查按键类型

```cpp
// ❌ 问题：忘记检查按键类型
void on_mouse_down(MouseEventArgs& args) override {
    handle_click(args.position());  // ❌ 所有按键都会触发
}

// ✅ 解决：检查按键类型
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        handle_click(args.position());
    }
}
```

---

### 3. MouseMove 中检查具体按键

```cpp
// ❌ 问题：在 MouseMove 中检查具体按键
void on_mouse_move(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {  // ❌ 永远不会触发
        update_drag(args.position());
    }
}

// ✅ 解决：使用拖拽状态标志
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        m_dragging = true;
    }
}

void on_mouse_move(MouseEventArgs& args) override {
    if (m_dragging) {  // ✅ 使用状态标志
        update_drag(args.position());
    }
}

void on_mouse_up(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        m_dragging = false;
    }
}
```

---

### 4. 忘记调用 set_handled

```cpp
// ❌ 问题：忘记调用 set_handled
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        handle_click(args.position());
        // ❌ 忘记调用 set_handled，事件会继续冒泡
    }
}

// ✅ 解决：调用 set_handled
void on_mouse_down(MouseEventArgs& args) override {
    if (args.button() == MouseButton::Left) {
        handle_click(args.position());
        args.set_handled(true);  // 阻止事件冒泡
    }
}
```

---

## 完整示例

```cpp
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/input/InputEvents.h>
#include <mine/ui/visual/Visual.h>
#include <iostream>

using namespace mine::ui::input;
using namespace mine::ui::visual;
using namespace mine::math;

// ────────────────────────────────────────────────────────────────────────────
// 可拖拽控件
// ────────────────────────────────────────────────────────────────────────────

class DraggableControl : public Visual {
protected:
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标按键按下
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_down(MouseEventArgs& args) override {
        MouseButton button = args.button();
        Point pos = args.position();
        
        switch (button) {
            case MouseButton::Left:
                std::cout << "鼠标左键按下: (" << pos.x << ", " << pos.y << ")" << std::endl;
                
                // 修饰键组合
                if (args.ctrl()) {
                    std::cout << "  Ctrl+左键（多选）" << std::endl;
                    multi_select(pos);
                } else if (args.shift()) {
                    std::cout << "  Shift+左键（范围选择）" << std::endl;
                    range_select(pos);
                } else {
                    std::cout << "  左键（开始拖拽）" << std::endl;
                    m_dragging = true;
                    m_start_pos = pos;
                }
                args.set_handled(true);
                break;
                
            case MouseButton::Right:
                std::cout << "鼠标右键按下: (" << pos.x << ", " << pos.y << ")" << std::endl;
                show_context_menu(pos);
                args.set_handled(true);
                break;
                
            case MouseButton::Middle:
                std::cout << "鼠标中键按下: (" << pos.x << ", " << pos.y << ")" << std::endl;
                enable_scroll_mode();
                args.set_handled(true);
                break;
                
            default:
                Visual::on_mouse_down(args);
                break;
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标按键释放
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_up(MouseEventArgs& args) override {
        if (args.button() == MouseButton::Left && m_dragging) {
            Point pos = args.position();
            std::cout << "鼠标左键释放: (" << pos.x << ", " << pos.y << ")" << std::endl;
            std::cout << "  结束拖拽" << std::endl;
            m_dragging = false;
            args.set_handled(true);
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标移动
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_move(MouseEventArgs& args) override {
        Point pos = args.position();
        
        if (m_dragging) {
            float dx = pos.x - m_start_pos.x;
            float dy = pos.y - m_start_pos.y;
            std::cout << "拖拽中: (" << dx << ", " << dy << ")" << std::endl;
            update_drag(dx, dy);
        } else {
            std::cout << "鼠标移动: (" << pos.x << ", " << pos.y << ")" << std::endl;
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标双击
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_double_click(MouseEventArgs& args) override {
        if (args.button() == MouseButton::Left) {
            Point pos = args.position();
            std::cout << "鼠标左键双击: (" << pos.x << ", " << pos.y << ")" << std::endl;
            handle_double_click(pos);
            args.set_handled(true);
        }
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 鼠标滚轮
    // ════════════════════════════════════════════════════════════════════════
    void on_mouse_wheel(MouseEventArgs& args) override {
        float delta = args.wheel_delta();
        float normalized_delta = delta / 120.0f;  // 归一化为格数
        
        if (normalized_delta > 0) {
            std::cout << "向上滚动: " << normalized_delta << " 格" << std::endl;
            zoom_in(normalized_delta);
        } else if (normalized_delta < 0) {
            std::cout << "向下滚动: " << -normalized_delta << " 格" << std::endl;
            zoom_out(-normalized_delta);
        }
        
        args.set_handled(true);
    }

private:
    bool m_dragging = false;
    Point m_start_pos;
    
    void multi_select(const Point& pos) {
        std::cout << "    执行: 多选 at (" << pos.x << ", " << pos.y << ")" << std::endl;
    }
    
    void range_select(const Point& pos) {
        std::cout << "    执行: 范围选择 at (" << pos.x << ", " << pos.y << ")" << std::endl;
    }
    
    void show_context_menu(const Point& pos) {
        std::cout << "    执行: 显示右键菜单 at (" << pos.x << ", " << pos.y << ")" << std::endl;
    }
    
    void enable_scroll_mode() {
        std::cout << "    执行: 启用中键滚动模式" << std::endl;
    }
    
    void update_drag(float dx, float dy) {
        std::cout << "    执行: 更新拖拽状态 (" << dx << ", " << dy << ")" << std::endl;
    }
    
    void handle_double_click(const Point& pos) {
        std::cout << "    执行: 处理双击 at (" << pos.x << ", " << pos.y << ")" << std::endl;
    }
    
    void zoom_in(float delta) {
        std::cout << "    执行: 放大 (" << delta << ")" << std::endl;
    }
    
    void zoom_out(float delta) {
        std::cout << "    执行: 缩小 (" << delta << ")" << std::endl;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    DraggableControl control;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 鼠标左键点击
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args1(
        MouseDownEvent(),
        MouseButton::Left,
        {100.0f, 200.0f},
        ModifierKeys::None
    );
    control.on_mouse_down(args1);
    // 输出：鼠标左键按下: (100, 200)
    //       左键（开始拖拽）
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 鼠标移动（拖拽）
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args2(
        MouseMoveEvent(),
        MouseButton::None,  // MouseMove 的 button 为 None
        {120.0f, 220.0f},
        ModifierKeys::None
    );
    control.on_mouse_move(args2);
    // 输出：拖拽中: (20, 20)
    //       执行: 更新拖拽状态 (20, 20)
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 鼠标左键释放
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args3(
        MouseUpEvent(),
        MouseButton::Left,
        {120.0f, 220.0f},
        ModifierKeys::None
    );
    control.on_mouse_up(args3);
    // 输出：鼠标左键释放: (120, 220)
    //       结束拖拽
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. Ctrl+左键点击
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args4(
        MouseDownEvent(),
        MouseButton::Left,
        {150.0f, 250.0f},
        ModifierKeys::Ctrl
    );
    control.on_mouse_down(args4);
    // 输出：鼠标左键按下: (150, 250)
    //       Ctrl+左键（多选）
    //       执行: 多选 at (150, 250)
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 鼠标滚轮
    // ════════════════════════════════════════════════════════════════════════
    
    MouseEventArgs args5(
        MouseWheelEvent(),
        MouseButton::None,  // MouseWheel 的 button 为 None
        {200.0f, 300.0f},
        ModifierKeys::None,
        120.0f  // 向上一格
    );
    control.on_mouse_wheel(args5);
    // 输出：向上滚动: 1 格
    //       执行: 放大 (1)
    
    return 0;
}
```

---

## 总结

`MouseEventArgs` 是 `mine.ui.input` 模块的鼠标路由事件参数类，具备：

1. **路由事件参数**：继承自 `RoutedEventArgs`，支持事件冒泡和隧道
2. **完整鼠标信息**：包含按键、鼠标位置、修饰键状态、滚轮增量
3. **逻辑坐标**：鼠标位置使用逻辑坐标（相对于根元素，与 DPI 无关）
4. **快捷访问**：提供 `ctrl()`、`shift()`、`alt()` 快捷方法
5. **栈分配**：在调用栈上分配，不可拷贝

**使用建议**：
- 使用 **快捷方法**（`ctrl()`、`shift()`、`alt()`）检查修饰键
- **区分鼠标按键和移动事件**（MouseMove 的 button 为 None）
- 使用 **set_handled** 阻止事件冒泡
- **滚轮增量归一化**（一格约 120.0f）
- 避免混淆逻辑坐标和物理像素
- 避免忘记检查按键类型
- 避免在 MouseMove 中检查具体按键（使用拖拽状态标志）
- 避免忘记调用 set_handled
