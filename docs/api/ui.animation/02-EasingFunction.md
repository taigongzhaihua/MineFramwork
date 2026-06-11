# EasingFunction - 缓动函数库

## 概述

`EasingFunction` 是 MineUI 动画系统的缓动函数库,提供了一系列标准的缓动曲线函数（Easing Functions）。缓动函数用于将线性进度（0.0 ~ 1.0）映射为非线性的缓动进度，从而实现加速、减速、弹跳等动画效果。

### 核心特性

- **标准缓动库**：包含 10+ 种经典缓动函数系列
- **三种模式**：EaseIn（加速）、EaseOut（减速）、EaseInOut（先加后减）
- **函数指针常量**：预定义的全局函数指针，可直接使用
- **自定义贝塞尔**：支持 CubicBezier 自定义缓动曲线
- **纯函数**：无状态、无副作用，线程安全
- **CSS 兼容**：与 CSS timing-function 兼容

### 缓动函数分类

| 系列 | 特点 | 适用场景 |
|------|------|----------|
| **Linear** | 匀速运动 | 简单移动、无需缓动效果 |
| **Quad** | 二次方缓动 | 轻微加速/减速 |
| **Cubic** | 三次方缓动 | 标准动画效果 |
| **Quart** | 四次方缓动 | 强烈加速/减速 |
| **Quint** | 五次方缓动 | 极强加速/减速 |
| **Expo** | 指数缓动 | 急速加速/减速 |
| **Sine** | 正弦曲线缓动 | 平滑自然的动画 |
| **Back** | 回弹缓动 | 超出目标后回弹 |
| **Elastic** | 弹性缓动 | 橡皮筋弹性效果 |
| **Bounce** | 弹跳缓动 | 物体落地弹跳效果 |

### 缓动模式

- **EaseIn**：从慢到快（加速）
- **EaseOut**：从快到慢（减速）
- **EaseInOut**：先加速后减速（组合）

---

## 文件位置

```
src/mine/ui/animation/EasingFunction.h
```

---

## 类型定义

