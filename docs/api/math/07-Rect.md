# Rect 详细接口文档

## 概述

`Rect` 是 `mine.math` 模块的 **轴对齐矩形** 类型,表示二维平面上的矩形区域。使用 `(x, y, width, height)` 表示法（左上角 + 尺寸）。

**核心特性:**
- **POD 类型**:Plain Old Data,可直接内存拷贝,`constexpr` 支持
- **轴对齐**:边平行于坐标轴,不支持旋转（旋转矩形需要其他类型）
- **丰富操作**:包含判断、相交、合并、膨胀、收缩、平移等操作
- **CSS Box Model**:配合 `Thickness` 支持 inflate/deflate 四边独立调整

---

## 文件位置

```
src/mine/math/api/include/mine/math/Rect.h
```

---

## 类定义

```cpp
namespace mine::math {

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    // 构造函数
    constexpr Rect() noexcept = default;
    constexpr Rect(float x_value, float y_value, 
                   float width_value, float height_value) noexcept;
    constexpr Rect(Point origin, Size size) noexcept;

    // 静态工厂函数
    [[nodiscard]] static constexpr Rect from_points(Point a, Point b) noexcept;

    // 边界访问
    [[nodiscard]] constexpr float left() const noexcept;
    [[nodiscard]] constexpr float top() const noexcept;
    [[nodiscard]] constexpr float right() const noexcept;
    [[nodiscard]] constexpr float bottom() const noexcept;

    // 查询
    [[nodiscard]] constexpr bool empty() const noexcept;
    [[nodiscard]] constexpr float area() const noexcept;

    // 属性访问
    [[nodiscard]] constexpr Point origin() const noexcept;
    [[nodiscard]] constexpr Size size() const noexcept;
    [[nodiscard]] constexpr Point top_left() const noexcept;
    [[nodiscard]] constexpr Point top_right() const noexcept;
    [[nodiscard]] constexpr Point bottom_left() const noexcept;
    [[nodiscard]] constexpr Point bottom_right() const noexcept;
    [[nodiscard]] constexpr Point center() const noexcept;

    // 几何判断
    [[nodiscard]] constexpr bool contains(Point point) const noexcept;
    [[nodiscard]] constexpr bool contains(Rect other) const noexcept;
    [[nodiscard]] constexpr bool intersects(Rect other) const noexcept;

    // 变换操作
    [[nodiscard]] constexpr Rect translated(Vec2 delta) const noexcept;
    [[nodiscard]] constexpr Rect inflated(float horizontal, float vertical) const noexcept;
    [[nodiscard]] constexpr Rect deflated(float horizontal, float vertical) const noexcept;
    [[nodiscard]] constexpr Rect inflated(const Thickness& t) const noexcept;
    [[nodiscard]] constexpr Rect deflated(const Thickness& t) const noexcept;

    // 集合操作
    [[nodiscard]] constexpr Rect intersection(Rect other) const noexcept;
    [[nodiscard]] constexpr Rect union_with(Rect other) const noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const Rect&) const noexcept = default;
};

using Rectf = Rect;

}
```

---

## 成员变量

### x / y / width / height

```cpp
float x{0.0f};
float y{0.0f};
float width{0.0f};
float height{0.0f};
```

**描述**:矩形的左上角坐标和尺寸,公开访问。

**坐标系**:通常 y 轴向下（UI 坐标系），向右为 x 正方向,向下为 y 正方向。

---

## 构造函数

### 默认构造函数

```cpp
constexpr Rect() noexcept = default;
```

**描述**:创建零矩形 `(0, 0, 0, 0)`。

---

### 坐标+尺寸构造函数

```cpp
constexpr Rect(float x_value, float y_value, 
               float width_value, float height_value) noexcept;
```

**示例**:
```cpp
constexpr Rect rect{100.0f, 50.0f, 200.0f, 100.0f};
// 左上角 (100, 50),宽 200,高 100
```

---

### Point+Size 构造函数

```cpp
constexpr Rect(Point origin, Size size) noexcept;
```

**示例**:
```cpp
Point origin{100.0f, 50.0f};
Size size{200.0f, 100.0f};
Rect rect{origin, size};
```

---

## 静态工厂函数

### from_points()

```cpp
[[nodiscard]] static constexpr Rect from_points(Point a, Point b) noexcept;
```

**描述**:从两个点构造包含这两个点的最小矩形。

**参数**:
- `a`, `b`:任意两个点（不要求顺序）

**返回值**:包含 `a` 和 `b` 的轴对齐矩形。

