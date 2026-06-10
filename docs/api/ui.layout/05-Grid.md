# Grid 详细接口文档

## 概述

`Grid` 是 `mine.ui.layout` 模块的**行列网格布局面板**。

**核心特性：**
- **行列网格**：将可用空间划分为若干行和列
- **三种尺寸**：Pixel（固定像素）、Auto（自动）、Star（比例分配）
- **附加属性**：RowProperty、ColumnProperty、RowSpanProperty、ColumnSpanProperty
- **跨行列**：子元素可跨越多行或多列
- **灵活布局**：支持固定、自动、比例混合布局

**继承关系：**
```
Visual (mine.ui.visual)
    └─ UIElement
        └─ FrameworkElement
            └─ Panel
                └─ Grid
```

**布局算法：**
1. **Measure 阶段**：
   - Pixel 行/列：直接使用固定尺寸
   - Auto 行/列：收集该行/列中所有子元素的 desiredSize，取最大值
   - Star 行/列：按权重比例分配剩余空间

2. **Arrange 阶段**：
   - 根据行/列尺寸计算每个子元素的 slot 矩形
   - 支持跨行列合并
   - 逐一调用 child->arrange(slot)

---

## 文件位置

```
src/mine/ui/layout/api/include/mine/ui/layout/Grid.h
```

---

## 类定义

```cpp
namespace mine::ui {

/**
 * @brief 行列网格布局面板。
 *
 * 附加属性（可在任意 FrameworkElement 上设置）：
 *   - Grid::RowProperty      ：子元素所在行索引（默认 0）
 *   - Grid::ColumnProperty   ：子元素所在列索引（默认 0）
 *   - Grid::RowSpanProperty  ：子元素跨越的行数（默认 1）
 *   - Grid::ColumnSpanProperty：子元素跨越的列数（默认 1）
 */
class MINE_UI_LAYOUT_API Grid : public Panel {
public:
    // ── 附加属性（注册在 Grid 上，可设置到任意 FrameworkElement）──────────

    /// 子元素所在行（0-based，默认 0）
    static const DependencyProperty& RowProperty;

    /// 子元素所在列（0-based，默认 0）
    static const DependencyProperty& ColumnProperty;

    /// 子元素跨越行数（默认 1）
    static const DependencyProperty& RowSpanProperty;

    /// 子元素跨越列数（默认 1）
    static const DependencyProperty& ColumnSpanProperty;

    // ── 便捷静态方法（等价于在子元素上 get/set 对应附加属性）──────────────

    static int  get_row(const UIElement& elem) noexcept;
    static void set_row(UIElement& elem, int row);

    static int  get_column(const UIElement& elem) noexcept;
    static void set_column(UIElement& elem, int col);

    static int  get_row_span(const UIElement& elem) noexcept;
    static void set_row_span(UIElement& elem, int span);

    static int  get_column_span(const UIElement& elem) noexcept;
    static void set_column_span(UIElement& elem, int span);

    // ── 生命周期 ──────────────────────────────────────────────────────────

    Grid();
    ~Grid() override;

    Grid(const Grid&)            = delete;
    Grid& operator=(const Grid&) = delete;
    Grid(Grid&&)                 = default;
    Grid& operator=(Grid&&)      = default;

    // ── 行列定义 ──────────────────────────────────────────────────────────

    /**
     * @brief 添加行定义（末尾追加）。
     *
     * 若未添加任何行定义，Grid 视为只有一行（1*）。
     *
     * @param def 行定义
     */
    void add_row(RowDefinition def);

    /**
     * @brief 添加列定义（末尾追加）。
     *
     * 若未添加任何列定义，Grid 视为只有一列（1*）。
     *
     * @param def 列定义
     */
    void add_column(ColumnDefinition def);

    [[nodiscard]] uint32_t row_count()    const noexcept;
    [[nodiscard]] uint32_t column_count() const noexcept;

protected:
    // ── 布局实现 ──────────────────────────────────────────────────────────

    math::Size measure_override(math::Size available) override;
    math::Size arrange_override(math::Size final_size) override;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
```