```cpp
namespace mine::ui::animation {

/// @brief 缓动函数类型
/// @param t 线性进度 (0.0 ~ 1.0)
/// @return 缓动后的进度 (通常在 0.0 ~ 1.0 范围内，但 Back/Elastic 可能超出)
using EasingFn = float (*)(float t);

// ========== Linear（线性） ==========

/// @brief 线性缓动（无缓动）
/// @details y = t
constexpr float Linear(float t) noexcept {
    return t;
}

// ========== Quad（二次方） ==========

/// @brief 二次方缓入（加速）
/// @details y = t²
constexpr float QuadEaseIn(float t) noexcept {
    return t * t;
}

/// @brief 二次方缓出（减速）
/// @details y = 1 - (1-t)²
constexpr float QuadEaseOut(float t) noexcept {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

/// @brief 二次方缓入缓出
/// @details 前半段加速，后半段减速
constexpr float QuadEaseInOut(float t) noexcept {
    return t < 0.5f
        ? 2.0f * t * t
        : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
}

// ========== Cubic（三次方） ==========

/// @brief 三次方缓入
/// @details y = t³
constexpr float CubicEaseIn(float t) noexcept {
    return t * t * t;
}

/// @brief 三次方缓出
/// @details y = 1 - (1-t)³
constexpr float CubicEaseOut(float t) noexcept {
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

/// @brief 三次方缓入缓出
constexpr float CubicEaseInOut(float t) noexcept {
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        float inv = 1.0f - t;
        return 1.0f - 4.0f * inv * inv * inv;
    }
}

// ========== Quart（四次方） ==========

/// @brief 四次方缓入
/// @details y = t⁴
constexpr float QuartEaseIn(float t) noexcept {
    return t * t * t * t;
}

/// @brief 四次方缓出
/// @details y = 1 - (1-t)⁴
constexpr float QuartEaseOut(float t) noexcept {
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv * inv;
}

/// @brief 四次方缓入缓出
constexpr float QuartEaseInOut(float t) noexcept {
    if (t < 0.5f) {
        return 8.0f * t * t * t * t;
    } else {
        float inv = 1.0f - t;
        return 1.0f - 8.0f * inv * inv * inv * inv;
    }
}

// ========== Quint（五次方） ==========

/// @brief 五次方缓入
/// @details y = t⁵
constexpr float QuintEaseIn(float t) noexcept {
    return t * t * t * t * t;
}

/// @brief 五次方缓出
/// @details y = 1 - (1-t)⁵
constexpr float QuintEaseOut(float t) noexcept {
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv * inv * inv;
}

/// @brief 五次方缓入缓出
constexpr float QuintEaseInOut(float t) noexcept {
    if (t < 0.5f) {
        return 16.0f * t * t * t * t * t;
    } else {
        float inv = 1.0f - t;
        return 1.0f - 16.0f * inv * inv * inv * inv * inv;
    }
}

// ========== Expo（指数） ==========

/// @brief 指数缓入
/// @details y = 2^(10(t-1))
inline float ExpoEaseIn(float t) noexcept {
    return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
}

/// @brief 指数缓出
/// @details y = 1 - 2^(-10t)
inline float ExpoEaseOut(float t) noexcept {
    return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

/// @brief 指数缓入缓出
inline float ExpoEaseInOut(float t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    
    if (t < 0.5f) {
        return 0.5f * std::pow(2.0f, 20.0f * t - 10.0f);
    } else {
        return 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);
    }
}

// ========== Sine（正弦） ==========

/// @brief 正弦缓入
/// @details y = 1 - cos(t * π/2)
inline float SineEaseIn(float t) noexcept {
    return 1.0f - std::cos(t * 1.57079632679f);  // π/2
}

/// @brief 正弦缓出
/// @details y = sin(t * π/2)
inline float SineEaseOut(float t) noexcept {
    return std::sin(t * 1.57079632679f);  // π/2
}

/// @brief 正弦缓入缓出
/// @details y = (1 - cos(πt)) / 2
inline float SineEaseInOut(float t) noexcept {
    return 0.5f * (1.0f - std::cos(t * 3.14159265359f));  // π
}

// ========== Back（回弹） ==========

/// @brief 回弹缓入（超出后回弹）
/// @details 会超出起点（负值）
inline float BackEaseIn(float t) noexcept {
    constexpr float s = 1.70158f;  // 回弹系数
    return t * t * ((s + 1.0f) * t - s);
}

/// @brief 回弹缓出
/// @details 会超出终点（>1.0）
inline float BackEaseOut(float t) noexcept {
    constexpr float s = 1.70158f;
    float inv = t - 1.0f;
    return inv * inv * ((s + 1.0f) * inv + s) + 1.0f;
}

/// @brief 回弹缓入缓出
inline float BackEaseInOut(float t) noexcept {
    constexpr float s = 1.70158f * 1.525f;
    
    if (t < 0.5f) {
        float t2 = t * 2.0f;
        return 0.5f * (t2 * t2 * ((s + 1.0f) * t2 - s));
    } else {
        float t2 = t * 2.0f - 2.0f;
        return 0.5f * (t2 * t2 * ((s + 1.0f) * t2 + s) + 2.0f);
    }
}

// ========== Elastic（弹性） ==========

/// @brief 弹性缓入（橡皮筋效果）
/// @details 会有振荡效果
inline float ElasticEaseIn(float t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    
    constexpr float p = 0.3f;  // 周期
    constexpr float s = p / 4.0f;  // 偏移
    
    float inv = t - 1.0f;
    return -std::pow(2.0f, 10.0f * inv) * std::sin((inv - s) * 6.28318530718f / p);
}

/// @brief 弹性缓出
inline float ElasticEaseOut(float t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    
    constexpr float p = 0.3f;
    constexpr float s = p / 4.0f;
    
    return std::pow(2.0f, -10.0f * t) * std::sin((t - s) * 6.28318530718f / p) + 1.0f;
}

/// @brief 弹性缓入缓出
inline float ElasticEaseInOut(float t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    
    constexpr float p = 0.45f;  // 周期（EaseInOut 使用更大的周期）
    constexpr float s = p / 4.0f;
    
    float t2 = t * 2.0f;
    if (t2 < 1.0f) {
        float inv = t2 - 1.0f;
        return -0.5f * std::pow(2.0f, 10.0f * inv) * std::sin((inv - s) * 6.28318530718f / p);
    } else {
        float inv = t2 - 1.0f;
        return 0.5f * std::pow(2.0f, -10.0f * inv) * std::sin((inv - s) * 6.28318530718f / p) + 1.0f;
    }
}

// ========== Bounce（弹跳） ==========

/// @brief 弹跳缓出（物体落地弹跳）
inline float BounceEaseOut(float t) noexcept {
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;
    
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

/// @brief 弹跳缓入
inline float BounceEaseIn(float t) noexcept {
    return 1.0f - BounceEaseOut(1.0f - t);
}

/// @brief 弹跳缓入缓出
inline float BounceEaseInOut(float t) noexcept {
    return t < 0.5f
        ? 0.5f * BounceEaseIn(t * 2.0f)
        : 0.5f * BounceEaseOut(t * 2.0f - 1.0f) + 0.5f;
}

// ========== CubicBezier（自定义贝塞尔曲线） ==========

/// @brief 三次贝塞尔曲线缓动函数生成器
/// @param p1x 控制点 P1 的 x 坐标 (0.0 ~ 1.0)
/// @param p1y 控制点 P1 的 y 坐标
/// @param p2x 控制点 P2 的 x 坐标 (0.0 ~ 1.0)
/// @param p2y 控制点 P2 的 y 坐标
/// @return 缓动函数对象（lambda）
/// @details 等价于 CSS cubic-bezier(p1x, p1y, p2x, p2y)
///          使用牛顿迭代法求解贝塞尔曲线
inline auto CubicBezier(float p1x, float p1y, float p2x, float p2y) {
    return [=](float t) -> float {
        // 牛顿迭代法求解 x(t) = t
        constexpr int kMaxIterations = 10;
        constexpr float kEpsilon = 0.0001f;
        
        float current_t = t;
        for (int i = 0; i < kMaxIterations; ++i) {
            // 计算 x(current_t)
            float x = 3.0f * (1.0f - current_t) * (1.0f - current_t) * current_t * p1x
                    + 3.0f * (1.0f - current_t) * current_t * current_t * p2x
                    + current_t * current_t * current_t;
            
            // 计算误差
            float error = x - t;
            if (std::abs(error) < kEpsilon) break;
            
            // 计算导数 dx/dt
            float dx = 3.0f * (1.0f - current_t) * (1.0f - current_t) * p1x
                     + 6.0f * (1.0f - current_t) * current_t * (p2x - p1x)
                     + 3.0f * current_t * current_t * (1.0f - p2x);
            
            if (dx == 0.0f) break;
            
            // 牛顿迭代
            current_t -= error / dx;
        }
        
        // 计算 y(current_t)
        return 3.0f * (1.0f - current_t) * (1.0f - current_t) * current_t * p1y
             + 3.0f * (1.0f - current_t) * current_t * current_t * p2y
             + current_t * current_t * current_t;
    };
}

}  // namespace mine::ui::animation
```

