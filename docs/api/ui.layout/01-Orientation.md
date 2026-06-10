# Orientation 详细接口文档

## 概述

`Orientation` 是 `mine.ui.layout` 模块的**布局方向枚举**。

**核心特性：**
- **两种方向**：Horizontal（水平）和 Vertical（垂直）
- **线性排列**：用于 StackPanel 等线性排列容器
- **轻量级**：基于 `uint8_t`，仅占 1 字节

**使用场景：**
- **StackPanel**：指定子元素排列方向
- **ScrollBar**：指定滚动条方向（水平/垂直）
- **Slider**：指定滑块方向
- **线性动画**：指定动画方向

---

## 文件位置

```
src/mine/ui/layout/api/include/mine/ui/layout/Orientation.h
```

---

## 枚举定义

```cpp
namespace mine::ui {

enum class Orientation : uint8_t {
    Horizontal = 0,  // 水平方向（从左到右）
    Vertical   = 1,  // 垂直方向（从上到下）
};

} // namespace mine::ui
```

---

## 枚举值

| 枚举值 | 数值 | 描述 |
|--------|------|------|
| `Horizontal` | `0` | 水平方向（子元素从左到右排列） |
| `Vertical` | `1` | 垂直方向（子元素从上到下排列） |

---

## 使用场景

### 1. StackPanel 水平排列

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/layout/Orientation.h>

using namespace mine::ui;

// 创建水平 StackPanel
StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Horizontal);

// 添加子元素（从左到右排列）
panel->add_child(button1);  // 最左侧
panel->add_child(button2);  // 中间
panel->add_child(button3);  // 最右侧
```

---

### 2. StackPanel 垂直排列

```cpp
// 创建垂直 StackPanel
StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

// 添加子元素（从上到下排列）
panel->add_child(button1);  // 最上方
panel->add_child(button2);  // 中间
panel->add_child(button3);  // 最下方
```

---

### 3. 动态切换方向

```cpp
class DynamicPanel : public StackPanel {
public:
    void toggle_orientation() {
        if (orientation() == Orientation::Horizontal) {
            set_orientation(Orientation::Vertical);
            std::cout << "切换为垂直排列" << std::endl;
        } else {
            set_orientation(Orientation::Horizontal);
            std::cout << "切换为水平排列" << std::endl;
        }
        
        // 触发重新布局
        invalidate_measure();
    }
};
```

---

### 4. 根据方向计算布局

```cpp
void calculate_layout(Orientation orientation, float available_width, float available_height) {
    if (orientation == Orientation::Horizontal) {
        // 水平方向：子元素沿 X 轴排列
        float x = 0.0f;
        for (auto* child : children()) {
            child->arrange({x, 0.0f, child_width, available_height});
            x += child_width + spacing;
        }
    } else {
        // 垂直方向：子元素沿 Y 轴排列
        float y = 0.0f;
        for (auto* child : children()) {
            child->arrange({0.0f, y, available_width, child_height});
            y += child_height + spacing;
        }
    }
}
```

---

### 5. 滚动条方向

```cpp
class ScrollBar : public Control {
public:
    ScrollBar(Orientation orientation)
        : orientation_(orientation) {}
    
    void render() {
        if (orientation_ == Orientation::Horizontal) {
            // 绘制水平滚动条
            draw_horizontal_scrollbar();
        } else {
            // 绘制垂直滚动条
            draw_vertical_scrollbar();
        }
    }

private:
    Orientation orientation_;
};

// 创建垂直滚动条
ScrollBar* vscrollbar = new ScrollBar(Orientation::Vertical);

// 创建水平滚动条
ScrollBar* hscrollbar = new ScrollBar(Orientation::Horizontal);
```

---

### 6. 滑块方向

```cpp
class Slider : public Control {
public:
    Slider(Orientation orientation)
        : orientation_(orientation) {}
    