**示例**:
```cpp
Point p1{100.0f, 200.0f};
Point p2{300.0f, 150.0f};
Rect rect = Rect::from_points(p1, p2);
// rect == {100.0f, 150.0f, 200.0f, 50.0f}
```

---

## 边界访问

### left() / top() / right() / bottom()

```cpp
[[nodiscard]] constexpr float left() const noexcept;   // 返回 x
[[nodiscard]] constexpr float top() const noexcept;    // 返回 y
[[nodiscard]] constexpr float right() const noexcept;  // 返回 x + width
[[nodiscard]] constexpr float bottom() const noexcept; // 返回 y + height
```

**时间复杂度**:O(1)

**示例**:
```cpp
Rect rect{100.0f, 50.0f, 200.0f, 100.0f};
assert(rect.left() == 100.0f);
assert(rect.top() == 50.0f);
assert(rect.right() == 300.0f);
assert(rect.bottom() == 150.0f);
```

---

## 查询方法

### empty()

```cpp
[[nodiscard]] constexpr bool empty() const noexcept;
```

**描述**:判断矩形是否无效（宽或高 `<= 0`）。

**返回值**:
- `true`：`width <= 0.0f || height <= 0.0f`
- `false`：矩形有效

---

### area()

```cpp
[[nodiscard]] constexpr float area() const noexcept;
```

**描述**:计算矩形面积。

**返回值**:
- 若 `empty()`：`0.0f`
- 否则：`width * height`

---

## 属性访问

### origin() / size()

```cpp
[[nodiscard]] constexpr Point origin() const noexcept;  // {x, y}
[[nodiscard]] constexpr Size size() const noexcept;     // {width, height}
```

---

### 角点访问

```cpp
[[nodiscard]] constexpr Point top_left() const noexcept;      // {left(), top()}
[[nodiscard]] constexpr Point top_right() const noexcept;     // {right(), top()}
[[nodiscard]] constexpr Point bottom_left() const noexcept;   // {left(), bottom()}
[[nodiscard]] constexpr Point bottom_right() const noexcept;  // {right(), bottom()}
```

**示例**:
```cpp
Rect rect{100, 50, 200, 100};
Point tl = rect.top_left();      // {100, 50}
Point br = rect.bottom_right();  // {300, 150}
```

---

### center()

```cpp
[[nodiscard]] constexpr Point center() const noexcept;
```

**描述**:返回矩形中心点。

**返回值**:`{x + width * 0.5f, y + height * 0.5f}`

**示例**:
```cpp
Rect rect{100, 50, 200, 100};
Point c = rect.center();  // {200, 100}
```

---

## 几何判断

### contains(Point)

```cpp
[[nodiscard]] constexpr bool contains(Point point) const noexcept;
```

**描述**:判断点是否在矩形内（**包含边界**）。

**返回值**:`point.x >= left() && point.x <= right() && point.y >= top() && point.y <= bottom()`

**示例**:
```cpp
Rect rect{100, 50, 200, 100};
assert(rect.contains({150, 75}));   // ✅ 内部
assert(rect.contains({100, 50}));   // ✅ 边界
assert(rect.contains({300, 150}));  // ✅ 右下角
assert(!rect.contains({99, 75}));   // ❌ 外部
```

---

### contains(Rect)

```cpp
[[nodiscard]] constexpr bool contains(Rect other) const noexcept;
```

**描述**:判断矩形是否完全包含另一个矩形。

**返回值**:
- 若 `other.empty()`：`true`（空矩形被认为包含）
- 否则：`other` 的四个边界都在 `this` 范围内

**示例**:
```cpp
Rect outer{100, 50, 200, 100};
Rect inner{150, 75, 50, 25};
assert(outer.contains(inner));

Rect overlapping{50, 25, 100, 50};
assert(!outer.contains(overlapping));
```

---

### intersects()

```cpp
[[nodiscard]] constexpr bool intersects(Rect other) const noexcept;
```

**描述**:判断两个矩形是否相交（**不包括仅触碰边界**）。

**返回值**:`right() > other.left() && other.right() > left() && bottom() > other.top() && other.bottom() > top()`

**注意**:边界触碰**不算相交**（使用 `>` 而非 `>=`）。

**示例**:
```cpp
Rect a{100, 50, 100, 100};
Rect b{150, 75, 100, 100};
assert(a.intersects(b));  // ✅ 相交

Rect c{200, 50, 100, 100};
assert(!a.intersects(c));  // ❌ 边界触碰不算相交
```

---

## 变换操作

### translated()