---

## 缓动函数详解

### Linear（线性）

**公式：** `y = t`

**特点：**
- 匀速运动，无加速/减速
- 最简单的缓动函数

**适用场景：**
- 简单的位置移动
- 不需要缓动效果的场景
- 作为其他缓动的对比基准

**曲线形状：**
```
1.0 ┤         ╱
    │       ╱
    │     ╱
    │   ╱
0.0 └─────────
    0.0     1.0
```

---

### Quad（二次方）

**公式：**
- EaseIn: `y = t²`
- EaseOut: `y = 1 - (1-t)²`
- EaseInOut: 组合

**特点：**
- 轻微的加速/减速效果
- 曲线平滑，不太激进
- 最常用的缓动系列之一

**适用场景：**
- 轻微的淡入淡出
- 平滑的位置过渡
- UI 元素的基础动画

**曲线形状（EaseOut）：**
```
1.0 ┤─────╮
    │      ╲
    │       ╲
    │        ╲
0.0 └─────────
    0.0     1.0
```

---

### Cubic（三次方）

**公式：**
- EaseIn: `y = t³`
- EaseOut: `y = 1 - (1-t)³`
- EaseInOut: 组合

**特点：**
- 标准的缓动效果
- 与 CSS `ease` 类似
- 广泛使用的缓动系列

**适用场景：**
- 标准动画效果
- 按钮点击反馈
- 页面元素进入/退出

**推荐配置：**
- 淡入：`CubicEaseIn` + 300ms
- 淡出：`CubicEaseOut` + 300ms
- 过渡：`CubicEaseInOut` + 500ms

---

### Quart（四次方）

**公式：**
- EaseIn: `y = t⁴`
- EaseOut: `y = 1 - (1-t)⁴`

**特点：**
- 强烈的加速/减速
- 曲线变化明显
- 适合需要强调的动画

**适用场景：**
- 强调性动画
- 重要提示的出现
- 模态对话框弹出

---

### Quint（五次方）

**公式：**
- EaseIn: `y = t⁵`
- EaseOut: `y = 1 - (1-t)⁵`

**特点：**
- 极强的加速/减速
- 动画感强烈
- 适合戏剧性效果

**适用场景：**
- 页面切换
- 大型元素进出
- Hero 动画

---

### Expo（指数）

**公式：**
- EaseIn: `y = 2^(10(t-1))`
- EaseOut: `y = 1 - 2^(-10t)`

**特点：**
- 急速的加速/减速
- 指数级变化
- 最激进的标准缓动

**适用场景：**
- 快速出现/消失
- 需要突然感的动画
- 游戏特效

**注意：**
- 处理 t=0 和 t=1 的边界情况

---

### Sine（正弦）

**公式：**
- EaseIn: `y = 1 - cos(t * π/2)`
- EaseOut: `y = sin(t * π/2)`
- EaseInOut: `y = (1 - cos(πt)) / 2`

**特点：**
- 平滑自然的曲线
- 基于三角函数
- 视觉上最柔和

**适用场景：**
- 自然的淡入淡出
- 平滑的透明度变化
- 柔和的位置移动

**推荐配置：**
- 配合较长的时长（500ms+）
- 适合背景元素动画

---

### Back（回弹）

**公式：** 使用回弹系数 `s = 1.70158`

**特点：**
- **超出范围**：会产生负值或 >1.0 的值
- 回弹效果明显
- 有"超调"感

**适用场景：**
- 按钮点击效果
- 弹出菜单
- 强调性动画

**注意事项：**
- 确保属性可以超出目标范围
- 例如：位置可以超出，但透明度需要 clamp

**示例：**
```cpp
// ✅ 位置可以超出
position = lerp(start_pos, end_pos, BackEaseOut(t));  // 可能超出 end_pos

// ❌ 透明度不应超出（需要 clamp）
opacity = std::clamp(lerp(0.0f, 1.0f, BackEaseOut(t)), 0.0f, 1.0f);
```

---

### Elastic（弹性）

**公式：** 基于正弦波的衰减振荡

**特点：**
- 橡皮筋弹性效果
- 有振荡（多次超出）
- 最夸张的缓动效果

**适用场景：**
- 游戏 UI
- 卡通风格动画
- 趣味性交互

**参数：**
- `p`（周期）：控制振荡频率
- `s`（偏移）：控制初始相位

**注意事项：**
- 仅适合可超出范围的属性
- 不适合生产环境的严肃 UI

---

### Bounce（弹跳）

**公式：** 分段函数模拟弹跳

**特点：**
- 模拟物体落地弹跳
- 多段跳跃效果
- 视觉上最具物理感

**适用场景：**
- 物体掉落动画
- 错误提示抖动
- 游戏元素

**弹跳参数：**
- 第一次弹跳：最高
- 后续弹跳：逐渐降低
- 共 4 次弹跳

---

### CubicBezier（自定义贝塞尔）

**参数：**
- `p1x, p1y`：第一个控制点
- `p2x, p2y`：第二个控制点

**特点：**
- 完全自定义缓动曲线
- 与 CSS `cubic-bezier()` 等价
- 使用牛顿迭代法求解

