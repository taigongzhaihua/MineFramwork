# Pen 与 Brush 详细接口文档

## 概述

`Pen` 和 `Brush` 是 `mine.paint` 模块的**绘制属性类型**，分别控制描边样式和填充样式。

**核心特性：**
- **Pen（描边样式）**：线宽、线连接、线端点、斜接限制
- **Brush（填充画刷）**：纯色、线性渐变、径向渐变、亚克力（磨砂玻璃）
- **值类型**：可直接嵌入 DrawCmd，无动态分配
- **内联存储**：Brush 使用 union 内联存储所有画刷数据

---

## 文件位置

```
src/mine/paint/api/include/mine/paint/Pen.h
src/mine/paint/api/include/mine/paint/Brush.h
```

---

## Pen（描边样式）

### 枚举类型

#### LineJoin

```cpp
enum class LineJoin : uint8_t {
    Miter,  ///< 斜接（尖角，受 miter_limit 限制）
    Bevel,  ///< 斜角（截去尖角，平直连接）
    Round,  ///< 圆角（弧形连接）
};
```

**描述**：两条线段交汇处的连接形状。

**枚举值**：
- `Miter`：斜接（尖角，受 `miter_limit` 限制）
- `Bevel`：斜角（截去尖角，平直连接）
- `Round`：圆角（弧形连接）

---

#### LineCap

```cpp
enum class LineCap : uint8_t {
    Flat,    ///< 平端（路径终点处截止，无延伸）
    Round,   ///< 圆端（以线宽为直径的半圆延伸）
    Square,  ///< 方端（延伸线宽/2 的距离，形成方形）
};
```

**描述**：开放路径两端（起点和终点）的形状。

**枚举值**：
- `Flat`：平端（路径终点处截止，无延伸）
- `Round`：圆端（以线宽为直径的半圆延伸）
- `Square`：方端（延伸线宽/2 的距离，形成方形）

---

### Pen 结构体

```cpp
struct Pen {
    float    width       = 1.0f;            ///< 描边宽度（逻辑像素）
    LineJoin line_join   = LineJoin::Miter; ///< 折线连接处形状
    LineCap  start_cap   = LineCap::Flat;   ///< 路径起点端点形状
    LineCap  end_cap     = LineCap::Flat;   ///< 路径终点端点形状
    float    miter_limit = 10.0f;           ///< 斜接限制（超出后退化为 Bevel）
};
```

**描述**：描边样式，描述路径或几何图形描边时的视觉参数。

**字段**：
- `width`：描边宽度（逻辑像素）
- `line_join`：折线连接处形状
- `start_cap`：路径起点端点形状
- `end_cap`：路径终点端点形状
- `miter_limit`：斜接限制（超出后退化为 Bevel）

**默认值**：1px 宽、斜接连接、平端

---

## Brush（填充画刷）

### 常量

```cpp
static constexpr uint32_t kMaxGradientStops = 4;
```

**描述**：渐变画刷最大色标数（内联存储，避免动态分配）。

---

### 枚举类型

#### BrushKind

```cpp
enum class BrushKind : uint8_t {
    SolidColor,       ///< 纯色填充（M0 已实现）
    LinearGradient,   ///< 线性渐变（M0 已实现）
    RadialGradient,   ///< 径向渐变（M0 已实现）
    AcrylicBrush,     ///< 亚克力（模糊背景 + 色调，M0 已实现）
    ImageBrush,       ///< 图像画刷（M1+ 实现）
};
```

**描述**：画刷类型枚举。

**枚举值**：
- `SolidColor`：纯色填充（M0 已实现）
- `LinearGradient`：线性渐变（M0 已实现）
- `RadialGradient`：径向渐变（M0 已实现）
- `AcrylicBrush`：亚克力（模糊背景 + 色调，M0 已实现）
- `ImageBrush`：图像画刷（M1+ 实现）

---

### 结构体类型

#### GradientStop

```cpp
struct GradientStop {
    float       offset{0.0f};  ///< 渐变路径上的归一化位置 [0, 1]
    math::Color color{};       ///< 此位置对应的线性空间颜色（RGBA）
};
```

**描述**：渐变色标（颜色 + 偏移量）。

**字段**：
- `offset`：渐变路径上的归一化位置 [0.0, 1.0]
- `color`：此位置对应的线性空间颜色（RGBA）

