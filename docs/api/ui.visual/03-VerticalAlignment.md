# VerticalAlignment 详细接口文档

## 概述

`VerticalAlignment` 是 `mine.ui.visual` 模块的**垂直对齐方式枚举**。

**核心特性：**
- **四种对齐**：Top（靠顶部）、Center（居中）、Bottom（靠底部）、Stretch（拉伸）
- **WPF 语义**：与 WPF/Avalonia 保持一致
- **布局影响**：影响元素在容器垂直空间中的位置和尺寸
- **Stretch 默认**：默认值为 Stretch（拉伸填满容器）
- **uint8_t 存储**：节省内存（仅占 1 字节）

---

## 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/VerticalAlignment.h
```

---

## 枚举定义

```cpp
enum class VerticalAlignment : uint8_t {
    Top     = 0,
    Center  = 1,
    Bottom  = 2,
    Stretch = 3,  ///< 默认值
};
```

**描述**：元素在容器垂直空间中的对齐方式（与 WPF VerticalAlignment 语义一致）。

---

## 枚举值

### Top

```cpp
Top = 0
```

**描述**：靠顶部对齐。

**特征**：
- 元素靠容器顶部边缘对齐
- 元素高度为 `desired_size.height`（不拉伸）
- 底部可能有空白空间

**布局行为**：
```
┌──────┐
│ 元素 │  ← 靠顶部对齐，占用 desired_size.height
└──────┘
       
       
       
       
       
       
─────────
容器
```

**示例**：
```cpp
VerticalAlignment v = VerticalAlignment::Top;

// 布局行为：元素靠顶部，高度 = desired_size.height
```

---

### Center

```cpp
Center = 1
```

**描述**：居中对齐。

**特征**：
- 元素在容器垂直方向居中
- 元素高度为 `desired_size.height`（不拉伸）
- 顶部和底部空白空间相等

**布局行为**：
```
       
       
       
┌──────┐
│ 元素 │  ← 居中对齐，占用 desired_size.height
└──────┘
       
       
       
─────────
容器
```

**示例**：
```cpp
VerticalAlignment v = VerticalAlignment::Center;

// 布局行为：元素居中，高度 = desired_size.height
```

---

### Bottom

```cpp
Bottom = 2
```

**描述**：靠底部对齐。

**特征**：
- 元素靠容器底部边缘对齐
- 元素高度为 `desired_size.height`（不拉伸）
- 顶部可能有空白空间

**布局行为**：
```
       
       
       
       
       
       
┌──────┐
│ 元素 │  ← 靠底部对齐，占用 desired_size.height
└──────┘
─────────
容器
```

**示例**：
```cpp
VerticalAlignment v = VerticalAlignment::Bottom;

// 布局行为：元素靠底部，高度 = desired_size.height
```

---

### Stretch

```cpp
Stretch = 3
```

**描述**：拉伸填满容器可用高度（默认值）。

**特征**：
- 元素拉伸填满容器分配的高度
- 元素高度为容器分配的高度（而非 `desired_size.height`）
- 无空白空间（除 Margin 外）
- **默认值**

**布局行为**：
```
┌──────┐
│      │
│      │
│ 元素 │  ← 拉伸填满容器高度
│      │
│      │
│      │
│      │
└──────┘
─────────
容器
```

**示例**：
```cpp
VerticalAlignment v = VerticalAlignment::Stretch;

// 布局行为：元素拉伸填满容器，高度 = 容器分配高度
```

---

## VerticalAlignment 值对比

| 对齐方式 | 垂直位置 | 元素高度 | 空白空间 | 数值 |
|----------|----------|----------|----------|------|
| **Top** | 靠顶部 | `desired_size.height` | 底部可能有 | 0 |
| **Center** | 居中 | `desired_size.height` | 顶底相等 | 1 |
| **Bottom** | 靠底部 | `desired_size.height` | 顶部可能有 | 2 |
| **Stretch** | 拉伸 | **容器分配高度** | 无（除 Margin） | **3**（默认） |

---

## 使用场景

### 1. 按钮垂直居中对齐

```cpp
#include <mine/ui/visual/VerticalAlignment.h>

using namespace mine::ui;

// 按钮垂直居中对齐
button.set_vertical_alignment(VerticalAlignment::Center);

// 布局行为：按钮在容器中垂直居中
```

---

### 2. 标题靠顶部对齐

```cpp
// 标题靠顶部对齐
title_label.set_vertical_alignment(VerticalAlignment::Top);

// 布局行为：标题靠容器顶部边缘
```

---

### 3. 状态栏靠底部对齐

```cpp
// 状态栏靠底部对齐（常见于窗口底部）
status_bar.set_vertical_alignment(VerticalAlignment::Bottom);

