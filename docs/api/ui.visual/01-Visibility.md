# Visibility 详细接口文档

## 概述

`Visibility` 是 `mine.ui.visual` 模块的**元素可见性枚举**。

**核心特性：**
- **三种状态**：Visible（可见）、Hidden（隐藏）、Collapsed（折叠）
- **WPF 语义**：与 WPF/Avalonia 保持一致
- **布局影响**：Collapsed 不占空间，Hidden 占据空间但不渲染
- **渲染影响**：Visible 正常渲染，Hidden/Collapsed 不渲染
- **uint8_t 存储**：节省内存（仅占 1 字节）

---

## 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/Visibility.h
```

---

## 枚举定义

```cpp
enum class Visibility : uint8_t {
    Visible   = 0, ///< 可见：参与布局与渲染
    Hidden    = 1, ///< 隐藏：占据布局空间但不渲染
    Collapsed = 2, ///< 折叠：不渲染也不占据布局空间
};
```

**描述**：元素可见性枚举（与 WPF/Avalonia 语义一致）。

---

## 枚举值

### Visible

```cpp
Visible = 0
```

**描述**：可见状态。

**特征**：
- 元素正常渲染
- 元素参与布局（占据空间）
- 可以接收输入事件
- 默认值（数值为 0）

**示例**：
```cpp
Visibility v = Visibility::Visible;

// 布局行为：元素参与布局，占据空间
// 渲染行为：元素正常渲染
// 输入行为：可以接收鼠标、键盘等事件
```

---

### Hidden

```cpp
Hidden = 1
```

**描述**：隐藏状态（占据布局空间但不渲染）。

**特征**：
- 元素不渲染（屏幕上不可见）
- 元素仍参与布局（占据空间）
- 不接收输入事件
- 类似 CSS `visibility: hidden`

**示例**：
```cpp
Visibility v = Visibility::Hidden;

// 布局行为：元素参与布局，占据空间（其他元素位置不变）
// 渲染行为：元素不渲染（屏幕上不可见）
// 输入行为：不接收输入事件
```

**使用场景**：
- 临时隐藏元素但保持布局稳定
- 避免其他元素位置跳动
- 占位符效果

---

### Collapsed

```cpp
Collapsed = 2
```

**描述**：折叠状态（不渲染也不占据布局空间）。

**特征**：
- 元素不渲染（屏幕上不可见）
- 元素不参与布局（不占据空间）
- 不接收输入事件
- 类似 CSS `display: none`

**示例**：
```cpp
Visibility v = Visibility::Collapsed;

// 布局行为：元素不参与布局，不占据空间（其他元素位置重排）
// 渲染行为：元素不渲染（屏幕上不可见）
// 输入行为：不接收输入事件
```

**使用场景**：
- 完全隐藏元素并释放布局空间
- 动态显示/隐藏面板
- 条件渲染（类似 `v-if` / `*ngIf`）

---

## Visibility 值对比

| 特性 | Visible | Hidden | Collapsed |
|------|---------|--------|-----------|
| **渲染** | ✅ 渲染 | ❌ 不渲染 | ❌ 不渲染 |
| **占据布局空间** | ✅ 占据 | ✅ 占据 | ❌ 不占据 |
| **接收输入事件** | ✅ 接收 | ❌ 不接收 | ❌ 不接收 |
| **其他元素位置** | 不变 | 不变 | **重排** |
| **类似 CSS** | `visibility: visible` | `visibility: hidden` | `display: none` |
| **数值** | 0 | 1 | 2 |

---

## 使用场景

### 1. 临时隐藏元素但保持布局（Hidden）

```cpp
#include <mine/ui/visual/Visibility.h>

using namespace mine::ui;

// 临时隐藏元素（保持其他元素位置不变）
element.set_visibility(Visibility::Hidden);

// 布局稳定：其他元素位置不变
// 适用场景：加载占位符、临时隐藏
```

---

### 2. 完全隐藏元素并释放空间（Collapsed）

```cpp
// 完全隐藏元素（其他元素重排填充空间）
element.set_visibility(Visibility::Collapsed);

// 布局重排：其他元素位置重新计算
// 适用场景：条件显示面板、动态菜单
```

---

### 3. 显示元素（Visible）

```cpp
// 显示元素
element.set_visibility(Visibility::Visible);