---

#### LinearGradientData

```cpp
struct LinearGradientData {
    math::Vec2   start{0.0f, 0.5f};             ///< 起点（归一化 [0,1]）
    math::Vec2   end{1.0f, 0.5f};               ///< 终点（归一化 [0,1]）
    uint32_t     stop_count{0};                 ///< 色标数量 [2..kMaxGradientStops]
    float        _pad{0.0f};                    ///< 对齐填充
    GradientStop stops[kMaxGradientStops]{};    ///< 色标数组（按 offset 升序排列）
};
```

**描述**：线性渐变数据。

**坐标系约定**：
- 起/终点使用归一化坐标，(0,0)=形状左上角，(1,1)=形状右下角
- 例：从左到右的渐变：start=(0,0.5), end=(1,0.5)

**字段**：
- `start`：起点（归一化 [0,1]）
- `end`：终点（归一化 [0,1]）
- `stop_count`：色标数量 [2..kMaxGradientStops]
- `stops`：色标数组（按 offset 升序排列）

---

#### RadialGradientData

```cpp
struct RadialGradientData {
    math::Vec2   center{0.5f, 0.5f};            ///< 圆心（归一化 [0,1]）
    float        inner_radius{0.0f};            ///< 内径（归一化，0=实心圆，>0=环形）
    float        outer_radius{1.0f};            ///< 外径（归一化，1.0=形状对角线的一半）
    uint32_t     stop_count{0};                 ///< 色标数量 [2..kMaxGradientStops]
    float        _pad{0.0f};                    ///< 对齐填充
    GradientStop stops[kMaxGradientStops]{};    ///< 色标数组（按 offset 升序排列）
};
```

**描述**：径向渐变数据。

**坐标系约定**：
- 圆心和半径均使用归一化坐标，1.0 对应形状包围盒的较短边长度的一半
- `inner_radius=0`：实心圆（从圆心开始渐变）
- `inner_radius>0`：环形渐变（从内圆边缘开始）

**字段**：
- `center`：圆心（归一化 [0,1]）
- `inner_radius`：内径（归一化，0=实心圆，>0=环形）
- `outer_radius`：外径（归一化，1.0=形状对角线的一半）
- `stop_count`：色标数量 [2..kMaxGradientStops]
- `stops`：色标数组（按 offset 升序排列）

---

#### AcrylicData

```cpp
struct AcrylicData {
    math::Color  tint_color{1.0f, 1.0f, 1.0f, 1.0f};  ///< 叠加色调（线性 RGBA）
    float        tint_opacity{0.7f};                    ///< 色调混合比例 [0,1]
    float        blur_amount{30.0f};                    ///< 模糊强度（逻辑像素）
    float        saturation{1.25f};                     ///< 饱和度增益（1.0=原始）
    float        _pad{0.0f};                            ///< 对齐填充
};
```

**描述**：亚克力画刷数据（磨砂玻璃效果）。

**亚克力效果由三个步骤组成**：
1. 捕获当前渲染目标背景（高斯模糊）
2. 增强饱和度（可选）
3. 叠加色调颜色

**字段**：
- `tint_color`：叠加色调（线性 RGBA）
- `tint_opacity`：色调混合比例 [0,1]，0=纯背景，1=纯色调
- `blur_amount`：模糊强度（逻辑像素），建议范围 10~60
- `saturation`：饱和度增益（1.0=不变，>1.0=增强，建议 1.25）

---

### Brush 类

```cpp
class Brush {
public:
    // 工厂方法
    [[nodiscard]] static Brush solid(math::Color color) noexcept;
    [[nodiscard]] static Brush solid_rgb(uint32_t rgb) noexcept;
    [[nodiscard]] static Brush solid_rgba(uint32_t rgba) noexcept;
    [[nodiscard]] static Brush linear_gradient(math::Vec2 start, math::Vec2 end, core::Span<const GradientStop> stops) noexcept;
    [[nodiscard]] static Brush radial_gradient(math::Vec2 center, float outer_radius, core::Span<const GradientStop> stops) noexcept;
    [[nodiscard]] static Brush radial_gradient_ring(math::Vec2 center, float inner_radius, float outer_radius, core::Span<const GradientStop> stops) noexcept;
    [[nodiscard]] static Brush acrylic(math::Color tint, float tint_opacity = 0.7f, float blur_amount = 30.0f, float saturation = 1.25f) noexcept;
    
    // 访问器
    [[nodiscard]] BrushKind kind() const noexcept;
    [[nodiscard]] math::Color color() const noexcept;
    [[nodiscard]] const LinearGradientData& linear_gradient_data() const noexcept;
    [[nodiscard]] const RadialGradientData& radial_gradient_data() const noexcept;
    [[nodiscard]] const AcrylicData& acrylic_data() const noexcept;
    [[nodiscard]] bool is_transparent() const noexcept;
    
    // 默认构造
    Brush() noexcept = default;
};
```