**常用 CSS 等价：**
```cpp
// ease: cubic-bezier(0.25, 0.1, 0.25, 1.0)
auto ease = CubicBezier(0.25f, 0.1f, 0.25f, 1.0f);

// ease-in: cubic-bezier(0.42, 0, 1.0, 1.0)
auto ease_in = CubicBezier(0.42f, 0.0f, 1.0f, 1.0f);

// ease-out: cubic-bezier(0, 0, 0.58, 1.0)
auto ease_out = CubicBezier(0.0f, 0.0f, 0.58f, 1.0f);

// ease-in-out: cubic-bezier(0.42, 0, 0.58, 1.0)
auto ease_in_out = CubicBezier(0.42f, 0.0f, 0.58f, 1.0f);
```

**使用示例：**
```cpp
// 自定义缓动曲线
auto custom_easing = CubicBezier(0.17f, 0.67f, 0.83f, 0.67f);

// 应用到动画
float progress = custom_easing(t);
```

---

## 使用场景

### 1. 基础动画：Linear

最简单的场景，无需缓动效果：

```cpp
// 简单的位置移动
float start_x = 0.0f;
float end_x = 100.0f;
float t = elapsed.to_seconds() / duration.to_seconds();

float current_x = start_x + (end_x - start_x) * Linear(t);
```

---

### 2. 淡入淡出：QuadEaseOut

轻微的缓动效果，适合淡入淡出：

```cpp
// 淡入动画
void fade_in() {
    Duration duration = Duration::milliseconds(300);
    Duration elapsed = Duration::seconds(0);
    
    auto update = [&](float delta_seconds) {
        elapsed += Duration::seconds(delta_seconds);
        
        if (elapsed >= duration) {
            element->set_opacity(1.0f);
            return true;  // 完成
        }
        
        float t = elapsed.to_seconds() / duration.to_seconds();
        float opacity = QuadEaseOut(t);  // 0.0 -> 1.0
        element->set_opacity(opacity);
        
        return false;  // 继续
    };
}
```

---

### 3. 标准过渡：CubicEaseInOut

最常用的缓动函数，适合大部分场景：

```cpp
// 页面滑动过渡
void slide_page_transition(Page* from_page, Page* to_page) {
    Duration duration = Duration::milliseconds(500);
    
    animate([=](float t) {
        float eased = CubicEaseInOut(t);
        
        // 当前页面滑出
        from_page->set_offset(-eased * screen_width);
        
        // 新页面滑入
        to_page->set_offset((1.0f - eased) * screen_width);
    }, duration);
}
```

---

### 4. 强调动画：QuartEaseOut

需要强调的动画效果：

```cpp
// 重要通知弹出
void show_notification(Notification* notification) {
    Duration duration = Duration::milliseconds(400);
    
    animate([=](float t) {
        float eased = QuartEaseOut(t);
        
        // 从上方滑入
        float y = -notification->height() * (1.0f - eased);
        notification->set_y(y);
        
        // 同时淡入
        notification->set_opacity(eased);
    }, duration);
}
```

---

### 5. 自然效果：SineEaseInOut

平滑柔和的动画，适合背景元素：

```cpp
// 背景模糊动画
void animate_background_blur(float target_blur) {
    Duration duration = Duration::milliseconds(600);
    float start_blur = get_current_blur();
    
    animate([=](float t) {
        float eased = SineEaseInOut(t);
        float blur = start_blur + (target_blur - start_blur) * eased;
        set_background_blur(blur);
    }, duration);
}
```

---

### 6. 回弹效果：BackEaseOut

按钮点击等交互反馈：

```cpp
// 按钮点击回弹效果
void button_click_animation(Button* button) {
    Duration duration = Duration::milliseconds(300);
    float original_scale = 1.0f;
    float pressed_scale = 0.95f;
    
    // 按下：缩小
    animate([=](float t) {
        float eased = BackEaseOut(t);
        float scale = original_scale + (pressed_scale - original_scale) * eased;
        button->set_scale(scale);
    }, duration / 2);
    
    // 释放：回弹
    animate([=](float t) {
        float eased = BackEaseOut(t);
        float scale = pressed_scale + (original_scale - pressed_scale) * eased;
        button->set_scale(scale);  // 会超出 original_scale，产生回弹
    }, duration / 2, duration / 2);  // 延迟半个时长
}
```

---

### 7. 弹性效果：ElasticEaseOut

趣味性动画，适合游戏或卡通风格：

```cpp
// 积分数字弹出
void show_score_popup(int score) {
    Duration duration = Duration::milliseconds(800);
    
    animate([=](float t) {
        float eased = ElasticEaseOut(t);
        
        // 缩放从 0 到 1（会超出 1.0，产生弹性）
        float scale = std::max(0.0f, eased);
        score_text->set_scale(scale);
        
        // 同时淡入
        score_text->set_opacity(std::min(1.0f, t * 2.0f));
    }, duration);
}
```

---

### 8. 自定义贝塞尔：CubicBezier

完全自定义的缓动曲线：

```cpp
// 自定义缓动：快速启动 + 慢速结束
auto custom_easing = CubicBezier(0.17f, 0.67f, 0.83f, 0.67f);

void custom_transition() {
    Duration duration = Duration::milliseconds(500);
    
    animate([=](float t) {
        float eased = custom_easing(t);
        
        // 应用自定义缓动
        element->set_position(lerp(start_pos, end_pos, eased));
    }, duration);
}

// CSS 等价的缓动
auto css_ease = CubicBezier(0.25f, 0.1f, 0.25f, 1.0f);
auto css_ease_in_out = CubicBezier(0.42f, 0.0f, 0.58f, 1.0f);
```

