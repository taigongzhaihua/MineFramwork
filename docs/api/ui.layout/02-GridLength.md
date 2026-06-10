# GridLength 详细接口文档

## 概述

`GridLength` 是 `mine.ui.layout` 模块的**Grid 行列尺寸描述符**。

**核心特性：**
- **三种尺寸类型**：Pixel（固定像素）、Auto（自动）、Star（比例分配）
- **WPF 风格**：支持 WPF 样式的网格布局尺寸描述
- **轻量级**：仅 8 字节（float + uint8_t + padding）

**相关类型：**
- **RowDefinition**：行定义（height、min_height、max_height）
- **ColumnDefinition**：列定义（width、min_width、max_width）

---

## 文件位置

```
src/mine/ui/layout/api/include/mine/ui/layout/GridLength.h
```

---

## 类型定义

### GridLength

```cpp
struct GridLength {
    /// 尺寸类型
    enum class Type : uint8_t {
        Pixel = 0,  // 固定像素
        Auto  = 1,  // 自动（内容驱动）
        Star  = 2,  // 比例（剩余空间权重分配）
    };

    float value = 1.0f;       // 像素值或 Star 权重（Auto 时忽略）
    Type  type  = Type::Star; // 默认 1*

    constexpr GridLength() noexcept = default;
    constexpr GridLength(float v, Type t) noexcept;

    // 静态工厂方法
    [[nodiscard]] static constexpr GridLength pixel(float pixels) noexcept;
    [[nodiscard]] static constexpr GridLength auto_() noexcept;
    [[nodiscard]] static constexpr GridLength star(float weight = 1.0f) noexcept;

    // 类型判断
    [[nodiscard]] constexpr bool is_pixel() const noexcept;
    [[nodiscard]] constexpr bool is_auto()  const noexcept;
    [[nodiscard]] constexpr bool is_star()  const noexcept;
};
```

---

### RowDefinition

```cpp
struct RowDefinition {
    GridLength height     = GridLength::star();
    float      min_height = 0.0f;
    float      max_height = std::numeric_limits<float>::infinity();
};
```

---

### ColumnDefinition

```cpp
struct ColumnDefinition {
    GridLength width     = GridLength::star();
    float      min_width = 0.0f;
    float      max_width = std::numeric_limits<float>::infinity();
};
```

---

## GridLength 成员

### 尺寸类型枚举

| 类型 | 值 | 描述 |
|------|---|------|
| `Pixel` | `0` | 固定像素尺寸 |
| `Auto` | `1` | 根据子元素期望尺寸自动确定 |
| `Star` | `2` | 按比例分配剩余空间 |

---

### 静态工厂方法

#### pixel

```cpp
[[nodiscard]] static constexpr GridLength pixel(float pixels) noexcept;
```

**描述**：创建固定像素长度。

**参数**：
- `pixels`：像素值

**返回值**：`GridLength` 实例（type=Pixel, value=pixels）

**示例**：
```cpp
GridLength len = GridLength::pixel(100.0f);  // 100 像素
```

---

#### auto_

```cpp
[[nodiscard]] static constexpr GridLength auto_() noexcept;
```

**描述**：创建自动长度（根据内容自动确定）。

**返回值**：`GridLength` 实例（type=Auto, value=0）

**示例**：
```cpp
GridLength len = GridLength::auto_();  // 自动
```

---

#### star

```cpp
[[nodiscard]] static constexpr GridLength star(float weight = 1.0f) noexcept;
```

**描述**：创建 Star 比例长度（按权重分配剩余空间）。

**参数**：
- `weight`：权重（默认 1.0）

**返回值**：`GridLength` 实例（type=Star, value=weight）

**示例**：
```cpp
GridLength len1 = GridLength::star();       // 1*
GridLength len2 = GridLength::star(2.0f);   // 2*
```

---

### 类型判断方法

```cpp
[[nodiscard]] constexpr bool is_pixel() const noexcept;  // 是否为 Pixel 类型
[[nodiscard]] constexpr bool is_auto()  const noexcept;  // 是否为 Auto 类型
[[nodiscard]] constexpr bool is_star()  const noexcept;  // 是否为 Star 类型
```

---

## 使用场景

### 1. 固定像素列

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>

using namespace mine::ui;

Grid* grid = new Grid();