---

## 成员方法

### Brush::solid()

```cpp
[[nodiscard]] static Brush solid(math::Color color) noexcept;
```

**描述**：创建纯色画刷。

**参数**：
- `color`：线性空间 RGBA 颜色

**返回**：纯色画刷

**示例**：
```cpp
auto brush = Brush::solid(Color{0.2f, 0.6f, 1.0f, 1.0f});
```

---

### Brush::solid_rgb()

```cpp
[[nodiscard]] static Brush solid_rgb(uint32_t rgb) noexcept;
```

**描述**：创建纯色画刷（从 0xRRGGBB 整数字面值，Alpha 默认为 1.0）。

**参数**：
- `rgb`：0xRRGGBB 格式的颜色值

**返回**：纯色画刷

**示例**：
```cpp
auto brush = Brush::solid_rgb(0xFF0000);  // 红色
```

---

### Brush::solid_rgba()

```cpp
[[nodiscard]] static Brush solid_rgba(uint32_t rgba) noexcept;
```

**描述**：创建纯色画刷（从 0xRRGGBBAA 整数字面值）。

**参数**：
- `rgba`：0xRRGGBBAA 格式的颜色值

**返回**：纯色画刷

**示例**：
```cpp
auto brush = Brush::solid_rgba(0xFF000080);  // 半透明红色
```

---

### Brush::linear_gradient()

```cpp
[[nodiscard]] static Brush linear_gradient(
    math::Vec2                  start,
    math::Vec2                  end,
    core::Span<const GradientStop> stops) noexcept;
```

**描述**：创建线性渐变画刷。

**参数**：
- `start`：渐变起点（归一化 [0,1]，(0,0)=左上，(1,1)=右下）
- `end`：渐变终点（归一化 [0,1]）
- `stops`：色标数组（2..kMaxGradientStops 个，按 offset 升序）

**返回**：线性渐变画刷

**特征**：
- 色标数量超过 `kMaxGradientStops` 时自动截断
- 起/终点使用归一化坐标，相对于形状包围盒

**示例**：
```cpp
GradientStop stops[] = {
    {0.0f, {0.2f, 0.5f, 1.0f, 1.0f}},  // 蓝色
    {1.0f, {1.0f, 0.5f, 0.1f, 1.0f}}   // 橙色
};
auto brush = Brush::linear_gradient({0, 0.5f}, {1, 0.5f}, stops);  // 从左到右
```

---

### Brush::radial_gradient()

```cpp
[[nodiscard]] static Brush radial_gradient(
    math::Vec2                  center,
    float                       outer_radius,
    core::Span<const GradientStop> stops) noexcept;
```

**描述**：创建径向渐变画刷（实心圆，从圆心向外）。

**参数**：
- `center`：圆心（归一化 [0,1]，(0.5,0.5)=形状中心）
- `outer_radius`：外径（归一化，1.0=形状短边的一半）
- `stops`：色标数组（2..kMaxGradientStops 个）

**返回**：径向渐变画刷

**示例**：
```cpp
GradientStop stops[] = {
    {0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // 白色（圆心）
    {1.0f, {0.2f, 0.5f, 1.0f, 1.0f}}   // 蓝色（外圈）
};
auto brush = Brush::radial_gradient({0.5f, 0.5f}, 1.0f, stops);
```

---

### Brush::radial_gradient_ring()

```cpp
[[nodiscard]] static Brush radial_gradient_ring(
    math::Vec2                  center,
    float                       inner_radius,
    float                       outer_radius,
    core::Span<const GradientStop> stops) noexcept;
```

**描述**：创建径向渐变画刷（环形，从内圆到外圆）。

