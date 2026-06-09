# mine.math —— 数学类型与几何工具模块

## 模块概述

`mine.math` 提供二维/三维图形计算所需的基础数学类型：向量、矩阵、矩形、颜色、变换等。所有类型为 `struct`、`constexpr` 友好，无虚函数、无堆分配。

| 类别 | 类型 |
|------|------|
| 向量 | `Vec2`、`Vec3`、`Vec4` |
| 几何 | `Point`、`Size`、`Rect`、`Thickness` |
| 圆角 | `CornerRadii`、`RoundedRect`、`ComplexRoundedRect` |
| 矩阵/变换 | `Mat3`、`Mat4`、`Transform2D` |
| 颜色 | `Color` |

**伞形头文件**：`<mine/math/Math.h>` 一次性引入所有类型。

**依赖**：仅 `mine.core`（`Assert`、`Result`）。

---

## 1. 向量 —— `Vec2`、`Vec3`、`Vec4`

**文件**：`<mine/math/Vec2.h>`、`Vec3.h`、`Vec4.h`

### 1.1 Vec2

```cpp
namespace mine::math {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() noexcept;
    constexpr Vec2(float x, float y) noexcept;

    static constexpr Vec2 zero()    noexcept;   // {0, 0}
    static constexpr Vec2 one()     noexcept;   // {1, 1}
    static constexpr Vec2 unit_x()  noexcept;   // {1, 0}
    static constexpr Vec2 unit_y()  noexcept;   // {0, 1}

    // 下标访问
    float&       operator[](size_t i) noexcept;
    const float& operator[](size_t i) const noexcept;

    // 算术
    Vec2 operator+(Vec2) const noexcept;
    Vec2 operator-(Vec2) const noexcept;
    Vec2 operator*(float) const noexcept;
    Vec2 operator/(float) const noexcept;
    Vec2& operator+=(Vec2) noexcept;
    Vec2& operator-=(Vec2) noexcept;
    Vec2& operator*=(float) noexcept;
    Vec2& operator/=(float) noexcept;
    Vec2  operator-() const noexcept;         // 取反

    // 几何
    float length()      const noexcept;
    float length_sq()   const noexcept;
    Vec2  normalized()  const noexcept;
    float dot(Vec2)     const noexcept;
    float cross(Vec2)   const noexcept;

    // 友元：标量 * 向量
    friend Vec2 operator*(float s, Vec2 v) noexcept;
};

} // namespace mine::math
```

### 1.2 Vec3 / Vec4

`Vec3` 和 `Vec4` 与 `Vec2` API 一致，分别增加 `z` 和 `z`、`w` 分量。

```cpp
struct Vec3 { float x, y, z; /* 同 Vec2 接口 + cross(Vec3) */ };
struct Vec4 { float x, y, z, w; /* 同 Vec3 接口 */ };
```

---

## 2. 点与尺寸 —— `Point`、`Size`

**文件**：`<mine/math/Point.h>`、`<mine/math/Size.h>`

```cpp
namespace mine::math {

struct Point {
    float x{0.0f};
    float y{0.0f};

    constexpr Point() noexcept;
    constexpr Point(float x, float y) noexcept;

    Point operator+(Vec2) const noexcept;  // 平移
    Point operator-(Vec2) const noexcept;
    Vec2 operator-(Point) const noexcept;  // 两点差为向量
};

struct Size {
    float width{0.0f};
    float height{0.0f};

    constexpr Size() noexcept;
    constexpr Size(float w, float h) noexcept;

    bool empty() const noexcept;           // width<=0 或 height<=0
    float area() const noexcept;           // empty ? 0 : w*h
};

} // namespace mine::math
```

---

## 3. 矩形与边距 —— `Rect`、`Thickness`

**文件**：`<mine/math/Rect.h>`、`<mine/math/Thickness.h>`

### 3.1 Rect

左上角 + 宽高表达的轴对齐矩形。

