# Transform2D 详细接口文档

## 概述

`Transform2D` 是 `mine.math` 模块的 **2D 仿射变换包装类型**，内部使用 `Mat3` 实现，提供更高级的 2D 变换 API。

**核心特性：**
- **Mat3 包装**：内部使用 `Mat3` 存储变换矩阵
- **流畅 API**：提供语义化的变换工厂函数
- **围绕点旋转**：`rotation_about()` 支持绕任意点旋转
- **多类型应用**：可应用于 `Point`、`Vec2`、`Rect`

---

## 文件位置

```
src/mine/math/api/include/mine/math/Transform2D.h
```

---

## 类定义

```cpp
namespace mine::math {

struct Transform2D {
    Mat3 matrix_;  // 内部变换矩阵

    // 构造函数
    constexpr Transform2D() noexcept = default;
    explicit constexpr Transform2D(const Mat3& matrix_value) noexcept;

    // 静态工厂函数
    [[nodiscard]] static constexpr Transform2D identity() noexcept;
    [[nodiscard]] static constexpr Transform2D translation(float tx, float ty) noexcept;
    [[nodiscard]] static constexpr Transform2D translation(Vec2 offset) noexcept;
    [[nodiscard]] static constexpr Transform2D scale(float uniform_scale) noexcept;
    [[nodiscard]] static constexpr Transform2D scale(float sx, float sy) noexcept;
    [[nodiscard]] static Transform2D rotation(float radians_value) noexcept;
    [[nodiscard]] static Transform2D rotation_about(float radians_value, Point pivot) noexcept;

    // 访问内部矩阵
    [[nodiscard]] constexpr const Mat3& matrix() const noexcept;

    // 应用变换
    [[nodiscard]] constexpr Point apply(Point point) const noexcept;
    [[nodiscard]] constexpr Vec2 apply(Vec2 vector) const noexcept;
    [[nodiscard]] Rect apply(Rect rect) const noexcept;

    // 逆变换
    [[nodiscard]] mine::core::Result<Transform2D> inverted(
        float epsilon = kDefaultEpsilon) const noexcept;

    // 组合变换
    [[nodiscard]] constexpr Transform2D operator*(const Transform2D& rhs) const noexcept;
    constexpr Transform2D& operator*=(const Transform2D& rhs) noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const Transform2D&) const noexcept = default;
};

}
```

---

## 成员变量

### matrix_

```cpp
Mat3 matrix_;
```

**描述**：内部 3x3 变换矩阵。

**默认值**：单位矩阵

---

## 构造函数

### 默认构造函数

```cpp
constexpr Transform2D() noexcept = default;
```

**描述**：创建单位变换（不做任何变换）。

---

### 从 Mat3 构造

```cpp
explicit constexpr Transform2D(const Mat3& matrix_value) noexcept;
```

**描述**：从现有 `Mat3` 创建变换。

---

## 静态工厂函数

### identity()

```cpp
[[nodiscard]] static constexpr Transform2D identity() noexcept;
```

**描述**：创建单位变换。

---

### translation()

```cpp
[[nodiscard]] static constexpr Transform2D translation(float tx, float ty) noexcept;
[[nodiscard]] static constexpr Transform2D translation(Vec2 offset) noexcept;
```

**描述**：创建平移变换。

**示例**：
```cpp
Transform2D t1 = Transform2D::translation(10, 20);
Transform2D t2 = Transform2D::translation(Vec2{10, 20});

Point p{5, 5};
Point moved = t1.apply(p);  // {15, 25}
```

---

### scale()

```cpp
[[nodiscard]] static constexpr Transform2D scale(float uniform_scale) noexcept;
[[nodiscard]] static constexpr Transform2D scale(float sx, float sy) noexcept;
```

**描述**：创建缩放变换。

**示例**：
```cpp
Transform2D s1 = Transform2D::scale(2.0f);  // 统一缩放
Transform2D s2 = Transform2D::scale(2.0f, 3.0f);  // 各向异性缩放

Vec2 v{10, 10};
Vec2 scaled = s2.apply(v);  // {20, 30}
```

---

### rotation()

```cpp
[[nodiscard]] static Transform2D rotation(float radians_value) noexcept;
```

**描述**：创建旋转变换（逆时针，绕原点）。

**示例**：
```cpp
float angle = std::numbers::pi_v<float> / 2;  // 90度
Transform2D r = Transform2D::rotation(angle);

Vec2 v{1, 0};
Vec2 rotated = r.apply(v);  // {0, 1}
```

---

### rotation_about()

```cpp
[[nodiscard]] static Transform2D rotation_about(float radians_value, Point pivot) noexcept;
```