```cpp
[[nodiscard]] constexpr Rect translated(Vec2 delta) const noexcept;
```

**描述**:平移矩形（尺寸不变）。

**返回值**:`{x + delta.x, y + delta.y, width, height}`

**示例**:
```cpp
Rect rect{100, 50, 200, 100};
Rect moved = rect.translated({50, 25});
// moved == {150, 75, 200, 100}
```

---

### inflated() / deflated()

```cpp
[[nodiscard]] constexpr Rect inflated(float horizontal, float vertical) const noexcept;
[[nodiscard]] constexpr Rect deflated(float horizontal, float vertical) const noexcept;
```

**描述**:
- `inflated()`:向外扩展（增大矩形）
- `deflated()`:向内收缩（减小矩形）

**参数**:
- `horizontal`:水平方向各边膨胀/收缩距离
- `vertical`:垂直方向各边膨胀/收缩距离

**返回值**（inflate）:
```cpp
{x - horizontal, y - vertical, 
 width + horizontal * 2, height + vertical * 2}
```

**示例**:
```cpp
Rect rect{100, 50, 200, 100};
Rect bigger = rect.inflated(10, 5);
// bigger == {90, 45, 220, 110}

Rect smaller = rect.deflated(10, 5);
// smaller == {110, 55, 180, 90}
```

---

### inflated(Thickness) / deflated(Thickness)

```cpp
[[nodiscard]] constexpr Rect inflated(const Thickness& t) const noexcept;
[[nodiscard]] constexpr Rect deflated(const Thickness& t) const noexcept;
```

**描述**:按四边各自的厚度膨胀或收缩。

**返回值**（deflate）:
```cpp
{x + t.left, y + t.top,
 width - t.left - t.right,
 height - t.top - t.bottom}
```

**示例**:
```cpp
Rect rect{100, 50, 200, 100};
Thickness padding{10, 5, 15, 8};
Rect inner = rect.deflated(padding);
// inner == {110, 55, 175, 87}
```

---

## 集合操作

### intersection()

```cpp
[[nodiscard]] constexpr Rect intersection(Rect other) const noexcept;
```

**描述**:计算两个矩形的交集。

**返回值**:
- 若不相交：空矩形（`empty() == true`）
- 否则：相交区域

**示例**:
```cpp
Rect a{100, 50, 100, 100};
Rect b{150, 75, 100, 100};
Rect inter = a.intersection(b);
// inter == {150, 75, 50, 75}

Rect c{300, 50, 100, 100};
Rect no_inter = a.intersection(c);
assert(no_inter.empty());
```

---

### union_with()

```cpp
[[nodiscard]] constexpr Rect union_with(Rect other) const noexcept;
```

**描述**:计算两个矩形的最小包围矩形（并集）。

**返回值**:
- 若 `this->empty()`：返回 `other`
- 若 `other.empty()`：返回 `*this`
- 否则：包含两个矩形的最小轴对齐矩形

**示例**:
```cpp
Rect a{100, 50, 100, 100};
Rect b{250, 75, 50, 50};
Rect u = a.union_with(b);
// u == {100, 50, 200, 100}（包含 a 和 b）
```

---

## 比较运算符

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const Rect&) const noexcept = default;
```

**描述**:逐分量精确比较。

---

## 使用场景

### 1. 碰撞检测

```cpp
bool check_collision(Rect player, Rect enemy) {
    return player.intersects(enemy);
}

Rect get_overlap(Rect a, Rect b) {
    Rect overlap = a.intersection(b);
    if (!overlap.empty()) {
        return overlap;
    }
    return {};
}
```

---

### 2. UI 布局

```cpp
class Button {
    Rect bounds_;
    Thickness padding_{5, 5, 5, 5};
    
public:
    void set_bounds(Rect bounds) {
        bounds_ = bounds;
    }
    
    Rect content_area() const {
        return bounds_.deflated(padding_);
    }
    
    bool hit_test(Point mouse_pos) const {
        return bounds_.contains(mouse_pos);
    }
};
```

---

### 3. 窗口管理

```cpp
Rect calculate_window_bounds(Rect screen) {
    Size window_size{800, 600};
    Point center = screen.center();
    Point origin{
        center.x - window_size.width * 0.5f,
        center.y - window_size.height * 0.5f
    };
    return {origin, window_size};
}

bool is_fully_visible(Rect window, Rect screen) {
    return screen.contains(window);
}
```

---

### 4. 脏区管理

```cpp
class DirtyRectManager {
    std::vector<Rect> dirty_rects_;
    
public:
    void mark_dirty(Rect rect) {
        if (rect.empty()) return;
        dirty_rects_.push_back(rect);
    }
    
