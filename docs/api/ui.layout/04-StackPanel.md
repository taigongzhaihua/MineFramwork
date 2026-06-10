# StackPanel 详细接口文档

## 概述

`StackPanel` 是 `mine.ui.layout` 模块的**线性堆叠布局面板**。

**核心特性：**
- **线性排列**：沿指定方向（Horizontal/Vertical）依次排列子元素
- **自动布局**：主轴方向累加尺寸，交叉轴取最大值
- **方向可配置**：通过 OrientationProperty 依赖属性切换水平/垂直
- **无限主轴空间**：子元素在主轴方向不受约束（传入 INFINITY）

**继承关系：**
```
Visual (mine.ui.visual)
    └─ UIElement
        └─ FrameworkElement
            └─ Panel
                └─ StackPanel
```

**布局算法：**
- **Vertical（垂直，默认）**：子元素自上而下排列
  - 主轴（垂直）：desiredHeight = sum(children desiredHeight)
  - 交叉轴（水平）：desiredWidth = max(children desiredWidth)
  
- **Horizontal（水平）**：子元素从左到右排列
  - 主轴（水平）：desiredWidth = sum(children desiredWidth)
  - 交叉轴（垂直）：desiredHeight = max(children desiredHeight)

---

## 文件位置

```
src/mine/ui/layout/api/include/mine/ui/layout/StackPanel.h
```

---

## 类定义

```cpp
namespace mine::ui {

/**
 * @brief 线性堆叠布局面板。
 *
 * Measure 算法：
 *   - Vertical：传给每个子元素的 available = {panelWidth, INFINITY}
 *     desiredWidth  = max(children desiredWidth)
 *     desiredHeight = sum(children desiredHeight)
 *   - Horizontal：传给每个子元素的 available = {INFINITY, panelHeight}
 *     desiredWidth  = sum(children desiredWidth)
 *     desiredHeight = max(children desiredHeight)
 *
 * Arrange 算法：
 *   - 沿主轴方向累计偏移，逐一调用 child->arrange(slot)
 *   - slot 的交叉轴尺寸 = 面板分配的交叉轴尺寸（允许 Stretch）
 *   - slot 的主轴尺寸   = child->desired_size() 在主轴的分量
 */
class MINE_UI_LAYOUT_API StackPanel : public Panel {
public:
    // ── 依赖属性 ──────────────────────────────────────────────────────────

    /// 排列方向（Orientation::Horizontal / Orientation::Vertical）
    static const DependencyProperty& OrientationProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    StackPanel();
    ~StackPanel() override;

    StackPanel(const StackPanel&)            = delete;
    StackPanel& operator=(const StackPanel&) = delete;
    StackPanel(StackPanel&&)                 = default;
    StackPanel& operator=(StackPanel&&)      = default;

    // ── 属性访问 ──────────────────────────────────────────────────────────

    [[nodiscard]] Orientation orientation() const noexcept;
    void set_orientation(Orientation o);

protected:
    // ── 布局实现 ──────────────────────────────────────────────────────────

    math::Size measure_override(math::Size available) override;
    math::Size arrange_override(math::Size final_size) override;
};

} // namespace mine::ui
```

---

## 成员方法

### 构造函数

```cpp
StackPanel();
```

**描述**：创建垂直 StackPanel（默认方向为 Vertical）。

**示例**：
```cpp
#include <mine/ui/layout/StackPanel.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
assert(panel->orientation() == Orientation::Vertical);
```

---

### 析构函数

```cpp
~StackPanel() override;
```

**描述**：销毁 StackPanel。注意：不会销毁子元素（继承自 Panel）。

---

### orientation

```cpp
[[nodiscard]] Orientation orientation() const noexcept;
```

**描述**：获取当前排列方向。

**返回值**：
- `Orientation::Horizontal`：水平排列
- `Orientation::Vertical`：垂直排列