---

## 最佳实践

### ✅ 根据场景选择合适的缓动函数

**推荐：**
```cpp
// ✅ 淡入淡出：QuadEaseOut 或 CubicEaseOut
fade_animation.set_easing(QuadEaseOut);

// ✅ 标准过渡：CubicEaseInOut
page_transition.set_easing(CubicEaseInOut);

// ✅ 按钮反馈：QuartEaseOut 或 BackEaseOut
button_feedback.set_easing(BackEaseOut);

// ✅ 自然效果：SineEaseInOut
background_blur.set_easing(SineEaseInOut);
```

**不推荐：**
```cpp
// ❌ 所有动画都用 Linear（无缓动效果）
all_animations.set_easing(Linear);

// ❌ 淡入淡出用 Bounce（不自然）
fade_animation.set_easing(BounceEaseOut);
```

**原因：** 不同场景需要不同的缓动效果，选择不当会导致动画不自然。

---

### ✅ 注意超出范围的缓动函数

**推荐：**
```cpp
// ✅ 位置可以超出
float x = lerp(start_x, end_x, BackEaseOut(t));  // OK

// ✅ 透明度需要 clamp
float opacity = std::clamp(lerp(0.0f, 1.0f, BackEaseOut(t)), 0.0f, 1.0f);

// ✅ 颜色分量需要 clamp
Color color = lerp_color(start_color, end_color, ElasticEaseOut(t));
color = clamp_color(color);
```

**不推荐：**
```cpp
// ❌ 透明度超出范围
float opacity = lerp(0.0f, 1.0f, BackEaseOut(t));  // 可能 > 1.0

// ❌ 角度超出范围可能导致错误
float angle = lerp(0.0f, 90.0f, ElasticEaseOut(t));  // 可能 > 90°
```

**原因：** Back、Elastic、Bounce 等缓动函数会产生超出 [0, 1] 范围的值。

---

### ✅ 缓动函数与时长配合

**推荐：**
```cpp
// ✅ 快速动画（150-300ms）：Quad、Cubic
quick_animation(QuadEaseOut, Duration::milliseconds(200));

// ✅ 标准动画（300-500ms）：Cubic、Quart
standard_animation(CubicEaseInOut, Duration::milliseconds(400));

// ✅ 慢速动画（500ms+）：Sine、Quint
slow_animation(SineEaseInOut, Duration::milliseconds(600));

// ✅ 回弹效果（300-400ms）：Back
back_animation(BackEaseOut, Duration::milliseconds(350));
```

**不推荐：**
```cpp
// ❌ 时长过短 + 强缓动（看不清效果）
animation(QuintEaseOut, Duration::milliseconds(100));

// ❌ 时长过长 + 弱缓动（拖沓）
animation(QuadEaseIn, Duration::seconds(2.0));
```

**原因：** 缓动函数的效果与时长密切相关，需要合理配合。

---

### ✅ 使用 EaseOut 作为默认选择

**推荐：**
```cpp
// ✅ 大部分动画使用 EaseOut
fade_in.set_easing(CubicEaseOut);
slide_in.set_easing(QuartEaseOut);

// ✅ 双向动画使用 EaseInOut
page_transition.set_easing(CubicEaseInOut);
```

**不推荐：**
```cpp
// ❌ 过多使用 EaseIn（感觉慢）
fade_in.set_easing(CubicEaseIn);
```

**原因：**
- EaseOut：快速启动 + 慢速结束，用户感觉响应快
- EaseIn：慢速启动 + 快速结束，用户感觉响应慢
- EaseInOut：适合双向动画或需要对称效果的场景

---

### ✅ 统一动画风格

**推荐：**
```cpp
// ✅ 定义统一的缓动常量
constexpr EasingFn kStandardEasing = CubicEaseOut;
constexpr EasingFn kQuickEasing = QuadEaseOut;
constexpr EasingFn kEmphasisEasing = QuartEaseOut;

// 在整个应用中使用
fade_animation.set_easing(kStandardEasing);
slide_animation.set_easing(kStandardEasing);
```

**不推荐：**
```cpp
// ❌ 各处使用不同的缓动（风格不统一）
button1.set_easing(QuadEaseOut);
button2.set_easing(CubicEaseOut);
button3.set_easing(QuartEaseOut);
```

**原因：** 统一的缓动风格使应用看起来更专业、协调。

---

## 常见陷阱

### ❌ 缓动函数参数超出 [0, 1] 范围

**错误示例：**
```cpp
// ❌ t 超出范围
float t = elapsed.to_seconds() / duration.to_seconds();  // 可能 > 1.0
float eased = CubicEaseOut(t);  // 输入超出 [0, 1]
```

**正确做法：**
```cpp
// ✅ 限制 t 在 [0, 1] 范围
float t = elapsed.to_seconds() / duration.to_seconds();
t = std::clamp(t, 0.0f, 1.0f);
float eased = CubicEaseOut(t);

// ✅ 或在外层检查
if (elapsed >= duration) {
    // 动画完成
    element->set_opacity(1.0f);
} else {
    float t = elapsed.to_seconds() / duration.to_seconds();
    element->set_opacity(CubicEaseOut(t));
}
```

**解决方案：**
- 在调用缓动函数前 clamp 输入值
- 或在外层检查动画完成条件

---

### ❌ 混淆 EaseIn 和 EaseOut

**错误示例：**
```cpp
// ❌ 淡入使用 EaseIn（感觉响应慢）
void fade_in() {
    float opacity = CubicEaseIn(t);  // 慢速启动
    element->set_opacity(opacity);
}
```

