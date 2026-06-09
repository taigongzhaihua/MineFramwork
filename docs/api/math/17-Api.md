# Api 详细接口文档

## 概述

`Api.h` 定义 `mine.math` 模块的**符号导出宏** `MINE_MATH_API`，用于跨模块（DLL/SO）公开符号。

**核心特性：**
- **平台适配**：Windows（`__declspec`）/ Linux（`__attribute__`）
- **构建模式切换**：静态库（无宏）/ 动态库（dllexport/dllimport）
- **条件编译**：根据 `MINE_SHARED_BUILD` 和 `MINE_BUILDING_MINE_MATH` 宏选择行为

---

## 文件位置

```
src/mine/math/api/include/mine/math/Api.h
```

---

## 宏定义

### MINE_MATH_API

```cpp
#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_MATH)
#            define MINE_MATH_API __declspec(dllexport)
#        else
#            define MINE_MATH_API __declspec(dllimport)
#        endif
#    else
#        define MINE_MATH_API
#    endif
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_MATH_API __attribute__((visibility("default")))
#    else
#        define MINE_MATH_API
#    endif
#endif
```

**描述**：为跨模块公开符号附加平台相关的导出或导入修饰。

---

## 行为逻辑

### Windows 平台（`_WIN32`）

| 条件 | 宏定义 | 说明 |
|------|--------|------|
| 静态库构建（`!MINE_SHARED_BUILD`） | `MINE_MATH_API` = *(空)* | 无需导出/导入 |
| 动态库构建 + 编译 math 模块（`MINE_BUILDING_MINE_MATH`） | `MINE_MATH_API` = `__declspec(dllexport)` | 导出符号到 DLL |
| 动态库构建 + 使用 math 模块（`!MINE_BUILDING_MINE_MATH`） | `MINE_MATH_API` = `__declspec(dllimport)` | 从 DLL 导入符号 |

---

### Linux/macOS 平台（非 `_WIN32`）

| 条件 | 宏定义 | 说明 |
|------|--------|------|
| 静态库构建（`!MINE_SHARED_BUILD`） | `MINE_MATH_API` = *(空)* | 无需导出/导入 |
| 动态库构建（`MINE_SHARED_BUILD`） | `MINE_MATH_API` = `__attribute__((visibility("default")))` | 导出符号到 SO |

---

## 使用方式

### 1. 导出函数

```cpp
#include <mine/math/Api.h>

namespace mine::math {

// 导出函数（仅当有非内联实现时需要）
MINE_MATH_API float calculate_area(Rect rect) noexcept;

}
```

**编译 math 模块时**（定义 `MINE_BUILDING_MINE_MATH`）：
- Windows: `__declspec(dllexport) float calculate_area(...)`
- Linux: `__attribute__((visibility("default"))) float calculate_area(...)`

**使用 math 模块时**（未定义 `MINE_BUILDING_MINE_MATH`）：
- Windows: `__declspec(dllimport) float calculate_area(...)`
- Linux: `__attribute__((visibility("default"))) float calculate_area(...)`

---

### 2. 导出类

```cpp
#include <mine/math/Api.h>

namespace mine::math {

// 导出类（通常不需要，math 类型全部 inline）
class MINE_MATH_API ComplexMatrix {
public:
    ComplexMatrix();
    ~ComplexMatrix();
    
    void compute();  // 非内联实现
};

}
```

---

## 当前使用情况

### math 模块现状

`mine.math` 模块当前**主要由头文件内联类型组成**，所有类型都是 POD（Plain Old Data）或 `constexpr` 函数，**无需使用 `MINE_MATH_API`**。

**内联类型**：
- `Vec2`, `Vec3`, `Vec4`
- `Point`, `Size`, `Rect`, `Thickness`
- `Mat3`, `Mat4`, `Transform2D`
- `Color`, `CornerRadii`, `RoundedRect`, `ComplexRoundedRect`

**内联函数**：
- `clamp()`, `lerp()`, `nearly_equal()`, `radians()`, `degrees()`

---

### 未来扩展

若后续引入**非内联实现**（如复杂矩阵运算、SIMD 优化版本），则应使用 `MINE_MATH_API` 标记公开符号：

```cpp
namespace mine::math {

// 假设未来添加 SIMD 优化版本
MINE_MATH_API Vec4 simd_dot_product(Vec4 lhs, Vec4 rhs) noexcept;

}
```

---

## 相关宏

### MINE_SHARED_BUILD