```cpp
namespace mine::math {

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    constexpr Rect() noexcept;
    constexpr Rect(float x, float y, float w, float h) noexcept;
    constexpr Rect(Point origin, Size size) noexcept;
    static constexpr Rect from_points(Point a, Point b) noexcept;

    // 边界
    float left()   const noexcept;     // x
    float top()    const noexcept;     // y
    float right()  const noexcept;     // x + width
    float bottom() const noexcept;     // y + height

    bool   empty() const noexcept;     // width<=0 或 height<=0
    float  area()  const noexcept;

    // 角点
    Point top_left()     const noexcept;
    Point top_right()    const noexcept;
    Point bottom_left()  const noexcept;
    Point bottom_right() const noexcept;
    Point center()       const noexcept;

    // 碰撞检测
    bool contains(Point) const noexcept;
    bool contains(Rect)  const noexcept;
    bool intersects(Rect) const noexcept;

    // 变换
    Rect translated(Vec2) const noexcept;
    Rect inflated(float dx, float dy) const noexcept;
    Rect deflated(float dx, float dy) const noexcept;
    Rect inset(Thickness) const noexcept;

    // 交集 / 并集
    Rect intersection(Rect) const noexcept;
    Rect union_rect(Rect)  const noexcept;
};

} // namespace mine::math
```

### 3.2 Thickness

四边独立宽度描述符，常用于边距、内边距、边框宽度。

```cpp
namespace mine::math {

struct Thickness {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};

    constexpr Thickness() noexcept;
    constexpr Thickness(float uniform) noexcept;                    // 四边相同
    constexpr Thickness(float l, float t, float r, float b) noexcept;

    static constexpr Thickness uniform(float v) noexcept;           // {v,v,v,v}
    static constexpr Thickness symmetric(float h, float v) noexcept; // {h,v,h,v}

    float horizontal() const noexcept;  // left + right
    float vertical()   const noexcept;  // top + bottom
    bool  is_zero()    const noexcept;  // 四边均为 0
};

} // namespace mine::math
```

---

## 4. 颜色 —— `Color`

**文件**：`<mine/math/Color.h>`

线性空间 RGBA，分量 `[0, 1]`。

```cpp
namespace mine::math {

struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    constexpr Color() noexcept;
    constexpr Color(float r, float g, float b, float a = 1.0f) noexcept;

    // 工厂
    static constexpr Color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255) noexcept;
    static constexpr Color from_rgb_u32(uint32_t rgb) noexcept;     // 0xRRGGBB
    static constexpr Color from_rgba_u32(uint32_t rgba) noexcept;   // 0xRRGGBBAA

    // 预设常量
    static constexpr Color Transparent{0,0,0,0};
    static constexpr Color Black{0,0,0};
    static constexpr Color White{1,1,1};

    // 修改
    constexpr Color with_alpha(float a)    const noexcept;
    constexpr Color clamped()              const noexcept;
    constexpr Color premultiplied()        const noexcept;

    // 混合
    Color blend_over(Color dst) const noexcept;    // source-over 合成
    Color lerp(Color to, float t) const noexcept;  // 线性插值

    // 序列化
    uint32_t to_rgba8() const noexcept;            // 打包为 0xRRGGBBAA

    // 算术
    Color operator+(Color) const noexcept;
    Color operator*(float) const noexcept;
    friend Color operator*(float, Color) noexcept;
};

} // namespace mine::math
```

---

## 5. 圆角几何

**文件**：`<mine/math/CornerRadii.h>`、`<mine/math/RoundedRect.h>`、`<mine/math/ComplexRoundedRect.h>`

### 5.1 CornerRadii

四角各自独立的椭圆圆角半径。

```cpp
namespace mine::math {

struct CornerRadii {
    Vec2 top_left{};
    Vec2 top_right{};
    Vec2 bottom_right{};
    Vec2 bottom_left{};

    constexpr CornerRadii() noexcept;
    constexpr CornerRadii(Vec2 tl, Vec2 tr, Vec2 br, Vec2 bl) noexcept;

    static constexpr CornerRadii uniform(float r) noexcept;                   // 四角相同
    static constexpr CornerRadii uniform(float rx, float ry) noexcept;        // 椭圆
    static constexpr CornerRadii from_corners(float tl, float tr,
                                              float br, float bl) noexcept;   // 四角独立

    bool is_zero()    const noexcept;
    bool is_uniform() const noexcept;
};

} // namespace mine::math
```

### 5.2 RoundedRect

带统一圆角半径的矩形，半径自动钳制到 `min(w, h) / 2`。

```cpp
struct RoundedRect {
    Rect  rect{};
    float radius_x{0.0f};
    float radius_y{0.0f};

    RoundedRect(Rect r, float radius) noexcept;
    RoundedRect(Rect r, float rx, float ry) noexcept;

    Rect  bounds() const noexcept;
    bool  contains(Point) const noexcept;
    RoundedRect translated(Vec2) const noexcept;
};
```

### 5.3 ComplexRoundedRect

四角独立圆角的矩形，用于精确定义控件外观形状和命中测试边界。