**示例**：
```cpp
#include <mine/ui/layout/StackPanel.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
Orientation dir = panel->orientation();
assert(dir == Orientation::Vertical);  // 默认垂直
```

---

### set_orientation

```cpp
void set_orientation(Orientation o);
```

**描述**：设置排列方向。

**参数**：
- `o`：排列方向（Horizontal / Vertical）

**行为**：
- 修改 OrientationProperty 依赖属性值
- 触发 invalidate_measure() 请求重新布局

**示例**：
```cpp
#include <mine/ui/layout/StackPanel.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Horizontal);  // 切换为水平排列
assert(panel->orientation() == Orientation::Horizontal);
```

---

### measure_override

```cpp
math::Size measure_override(math::Size available) override;
```

**描述**：测量 StackPanel 及其子元素的期望尺寸。

**参数**：
- `available`：父容器提供的可用尺寸

**返回值**：StackPanel 的期望尺寸。

**算法**：
- **Vertical（垂直）**：
  1. 传给每个子元素的 available = {panelWidth, INFINITY}
  2. desiredWidth = max(children desiredWidth)
  3. desiredHeight = sum(children desiredHeight)

- **Horizontal（水平）**：
  1. 传给每个子元素的 available = {INFINITY, panelHeight}
  2. desiredWidth = sum(children desiredWidth)
  3. desiredHeight = max(children desiredHeight)

**示例**：
```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;
using namespace mine::math;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn1 = new Button();
btn1->set_width(100);
btn1->set_height(50);
Button* btn2 = new Button();
btn2->set_width(150);
btn2->set_height(30);

panel->add_child(btn1);
panel->add_child(btn2);

// 测量
Size available{200, 300};
panel->measure(available);

Size desired = panel->desired_size();
// desiredWidth  = max(100, 150) = 150
// desiredHeight = 50 + 30 = 80
assert(desired.width == 150);
assert(desired.height == 80);
```

---

### arrange_override

```cpp
math::Size arrange_override(math::Size final_size) override;
```

**描述**：排列 StackPanel 的子元素。

**参数**：
- `final_size`：父容器分配给 StackPanel 的最终尺寸

**返回值**：实际使用的尺寸（通常返回 final_size）。

**算法**：
- 沿主轴方向累计偏移
- slot 的交叉轴尺寸 = final_size 的交叉轴分量
- slot 的主轴尺寸 = child->desired_size() 的主轴分量
- 逐一调用 child->arrange(slot)

**示例**：
```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;
using namespace mine::math;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn1 = new Button();
btn1->set_height(50);
Button* btn2 = new Button();
btn2->set_height(30);

panel->add_child(btn1);
panel->add_child(btn2);

// 测量和排列
panel->measure({200, 300});
panel->arrange({0, 0, 200, 300});

// btn1: x=0, y=0, width=200, height=50
// btn2: x=0, y=50, width=200, height=30
Rect bounds1 = btn1->render_bounds();
assert(bounds1.y == 0);
assert(bounds1.height == 50);

Rect bounds2 = btn2->render_bounds();
assert(bounds2.y == 50);
assert(bounds2.height == 30);
```

---

## 使用场景

### 1. 垂直排列按钮列表

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);  // 默认垂直，可省略

Button* btn1 = new Button();
btn1->set_text("按钮 1");
Button* btn2 = new Button();
btn2->set_text("按钮 2");
Button* btn3 = new Button();
btn3->set_text("按钮 3");

panel->add_child(btn1);
panel->add_child(btn2);
panel->add_child(btn3);

// 按钮自上而下排列
```

---

### 2. 水平排列工具栏

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* toolbar = new StackPanel();
toolbar->set_orientation(Orientation::Horizontal);

Button* btn_new = new Button();
btn_new->set_text("新建");
Button* btn_open = new Button();
btn_open->set_text("打开");
Button* btn_save = new Button();
btn_save->set_text("保存");

toolbar->add_child(btn_new);
toolbar->add_child(btn_open);
toolbar->add_child(btn_save);

// 按钮从左到右排列
```

