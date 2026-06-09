# Point 详细接口文档

## 概述

`Point` 是 `mine.math` 模块的 **二维点** 类型,语义上表示 **位置** 而非方向。与 `Vec2` 区分开来,`Point` 在类型系统中明确标识位置坐标,提升代码可读性和类型安全。

**核心特性:**
- **POD 类型**:Plain Old Data,可直接内存拷贝,`constexpr` 支持
- **语义明确**:表示位置而非偏移,类型安全
- **与 Vec2 互转**:`to_vec2()` 和构造函数
- **点-向量运算**:点 ± 向量 = 点,点 - 点 = 向量

**与 Vec2 的区别:**
- **Point**:位置坐标（受平移影响）
- **Vec2**:偏移、方向、缩放系数（表示变化量）

---

## 文件位置

```
src/mine/math/api/include/mine/math/Point.h
```

---

## 类定义

```cpp
namespace mine::math {

struct Point {
    float x{0.0f};
    float y{0.0f};

    // 构造函数
    constexpr Point() noexcept = default;
    constexpr Point(float x_value, float y_value) noexcept;
    explicit constexpr Point(Vec2 value) noexcept;

    // 静态工厂函数
    [[nodiscard]] static constexpr Point zero() noexcept;

    // 类型转换
    [[nodiscard]] constexpr Vec2 to_vec2() const noexcept;

    // 点-向量运算
    [[nodiscard]] constexpr Point operator+(Vec2 delta) const noexcept;
    [[nodiscard]] constexpr Point operator-(Vec2 delta) const noexcept;
    [[nodiscard]] constexpr Vec2 operator-(Point rhs) const noexcept;

    constexpr Point& operator+=(Vec2 delta) noexcept;
    constexpr Point& operator-=(Vec2 delta) noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const Point&) const noexcept = default;
};

using Point2f = Point;

}
```

---

## 成员变量

### x / y

```cpp
float x{0.0f};
float y{0.0f};
```

**描述**:点的 X 和 Y 坐标,公开访问。

**默认值**:`0.0f`（原点）

**示例**:
```cpp
Point p{100.0f, 200.0f};
std::cout << "Position: " << p.x << ", " << p.y;
```

---

## 构造函数

### 默认构造函数

```cpp
constexpr Point() noexcept = default;
```

**描述**:创建原点 `(0, 0)`。

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Point origin;
static_assert(origin.x == 0.0f && origin.y == 0.0f);
```

---

### 坐标构造函数

```cpp
constexpr Point(float x_value, float y_value) noexcept;
```

**参数**:
- `x_value`:X 坐标
- `y_value`:Y 坐标

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Point window_top_left{0.0f, 0.0f};
constexpr Point widget_pos{100.0f, 50.0f};
```

---

### Vec2 构造函数

```cpp
explicit constexpr Point(Vec2 value) noexcept;
```

**参数**:
- `value`:Vec2 向量,其 x/y 转为点坐标

**描述**:`explicit` 防止隐式转换,需显式调用。

**时间复杂度**:O(1)

**示例**:
```cpp
Vec2 offset{50.0f, 100.0f};
Point pos{offset};  // 显式转换

// Point bad = offset;  // ❌ 编译错误：隐式转换被禁止
```

---

## 静态工厂函数

### zero()

```cpp
[[nodiscard]] static constexpr Point zero() noexcept;
```

**描述**:返回原点 `(0, 0)`。

