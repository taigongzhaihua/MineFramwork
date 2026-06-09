# Math 详细接口文档

## 概述

`Math.h` 是 `mine.math` 模块的**伞形头文件**（Umbrella Header），包含此头文件即可使用 `mine.math` 模块的全部基础 API。

**核心特性：**
- **一站式导入**：单个 `#include` 包含所有数学类型
- **模块化设计**：内部按类型分类，外部统一入口
- **依赖管理**：自动包含所有依赖头文件

---

## 文件位置

```
src/mine/math/api/include/mine/math/Math.h
```

---

## 包含内容

### 模块元数据

```cpp
#include <mine/math/Api.h>          // MINE_MATH_API 导出宏
#include <mine/math/ModuleTag.h>    // kModuleName 常量
```

---

### 工具函数

```cpp
#include <mine/math/Common.h>       // 标量工具函数和常量
```

**提供**：
- `kDefaultEpsilon`：默认浮点容差
- `nearly_equal()`：浮点比较
- `clamp()` / `clamp01()`：区间限制
- `lerp()`：线性插值
- `radians()` / `degrees()`：角度单位转换

---

### 向量类型

```cpp
#include <mine/math/Vec2.h>         // 2D 向量
#include <mine/math/Vec3.h>         // 3D 向量
#include <mine/math/Vec4.h>         // 4D 向量
```

**提供**：
- `Vec2`：2D 浮点向量（8 字节）
- `Vec3`：3D 浮点向量（12 字节）
- `Vec4`：4D 浮点向量（16 字节，SIMD 对齐）

---

### 几何基础类型

```cpp
#include <mine/math/Point.h>        // 2D 点
#include <mine/math/Size.h>         // 2D 尺寸
#include <mine/math/Rect.h>         // 轴对齐矩形
#include <mine/math/Thickness.h>    // 四边厚度
```

**提供**：
- `Point`：2D 位置（区别于 `Vec2` 方向）
- `Size`：宽度和高度
- `Rect`：轴对齐矩形（x, y, width, height）
- `Thickness`：四边各自独立的厚度（left, top, right, bottom）

---

### 圆角类型

```cpp
#include <mine/math/RoundedRect.h>         // 简单圆角矩形
#include <mine/math/CornerRadii.h>         // 四角独立圆角半径
#include <mine/math/ComplexRoundedRect.h>  // 复杂圆角矩形
```

**提供**：
- `RoundedRect`：全局统一圆角的矩形
- `CornerRadii`：四角各自独立的椭圆圆角半径
- `ComplexRoundedRect`：四角各自独立圆角，支持 CSS 规范自动缩放

---

### 矩阵类型

```cpp
#include <mine/math/Mat3.h>         // 3x3 矩阵
#include <mine/math/Mat4.h>         // 4x4 矩阵
```

**提供**：
- `Mat3`：3x3 矩阵（2D 仿射变换）
- `Mat4`：4x4 矩阵（3D 仿射变换、透视投影）

---

### 颜色类型

```cpp
#include <mine/math/Color.h>        // 线性空间颜色
```

**提供**：
- `Color`：线性空间 RGBA 颜色（4 floats）
- 格式转换：`from_rgba8()`, `to_rgba8()`, `from_rgb_u32()`
- 颜色运算：`blend_over()`, `premultiplied()`, `modulate()`

---

### 变换类型

```cpp
#include <mine/math/Transform2D.h>  // 2D 仿射变换
```

**提供**：
- `Transform2D`：2D 仿射变换包装（内部使用 `Mat3`）
- 工厂函数：`translation()`, `rotation()`, `scale()`, `rotation_about()`

---

## 使用方式

### 方式一：包含伞形头文件（推荐）

```cpp
#include <mine/math/Math.h>

// 可使用所有 mine.math 类型
Vec2 v{1, 2};
Point p{10, 20};
Mat3 m = Mat3::rotation(radians(45.0f));
Color c = Color::Red;
```

**优点**：
- 简单方便，一次导入所有类型
- 适合大多数使用场景

**缺点**：
- 编译时间稍长（包含所有头文件）

---

### 方式二：按需包含单个头文件

```cpp
#include <mine/math/Vec2.h>
#include <mine/math/Rect.h>

// 只使用 Vec2 和 Rect
Vec2 v{1, 2};
Rect r{0, 0, 100, 50};
```

**优点**：
- 编译时间更短
- 明确依赖关系

**缺点**：
- 需要手动管理多个 `#include`
- 适合仅使用少数类型的场景

---

## 命名空间

所有类型位于 `mine::math` 命名空间：

