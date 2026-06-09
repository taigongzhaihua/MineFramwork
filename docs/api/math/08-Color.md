# Color 详细接口文档

## 概述

`Color` 是 `mine.math` 模块的 **线性空间 RGBA 颜色** 类型，用于图形渲染和颜色计算。分量范围通常为 `[0, 1]`，输入输出层负责与 sRGB 进行转换。

**核心特性：**
- **线性空间**：适合 GPU 渲染和混合计算
- **浮点精度**：4 个 `float` 分量（r, g, b, a）
- **预定义常量**：Transparent, Black, White, Red, Green, Blue
- **格式转换**：支持 RGBA8, RGB_U32, RGBA_U32 互转
- **颜色运算**：加减乘、调制、预乘、混合

---

## 文件位置

```
src/mine/math/api/include/mine/math/Color.h
```

---

## 类定义

```cpp
namespace mine::math {

struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    // 构造函数
    constexpr Color() noexcept = default;
    constexpr Color(float red, float green, float blue, float alpha = 1.0f) noexcept;

    // 静态工厂函数
    [[nodiscard]] static constexpr Color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept;
    [[nodiscard]] static constexpr Color from_rgb_u32(uint32_t rgb) noexcept;
    [[nodiscard]] static constexpr Color from_rgba_u32(uint32_t rgba) noexcept;

    // 颜色操作
    [[nodiscard]] constexpr Color clamped() const noexcept;
    [[nodiscard]] constexpr Color with_alpha(float alpha) const noexcept;
    [[nodiscard]] constexpr Color premultiplied() const noexcept;
    [[nodiscard]] Color blend_over(Color dst) const noexcept;

    // 格式转换
    [[nodiscard]] uint32_t to_rgba8() const noexcept;

    // 运算符
    [[nodiscard]] constexpr Color operator+(Color rhs) const noexcept;
    [[nodiscard]] constexpr Color operator-(Color rhs) const noexcept;
    [[nodiscard]] constexpr Color operator*(float scalar) const noexcept;
    [[nodiscard]] constexpr Color modulate(Color rhs) const noexcept;
    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;

    // 预定义常量
    static const Color Transparent;
    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
};

}
```

---

## 成员变量

### r / g / b / a

```cpp
float r{0.0f};  // 红色分量
float g{0.0f};  // 绿色分量
float b{0.0f};  // 蓝色分量
float a{1.0f};  // Alpha 透明度
```

**值域**：通常为 `[0.0, 1.0]`，但允许超出范围（HDR）

**默认值**：`(0, 0, 0, 1)` = 不透明黑色

---

## 构造函数

### 默认构造函数

```cpp
constexpr Color() noexcept = default;
```

**描述**：创建不透明黑色 `(0, 0, 0, 1)`。

---

### 浮点构造函数

```cpp
constexpr Color(float red, float green, float blue, float alpha = 1.0f) noexcept;
```

**参数**：
- `red`, `green`, `blue`：颜色分量
- `alpha`：透明度（默认 1.0 = 不透明）

**示例**：
```cpp
constexpr Color red{1.0f, 0.0f, 0.0f};           // 不透明红色
constexpr Color semi_white{1.0f, 1.0f, 1.0f, 0.5f};  // 50% 透明白色
```

---

## 静态工厂函数

### from_rgba8()

```cpp
[[nodiscard]] static constexpr Color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept;
```

**描述**：从 8 位整数分量（0-255）创建颜色。

**转换公式**：`color_component = uint8_value / 255.0f`

**示例**：
```cpp
Color red = Color::from_rgba8(255, 0, 0);           // 红色
Color semi_blue = Color::from_rgba8(0, 0, 255, 128);  // 50% 透明蓝色
```

---

### from_rgb_u32()

```cpp
[[nodiscard]] static constexpr Color from_rgb_u32(uint32_t rgb) noexcept;
```

**描述**：从 `0xRRGGBB` 格式创建不透明颜色。