---

### 3. 垂直表单布局

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/TextBox.h>

using namespace mine::ui;

StackPanel* form = new StackPanel();
form->set_orientation(Orientation::Vertical);

TextBlock* label1 = new TextBlock();
label1->set_text("用户名：");
TextBox* input1 = new TextBox();

TextBlock* label2 = new TextBlock();
label2->set_text("密码：");
TextBox* input2 = new TextBox();
input2->set_is_password(true);

form->add_child(label1);
form->add_child(input1);
form->add_child(label2);
form->add_child(input2);

// 表单字段自上而下排列
```

---

### 4. 水平导航栏

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* navbar = new StackPanel();
navbar->set_orientation(Orientation::Horizontal);

Button* btn_home = new Button();
btn_home->set_text("首页");
Button* btn_products = new Button();
btn_products->set_text("产品");
Button* btn_about = new Button();
btn_about->set_text("关于");

navbar->add_child(btn_home);
navbar->add_child(btn_products);
navbar->add_child(btn_about);

// 导航按钮从左到右排列
```

---

### 5. 嵌套 StackPanel（垂直 + 水平）

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

// 外层：垂直 StackPanel
StackPanel* outer = new StackPanel();
outer->set_orientation(Orientation::Vertical);

TextBlock* title = new TextBlock();
title->set_text("欢迎使用 MineUI");

// 内层：水平 StackPanel（按钮行）
StackPanel* button_row = new StackPanel();
button_row->set_orientation(Orientation::Horizontal);

Button* btn_ok = new Button();
btn_ok->set_text("确定");
Button* btn_cancel = new Button();
btn_cancel->set_text("取消");

button_row->add_child(btn_ok);
button_row->add_child(btn_cancel);

// 组合
outer->add_child(title);
outer->add_child(button_row);

// 结果：
// 标题（垂直排列）
// [确定] [取消]（水平排列）
```

---

### 6. 动态切换方向

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn1 = new Button();
Button* btn2 = new Button();
panel->add_child(btn1);
panel->add_child(btn2);

// 切换为水平排列
panel->set_orientation(Orientation::Horizontal);
// 布局会自动更新（触发 invalidate_measure）
```

---

### 7. 响应式布局（根据窗口宽度切换方向）

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

class ResponsivePanel : public StackPanel {
protected:
    math::Size measure_override(math::Size available) override {
        // 窗口宽度 < 600 时垂直排列，否则水平排列
        if (available.width < 600) {
            set_orientation(Orientation::Vertical);
        } else {
            set_orientation(Orientation::Horizontal);
        }
        return StackPanel::measure_override(available);
    }
};

ResponsivePanel* panel = new ResponsivePanel();
Button* btn1 = new Button();
Button* btn2 = new Button();
panel->add_child(btn1);
panel->add_child(btn2);

// 窗口宽度变化时自动切换排列方向
```

---

### 8. 侧边栏布局

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Separator.h>

using namespace mine::ui;

StackPanel* sidebar = new StackPanel();
sidebar->set_orientation(Orientation::Vertical);
sidebar->set_width(200);  // 固定宽度

Button* btn_dashboard = new Button();
btn_dashboard->set_text("仪表盘");
Button* btn_projects = new Button();
btn_projects->set_text("项目");
Separator* sep = new Separator();
Button* btn_settings = new Button();
btn_settings->set_text("设置");

sidebar->add_child(btn_dashboard);
sidebar->add_child(btn_projects);
sidebar->add_child(sep);
sidebar->add_child(btn_settings);

// 侧边栏垂直排列，分隔线分组
```

---

## 最佳实践

### ✅ 推荐：使用 set_orientation 切换方向

```cpp
#include <mine/ui/layout/StackPanel.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Horizontal);  // 水平排列
```