```cpp
namespace mine::math {
    // Vec2, Vec3, Vec4
    // Point, Size, Rect, Thickness
    // Mat3, Mat4, Transform2D
    // Color, CornerRadii, RoundedRect, ComplexRoundedRect
    // clamp(), lerp(), nearly_equal(), ...
}
```

**使用**：

```cpp
// 方式一：完全限定名
mine::math::Vec2 v{1, 2};

// 方式二：using 声明（推荐）
using mine::math::Vec2;
using mine::math::Mat3;

Vec2 v{1, 2};
Mat3 m = Mat3::identity();

// 方式三：using namespace（不推荐，易冲突）
using namespace mine::math;
Vec2 v{1, 2};
```

---

## 依赖关系

### 核心依赖

- `mine.core`：`Result<T>` 类型（用于 `Mat3::inverted()`, `Mat4::inverted()`）

### 内部依赖

- `Mat3` / `Mat4` 依赖 `Vec3` / `Vec4`
- `Transform2D` 依赖 `Mat3`, `Point`, `Rect`, `Vec2`
- `ComplexRoundedRect` 依赖 `Rect`, `CornerRadii`

**注意**：包含 `Math.h` 会自动处理所有依赖。

---

## 类型概览

| 类型 | 大小 | 用途 |
|------|------|------|
| `Vec2` | 8 字节 | 2D 向量（方向、偏移） |
| `Vec3` | 12 字节 | 3D 向量 |
| `Vec4` | 16 字节 | 4D 向量（SIMD 对齐） |
| `Point` | 8 字节 | 2D 点（位置） |
| `Size` | 8 字节 | 2D 尺寸 |
| `Rect` | 16 字节 | 轴对齐矩形 |
| `Thickness` | 16 字节 | 四边厚度 |
| `Color` | 16 字节 | 线性空间 RGBA |
| `CornerRadii` | 32 字节 | 四角独立圆角半径 |
| `RoundedRect` | 24 字节 | 简单圆角矩形 |
| `ComplexRoundedRect` | 48 字节 | 复杂圆角矩形 |
| `Mat3` | 36 字节 | 3x3 矩阵 |
| `Mat4` | 64 字节 | 4x4 矩阵 |
| `Transform2D` | 36 字节 | 2D 仿射变换 |

---

## 最佳实践

### 1. 默认使用伞形头文件

```cpp
// ✅ 推荐：大多数情况
#include <mine/math/Math.h>

// ❌ 不推荐：除非明确只需少数类型
#include <mine/math/Vec2.h>
#include <mine/math/Point.h>
#include <mine/math/Rect.h>
// ... 10+ 个 #include
```

---

### 2. 使用 using 声明避免冗长

```cpp
// ✅ 推荐：模块级或函数级 using
using mine::math::Vec2;
using mine::math::Mat3;

void foo() {
    Vec2 v{1, 2};
    Mat3 m = Mat3::identity();
}

// ❌ 不推荐：全局 using namespace
using namespace mine::math;  // 可能与其他库冲突
```

---

### 3. 明确依赖关系（大型项目）

```cpp
// 头文件：按需包含
// MyWidget.h
#include <mine/math/Point.h>
#include <mine/math/Rect.h>

class MyWidget {
    mine::math::Point position_;
    mine::math::Rect bounds_;
};

// 实现文件：使用伞形头文件
// MyWidget.cpp
#include <mine/math/Math.h>
```

---

## 完整示例

```cpp
#include <mine/math/Math.h>

using namespace mine::math;

// 示例：计算旋转后的包围盒
Rect calculate_rotated_bounds(Rect rect, float angle_deg) {
    // 围绕矩形中心旋转
    Point center = rect.center();
    float angle_rad = radians(angle_deg);
    
    Transform2D transform = Transform2D::rotation_about(angle_rad, center);
    
    // 返回旋转后的轴对齐包围盒
    return transform.apply(rect);
}

// 使用
void example() {
    Rect original{100, 100, 200, 100};
    Rect rotated_bounds = calculate_rotated_bounds(original, 45.0f);
    
    // 绘制原始矩形和包围盒
    draw_rect(original, Color::Red);
    draw_rect(rotated_bounds, Color::Blue);
}
```

---

## 总结

`Math.h` 是 `mine.math` 模块的**伞形头文件**，具备：

1. **一站式导入**：单个 `#include` 包含所有类型
2. **完整覆盖**：向量、矩阵、几何、颜色、变换
3. **依赖自动处理**：无需手动包含依赖头文件

**使用建议**：
- 默认使用 `#include <mine/math/Math.h>`
- 大型项目头文件按需包含，实现文件使用伞形头
- 使用 `using` 声明简化类型名
- 所有类型位于 `mine::math` 命名空间