**参数**：
- `rgb`：打包值，格式 `0xRRGGBB`

**示例**：
```cpp
Color red = Color::from_rgb_u32(0xFF0000);     // #FF0000 红色
Color green = Color::from_rgb_u32(0x00FF00);   // #00FF00 绿色
Color custom = Color::from_rgb_u32(0x3498DB);  // #3498DB 浅蓝色
```

---

### from_rgba_u32()

```cpp
[[nodiscard]] static constexpr Color from_rgba_u32(uint32_t rgba) noexcept;
```

**描述**：从 `0xRRGGBBAA` 格式创建颜色（包含 alpha）。

**参数**：
- `rgba`：打包值，格式 `0xRRGGBBAA`

**示例**：
```cpp
Color semi_red = Color::from_rgba_u32(0xFF000080);  // 50% 透明红色
```

---

## 颜色操作

### clamped()

```cpp
[[nodiscard]] constexpr Color clamped() const noexcept;
```

**描述**：将所有分量限制在 `[0, 1]` 范围内。

**返回值**：每个分量执行 `clamp01(component)`

**示例**：
```cpp
Color hdr{2.5f, 0.8f, -0.1f, 1.2f};
Color sdr = hdr.clamped();  // {1.0f, 0.8f, 0.0f, 1.0f}
```

---

### with_alpha()

```cpp
[[nodiscard]] constexpr Color with_alpha(float alpha) const noexcept;
```

**描述**：返回修改 alpha 分量后的新颜色（RGB 不变）。

**示例**：
```cpp
Color opaque_red{1.0f, 0.0f, 0.0f, 1.0f};
Color semi_red = opaque_red.with_alpha(0.5f);  // {1.0f, 0.0f, 0.0f, 0.5f}
```

---

### premultiplied()

```cpp
[[nodiscard]] constexpr Color premultiplied() const noexcept;
```

**描述**：返回预乘 alpha 的颜色。

**转换公式**：`{r * a, g * a, b * a, a}`

**用途**：GPU 渲染优化，避免运行时乘法

**示例**：
```cpp
Color c{1.0f, 0.5f, 0.2f, 0.5f};
Color pre = c.premultiplied();  // {0.5f, 0.25f, 0.1f, 0.5f}
```

---

### blend_over()

```cpp
[[nodiscard]] Color blend_over(Color dst) const noexcept;
```

**描述**：将当前颜色混合到目标颜色上（Porter-Duff 源覆盖目标混合）。

**混合公式**：
```
out_a = src_a + dst_a * (1 - src_a)
out_rgb = (src_rgb * src_a + dst_rgb * dst_a * (1 - src_a)) / out_a
```

**示例**：
```cpp
Color bg = Color::White;
Color fg = Color::from_rgba8(255, 0, 0, 128);  // 50% 透明红色
Color result = fg.blend_over(bg);  // 粉红色
```

---

## 格式转换

### to_rgba8()

```cpp
[[nodiscard]] uint32_t to_rgba8() const noexcept;
```

**描述**：转换为 `0xRRGGBBAA` 打包格式（8 位整数）。

**转换公式**：`uint8 = clamp01(float) * 255 + 0.5` (四舍五入)

**返回值**：`(R << 24) | (G << 16) | (B << 8) | A`

**示例**：
```cpp
Color red{1.0f, 0.0f, 0.0f, 1.0f};
uint32_t packed = red.to_rgba8();  // 0xFF0000FF
```

---

## 运算符

### operator+

```cpp
[[nodiscard]] constexpr Color operator+(Color rhs) const noexcept;
```

**描述**：分量相加（用于光照累加）。

**示例**：
```cpp
Color ambient{0.1f, 0.1f, 0.1f};
Color diffuse{0.5f, 0.3f, 0.2f};
Color total = ambient + diffuse;  // {0.6f, 0.4f, 0.3f, 2.0f}（注意 alpha 也相加）
```

---

### operator-

```cpp
[[nodiscard]] constexpr Color operator-(Color rhs) const noexcept;
```

