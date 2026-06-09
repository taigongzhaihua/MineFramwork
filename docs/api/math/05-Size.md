# Size 详细接口文档

## 概述

`Size` 是 `mine.math` 模块的 **二维尺寸** 类型,语义上表示 **宽度和高度** 而非位置或偏移。与 `Vec2` 和 `Point` 区分开来,`Size` 在类型系统中明确标识尺寸测量,提升代码可读性。

**核心特性:**
- **POD 类型**:Plain Old Data,可直接内存拷贝,`constexpr` 支持
- **语义明确**:表示尺寸而非位置或方向
- **边界检查**:`empty()` 判断尺寸是否有效
- **面积计算**:`area()` 计算矩形面积
- **尺寸约束**:`clamped()` 限制尺寸范围

---

## 文件位置

```
src/mine/math/api/include/mine/math/Size.h
```

---

## 类定义

```cpp
namespace mine::math {

struct Size {
    float width{0.0f};
    float height{0.0f};

    // 构造函数
    constexpr Size() noexcept = default;
    constexpr Size(float width_value, float height_value) noexcept;

    // 静态工厂函数
    [[nodiscard]] static constexpr Size zero() noexcept;

    // 查询
    [[nodiscard]] constexpr bool empty() const noexcept;
    [[nodiscard]] constexpr float area() const noexcept;

    // 类型转换
    [[nodiscard]] constexpr Vec2 to_vec2() const noexcept;

    // 算术运算符
    [[nodiscard]] constexpr Size operator+(Size rhs) const noexcept;
    [[nodiscard]] constexpr Size operator-(Size rhs) const noexcept;
    [[nodiscard]] constexpr Size operator*(float scalar) const noexcept;
    [[nodiscard]] constexpr Size operator/(float scalar) const noexcept;

    constexpr Size& operator+=(Size rhs) noexcept;
    constexpr Size& operator-=(Size rhs) noexcept;
    constexpr Size& operator*=(float scalar) noexcept;
    constexpr Size& operator/=(float scalar) noexcept;

    // 尺寸约束
    [[nodiscard]] constexpr Size clamped(Size min_size, Size max_size) const noexcept;

    // 比较运算符
    [[nodiscard]] constexpr bool operator==(const Size&) const noexcept = default;
};

using Size2f = Size;

}
```

---

## 成员变量

### width / height

```cpp
float width{0.0f};
float height{0.0f};
```

**描述**:尺寸的宽度和高度,公开访问。

**默认值**:`0.0f`（零尺寸）

**语义**:
- 通常为非负值
- `<= 0` 表示无效尺寸（`empty()` 返回 `true`）

**示例**:
```cpp
Size button_size{100.0f, 40.0f};
std::cout << "Button: " << button_size.width << "x" << button_size.height;
```

---

## 构造函数

### 默认构造函数

```cpp
constexpr Size() noexcept = default;
```

**描述**:创建零尺寸 `(0, 0)`。

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Size zero_size;
static_assert(zero_size.empty());
```

---

### 宽高构造函数

```cpp
constexpr Size(float width_value, float height_value) noexcept;
```

**参数**:
- `width_value`:宽度
- `height_value`:高度

**时间复杂度**:O(1)

**示例**:
```cpp
constexpr Size window_size{1920.0f, 1080.0f};
constexpr Size icon_size{32.0f, 32.0f};
```

---

## 静态工厂函数

### zero()

```cpp
[[nodiscard]] static constexpr Size zero() noexcept;
```

**描述**:返回零尺寸 `(0, 0)`。

**返回值**:`Size{0.0f, 0.0f}`

**时间复杂度**:O(1)

---

## 查询方法

### empty()

```cpp
[[nodiscard]] constexpr bool empty() const noexcept;
```

**描述**:判断尺寸是否无效（宽或高 `<= 0`）。

**返回值**:
- `true`：`width <= 0.0f || height <= 0.0f`
- `false`：尺寸有效

**时间复杂度**:O(1)

**示例**:
```cpp
Size valid{100.0f, 50.0f};
Size invalid1{0.0f, 50.0f};
Size invalid2{100.0f, -10.0f};

assert(!valid.empty());
assert(invalid1.empty());
assert(invalid2.empty());
```

---

### area()

```cpp
[[nodiscard]] constexpr float area() const noexcept;
```

**描述**:计算矩形面积。

**返回值**:
- 若 `empty()`：`0.0f`
- 否则：`width * height`

**时间复杂度**:O(1)

**示例**:
```cpp
Size rect{100.0f, 50.0f};
float area = rect.area();  // 5000.0

