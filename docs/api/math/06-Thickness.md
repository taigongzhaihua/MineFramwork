# Thickness 详细接口文档

## 概述

`Thickness` 是 `mine.math` 模块的 **四边独立厚度** 类型,表示矩形四边（左、上、右、下）各自独立的厚度值。对应 CSS box model 中的 border-width, padding, margin 等概念,常与 `Rect` 配合使用。

**核心特性:**
- **POD 类型**:Plain Old Data,可直接内存拷贝,`constexpr` 支持
- **四边独立**:left, top, right, bottom 各自指定
- **快捷构造**:`uniform()` 四边相同,`symmetric()` 水平/垂直对称
- **查询方法**:`horizontal()`, `vertical()`, `is_zero()`, `is_uniform()`
- **与 Rect 配合**:用于 `Rect::inflated()` / `deflated()` 操作

---

## 文件位置

```
src/mine/math/api/include/mine/math/Thickness.h
```

---

## 类定义

```cpp
namespace mine::math {

struct Thickness {
    float left{};
    float top{};
    float right{};
    float bottom{};

    // 构造函数
    constexpr Thickness() noexcept = default;
    constexpr Thickness(float left_val, float top_val,
                        float right_val, float bottom_val) noexcept;

    // 静态工厂函数
    [[nodiscard]] static constexpr Thickness uniform(float v) noexcept;
    [[nodiscard]] static constexpr Thickness symmetric(float horizontal, 
                                                        float vertical) noexcept;

    // 查询
    [[nodiscard]] constexpr float horizontal() const noexcept;
    [[nodiscard]] constexpr float vertical() const noexcept;
    [[nodiscard]] constexpr bool is_zero() const noexcept;
    [[nodiscard]] constexpr bool is_uniform() const noexcept;

    // 运算符
    [[nodiscard]] constexpr Thickness operator+(const Thickness& other) const noexcept;
    [[nodiscard]] constexpr Thickness operator*(float scale) const noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const Thickness&) const noexcept = default;
};

}
```

---

## 成员变量

### left / top / right / bottom

```cpp
float left{};
float top{};
float right{};
float bottom{};
```

**描述**:矩形四边的厚度值,公开访问。

**默认值**:`0.0f`（零厚度）

**语义**:
- **正值**:向内收缩（deflate）或向外扩展（inflate）的距离
- **负值**:反向操作（语义上等效于反向 inflate/deflate）
- **通常非负**:大多数场景使用正值

**示例**:
```cpp
Thickness padding{10.0f, 5.0f, 10.0f, 5.0f};  // 左右 10，上下 5
Thickness border{1.0f, 1.0f, 1.0f, 1.0f};      // 四边 1
```

---

## 构造函数

### 默认构造函数

```cpp
constexpr Thickness() noexcept = default;
```

**描述**:创建零厚度 `(0, 0, 0, 0)`。

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Thickness zero;
static_assert(zero.is_zero());
```

---

### 四边构造函数

```cpp
constexpr Thickness(float left_val, float top_val,
                    float right_val, float bottom_val) noexcept;
```

**参数**:
- `left_val`:左边厚度
- `top_val`:上边厚度
- `right_val`:右边厚度
- `bottom_val`:下边厚度

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Thickness padding{10.0f, 5.0f, 10.0f, 5.0f};
constexpr Thickness margin{20.0f, 10.0f, 20.0f, 10.0f};
```

---

## 静态工厂函数

### uniform()

```cpp
[[nodiscard]] static constexpr Thickness uniform(float v) noexcept;
```

**描述**:创建四边相同的厚度。

**参数**:
- `v`:四边统一厚度值

**返回值**:`Thickness{v, v, v, v}`

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Thickness border = Thickness::uniform(2.0f);
// border == {2.0f, 2.0f, 2.0f, 2.0f}