**理由**：
- 清晰明确
- 自动触发布局更新

---

### ❌ 不推荐：直接修改依赖属性（除非特殊需求）

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/property/DependencyProperty.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
// ❌ 不推荐直接设置属性值
panel->set_value(StackPanel::OrientationProperty, Orientation::Horizontal);

// ✅ 推荐使用封装的 set_orientation
panel->set_orientation(Orientation::Horizontal);
```

**问题**：直接设置依赖属性不够语义化，且容易出错。

---

### ✅ 推荐：为子元素设置固定尺寸（主轴方向）

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn1 = new Button();
btn1->set_height(50);  // ✅ 垂直布局时设置高度
Button* btn2 = new Button();
btn2->set_height(30);

panel->add_child(btn1);
panel->add_child(btn2);
```

**理由**：StackPanel 在主轴方向传入 INFINITY，子元素需要自行确定尺寸。

---

### ❌ 不推荐：主轴方向不设尺寸（导致不可预测的布局）

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn1 = new Button();
// ❌ 未设置高度，按钮可能不可见或尺寸异常
Button* btn2 = new Button();

panel->add_child(btn1);
panel->add_child(btn2);
```

**问题**：子元素在主轴方向没有明确尺寸，布局结果不可预测。

---

### ✅ 推荐：交叉轴使用 Stretch 对齐（默认）

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);
panel->set_width(300);

Button* btn = new Button();
btn->set_horizontal_alignment(HorizontalAlignment::Stretch);  // ✅ 默认
panel->add_child(btn);

// 按钮宽度自动填充 300px
```

**理由**：子元素自动填充交叉轴空间，布局更灵活。

---

### ❌ 不推荐：交叉轴固定尺寸（除非确实需要）

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);
panel->set_width(300);

Button* btn = new Button();
btn->set_width(100);  // ❌ 固定宽度，无法响应面板宽度变化
panel->add_child(btn);
```

**问题**：子元素无法响应面板尺寸变化，布局不够灵活。

---

### ✅ 推荐：嵌套 StackPanel 实现复杂布局

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

// 外层：垂直
StackPanel* outer = new StackPanel();
outer->set_orientation(Orientation::Vertical);

TextBlock* title = new TextBlock();
title->set_text("标题");

// 内层：水平
StackPanel* inner = new StackPanel();
inner->set_orientation(Orientation::Horizontal);
Button* btn1 = new Button();
Button* btn2 = new Button();
inner->add_child(btn1);
inner->add_child(btn2);

outer->add_child(title);
outer->add_child(inner);
```

**理由**：通过嵌套实现复杂的行列布局。

---

### ❌ 不推荐：使用 StackPanel 实现网格布局

```cpp
#include <mine/ui/layout/StackPanel.h>

using namespace mine::ui;

// ❌ 不推荐使用 StackPanel 实现网格布局
StackPanel* row1 = new StackPanel();
row1->set_orientation(Orientation::Horizontal);
// 添加列1, 列2, 列3...

StackPanel* row2 = new StackPanel();
row2->set_orientation(Orientation::Horizontal);
// 添加列1, 列2, 列3...

StackPanel* grid = new StackPanel();
grid->set_orientation(Orientation::Vertical);
grid->add_child(row1);
grid->add_child(row2);
```

**问题**：列宽度难以对齐，不如直接使用 Grid。

**✅ 解决方案**：使用 Grid 实现网格布局

```cpp
#include <mine/ui/layout/Grid.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::star()});
grid->add_column({GridLength::star()});
grid->add_column({GridLength::star()});

// 添加子元素，使用 Grid::set_row / Grid::set_column 指定位置
```

---

## 常见陷阱

