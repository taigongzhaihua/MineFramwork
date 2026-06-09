# Common 详细接口文档

## 概述

`Common.h` 是 `mine.math` 模块的**标量辅助函数与常量**头文件，提供浮点比较、限制、插值等通用数学工具函数。

**核心特性：**
- **浮点比较**：`nearly_equal()` 解决浮点精度问题
- **区间限制**：`clamp()` / `clamp01()` 约束值范围
- **线性插值**：`lerp()` 平滑过渡
- **角度转换**：`radians()` / `degrees()` 单位转换

---

## 文件位置

```
src/mine/math/api/include/mine/math/Common.h
```

---

## 常量

### kDefaultEpsilon

```cpp
inline constexpr float kDefaultEpsilon = 1.0e-6f;
```

**描述**：浮点比较与归一化默认使用的容差（`0.000001`）。

**用途**：
- `nearly_equal()` 默认误差范围
- `Mat3::inverted()` / `Mat4::inverted()` 默认阈值

---

## 函数

### nearly_equal()

```cpp
[[nodiscard]] inline bool nearly_equal(
    float lhs,
    float rhs,
    float epsilon = kDefaultEpsilon) noexcept;
```

**描述**：比较两个浮点值是否在给定误差范围内近似相等。

**参数**：
- `lhs`, `rhs`：待比较的浮点值
- `epsilon`：容差（默认 `kDefaultEpsilon = 1.0e-6f`）

**返回值**：`|lhs - rhs| <= epsilon`

**示例**：
```cpp
float a = 0.1f + 0.2f;
float b = 0.3f;

// ❌ 直接比较可能失败（浮点精度问题）
assert(a == b);  // 可能为 false

// ✅ 推荐：使用容差比较
assert(nearly_equal(a, b));  // true
```

---

### clamp()

```cpp
[[nodiscard]] constexpr float clamp(
    float value,
    float min_value,
    float max_value) noexcept;
```

**描述**：将标量限制在 `[min_value, max_value]` 区间内。

**返回值**：
- `value < min_value` → `min_value`
- `value > max_value` → `max_value`
- 否则 → `value`

**时间复杂度**：O(1)

**示例**：
```cpp
float x = clamp(150.0f, 0.0f, 100.0f);  // 100.0f
float y = clamp(-10.0f, 0.0f, 100.0f);  // 0.0f
float z = clamp(50.0f, 0.0f, 100.0f);   // 50.0f
```

---

### clamp01()

```cpp
[[nodiscard]] constexpr float clamp01(float value) noexcept;
```

**描述**：将标量限制在 `[0, 1]` 区间内。

**等价于**：`clamp(value, 0.0f, 1.0f)`

**示例**：
```cpp
float alpha = clamp01(1.5f);   // 1.0f
float t = clamp01(-0.2f);      // 0.0f
float s = clamp01(0.7f);       // 0.7f
```

---

### lerp()

```cpp
[[nodiscard]] constexpr float lerp(float a, float b, float t) noexcept;
```

**描述**：线性插值（Linear Interpolation）。

**公式**：`a + (b - a) * t`

**参数**：
- `a`：起始值
- `b`：结束值
- `t`：插值因子（通常 `[0, 1]`）

**返回值**：
- `t = 0` → `a`
- `t = 1` → `b`
- `t = 0.5` → `(a + b) / 2`

**时间复杂度**：O(1)

**示例**：
```cpp
float start = 0.0f;
float end = 100.0f;

float mid = lerp(start, end, 0.5f);     // 50.0f
float quarter = lerp(start, end, 0.25f); // 25.0f
```

**用途**：
- 动画插值
- 颜色混合
- 平滑过渡

---

### radians()

```cpp
[[nodiscard]] constexpr float radians(float degrees_value) noexcept;
```

**描述**：将角度转换为弧度。

**公式**：`degrees * π / 180`

**实现**：`degrees * 0.01745329251994329577f`

**示例**：
```cpp
float angle_deg = 90.0f;
float angle_rad = radians(angle_deg);  // π/2 ≈ 1.5708
```

---

### degrees()

```cpp
[[nodiscard]] constexpr float degrees(float radians_value) noexcept;
```

**描述**：将弧度转换为角度。

**公式**：`radians * 180 / π`

**实现**：`radians * 57.295779513082320876f`

**示例**：
```cpp
float angle_rad = std::numbers::pi_v<float> / 2;
float angle_deg = degrees(angle_rad);  // 90.0
```

---

## 使用场景

### 1. 浮点比较

```cpp
// 向量归一化后长度应为 1
Vec2 v{3, 4};
Vec2 normalized = v.normalized();
float length = normalized.length();

assert(nearly_equal(length, 1.0f));  // ✅ 使用容差比较
```

---

### 2. 限制值范围

```cpp
// UI 透明度限制在 [0, 1]
float alpha = user_input * 0.01f;
alpha = clamp01(alpha);

// 音量限制在 [0, 100]
int volume = clamp(slider_value, 0, 100);
```

---

### 3. 动画插值

