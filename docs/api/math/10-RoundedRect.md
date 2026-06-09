# RoundedRect 详细接口文档

## 概述

`RoundedRect` 是 `mine.math` 模块的 **带圆角的轴对齐矩形** 类型，四个角使用相同的椭圆圆角半径。圆角半径会自动限制在矩形可容纳范围内。

**核心特性：**
- **简化圆角**：四角相同的椭圆圆角（`rx`, `ry`）
- **自动约束**：构造时自动限制半径不超过矩形尺寸的一半
- **点包含测试**：精确的椭圆方程判断
- **平移操作**：保持圆角半径不变

---

## 文件位置

```
src/mine/math/api/include/mine/math/RoundedRect.h
```

---

## 类定义

```cpp
namespace mine::math {

struct RoundedRect {
    Rect rect{};
    float radius_x{0.0f};
    float radius_y{0.0f};

    // 构造函数
    constexpr RoundedRect() noexcept = default;
    constexpr RoundedRect(Rect rect_value, float radius) noexcept;
    constexpr RoundedRect(Rect rect_value, float radius_x_value, float radius_y_value) noexcept;

    // 查询
    [[nodiscard]] constexpr Rect bounds() const noexcept;

    // 变换操作
    [[nodiscard]] constexpr RoundedRect translated(Vec2 delta) const noexcept;

    // 几何判断
    [[nodiscard]] constexpr bool contains(Point point) const noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const RoundedRect&) const noexcept = default;
};

}
```

---

## 成员变量

### rect / radius_x / radius_y

```cpp
Rect rect{};
float radius_x{0.0f};
float radius_y{0.0f};
```

**描述**：
- `rect`：轴对齐矩形
- `radius_x`：椭圆圆角的水平半径（自动约束 ≤ `rect.width * 0.5`）
- `radius_y`：椭圆圆角的垂直半径（自动约束 ≤ `rect.height * 0.5`）

---

## 构造函数

### 默认构造函数

```cpp
constexpr RoundedRect() noexcept = default;
```

**描述**：创建零矩形，无圆角。

---

### 圆形圆角构造函数

```cpp
constexpr RoundedRect(Rect rect_value, float radius) noexcept;
```

**描述**：创建圆形圆角矩形（`radius_x == radius_y == radius`）。

**参数**：
- `rect_value`：矩形
- `radius`：圆形圆角半径

**自动约束**：
```cpp
radius_x = clamp(radius, 0, rect.width * 0.5)
radius_y = clamp(radius, 0, rect.height * 0.5)
```

**示例**：
```cpp
Rect rect{0, 0, 100, 50};
RoundedRect rr{rect, 10.0f};  // 圆角半径 10
// 实际半径: rx = 10, ry = 10（未超过限制）

RoundedRect rr2{rect, 60.0f};  // 请求半径 60（超过限制）
// 实际半径: rx = 50, ry = 25（自动约束）
```

---

### 椭圆圆角构造函数

```cpp
constexpr RoundedRect(Rect rect_value, float radius_x_value, float radius_y_value) noexcept;
```

**描述**：创建椭圆圆角矩形（`radius_x` 和 `radius_y` 可不同）。

**参数**：
- `rect_value`：矩形
- `radius_x_value`：水平半径
- `radius_y_value`：垂直半径

**自动约束**：
```cpp
radius_x = clamp(radius_x_value, 0, rect.width * 0.5)
radius_y = clamp(radius_y_value, 0, rect.height * 0.5)
```

**示例**：
```cpp
Rect rect{0, 0, 100, 50};
RoundedRect rr{rect, 20.0f, 10.0f};  // 椭圆圆角
// 实际半径: rx = 20, ry = 10
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
[[nodiscard]] constexpr RoundedRect translated(Vec2 delta) const noexcept;
```

**描述**：平移圆角矩形（圆角半径不变）。

**返回值**：`{rect.translated(delta), radius_x, radius_y}`