**参数**：
- `center`：圆心（归一化 [0,1]）
- `inner_radius`：内径（归一化，0=实心圆）
- `outer_radius`：外径（归一化）
- `stops`：色标数组（2..kMaxGradientStops 个）

**返回**：径向渐变画刷

**示例**：
```cpp
GradientStop stops[] = {
    {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},  // 红色（内圆）
    {1.0f, {0.0f, 0.0f, 1.0f, 1.0f}}   // 蓝色（外圆）
};
auto brush = Brush::radial_gradient_ring({0.5f, 0.5f}, 0.3f, 1.0f, stops);
```

---

### Brush::acrylic()

```cpp
[[nodiscard]] static Brush acrylic(
    math::Color tint,
    float       tint_opacity  = 0.7f,
    float       blur_amount   = 30.0f,
    float       saturation    = 1.25f) noexcept;
```

**描述**：创建亚克力画刷（磨砂玻璃效果）。

**参数**：
- `tint`：叠加色调（线性 RGBA）
- `tint_opacity`：色调混合比例 [0,1]，0=纯背景，1=纯色调
- `blur_amount`：模糊强度（逻辑像素），建议值 20~40
- `saturation`：饱和度增益，1.0=不变，>1.0=增强（建议 1.25）

**返回**：亚克力画刷

**特征**：
- 仅在 RhiRenderer 有亚克力管线时生效
- 模糊强度建议范围 10~60

**示例**：
```cpp
auto brush = Brush::acrylic({0.5f, 0.7f, 1.0f, 1.0f}, 0.6f, 30.0f);
```

---

### Brush::kind()

```cpp
[[nodiscard]] BrushKind kind() const noexcept;
```

**描述**：返回画刷类型。

**返回**：画刷类型枚举值

---

### Brush::color()

```cpp
[[nodiscard]] math::Color color() const noexcept;
```

**描述**：返回纯色画刷的颜色值（线性空间）。

**前置条件**：`kind() == BrushKind::SolidColor`

**返回**：线性空间 RGBA 颜色

---

### Brush::linear_gradient_data()

```cpp
[[nodiscard]] const LinearGradientData& linear_gradient_data() const noexcept;
```

**描述**：返回线性渐变数据。

**前置条件**：`kind() == BrushKind::LinearGradient`

**返回**：线性渐变数据引用

---

### Brush::radial_gradient_data()

```cpp
[[nodiscard]] const RadialGradientData& radial_gradient_data() const noexcept;
```

**描述**：返回径向渐变数据。

**前置条件**：`kind() == BrushKind::RadialGradient`

**返回**：径向渐变数据引用

---

### Brush::acrylic_data()

```cpp
[[nodiscard]] const AcrylicData& acrylic_data() const noexcept;
```

**描述**：返回亚克力画刷数据。

**前置条件**：`kind() == BrushKind::AcrylicBrush`

**返回**：亚克力数据引用

---

### Brush::is_transparent()

```cpp
[[nodiscard]] bool is_transparent() const noexcept;
```

**描述**：判断画刷是否完全透明（Alpha == 0）。

**返回**：完全透明返回 true

---

## 使用场景

### 1. 纯色描边

```cpp
Pen pen;
pen.width = 2.0f;
pen.line_join = LineJoin::Round;
pen.start_cap = LineCap::Round;
pen.end_cap = LineCap::Round;

auto brush = Brush::solid_rgb(0xFF0000);
canvas.stroke_rect({10, 10, 200, 100}, brush, pen);
```

---

### 2. 线性渐变填充

```cpp
GradientStop stops[] = {
    {0.0f, {0.2f, 0.5f, 1.0f, 1.0f}},  // 蓝色
    {1.0f, {1.0f, 0.5f, 0.1f, 1.0f}}   // 橙色
};

auto brush = Brush::linear_gradient(
    {0, 0.5f},  // 从左
    {1, 0.5f},  // 到右
    stops
);

canvas.fill_rect({10, 10, 200, 100}, brush);
```

---

### 3. 径向渐变填充

```cpp
GradientStop stops[] = {
    {0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // 白色（圆心）
    {1.0f, {0.2f, 0.5f, 1.0f, 1.0f}}   // 蓝色（外圈）
};

auto brush = Brush::radial_gradient(
    {0.5f, 0.5f},  // 圆心
    1.0f,          // 半径
    stops
);

canvas.fill_ellipse({100, 100}, {50, 50}, brush);
```