    Rect get_unified_dirty_rect() const {
        if (dirty_rects_.empty()) {
            return {};
        }
        Rect result = dirty_rects_[0];
        for (size_t i = 1; i < dirty_rects_.size(); ++i) {
            result = result.union_with(dirty_rects_[i]);
        }
        return result;
    }
};
```

---

## 性能分析

### 内存布局

```cpp
sizeof(Rect) == 16  // 4 * sizeof(float)
alignof(Rect) == 4  // float 对齐
```

---

### 运算性能

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | 寄存器赋值 |
| `empty()` / `area()` | O(1) | 简单运算 |
| 属性访问 | O(1) | 加法或直接返回 |
| `contains(Point)` | O(1) | 4 次比较 |
| `contains(Rect)` | O(1) | 4 次比较 + 分支 |
| `intersects()` | O(1) | 4 次比较 |
| `translated()` | O(1) | 2 次加法 |
| `inflated()` / `deflated()` | O(1) | 4 次运算 |
| `intersection()` | O(1) | 8 次比较 + 分支 |
| `union_with()` | O(1) | 4 次比较 + 分支 |

---

## 最佳实践

### 1. 使用类型安全构造函数

```cpp
// ✅ 推荐：使用 Point + Size
Point origin{100, 50};
Size size{200, 100};
Rect rect{origin, size};

// ❌ 不推荐：混用裸数值
Rect rect2{100, 50, 200, 100};  // 不够清晰
```

---

### 2. 使用辅助方法而非直接访问

```cpp
// ✅ 推荐：使用 size()
Size s = rect.size();

// ❌ 不推荐：手动构造
Size s2{rect.width, rect.height};
```

---

### 3. 检查空矩形

```cpp
// ✅ 推荐：检查 empty()
Rect result = a.intersection(b);
if (!result.empty()) {
    handle_overlap(result);
}

// ❌ 不推荐：手动检查
if (result.width > 0 && result.height > 0) {
    handle_overlap(result);
}
```

---

## 常见陷阱

### 1. intersects() 不包括边界触碰

```cpp
Rect a{0, 0, 10, 10};
Rect b{10, 0, 10, 10};  // 右边界触碰
assert(!a.intersects(b));  // ✅ 不相交

// 如需包括边界触碰，使用 intersection().empty()
Rect inter = a.intersection(b);
// inter.empty() == true（源码使用 <= 判断）
```

---

### 2. deflated() 可能产生负尺寸

```cpp
Rect small{0, 0, 10, 10};
Thickness large{15, 15, 15, 15};
Rect result = small.deflated(large);
// result.width == 10 - 15 - 15 == -20（负宽度）
assert(result.empty());
```

---

## 完整示例

```cpp
#include <mine/math/Rect.h>
#include <vector>

class Window {
    Rect bounds_;
    Thickness border_{2, 2, 2, 2};
    Thickness padding_{10, 10, 10, 10};
    std::vector<Rect> children_;
    
public:
    void set_bounds(Rect bounds) {
        bounds_ = bounds;
        layout_children();
    }
    
    Rect client_area() const {
        return bounds_.deflated(border_ + padding_);
    }
    
    bool hit_test(Point mouse_pos) const {
        return bounds_.contains(mouse_pos);
    }
    
    Rect get_dirty_rect() const {
        if (children_.empty()) {
            return bounds_;
        }
        Rect dirty = children_[0];
        for (size_t i = 1; i < children_.size(); ++i) {
            dirty = dirty.union_with(children_[i]);
        }
        return dirty.intersection(bounds_);
    }
    
private:
    void layout_children() {
        Rect client = client_area();
        float y = client.top();
        for (Rect& child : children_) {
            child = {client.left(), y, client.width, 30};
            y += 35;  // 30 height + 5 gap
        }
    }
};
```

---

## 总结

`Rect` 是 `mine.math` 模块的轴对齐矩形类型,具备:

1. **POD 类型**:高性能,可 `constexpr`,ABI 稳定
2. **丰富操作**:包含判断、相交、合并、膨胀、收缩、平移
3. **与 Thickness 配合**:支持四边独立调整
4. **UI/游戏常用**:碰撞检测、布局、脏区管理

**使用建议**:
- 使用 `Point + Size` 构造提高可读性
- 使用辅助方法访问属性
- 注意 `intersects()` 不包括边界触碰
- 检查 `empty()` 避免无效矩形
