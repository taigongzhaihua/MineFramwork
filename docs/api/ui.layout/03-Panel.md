# Panel 详细接口文档

## 概述

`Panel` 是 `mine.ui.layout` 模块的**布局面板基类**，管理 UIElement 子元素集合。

**核心特性：**
- **子元素管理**：add_child、remove_child、child_at、children_count
- **不拥有生命周期**：存储裸指针，不负责子元素的创建和销毁
- **视觉树集成**：添加/移除子元素时自动同步到视觉树
- **布局抽象**：子类通过 measure_override/arrange_override 实现具体布局算法

**继承关系：**
```
Visual (mine.ui.visual)
    └─ UIElement
        └─ FrameworkElement
            └─ Panel
                ├─ StackPanel
                └─ Grid
```

**使用场景：**
- **StackPanel**：线性堆叠布局
- **Grid**：网格布局
- **自定义面板**：继承 Panel 实现自定义布局算法

---

## 文件位置

```
src/mine/ui/layout/api/include/mine/ui/layout/Panel.h
```

---

## 类定义

```cpp
namespace mine::ui {

/**
 * @brief 布局面板基类。
 *
 * Panel 提供子元素管理（add/remove/at/count），子类负责在
 * measure_override / arrange_override 中实现具体布局算法。
 *
 * 注意事项：
 *   - Panel 不拥有子元素的生命周期（存储裸指针）
 *   - 子元素析构时应先从 Panel 中移除（或确保 Panel 生命周期短于子元素）
 *   - 子元素要求是 UIElement 的实例（或子类）
 */
class MINE_UI_LAYOUT_API Panel : public FrameworkElement {
public:
    // ── 生命周期 ──────────────────────────────────────────────────────────

    Panel();
    ~Panel() override;

    Panel(const Panel&)            = delete;
    Panel& operator=(const Panel&) = delete;
    Panel(Panel&&)                 = default;
    Panel& operator=(Panel&&)      = default;

    // ── 子元素管理 ────────────────────────────────────────────────────────

    /**
     * @brief 向面板末尾添加子元素。
     *
     * 同时调用 Visual::add_visual_child() 将其纳入视觉树。
     * 添加后触发 invalidate_measure()。
     *
     * @param child 要添加的子元素（非空，不重复添加）
     */
    void add_child(UIElement* child);

    /**
     * @brief 从面板移除子元素。
     *
     * 同时调用 Visual::remove_visual_child() 从视觉树移除。
     * 移除后触发 invalidate_measure()。
     *
     * @param child 要移除的子元素；若不存在则忽略
     */
    void remove_child(UIElement* child);

    /**
     * @brief 按索引获取子元素。
     *
     * @param index 子元素索引（范围：[0, child_count())）
     * @return      对应子元素指针
     */
    [[nodiscard]] UIElement* child_at(uint32_t index) const noexcept;

    /// 子元素数量
    [[nodiscard]] uint32_t children_count() const noexcept;

protected:
    /// 子元素指针列表（非拥有，裸指针）
    containers::SmallVector<UIElement*, 8> children_;
};

} // namespace mine::ui
```

---

## 成员方法

### 构造函数

```cpp
Panel();
```

**描述**：创建空面板（无子元素）。

**示例**：
```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;

Panel* panel = new Panel();
assert(panel->children_count() == 0);
```

---

### 析构函数

```cpp
~Panel() override;
```

**描述**：销毁面板。注意：不会销毁子元素（裸指针）。

**示例**：
```cpp
Panel* panel = new Panel();
Button* btn = new Button();
panel->add_child(btn);

delete panel;  // ❌ btn 未被销毁，需手动 delete btn
```

---

### add_child

```cpp
void add_child(UIElement* child);
```

**描述**：向面板末尾添加子元素。

**参数**：
- `child`：要添加的子元素指针（非空，不重复添加）

**行为**：
1. 将 child 添加到 `children_` 末尾
2. 调用 `add_visual_child(child)` 纳入视觉树
3. 触发 `invalidate_measure()` 请求重新布局

**示例**：
```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Panel* panel = new Panel();
Button* btn1 = new Button();
Button* btn2 = new Button();

panel->add_child(btn1);
panel->add_child(btn2);

assert(panel->children_count() == 2);
assert(panel->child_at(0) == btn1);
assert(panel->child_at(1) == btn2);
```

