# CornerRadii 详细接口文档

## 概述

`CornerRadii` 是 `mine.math` 模块的 **四角独立椭圆圆角半径** 类型，表示矩形四个角各自的椭圆圆角半径。每个角使用 `(rx, ry)` 表示椭圆的水平和垂直半轴。

**核心特性：**
- **四角独立**：top_left, top_right, bottom_right, bottom_left 各自指定
- **椭圆圆角**：每角用 `Vec2{rx, ry}` 表示，支持非对称圆角
- **快捷构造**：`uniform()` 四角相同，`from_corners()` 对称圆角
- **查询方法**：`is_zero()`, `is_uniform()`
- **CSS 兼容**：对应 CSS `border-radius` 完整形式

---

## 文件位置

```
src/mine/math/api/include/mine/math/CornerRadii.h
```

---

## 类定义

```cpp
namespace mine::math {

struct CornerRadii {
    Vec2 top_left{};
    Vec2 top_right{};
    Vec2 bottom_right{};
    Vec2 bottom_left{};

    // 构造函数
    constexpr CornerRadii() noexcept = default;
    constexpr CornerRadii(Vec2 tl, Vec2 tr, Vec2 br, Vec2 bl) noexcept;

    // 静态工厂函数
    [[nodiscard]] static constexpr CornerRadii uniform(float r) noexcept;
    [[nodiscard]] static constexpr CornerRadii uniform(float rx, float ry) noexcept;
    [[nodiscard]] static constexpr CornerRadii from_corners(float tl, float tr, float br, float bl) noexcept;

    // 查询
    [[nodiscard]] constexpr bool is_zero() const noexcept;
    [[nodiscard]] constexpr bool is_uniform() const noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const CornerRadii&) const noexcept = default;
};

}
```

---

## 成员变量

### top_left / top_right / bottom_right / bottom_left

```cpp
Vec2 top_left{};
Vec2 top_right{};
Vec2 bottom_right{};
Vec2 bottom_left{};
```

**描述**：矩形四个角的椭圆圆角半径，顺时针顺序。

**语义**：
- `Vec2{rx, ry}`：椭圆的水平和垂直半轴
- `rx == ry`：退化为圆形圆角
- `rx == 0 || ry == 0`：退化为直角

**默认值**：`Vec2{0, 0}`（无圆角）

---

## 构造函数

### 默认构造函数

```cpp
constexpr CornerRadii() noexcept = default;
```

**描述**：创建无圆角（四角全零）。

---

### 四角构造函数

```cpp
constexpr CornerRadii(Vec2 tl, Vec2 tr, Vec2 br, Vec2 bl) noexcept;
```

**参数**：
- `tl`, `tr`, `br`, `bl`：左上、右上、右下、左下角的椭圆半径

**示例**：
```cpp
CornerRadii radii{
    {10.0f, 10.0f},  // 左上：圆形
    {20.0f, 10.0f},  // 右上：椭圆（宽 > 高）
    {10.0f, 20.0f},  // 右下：椭圆（高 > 宽）
    {0.0f, 0.0f}     // 左下：直角
};
```

---

## 静态工厂函数

### uniform(float)

```cpp
[[nodiscard]] static constexpr CornerRadii uniform(float r) noexcept;
```

**描述**：创建四角完全相同的圆形圆角（`rx == ry == r`）。

**返回值**：`CornerRadii{{r, r}, {r, r}, {r, r}, {r, r}}`

**示例**：
```cpp
CornerRadii radii = CornerRadii::uniform(10.0f);
// 四角都是半径 10 的圆形圆角
```

---

### uniform(float, float)

```cpp
[[nodiscard]] static constexpr CornerRadii uniform(float rx, float ry) noexcept;
```

**描述**：创建四角完全相同的椭圆圆角（`rx` 和 `ry` 可不同）。

**返回值**：`CornerRadii{{rx, ry}, {rx, ry}, {rx, ry}, {rx, ry}}`

**示例**：
```cpp
CornerRadii radii = CornerRadii::uniform(20.0f, 10.0f);
// 四角都是宽 20、高 10 的椭圆圆角
```

---

### from_corners()