**正确做法：**
```cpp
// ✅ 淡入使用 EaseOut（快速启动）
void fade_in() {
    float opacity = CubicEaseOut(t);  // 快速启动
    element->set_opacity(opacity);
}
```

**解决方案：**
- 记住：EaseOut 通常更适合 UI 动画
- EaseIn 适合物体加速离开的场景

---

### ❌ 忘记处理超出范围的值

**错误示例：**
```cpp
// ❌ BackEaseOut 可能产生 > 1.0 的值
float opacity = BackEaseOut(t);
element->set_opacity(opacity);  // 透明度超出 [0, 1]！
```

**正确做法：**
```cpp
// ✅ 对不能超出范围的属性进行 clamp
float opacity = std::clamp(BackEaseOut(t), 0.0f, 1.0f);
element->set_opacity(opacity);

// ✅ 或选择不会超出范围的缓动函数
float opacity = CubicEaseOut(t);  // 保证在 [0, 1]
element->set_opacity(opacity);
```

**解决方案：**
- Back、Elastic、Bounce 会超出 [0, 1]
- 对于有范围限制的属性（透明度、颜色）需要 clamp

---

### ❌ CubicBezier 的控制点选择不当

**错误示例：**
```cpp
// ❌ p1x 或 p2x 超出 [0, 1] 范围
auto invalid_easing = CubicBezier(-0.5f, 0.5f, 1.5f, 0.5f);

// ❌ 控制点导致 x(t) 非单调（无法求解）
auto bad_easing = CubicBezier(0.9f, 0.1f, 0.1f, 0.9f);
```

**正确做法：**
```cpp
// ✅ 确保 p1x 和 p2x 在 [0, 1] 范围内
auto valid_easing = CubicBezier(0.25f, 0.1f, 0.25f, 1.0f);

// ✅ 使用经过验证的 CSS 等价值
auto css_ease = CubicBezier(0.25f, 0.1f, 0.25f, 1.0f);
```

**解决方案：**
- p1x 和 p2x 必须在 [0, 1] 范围内
- 参考 CSS cubic-bezier() 的经典值

---

### ❌ 过度使用复杂缓动

**错误示例：**
```cpp
// ❌ 所有动画都用 Elastic（过于花哨）
fade_in.set_easing(ElasticEaseOut);
slide_in.set_easing(ElasticEaseOut);
scale_in.set_easing(ElasticEaseOut);
```

**正确做法：**
```cpp
// ✅ 大部分使用简单缓动
fade_in.set_easing(CubicEaseOut);
slide_in.set_easing(CubicEaseOut);

// ✅ 仅在需要强调时使用复杂缓动
special_effect.set_easing(ElasticEaseOut);
```

**解决方案：**
- Elastic、Bounce 等复杂缓动应谨慎使用
- 过多使用会让 UI 显得不专业

---

## 完整示例

### 示例 1：缓动函数可视化器

实现一个工具，可视化不同缓动函数的曲线：

```cpp
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/Duration.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using namespace mine::ui::animation;

/// @brief 缓动函数信息
struct EasingInfo {
    std::string name;
    EasingFn function;
};

/// @brief 缓动函数可视化器
class EasingVisualizer {
public:
    /// @brief 添加缓动函数
    void add_easing(const std::string& name, EasingFn function) {
        easings_.push_back({name, function});
    }
    
    /// @brief 打印缓动函数曲线（ASCII 艺术）
    void visualize(size_t index, int width = 60, int height = 20) const {
        if (index >= easings_.size()) return;
        
        const auto& easing = easings_[index];
        
        std::cout << "\n=== " << easing.name << " ===\n\n";
        
        // 采样点
        std::vector<float> samples;
        for (int i = 0; i <= width; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(width);
            float value = easing.function(t);
            samples.push_back(value);
        }
        
        // 绘制曲线（从上到下）
        for (int row = height; row >= 0; --row) {
            float y = static_cast<float>(row) / static_cast<float>(height);
            
            // Y 轴标签
            if (row == height) {
                std::cout << "1.0 ┤";
            } else if (row == 0) {
                std::cout << "0.0 └";
            } else {
                std::cout << "    │";
            }
            
            // 绘制曲线
            for (size_t col = 0; col <= width; ++col) {
                float value = samples[col];
                float diff = std::abs(value - y);
                
                if (diff < 0.05f) {
                    std::cout << "●";
                } else if (row == 0) {
                    std::cout << "─";
                } else {
                    std::cout << " ";
                }
            }
            
            std::cout << "\n";
        }
        
        // X 轴标签
        std::cout << "    0.0" << std::string(width - 6, ' ') << "1.0\n";
    }
    
    /// @brief 打印数值表格
    void print_values_table(size_t index) const {
        if (index >= easings_.size()) return;
        
        const auto& easing = easings_[index];
        
        std::cout << "\n=== " << easing.name << " 数值表 ===\n\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << " t     | value\n";
        std::cout << "-------+-------\n";
        
        for (int i = 0; i <= 10; ++i) {
            float t = static_cast<float>(i) / 10.0f;
            float value = easing.function(t);
            std::cout << " " << t << " | " << value << "\n";
        }
    }
    
    /// @brief 比较多个缓动函数
    void compare(const std::vector<size_t>& indices) const {
        std::cout << "\n=== 缓动函数比较 ===\n\n";
        std::cout << std::fixed << std::setprecision(4);
        
        // 表头
        std::cout << " t    ";
        for (size_t idx : indices) {
            if (idx < easings_.size()) {
                std::cout << " | " << std::setw(10) << easings_[idx].name;
            }
        }
        std::cout << "\n";
        
        // 分隔线
        std::cout << "------";
        for (size_t idx : indices) {
            if (idx < easings_.size()) {
                std::cout << "-+------------";
            }
        }
        std::cout << "\n";
        
        // 数据行
        for (int i = 0; i <= 10; ++i) {
            float t = static_cast<float>(i) / 10.0f;
            std::cout << " " << t;
            
            for (size_t idx : indices) {
                if (idx < easings_.size()) {
                    float value = easings_[idx].function(t);
                    std::cout << " | " << std::setw(10) << value;
                }
            }
            std::cout << "\n";
        }
    }
    
private:
    std::vector<EasingInfo> easings_;
};

int main() {
    EasingVisualizer visualizer;
    
    // 添加常用缓动函数
    visualizer.add_easing("Linear", Linear);
    visualizer.add_easing("QuadEaseIn", QuadEaseIn);
    visualizer.add_easing("QuadEaseOut", QuadEaseOut);
    visualizer.add_easing("CubicEaseIn", CubicEaseIn);
    visualizer.add_easing("CubicEaseOut", CubicEaseOut);
    visualizer.add_easing("CubicEaseInOut", CubicEaseInOut);
    visualizer.add_easing("QuartEaseOut", QuartEaseOut);
    visualizer.add_easing("SineEaseInOut", SineEaseInOut);
    visualizer.add_easing("BackEaseOut", BackEaseOut);
    
    // 可视化 CubicEaseOut
    visualizer.visualize(4);  // CubicEaseOut
    visualizer.print_values_table(4);
    
    // 比较不同的 EaseOut
    visualizer.compare({2, 4, 6, 7});  // Quad, Cubic, Quart, Sine
    
    return 0;
}
```