### ❌ 陷阱 1：忘记设置主轴方向尺寸

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn = new Button();
// ❌ 未设置高度，按钮在垂直布局中可能不可见
panel->add_child(btn);
```

**问题**：StackPanel 在主轴方向传入 INFINITY，子元素必须自行确定尺寸。

**✅ 解决方案**：设置主轴方向的固定尺寸

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn = new Button();
btn->set_height(50);  // ✅ 设置高度
panel->add_child(btn);
```

---

### ❌ 陷阱 2：期望子元素在主轴方向均分空间

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Horizontal);
panel->set_width(300);

Button* btn1 = new Button();
Button* btn2 = new Button();
Button* btn3 = new Button();
panel->add_child(btn1);
panel->add_child(btn2);
panel->add_child(btn3);

// ❌ 期望每个按钮宽度为 100px（300 / 3）
// 实际：按钮宽度取决于 desired_size
```

**问题**：StackPanel 不会自动均分主轴空间，子元素按 desired_size 排列。

**✅ 解决方案**：使用 Grid 实现均分布局

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_column({GridLength::star()});  // 1*
grid->add_column({GridLength::star()});  // 1*
grid->add_column({GridLength::star()});  // 1*

Button* btn1 = new Button();
Button* btn2 = new Button();
Button* btn3 = new Button();

grid->add_child(btn1);
Grid::set_column(*btn1, 0);
grid->add_child(btn2);
Grid::set_column(*btn2, 1);
grid->add_child(btn3);
Grid::set_column(*btn3, 2);

// ✅ 每列宽度均分（1* : 1* : 1*）
```

---

### ❌ 陷阱 3：主轴方向子元素溢出

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);
panel->set_height(100);  // 面板高度固定 100px

Button* btn1 = new Button();
btn1->set_height(80);
Button* btn2 = new Button();
btn2->set_height(80);

panel->add_child(btn1);
panel->add_child(btn2);

// ❌ 总高度 160px > 面板高度 100px，子元素溢出
```

**问题**：StackPanel 不会自动裁剪或缩放子元素，溢出部分可能不可见。

**✅ 解决方案**：使用 ScrollViewer 包裹 StackPanel

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/ScrollViewer.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);

Button* btn1 = new Button();
btn1->set_height(80);
Button* btn2 = new Button();
btn2->set_height(80);

panel->add_child(btn1);
panel->add_child(btn2);

// 使用 ScrollViewer 包裹
ScrollViewer* scroll = new ScrollViewer();
scroll->set_content(panel);
scroll->set_height(100);

// ✅ 溢出部分可以滚动查看
```

---

### ❌ 陷阱 4：混淆主轴和交叉轴

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);  // 垂直布局

Button* btn = new Button();
btn->set_width(100);  // ❌ 设置宽度（交叉轴），但忘记设置高度（主轴）
panel->add_child(btn);

// 按钮高度可能异常
```

**问题**：混淆主轴（垂直）和交叉轴（水平），导致尺寸设置错误。

**✅ 解决方案**：明确主轴和交叉轴

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

StackPanel* panel = new StackPanel();
panel->set_orientation(Orientation::Vertical);  // 主轴=垂直，交叉轴=水平

Button* btn = new Button();
btn->set_height(50);  // ✅ 主轴方向（垂直）设置高度
btn->set_horizontal_alignment(HorizontalAlignment::Stretch);  // ✅ 交叉轴（水平）Stretch
panel->add_child(btn);
```

---

## 完整示例

以下示例演示如何使用 StackPanel 实现一个完整的登录界面。