```cpp
struct ComplexRoundedRect {
    Rect        rect{};
    CornerRadii radii{};

    ComplexRoundedRect(Rect r, CornerRadii cr) noexcept;

    Rect  bounds() const noexcept;
    bool  contains(Point) const noexcept;
    ComplexRoundedRect translated(Vec2) const noexcept;
};
```

---

## 6. 矩阵与变换 —— `Mat3`、`Mat4`、`Transform2D`

**文件**：`<mine/math/Mat3.h>`、`<mine/math/Mat4.h>`、`<mine/math/Transform2D.h>`

### 6.1 Mat3

3×3 列主序矩阵，用于 2D 仿射变换。

```cpp
namespace mine::math {

struct Mat3 {
    // 元素按列存储：m00 m10 m20 | m01 m11 m21 | m02 m12 m22
    float m[9]{};

    static constexpr Mat3 identity() noexcept;

    // 基础变换矩阵
    static constexpr Mat3 translation(float tx, float ty) noexcept;
    static constexpr Mat3 scaling(float sx, float sy) noexcept;
    static Mat3 rotation(float radians) noexcept;                          // 非 constexpr（含 sin/cos）

    // 运算
    constexpr Mat3 operator*(const Mat3&) const noexcept;                  // 矩阵乘
    constexpr Vec2 transform_point(Vec2) const noexcept;                   // 点变换（平移生效）
    constexpr Vec2 transform_vector(Vec2) const noexcept;                  // 向量变换（忽略平移）

    // 逆矩阵
    Result<Mat3> inverted() const noexcept;                                // 不可逆时返回错误
};

} // namespace mine::math
```

### 6.2 Mat4

4×4 列主序矩阵，用于 3D 变换和透视投影。

```cpp
struct Mat4 {
    float m[16]{};

    static constexpr Mat4 identity() noexcept;
    static Mat4 perspective(float fov_y_rad, float aspect,
                            float near_z, float far_z) noexcept;
    static Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept;

    constexpr Mat4 operator*(const Mat4&) const noexcept;
    Result<Mat4> inverted() const noexcept;
};
```

### 6.3 Transform2D

对 `Mat3` 的语义包装，提供 2D 变换的便捷接口。

```cpp
namespace mine::math {

struct Transform2D {
    Mat3 matrix_{};

    constexpr Transform2D() noexcept;
    explicit constexpr Transform2D(const Mat3&) noexcept;

    static constexpr Transform2D identity() noexcept;
    static constexpr Transform2D translation(float tx, float ty) noexcept;
    static constexpr Transform2D translation(Vec2 offset) noexcept;
    static constexpr Transform2D scale(float uniform) noexcept;
    static constexpr Transform2D scale(float sx, float sy) noexcept;
    static Transform2D rotation(float radians) noexcept;
    static Transform2D rotation_about(float radians, Point pivot) noexcept;

    // 变换点/向量/矩形
    constexpr Point apply(Point) const noexcept;
    constexpr Vec2  apply_vector(Vec2) const noexcept;
    Rect            apply(Rect) const noexcept;             // 返回包围盒（非矩形变换）

    // 组合
    constexpr Transform2D operator*(const Transform2D&) const noexcept;

    // 逆
    Result<Transform2D> inverted() const noexcept;

    // 访问底层矩阵
    constexpr const Mat3& matrix() const noexcept;
};

} // namespace mine::math
```

### 使用示例

```cpp
// 构建变换链
Transform2D t = Transform2D::translation(100, 50)
              * Transform2D::scale(2.0f)
              * Transform2D::rotation(0.5f);

// 应用
Point p2 = t.apply({10, 20});                     // 平移 → 缩放 → 旋转
Rect  r2 = t.apply(Rect{0,0,100,100});             // 返回轴对齐包围盒

// 逆变换（用于命中测试坐标映射）
auto inv = t.inverted();
if (inv.ok()) {
    Point local = inv.value().apply(screen_point);
}
```

---

## 7. 伞形头文件 —— `<mine/math/Math.h>`

一次性引入所有数学类型：

```cpp
#include <mine/math/Math.h>
// 等价于逐一包含 Vec2~Vec4, Point, Size, Rect, Thickness,
// Color, CornerRadii, RoundedRect, ComplexRoundedRect, Mat3, Mat4, Transform2D
```

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.core` | `math` 的基础依赖 |
| `mine.paint` | 使用 `Rect`、`Color`、`CornerRadii` 进行绘制 |
| `mine.ui.visual` | 使用 `Transform2D`、`Rect` 进行布局和命中测试 |