// 布局行为：状态栏靠容器底部边缘
```

---

### 4. 内容面板拉伸填满

```cpp
// 内容面板拉伸填满容器（默认行为）
content_panel.set_vertical_alignment(VerticalAlignment::Stretch);

// 布局行为：内容面板高度等于容器分配高度
```

---

### 5. 图标居中对齐

```cpp
// 图标垂直居中对齐（与文本对齐）
icon.set_vertical_alignment(VerticalAlignment::Center);

// 布局行为：图标在容器中垂直居中
```

---

### 6. 通知消息靠顶部对齐

```cpp
// 通知消息靠顶部对齐（常见于顶部通知）
notification.set_vertical_alignment(VerticalAlignment::Top);

// 布局行为：通知消息靠容器顶部
```

---

### 7. 响应式布局（窗口高度变化）

```cpp
// 窗口高度变化时，元素对齐方式决定位置

// Top：始终靠顶部
header.set_vertical_alignment(VerticalAlignment::Top);

// Bottom：始终靠底部
footer.set_vertical_alignment(VerticalAlignment::Bottom);

// Stretch：始终填满
content.set_vertical_alignment(VerticalAlignment::Stretch);
```

---

## 最佳实践

### 1. 默认使用 Stretch（充分利用空间）

```cpp
// ✅ 推荐：默认使用 Stretch（充分利用容器空间）
content_panel.set_vertical_alignment(VerticalAlignment::Stretch);

// ❌ 不推荐：除非有明确需求，否则不要使用 Top/Center/Bottom
content_panel.set_vertical_alignment(VerticalAlignment::Top);  // 底部有空白
```

---

### 2. 按钮/标签使用 Center（视觉平衡）

```cpp
// ✅ 推荐：按钮/标签垂直居中（视觉平衡）
button.set_vertical_alignment(VerticalAlignment::Center);
label.set_vertical_alignment(VerticalAlignment::Center);

// ❌ 不推荐：按钮拉伸（通常不符合设计规范）
button.set_vertical_alignment(VerticalAlignment::Stretch);
```

---

### 3. 内容面板使用 Stretch（填满高度）

```cpp
// ✅ 推荐：内容面板拉伸填满（用户体验更好）
content_panel.set_vertical_alignment(VerticalAlignment::Stretch);
scroll_viewer.set_vertical_alignment(VerticalAlignment::Stretch);

// ❌ 不推荐：内容面板固定高度（底部有空白）
content_panel.set_vertical_alignment(VerticalAlignment::Top);
```

---

### 4. 使用枚举类型（而非整数）

```cpp
// ✅ 推荐：使用枚举类型
element.set_vertical_alignment(VerticalAlignment::Center);

// ❌ 不推荐：使用整数（类型不安全）
element.set_vertical_alignment(1);  // ❌ 编译错误（enum class 不允许隐式转换）
```

---

## 常见陷阱

### 1. 混淆 Stretch 和固定高度

```cpp
// ❌ 问题：期望元素填满容器，却使用了 Top
content_panel.set_vertical_alignment(VerticalAlignment::Top);
content_panel.set_height(300);  // 固定高度 300

// ❌ 元素高度固定为 300，底部有空白

// ✅ 解决：使用 Stretch（移除固定高度）
content_panel.set_vertical_alignment(VerticalAlignment::Stretch);
// content_panel.set_height(...);  // 不设置固定高度
```

---

### 2. 按钮使用 Stretch 导致过高

```cpp
// ❌ 问题：按钮使用 Stretch 导致按钮过高（不符合设计规范）
ok_button.set_vertical_alignment(VerticalAlignment::Stretch);

// ❌ 按钮高度等于容器高度（通常不符合设计规范）

// ✅ 解决：按钮使用 Center
ok_button.set_vertical_alignment(VerticalAlignment::Center);
```

---

### 3. 期望 Top 居中显示

```cpp
// ❌ 问题：期望元素居中，却使用了 Top
label.set_vertical_alignment(VerticalAlignment::Top);

// ❌ 元素靠顶部，不居中

// ✅ 解决：使用 Center
label.set_vertical_alignment(VerticalAlignment::Center);
```

---

### 4. 尝试将整数隐式转换为 VerticalAlignment

```cpp
// ❌ 问题：尝试将整数隐式转换为 VerticalAlignment
VerticalAlignment v = 1;  // ❌ 编译错误（enum class 不允许隐式转换）

// ✅ 解决：使用枚举值
VerticalAlignment v = VerticalAlignment::Center;

// ✅ 解决：使用显式转换（不推荐）
VerticalAlignment v = static_cast<VerticalAlignment>(1);
```

---

## 完整示例

```cpp
#include <mine/ui/visual/VerticalAlignment.h>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 模拟 UIElement（简化版）
// ────────────────────────────────────────────────────────────────────────────

