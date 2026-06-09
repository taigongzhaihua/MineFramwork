# ComplexRoundedRect 详细接口文档

## 概述

`ComplexRoundedRect` 是 `mine.math` 模块的 **四角各自独立的椭圆圆角矩形** 类型，对应 CSS `border-radius` 完整形式。构造时自动执行 CSS 规范的等比缩放算法，确保相邻角不重叠。

**核心特性：**
- **四角独立**：每个角单独指定椭圆圆角半径
- **CSS 兼容**：符合 CSS `border-radius` 规范的比例钳制算法
- **精确判断**：`contains()` 对每个角独立执行椭圆方程判断
- **自动钳制**：确保圆角几何始终有效（相邻角不重叠）

---

## 文件位置

```
src/mine/math/api/include/mine/math/ComplexRoundedRect.h
```

---

## 类定义

```cpp
namespace mine::math {

struct ComplexRoundedRect {
    Rect rect{};
    CornerRadii radii{};

    // 构造函数
    constexpr ComplexRoundedRect() noexcept = default;
    constexpr ComplexRoundedRect(Rect rect_val, CornerRadii radii_val) noexcept;

    // 查询
    [[nodiscard]] constexpr Rect bounds() const noexcept;

    // 变换操作
    [[nodiscard]] constexpr ComplexRoundedRect translated(Vec2 delta) const noexcept;

    // 几何判断
    [[nodiscard]] constexpr bool contains(Point p) const noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const ComplexRoundedRect&) const noexcept = default;
};

}
```

---

## 成员变量

### rect / radii

```cpp
Rect rect{};
CornerRadii radii{};
```

**描述**：
- `rect`：轴对齐矩形
- `radii`：四角的椭圆圆角半径（**已钳制**）

---

## 构造函数

### 默认构造函数

```cpp
constexpr ComplexRoundedRect() noexcept = default;
```

**描述**：创建零矩形，无圆角。

---

### 矩形+圆角构造函数

```cpp
constexpr ComplexRoundedRect(Rect rect_val, CornerRadii radii_val) noexcept;
```

**描述**：创建圆角矩形，自动执行 CSS 规范的比例钳制。

**参数**：
- `rect_val`：矩形
- `radii_val`：四角的圆角半径（**未钳制**）

**自动钳制算法**（CSS 规范）：
1. 计算每条边上两角半径之和：
   - `top_sum = radii.top_left.x + radii.top_right.x`
   - `right_sum = radii.top_right.y + radii.bottom_right.y`
   - `bottom_sum = radii.bottom_right.x + radii.bottom_left.x`
   - `left_sum = radii.bottom_left.y + radii.top_left.y`

2. 计算缩放因子：
   - `scale_h = min(1.0, rect.width / max(top_sum, bottom_sum))`
   - `scale_v = min(1.0, rect.height / max(left_sum, right_sum))`
   - `scale = min(scale_h, scale_v)`

3. 若 `scale < 1.0`，对所有角按比例均匀缩小：
   ```cpp
   radii.top_left *= scale
   radii.top_right *= scale
   radii.bottom_right *= scale
   radii.bottom_left *= scale
   ```

**示例**：
```cpp
Rect rect{0, 0, 100, 50};
CornerRadii radii = CornerRadii::from_corners(40, 40, 40, 40);
// 左上 + 右上 = 80 > 100（超过宽度）

ComplexRoundedRect crr{rect, radii};
// 自动缩放：scale = 100 / 80 = 1.25
// 实际半径：所有角变为 40 / 1.25 = 32
```

---

## 查询方法

### bounds()

```cpp
[[nodiscard]] constexpr Rect bounds() const noexcept;
```

**描述**：返回外接矩形。

**返回值**：`rect`

---

## 变换操作

### translated()

```cpp
[[nodiscard]] constexpr ComplexRoundedRect translated(Vec2 delta) const noexcept;
```

**描述**：平移圆角矩形（圆角半径不变，**不重新钳制**）。

**返回值**：
```cpp
ComplexRoundedRect result;
result.rect = rect.translated(delta);
result.radii = radii;  // 半径不变
```

**示例**：
```cpp
ComplexRoundedRect crr{{0, 0, 100, 50}, CornerRadii::uniform(10)};
ComplexRoundedRect moved = crr.translated({50, 25});
// moved.rect == {50, 25, 100, 50}
// moved.radii == crr.radii（半径不变）
```

---

## 几何判断

### contains()