**描述**：创建绕指定点旋转的变换。

**实现**：
```cpp
translation(pivot) * rotation(radians) * translation(-pivot)
```

**示例**：
```cpp
Point center{100, 100};
float angle = std::numbers::pi_v<float> / 4;  // 45度
Transform2D r = Transform2D::rotation_about(angle, center);

// 围绕 (100, 100) 旋转 45 度
Point p{150, 100};
Point rotated = r.apply(p);
```

---

## 应用变换

### apply(Point)

```cpp
[[nodiscard]] constexpr Point apply(Point point) const noexcept;
```

**描述**：应用变换到点（受平移影响）。

**实现**：`matrix_.transform_point(point)`

---

### apply(Vec2)

```cpp
[[nodiscard]] constexpr Vec2 apply(Vec2 vector) const noexcept;
```

**描述**：应用变换到向量（**不受平移影响**）。

**实现**：`matrix_.transform_vector(vector)`

---

### apply(Rect)

```cpp
[[nodiscard]] Rect apply(Rect rect) const noexcept;
```

**描述**：应用变换到矩形，返回变换后四个角的**轴对齐包围盒（AABB）**。

**实现**：
1. 变换矩形四个角
2. 计算 AABB（`min(x)`, `min(y)`, `max(x) - min(x)`, `max(y) - min(y)`）

**示例**：
```cpp
Rect rect{0, 0, 100, 50};
Transform2D r = Transform2D::rotation(std::numbers::pi_v<float> / 4);

Rect aabb = r.apply(rect);
// aabb 是旋转后四个角的轴对齐包围盒
```

**注意**：返回的是 AABB，不是旋转后的矩形本身。

---

## 逆变换

### inverted()

```cpp
[[nodiscard]] mine::core::Result<Transform2D> inverted(
    float epsilon = kDefaultEpsilon) const noexcept;
```

**描述**：计算逆变换。

**返回值**：
- 成功：包含逆变换的 `Result<Transform2D>`
- 失败（矩阵不可逆）：包含错误状态

**示例**：
```cpp
Transform2D t = Transform2D::translation(10, 20);
auto inv_result = t.inverted();

if (inv_result) {
    Transform2D inv = inv_result.value();
    Point p{15, 25};
    Point original = inv.apply(p);  // {5, 5}
}
```

---

## 组合变换

### operator*

```cpp
[[nodiscard]] constexpr Transform2D operator*(const Transform2D& rhs) const noexcept;
constexpr Transform2D& operator*=(const Transform2D& rhs) noexcept;
```

**描述**：组合两个变换（`this * rhs`）。

**语义**：先应用 `rhs`，再应用 `this`

**示例**：
```cpp
Transform2D t = Transform2D::translation(100, 0);
Transform2D r = Transform2D::rotation(std::numbers::pi_v<float> / 2);
Transform2D combined = t * r;  // 先旋转，再平移

Point p{10, 0};
Point result = combined.apply(p);
// 等价于：t.apply(r.apply(p))
```

---

## 使用场景

### 1. UI 控件变换

```cpp
class Widget {
    Point position_{100, 100};
    float rotation_ = 0;
    Vec2 scale_{1, 1};
    
public:
    Transform2D get_transform() const {
        return Transform2D::translation(position_.to_vec2()) *
               Transform2D::rotation(rotation_) *
               Transform2D::scale(scale_.x, scale_.y);
    }
    
    bool hit_test(Point screen_pos) {
        auto inv = get_transform().inverted();
        if (!inv) return false;
        
        Point local_pos = inv.value().apply(screen_pos);
        return local_bounds_.contains(local_pos);
    }
};
```

---

### 2. 围绕中心旋转

```cpp
Rect bounds{100, 100, 200, 100};
Point center = bounds.center();
float angle = std::numbers::pi_v<float> / 6;  // 30度

// 围绕矩形中心旋转
Transform2D transform = Transform2D::rotation_about(angle, center);

Rect aabb = transform.apply(bounds);
// aabb 是旋转后的包围盒
```

---

### 3. 相机变换

```cpp
Transform2D calculate_camera_transform(Point camera_pos, float zoom) {
    return Transform2D::scale(zoom) *
           Transform2D::translation(-camera_pos.to_vec2());
}

// 使用
Point world_pos{500, 300};
Transform2D view = calculate_camera_transform({250, 150}, 2.0f);
Point screen_pos = view.apply(world_pos);
```

---