---

### 4. 亚克力效果

```cpp
auto brush = Brush::acrylic(
    {0.5f, 0.7f, 1.0f, 1.0f},  // 蓝色色调
    0.6f,                       // 60% 色调混合
    30.0f                       // 30px 模糊
);

canvas.fill_rect({10, 10, 200, 100}, brush);
```

---

### 5. 斜接/圆角描边

```cpp
// 斜接
Pen miter_pen;
miter_pen.width = 4.0f;
miter_pen.line_join = LineJoin::Miter;
miter_pen.miter_limit = 10.0f;

// 圆角
Pen round_pen;
round_pen.width = 4.0f;
round_pen.line_join = LineJoin::Round;

auto brush = Brush::solid_rgb(0xFF0000);
canvas.stroke_rounded_rect({{10, 10, 200, 100}, 8.f}, brush, round_pen);
```

---

## 性能分析

### Brush 内存布局

**特征**：
- Brush 使用 union 内联存储所有画刷数据
- 无动态分配，可直接嵌入 DrawCmd
- 拷贝安全

**大小估算**：
- `BrushKind`：1 字节
- `union` 最大成员（`RadialGradientData`）：约 80 字节
- 总计：约 80~100 字节（取决于对齐）

---

### 渐变色标限制

**特征**：
- 最多 4 个色标（`kMaxGradientStops`）
- 超出自动截断
- 对于复杂渐变，建议在 CPU 预计算中间色

---

### 亚克力性能

**特征**：
- 需要捕获背景并执行高斯模糊
- 性能开销较大（GPU 密集）
- 建议限制同屏亚克力元素数量

---

## 最佳实践

### 1. 优先使用纯色

```cpp
// ✅ 推荐：纯色性能最优
auto brush = Brush::solid_rgb(0xFF0000);

// ❌ 不推荐：单色渐变浪费性能
GradientStop stops[] = {
    {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
    {1.0f, {1.0f, 0.0f, 0.0f, 1.0f}}  // 相同颜色
};
auto brush = Brush::linear_gradient({0, 0}, {1, 1}, stops);
```

---

### 2. 限制渐变色标数量

```cpp
// ✅ 推荐：2-4 个色标
GradientStop stops[] = {
    {0.0f, {0.2f, 0.5f, 1.0f, 1.0f}},
    {1.0f, {1.0f, 0.5f, 0.1f, 1.0f}}
};

// ❌ 不推荐：超过 4 个会被截断
GradientStop stops[] = {
    {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
    {0.25f, {0.0f, 1.0f, 0.0f, 1.0f}},
    {0.5f, {0.0f, 0.0f, 1.0f, 1.0f}},
    {0.75f, {1.0f, 1.0f, 0.0f, 1.0f}},
    {1.0f, {1.0f, 0.0f, 1.0f, 1.0f}}  // 第 5 个会被截断
};
```

---

### 3. 合理设置斜接限制

```cpp
// ✅ 推荐：合理限制斜接长度
Pen pen;
pen.width = 2.0f;
pen.line_join = LineJoin::Miter;
pen.miter_limit = 10.0f;  // 超出后退化为 Bevel

// ❌ 不推荐：过大的斜接限制导致尖角过长
pen.miter_limit = 100.0f;
```

---

### 4. 谨慎使用亚克力

```cpp
// ✅ 推荐：限制同屏亚克力元素数量
auto brush = Brush::acrylic({0.5f, 0.7f, 1.0f, 1.0f}, 0.6f, 30.0f);
canvas.fill_rect({10, 10, 200, 100}, brush);  // 仅一个元素

// ❌ 不推荐：大量亚克力元素
for (int i = 0; i < 100; ++i) {
    auto brush = Brush::acrylic({0.5f, 0.7f, 1.0f, 1.0f}, 0.6f, 30.0f);
    canvas.fill_rect({i * 10.f, i * 10.f, 100, 100}, brush);
}
```

---

## 常见陷阱

### 1. 忘记设置色标

```cpp
// ❌ 问题：空色标数组
GradientStop stops[0];
auto brush = Brush::linear_gradient({0, 0}, {1, 1}, stops);  // 未定义行为

// ✅ 解决：至少 2 个色标
GradientStop stops[] = {
    {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
    {1.0f, {0.0f, 0.0f, 1.0f, 1.0f}}
};
auto brush = Brush::linear_gradient({0, 0}, {1, 1}, stops);
```