**返回值**:`Point{0.0f, 0.0f}`

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Point origin = Point::zero();
```

---

## 类型转换

### to_vec2()

```cpp
[[nodiscard]] constexpr Vec2 to_vec2() const noexcept;
```

**描述**:转换为 `Vec2` 向量。

**返回值**:`Vec2{x, y}`

**时间复杂度**:O(1)

**示例**:
```cpp
Point p{100.0f, 200.0f};
Vec2 v = p.to_vec2();  // {100.0f, 200.0f}
```

---

## 点-向量运算

### operator+ (点 + 向量 = 点)

```cpp
[[nodiscard]] constexpr Point operator+(Vec2 delta) const noexcept;
```

**描述**:点沿向量移动。

**参数**:
- `delta`:偏移向量

**返回值**:`Point{x + delta.x, y + delta.y}`

**时间复杂度**:O(1)

**示例**:
```cpp
Point pos{100.0f, 200.0f};
Vec2 offset{10.0f, -5.0f};
Point new_pos = pos + offset;  // {110.0f, 195.0f}
```

---

### operator- (点 - 向量 = 点)

```cpp
[[nodiscard]] constexpr Point operator-(Vec2 delta) const noexcept;
```

**描述**:点沿向量反向移动。

**参数**:
- `delta`:偏移向量

**返回值**:`Point{x - delta.x, y - delta.y}`

**时间复杂度**:O(1)

**示例**:
```cpp
Point pos{100.0f, 200.0f};
Vec2 offset{10.0f, 5.0f};
Point new_pos = pos - offset;  // {90.0f, 195.0f}
```

---

### operator- (点 - 点 = 向量)

```cpp
[[nodiscard]] constexpr Vec2 operator-(Point rhs) const noexcept;
```

**描述**:计算从 `rhs` 到 `this` 的位移向量。

**参数**:
- `rhs`:另一个点

**返回值**:`Vec2{x - rhs.x, y - rhs.y}`

**时间复杂度**:O(1)

**示例**:
```cpp
Point from{100.0f, 200.0f};
Point to{150.0f, 250.0f};
Vec2 displacement = to - from;  // {50.0f, 50.0f}
```

---

### operator+=

```cpp
constexpr Point& operator+=(Vec2 delta) noexcept;
```

**描述**:就地沿向量移动。

**参数**:
- `delta`:偏移向量

**返回值**:`*this`

**时间复杂度**:O(1)

**示例**:
```cpp
Point pos{100.0f, 200.0f};
Vec2 velocity{1.0f, 2.0f};
pos += velocity * dt;  // 物理更新
```

---

### operator-=

```cpp
constexpr Point& operator-=(Vec2 delta) noexcept;
```

**描述**:就地沿向量反向移动。

**参数**:
- `delta`:偏移向量

**返回值**:`*this`

**时间复杂度**:O(1)

---

## 比较运算符

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const Point&) const noexcept = default;
```

**描述**:逐坐标精确比较。

**返回值**:`x == rhs.x && y == rhs.y`

**时间复杂度**:O(1)

**注意**:浮点精确比较,可能因舍入误差失败。

---

## 使用场景

### 1. UI 控件位置

```cpp
class Widget {
    Point position_{0.0f, 0.0f};
    Size size_{100.0f, 50.0f};
    
public:
    void move_by(Vec2 delta) {
        position_ += delta;  // 点 + 向量
    }
    
    Point center() const {
        return position_ + Vec2{size_.width / 2.0f, size_.height / 2.0f};
    }
};
```

---

### 2. 鼠标事件处理

```cpp
void on_mouse_move(Point mouse_pos) {
    Vec2 delta = mouse_pos - last_mouse_pos_;  // 点 - 点 = 向量
    camera_offset_ += delta;
    last_mouse_pos_ = mouse_pos;
}
```

---

### 3. 边界检测

```cpp
bool is_inside_window(Point pos, Rect window) {
    return pos.x >= window.x && pos.x <= window.x + window.width &&
           pos.y >= window.y && pos.y <= window.y + window.height;
}
```

---

### 4. 路径点插值

```cpp
Point lerp(Point a, Point b, float t) {
    Vec2 dir = b - a;  // 点 - 点 = 向量
    return a + dir * t;  // 点 + 向量 = 点
}
```

---

## 性能分析

### 内存布局

```cpp
sizeof(Point) == 8  // 2 * sizeof(float)
alignof(Point) == 4  // float 对齐
```

**与 Vec2 二进制兼容**:
```cpp
static_assert(sizeof(Point) == sizeof(Vec2));
static_assert(alignof(Point) == alignof(Vec2));
```