---

## 成员方法

### 构造函数

```cpp
Grid();
```

**描述**：创建空 Grid（无行列定义，默认 1 行 1 列）。

**示例**：
```cpp
#include <mine/ui/layout/Grid.h>

using namespace mine::ui;

Grid* grid = new Grid();
assert(grid->row_count() == 0);     // 默认无行定义
assert(grid->column_count() == 0);  // 默认无列定义
```

---

### 析构函数

```cpp
~Grid() override;
```

**描述**：销毁 Grid。注意：不会销毁子元素（继承自 Panel）。

---

### add_row

```cpp
void add_row(RowDefinition def);
```

**描述**：添加行定义（末尾追加）。

**参数**：
- `def`：行定义（包含 height、min_height、max_height）

**行为**：
- 将行定义添加到行列表末尾
- 触发 invalidate_measure() 请求重新布局

**示例**：
```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 添加 3 行
grid->add_row({GridLength::auto_()});        // 第 0 行：Auto
grid->add_row({GridLength::star()});         // 第 1 行：1*
grid->add_row({GridLength::pixel(100)});     // 第 2 行：100px

assert(grid->row_count() == 3);
```

---

### add_column

```cpp
void add_column(ColumnDefinition def);
```

**描述**：添加列定义（末尾追加）。

**参数**：
- `def`：列定义（包含 width、min_width、max_width）

**行为**：
- 将列定义添加到列列表末尾
- 触发 invalidate_measure() 请求重新布局

**示例**：
```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 添加 3 列
grid->add_column({GridLength::pixel(200)});  // 第 0 列：200px
grid->add_column({GridLength::star()});      // 第 1 列：1*
grid->add_column({GridLength::star(2)});     // 第 2 列：2*

assert(grid->column_count() == 3);
```

---

### row_count

```cpp
[[nodiscard]] uint32_t row_count() const noexcept;
```

**描述**：获取行定义数量。

**返回值**：行数。

**示例**：
```cpp
Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::star()});
assert(grid->row_count() == 2);
```

---

### column_count

```cpp
[[nodiscard]] uint32_t column_count() const noexcept;
```

**描述**：获取列定义数量。

**返回值**：列数。

**示例**：
```cpp
Grid* grid = new Grid();
grid->add_column({GridLength::pixel(100)});
grid->add_column({GridLength::pixel(200)});
assert(grid->column_count() == 2);
```

---

### get_row / set_row

```cpp
static int get_row(const UIElement& elem) noexcept;
static void set_row(UIElement& elem, int row);
```

**描述**：获取/设置子元素所在行索引（0-based）。

**参数**：
- `elem`：子元素
- `row`：行索引（默认 0）

**示例**：
```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});

Button* btn = new Button();
Grid::set_row(*btn, 1);  // 放置在第 1 行
grid->add_child(btn);

assert(Grid::get_row(*btn) == 1);
```

---

### get_column / set_column

```cpp
static int get_column(const UIElement& elem) noexcept;
static void set_column(UIElement& elem, int col);
```

**描述**：获取/设置子元素所在列索引（0-based）。

**参数**：
- `elem`：子元素
- `col`：列索引（默认 0）

**示例**：
```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn = new Button();
Grid::set_column(*btn, 1);  // 放置在第 1 列
grid->add_child(btn);

assert(Grid::get_column(*btn) == 1);
```

---

### get_row_span / set_row_span

```cpp
static int get_row_span(const UIElement& elem) noexcept;
static void set_row_span(UIElement& elem, int span);
```

**描述**：获取/设置子元素跨越的行数。

**参数**：
- `elem`：子元素
- `span`：跨越行数（默认 1）

**示例**：
```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});

Button* btn = new Button();
Grid::set_row(*btn, 0);
Grid::set_row_span(*btn, 2);  // 跨越第 0、1 行
grid->add_child(btn);

assert(Grid::get_row_span(*btn) == 2);
```