constexpr Thickness padding = Thickness::uniform(10.0f);
```

---

### symmetric()

```cpp
[[nodiscard]] static constexpr Thickness symmetric(float horizontal, float vertical) noexcept;
```

**描述**:创建水平对称和垂直对称的厚度。

**参数**:
- `horizontal`:左右两边的厚度
- `vertical`:上下两边的厚度

**返回值**:`Thickness{horizontal, vertical, horizontal, vertical}`

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Thickness padding = Thickness::symmetric(10.0f, 5.0f);
// padding == {10.0f, 5.0f, 10.0f, 5.0f}（左右 10，上下 5）
```

---

## 查询方法

### horizontal()

```cpp
[[nodiscard]] constexpr float horizontal() const noexcept;
```

**描述**:返回水平方向总厚度（左 + 右）。

**返回值**:`left + right`

**时间复杂度**:O(1)

**示例**:
```cpp
Thickness t{10.0f, 5.0f, 15.0f, 8.0f};
float h = t.horizontal();  // 25.0f（10 + 15）
```

---

### vertical()

```cpp
[[nodiscard]] constexpr float vertical() const noexcept;
```

**描述**:返回垂直方向总厚度（上 + 下）。

**返回值**:`top + bottom`

**时间复杂度**:O(1)

**示例**:
```cpp
Thickness t{10.0f, 5.0f, 15.0f, 8.0f};
float v = t.vertical();  // 13.0f（5 + 8）
```

---

### is_zero()

```cpp
[[nodiscard]] constexpr bool is_zero() const noexcept;
```

**描述**:判断是否四边全为零。

**返回值**:
- `true`：`left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f`
- `false`：至少有一边非零

**时间复杂度**:O(1)

**示例**:
```cpp
Thickness zero;
assert(zero.is_zero());

Thickness padding{10.0f, 0.0f, 0.0f, 0.0f};
assert(!padding.is_zero());
```

---

### is_uniform()

```cpp
[[nodiscard]] constexpr bool is_uniform() const noexcept;
```

**描述**:判断是否四边相同。

**返回值**:
- `true`：`left == top && top == right && right == bottom`
- `false`：四边不完全相同

**时间复杂度**:O(1)

**示例**:
```cpp
Thickness uniform = Thickness::uniform(5.0f);
assert(uniform.is_uniform());

Thickness asymmetric{10.0f, 5.0f, 10.0f, 5.0f};
assert(!asymmetric.is_uniform());
```

---

## 运算符

### operator+

```cpp
[[nodiscard]] constexpr Thickness operator+(const Thickness& other) const noexcept;
```

**描述**:厚度相加（分量相加）。

**参数**:
- `other`:右操作数

**返回值**:
```cpp
Thickness{
    left + other.left, 
    top + other.top,
    right + other.right, 
    bottom + other.bottom
}
```

**时间复杂度**:O(1)

**示例**:
```cpp
Thickness padding{10.0f, 5.0f, 10.0f, 5.0f};
Thickness border{2.0f, 2.0f, 2.0f, 2.0f};
Thickness total = padding + border;
// total == {12.0f, 7.0f, 12.0f, 7.0f}
```

---

### operator*

```cpp
[[nodiscard]] constexpr Thickness operator*(float scale) const noexcept;
```

**描述**:厚度缩放（分量乘法）。

**参数**:
- `scale`:缩放因子

**返回值**:
```cpp
Thickness{
    left * scale, 
    top * scale,
    right * scale, 
    bottom * scale
}
```

**时间复杂度**:O(1)

**示例**:
```cpp
Thickness base{10.0f, 5.0f, 10.0f, 5.0f};
Thickness doubled = base * 2.0f;
// doubled == {20.0f, 10.0f, 20.0f, 10.0f}
```

---