    void on_mouse_move(MouseEventArgs& args) {
        if (orientation_ == Orientation::Horizontal) {
            // 水平滑块：根据鼠标 X 坐标更新值
            value_ = (args.position().x - bounds().x) / bounds().width;
        } else {
            // 垂直滑块：根据鼠标 Y 坐标更新值
            value_ = (args.position().y - bounds().y) / bounds().height;
        }
    }

private:
    Orientation orientation_;
    float value_{0.0f};
};
```

---

### 7. 方向枚举转换为字符串

```cpp
const char* orientation_to_string(Orientation orientation) {
    switch (orientation) {
        case Orientation::Horizontal:
            return "Horizontal";
        case Orientation::Vertical:
            return "Vertical";
        default:
            return "Unknown";
    }
}

// 使用
std::cout << "当前方向: " << orientation_to_string(panel->orientation()) << std::endl;
```

---

## 最佳实践

### 1. 使用枚举类避免隐式转换

```cpp
// ✅ 推荐：使用枚举类（类型安全）
Orientation orientation = Orientation::Horizontal;

// ❌ 不推荐：使用整数（易出错）
int orientation = 0;  // ❌ 不明确含义
```

---

### 2. 使用 switch 处理所有枚举值

```cpp
// ✅ 推荐：switch 覆盖所有枚举值
switch (orientation) {
    case Orientation::Horizontal:
        handle_horizontal();
        break;
    case Orientation::Vertical:
        handle_vertical();
        break;
    // 编译器会检测是否覆盖所有枚举值
}

// ❌ 不推荐：if-else（未覆盖所有值）
if (orientation == Orientation::Horizontal) {
    handle_horizontal();
}
// ❌ 忘记处理 Vertical
```

---

### 3. 方向变化时触发重新布局

```cpp
// ✅ 推荐：方向变化时触发重新布局
void set_orientation(Orientation orientation) {
    if (orientation_ != orientation) {
        orientation_ = orientation;
        invalidate_measure();  // 触发重新测量
        invalidate_arrange();  // 触发重新排列
    }
}

// ❌ 不推荐：忘记触发重新布局
void set_orientation(Orientation orientation) {
    orientation_ = orientation;
    // ❌ 未触发重新布局
}
```

---

### 4. 使用 const 引用传递枚举

```cpp
// ✅ 推荐：按值传递枚举（轻量级）
void set_orientation(Orientation orientation);

// ⚠️ 不必要：枚举很小，按值传递更高效
void set_orientation(const Orientation& orientation);  // 不必要
```

---

## 常见陷阱

### 1. 混淆水平和垂直

```cpp
// ❌ 问题：混淆水平和垂直
panel->set_orientation(Orientation::Vertical);
// 子元素从上到下排列（而非从左到右）

// ✅ 解决：明确方向含义
// Horizontal：从左到右（水平方向）
// Vertical：从上到下（垂直方向）
```

---

### 2. 忘记触发重新布局

```cpp
// ❌ 问题：方向变化后未触发重新布局
void set_orientation(Orientation orientation) {
    orientation_ = orientation;
    // ❌ 未触发重新布局，UI 不更新
}

// ✅ 解决：触发重新布局
void set_orientation(Orientation orientation) {
    if (orientation_ != orientation) {
        orientation_ = orientation;
        invalidate_measure();
        invalidate_arrange();
    }
}
```

---

### 3. 使用整数代替枚举

```cpp
// ❌ 问题：使用整数代替枚举
void set_orientation(int orientation) {
    if (orientation == 0) {
        // ❌ 魔法数字，不明确含义
    }
}

// ✅ 解决：使用枚举类型
void set_orientation(Orientation orientation) {
    if (orientation == Orientation::Horizontal) {
        // ✅ 类型安全，含义明确
    }
}
```

---

### 4. switch 未覆盖所有枚举值

```cpp
// ❌ 问题：switch 未覆盖所有枚举值
switch (orientation) {
    case Orientation::Horizontal:
        handle_horizontal();
        break;
    // ❌ 缺少 Vertical 分支
}