---

### get_column_span / set_column_span

```cpp
static int get_column_span(const UIElement& elem) noexcept;
static void set_column_span(UIElement& elem, int span);
```

**描述**：获取/设置子元素跨越的列数。

**参数**：
- `elem`：子元素
- `span`：跨越列数（默认 1）

**示例**：
```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn = new Button();
Grid::set_column(*btn, 0);
Grid::set_column_span(*btn, 3);  // 跨越第 0、1、2 列
grid->add_child(btn);

assert(Grid::get_column_span(*btn) == 3);
```

---

### measure_override

```cpp
math::Size measure_override(math::Size available) override;
```

**描述**：测量 Grid 及其子元素的期望尺寸。

**参数**：
- `available`：父容器提供的可用尺寸

**返回值**：Grid 的期望尺寸。

**算法**：
1. **处理 Pixel 行/列**：直接使用固定尺寸
2. **处理 Auto 行/列**：收集该行/列中所有子元素的 desiredSize，取最大值
3. **处理 Star 行/列**：按权重比例分配剩余空间
4. **返回总尺寸**：所有行高之和 × 所有列宽之和

---

### arrange_override

```cpp
math::Size arrange_override(math::Size final_size) override;
```

**描述**：排列 Grid 的子元素。

**参数**：
- `final_size`：父容器分配给 Grid 的最终尺寸

**返回值**：实际使用的尺寸（通常返回 final_size）。

**算法**：
1. 根据行/列尺寸计算每行/列的起始位置和高度/宽度
2. 遍历所有子元素
3. 根据 Row、Column、RowSpan、ColumnSpan 计算 slot 矩形
4. 调用 child->arrange(slot)

---

## 使用场景

### 1. 固定像素布局（2 行 3 列）

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 定义 2 行 3 列（固定像素）
grid->add_row({GridLength::pixel(100)});
grid->add_row({GridLength::pixel(150)});
grid->add_column({GridLength::pixel(200)});
grid->add_column({GridLength::pixel(250)});
grid->add_column({GridLength::pixel(300)});

// 添加按钮到 (0, 0)
Button* btn00 = new Button();
btn00->set_text("(0, 0)");
Grid::set_row(*btn00, 0);
Grid::set_column(*btn00, 0);
grid->add_child(btn00);

// 添加按钮到 (1, 2)
Button* btn12 = new Button();
btn12->set_text("(1, 2)");
Grid::set_row(*btn12, 1);
Grid::set_column(*btn12, 2);
grid->add_child(btn12);
```

---

### 2. Auto 自动尺寸（根据内容）

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/TextBlock.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 定义 2 行 2 列（Auto）
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

// 第 0 列自动适应最宽的标签
TextBlock* lbl00 = new TextBlock();
lbl00->set_text("用户名：");
Grid::set_row(*lbl00, 0);
Grid::set_column(*lbl00, 0);
grid->add_child(lbl00);

TextBlock* lbl10 = new TextBlock();
lbl10->set_text("密码：");
Grid::set_row(*lbl10, 1);
Grid::set_column(*lbl10, 0);
grid->add_child(lbl10);

// 第 1 列自动适应最宽的输入框
TextBox* txt00 = new TextBox();
Grid::set_row(*txt00, 0);
Grid::set_column(*txt00, 1);
grid->add_child(txt00);

TextBox* txt10 = new TextBox();
Grid::set_row(*txt10, 1);
Grid::set_column(*txt10, 1);
grid->add_child(txt10);
```

---

### 3. Star 比例分配（响应式）

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 定义 1 行 3 列（比例 1:2:1）
grid->add_column({GridLength::star(1)});  // 25%
grid->add_column({GridLength::star(2)});  // 50%
grid->add_column({GridLength::star(1)});  // 25%

Button* btn0 = new Button();
btn0->set_text("左");
Grid::set_column(*btn0, 0);
grid->add_child(btn0);