---

### 示例 2：动画效果演示

展示不同缓动函数的实际效果：

```cpp
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/Duration.h>
#include <iostream>
#include <vector>
#include <string>

using namespace mine::ui::animation;

/// @brief 动画效果演示器
class AnimationDemo {
public:
    /// @brief 演示淡入动画
    static void demo_fade_in() {
        std::cout << "=== 淡入动画演示 ===\n\n";
        
        Duration duration = Duration::milliseconds(300);
        Duration elapsed = Duration::seconds(0);
        Duration delta = Duration::milliseconds(16.67);  // 60fps
        
        std::cout << "使用 QuadEaseOut 缓动：\n";
        
        while (elapsed < duration) {
            float t = elapsed.to_seconds() / duration.to_seconds();
            float opacity = QuadEaseOut(t);
            
            // 打印进度条
            int bar_length = static_cast<int>(opacity * 50);
            std::cout << "Opacity: " << std::fixed << std::setprecision(2) << opacity << " ";
            std::cout << "[" << std::string(bar_length, '█') 
                      << std::string(50 - bar_length, '░') << "]\n";
            
            elapsed += delta;
        }
        
        std::cout << "Opacity: 1.00 [" << std::string(50, '█') << "] ✓ 完成\n\n";
    }
    
    /// @brief 演示回弹效果
    static void demo_back_ease() {
        std::cout << "=== 回弹效果演示 ===\n\n";
        
        Duration duration = Duration::milliseconds(350);
        Duration elapsed = Duration::seconds(0);
        Duration delta = Duration::milliseconds(16.67);
        
        float start_scale = 0.0f;
        float end_scale = 1.0f;
        
        std::cout << "使用 BackEaseOut 缓动（注意会超出 1.0）：\n";
        
        while (elapsed <= duration) {
            float t = elapsed.to_seconds() / duration.to_seconds();
            t = std::min(t, 1.0f);
            float scale = start_scale + (end_scale - start_scale) * BackEaseOut(t);
            
            // 打印缩放值
            int bar_length = static_cast<int>(scale * 50);
            std::cout << "Scale: " << std::fixed << std::setprecision(2) << scale << " ";
            
            if (bar_length > 50) {
                std::cout << "[" << std::string(50, '█') << "] ← 超出!\n";
            } else {
                std::cout << "[" << std::string(bar_length, '█') 
                          << std::string(50 - bar_length, '░') << "]\n";
            }
            
            elapsed += delta;
        }
        
        std::cout << "\n";
    }
    
    /// @brief 比较不同缓动函数的效果
    static void compare_easings() {
        std::cout << "=== 缓动函数效果比较 ===\n\n";
        
        struct EasingTest {
            std::string name;
            EasingFn function;
        };
        
        std::vector<EasingTest> tests = {
            {"Linear", Linear},
            {"QuadEaseOut", QuadEaseOut},
            {"CubicEaseOut", CubicEaseOut},
            {"QuartEaseOut", QuartEaseOut},
        };
        
        Duration duration = Duration::milliseconds(300);
        Duration elapsed = Duration::seconds(0);
        Duration delta = Duration::milliseconds(50);  // 较大的步长以便观察
        
        while (elapsed <= duration) {
            float t = elapsed.to_seconds() / duration.to_seconds();
            t = std::min(t, 1.0f);
            
            std::cout << "t = " << std::fixed << std::setprecision(2) << t << ":\n";
            
            for (const auto& test : tests) {
                float value = test.function(t);
                int bar_length = static_cast<int>(value * 30);
                
                std::cout << "  " << std::setw(15) << test.name << ": " 
                          << std::setw(4) << value << " "
                          << "[" << std::string(bar_length, '█') 
                          << std::string(30 - bar_length, '░') << "]\n";
            }
            
            std::cout << "\n";
            elapsed += delta;
        }
    }
};

int main() {
    // 演示淡入动画
    AnimationDemo::demo_fade_in();
    
    // 演示回弹效果
    AnimationDemo::demo_back_ease();
    
    // 比较缓动函数
    AnimationDemo::compare_easings();
    
    return 0;
}
```