---

### remove_child

```cpp
void remove_child(UIElement* child);
```

**描述**：从面板移除子元素。

**参数**：
- `child`：要移除的子元素指针（若不存在则忽略）

**行为**：
1. 从 `children_` 中移除 child（若存在）
2. 调用 `remove_visual_child(child)` 从视觉树移除
3. 触发 `invalidate_measure()` 请求重新布局

**示例**：
```cpp
Panel* panel = new Panel();
Button* btn1 = new Button();
Button* btn2 = new Button();

panel->add_child(btn1);
panel->add_child(btn2);
assert(panel->children_count() == 2);

panel->remove_child(btn1);
assert(panel->children_count() == 1);
assert(panel->child_at(0) == btn2);

// 移除不存在的子元素（忽略）
panel->remove_child(btn1);
assert(panel->children_count() == 1);
```

---

### child_at

```cpp
[[nodiscard]] UIElement* child_at(uint32_t index) const noexcept;
```

**描述**：按索引获取子元素。

**参数**：
- `index`：子元素索引（范围：[0, children_count())）

**返回值**：对应子元素指针。

**示例**：
```cpp
Panel* panel = new Panel();
Button* btn1 = new Button();
Button* btn2 = new Button();

panel->add_child(btn1);
panel->add_child(btn2);

assert(panel->child_at(0) == btn1);
assert(panel->child_at(1) == btn2);
```

---

### children_count

```cpp
[[nodiscard]] uint32_t children_count() const noexcept;
```

**描述**：获取子元素数量。

**返回值**：子元素数量。

**示例**：
```cpp
Panel* panel = new Panel();
assert(panel->children_count() == 0);

Button* btn = new Button();
panel->add_child(btn);
assert(panel->children_count() == 1);

panel->remove_child(btn);
assert(panel->children_count() == 0);
```

---

## 使用场景

### 1. 创建空面板

```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;

Panel* panel = new Panel();
assert(panel->children_count() == 0);
```

---

### 2. 添加多个子元素

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>

using namespace mine::ui;

Panel* panel = new Panel();
Button* btn1 = new Button();
Button* btn2 = new Button();
TextBlock* text = new TextBlock();

panel->add_child(btn1);
panel->add_child(btn2);
panel->add_child(text);

assert(panel->children_count() == 3);
```

---

### 3. 遍历子元素

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/visual/UIElement.h>

using namespace mine::ui;

Panel* panel = new Panel();
// ... 添加子元素 ...

// 遍历所有子元素
for (uint32_t i = 0; i < panel->children_count(); ++i) {
    UIElement* child = panel->child_at(i);
    // 对 child 进行操作
    child->invalidate_visual();
}
```

---

### 4. 移除指定子元素

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Panel* panel = new Panel();
Button* btn1 = new Button();
Button* btn2 = new Button();

panel->add_child(btn1);
panel->add_child(btn2);

// 移除第一个按钮
panel->remove_child(btn1);

assert(panel->children_count() == 1);
assert(panel->child_at(0) == btn2);
```

---

### 5. 清空所有子元素

```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;

Panel* panel = new Panel();
// ... 添加子元素 ...

// 清空所有子元素
while (panel->children_count() > 0) {
    panel->remove_child(panel->child_at(0));
}

assert(panel->children_count() == 0);
```

---

### 6. 子元素生命周期管理

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>
#include <memory>

using namespace mine::ui;

// 使用智能指针管理子元素生命周期
Panel* panel = new Panel();
std::unique_ptr<Button> btn_owner = std::make_unique<Button>();

// 添加到面板（传递裸指针）
panel->add_child(btn_owner.get());

// 移除时确保顺序正确
panel->remove_child(btn_owner.get());
btn_owner.reset();  // 安全销毁

// 或者在 panel 销毁前先销毁子元素
delete panel;
```

---

### 7. 自定义面板子类

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/math/Size.h>

using namespace mine::ui;
using namespace mine::math;

// 自定义水平居中面板
class CenterPanel : public Panel {
protected:
    Size measure_override(Size available) override {
        // 测量所有子元素
        Size max_size{0, 0};
        for (uint32_t i = 0; i < children_count(); ++i) {
            UIElement* child = child_at(i);
            child->measure(available);
            Size desired = child->desired_size();
            max_size.width = std::max(max_size.width, desired.width);
            max_size.height = std::max(max_size.height, desired.height);
        }
        return max_size;
    }