Button* btn1 = new Button();
btn1->set_text("中（宽）");
Grid::set_column(*btn1, 1);
grid->add_child(btn1);

Button* btn2 = new Button();
btn2->set_text("右");
Grid::set_column(*btn2, 2);
grid->add_child(btn2);
```

---

### 4. 混合布局（Pixel + Auto + Star）

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 定义 3 行：Auto, 1*, 50px
grid->add_row({GridLength::auto_()});     // 标题行（自动）
grid->add_row({GridLength::star()});      // 内容行（填充剩余）
grid->add_row({GridLength::pixel(50)});   // 按钮行（固定 50px）

// 定义 2 列：200px, 1*
grid->add_column({GridLength::pixel(200)});  // 侧边栏（固定 200px）
grid->add_column({GridLength::star()});      // 主内容区（填充剩余）

// 标题（跨 2 列）
TextBlock* title = new TextBlock();
title->set_text("应用标题");
Grid::set_row(*title, 0);
Grid::set_column(*title, 0);
Grid::set_column_span(*title, 2);
grid->add_child(title);

// 侧边栏
TextBlock* sidebar = new TextBlock();
sidebar->set_text("侧边栏");
Grid::set_row(*sidebar, 1);
Grid::set_column(*sidebar, 0);
grid->add_child(sidebar);

// 主内容区
TextBox* content = new TextBox();
content->set_text("主内容区");
Grid::set_row(*content, 1);
Grid::set_column(*content, 1);
grid->add_child(content);

// 按钮行（跨 2 列）
Button* btn = new Button();
btn->set_text("提交");
Grid::set_row(*btn, 2);
Grid::set_column(*btn, 0);
Grid::set_column_span(*btn, 2);
grid->add_child(btn);
```

---

### 5. 跨行列合并

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 定义 3 行 3 列
grid->add_row({GridLength::star()});
grid->add_row({GridLength::star()});
grid->add_row({GridLength::star()});
grid->add_column({GridLength::star()});
grid->add_column({GridLength::star()});
grid->add_column({GridLength::star()});

// 左上角按钮（跨 2 行 2 列）
Button* btn_large = new Button();
btn_large->set_text("大按钮");
Grid::set_row(*btn_large, 0);
Grid::set_column(*btn_large, 0);
Grid::set_row_span(*btn_large, 2);
Grid::set_column_span(*btn_large, 2);
grid->add_child(btn_large);

// 右上角按钮（1 行 1 列）
Button* btn_small = new Button();
btn_small->set_text("小");
Grid::set_row(*btn_small, 0);
Grid::set_column(*btn_small, 2);
grid->add_child(btn_small);
```

---

### 6. 响应式表单布局

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/TextBox.h>

using namespace mine::ui;

Grid* form = new Grid();

// 定义 3 行（每行一个字段）
form->add_row({GridLength::auto_()});
form->add_row({GridLength::auto_()});
form->add_row({GridLength::auto_()});

// 定义 2 列：标签列（Auto），输入列（1*）
form->add_column({GridLength::auto_()});
form->add_column({GridLength::star()});

// 姓名行
TextBlock* lbl_name = new TextBlock();
lbl_name->set_text("姓名：");
Grid::set_row(*lbl_name, 0);
Grid::set_column(*lbl_name, 0);
form->add_child(lbl_name);

TextBox* txt_name = new TextBox();
Grid::set_row(*txt_name, 0);
Grid::set_column(*txt_name, 1);
form->add_child(txt_name);

// 年龄行
TextBlock* lbl_age = new TextBlock();
lbl_age->set_text("年龄：");
Grid::set_row(*lbl_age, 1);
Grid::set_column(*lbl_age, 0);
form->add_child(lbl_age);

TextBox* txt_age = new TextBox();
Grid::set_row(*txt_age, 1);
Grid::set_column(*txt_age, 1);
form->add_child(txt_age);

// 邮箱行
TextBlock* lbl_email = new TextBlock();
lbl_email->set_text("邮箱：");
Grid::set_row(*lbl_email, 2);
Grid::set_column(*lbl_email, 0);
form->add_child(lbl_email);

TextBox* txt_email = new TextBox();
Grid::set_row(*txt_email, 2);
Grid::set_column(*txt_email, 1);
form->add_child(txt_email);
```