// 添加列定义：100px, 200px, 300px
grid->add_column({GridLength::pixel(100)});
grid->add_column({GridLength::pixel(200)});
grid->add_column({GridLength::pixel(300)});
```

---

### 2. 自动列（根据内容）

```cpp
// 添加列定义：Auto, Auto, Auto
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});

// 每列宽度自动适应内容
```

---

### 3. 比例列（Star）

```cpp
// 添加列定义：1*, 2*, 1*
grid->add_column({GridLength::star(1.0f)});
grid->add_column({GridLength::star(2.0f)});
grid->add_column({GridLength::star(1.0f)});

// 假设可用宽度 400px：
// 第1列: 400 * (1 / 4) = 100px
// 第2列: 400 * (2 / 4) = 200px
// 第3列: 400 * (1 / 4) = 100px
```

---

### 4. 混合列（Pixel + Auto + Star）

```cpp
// 添加列定义：100px, Auto, 1*
grid->add_column({GridLength::pixel(100)});  // 固定 100px
grid->add_column({GridLength::auto_()});      // 自动
grid->add_column({GridLength::star()});       // 填充剩余空间

// 假设可用宽度 400px，Auto 列内容宽度 50px：
// 第1列: 100px（固定）
// 第2列: 50px（内容宽度）
// 第3列: 400 - 100 - 50 = 250px（剩余空间）
```

---

### 5. 行定义（带最小/最大高度）

```cpp
// 添加行定义：1*，最小 50px，最大 200px
RowDefinition row;
row.height     = GridLength::star();
row.min_height = 50.0f;
row.max_height = 200.0f;

grid->add_row(row);
```

---

### 6. 响应式布局

```cpp
class ResponsiveGrid : public Grid {
public:
    ResponsiveGrid() {
        // 桌面端：固定列宽
        add_column({GridLength::pixel(200)});  // 侧边栏
        add_column({GridLength::star()});       // 主内容区

        // 可动态调整为移动端：
        // add_column({GridLength::star()});    // 全宽
    }
    
    void switch_to_mobile() {
        // 清除旧列定义并重新添加
        // （简化实现，实际需要 clear_columns 方法）
    }
};
```

---

### 7. 判断类型

```cpp
void print_grid_length(const GridLength& len) {
    if (len.is_pixel()) {
        std::cout << "固定: " << len.value << " px" << std::endl;
    } else if (len.is_auto()) {
        std::cout << "自动" << std::endl;
    } else if (len.is_star()) {
        std::cout << "比例: " << len.value << "*" << std::endl;
    }
}

// 使用
print_grid_length(GridLength::pixel(100));  // 输出: 固定: 100 px
print_grid_length(GridLength::auto_());      // 输出: 自动
print_grid_length(GridLength::star(2));      // 输出: 比例: 2*
```

---

## 最佳实践

### 1. 使用静态工厂方法

```cpp
// ✅ 推荐：使用静态工厂方法（语义明确）
GridLength len1 = GridLength::pixel(100);
GridLength len2 = GridLength::auto_();
GridLength len3 = GridLength::star(2.0f);

// ❌ 不推荐：直接构造（不够直观）
GridLength len1(100, GridLength::Type::Pixel);
GridLength len2(0, GridLength::Type::Auto);
GridLength len3(2.0f, GridLength::Type::Star);
```

---

### 2. Auto 用于内容驱动的列

```cpp
// ✅ 推荐：Auto 列用于内容驱动（如文本标签）
grid->add_column({GridLength::auto_()});  // 标签列（自动宽度）
grid->add_column({GridLength::star()});    // 输入框列（填充剩余空间）

// ❌ 不推荐：所有列都用 Auto（可能溢出容器）
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});
grid->add_column({GridLength::auto_()});
```

---

### 3. Star 用于比例分配

```cpp
// ✅ 推荐：Star 用于比例分配（响应式布局）
grid->add_column({GridLength::star(1)});
grid->add_column({GridLength::star(2)});
grid->add_column({GridLength::star(1)});
// 比例：1:2:1

// ❌ 不推荐：所有列都用固定像素（不响应容器尺寸变化）
grid->add_column({GridLength::pixel(100)});
grid->add_column({GridLength::pixel(200)});
grid->add_column({GridLength::pixel(100)});
```

---

### 4. 设置最小/最大尺寸约束

```cpp
// ✅ 推荐：设置最小/最大尺寸约束（避免过小/过大）
RowDefinition row;
row.height     = GridLength::star();
row.min_height = 50.0f;   // 最小 50px
row.max_height = 200.0f;  // 最大 200px
grid->add_row(row);

