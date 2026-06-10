# HorizontalAlignment 详细接口文档

## 概述

`HorizontalAlignment` 是 `mine.ui.visual` 模块的**水平对齐方式枚举**。

**核心特性：**
- **四种对齐**：Left（靠左）、Center（居中）、Right（靠右）、Stretch（拉伸）
- **WPF 语义**：与 WPF/Avalonia 保持一致
- **布局影响**：影响元素在容器水平空间中的位置和尺寸
- **Stretch 默认**：默认值为 Stretch（拉伸填满容器）
- **uint8_t 存储**：节省内存（仅占 1 字节）

---

## 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/HorizontalAlignment.h
```

---

## 枚举定义

```cpp
enum class HorizontalAlignment : uint8_t {
    Left    = 0,
    Center  = 1,
    Right   = 2,
    Stretch = 3,  ///< 默认值
};
```

**描述**：元素在容器水平空间中的对齐方式（与 WPF HorizontalAlignment 语义一致）。

---

## 枚举值

### Left

```cpp
Left = 0
```

**描述**：靠左对齐。

**特征**：
- 元素靠容器左边缘对齐
- 元素宽度为 `desired_size.width`（不拉伸）
- 右侧可能有空白空间

**布局行为**：
```
┌─────────────────────────────┐
│ ┌──────┐                    │  容器
│ │ 元素 │                    │
│ └──────┘                    │
└─────────────────────────────┘
  ↑
  靠左对齐，占用 desired_size.width
```

**示例**：
```cpp
HorizontalAlignment h = HorizontalAlignment::Left;

// 布局行为：元素靠左，宽度 = desired_size.width
```

---

### Center

```cpp
Center = 1
```

**描述**：居中对齐。

**特征**：
- 元素在容器水平方向居中
- 元素宽度为 `desired_size.width`（不拉伸）
- 左右两侧空白空间相等

**布局行为**：
```
┌─────────────────────────────┐
│         ┌──────┐            │  容器
│         │ 元素 │            │
│         └──────┘            │
└─────────────────────────────┘
          ↑
          居中对齐，占用 desired_size.width
```

**示例**：
```cpp
HorizontalAlignment h = HorizontalAlignment::Center;

// 布局行为：元素居中，宽度 = desired_size.width
```

---

### Right

```cpp
Right = 2
```

**描述**：靠右对齐。

**特征**：
- 元素靠容器右边缘对齐
- 元素宽度为 `desired_size.width`（不拉伸）
- 左侧可能有空白空间

**布局行为**：
```
┌─────────────────────────────┐
│                    ┌──────┐ │  容器
│                    │ 元素 │ │
│                    └──────┘ │
└─────────────────────────────┘
                     ↑
                     靠右对齐，占用 desired_size.width
```

**示例**：
```cpp
HorizontalAlignment h = HorizontalAlignment::Right;

// 布局行为：元素靠右，宽度 = desired_size.width
```

---

### Stretch

```cpp
Stretch = 3
```

**描述**：拉伸填满容器可用宽度（默认值）。

**特征**：
- 元素拉伸填满容器分配的宽度
- 元素宽度为容器分配的宽度（而非 `desired_size.width`）
- 无空白空间（除 Margin 外）
- **默认值**

**布局行为**：
```
┌─────────────────────────────┐
│ ┌─────────────────────────┐ │  容器
│ │        元素              │ │
│ └─────────────────────────┘ │
└─────────────────────────────┘
  ↑                          ↑
  拉伸填满容器宽度
```

**示例**：
```cpp
HorizontalAlignment h = HorizontalAlignment::Stretch;

// 布局行为：元素拉伸填满容器，宽度 = 容器分配宽度
```

---

## HorizontalAlignment 值对比

| 对齐方式 | 水平位置 | 元素宽度 | 空白空间 | 数值 |
|----------|----------|----------|----------|------|
| **Left** | 靠左 | `desired_size.width` | 右侧可能有 | 0 |
| **Center** | 居中 | `desired_size.width` | 左右相等 | 1 |
| **Right** | 靠右 | `desired_size.width` | 左侧可能有 | 2 |
| **Stretch** | 拉伸 | **容器分配宽度** | 无（除 Margin） | **3**（默认） |

---

## 使用场景

### 1. 按钮居中对齐

```cpp
#include <mine/ui/visual/HorizontalAlignment.h>

using namespace mine::ui;

// 按钮居中对齐
button.set_horizontal_alignment(HorizontalAlignment::Center);

// 布局行为：按钮在容器中水平居中
```

---

### 2. 标签靠左对齐

```cpp
// 标签靠左对齐
label.set_horizontal_alignment(HorizontalAlignment::Left);

// 布局行为：标签靠容器左边缘
```

---

### 3. 关闭按钮靠右对齐

```cpp
// 关闭按钮靠右对齐（常见于标题栏）
close_button.set_horizontal_alignment(HorizontalAlignment::Right);