---

### 7. 仪表板布局

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Border.h>

using namespace mine::ui;

Grid* dashboard = new Grid();

// 定义 2 行 2 列（均分）
dashboard->add_row({GridLength::star()});
dashboard->add_row({GridLength::star()});
dashboard->add_column({GridLength::star()});
dashboard->add_column({GridLength::star()});

// 左上：销售额
Border* sales = new Border();
sales->set_child(new TextBlock("销售额"));
Grid::set_row(*sales, 0);
Grid::set_column(*sales, 0);
dashboard->add_child(sales);

// 右上：订单数
Border* orders = new Border();
orders->set_child(new TextBlock("订单数"));
Grid::set_row(*orders, 0);
Grid::set_column(*orders, 1);
dashboard->add_child(orders);

// 左下：用户数
Border* users = new Border();
users->set_child(new TextBlock("用户数"));
Grid::set_row(*users, 1);
Grid::set_column(*users, 0);
dashboard->add_child(users);

// 右下：收入
Border* revenue = new Border();
revenue->set_child(new TextBlock("收入"));
Grid::set_row(*revenue, 1);
Grid::set_column(*revenue, 1);
dashboard->add_child(revenue);
```

---

### 8. 应用主窗口布局

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/TextBlock.h>

using namespace mine::ui;

Grid* main_window = new Grid();

// 定义 3 行：标题栏(50px), 内容区(1*), 状态栏(30px)
main_window->add_row({GridLength::pixel(50)});   // 标题栏
main_window->add_row({GridLength::star()});      // 内容区
main_window->add_row({GridLength::pixel(30)});   // 状态栏

// 定义 2 列：侧边栏(200px), 主内容(1*)
main_window->add_column({GridLength::pixel(200)});  // 侧边栏
main_window->add_column({GridLength::star()});      // 主内容

// 标题栏（跨 2 列）
TextBlock* titlebar = new TextBlock();
titlebar->set_text("MineUI 应用");
Grid::set_row(*titlebar, 0);
Grid::set_column(*titlebar, 0);
Grid::set_column_span(*titlebar, 2);
main_window->add_child(titlebar);

// 侧边栏
TextBlock* sidebar = new TextBlock();
sidebar->set_text("侧边栏");
Grid::set_row(*sidebar, 1);
Grid::set_column(*sidebar, 0);
main_window->add_child(sidebar);

// 主内容区
TextBlock* content = new TextBlock();
content->set_text("主内容区");
Grid::set_row(*content, 1);
Grid::set_column(*content, 1);
main_window->add_child(content);

// 状态栏（跨 2 列）
TextBlock* statusbar = new TextBlock();
statusbar->set_text("就绪");
Grid::set_row(*statusbar, 2);
Grid::set_column(*statusbar, 0);
Grid::set_column_span(*statusbar, 2);
main_window->add_child(statusbar);
```

---

## 最佳实践

### ✅ 推荐：使用 GridLength 静态工厂方法

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ✅ 清晰明确
grid->add_row({GridLength::pixel(100)});
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::star()});
```

**理由**：代码可读性高，意图明确。

---

### ❌ 不推荐：直接构造 GridLength（除非必要）

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ❌ 不够直观
grid->add_row({GridLength{100, GridLength::Type::Pixel}});
grid->add_row({GridLength{0, GridLength::Type::Auto}});
grid->add_row({GridLength{1, GridLength::Type::Star}});
```

**问题**：代码冗长，可读性差。

---

### ✅ 推荐：使用静态方法设置附加属性

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
Button* btn = new Button();