    Size arrange_override(Size final_size) override {
        // 将所有子元素居中排列（重叠）
        for (uint32_t i = 0; i < children_count(); ++i) {
            UIElement* child = child_at(i);
            Size desired = child->desired_size();
            
            // 计算居中位置
            float x = (final_size.width - desired.width) / 2.0f;
            float y = (final_size.height - desired.height) / 2.0f;
            
            Rect slot{x, y, desired.width, desired.height};
            child->arrange(slot);
        }
        return final_size;
    }
};
```

---

### 8. 动态子元素管理

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>
#include <vector>

using namespace mine::ui;

Panel* panel = new Panel();
std::vector<std::unique_ptr<Button>> buttons;

// 动态添加 10 个按钮
for (int i = 0; i < 10; ++i) {
    auto btn = std::make_unique<Button>();
    panel->add_child(btn.get());
    buttons.push_back(std::move(btn));
}

assert(panel->children_count() == 10);

// 移除偶数索引的按钮
for (int i = 0; i < 10; i += 2) {
    panel->remove_child(buttons[i].get());
}

assert(panel->children_count() == 5);
```

---

## 最佳实践

### ✅ 推荐：使用智能指针管理子元素生命周期

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>
#include <memory>
#include <vector>

using namespace mine::ui;

class MyWindow {
    Panel* panel_;
    std::vector<std::unique_ptr<UIElement>> children_;

public:
    MyWindow() : panel_(new Panel()) {}

    void add_button() {
        auto btn = std::make_unique<Button>();
        panel_->add_child(btn.get());
        children_.push_back(std::move(btn));
    }

    ~MyWindow() {
        // panel_ 销毁前，children_ 中的智能指针会自动清理子元素
        delete panel_;
    }
};
```

**理由**：
- Panel 不拥有子元素生命周期，需要外部管理
- 智能指针自动处理析构顺序，避免悬垂指针

---

### ❌ 不推荐：直接 delete panel 而不清理子元素

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Panel* panel = new Panel();
Button* btn = new Button();
panel->add_child(btn);

delete panel;  // ❌ btn 未被销毁，内存泄漏
// 正确做法：
// panel->remove_child(btn);
// delete btn;
// delete panel;
```

**问题**：内存泄漏，子元素未被销毁。

---

### ✅ 推荐：移除子元素后再销毁

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Panel* panel = new Panel();
Button* btn = new Button();
panel->add_child(btn);

// 移除后再销毁
panel->remove_child(btn);
delete btn;
delete panel;
```

**理由**：确保子元素在 Panel 销毁前被正确清理。

---

### ❌ 不推荐：重复添加同一子元素

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Panel* panel = new Panel();
Button* btn = new Button();

panel->add_child(btn);
panel->add_child(btn);  // ❌ 重复添加

assert(panel->children_count() == 2);  // 同一按钮出现两次
```

**问题**：同一子元素在 Panel 中出现多次，导致布局和渲染异常。

---

### ✅ 推荐：添加前检查是否已存在

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

bool contains(Panel* panel, UIElement* child) {
    for (uint32_t i = 0; i < panel->children_count(); ++i) {
        if (panel->child_at(i) == child) {
            return true;
        }
    }
    return false;
}

Panel* panel = new Panel();
Button* btn = new Button();

if (!contains(panel, btn)) {
    panel->add_child(btn);
}
```

**理由**：避免重复添加导致的异常行为。

---

### ✅ 推荐：在子类中实现布局算法

```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;
using namespace mine::math;

class CustomPanel : public Panel {
protected:
    Size measure_override(Size available) override {
        // 实现自定义测量逻辑
        Size desired{0, 0};
        for (uint32_t i = 0; i < children_count(); ++i) {
            child_at(i)->measure(available);
            // 累加或取最大值...
        }
        return desired;
    }