**示例**：
```cpp
RoundedRect rr{{0, 0, 100, 50}, 10.0f};
RoundedRect moved = rr.translated({50, 25});
// moved.rect == {50, 25, 100, 50}
// moved.radius_x == 10, moved.radius_y == 10（半径不变）
```

---

## 几何判断

### contains()

```cpp
[[nodiscard]] constexpr bool contains(Point point) const noexcept;
```

**描述**：判断点是否在圆角矩形内（精确椭圆方程判断）。

**算法**：
1. 若点不在外接矩形内：返回 `false`
2. 若无圆角（`radius_x <= 0 || radius_y <= 0`）：返回 `true`
3. 若点在中心十字区域（不涉及圆角）：返回 `true`
4. 否则，判断点是否在对应角的椭圆内：
   ```
   dx = (point.x - center_x) / radius_x
   dy = (point.y - center_y) / radius_y
   dx² + dy² <= 1
   ```

**时间复杂度**：O(1)

**示例**：
```cpp
RoundedRect rr{{0, 0, 100, 50}, 10.0f};

assert(rr.contains({50, 25}));   // ✅ 中心点
assert(rr.contains({10, 10}));   // ✅ 圆角边缘（椭圆上）
assert(!rr.contains({2, 2}));    // ❌ 圆角外部
```

---

## 比较运算符

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const RoundedRect&) const noexcept = default;
```

**描述**：逐成员精确比较。

---

## 使用场景

### 1. UI 控件圆角

```cpp
class Button {
    RoundedRect shape_;
    
public:
    Button(Rect bounds, float corner_radius)
        : shape_{bounds, corner_radius}
    {}
    
    bool hit_test(Point mouse_pos) const {
        return shape_.contains(mouse_pos);
    }
    
    void render(Canvas& canvas) {
        canvas.fill_rounded_rect(shape_, bg_color_);
    }
};
```

---

### 2. 碰撞检测

```cpp
bool check_collision(RoundedRect player, RoundedRect enemy) {
    // 简化：检查外接矩形相交
    if (!player.bounds().intersects(enemy.bounds())) {
        return false;
    }
    
    // 精确：采样边界点
    Point corners[] = {
        player.bounds().top_left(),
        player.bounds().top_right(),
        player.bounds().bottom_left(),
        player.bounds().bottom_right()
    };
    
    for (Point corner : corners) {
        if (enemy.contains(corner)) {
            return true;
        }
    }
    return false;
}
```

---

### 3. 窗口形状

```cpp
class RoundedWindow {
    RoundedRect shape_;
    
public:
    void set_shape(Size size, float corner_radius) {
        Rect rect{0, 0, size.width, size.height};
        shape_ = {rect, corner_radius};
    }
    
    bool is_inside_window(Point screen_pos) const {
        return shape_.contains(screen_pos);
    }
};
```

---

## 性能分析

### 内存布局

```cpp
sizeof(RoundedRect) == 24  // sizeof(Rect) + 2 * sizeof(float) = 16 + 8
alignof(RoundedRect) == 4  // float 对齐
```

---

### 运算性能

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | 2 次 `clamp` |
| `bounds()` | O(1) | 直接返回 |
| `translated()` | O(1) | 1 次 `Rect::translated` |
| `contains()` | O(1) | 最多 8 次比较 + 2 次浮点运算 |

---

## 最佳实践

### 1. 自动约束无需手动检查

```cpp
// ✅ 推荐：直接使用，构造函数自动约束
RoundedRect rr{{0, 0, 20, 20}, 15.0f};
// 自动约束为 rx=10, ry=10

// ❌ 不推荐：手动 clamp
float r = std::min(15.0f, 20.0f * 0.5f);
RoundedRect rr2{{0, 0, 20, 20}, r};
```

---

### 2. 使用 contains() 而非手动判断

```cpp
// ✅ 推荐：使用 contains()
if (rr.contains(mouse_pos)) {
    handle_click();
}