```cpp
[[nodiscard]] constexpr bool contains(Point p) const noexcept;
```

**描述**：判断点是否在圆角矩形内（精确椭圆方程判断，每个角独立处理）。

**算法**：
1. 若点不在外接矩形内：返回 `false`
2. 转换为矩形本地坐标（左上角为原点）：
   ```cpp
   lx = p.x - rect.x
   ly = p.y - rect.y
   ```
3. 对四个角分别判断：
   - **左上角**：若 `lx < tl.x && ly < tl.y`，判断椭圆：
     ```cpp
     nx = (lx - tl.x) / tl.x
     ny = (ly - tl.y) / tl.y
     if (nx² + ny² > 1) return false
     ```
   - **右上角**：若 `lx > w - tr.x && ly < tr.y`，判断椭圆
   - **左下角**：若 `lx < bl.x && ly > h - bl.y`，判断椭圆
   - **右下角**：若 `lx > w - br.x && ly > h - br.y`，判断椭圆
4. 若所有角都通过，返回 `true`

**时间复杂度**：O(1)（最多 4 个角的椭圆判断）

**示例**：
```cpp
Rect rect{0, 0, 100, 50};
CornerRadii radii = CornerRadii::from_corners(10, 5, 0, 10);
ComplexRoundedRect crr{rect, radii};

assert(crr.contains({50, 25}));   // ✅ 中心点
assert(crr.contains({10, 10}));   // ✅ 左上角圆角边缘
assert(crr.contains({95, 5}));    // ✅ 右上角圆角边缘
assert(crr.contains({95, 45}));   // ✅ 右下角直角
assert(!crr.contains({2, 2}));    // ❌ 左上角圆角外部
```

---

## 比较运算符

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const ComplexRoundedRect&) const noexcept = default;
```

**描述**：逐成员精确比较。

---

## 使用场景

### 1. CSS border-radius 完整形式

```cpp
// CSS: border-radius: 10px 20px 30px 40px / 5px 10px 15px 20px;
Rect rect{0, 0, 200, 100};
CornerRadii radii{
    {10, 5},   // top-left
    {20, 10},  // top-right
    {30, 15},  // bottom-right
    {40, 20}   // bottom-left
};
ComplexRoundedRect crr{rect, radii};
```

---

### 2. 对话框样式（上方圆角，下方直角）

```cpp
Rect dialog{100, 100, 400, 300};
CornerRadii radii = CornerRadii::from_corners(
    12,  // top-left
    12,  // top-right
    0,   // bottom-right
    0    // bottom-left
);
ComplexRoundedRect crr{dialog, radii};
```

---

### 3. 非对称圆角卡片

```cpp
class Card {
    ComplexRoundedRect shape_;
    
public:
    Card(Rect bounds) {
        // 左侧大圆角，右侧小圆角
        CornerRadii radii{
            {20, 20},  // top-left
            {5, 5},    // top-right
            {5, 5},    // bottom-right
            {20, 20}   // bottom-left
        };
        shape_ = {bounds, radii};
    }
    
    bool hit_test(Point mouse_pos) const {
        return shape_.contains(mouse_pos);
    }
};
```

---

## 性能分析

### 内存布局

```cpp
sizeof(ComplexRoundedRect) == 48  // sizeof(Rect) + sizeof(CornerRadii) = 16 + 32
alignof(ComplexRoundedRect) == 4  // float 对齐
```

---

### 运算性能

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | CSS 钳制算法（~20 次浮点运算） |
| `bounds()` | O(1) | 直接返回 |
| `translated()` | O(1) | 1 次 `Rect::translated` |
| `contains()` | O(1) | 最多 4 个角的椭圆判断（~40 次浮点运算） |

---

## 最佳实践

### 1. 使用 CSS 兼容语法

```cpp
// ✅ 推荐：使用 CornerRadii 构造
CornerRadii radii = CornerRadii::from_corners(10, 5, 0, 10);
ComplexRoundedRect crr{rect, radii};

// ❌ 不推荐：手动构造每个角
ComplexRoundedRect crr2;
crr2.rect = rect;
crr2.radii.top_left = {10, 10};
// ... 冗长且易错
```

---

### 2. 利用自动钳制

```cpp
// ✅ 推荐：直接使用，自动钳制
Rect small{0, 0, 50, 50};
CornerRadii large = CornerRadii::uniform(40);  // 超出限制
ComplexRoundedRect crr{small, large};  // 自动缩放