**定义位置**：构建系统（xmake.lua / CMakeLists.txt）

**作用**：指示当前构建动态库（DLL/SO）

**示例**：
```lua
-- xmake.lua
if is_kind("shared") then
    add_defines("MINE_SHARED_BUILD")
end
```

---

### MINE_BUILDING_MINE_MATH

**定义位置**：构建系统（仅在编译 math 模块时定义）

**作用**：区分"编译 math 模块"和"使用 math 模块"

**示例**：
```lua
-- xmake.lua
target("mine.math")
    add_defines("MINE_BUILDING_MINE_MATH", {public = false})
```

---

## 最佳实践

### 1. 仅在必要时使用 MINE_MATH_API

```cpp
// ✅ 推荐：内联函数无需宏
constexpr Vec2 add(Vec2 lhs, Vec2 rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

// ❌ 不推荐：内联函数不需要导出宏
MINE_MATH_API constexpr Vec2 add(Vec2 lhs, Vec2 rhs) noexcept;
```

---

### 2. 非内联实现需要导出宏

```cpp
// 声明（头文件）
MINE_MATH_API float expensive_computation(Mat4 matrix) noexcept;

// 实现（.cpp 文件）
float expensive_computation(Mat4 matrix) noexcept {
    // 复杂计算...
    return result;
}
```

---

### 3. 模板类型无需导出宏

```cpp
// ✅ 推荐：模板全部内联，无需宏
template<typename T>
struct Vector {
    T x, y;
    
    constexpr Vector operator+(const Vector& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y};
    }
};
```

---

## 常见问题

### 1. 为什么 math 模块不需要导出宏？

**答**：`mine.math` 模块类型全部是：
- POD 结构体（`Vec2`, `Point`, `Rect` 等）
- `constexpr` 构造函数和方法
- 内联函数（`clamp()`, `lerp()` 等）

这些类型在头文件中完全定义，编译器会在使用处内联展开，**无需跨 DLL/SO 边界**。

---

### 2. 什么时候需要 MINE_MATH_API？

**答**：当添加以下内容时需要：
- 非内联全局函数
- 包含非内联成员函数的类
- 导出的全局变量

**示例**：
```cpp
// 需要 MINE_MATH_API
MINE_MATH_API extern const Mat4 kProjectionMatrix;

MINE_MATH_API void initialize_math_library() noexcept;
```

---

### 3. 静态库构建需要定义宏吗？

**答**：静态库构建时，`MINE_MATH_API` 为空宏，**无需任何操作**。构建系统不定义 `MINE_SHARED_BUILD`，宏自动为空。

---

## 平台差异

| 平台 | 动态库扩展名 | 导出语法 | 导入语法 |
|------|-------------|----------|----------|
| Windows | `.dll` | `__declspec(dllexport)` | `__declspec(dllimport)` |
| Linux | `.so` | `__attribute__((visibility("default")))` | 同左 |
| macOS | `.dylib` | `__attribute__((visibility("default")))` | 同左 |

**注意**：Linux/macOS 使用相同的 visibility 属性，无需区分导出/导入。

---

## 完整示例

```cpp
// Api.h（已定义 MINE_MATH_API）
#include <mine/math/Api.h>

// 头文件：声明非内联函数
namespace mine::math {

MINE_MATH_API float compute_determinant_4x4(const float* matrix) noexcept;

}

// .cpp 文件：实现
namespace mine::math {

float compute_determinant_4x4(const float* matrix) noexcept {
    // 实现 4x4 行列式计算...
    return result;
}

}

// 使用
#include <mine/math/Api.h>
#include <mine/math/Mat4.h>

void example() {
    Mat4 m = Mat4::identity();
    float det = compute_determinant_4x4(&m.m[0][0]);
}
```

---

## 总结

`Api.h` 定义 `mine.math` 模块的**符号导出宏** `MINE_MATH_API`，具备：

1. **平台适配**：Windows（`__declspec`）/ Linux（`__attribute__`）
2. **构建模式切换**：静态库（空宏）/ 动态库（导出/导入）
3. **当前状态**：math 模块全部内联，**暂无需使用宏**

**使用建议**：
- 当前 math 模块无需使用 `MINE_MATH_API`
- 未来添加非内联实现时标记宏
- 模板和 `constexpr` 函数无需宏
- 构建系统自动处理 `MINE_SHARED_BUILD` 和 `MINE_BUILDING_MINE_MATH`