```cpp
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/visual/HorizontalAlignment.h>
#include <mine/ui/visual/VerticalAlignment.h>
#include <memory>

using namespace mine::ui;
using namespace mine::math;

/**
 * @brief 登录界面
 *
 * 布局结构：
 *   StackPanel (Vertical)
 *       ├─ TextBlock (标题)
 *       ├─ StackPanel (用户名行)
 *       │   ├─ TextBlock (标签)
 *       │   └─ TextBox (输入框)
 *       ├─ StackPanel (密码行)
 *       │   ├─ TextBlock (标签)
 *       │   └─ TextBox (输入框)
 *       └─ StackPanel (按钮行)
 *           ├─ Button (登录)
 *           └─ Button (取消)
 */
class LoginPanel {
public:
    LoginPanel() {
        create_ui();
    }

    StackPanel* get_root() const {
        return root_.get();
    }

private:
    void create_ui() {
        // 根面板（垂直排列）
        root_ = std::make_unique<StackPanel>();
        root_->set_orientation(Orientation::Vertical);
        root_->set_width(400);
        root_->set_padding(Thickness{20, 20, 20, 20});
        root_->set_horizontal_alignment(HorizontalAlignment::Center);
        root_->set_vertical_alignment(VerticalAlignment::Center);

        // 标题
        auto title = std::make_unique<TextBlock>();
        title->set_text("用户登录");
        title->set_font_size(24);
        title->set_horizontal_alignment(HorizontalAlignment::Center);
        title->set_margin(Thickness{0, 0, 0, 20});
        root_->add_child(title.get());
        children_.push_back(std::move(title));

        // 用户名行
        auto username_row = create_input_row("用户名：", &txt_username_);
        root_->add_child(username_row.get());
        children_.push_back(std::move(username_row));

        // 密码行
        auto password_row = create_input_row("密码：", &txt_password_);
        txt_password_->set_is_password(true);
        root_->add_child(password_row.get());
        children_.push_back(std::move(password_row));

        // 按钮行
        auto button_row = create_button_row();
        root_->add_child(button_row.get());
        children_.push_back(std::move(button_row));
    }

    std::unique_ptr<StackPanel> create_input_row(const std::string& label_text, TextBox** out_textbox) {
        auto row = std::make_unique<StackPanel>();
        row->set_orientation(Orientation::Horizontal);
        row->set_margin(Thickness{0, 0, 0, 10});

        // 标签
        auto label = std::make_unique<TextBlock>();
        label->set_text(label_text);
        label->set_width(80);
        label->set_vertical_alignment(VerticalAlignment::Center);
        row->add_child(label.get());
        children_.push_back(std::move(label));

        // 输入框
        auto textbox = std::make_unique<TextBox>();
        textbox->set_horizontal_alignment(HorizontalAlignment::Stretch);
        textbox->set_height(30);
        *out_textbox = textbox.get();
        row->add_child(textbox.get());
        children_.push_back(std::move(textbox));

        return row;
    }

    std::unique_ptr<StackPanel> create_button_row() {
        auto row = std::make_unique<StackPanel>();
        row->set_orientation(Orientation::Horizontal);
        row->set_horizontal_alignment(HorizontalAlignment::Right);
        row->set_margin(Thickness{0, 20, 0, 0});

        // 登录按钮
        auto btn_login = std::make_unique<Button>();
        btn_login->set_text("登录");
        btn_login->set_width(80);
        btn_login->set_height(30);
        btn_login->set_margin(Thickness{0, 0, 10, 0});
        btn_login->on_click([this]() { on_login_clicked(); });
        row->add_child(btn_login.get());
        children_.push_back(std::move(btn_login));

        // 取消按钮
        auto btn_cancel = std::make_unique<Button>();
        btn_cancel->set_text("取消");
        btn_cancel->set_width(80);
        btn_cancel->set_height(30);
        btn_cancel->on_click([this]() { on_cancel_clicked(); });
        row->add_child(btn_cancel.get());
        children_.push_back(std::move(btn_cancel));

        return row;
    }

    void on_login_clicked() {
        std::string username = txt_username_->text();
        std::string password = txt_password_->text();
        
        // 验证逻辑
        if (username.empty() || password.empty()) {
            // 显示错误消息
            return;
        }
        
        // 执行登录
        std::cout << "登录：用户名=" << username << ", 密码=" << password << "\n";
    }

    void on_cancel_clicked() {
        // 清空输入
        txt_username_->set_text("");
        txt_password_->set_text("");
        std::cout << "取消登录\n";
    }

    std::unique_ptr<StackPanel> root_;
    std::vector<std::unique_ptr<UIElement>> children_;
    TextBox* txt_username_ = nullptr;
    TextBox* txt_password_ = nullptr;
};

// ═════════════════════════════════════════════════════════════════════════
// 使用示例
// ═════════════════════════════════════════════════════════════════════════

#include <iostream>

int main() {
    LoginPanel login;
    StackPanel* root = login.get_root();

    // 测量和排列
    Size available{800, 600};
    root->measure(available);
    
    Size desired = root->desired_size();
    std::cout << "登录面板期望尺寸：" << desired.width << " × " << desired.height << "\n";

    // 居中排列
    Rect final_rect{
        (800 - desired.width) / 2,
        (600 - desired.height) / 2,
        desired.width,
        desired.height
    };
    root->arrange(final_rect);

    std::cout << "\n布局结果：\n";
    std::cout << "根面板位置：x=" << final_rect.x << ", y=" << final_rect.y << "\n";
    std::cout << "根面板尺寸：w=" << final_rect.width << ", h=" << final_rect.height << "\n";

    // 遍历子元素
    for (uint32_t i = 0; i < root->children_count(); ++i) {
        UIElement* child = root->child_at(i);
        Rect bounds = child->render_bounds();
        std::cout << "子元素 " << i << "：";
        std::cout << "x=" << bounds.x << ", ";
        std::cout << "y=" << bounds.y << ", ";
        std::cout << "w=" << bounds.width << ", ";
        std::cout << "h=" << bounds.height << "\n";
    }

    /*
     * 预期输出：
     *
     * 登录面板期望尺寸：400 × 220
     *
     * 布局结果：
     * 根面板位置：x=200, y=190
     * 根面板尺寸：w=400, h=220
     * 子元素 0：x=0, y=0, w=400, h=40     (标题)
     * 子元素 1：x=0, y=60, w=400, h=30    (用户名行)
     * 子元素 2：x=0, y=100, w=400, h=30   (密码行)
     * 子元素 3：x=240, y=150, w=160, h=30 (按钮行)
     *
     * （所有子元素在垂直方向上依次排列，间距由 margin 控制）
     */

    return 0;
}
```