// ❌ 不推荐：手动判断（错误：未处理圆角）
if (rr.bounds().contains(mouse_pos)) {
    handle_click();  // 错误：圆角外部也会触发
}
```

---

### 3. 圆形圆角 vs 椭圆圆角

```cpp
// 圆形圆角（推荐用于正方形或接近正方形的矩形）
RoundedRect circular{{0, 0, 100, 100}, 20.0f};

// 椭圆圆角（用于宽高比悬殊的矩形）
RoundedRect elliptical{{0, 0, 200, 50}, 25.0f, 10.0f};
```

---

## 常见陷阱

### 1. 圆角半径过大会被自动约束

```cpp
Rect small{0, 0, 20, 20};
RoundedRect rr{small, 15.0f};  // 请求半径 15
// 实际半径：10（自动约束为 width/2 和 height/2 的最小值）

assert(rr.radius_x == 10.0f);
assert(rr.radius_y == 10.0f);
```

---

### 2. translated() 不重新约束半径

```cpp
RoundedRect rr{{0, 0, 100, 50}, 10.0f};
RoundedRect moved = rr.translated({0, 0});  // 平移到原位置

// 半径不变（不重新约束）
assert(moved.radius_x == 10.0f);
assert(moved.radius_y == 10.0f);
```

---

## 完整示例

### 示例：圆角按钮

```cpp
#include <mine/math/RoundedRect.h>
#include <mine/math/Color.h>

class RoundedButton {
    RoundedRect shape_;
    Color bg_color_{Color::from_rgb_u32(0x2196F3)};  // 蓝色
    Color hover_color_{Color::from_rgb_u32(0x1976D2)};  // 深蓝
    Color active_color_{Color::from_rgb_u32(0x0D47A1)};  // 更深蓝
    bool is_hovered_{false};
    bool is_pressed_{false};
    
public:
    RoundedButton(Rect bounds, float corner_radius = 5.0f)
        : shape_{bounds, corner_radius}
    {}
    
    void on_mouse_move(Point mouse_pos) {
        is_hovered_ = shape_.contains(mouse_pos);
    }
    
    void on_mouse_down(Point mouse_pos) {
        if (shape_.contains(mouse_pos)) {
            is_pressed_ = true;
        }
    }
    
    void on_mouse_up() {
        if (is_pressed_ && is_hovered_) {
            on_clicked();
        }
        is_pressed_ = false;
    }
    
    void render(Canvas& canvas) {
        Color bg = bg_color_;
        if (is_pressed_) {
            bg = active_color_;
        } else if (is_hovered_) {
            bg = hover_color_;
        }
        
        canvas.fill_rounded_rect(shape_, bg);
        
        // 绘制文本（居中）
        Point center = shape_.bounds().center();
        canvas.draw_text("Click Me", center, Color::White);
    }
    
private:
    virtual void on_clicked() {
        // 子类实现点击逻辑
    }
};

// 使用
void example() {
    Rect bounds{100, 100, 150, 40};
    RoundedButton btn{bounds, 8.0f};  // 圆角半径 8
    
    // 事件处理
    btn.on_mouse_move({175, 120});  // 悬停
    btn.on_mouse_down({175, 120});  // 按下
    btn.on_mouse_up();              // 释放 → 触发点击
}
```

---

## 总结

`RoundedRect` 是 `mine.math` 模块的简化圆角矩形类型，具备：

1. **简化圆角**：四角相同的椭圆圆角
2. **自动约束**：构造时自动限制半径不超过矩形尺寸
3. **精确判断**：`contains()` 使用椭圆方程判断
4. **高性能**：24 字节，O(1) 所有操作

**使用建议**：
- 适合四角相同圆角的场景（大多数 UI 控件）
- 利用自动约束，无需手动检查半径
- 使用 `contains()` 进行精确的点包含测试
- 四角需要不同圆角时使用 `ComplexRoundedRect`