    Size arrange_override(Size final_size) override {
        // 实现自定义排列逻辑
        for (uint32_t i = 0; i < children_count(); ++i) {
            Rect slot = /* 计算子元素位置 */;
            child_at(i)->arrange(slot);
        }
        return final_size;
    }
};
```

**理由**：Panel 是抽象基类，具体布局算法由子类实现。

---

### ❌ 不推荐：在应用代码中直接使用 Panel

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

// ❌ Panel 没有实现 measure_override / arrange_override
// 所有子元素会重叠在 (0, 0) 位置
Panel* panel = new Panel();
Button* btn1 = new Button();
Button* btn2 = new Button();
panel->add_child(btn1);
panel->add_child(btn2);
```

**问题**：Panel 基类没有布局逻辑，子元素会重叠。

---

### ✅ 推荐：使用 StackPanel 或 Grid 等具体面板

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

// ✅ 使用 StackPanel 实现垂直排列
StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn1 = new Button();
Button* btn2 = new Button();
panel->add_child(btn1);
panel->add_child(btn2);
```

**理由**：具体面板类实现了完整的布局算法。

---

## 常见陷阱

### ❌ 陷阱 1：子元素生命周期管理错误

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Panel* create_panel() {
    Panel* panel = new Panel();
    Button btn;  // ❌ 局部变量，函数返回后销毁
    panel->add_child(&btn);
    return panel;
}

// 使用时 panel 中的子元素指针已悬垂
Panel* panel = create_panel();
```

**问题**：子元素是局部变量，函数返回后被销毁，Panel 中持有悬垂指针。

**✅ 解决方案**：使用堆分配或智能指针

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>
#include <memory>

using namespace mine::ui;

struct PanelWithChildren {
    std::unique_ptr<Panel> panel;
    std::vector<std::unique_ptr<UIElement>> children;
};

PanelWithChildren create_panel() {
    PanelWithChildren result;
    result.panel = std::make_unique<Panel>();
    
    auto btn = std::make_unique<Button>();
    result.panel->add_child(btn.get());
    result.children.push_back(std::move(btn));
    
    return result;
}
```

---

### ❌ 陷阱 2：遍历时修改子元素列表

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Panel* panel = new Panel();
// ... 添加子元素 ...

// ❌ 遍历时移除子元素
for (uint32_t i = 0; i < panel->children_count(); ++i) {
    UIElement* child = panel->child_at(i);
    if (/* 某条件 */) {
        panel->remove_child(child);  // ❌ 索引失效
    }
}
```

**问题**：移除元素后，后续元素索引发生变化，导致遍历错误。

**✅ 解决方案**：逆向遍历或收集待移除元素

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/visual/UIElement.h>
#include <vector>

using namespace mine::ui;

Panel* panel = new Panel();
// ... 添加子元素 ...

// 方案 1：逆向遍历
for (int i = static_cast<int>(panel->children_count()) - 1; i >= 0; --i) {
    UIElement* child = panel->child_at(static_cast<uint32_t>(i));
    if (/* 某条件 */) {
        panel->remove_child(child);
    }
}

// 方案 2：收集待移除元素
std::vector<UIElement*> to_remove;
for (uint32_t i = 0; i < panel->children_count(); ++i) {
    UIElement* child = panel->child_at(i);
    if (/* 某条件 */) {
        to_remove.push_back(child);
    }
}
for (UIElement* child : to_remove) {
    panel->remove_child(child);
}
```

---

### ❌ 陷阱 3：忘记实现 measure_override / arrange_override

```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;
using namespace mine::math;

class MyPanel : public Panel {
    // ❌ 未实现 measure_override / arrange_override
};

MyPanel* panel = new MyPanel();
// 子元素会重叠在 (0, 0) 位置
```

**问题**：Panel 基类的 measure_override / arrange_override 不实现任何布局逻辑。

**✅ 解决方案**：覆盖布局方法

```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;
using namespace mine::math;

class MyPanel : public Panel {
protected:
    Size measure_override(Size available) override {
        // 实现测量逻辑
        Size desired{0, 0};
        for (uint32_t i = 0; i < children_count(); ++i) {
            UIElement* child = child_at(i);
            child->measure(available);
            Size child_desired = child->desired_size();
            desired.width = std::max(desired.width, child_desired.width);
            desired.height += child_desired.height;
        }
        return desired;
    }