**示例说明**：
1. **LoginPanel**：使用 StackPanel 实现登录界面
2. **create_input_row**：创建标签 + 输入框的水平行
3. **create_button_row**：创建登录 + 取消按钮的水平行
4. **嵌套布局**：外层垂直 StackPanel，内层水平 StackPanel
5. **事件处理**：on_login_clicked / on_cancel_clicked 处理按钮点击
6. **输出**：显示布局结果和子元素位置

---

## 总结

**核心要点**：
1. **StackPanel 是线性堆叠布局面板**，沿指定方向依次排列子元素。
2. **支持两种方向**：Horizontal（水平）和 Vertical（垂直）。
3. **主轴方向累加尺寸**，交叉轴方向取最大值。
4. **主轴方向传入 INFINITY**，子元素需自行确定尺寸。
5. **通过 set_orientation 切换方向**，自动触发布局更新。
6. **适合工具栏、列表、表单等线性布局场景**。
7. **可嵌套使用**，实现复杂的行列布局。

**使用建议**：
- ✅ 为子元素设置主轴方向的固定尺寸
- ✅ 交叉轴使用 Stretch 对齐（默认）
- ✅ 嵌套 StackPanel 实现复杂布局
- ✅ 使用 Grid 实现网格布局（而非多层 StackPanel）
- ❌ 不要期望子元素在主轴方向自动均分空间
- ❌ 不要混淆主轴和交叉轴
- ❌ 不要忘记处理主轴方向溢出的情况