## 比较运算符

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const Thickness&) const noexcept = default;
```

**描述**:逐分量精确比较。

**返回值**:`left == rhs.left && top == rhs.top && right == rhs.right && bottom == rhs.bottom`

**时间复杂度**:O(1)

---

## 使用场景

### 1. 与 Rect 配合使用

```cpp
Rect content_rect{100.0f, 100.0f, 200.0f, 100.0f};
Thickness padding{10.0f, 5.0f, 10.0f, 5.0f};

// 向内收缩（减去 padding）
Rect inner = content_rect.deflated(padding);
// inner == {110.0f, 105.0f, 180.0f, 90.0f}

// 向外扩展（加上 border）
Thickness border{2.0f, 2.0f, 2.0f, 2.0f};
Rect outer = content_rect.inflated(border);
// outer == {98.0f, 98.0f, 204.0f, 104.0f}
```

---

### 2. CSS Box Model

```cpp
struct BoxModel {
    Rect content;
    Thickness padding;
    Thickness border;
    Thickness margin;
    
    Rect padding_box() const {
        return content.inflated(padding);
    }
    
    Rect border_box() const {
        return content.inflated(padding + border);
    }
    
    Rect margin_box() const {
        return content.inflated(padding + border + margin);
    }
    
    Size total_size() const {
        float width = content.width + padding.horizontal() + 
                      border.horizontal() + margin.horizontal();
        float height = content.height + padding.vertical() + 
                       border.vertical() + margin.vertical();
        return {width, height};
    }
};
```

---

### 3. UI 布局计算

```cpp
class Panel {
    Rect bounds_;
    Thickness padding_{10.0f, 10.0f, 10.0f, 10.0f};
    
public:
    Rect client_area() const {
        return bounds_.deflated(padding_);
    }
    
    void set_padding(Thickness p) {
        padding_ = p;
        layout_children();
    }
    
    Size calculate_min_size(Size content_size) const {
        return {
            content_size.width + padding_.horizontal(),
            content_size.height + padding_.vertical()
        };
    }
};
```

---

### 4. 九宫格图片裁切

```cpp
struct NinePatch {
    Thickness slice_thickness;  // 边缘厚度
    
    void render(Rect dest_rect, Texture& texture) {
        Size tex_size = texture.size();
        
        // 四个角
        Rect top_left_src{0, 0, slice_thickness.left, slice_thickness.top};
        Rect top_left_dst{dest_rect.x, dest_rect.y, 
                          slice_thickness.left, slice_thickness.top};
        
        // 四条边（拉伸）
        float center_width = dest_rect.width - 
                             slice_thickness.horizontal();
        Rect top_edge_src{slice_thickness.left, 0, 
                          tex_size.width - slice_thickness.horizontal(),
                          slice_thickness.top};
        Rect top_edge_dst{dest_rect.x + slice_thickness.left, dest_rect.y,
                          center_width, slice_thickness.top};
        
        // ... 渲染其他部分
    }
};
```

---

## 性能分析

### 内存布局

```cpp
sizeof(Thickness) == 16  // 4 * sizeof(float)
alignof(Thickness) == 4  // float 对齐
```

---

### 运算性能

| 操作 | 时间复杂度 | 周期数（估算） | 说明 |
|------|-----------|-------------|------|
| 构造 | O(1) | ~1 | 寄存器赋值 |
| `uniform()` | O(1) | ~1 | 广播赋值 |
| `horizontal()` / `vertical()` | O(1) | ~1 | 1 次加法 |
| `is_zero()` | O(1) | ~4 | 4 次比较 + 逻辑与 |
| `is_uniform()` | O(1) | ~3 | 3 次比较 + 逻辑与 |
| `operator+` | O(1) | ~4 | 4 次加法 |
| `operator*` | O(1) | ~4 | 4 次乘法 |

---

## 最佳实践

### 1. 使用静态工厂函数

```cpp
// ✅ 推荐：语义清晰
Thickness padding = Thickness::uniform(10.0f);
Thickness margin = Thickness::symmetric(20.0f, 10.0f);