// ✅ 简洁明了
Grid::set_row(*btn, 1);
Grid::set_column(*btn, 2);
Grid::set_row_span(*btn, 2);
Grid::set_column_span(*btn, 3);

grid->add_child(btn);
```

**理由**：语义清晰，代码简洁。

---

### ❌ 不推荐：直接设置依赖属性（除非特殊需求）

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
Button* btn = new Button();

// ❌ 代码冗长
btn->set_value(Grid::RowProperty, 1);
btn->set_value(Grid::ColumnProperty, 2);
btn->set_value(Grid::RowSpanProperty, 2);
btn->set_value(Grid::ColumnSpanProperty, 3);

grid->add_child(btn);
```

**问题**：代码不够简洁，可读性差。

---

### ✅ 推荐：合理使用 Star 比例实现响应式布局

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ✅ 列宽度自动适应窗口尺寸（比例 1:2:1）
grid->add_column({GridLength::star(1)});
grid->add_column({GridLength::star(2)});
grid->add_column({GridLength::star(1)});
```

**理由**：布局自动适应容器尺寸，响应式设计。

---

### ❌ 不推荐：所有列都使用固定像素

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ❌ 无法响应窗口尺寸变化
grid->add_column({GridLength::pixel(300)});
grid->add_column({GridLength::pixel(400)});
grid->add_column({GridLength::pixel(300)});
```

**问题**：布局固定，无法适应不同窗口尺寸。

---

### ✅ 推荐：使用 Auto 实现内容驱动布局

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ✅ 列宽度自动适应内容
grid->add_column({GridLength::auto_()});  // 标签列（自动）
grid->add_column({GridLength::star()});   // 输入列（填充）
```

**理由**：标签列宽度自动适应最宽的标签，输入列填充剩余空间。

---

### ❌ 不推荐：标签列使用 Star 导致过宽

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ❌ 标签列和输入列均分空间（标签列过宽）
grid->add_column({GridLength::star()});
grid->add_column({GridLength::star()});
```

**问题**：标签列占用过多空间，浪费布局。

---

### ✅ 推荐：跨行列时确保不超出范围

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn = new Button();
Grid::set_row(*btn, 0);
Grid::set_column(*btn, 0);
Grid::set_row_span(*btn, 2);  // ✅ 跨越第 0、1 行（总共 2 行）
Grid::set_column_span(*btn, 2);  // ✅ 跨越第 0、1 列（总共 2 列）
grid->add_child(btn);
```

**理由**：确保跨行列不超出 Grid 范围。

---

### ❌ 不推荐：跨行列超出 Grid 范围

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});

Button* btn = new Button();
Grid::set_row(*btn, 0);
Grid::set_row_span(*btn, 5);  // ❌ 超出范围（总共只有 2 行）
grid->add_child(btn);
```

**问题**：布局异常，可能导致崩溃或未定义行为。

---

## 常见陷阱

### ❌ 陷阱 1：忘记添加行列定义

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
// ❌ 未添加行列定义，默认 1 行 1 列

Button* btn1 = new Button();
Grid::set_row(*btn1, 0);
Grid::set_column(*btn1, 0);
grid->add_child(btn1);

Button* btn2 = new Button();
Grid::set_row(*btn2, 0);
Grid::set_column(*btn2, 1);  // ❌ 列 1 不存在
grid->add_child(btn2);

// 两个按钮重叠在 (0, 0) 位置
```

**问题**：未添加行列定义，所有子元素重叠在默认行列。

**✅ 解决方案**：先添加行列定义

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ✅ 添加行列定义
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn1 = new Button();
Grid::set_row(*btn1, 0);
Grid::set_column(*btn1, 0);
grid->add_child(btn1);

Button* btn2 = new Button();
Grid::set_row(*btn2, 0);
Grid::set_column(*btn2, 1);
grid->add_child(btn2);
```

---