```cpp
[[nodiscard]] static constexpr CornerRadii from_corners(float tl, float tr, float br, float bl) noexcept;
```

**描述**：从四个圆形圆角半径创建（每角 `rx == ry`）。

**参数**：
- `tl`, `tr`, `br`, `bl`：四个角的圆形半径

**返回值**：`CornerRadii{{tl, tl}, {tr, tr}, {br, br}, {bl, bl}}`

**示例**：
```cpp
CornerRadii radii = CornerRadii::from_corners(10.0f, 5.0f, 0.0f, 10.0f);
// 左上和左下：半径 10
// 右上：半径 5
// 右下：直角
```

---

## 查询方法

### is_zero()

```cpp
[[nodiscard]] constexpr bool is_zero() const noexcept;
```

**描述**：判断是否四角全为零（无圆角）。

**返回值**：
- `true`：所有角的 `rx <= 0 && ry <= 0`
- `false`：至少有一个角有圆角

**示例**：
```cpp
CornerRadii zero;
assert(zero.is_zero());

CornerRadii non_zero = CornerRadii::uniform(5.0f);
assert(!non_zero.is_zero());
```

---

### is_uniform()

```cpp
[[nodiscard]] constexpr bool is_uniform() const noexcept;
```

**描述**：判断是否四角完全相同。

**返回值**：
- `true`：`top_left == top_right == bottom_right == bottom_left`
- `false`：四角不完全相同

**示例**：
```cpp
CornerRadii uniform = CornerRadii::uniform(10.0f);
assert(uniform.is_uniform());

CornerRadii asymmetric = CornerRadii::from_corners(10, 5, 10, 5);
assert(!asymmetric.is_uniform());
```

---

## 比较运算符

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const CornerRadii&) const noexcept = default;
```

**描述**：逐角精确比较（每个角的 `Vec2` 都相等）。

---

## 使用场景

### 1. UI 控件圆角

```cpp
class Button {
    Rect bounds_;
    CornerRadii radii_ = CornerRadii::uniform(5.0f);
    
public:
    void set_corner_radii(CornerRadii radii) {
        radii_ = radii;
    }
    
    void render(Canvas& canvas) {
        canvas.draw_rounded_rect(bounds_, radii_);
    }
};

// 使用
Button btn;
btn.set_corner_radii(CornerRadii::uniform(8.0f));  // 圆角按钮
```

---

### 2. CSS border-radius 完整形式

```cpp
// CSS: border-radius: 10px 20px 30px 40px / 5px 10px 15px 20px;
CornerRadii css_radii{
    {10.0f, 5.0f},   // top-left
    {20.0f, 10.0f},  // top-right
    {30.0f, 15.0f},  // bottom-right
    {40.0f, 20.0f}   // bottom-left
};
```

---

### 3. 对话框样式

```cpp
CornerRadii dialog_style() {
    // 上方圆角，下方直角（类似 macOS 对话框）
    return CornerRadii::from_corners(
        10.0f,  // top-left
        10.0f,  // top-right
        0.0f,   // bottom-right
        0.0f    // bottom-left
    );
}
```

---

### 4. 椭圆圆角（非对称）

```cpp
CornerRadii pill_shape(Size size) {
    float rx = size.height * 0.5f;  // 水平半径 = 高度的一半
    float ry = size.height * 0.5f;  // 垂直半径 = 高度的一半
    return CornerRadii::uniform(rx, ry);
}

// 使用：创建胶囊形状按钮
Size button_size{100.0f, 40.0f};
CornerRadii pill = pill_shape(button_size);  // 左右两端半圆
```

---

## 性能分析

### 内存布局

```cpp
sizeof(CornerRadii) == 32  // 4 * sizeof(Vec2) = 4 * 8
alignof(CornerRadii) == 4  // Vec2 对齐
```

---

### 运算性能

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | 寄存器赋值 |
| `uniform()` | O(1) | 广播赋值 |
| `is_zero()` | O(1) | 8 次比较 + 逻辑与 |
| `is_uniform()` | O(1) | 3 次 `Vec2` 比较 |

---

## 最佳实践

### 1. 使用静态工厂函数

```cpp
// ✅ 推荐：语义清晰
CornerRadii radii = CornerRadii::uniform(10.0f);
CornerRadii corners = CornerRadii::from_corners(10, 5, 0, 10);