class UIElement {
public:
    void set_vertical_alignment(VerticalAlignment v) {
        if (vertical_alignment_ != v) {
            vertical_alignment_ = v;
            std::cout << "VerticalAlignment changed to: ";
            switch (vertical_alignment_) {
                case VerticalAlignment::Top:
                    std::cout << "Top" << std::endl;
                    break;
                case VerticalAlignment::Center:
                    std::cout << "Center" << std::endl;
                    break;
                case VerticalAlignment::Bottom:
                    std::cout << "Bottom" << std::endl;
                    break;
                case VerticalAlignment::Stretch:
                    std::cout << "Stretch" << std::endl;
                    break;
            }
        }
    }

    [[nodiscard]] VerticalAlignment vertical_alignment() const noexcept {
        return vertical_alignment_;
    }

    void simulate_layout(double container_height, double desired_height) {
        std::cout << "容器高度: " << container_height << ", 期望高度: " << desired_height << std::endl;
        
        double final_height = 0.0;
        double y_offset = 0.0;
        
        switch (vertical_alignment_) {
            case VerticalAlignment::Top:
                final_height = desired_height;
                y_offset = 0.0;
                std::cout << "  布局结果: y=" << y_offset << ", height=" << final_height << " (靠顶部)" << std::endl;
                break;
            
            case VerticalAlignment::Center:
                final_height = desired_height;
                y_offset = (container_height - desired_height) / 2.0;
                std::cout << "  布局结果: y=" << y_offset << ", height=" << final_height << " (居中)" << std::endl;
                break;
            
            case VerticalAlignment::Bottom:
                final_height = desired_height;
                y_offset = container_height - desired_height;
                std::cout << "  布局结果: y=" << y_offset << ", height=" << final_height << " (靠底部)" << std::endl;
                break;
            
            case VerticalAlignment::Stretch:
                final_height = container_height;
                y_offset = 0.0;
                std::cout << "  布局结果: y=" << y_offset << ", height=" << final_height << " (拉伸)" << std::endl;
                break;
        }
    }

private:
    VerticalAlignment vertical_alignment_ = VerticalAlignment::Stretch;
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    UIElement element;
    constexpr double container_height = 400.0;
    constexpr double desired_height = 100.0;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 默认拉伸
    // ════════════════════════════════════════════════════════════════════════
    std::cout << "默认拉伸（Stretch）:" << std::endl;
    element.simulate_layout(container_height, desired_height);
    // 输出：
    //   布局结果: y=0, height=400 (拉伸)
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 靠顶部对齐
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "靠顶部对齐（Top）:" << std::endl;
    element.set_vertical_alignment(VerticalAlignment::Top);
    element.simulate_layout(container_height, desired_height);
    // 输出：
    //   VerticalAlignment changed to: Top
    //   布局结果: y=0, height=100 (靠顶部)
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 居中对齐
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "居中对齐（Center）:" << std::endl;
    element.set_vertical_alignment(VerticalAlignment::Center);
    element.simulate_layout(container_height, desired_height);
    // 输出：
    //   VerticalAlignment changed to: Center
    //   布局结果: y=150, height=100 (居中)
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 靠底部对齐
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "靠底部对齐（Bottom）:" << std::endl;
    element.set_vertical_alignment(VerticalAlignment::Bottom);
    element.simulate_layout(container_height, desired_height);
    // 输出：
    //   VerticalAlignment changed to: Bottom
    //   布局结果: y=300, height=100 (靠底部)
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 响应式布局（容器高度变化）
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "响应式布局（容器高度变化）:" << std::endl;
    
    std::cout << "  容器高度 200:" << std::endl;
    element.set_vertical_alignment(VerticalAlignment::Center);
    element.simulate_layout(200.0, desired_height);
    // 输出：
    //   布局结果: y=50, height=100 (居中)
    
    std::cout << "  容器高度 600:" << std::endl;
    element.simulate_layout(600.0, desired_height);
    // 输出：
    //   布局结果: y=250, height=100 (居中)
    
    return 0;
}
```

---

## 总结

`VerticalAlignment` 是 `mine.ui.visual` 模块的垂直对齐方式枚举，具备：

1. **四种对齐**：Top（靠顶部）、Center（居中）、Bottom（靠底部）、Stretch（拉伸）
2. **WPF 语义**：与 WPF/Avalonia 保持一致
3. **布局影响**：影响元素在容器垂直空间中的位置和尺寸
4. **Stretch 默认**：默认值为 Stretch（拉伸填满容器）
5. **uint8_t 存储**：节省内存（仅占 1 字节）

**使用建议**：
- 默认使用 **Stretch**（充分利用容器空间）
- 按钮/标签使用 **Center**（视觉平衡）
- 内容面板使用 **Stretch**（填满高度）
- 使用枚举类型（而非整数）
- 理解 Stretch 和固定高度的区别
- 避免按钮使用 Stretch（导致过高）