// 布局行为：关闭按钮靠容器右边缘
```

---

### 4. 输入框拉伸填满

```cpp
// 输入框拉伸填满容器（默认行为）
text_box.set_horizontal_alignment(HorizontalAlignment::Stretch);

// 布局行为：输入框宽度等于容器分配宽度
```

---

### 5. 对话框按钮靠右对齐

```cpp
// 对话框底部按钮（确定/取消）靠右对齐
button_panel.set_horizontal_alignment(HorizontalAlignment::Right);

// 布局行为：按钮面板靠右
```

---

### 6. 标题居中对齐

```cpp
// 标题文本居中对齐
title_label.set_horizontal_alignment(HorizontalAlignment::Center);

// 布局行为：标题在容器中水平居中
```

---

### 7. 响应式布局（窗口大小变化）

```cpp
// 窗口大小变化时，元素对齐方式决定位置

// Left：始终靠左
left_panel.set_horizontal_alignment(HorizontalAlignment::Left);

// Right：始终靠右
right_panel.set_horizontal_alignment(HorizontalAlignment::Right);

// Stretch：始终填满
content_panel.set_horizontal_alignment(HorizontalAlignment::Stretch);
```

---

## 最佳实践

### 1. 默认使用 Stretch（充分利用空间）

```cpp
// ✅ 推荐：默认使用 Stretch（充分利用容器空间）
text_box.set_horizontal_alignment(HorizontalAlignment::Stretch);

// ❌ 不推荐：除非有明确需求，否则不要使用 Left/Center/Right
text_box.set_horizontal_alignment(HorizontalAlignment::Left);  // 右侧有空白
```

---

### 2. 按钮使用 Center 或 Right（视觉平衡）

```cpp
// ✅ 推荐：按钮居中或靠右（视觉平衡）
ok_button.set_horizontal_alignment(HorizontalAlignment::Center);
close_button.set_horizontal_alignment(HorizontalAlignment::Right);

// ❌ 不推荐：按钮拉伸（通常不符合设计规范）
ok_button.set_horizontal_alignment(HorizontalAlignment::Stretch);
```

---

### 3. 输入控件使用 Stretch（填满宽度）

```cpp
// ✅ 推荐：输入控件拉伸填满（用户体验更好）
text_box.set_horizontal_alignment(HorizontalAlignment::Stretch);
combo_box.set_horizontal_alignment(HorizontalAlignment::Stretch);

// ❌ 不推荐：输入控件固定宽度（右侧有空白）
text_box.set_horizontal_alignment(HorizontalAlignment::Left);
```

---

### 4. 使用枚举类型（而非整数）

```cpp
// ✅ 推荐：使用枚举类型
element.set_horizontal_alignment(HorizontalAlignment::Center);

// ❌ 不推荐：使用整数（类型不安全）
element.set_horizontal_alignment(1);  // ❌ 编译错误（enum class 不允许隐式转换）
```

---

## 常见陷阱

### 1. 混淆 Stretch 和固定宽度

```cpp
// ❌ 问题：期望元素填满容器，却使用了 Left
text_box.set_horizontal_alignment(HorizontalAlignment::Left);
text_box.set_width(200);  // 固定宽度 200

// ❌ 元素宽度固定为 200，右侧有空白

// ✅ 解决：使用 Stretch（移除固定宽度）
text_box.set_horizontal_alignment(HorizontalAlignment::Stretch);
// text_box.set_width(...);  // 不设置固定宽度
```

---

### 2. 按钮使用 Stretch 导致过宽

```cpp
// ❌ 问题：按钮使用 Stretch 导致按钮过宽（不符合设计规范）
ok_button.set_horizontal_alignment(HorizontalAlignment::Stretch);

// ❌ 按钮宽度等于容器宽度（通常不符合设计规范）

// ✅ 解决：按钮使用 Center 或 Right
ok_button.set_horizontal_alignment(HorizontalAlignment::Center);
```

---

### 3. 期望 Left 居中显示

```cpp
// ❌ 问题：期望元素居中，却使用了 Left
label.set_horizontal_alignment(HorizontalAlignment::Left);

// ❌ 元素靠左，不居中

// ✅ 解决：使用 Center
label.set_horizontal_alignment(HorizontalAlignment::Center);
```

---

### 4. 尝试将整数隐式转换为 HorizontalAlignment

```cpp
// ❌ 问题：尝试将整数隐式转换为 HorizontalAlignment
HorizontalAlignment h = 1;  // ❌ 编译错误（enum class 不允许隐式转换）

// ✅ 解决：使用枚举值
HorizontalAlignment h = HorizontalAlignment::Center;

// ✅ 解决：使用显式转换（不推荐）
HorizontalAlignment h = static_cast<HorizontalAlignment>(1);
```

---

## 完整示例

```cpp
#include <mine/ui/visual/HorizontalAlignment.h>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 模拟 UIElement（简化版）
// ────────────────────────────────────────────────────────────────────────────