    Size arrange_override(Size final_size) override {
        // 实现排列逻辑
        float y = 0.0f;
        for (uint32_t i = 0; i < children_count(); ++i) {
            UIElement* child = child_at(i);
            Size desired = child->desired_size();
            Rect slot{0, y, final_size.width, desired.height};
            child->arrange(slot);
            y += desired.height;
        }
        return final_size;
    }
};
```

---

### ❌ 陷阱 4：子元素索引越界

```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;

Panel* panel = new Panel();
// ... 添加 3 个子元素 ...

// ❌ 索引越界（未检查）
UIElement* child = panel->child_at(10);  // 崩溃或未定义行为
```

**问题**：访问超出范围的索引导致未定义行为。

**✅ 解决方案**：检查索引范围

```cpp
#include <mine/ui/layout/Panel.h>

using namespace mine::ui;

Panel* panel = new Panel();
// ... 添加子元素 ...

uint32_t index = 10;
if (index < panel->children_count()) {
    UIElement* child = panel->child_at(index);
    // 安全访问
}
```

---

## 完整示例

以下示例演示如何继承 Panel 实现自定义的瀑布流布局面板（WaterfallPanel）。

```cpp
#include <mine/ui/layout/Panel.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/math/Size.h>
#include <mine/math/Rect.h>
#include <vector>
#include <algorithm>

using namespace mine::ui;
using namespace mine::math;

/**
 * @brief 瀑布流布局面板（类似 Pinterest）
 *
 * 将子元素排列成多列，每个子元素添加到当前高度最小的列中。
 */
class WaterfallPanel : public Panel {
public:
    WaterfallPanel() : columns_(2) {}

    // 设置列数
    void set_columns(uint32_t cols) {
        if (cols > 0 && cols != columns_) {
            columns_ = cols;
            invalidate_measure();
        }
    }

    [[nodiscard]] uint32_t columns() const noexcept {
        return columns_;
    }

protected:
    Size measure_override(Size available) override {
        if (children_count() == 0) {
            return Size{0, 0};
        }

        // 计算每列宽度
        float col_width = available.width / static_cast<float>(columns_);

        // 测量所有子元素
        std::vector<float> col_heights(columns_, 0.0f);
        for (uint32_t i = 0; i < children_count(); ++i) {
            UIElement* child = child_at(i);
            
            // 子元素可用宽度 = 列宽
            child->measure(Size{col_width, std::numeric_limits<float>::infinity()});
            Size desired = child->desired_size();

            // 添加到当前最短的列
            uint32_t shortest_col = 0;
            float min_height = col_heights[0];
            for (uint32_t c = 1; c < columns_; ++c) {
                if (col_heights[c] < min_height) {
                    min_height = col_heights[c];
                    shortest_col = c;
                }
            }
            col_heights[shortest_col] += desired.height;
        }

        // 期望尺寸 = 面板宽度 × 最高列的高度
        float max_height = *std::max_element(col_heights.begin(), col_heights.end());
        return Size{available.width, max_height};
    }

    Size arrange_override(Size final_size) override {
        if (children_count() == 0) {
            return final_size;
        }

        // 计算每列宽度
        float col_width = final_size.width / static_cast<float>(columns_);

        // 跟踪每列当前高度
        std::vector<float> col_heights(columns_, 0.0f);

        // 排列所有子元素
        for (uint32_t i = 0; i < children_count(); ++i) {
            UIElement* child = child_at(i);
            Size desired = child->desired_size();

            // 找到当前最短的列
            uint32_t shortest_col = 0;
            float min_height = col_heights[0];
            for (uint32_t c = 1; c < columns_; ++c) {
                if (col_heights[c] < min_height) {
                    min_height = col_heights[c];
                    shortest_col = c;
                }
            }

            // 在最短列中排列子元素
            float x = col_width * static_cast<float>(shortest_col);
            float y = col_heights[shortest_col];
            Rect slot{x, y, col_width, desired.height};
            child->arrange(slot);

            // 更新列高度
            col_heights[shortest_col] += desired.height;
        }

        return final_size;
    }

private:
    uint32_t columns_;  // 列数
};

// ═════════════════════════════════════════════════════════════════════════
// 使用示例
// ═════════════════════════════════════════════════════════════════════════

#include <memory>
#include <iostream>

class WaterfallDemo {
public:
    WaterfallDemo() {
        panel_ = std::make_unique<WaterfallPanel>();
        panel_->set_columns(3);  // 3 列瀑布流
    }