// ❌ 不推荐：无约束（行可能过小或过大）
RowDefinition row;
row.height = GridLength::star();
// 缺少 min_height / max_height
grid->add_row(row);
```

---

## 常见陷阱

### 1. 混淆 Auto 和 Star

```cpp
// ❌ 问题：混淆 Auto 和 Star
grid->add_column({GridLength::auto_()});  // ❌ 期望填充剩余空间，但实际是内容驱动

// ✅ 解决：使用 Star 填充剩余空间
grid->add_column({GridLength::star()});  // ✅ 填充剩余空间
```

---

### 2. Star 权重为 0

```cpp
// ❌ 问题：Star 权重为 0（列宽为 0）
grid->add_column({GridLength::star(0.0f)});  // ❌ 列宽为 0

// ✅ 解决：使用正权重
grid->add_column({GridLength::star(1.0f)});  // ✅ 列宽正常分配
```

---

### 3. 忘记设置最小高度

```cpp
// ❌ 问题：忘记设置最小高度（行可能过小）
RowDefinition row;
row.height = GridLength::auto_();
// ❌ 无最小高度，内容为空时行高为 0
grid->add_row(row);

// ✅ 解决：设置最小高度
RowDefinition row;
row.height     = GridLength::auto_();
row.min_height = 20.0f;  // ✅ 最小 20px
grid->add_row(row);
```

---

### 4. 所有列都用 Pixel（不响应尺寸变化）

```cpp
// ❌ 问题：所有列都用 Pixel（容器尺寸变化时不响应）
grid->add_column({GridLength::pixel(100)});
grid->add_column({GridLength::pixel(200)});
grid->add_column({GridLength::pixel(300)});
// 总宽度固定为 600px，容器宽度 > 600px 时有空白，< 600px 时溢出

// ✅ 解决：至少一列使用 Star（响应容器尺寸）
grid->add_column({GridLength::pixel(100)});
grid->add_column({GridLength::star()});      // ✅ 填充剩余空间
grid->add_column({GridLength::pixel(300)});
```

---

## 完整示例

```cpp
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/GridLength.h>
#include <mine/ui/controls/Button.h>
#include <iostream>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// 响应式 Grid 布局
// ────────────────────────────────────────────────────────────────────────────