```cpp
// 平滑动画（0.0 → 1.0）
float duration = 2.0f;  // 秒
float elapsed = 0.5f;   // 0.5 秒已过
float t = clamp01(elapsed / duration);  // 0.25

float start_pos = 0.0f;
float end_pos = 100.0f;
float current_pos = lerp(start_pos, end_pos, t);  // 25.0
```

---

### 4. 角度单位转换

```cpp
// 旋转 45 度
float angle_deg = 45.0f;
float angle_rad = radians(angle_deg);
Mat3 rotation = Mat3::rotation(angle_rad);

// 显示旋转角度（弧度 → 角度）
float display_angle = degrees(current_rotation);
```

---

## 性能分析

| 函数 | 时间复杂度 | 说明 |
|------|-----------|------|
| `nearly_equal()` | O(1) | 一次 `fabs()` + 比较 |
| `clamp()` | O(1) | 两次比较 |
| `clamp01()` | O(1) | 调用 `clamp()` |
| `lerp()` | O(1) | 一次减法 + 一次乘法 + 一次加法 |
| `radians()` | O(1) | 一次乘法（编译期常量） |
| `degrees()` | O(1) | 一次乘法（编译期常量） |

**内联优化**：所有函数都是 `inline` 或 `constexpr`，编译器会内联展开。

---

## 最佳实践

### 1. 使用 nearly_equal 比较浮点数

```cpp
// ✅ 推荐：容差比较
if (nearly_equal(a, b)) {
    // 近似相等
}

// ❌ 不推荐：直接比较
if (a == b) {
    // 可能因浮点精度失败
}
```

---

### 2. 限制插值因子范围

```cpp
// ✅ 推荐：限制 t 在 [0, 1]
float t = clamp01(elapsed / duration);
float value = lerp(start, end, t);

// ❌ 不推荐：t 超出范围
float t2 = elapsed / duration;  // 可能 > 1
float value2 = lerp(start, end, t2);  // 结果超出 [start, end]
```

---

### 3. 角度单位一致性

```cpp
// ✅ 推荐：统一使用弧度（内部计算）
float angle_rad = radians(user_input_deg);
Mat3 rot = Mat3::rotation(angle_rad);

// ❌ 不推荐：混用角度和弧度
Mat3 rot2 = Mat3::rotation(user_input_deg);  // 错误！rotation 需要弧度
```

---

## 常见陷阱

### 1. lerp() 不自动限制 t

```cpp
float t = 1.5f;  // 超出 [0, 1]
float value = lerp(0.0f, 100.0f, t);
// value == 150.0（超出 [0, 100]）

// 解决：手动限制
float safe_t = clamp01(t);
float safe_value = lerp(0.0f, 100.0f, safe_t);  // 100.0
```

---

### 2. nearly_equal 默认容差可能不适用

```cpp
// 默认 epsilon = 1e-6 适用于常规精度
assert(nearly_equal(1.0000001f, 1.0f));  // true

// 大数值可能需要更大的 epsilon
float large_a = 1000000.0f;
float large_b = 1000000.1f;
assert(nearly_equal(large_a, large_b));  // false（差值 0.1 > 1e-6）

// 解决：使用自定义 epsilon
assert(nearly_equal(large_a, large_b, 1.0f));  // true
```

---

## 完整示例

```cpp
#include <mine/math/Common.h>

// 示例：平滑颜色过渡动画
struct ColorAnimation {
    Color start_color_;
    Color end_color_;
    float duration_;
    float elapsed_ = 0.0f;
    
    void update(float delta_time) {
        elapsed_ += delta_time;
        elapsed_ = clamp(elapsed_, 0.0f, duration_);
    }
    
    Color current_color() const {
        float t = clamp01(elapsed_ / duration_);
        
        return {
            lerp(start_color_.r, end_color_.r, t),
            lerp(start_color_.g, end_color_.g, t),
            lerp(start_color_.b, end_color_.b, t),
            lerp(start_color_.a, end_color_.a, t),
        };
    }
    
    bool is_complete() const {
        return nearly_equal(elapsed_, duration_);
    }
};

// 使用
void example() {
    ColorAnimation anim{
        .start_color_ = Color::Red,
        .end_color_ = Color::Blue,
        .duration_ = 2.0f,
    };
    
    // 每帧更新
    anim.update(0.016f);  // 16ms
    Color current = anim.current_color();
    
    if (anim.is_complete()) {
        // 动画完成
    }
}
```

---

## 总结

`Common.h` 提供 `mine.math` 模块的**标量工具函数**，具备：

1. **浮点比较**：`nearly_equal()` 解决精度问题
2. **区间限制**：`clamp()` / `clamp01()` 约束值范围
3. **线性插值**：`lerp()` 平滑过渡
4. **角度转换**：`radians()` / `degrees()` 单位转换

**使用建议**：
- 浮点比较必须使用 `nearly_equal()`
- 插值前使用 `clamp01()` 限制 `t`
- 内部计算统一使用弧度
- 大数值可能需要自定义 epsilon