// ❌ 不推荐：手动计算缩放
// 复杂且易出错
```

---

### 3. 四角相同时使用 RoundedRect

```cpp
// ✅ 推荐：四角相同时使用 RoundedRect（更简单）
RoundedRect rr{rect, 10.0f};

// ❌ 不推荐：不必要地使用 ComplexRoundedRect
ComplexRoundedRect crr{rect, CornerRadii::uniform(10)};  // 冗长
```

---

## 常见陷阱

### 1. translated() 不重新钳制

```cpp
Rect small{0, 0, 50, 50};
CornerRadii radii = CornerRadii::uniform(40);
ComplexRoundedRect crr{small, radii};  // 自动钳制

ComplexRoundedRect moved = crr.translated({100, 100});
// 半径不变（不重新钳制）
```

---

### 2. 圆角半径过大导致缩放

```cpp
Rect rect{0, 0, 100, 50};
CornerRadii radii = CornerRadii::uniform(40);
// top_left.x + top_right.x = 80 < 100（OK）
// top_left.y + bottom_left.y = 80 > 50（超出！）

ComplexRoundedRect crr{rect, radii};
// 自动缩放：scale = 50 / 80 = 0.625
// 实际半径：所有角变为 40 * 0.625 = 25
```

---

## 完整示例

### 示例：高级对话框

```cpp
#include <mine/math/ComplexRoundedRect.h>
#include <mine/math/Color.h>

class FancyDialog {
    ComplexRoundedRect shape_;
    Color bg_color_{Color::White};
    Color border_color_{Color::from_rgba8(200, 200, 200)};
    
public:
    enum class Style {
        Standard,      // 四角相同
        TopRounded,    // 仅上方圆角
        LeftRounded,   // 仅左侧圆角
        Asymmetric     // 非对称
    };
    
    FancyDialog(Rect bounds, Style style)
        : shape_{bounds, calculate_radii(style)}
    {}
    
    void render(Canvas& canvas) {
        // 绘制阴影
        ComplexRoundedRect shadow = shape_.translated({2, 2});
        canvas.fill_rounded_rect(shadow, Color::Black.with_alpha(0.2f));
        
        // 绘制背景
        canvas.fill_rounded_rect(shape_, bg_color_);
        
        // 绘制边框
        canvas.stroke_rounded_rect(shape_, border_color_, 1.0f);
    }
    
    bool hit_test(Point mouse_pos) const {
        return shape_.contains(mouse_pos);
    }
    
private:
    CornerRadii calculate_radii(Style style) {
        switch (style) {
            case Style::Standard:
                return CornerRadii::uniform(8.0f);
            
            case Style::TopRounded:
                return CornerRadii::from_corners(
                    12,  // top-left
                    12,  // top-right
                    0,   // bottom-right
                    0    // bottom-left
                );
            
            case Style::LeftRounded:
                return CornerRadii::from_corners(
                    12,  // top-left
                    0,   // top-right
                    0,   // bottom-right
                    12   // bottom-left
                );
            
            case Style::Asymmetric:
                return CornerRadii{
                    {20, 10},  // top-left (椭圆)
                    {10, 10},  // top-right (圆形)
                    {5, 5},    // bottom-right (小圆)
                    {15, 20}   // bottom-left (椭圆)
                };
        }
        return {};
    }
};

// 使用
void example() {
    Rect bounds{100, 100, 400, 300};
    
    FancyDialog standard{bounds, FancyDialog::Style::Standard};
    FancyDialog top_rounded{bounds, FancyDialog::Style::TopRounded};
    FancyDialog asymmetric{bounds, FancyDialog::Style::Asymmetric};
    
    // 点击测试
    Point mouse{250, 200};
    if (standard.hit_test(mouse)) {
        handle_dialog_click();
    }
}
```

---

## 总结

`ComplexRoundedRect` 是 `mine.math` 模块的复杂圆角矩形类型，具备：

1. **四角独立**：每个角单独指定椭圆圆角半径
2. **CSS 兼容**：符合 CSS `border-radius` 规范的比例钳制算法
3. **精确判断**：`contains()` 对每个角独立执行椭圆方程判断
4. **自动钳制**：确保相邻角不重叠

**使用建议**：
- 四角需要不同圆角时使用（否则用 `RoundedRect`）
- 利用 CSS 规范的自动钳制，无需手动计算
- 使用 `contains()` 进行精确的点包含测试
- 配合 `CornerRadii` 工厂函数提高可读性