// ✅ 解决：覆盖所有枚举值
switch (orientation) {
    case Orientation::Horizontal:
        handle_horizontal();
        break;
    case Orientation::Vertical:
        handle_vertical();
        break;
}
```

---

## 完整示例

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/layout/Orientation.h>
#include <mine/ui/controls/Button.h>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 可切换方向的 StackPanel
// ────────────────────────────────────────────────────────────────────────────

class ToggleOrientationPanel : public StackPanel {
public:
    ToggleOrientationPanel() {
        // 初始为水平排列
        set_orientation(Orientation::Horizontal);
    }
    
    void toggle_orientation() {
        if (orientation() == Orientation::Horizontal) {
            set_orientation(Orientation::Vertical);
            std::cout << "切换为垂直排列" << std::endl;
        } else {
            set_orientation(Orientation::Horizontal);
            std::cout << "切换为水平排列" << std::endl;
        }
    }
    
    void print_info() const {
        std::cout << "当前方向: " 
                  << orientation_to_string(orientation()) << std::endl;
        std::cout << "子元素数量: " << children().size() << std::endl;
    }

private:
    const char* orientation_to_string(Orientation orientation) const {
        switch (orientation) {
            case Orientation::Horizontal:
                return "水平（从左到右）";
            case Orientation::Vertical:
                return "垂直（从上到下）";
            default:
                return "未知";
        }
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 主函数
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // ════════════════════════════════════════════════════════════════════════
    // 1. 创建面板
    // ════════════════════════════════════════════════════════════════════════
    
    ToggleOrientationPanel panel;
    
    // 添加子元素
    Button* button1 = new Button();
    button1->set_content("按钮 1");
    panel.add_child(button1);
    
    Button* button2 = new Button();
    button2->set_content("按钮 2");
    panel.add_child(button2);
    
    Button* button3 = new Button();
    button3->set_content("按钮 3");
    panel.add_child(button3);
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 初始状态（水平排列）
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "=== 初始状态 ===" << std::endl;
    panel.print_info();
    // 输出：
    // 当前方向: 水平（从左到右）
    // 子元素数量: 3
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 切换为垂直排列
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 切换方向 ===" << std::endl;
    panel.toggle_orientation();
    // 输出：切换为垂直排列
    
    panel.print_info();
    // 输出：
    // 当前方向: 垂直（从上到下）
    // 子元素数量: 3
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 再次切换（回到水平）
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 再次切换 ===" << std::endl;
    panel.toggle_orientation();
    // 输出：切换为水平排列
    
    panel.print_info();
    // 输出：
    // 当前方向: 水平（从左到右）
    // 子元素数量: 3
    
    return 0;
}
```

**运行效果**：
```
=== 初始状态 ===
当前方向: 水平（从左到右）
子元素数量: 3

=== 切换方向 ===
切换为垂直排列
当前方向: 垂直（从上到下）
子元素数量: 3

=== 再次切换 ===
切换为水平排列
当前方向: 水平（从左到右）
子元素数量: 3
```

---

## 总结

`Orientation` 是 `mine.ui.layout` 模块的布局方向枚举，具备：

1. **两种方向**：Horizontal（水平，从左到右）和 Vertical（垂直，从上到下）
2. **线性排列**：用于 StackPanel 等线性排列容器
3. **轻量级**：基于 `uint8_t`，仅占 1 字节
4. **类型安全**：使用 `enum class`，避免隐式转换

**使用建议**：
- **使用枚举类**（类型安全）
- **switch 覆盖所有枚举值**
- **方向变化时触发重新布局**
- **按值传递枚举**（轻量级）
- 避免混淆水平和垂直
- 避免忘记触发重新布局
- 避免使用整数代替枚举
- 避免 switch 未覆盖所有枚举值