**描述**：分量相减。

---

### operator*

```cpp
[[nodiscard]] constexpr Color operator*(float scalar) const noexcept;
```

**描述**：标量乘法（调整亮度）。

**示例**：
```cpp
Color base{0.8f, 0.5f, 0.3f};
Color darker = base * 0.5f;  // {0.4f, 0.25f, 0.15f, 0.5f}（注意 alpha 也缩放）
```

---

### modulate()

```cpp
[[nodiscard]] constexpr Color modulate(Color rhs) const noexcept;
```

**描述**：分量相乘（用于纹理调制、颜色过滤）。

**返回值**：`{r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a}`

**示例**：
```cpp
Color texture{0.8f, 0.6f, 0.4f};
Color tint{1.0f, 0.5f, 0.5f};  // 红色调
Color result = texture.modulate(tint);  // {0.8f, 0.3f, 0.2f, 1.0f}
```

---

## 预定义常量

```cpp
static const Color Transparent;  // {0.0f, 0.0f, 0.0f, 0.0f}
static const Color Black;        // {0.0f, 0.0f, 0.0f, 1.0f}
static const Color White;        // {1.0f, 1.0f, 1.0f, 1.0f}
static const Color Red;          // {1.0f, 0.0f, 0.0f, 1.0f}
static const Color Green;        // {0.0f, 1.0f, 0.0f, 1.0f}
static const Color Blue;         // {0.0f, 0.0f, 1.0f, 1.0f}
```

**示例**：
```cpp
Color bg = Color::White;
Color text = Color::Black;
```

---

## 使用场景

### 1. 基本颜色指定

```cpp
// 方式 1：浮点构造
Color red{1.0f, 0.0f, 0.0f};

// 方式 2：从 8 位整数
Color green = Color::from_rgba8(0, 255, 0);

// 方式 3：从 16 进制字面值
Color blue = Color::from_rgb_u32(0x0000FF);

// 方式 4：使用预定义常量
Color white = Color::White;
```

---

### 2. Alpha 混合

```cpp
Color blend_colors(Color foreground, Color background) {
    return foreground.blend_over(background);
}

// 半透明窗口
Color window_bg = Color::White.with_alpha(0.95f);
Color desktop = get_desktop_color();
Color final_color = window_bg.blend_over(desktop);
```

---

### 3. 纹理调制

```cpp
void render_sprite(Texture& texture, Color tint) {
    for (int y = 0; y < texture.height; ++y) {
        for (int x = 0; x < texture.width; ++x) {
            Color texel = texture.get_pixel(x, y);
            Color modulated = texel.modulate(tint);
            framebuffer.set_pixel(x, y, modulated);
        }
    }
}

// 红色调
render_sprite(sprite, Color::Red);
```

---

### 4. 颜色插值

```cpp
Color lerp(Color a, Color b, float t) {
    return a * (1.0f - t) + b * t;
}

// 渐变
std::vector<Color> gradient;
for (float t = 0.0f; t <= 1.0f; t += 0.01f) {
    gradient.push_back(lerp(Color::Red, Color::Blue, t));
}
```

---

### 5. 光照计算

```cpp
struct Light {
    Color ambient{0.1f, 0.1f, 0.1f};
    Color diffuse{0.8f, 0.8f, 0.8f};
    Color specular{1.0f, 1.0f, 1.0f};
};

Color calculate_lighting(Color material_color, Light light, float intensity) {
    Color ambient = light.ambient.modulate(material_color);
    Color diffuse = light.diffuse.modulate(material_color) * intensity;
    return ambient + diffuse;
}
```

---

## 性能分析

### 内存布局

```cpp
sizeof(Color) == 16  // 4 * sizeof(float)
alignof(Color) == 4  // float 对齐
```

**与 Vec4 二进制兼容**

---