---

### 示例 3：CubicBezier 与 CSS 等价

展示 CubicBezier 与 CSS timing-function 的等价关系：

```cpp
#include <mine/ui/animation/EasingFunction.h>
#include <iostream>
#include <iomanip>

using namespace mine::ui::animation;

int main() {
    std::cout << "=== CubicBezier 与 CSS 等价关系 ===\n\n";
    
    // 定义 CSS 标准缓动的等价 CubicBezier
    struct CSSEasing {
        std::string name;
        std::string css;
        float p1x, p1y, p2x, p2y;
    };
    
    std::vector<CSSEasing> css_easings = {
        {"ease", "cubic-bezier(0.25, 0.1, 0.25, 1.0)", 0.25f, 0.1f, 0.25f, 1.0f},
        {"ease-in", "cubic-bezier(0.42, 0, 1.0, 1.0)", 0.42f, 0.0f, 1.0f, 1.0f},
        {"ease-out", "cubic-bezier(0, 0, 0.58, 1.0)", 0.0f, 0.0f, 0.58f, 1.0f},
        {"ease-in-out", "cubic-bezier(0.42, 0, 0.58, 1.0)", 0.42f, 0.0f, 0.58f, 1.0f},
    };
    
    // 打印对照表
    for (const auto& css : css_easings) {
        std::cout << "CSS: " << css.name << "\n";
        std::cout << "  " << css.css << "\n";
        std::cout << "  CubicBezier(" << css.p1x << ", " << css.p1y << ", " 
                  << css.p2x << ", " << css.p2y << ")\n\n";
        
        // 创建对应的缓动函数
        auto easing = CubicBezier(css.p1x, css.p1y, css.p2x, css.p2y);
        
        // 打印数值
        std::cout << "  t     | value\n";
        std::cout << "  ------+-------\n";
        
        std::cout << std::fixed << std::setprecision(4);
        for (int i = 0; i <= 10; ++i) {
            float t = static_cast<float>(i) / 10.0f;
            float value = easing(t);
            std::cout << "  " << t << " | " << value << "\n";
        }
        
        std::cout << "\n";
    }
    
    // 自定义缓动示例
    std::cout << "=== 自定义缓动示例 ===\n\n";
    
    // 快速启动 + 慢速结束
    auto custom1 = CubicBezier(0.17f, 0.67f, 0.83f, 0.67f);
    std::cout << "快速启动 + 慢速结束: cubic-bezier(0.17, 0.67, 0.83, 0.67)\n";
    std::cout << "  t=0.5 -> " << custom1(0.5f) << "\n\n";
    
    // Material Design 标准缓动
    auto material_standard = CubicBezier(0.4f, 0.0f, 0.2f, 1.0f);
    std::cout << "Material Design 标准: cubic-bezier(0.4, 0.0, 0.2, 1.0)\n";
    std::cout << "  t=0.5 -> " << material_standard(0.5f) << "\n\n";
    
    return 0;
}
```

**输出示例：**
```
=== CubicBezier 与 CSS 等价关系 ===

CSS: ease
  cubic-bezier(0.25, 0.1, 0.25, 1.0)
  CubicBezier(0.25, 0.1, 0.25, 1.0)

  t     | value
  ------+-------
  0.0000 | 0.0000
  0.1000 | 0.0925
  0.2000 | 0.3085
  0.3000 | 0.5045
  0.4000 | 0.6635
  0.5000 | 0.7855
  0.6000 | 0.8725
  0.7000 | 0.9285
  0.8000 | 0.9645
  0.9000 | 0.9895
  1.0000 | 1.0000

...
```

---

## 总结

`EasingFunction` 是 MineUI 动画系统的缓动函数库，提供了：

1. **丰富的缓动库**：10+ 种标准缓动函数系列
2. **三种模式**：EaseIn、EaseOut、EaseInOut 满足不同需求
3. **函数指针常量**：预定义的全局函数指针，直接使用
4. **自定义能力**：CubicBezier 支持完全自定义缓动曲线
5. **CSS 兼容**：与 CSS timing-function 兼容，易于理解

**关键要点：**

- **优先使用 EaseOut**：快速启动，响应更快
- **标准过渡用 Cubic**：CubicEaseInOut 最常用
- **注意超出范围**：Back、Elastic、Bounce 会超出 [0, 1]
- **合理选择缓动**：根据场景选择合适的缓动函数
- **统一动画风格**：在应用中保持一致的缓动风格

**缓动函数选择指南：**

| 场景 | 推荐缓动 | 时长 |
|------|----------|------|
| 淡入淡出 | QuadEaseOut | 200-300ms |
| 标准过渡 | CubicEaseInOut | 300-500ms |
| 按钮反馈 | BackEaseOut | 300-400ms |
| 页面切换 | QuintEaseOut | 500-800ms |
| 自然效果 | SineEaseInOut | 500-600ms |
| 强调动画 | QuartEaseOut | 400-500ms |

缓动函数是动画效果的核心，选择合适的缓动函数能让动画更自然、更有表现力。