## 性能分析

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | 内联赋值 |
| `apply(Point/Vec2)` | O(9) | 委托给 Mat3 |
| `apply(Rect)` | O(36) | 四个角 + AABB 计算 |
| `inverted()` | O(50) | 委托给 Mat3 |
| `operator*` | O(27) | 矩阵乘法 |

**内存大小**：`sizeof(Transform2D) == 36`（等于 Mat3）

---

## 最佳实践

### 1. 使用语义化的工厂函数

```cpp
// ✅ 推荐：清晰的意图
Transform2D t = Transform2D::translation(10, 20);

// ❌ 不推荐：直接构造 Mat3
Transform2D t2{Mat3::translation(10, 20)};
```

---

### 2. 组合变换顺序

```cpp
// ✅ 推荐：从右到左理解
Transform2D combined = translate * rotate * scale;
// 应用时：先 scale，再 rotate，最后 translate

// 验证
Point p = scale.apply({10, 10});
p = rotate.apply(p);
p = translate.apply(p);
// 等价于：combined.apply({10, 10})
```

---

### 3. 检查逆变换

```cpp
// ✅ 推荐：检查 Result
auto inv = transform.inverted();
if (inv) {
    Point local = inv.value().apply(screen_pos);
}

// ❌ 不推荐：不检查直接使用
Point local = transform.inverted().value().apply(screen_pos);  // 可能崩溃
```

---

## 常见陷阱

### 1. apply(Rect) 返回 AABB

```cpp
Rect rect{0, 0, 100, 50};
Transform2D r = Transform2D::rotation(std::numbers::pi_v<float> / 4);

Rect result = r.apply(rect);
// result 是 AABB，不是旋转后的矩形！
// 宽度和高度会变大（包围盒效应）
```

---

### 2. 向量不受平移影响

```cpp
Transform2D t = Transform2D::translation(100, 100);
Vec2 direction{1, 0};
Vec2 transformed = t.apply(direction);
// transformed == {1, 0}（向量不受平移影响）

// 使用 Point 如果需要平移
Point pos{1, 0};
Point moved = t.apply(Point{pos.x, pos.y});  // {101, 100}
```

---

## 完整示例

```cpp
#include <mine/math/Transform2D.h>
#include <mine/math/Rect.h>

// 示例：可拖动可旋转的 Widget
class TransformableWidget {
    Point position_{100, 100};
    float rotation_ = 0.0f;
    float scale_ = 1.0f;
    Rect local_bounds_{0, 0, 100, 50};
    
public:
    void set_position(Point pos) { position_ = pos; }
    void set_rotation(float radians) { rotation_ = radians; }
    void set_scale(float s) { scale_ = s; }
    
    Transform2D get_world_transform() const {
        // 先缩放，再旋转，最后平移到世界坐标
        return Transform2D::translation(position_.to_vec2()) *
               Transform2D::rotation(rotation_) *
               Transform2D::scale(scale_);
    }
    
    bool hit_test(Point screen_pos) {
        // 将屏幕坐标转换到局部坐标
        auto inv = get_world_transform().inverted();
        if (!inv) return false;
        
        Point local_pos = inv.value().apply(screen_pos);
        return local_bounds_.contains(local_pos);
    }
    
    Rect get_screen_bounds() const {
        // 获取屏幕空间的包围盒
        return get_world_transform().apply(local_bounds_);
    }
    
    void draw(Canvas& canvas) {
        canvas.save();
        canvas.set_transform(get_world_transform().matrix());
        
        // 在局部坐标系绘制
        canvas.draw_rect(local_bounds_, paint_);
        
        canvas.restore();
    }
};

// 使用示例
void example() {
    TransformableWidget widget;
    widget.set_position({200, 150});
    widget.set_rotation(std::numbers::pi_v<float> / 4);  // 45度
    widget.set_scale(1.5f);
    
    // 碰撞检测
    Point mouse_pos{250, 200};
    if (widget.hit_test(mouse_pos)) {
        // 鼠标在 widget 内
    }
    
    // 获取屏幕包围盒
    Rect screen_bounds = widget.get_screen_bounds();
}
```

---

## 总结

`Transform2D` 是 `mine.math` 模块的 2D 仿射变换包装类，具备：

1. **Mat3 包装**：提供更高级的 API，隐藏矩阵细节
2. **流畅 API**：语义化的工厂函数（`translation`、`rotation`、`scale`）
3. **围绕点旋转**：`rotation_about()` 简化常见需求
4. **多类型应用**：支持 `Point`、`Vec2`、`Rect`

**使用建议**：
- 优先使用 `Transform2D` 而非直接使用 `Mat3`
- 注意 `apply(Rect)` 返回 AABB
- 组合变换从右到左理解
- 检查 `inverted()` 返回值