// 元素正常渲染和布局
```

---

### 4. 切换可见性（Toggle）

```cpp
// 切换可见性（Visible <-> Collapsed）
void toggle_visibility(UIElement& element) {
    if (element.visibility() == Visibility::Visible) {
        element.set_visibility(Visibility::Collapsed);
    } else {
        element.set_visibility(Visibility::Visible);
    }
}
```

---

### 5. 条件渲染（类似 v-if）

```cpp
// 条件渲染：根据数据决定是否显示
void update_visibility(UIElement& element, bool show) {
    element.set_visibility(show ? Visibility::Visible : Visibility::Collapsed);
}

// 使用示例
bool has_data = view_model.has_data();
update_visibility(data_panel, has_data);
```

---

### 6. 加载占位符（Hidden）

```cpp
// 显示加载占位符（Hidden）
loading_indicator.set_visibility(Visibility::Visible);
content_panel.set_visibility(Visibility::Hidden);  // 占据空间但不显示

// 加载完成后切换
loading_indicator.set_visibility(Visibility::Collapsed);
content_panel.set_visibility(Visibility::Visible);
```

---

### 7. 动态菜单（Collapsed）

```cpp
// 默认折叠菜单
menu_panel.set_visibility(Visibility::Collapsed);

// 用户点击时展开
void on_menu_button_click() {
    if (menu_panel.visibility() == Visibility::Collapsed) {
        menu_panel.set_visibility(Visibility::Visible);
    } else {
        menu_panel.set_visibility(Visibility::Collapsed);
    }
}
```

---

## 最佳实践

### 1. 使用 Collapsed 释放布局空间

```cpp
// ✅ 推荐：使用 Collapsed 释放布局空间
element.set_visibility(Visibility::Collapsed);

// ❌ 不推荐：使用 Hidden 仍占据空间（浪费布局资源）
element.set_visibility(Visibility::Hidden);
```

---

### 2. 使用 Hidden 保持布局稳定

```cpp
// ✅ 推荐：使用 Hidden 保持布局稳定（避免其他元素跳动）
loading_indicator.set_visibility(Visibility::Visible);
content_panel.set_visibility(Visibility::Hidden);  // 占据空间，避免布局跳动

// ❌ 不推荐：使用 Collapsed 导致布局跳动
content_panel.set_visibility(Visibility::Collapsed);  // 其他元素位置重排
```

---

### 3. 默认值使用 Visible

```cpp
// ✅ 推荐：默认值使用 Visible（数值为 0）
Visibility visibility = Visibility::Visible;

// ✅ 推荐：零初始化自动得到 Visible
Visibility visibility{};  // = Visibility::Visible
```

---

### 4. 使用枚举类型（而非整数）

```cpp
// ✅ 推荐：使用枚举类型
element.set_visibility(Visibility::Collapsed);

// ❌ 不推荐：使用整数（类型不安全）
element.set_visibility(2);  // ❌ 编译错误（C++ enum class 不允许隐式转换）
```

---

## 常见陷阱

### 1. 混淆 Hidden 和 Collapsed

```cpp
// ❌ 问题：期望释放空间，却使用了 Hidden
element.set_visibility(Visibility::Hidden);
// ❌ 元素仍占据空间，其他元素位置不变

// ✅ 解决：使用 Collapsed 释放空间
element.set_visibility(Visibility::Collapsed);
// ✅ 元素不占据空间，其他元素重排
```

---

### 2. 期望 Hidden 不占据空间

```cpp
// ❌ 问题：期望 Hidden 不占据空间
element.set_visibility(Visibility::Hidden);
// ❌ Hidden 仍占据空间（与 CSS visibility: hidden 一致）

// ✅ 解决：使用 Collapsed 不占据空间
element.set_visibility(Visibility::Collapsed);
```

---

### 3. 频繁切换 Collapsed 导致布局抖动

```cpp
// ❌ 问题：频繁切换 Collapsed 导致布局重排（性能问题）
for (int i = 0; i < 100; ++i) {
    element.set_visibility(i % 2 == 0 ? Visibility::Visible : Visibility::Collapsed);
    // ❌ 每次切换都触发布局重排
}

// ✅ 解决：使用 Hidden 保持布局稳定
for (int i = 0; i < 100; ++i) {
    element.set_visibility(i % 2 == 0 ? Visibility::Visible : Visibility::Hidden);
    // ✅ 不触发布局重排，仅切换渲染
}
```

---

### 4. 尝试将整数隐式转换为 Visibility

```cpp
// ❌ 问题：尝试将整数隐式转换为 Visibility
Visibility v = 0;  // ❌ 编译错误（enum class 不允许隐式转换）

// ✅ 解决：使用枚举值
Visibility v = Visibility::Visible;