---

### 2. 色标 offset 未升序

```cpp
// ❌ 问题：色标 offset 未升序
GradientStop stops[] = {
    {1.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
    {0.0f, {0.0f, 0.0f, 1.0f, 1.0f}}  // offset 逆序
};

// ✅ 解决：按 offset 升序排列
GradientStop stops[] = {
    {0.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
    {1.0f, {1.0f, 0.0f, 0.0f, 1.0f}}
};
```

---

### 3. 访问错误的画刷数据

```cpp
// ❌ 问题：访问错误类型的数据
auto brush = Brush::solid_rgb(0xFF0000);
auto gradient_data = brush.linear_gradient_data();  // 未定义行为

// ✅ 解决：先检查类型
if (brush.kind() == BrushKind::LinearGradient) {
    auto gradient_data = brush.linear_gradient_data();
}
```

---

### 4. 亚克力模糊强度过大

```cpp
// ❌ 问题：过大的模糊强度
auto brush = Brush::acrylic({0.5f, 0.7f, 1.0f, 1.0f}, 0.6f, 200.0f);  // 性能差

// ✅ 解决：合理范围 10~60
auto brush = Brush::acrylic({0.5f, 0.7f, 1.0f, 1.0f}, 0.6f, 30.0f);
```

---

## 完整示例

```cpp
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>

using namespace mine::paint;
using namespace mine::math;

void draw_examples(Canvas& canvas) {
    // 1. 纯色矩形
    auto red = Brush::solid_rgb(0xFF0000);
    canvas.fill_rect({10, 10, 100, 50}, red);
    
    // 2. 线性渐变矩形
    GradientStop linear_stops[] = {
        {0.0f, {0.2f, 0.5f, 1.0f, 1.0f}},  // 蓝色
        {1.0f, {1.0f, 0.5f, 0.1f, 1.0f}}   // 橙色
    };
    auto linear_brush = Brush::linear_gradient({0, 0.5f}, {1, 0.5f}, linear_stops);
    canvas.fill_rect({120, 10, 100, 50}, linear_brush);
    
    // 3. 径向渐变椭圆
    GradientStop radial_stops[] = {
        {0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // 白色（圆心）
        {1.0f, {0.2f, 0.5f, 1.0f, 1.0f}}   // 蓝色（外圈）
    };
    auto radial_brush = Brush::radial_gradient({0.5f, 0.5f}, 1.0f, radial_stops);
    canvas.fill_ellipse({60, 150}, {40, 40}, radial_brush);
    
    // 4. 亚克力矩形
    auto acrylic_brush = Brush::acrylic({0.5f, 0.7f, 1.0f, 1.0f}, 0.6f, 30.0f);
    canvas.fill_rect({230, 10, 100, 50}, acrylic_brush);
    
    // 5. 圆角描边
    Pen pen;
    pen.width = 3.0f;
    pen.line_join = LineJoin::Round;
    pen.start_cap = LineCap::Round;
    pen.end_cap = LineCap::Round;
    
    auto blue = Brush::solid_rgb(0x0000FF);
    canvas.stroke_rounded_rect({{10, 80, 100, 50}, 10.f}, blue, pen);
    
    // 6. 斜接描边
    Pen miter_pen;
    miter_pen.width = 3.0f;
    miter_pen.line_join = LineJoin::Miter;
    miter_pen.miter_limit = 10.0f;
    
    canvas.stroke_rect({120, 80, 100, 50}, blue, miter_pen);
}
```

---

## 总结

`Pen` 和 `Brush` 是 `mine.paint` 模块的绘制属性类型，具备：

1. **Pen（描边样式）**：线宽、线连接、线端点、斜接限制
2. **Brush（填充画刷）**：纯色、线性渐变、径向渐变、亚克力
3. **值类型**：可直接嵌入 DrawCmd，无动态分配
4. **内联存储**：Brush 使用 union 内联存储所有画刷数据

**使用建议**：
- 优先使用纯色（性能最优）
- 限制渐变色标数量（最多 4 个）
- 合理设置斜接限制（避免尖角过长）
- 谨慎使用亚克力（性能开销大）
- 色标 offset 按升序排列
- 访问画刷数据前先检查类型