### 运算性能

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) | 寄存器赋值 |
| `from_rgba8()` | O(1) | 4 次除法（编译期可优化为乘法） |
| `clamped()` | O(1) | 4 次 clamp |
| `premultiplied()` | O(1) | 3 次乘法 |
| `blend_over()` | O(1) | ~15 次浮点运算 + 1 次分支 |
| `to_rgba8()` | O(1) | 4 次乘法 + 4 次取整 + 位运算 |
| 算术运算 | O(1) | 4 次浮点运算 |

---

## 最佳实践

### 1. 使用线性空间进行计算

```cpp
// ✅ 推荐：在线性空间混合
Color linear_blend = lerp(color_a, color_b, t);

// ❌ 不推荐：在 sRGB 空间混合（会导致颜色偏暗）
// （需要先转到线性空间，混合后再转回 sRGB）
```

---

### 2. 预乘 alpha 优化 GPU 渲染

```cpp
// ✅ 推荐：上传前预乘
Color pre = color.premultiplied();
upload_to_gpu(pre);

// GPU 混合模式：(One, OneMinusSrcAlpha)
```

---

### 3. 使用 clamped() 避免溢出

```cpp
// ✅ 推荐：计算后裁剪
Color result = (color1 + color2 + color3).clamped();

// ❌ 不推荐：不裁剪可能导致显示错误
Color result2 = color1 + color2;  // 可能 > 1.0
```

---

## 常见陷阱

### 1. blend_over() 要求未预乘 alpha

```cpp
Color pre = color.premultiplied();
Color result = pre.blend_over(background);  // ❌ 错误：blend_over 期望未预乘颜色
```

---

### 2. 算术运算会影响 alpha

```cpp
Color c{0.5f, 0.5f, 0.5f, 1.0f};
Color doubled = c * 2.0f;  // {1.0f, 1.0f, 1.0f, 2.0f}（alpha 也翻倍！）

// 如需保持 alpha：
Color doubled_correct = c * 2.0f;
doubled_correct.a = c.a;
```

---

## 完整示例

### 示例：UI 颜色主题

```cpp
#include <mine/math/Color.h>

struct Theme {
    Color primary{};
    Color secondary{};
    Color background{};
    Color text{};
    Color accent{};
    
    static Theme dark() {
        return {
            .primary = Color::from_rgb_u32(0x2196F3),     // 蓝色
            .secondary = Color::from_rgb_u32(0x757575),   // 灰色
            .background = Color::from_rgba8(30, 30, 30),  // 深灰
            .text = Color::White,
            .accent = Color::from_rgb_u32(0xFF5722),      // 橙色
        };
    }
    
    static Theme light() {
        return {
            .primary = Color::from_rgb_u32(0x2196F3),
            .secondary = Color::from_rgb_u32(0xBDBDBD),
            .background = Color::White,
            .text = Color::Black,
            .accent = Color::from_rgb_u32(0xFF5722),
        };
    }
    
    Color button_hover(Color base) const {
        return base * 1.1f;  // 提亮 10%
    }
    
    Color button_active(Color base) const {
        return base * 0.9f;  // 变暗 10%
    }
    
    Color disabled(Color base) const {
        return base.with_alpha(0.5f);  // 50% 透明
    }
};

// 使用
void render_button(const Theme& theme, bool hovered) {
    Color bg = theme.primary;
    if (hovered) {
        bg = theme.button_hover(bg);
    }
    bg = bg.clamped();  // 确保不溢出
    fill_rect(button_rect, bg);
}
```

---

## 总结

`Color` 是 `mine.math` 模块的线性空间 RGBA 颜色类型，具备：

1. **线性空间**：适合 GPU 渲染和混合计算
2. **格式转换**：RGBA8, RGB_U32, RGBA_U32 互转
3. **颜色运算**：加减乘、调制、预乘、混合
4. **预定义常量**：常用颜色快速访问

**使用建议**：
- 使用线性空间进行颜色计算
- 上传 GPU 前执行 `premultiplied()`
- 计算后使用 `clamped()` 避免溢出
- 注意算术运算会影响 alpha 分量