// ✅ 解决：使用显式转换（不推荐）
Visibility v = static_cast<Visibility>(0);
```

---

## 完整示例

```cpp
#include <mine/ui/visual/Visibility.h>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 模拟 UIElement（简化版）
// ────────────────────────────────────────────────────────────────────────────

class UIElement {
public:
    void set_visibility(Visibility v) {
        if (visibility_ != v) {
            visibility_ = v;
            std::cout << "Visibility changed to: ";
            switch (visibility_) {
                case Visibility::Visible:
                    std::cout << "Visible" << std::endl;
                    break;
                case Visibility::Hidden:
                    std::cout << "Hidden" << std::endl;
                    break;
                case Visibility::Collapsed:
                    std::cout << "Collapsed" << std::endl;
                    break;
            }
        }
    }

    [[nodiscard]] Visibility visibility() const noexcept {
        return visibility_;
    }

    [[nodiscard]] bool is_visible() const noexcept {
        return visibility_ == Visibility::Visible;
    }

    [[nodiscard]] bool participates_in_layout() const noexcept {
        return visibility_ != Visibility::Collapsed;
    }

private:
    Visibility visibility_ = Visibility::Visible;
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    UIElement element;
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 默认可见
    // ════════════════════════════════════════════════════════════════════════
    std::cout << "初始状态:" << std::endl;
    std::cout << "  is_visible: " << (element.is_visible() ? "Yes" : "No") << std::endl;
    std::cout << "  participates_in_layout: " << (element.participates_in_layout() ? "Yes" : "No") << std::endl;
    // 输出：
    //   is_visible: Yes
    //   participates_in_layout: Yes
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 隐藏元素（占据空间但不渲染）
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "隐藏元素（Hidden）:" << std::endl;
    element.set_visibility(Visibility::Hidden);
    std::cout << "  is_visible: " << (element.is_visible() ? "Yes" : "No") << std::endl;
    std::cout << "  participates_in_layout: " << (element.participates_in_layout() ? "Yes" : "No") << std::endl;
    // 输出：
    //   Visibility changed to: Hidden
    //   is_visible: No
    //   participates_in_layout: Yes
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 折叠元素（不占据空间也不渲染）
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "折叠元素（Collapsed）:" << std::endl;
    element.set_visibility(Visibility::Collapsed);
    std::cout << "  is_visible: " << (element.is_visible() ? "Yes" : "No") << std::endl;
    std::cout << "  participates_in_layout: " << (element.participates_in_layout() ? "Yes" : "No") << std::endl;
    // 输出：
    //   Visibility changed to: Collapsed
    //   is_visible: No
    //   participates_in_layout: No
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 恢复可见
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "恢复可见（Visible）:" << std::endl;
    element.set_visibility(Visibility::Visible);
    std::cout << "  is_visible: " << (element.is_visible() ? "Yes" : "No") << std::endl;
    std::cout << "  participates_in_layout: " << (element.participates_in_layout() ? "Yes" : "No") << std::endl;
    // 输出：
    //   Visibility changed to: Visible
    //   is_visible: Yes
    //   participates_in_layout: Yes
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 切换可见性（Toggle）
    // ════════════════════════════════════════════════════════════════════════
    std::cout << std::endl << "切换可见性（Toggle）:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        Visibility current = element.visibility();
        Visibility next = (current == Visibility::Visible) ? Visibility::Collapsed : Visibility::Visible;
        element.set_visibility(next);
    }
    // 输出：
    //   Visibility changed to: Collapsed
    //   Visibility changed to: Visible
    //   Visibility changed to: Collapsed
    
    return 0;
}
```

---

## 总结

`Visibility` 是 `mine.ui.visual` 模块的元素可见性枚举，具备：

1. **三种状态**：Visible（可见）、Hidden（隐藏）、Collapsed（折叠）
2. **WPF 语义**：与 WPF/Avalonia 保持一致
3. **布局影响**：Collapsed 不占空间，Hidden 占据空间但不渲染
4. **渲染影响**：Visible 正常渲染，Hidden/Collapsed 不渲染
5. **uint8_t 存储**：节省内存（仅占 1 字节）

**使用建议**：
- 使用 **Collapsed** 释放布局空间（类似 `display: none`）
- 使用 **Hidden** 保持布局稳定（类似 `visibility: hidden`）
- 默认值使用 **Visible**（数值为 0）
- 使用枚举类型（而非整数）
- 避免频繁切换 Collapsed（触发布局重排）
- 理解 Hidden 和 Collapsed 的区别（占据空间 vs 不占据空间）