---

### 运算性能

| 操作 | 时间复杂度 | 周期数（估算） | 说明 |
|------|-----------|-------------|------|
| 构造 | O(1) | ~1 | 寄存器赋值 |
| `operator+` | O(1) | ~2 | 2 次浮点加法 |
| `operator-` (点-点) | O(1) | ~2 | 2 次浮点减法 |
| `to_vec2()` | O(1) | ~0 | 零开销转换 |

---

## 最佳实践

### 1. 使用类型区分位置和偏移

```cpp
// ✅ 推荐：类型明确
Point position{100.0f, 200.0f};  // 位置
Vec2 velocity{10.0f, -5.0f};     // 速度（偏移）
position += velocity * dt;

// ❌ 不推荐：混用 Vec2 表示位置
Vec2 pos{100.0f, 200.0f};  // 不明确：是位置还是偏移？
```

---

### 2. 点-向量运算遵循语义

```cpp
// ✅ 正确：点 + 向量 = 点
Point new_pos = position + offset;

// ✅ 正确：点 - 点 = 向量
Vec2 displacement = target - current;

// ❌ 错误：点 + 点 无意义（编译错误）
// Point bad = pos1 + pos2;  // 编译错误：没有 operator+(Point, Point)
```

---

### 3. 使用 explicit 构造防止意外转换

```cpp
// ✅ 正确：显式转换
Vec2 offset{50.0f, 100.0f};
Point pos{offset};

// ❌ 编译错误：隐式转换被禁止
// Point bad = offset;  // 编译错误
```

---

## 常见陷阱

### 1. 点-点运算返回向量

```cpp
Point a{100.0f, 200.0f};
Point b{150.0f, 250.0f};
Vec2 diff = b - a;  // 返回 Vec2，不是 Point

// ❌ 错误：不能赋值给 Point
// Point bad = b - a;  // 类型不匹配
```

---

### 2. 浮点精度比较

```cpp
// ❌ 不推荐：精确比较
Point p1{1.0f / 3.0f, 2.0f / 3.0f};
Point p2{0.333333f, 0.666666f};
bool equal = (p1 == p2);  // 可能为 false（舍入误差）

// ✅ 推荐：误差范围比较
bool approx_equal(Point a, Point b, float epsilon = 1e-5f) {
    return (a - b).to_vec2().length_squared() < epsilon * epsilon;
}
```

---

## 完整示例

### 示例：拖拽系统

```cpp
#include <mine/math/Point.h>
#include <mine/math/Vec2.h>

class DragHandler {
    Point drag_start_{Point::zero()};
    Point object_initial_pos_{Point::zero()};
    bool dragging_{false};
    
public:
    void begin_drag(Point mouse_pos, Point object_pos) {
        dragging_ = true;
        drag_start_ = mouse_pos;
        object_initial_pos_ = object_pos;
    }
    
    Point update_drag(Point current_mouse_pos) {
        if (!dragging_) {
            return object_initial_pos_;
        }
        
        // 计算鼠标移动的偏移量（向量）
        Vec2 mouse_delta = current_mouse_pos - drag_start_;
        
        // 应用偏移到对象初始位置
        return object_initial_pos_ + mouse_delta;
    }
    
    void end_drag() {
        dragging_ = false;
    }
};
```

---

## 总结

`Point` 是 `mine.math` 模块的二维点类型,具备:

1. **语义明确**:表示位置而非偏移,类型安全
2. **POD 类型**:高性能,可 `constexpr`,ABI 稳定
3. **点-向量运算**:点 ± 向量 = 点,点 - 点 = 向量
4. **与 Vec2 互转**:`to_vec2()` 和 `explicit` 构造函数

**使用建议**:
- 用 `Point` 表示位置,用 `Vec2` 表示偏移/方向
- 点-向量运算遵循几何语义
- `explicit` 构造防止意外类型转换
- 浮点比较使用误差范围