class UIElement {
public:
    void set_horizontal_alignment(HorizontalAlignment h) {
        if (horizontal_alignment_ != h) {
            horizontal_alignment_ = h;
            std::cout << "HorizontalAlignment changed to: ";
            switch (horizontal_alignment_) {
                case HorizontalAlignment::Left:
                    std::cout << "Left" << std::endl;
                    break;
                case HorizontalAlignment::Center:
                    std::cout << "Center" << std::endl;
                    break;
                case HorizontalAlignment::Right:
                    std::cout << "Right" << std::endl;
                    break;
                case HorizontalAlignment::Stretch:
                    std::cout << "Stretch" << std::endl;
                    break;
            }
        }
    }

    [[nodiscard]] HorizontalAlignment horizontal_alignment() const noexcept {
        return horizontal_alignment_;
    }

    void simulate_layout(double container_width, double desired_width) {
        std::cout << "容器宽度: " << container_width << ", 期望宽度: " << desired_width << std::endl;
        
        double final_width = 0.0;
        double x_offset = 0.0;
        
        switch (horizontal_alignment_) {
            case HorizontalAlignment::Left:
                final_width = desired_width;
                x_offset = 0.0;
                std::cout << "  布局结果: x=" << x_offset << ", width=" << final_width << " (靠左)" << std::endl;
                break;
            
            case HorizontalAlignment::Center:
                final_width = desired_width;
                x_offset = (container_width - desired_width) / 2.0;
                std::cout << "  布局结果: x=" << x_offset << ", width=" << final_width << " (居中)" << std::endl;
                break;
            
            case HorizontalAlignment::Right:
                final_width = desired_width;
                x_offset = container_width - desired_width;
                std::cout << "  布局结果: x=" << x_offset << ", width=" << final_width << " (靠右)" << std::endl;
                break;
            
            case HorizontalAlignment::Stretch:
                final_width = container_width;
                x_offset = 0.0;
                std::cout << "  布局结果: x=" << x_offset << ", width=" << final_width << " (拉伸)" << std::endl;
                break;
        }
    }

private:
    HorizontalAlignment horizontal_alignment_ = HorizontalAlignment::Stretch;
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    UIElement element;
    constexpr double container_width = 400.0;
    constexpr double desired_width = 100.0;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 默认拉伸
    // ════════════════════════════════════════════════════════════════════════
    std::cout << "默认拉伸（Stretch）:" << std::endl;
    element.simulate_layout(container_width, desired_width);
    // 输出：
    //   布局结果: x=0, width=400 (拉伸)
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 靠左对齐
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "靠左对齐（Left）:" << std::endl;
    element.set_horizontal_alignment(HorizontalAlignment::Left);
    element.simulate_layout(container_width, desired_width);
    // 输出：
    //   HorizontalAlignment changed to: Left
    //   布局结果: x=0, width=100 (靠左)
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 居中对齐
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "居中对齐（Center）:" << std::endl;
    element.set_horizontal_alignment(HorizontalAlignment::Center);
    element.simulate_layout(container_width, desired_width);
    // 输出：
    //   HorizontalAlignment changed to: Center
    //   布局结果: x=150, width=100 (居中)
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 靠右对齐
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "靠右对齐（Right）:" << std::endl;
    element.set_horizontal_alignment(HorizontalAlignment::Right);
    element.simulate_layout(container_width, desired_width);
    // 输出：
    //   HorizontalAlignment changed to: Right
    //   布局结果: x=300, width=100 (靠右)
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 响应式布局（容器宽度变化）
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "响应式布局（容器宽度变化）:" << std::endl;
    
    std::cout << "  容器宽度 200:" << std::endl;
    element.set_horizontal_alignment(HorizontalAlignment::Center);
    element.simulate_layout(200.0, desired_width);
    // 输出：
    //   布局结果: x=50, width=100 (居中)
    
    std::cout << "  容器宽度 600:" << std::endl;
    element.simulate_layout(600.0, desired_width);
    // 输出：
    //   布局结果: x=250, width=100 (居中)
    
    return 0;
}
```

---

## 总结

`HorizontalAlignment` 是 `mine.ui.visual` 模块的水平对齐方式枚举，具备：

1. **四种对齐**：Left（靠左）、Center（居中）、Right（靠右）、Stretch（拉伸）
2. **WPF 语义**：与 WPF/Avalonia 保持一致
3. **布局影响**：影响元素在容器水平空间中的位置和尺寸
4. **Stretch 默认**：默认值为 Stretch（拉伸填满容器）
5. **uint8_t 存储**：节省内存（仅占 1 字节）

**使用建议**：
- 默认使用 **Stretch**（充分利用容器空间）
- 按钮使用 **Center** 或 **Right**（视觉平衡）
- 输入控件使用 **Stretch**（填满宽度）
- 使用枚举类型（而非整数）
- 理解 Stretch 和固定宽度的区别
- 避免按钮使用 Stretch（导致过宽）