### ❌ 陷阱 2：行列索引超出范围

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn = new Button();
Grid::set_row(*btn, 5);  // ❌ 行 5 不存在（总共 2 行）
Grid::set_column(*btn, 0);
grid->add_child(btn);

// 按钮位置异常
```

**问题**：行列索引超出定义范围。

**✅ 解决方案**：确保索引在有效范围内

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn = new Button();
Grid::set_row(*btn, 1);  // ✅ 行 1 存在
Grid::set_column(*btn, 0);
grid->add_child(btn);
```

---

### ❌ 陷阱 3：Auto 行列尺寸为 0

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn = new Button();
// ❌ 未设置尺寸，desired_size 为 0
Grid::set_row(*btn, 0);
Grid::set_column(*btn, 0);
grid->add_child(btn);

// Auto 行列尺寸为 0，按钮不可见
```

**问题**：Auto 行列尺寸取决于子元素 desired_size，若子元素无尺寸则为 0。

**✅ 解决方案**：为子元素设置尺寸或使用 Pixel/Star

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Button.h>

using namespace mine::ui;

Grid* grid = new Grid();
grid->add_row({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

Button* btn = new Button();
btn->set_width(100);   // ✅ 设置尺寸
btn->set_height(50);
Grid::set_row(*btn, 0);
Grid::set_column(*btn, 0);
grid->add_child(btn);

// Auto 行列尺寸 = 子元素尺寸
```

---

### ❌ 陷阱 4：Star 权重为 0

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ❌ Star 权重为 0，列宽度为 0
grid->add_column({GridLength::star(0)});
grid->add_column({GridLength::star(1)});
```

**问题**：Star 权重为 0，该列宽度为 0。

**✅ 解决方案**：使用正数权重

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// ✅ 权重为正数
grid->add_column({GridLength::star(1)});
grid->add_column({GridLength::star(1)});
```

---

## 完整示例

以下示例演示如何使用 Grid 实现一个完整的计算器界面。

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

using namespace mine::ui;
using namespace mine::math;

/**
 * @brief 计算器界面
 *
 * 布局结构：
 *   Grid (5 行 4 列)
 *       ├─ 第 0 行：显示屏（跨 4 列）
 *       ├─ 第 1 行：7 8 9 ÷
 *       ├─ 第 2 行：4 5 6 ×
 *       ├─ 第 3 行：1 2 3 -
 *       └─ 第 4 行：0 . = +
 */
class Calculator {
public:
    Calculator() {
        create_ui();
    }

    Grid* get_root() const {
        return root_.get();
    }

private:
    void create_ui() {
        // 根 Grid
        root_ = std::make_unique<Grid>();

        // 定义 5 行 4 列（所有行列 Star 1*，均分）
        for (int i = 0; i < 5; ++i) {
            root_->add_row({GridLength::star()});
        }
        for (int i = 0; i < 4; ++i) {
            root_->add_column({GridLength::star()});
        }

        // 第 0 行：显示屏（跨 4 列）
        auto display = std::make_unique<TextBlock>();
        display->set_text("0");
        display->set_font_size(32);
        display->set_horizontal_alignment(HorizontalAlignment::Right);
        display->set_vertical_alignment(VerticalAlignment::Center);
        display->set_padding(Thickness{10, 10, 10, 10});
        Grid::set_row(*display, 0);
        Grid::set_column(*display, 0);
        Grid::set_column_span(*display, 4);
        root_->add_child(display.get());
        display_ = display.get();
        children_.push_back(std::move(display));

        // 第 1-4 行：按钮
        const char* button_labels[4][4] = {
            {"7", "8", "9", "÷"},
            {"4", "5", "6", "×"},
            {"1", "2", "3", "-"},
            {"0", ".", "=", "+"}
        };

        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                auto btn = std::make_unique<Button>();
                btn->set_text(button_labels[row][col]);
                btn->set_font_size(24);
                btn->on_click([this, label = std::string(button_labels[row][col])]() {
                    on_button_clicked(label);
                });
                Grid::set_row(*btn, row + 1);
                Grid::set_column(*btn, col);
                root_->add_child(btn.get());
                children_.push_back(std::move(btn));
            }
        }
    }

    void on_button_clicked(const std::string& label) {
        std::cout << "按钮点击：" << label << "\n";
        
        if (label >= "0" && label <= "9") {
            // 数字
            if (display_->text() == "0") {
                display_->set_text(label);
            } else {
                display_->set_text(display_->text() + label);
            }
        } else if (label == ".") {
            // 小数点
            if (display_->text().find('.') == std::string::npos) {
                display_->set_text(display_->text() + ".");
            }
        } else if (label == "=") {
            // 计算（简化实现）
            display_->set_text("结果");
        } else {
            // 运算符
            operator_ = label;
            display_->set_text("0");
        }
    }

    std::unique_ptr<Grid> root_;
    std::vector<std::unique_ptr<UIElement>> children_;
    TextBlock* display_ = nullptr;
    std::string operator_;
};