Size empty_size{0.0f, 50.0f};
assert(empty_size.area() == 0.0f);
```

---

## 类型转换

### to_vec2()

```cpp
[[nodiscard]] constexpr Vec2 to_vec2() const noexcept;
```

**描述**:转换为 `Vec2` 向量。

**返回值**:`Vec2{width, height}`

**时间复杂度**:O(1)

**示例**:
```cpp
Size size{100.0f, 50.0f};
Vec2 v = size.to_vec2();  // {100.0f, 50.0f}
```

---

## 算术运算符

### operator+

```cpp
[[nodiscard]] constexpr Size operator+(Size rhs) const noexcept;
```

**描述**:尺寸相加（分量相加）。

**参数**:
- `rhs`:右操作数

**返回值**:`Size{width + rhs.width, height + rhs.height}`

**时间复杂度**:O(1)

**示例**:
```cpp
Size content{100.0f, 50.0f};
Size padding{10.0f, 5.0f};
Size total = content + padding * 2.0f;  // {120.0f, 60.0f}
```

---

### operator-

```cpp
[[nodiscard]] constexpr Size operator-(Size rhs) const noexcept;
```

**描述**:尺寸相减（分量相减）。

**参数**:
- `rhs`:右操作数

**返回值**:`Size{width - rhs.width, height - rhs.height}`

**时间复杂度**:O(1)

**示例**:
```cpp
Size total{120.0f, 60.0f};
Size margin{10.0f, 5.0f};
Size content = total - margin * 2.0f;  // {100.0f, 50.0f}
```

---

### operator*

```cpp
[[nodiscard]] constexpr Size operator*(float scalar) const noexcept;
```

**描述**:尺寸缩放。

**参数**:
- `scalar`:缩放因子

**返回值**:`Size{width * scalar, height * scalar}`

**时间复杂度**:O(1)

**示例**:
```cpp
Size base{100.0f, 50.0f};
Size doubled = base * 2.0f;  // {200.0f, 100.0f}
Size half = base * 0.5f;     // {50.0f, 25.0f}
```

---

### operator/

```cpp
[[nodiscard]] constexpr Size operator/(float scalar) const noexcept;
```

**描述**:尺寸除法。

**参数**:
- `scalar`:除数（**不检查零除**）

**返回值**:`Size{width / scalar, height / scalar}`

**时间复杂度**:O(1)

---

### operator+= / operator-= / operator*= / operator/=

```cpp
constexpr Size& operator+=(Size rhs) noexcept;
constexpr Size& operator-=(Size rhs) noexcept;
constexpr Size& operator*=(float scalar) noexcept;
constexpr Size& operator/=(float scalar) noexcept;
```

**描述**:就地运算。

**返回值**:`*this`

**时间复杂度**:O(1)

---

## 尺寸约束

### clamped()

```cpp
[[nodiscard]] constexpr Size clamped(Size min_size, Size max_size) const noexcept;
```

**描述**:将尺寸限制在指定范围内（分量独立约束）。

**参数**:
- `min_size`:最小尺寸
- `max_size`:最大尺寸

**返回值**:
```cpp
Size{
    clamp(width, min_size.width, max_size.width),
    clamp(height, min_size.height, max_size.height)
}
```

**时间复杂度**:O(1)

**示例**:
```cpp
Size requested{2000.0f, 1500.0f};
Size min{800.0f, 600.0f};
Size max{1920.0f, 1080.0f};

Size actual = requested.clamped(min, max);
// actual == {1920.0f, 1080.0f}（被约束到最大值）
```

---

## 比较运算符

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const Size&) const noexcept = default;
```

**描述**:逐分量精确比较。

**返回值**:`width == rhs.width && height == rhs.height`

**时间复杂度**:O(1)

---

## 使用场景

### 1. UI 控件尺寸

```cpp
class Button {
    Size size_{100.0f, 40.0f};
    Size min_size_{50.0f, 20.0f};
    Size max_size_{200.0f, 60.0f};
    
public:
    void set_size(Size requested) {
        size_ = requested.clamped(min_size_, max_size_);
    }
    
    bool can_fit_text(const char* text, float font_size) {
        Size text_size = measure_text(text, font_size);
        return text_size.width <= size_.width && 
               text_size.height <= size_.height;
    }
};
```

---

### 2. 布局计算

```cpp
Size calculate_container_size(Span<Size> children, float gap) {
    if (children.empty()) {
        return Size::zero();
    }
    
    float total_width = 0.0f;
    float max_height = 0.0f;
    
    for (Size child : children) {
        total_width += child.width;
        max_height = std::max(max_height, child.height);
    }
    
    total_width += gap * (children.size() - 1);
    return {total_width, max_height};
}
```

---

### 3. 纹理/图像尺寸

```cpp
struct Texture {
    Size size;
    
    bool is_power_of_two() const {
        auto is_pot = [](float v) {
            int i = static_cast<int>(v);
            return i > 0 && (i & (i - 1)) == 0;
        };
        return is_pot(size.width) && is_pot(size.height);
    }
    
    Size mip_level_size(int level) const {
        float scale = std::pow(0.5f, static_cast<float>(level));
        return Size{
            std::max(1.0f, size.width * scale),
            std::max(1.0f, size.height * scale)
        };
    }
};
```