int main() {
    Grid* grid = new Grid();
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 定义列（固定 + 自动 + 比例）
    // ════════════════════════════════════════════════════════════════════════
    
    // 第 1 列：固定 100px（侧边栏）
    grid->add_column({GridLength::pixel(100)});
    
    // 第 2 列：自动宽度（内容驱动）
    grid->add_column({GridLength::auto_()});
    
    // 第 3 列：1* 比例（填充剩余空间）
    grid->add_column({GridLength::star(1.0f)});
    
    // 第 4 列：2* 比例（占剩余空间的 2/3）
    grid->add_column({GridLength::star(2.0f)});
    
    std::cout << "列定义：" << std::endl;
    std::cout << "  列 0: 固定 100px" << std::endl;
    std::cout << "  列 1: Auto（内容驱动）" << std::endl;
    std::cout << "  列 2: 1*（比例）" << std::endl;
    std::cout << "  列 3: 2*（比例）" << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 定义行（带最小/最大高度约束）
    // ════════════════════════════════════════════════════════════════════════
    
    // 第 1 行：固定 50px（标题栏）
    RowDefinition row1;
    row1.height = GridLength::pixel(50);
    grid->add_row(row1);
    
    // 第 2 行：1*（主内容区），最小 100px，最大 500px
    RowDefinition row2;
    row2.height     = GridLength::star(1.0f);
    row2.min_height = 100.0f;
    row2.max_height = 500.0f;
    grid->add_row(row2);
    
    // 第 3 行：Auto（底部工具栏），最小 30px
    RowDefinition row3;
    row3.height     = GridLength::auto_();
    row3.min_height = 30.0f;
    grid->add_row(row3);
    
    std::cout << "\n行定义：" << std::endl;
    std::cout << "  行 0: 固定 50px（标题栏）" << std::endl;
    std::cout << "  行 1: 1*（主内容区），最小 100px，最大 500px" << std::endl;
    std::cout << "  行 2: Auto（工具栏），最小 30px" << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 添加子元素
    // ════════════════════════════════════════════════════════════════════════
    
    // 标题栏（跨所有列）
    Button* title_btn = new Button();
    title_btn->set_content("标题栏");
    Grid::set_row(*title_btn, 0);
    Grid::set_column(*title_btn, 0);
    Grid::set_column_span(*title_btn, 4);  // 跨 4 列
    grid->add_child(title_btn);
    
    // 侧边栏（第 1 列，第 2 行）
    Button* sidebar_btn = new Button();
    sidebar_btn->set_content("侧边栏");
    Grid::set_row(*sidebar_btn, 1);
    Grid::set_column(*sidebar_btn, 0);
    grid->add_child(sidebar_btn);
    
    // 主内容（第 2-4 列，第 2 行）
    Button* content_btn = new Button();
    content_btn->set_content("主内容区");
    Grid::set_row(*content_btn, 1);
    Grid::set_column(*content_btn, 1);
    Grid::set_column_span(*content_btn, 3);  // 跨 3 列
    grid->add_child(content_btn);
    
    // 底部工具栏（跨所有列）
    Button* toolbar_btn = new Button();
    toolbar_btn->set_content("工具栏");
    Grid::set_row(*toolbar_btn, 2);
    Grid::set_column(*toolbar_btn, 0);
    Grid::set_column_span(*toolbar_btn, 4);  // 跨 4 列
    grid->add_child(toolbar_btn);
    
    std::cout << "\n子元素：" << std::endl;
    std::cout << "  标题栏：行 0，列 0-3（跨 4 列）" << std::endl;
    std::cout << "  侧边栏：行 1，列 0" << std::endl;
    std::cout << "  主内容区：行 1，列 1-3（跨 3 列）" << std::endl;
    std::cout << "  工具栏：行 2，列 0-3（跨 4 列）" << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 类型判断示例
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n类型判断：" << std::endl;
    GridLength len1 = GridLength::pixel(100);
    GridLength len2 = GridLength::auto_();
    GridLength len3 = GridLength::star(2.0f);
    
    std::cout << "  len1 is_pixel: " << len1.is_pixel() << std::endl;  // 1
    std::cout << "  len2 is_auto: "  << len2.is_auto()  << std::endl;  // 1
    std::cout << "  len3 is_star: "  << len3.is_star()  << std::endl;  // 1
    
    return 0;
}
```

**运行效果**：
```
列定义：
  列 0: 固定 100px
  列 1: Auto（内容驱动）
  列 2: 1*（比例）
  列 3: 2*（比例）

行定义：
  行 0: 固定 50px（标题栏）
  行 1: 1*（主内容区），最小 100px，最大 500px
  行 2: Auto（工具栏），最小 30px

子元素：
  标题栏：行 0，列 0-3（跨 4 列）
  侧边栏：行 1，列 0
  主内容区：行 1，列 1-3（跨 3 列）
  工具栏：行 2，列 0-3（跨 4 列）

类型判断：
  len1 is_pixel: 1
  len2 is_auto: 1
  len3 is_star: 1
```

---

## 总结

`GridLength` 是 `mine.ui.layout` 模块的 Grid 行列尺寸描述符，具备：

1. **三种尺寸类型**：
   - **Pixel**：固定像素尺寸
   - **Auto**：根据子元素期望尺寸自动确定
   - **Star**：按权重比例分配剩余空间
2. **相关类型**：
   - **RowDefinition**：行定义（height、min_height、max_height）
   - **ColumnDefinition**：列定义（width、min_width、max_width）
3. **静态工厂方法**：`pixel()`, `auto_()`, `star()`
4. **类型判断**：`is_pixel()`, `is_auto()`, `is_star()`

**使用建议**：
- **使用静态工厂方法**（语义明确）
- **Auto 用于内容驱动的列**
- **Star 用于比例分配**（响应式布局）
- **设置最小/最大尺寸约束**（避免过小/过大）
- 避免混淆 Auto 和 Star
- 避免 Star 权重为 0
- 避免忘记设置最小高度
- 避免所有列都用 Pixel（不响应尺寸变化）