// ❌ 不推荐：冗长
Thickness padding2{10.0f, 10.0f, 10.0f, 10.0f};
Thickness margin2{20.0f, 10.0f, 20.0f, 10.0f};
```

---

### 2. 使用 horizontal() / vertical() 计算总尺寸

```cpp
// ✅ 推荐：使用辅助方法
float total_width = content_width + padding.horizontal();
float total_height = content_height + padding.vertical();

// ❌ 不推荐：手动计算
float total_width2 = content_width + padding.left + padding.right;
float total_height2 = content_height + padding.top + padding.bottom;
```

---

### 3. 负厚度用于反向操作

```cpp
Rect rect{100, 100, 200, 100};
Thickness inset{-10.0f, -5.0f, -10.0f, -5.0f};

// deflate 负厚度 = inflate 正厚度
Rect expanded = rect.deflated(inset);
// 等价于 rect.inflated({10.0f, 5.0f, 10.0f, 5.0f})
```

---

## 常见陷阱

### 1. 过大的厚度导致矩形退化

```cpp
Rect small{0, 0, 20.0f, 10.0f};
Thickness large{15.0f, 8.0f, 15.0f, 8.0f};

Rect result = small.deflated(large);
// result 可能为空或负尺寸（取决于 Rect::deflated 实现）
```

---

### 2. 混淆 horizontal/vertical 和 left/right

```cpp
Thickness t{10.0f, 5.0f, 15.0f, 8.0f};

// ❌ 错误：以为 horizontal() 返回 left 和 right 的平均值
float avg = t.horizontal();  // 25.0（不是 12.5）

// ✅ 正确：horizontal() 返回 left + right
float sum = t.horizontal();  // 25.0
```

---

## 完整示例

### 示例：CSS Box Model 计算

```cpp
#include <mine/math/Thickness.h>
#include <mine/math/Rect.h>

class CSSBox {
    Rect content_box_;
    Thickness padding_;
    Thickness border_;
    Thickness margin_;
    
public:
    CSSBox(Rect content, Thickness padding, 
           Thickness border, Thickness margin)
        : content_box_{content}
        , padding_{padding}
        , border_{border}
        , margin_{margin}
    {}
    
    Rect content_box() const { return content_box_; }
    
    Rect padding_box() const {
        return content_box_.inflated(padding_);
    }
    
    Rect border_box() const {
        return content_box_.inflated(padding_ + border_);
    }
    
    Rect margin_box() const {
        return content_box_.inflated(padding_ + border_ + margin_);
    }
    
    Size outer_size() const {
        Rect outer = margin_box();
        return outer.size();
    }
    
    bool contains_point(Point p) const {
        return margin_box().contains(p);
    }
};

// 使用示例
void example() {
    Rect content{100, 100, 200, 100};
    Thickness padding = Thickness::uniform(10.0f);
    Thickness border = Thickness::uniform(2.0f);
    Thickness margin = Thickness::symmetric(20.0f, 10.0f);
    
    CSSBox box{content, padding, border, margin};
    
    Size outer = box.outer_size();
    // outer.width = 200 + 10*2 + 2*2 + 20*2 = 264
    // outer.height = 100 + 10*2 + 2*2 + 10*2 = 144
}
```

---

## 总结

`Thickness` 是 `mine.math` 模块的四边独立厚度类型,具备:

1. **POD 类型**:高性能,可 `constexpr`,ABI 稳定
2. **四边独立**:left, top, right, bottom 各自指定
3. **快捷构造**:`uniform()` 和 `symmetric()` 工厂函数
4. **与 Rect 配合**:用于 inflate/deflate 操作

**使用建议**:
- 使用 `uniform()` 和 `symmetric()` 提高可读性
- 使用 `horizontal()` 和 `vertical()` 计算总尺寸
- 负值可用于反向操作
- 注意过大厚度可能导致矩形退化