// ═════════════════════════════════════════════════════════════════════════
// 使用示例
// ═════════════════════════════════════════════════════════════════════════

int main() {
    Calculator calc;
    Grid* root = calc.get_root();

    // 测量和排列
    Size available{400, 500};
    root->measure(available);
    
    Size desired = root->desired_size();
    std::cout << "计算器期望尺寸：" << desired.width << " × " << desired.height << "\n";

    Rect final_rect{0, 0, 400, 500};
    root->arrange(final_rect);

    std::cout << "\n布局结果：\n";
    std::cout << "行数：" << root->row_count() << "\n";
    std::cout << "列数：" << root->column_count() << "\n";

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
     * 计算器期望尺寸：400 × 500
     *
     * 布局结果：
     * 行数：5
     * 列数：4
     * 子元素 0：x=0, y=0, w=400, h=100       (显示屏，跨 4 列)
     * 子元素 1：x=0, y=100, w=100, h=100     (按钮 7)
     * 子元素 2：x=100, y=100, w=100, h=100   (按钮 8)
     * 子元素 3：x=200, y=100, w=100, h=100   (按钮 9)
     * 子元素 4：x=300, y=100, w=100, h=100   (按钮 ÷)
     * 子元素 5：x=0, y=200, w=100, h=100     (按钮 4)
     * ...
     * 子元素 20：x=300, y=400, w=100, h=100  (按钮 +)
     *
     * （所有行列均分，每个单元格 100×100）
     */

    return 0;
}
```

**示例说明**：
1. **Calculator**：使用 Grid 实现计算器界面
2. **5 行 4 列**：第 0 行显示屏（跨 4 列），第 1-4 行按钮
3. **所有行列 Star 1***：均分空间，每个单元格尺寸相同
4. **按钮点击事件**：on_button_clicked 处理数字和运算符输入
5. **输出**：显示布局结果和子元素位置

---

## 总结

**核心要点**：
1. **Grid 是行列网格布局面板**，将空间划分为行和列。
2. **支持三种尺寸**：Pixel（固定像素）、Auto（自动）、Star（比例分配）。
3. **附加属性**：RowProperty、ColumnProperty、RowSpanProperty、ColumnSpanProperty。
4. **跨行列**：子元素可跨越多行或多列。
5. **布局算法**：Pixel → Auto → Star 依次计算行列尺寸。
6. **使用静态方法**：Grid::set_row / set_column / set_row_span / set_column_span。
7. **适合复杂布局**：表单、仪表板、应用主窗口等。

**使用建议**：
- ✅ 使用 GridLength 静态工厂方法（pixel / auto_ / star）
- ✅ 使用静态方法设置附加属性
- ✅ 合理使用 Star 比例实现响应式布局
- ✅ 使用 Auto 实现内容驱动布局
- ✅ 确保跨行列不超出范围
- ❌ 不要忘记添加行列定义
- ❌ 不要使用超出范围的行列索引
- ❌ 不要所有列都使用固定像素（除非确实需要）