// ❌ 不推荐：冗长
CornerRadii radii2{{10, 10}, {10, 10}, {10, 10}, {10, 10}};
```

---

### 2. 检查零圆角优化渲染

```cpp
void render_rounded_rect(Rect rect, CornerRadii radii) {
    if (radii.is_zero()) {
        // 优化路径：直接绘制矩形
        draw_rect(rect);
    } else {
        // 复杂路径：绘制圆角矩形
        draw_rounded_rect(rect, radii);
    }
}
```

---

### 3. 圆形圆角 vs 椭圆圆角

```cpp
// 圆形圆角（推荐用于小控件）
CornerRadii circular = CornerRadii::uniform(8.0f);

// 椭圆圆角（用于特殊形状）
CornerRadii elliptical = CornerRadii::uniform(20.0f, 10.0f);
```

---

## 常见陷阱

### 1. 圆角半径过大

```cpp
Rect small{0, 0, 20.0f, 20.0f};
CornerRadii large = CornerRadii::uniform(15.0f);
// 四角半径之和 = 30，超过矩形宽度 20
// 需要配合 ComplexRoundedRect 自动钳制
```

---

### 2. 忘记椭圆半径的含义

```cpp
// ❌ 错误理解：以为 Vec2{10, 20} 是宽和高
CornerRadii radii{{10, 20}, ...};  // 实际是 rx=10, ry=20（椭圆半轴）

// ✅ 正确理解：Vec2 是椭圆的两个半轴
```

---

## 完整示例

### 示例：卡片样式

```cpp
#include <mine/math/CornerRadii.h>
#include <mine/math/Rect.h>

class Card {
    Rect bounds_;
    CornerRadii radii_;
    Color bg_color_;
    
public:
    enum class Style {
        Flat,      // 无圆角
        Rounded,   // 小圆角
        Pill,      // 胶囊形
        Custom     // 自定义
    };
    
    Card(Rect bounds, Style style)
        : bounds_{bounds}
        , radii_{calculate_radii(style)}
        , bg_color_{Color::White}
    {}
    
    void render(Canvas& canvas) {
        if (radii_.is_zero()) {
            canvas.fill_rect(bounds_, bg_color_);
        } else {
            ComplexRoundedRect rr{bounds_, radii_};
            canvas.fill_rounded_rect(rr, bg_color_);
        }
    }
    
private:
    CornerRadii calculate_radii(Style style) {
        switch (style) {
            case Style::Flat:
                return {};  // 零圆角
            
            case Style::Rounded:
                return CornerRadii::uniform(8.0f);
            
            case Style::Pill: {
                float r = bounds_.height * 0.5f;
                return CornerRadii::uniform(r);
            }
            
            case Style::Custom:
                // 上方大圆角，下方小圆角
                return CornerRadii::from_corners(
                    12.0f,  // top-left
                    12.0f,  // top-right
                    4.0f,   // bottom-right
                    4.0f    // bottom-left
                );
        }
        return {};
    }
};

// 使用
void example() {
    Card flat{{0, 0, 200, 100}, Card::Style::Flat};
    Card rounded{{0, 110, 200, 100}, Card::Style::Rounded};
    Card pill{{0, 220, 200, 40}, Card::Style::Pill};
    Card custom{{0, 270, 200, 100}, Card::Style::Custom};
}
```

---

## 总结

`CornerRadii` 是 `mine.math` 模块的四角独立椭圆圆角半径类型，具备：

1. **四角独立**：每个角单独指定 `(rx, ry)` 椭圆半径
2. **椭圆支持**：每角可以是椭圆圆角（`rx != ry`）
3. **快捷构造**：`uniform()` 和 `from_corners()` 工厂函数
4. **CSS 兼容**：对应 CSS `border-radius` 完整形式

**使用建议**：
- 使用 `uniform()` 和 `from_corners()` 提高可读性
- 使用 `is_zero()` 优化无圆角渲染
- 配合 `ComplexRoundedRect` 自动钳制圆角半径
- 区分圆形圆角（`rx == ry`）和椭圆圆角（`rx != ry`）