---

### 4. 窗口尺寸约束

```cpp
class Window {
    Size size_{800.0f, 600.0f};
    Size min_size_{400.0f, 300.0f};
    Size max_size_{3840.0f, 2160.0f};
    
public:
    void resize(Size requested) {
        size_ = requested.clamped(min_size_, max_size_);
        on_size_changed();
    }
    
    bool is_fullscreen_size(Size screen_size) const {
        return (size_ - screen_size).area() < 1.0f;  // 误差容忍
    }
};
```

---

## 性能分析

### 内存布局

```cpp
sizeof(Size) == 8  // 2 * sizeof(float)
alignof(Size) == 4  // float 对齐
```

**与 Vec2 二进制兼容**:
```cpp
static_assert(sizeof(Size) == sizeof(Vec2));
```

---

### 运算性能

| 操作 | 时间复杂度 | 周期数（估算） | 说明 |
|------|-----------|-------------|------|
| 构造 | O(1) | ~1 | 寄存器赋值 |
| `empty()` | O(1) | ~2 | 2 次比较 + 1 次逻辑或 |
| `area()` | O(1) | ~3 | 1 次乘法 + 分支 |
| 加减乘除 | O(1) | ~2 | 2 次浮点运算 |
| `clamped()` | O(1) | ~8 | 4 次 `clamp`（每次 2 比较） |

---

## 最佳实践

### 1. 使用类型区分尺寸和位置

```cpp
// ✅ 推荐：类型明确
Size window_size{1920.0f, 1080.0f};  // 尺寸
Point window_pos{100.0f, 100.0f};     // 位置

// ❌ 不推荐：混用 Vec2
Vec2 size{1920.0f, 1080.0f};  // 不明确：是尺寸还是偏移？
```

---

### 2. 检查尺寸有效性

```cpp
// ✅ 推荐：使用 empty() 检查
Size size = get_window_size();
if (!size.empty()) {
    render_to_texture(size);
}

// ❌ 不推荐：手动检查
if (size.width > 0 && size.height > 0) {  // 冗长
    render_to_texture(size);
}
```

---

### 3. 尺寸约束避免无效值

```cpp
// ✅ 推荐：使用 clamped()
Size safe_size = user_input.clamped(min_size, max_size);

// ❌ 不推荐：手动 clamp
Size safe_size2{
    std::clamp(user_input.width, min_size.width, max_size.width),
    std::clamp(user_input.height, min_size.height, max_size.height)
};  // 冗长且易错
```

---

## 常见陷阱

### 1. 负尺寸导致 empty()

```cpp
Size size{100.0f, -50.0f};  // height < 0
assert(size.empty());  // ✅
assert(size.area() == 0.0f);  // ✅
```

---

### 2. 尺寸运算不检查合法性

```cpp
Size a{100.0f, 50.0f};
Size b{200.0f, 100.0f};
Size diff = a - b;  // {-100.0f, -50.0f}（负尺寸）

assert(diff.empty());  // ✅
// 需要手动检查或使用 std::max
```

---

## 完整示例

### 示例：响应式布局

```cpp
#include <mine/math/Size.h>
#include <mine/math/Point.h>
#include <mine/math/Rect.h>

class ResponsiveLayout {
    Size container_size_;
    Size min_child_size_{100.0f, 50.0f};
    Size max_child_size_{400.0f, 300.0f};
    float gap_{10.0f};
    
public:
    void set_container_size(Size size) {
        container_size_ = size;
    }
    
    Size calculate_child_size(int column_count) const {
        if (container_size_.empty() || column_count <= 0) {
            return Size::zero();
        }
        
        float total_gap = gap_ * (column_count - 1);
        float available_width = container_size_.width - total_gap;
        float child_width = available_width / column_count;
        
        // 保持宽高比
        float child_height = child_width * 0.75f;
        
        Size child{child_width, child_height};
        return child.clamped(min_child_size_, max_child_size_);
    }
    
    int calculate_optimal_columns() const {
        for (int cols = 1; cols <= 10; ++cols) {
            Size child = calculate_child_size(cols);
            if (!child.empty() && 
                child.width >= min_child_size_.width &&
                child.height >= min_child_size_.height) {
                return cols;
            }
        }
        return 1;
    }
};
```

---

## 总结

`Size` 是 `mine.math` 模块的二维尺寸类型,具备:

1. **语义明确**:表示宽高而非位置或偏移
2. **POD 类型**:高性能,可 `constexpr`,ABI 稳定
3. **边界检查**:`empty()` 判断有效性
4. **尺寸约束**:`clamped()` 限制范围

**使用建议**:
- 用 `Size` 表示尺寸,用 `Point` 表示位置,用 `Vec2` 表示偏移
- 使用 `empty()` 检查有效性
- 使用 `clamped()` 约束尺寸范围
- 注意运算可能产生负值