    void add_card(const std::string& title, float height) {
        // 创建卡片（TextBlock 模拟）
        auto card = std::make_unique<TextBlock>();
        card->set_text(title);
        card->set_height(height);  // 不同高度的卡片
        
        panel_->add_child(card.get());
        cards_.push_back(std::move(card));
    }

    void layout() {
        // 测量和排列
        Size available{900, std::numeric_limits<float>::infinity()};
        panel_->measure(available);
        
        Size desired = panel_->desired_size();
        Rect final_rect{0, 0, 900, desired.height};
        panel_->arrange(final_rect);

        // 打印布局结果
        std::cout << "瀑布流布局结果：\n";
        std::cout << "面板尺寸：" << desired.width << " × " << desired.height << "\n\n";

        for (uint32_t i = 0; i < panel_->children_count(); ++i) {
            UIElement* child = panel_->child_at(i);
            Rect bounds = child->render_bounds();
            std::cout << "子元素 " << i << "：";
            std::cout << "x=" << bounds.x << ", ";
            std::cout << "y=" << bounds.y << ", ";
            std::cout << "w=" << bounds.width << ", ";
            std::cout << "h=" << bounds.height << "\n";
        }
    }

    void clear() {
        while (panel_->children_count() > 0) {
            panel_->remove_child(panel_->child_at(0));
        }
        cards_.clear();
    }

private:
    std::unique_ptr<WaterfallPanel> panel_;
    std::vector<std::unique_ptr<UIElement>> cards_;
};

int main() {
    WaterfallDemo demo;

    // 添加不同高度的卡片
    demo.add_card("Card 1", 150.0f);
    demo.add_card("Card 2", 200.0f);
    demo.add_card("Card 3", 100.0f);
    demo.add_card("Card 4", 250.0f);
    demo.add_card("Card 5", 180.0f);
    demo.add_card("Card 6", 120.0f);
    demo.add_card("Card 7", 220.0f);
    demo.add_card("Card 8", 160.0f);
    demo.add_card("Card 9", 190.0f);

    // 执行布局
    demo.layout();

    /*
     * 预期输出：
     *
     * 瀑布流布局结果：
     * 面板尺寸：900 × 620
     *
     * 子元素 0：x=0, y=0, w=300, h=150
     * 子元素 1：x=300, y=0, w=300, h=200
     * 子元素 2：x=600, y=0, w=300, h=100
     * 子元素 3：x=600, y=100, w=300, h=250
     * 子元素 4：x=0, y=150, w=300, h=180
     * 子元素 5：x=300, y=200, w=300, h=120
     * 子元素 6：x=300, y=320, w=300, h=220
     * 子元素 7：x=0, y=330, w=300, h=160
     * 子元素 8：x=600, y=350, w=300, h=190
     *
     * （子元素自动分配到当前最短的列，形成瀑布流效果）
     */

    return 0;
}
```

**示例说明**：
1. **WaterfallPanel**：继承 Panel，实现瀑布流布局
2. **measure_override**：测量所有子元素，计算每列高度
3. **arrange_override**：将子元素排列到当前最短的列中
4. **WaterfallDemo**：演示如何使用 WaterfallPanel
5. **输出**：显示每个子元素的最终位置和尺寸

---

## 总结

**核心要点**：
1. **Panel 是布局面板基类**，管理 UIElement 子元素集合。
2. **不拥有子元素生命周期**，存储裸指针，需要外部管理。
3. **提供 add_child / remove_child / child_at / children_count 方法**。
4. **子类通过 measure_override / arrange_override 实现具体布局算法**。
5. **添加/移除子元素时自动同步到视觉树**（add_visual_child / remove_visual_child）。
6. **应使用智能指针管理子元素生命周期**，避免悬垂指针和内存泄漏。
7. **遍历时修改子元素列表需要特殊处理**（逆向遍历或收集待移除元素）。

**使用建议**：
- ✅ 使用 StackPanel / Grid 等具体面板类，而非直接使用 Panel
- ✅ 继承 Panel 实现自定义布局算法
- ✅ 使用智能指针管理子元素生命周期
- ❌ 不要重复添加同一子元素
- ❌ 不要在遍历时修改子元素列表
- ❌ 不要忘记实现 measure_override / arrange_override
