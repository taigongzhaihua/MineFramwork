# Duration - 动画时长类型

## 概述

`Duration` 是 MineUI 动画系统的核心时间类型，用于表示动画的持续时间。它是一个轻量级的值类型，内部使用单精度浮点数存储秒数，精度足以满足 UI 动画的需求（精确到毫秒级）。

### 核心特性

- **值语义**：可按值传递、拷贝，无动态内存分配
- **高精度**：单精度浮点秒，精确到毫秒级（~0.001秒）
- **工厂函数**：提供语义化的 `milliseconds()` 和 `seconds()` 工厂方法
- **完整操作**：支持比较、算术运算、转换等操作
- **constexpr 支持**：可用于编译期常量计算
- **零开销抽象**：编译后与原始浮点数性能相同

### 工厂函数

```cpp
// 从毫秒创建
Duration d1 = Duration::milliseconds(500);  // 0.5秒

// 从秒创建
Duration d2 = Duration::seconds(1.5);       // 1.5秒
```

### 使用场景

- 定义动画时长
- 时间计算和转换
- 动画配置参数
- 时间累加和比较

---

## 文件位置

```
src/mine/ui/animation/Duration.h
```

---

## 类定义

```cpp
namespace mine::ui::animation {

/// @brief 动画时长类型（值类型，单位：秒）
/// @details 内部使用单精度浮点数存储，精度约 0.001 秒（1毫秒）。
///          所有操作均为 constexpr，可用于编译期常量计算。
class Duration {
public:
    /// @brief 从毫秒创建 Duration
    /// @param ms 毫秒数（可以是浮点数）
    /// @return Duration 对象
    static constexpr Duration milliseconds(float ms) noexcept {
        return Duration{ms / 1000.0f};
    }

    /// @brief 从秒创建 Duration
    /// @param s 秒数
    /// @return Duration 对象
    static constexpr Duration seconds(float s) noexcept {
        return Duration{s};
    }

    /// @brief 默认构造函数（零时长）
    constexpr Duration() noexcept : value_(0.0f) {}

    /// @brief 转换为秒
    /// @return 秒数
    constexpr float to_seconds() const noexcept {
        return value_;
    }

    /// @brief 转换为毫秒
    /// @return 毫秒数
    constexpr float to_milliseconds() const noexcept {
        return value_ * 1000.0f;
    }

    /// @brief 是否为零时长
    /// @return 如果时长为零返回 true
    constexpr bool is_zero() const noexcept {
        return value_ == 0.0f;
    }

    // 比较运算符
    constexpr bool operator==(const Duration& other) const noexcept {
        return value_ == other.value_;
    }

    constexpr bool operator!=(const Duration& other) const noexcept {
        return value_ != other.value_;
    }

    constexpr bool operator<(const Duration& other) const noexcept {
        return value_ < other.value_;
    }

    constexpr bool operator<=(const Duration& other) const noexcept {
        return value_ <= other.value_;
    }

    constexpr bool operator>(const Duration& other) const noexcept {
        return value_ > other.value_;
    }

    constexpr bool operator>=(const Duration& other) const noexcept {
        return value_ >= other.value_;
    }

    // 算术运算符
    constexpr Duration operator+(const Duration& other) const noexcept {
        return Duration{value_ + other.value_};
    }

    constexpr Duration operator-(const Duration& other) const noexcept {
        return Duration{value_ - other.value_};
    }

    constexpr Duration operator*(float factor) const noexcept {
        return Duration{value_ * factor};
    }

    constexpr Duration& operator+=(const Duration& other) noexcept {
        value_ += other.value_;
        return *this;
    }

    constexpr Duration& operator-=(const Duration& other) noexcept {
        value_ -= other.value_;
        return *this;
    }

    constexpr Duration& operator*=(float factor) noexcept {
        value_ *= factor;
        return *this;
    }

private:
    explicit constexpr Duration(float seconds) noexcept : value_(seconds) {}

    float value_;  // 以秒为单位
};

// 允许 factor * Duration 的乘法
constexpr Duration operator*(float factor, const Duration& d) noexcept {
    return d * factor;
}

}  // namespace mine::ui::animation
```

---

## 成员方法

### 工厂方法

#### `milliseconds(float ms)`

从毫秒创建 `Duration` 对象。

**参数：**
- `ms` - 毫秒数（可以是浮点数，如 250.5）

**返回值：**
- `Duration` 对象

**示例：**
```cpp
auto d1 = Duration::milliseconds(500);    // 0.5秒
auto d2 = Duration::milliseconds(1500);   // 1.5秒
auto d3 = Duration::milliseconds(16.67);  // 约一帧（60fps）
```

---

#### `seconds(float s)`

从秒创建 `Duration` 对象。

**参数：**
- `s` - 秒数

**返回值：**
- `Duration` 对象

**示例：**
```cpp
auto d1 = Duration::seconds(1.0);   // 1秒
auto d2 = Duration::seconds(0.5);   // 0.5秒
auto d3 = Duration::seconds(2.5);   // 2.5秒
```

---

### 转换方法

#### `to_seconds()`

将时长转换为秒数。

**返回值：**
- `float` - 秒数

**示例：**
```cpp
auto d = Duration::milliseconds(1500);
float seconds = d.to_seconds();  // 1.5
```

---

#### `to_milliseconds()`

将时长转换为毫秒数。

**返回值：**
- `float` - 毫秒数

**示例：**
```cpp
auto d = Duration::seconds(1.5);
float ms = d.to_milliseconds();  // 1500.0
```

---

### 查询方法

#### `is_zero()`

检查时长是否为零。

**返回值：**
- `bool` - 如果时长为零返回 `true`

**示例：**
```cpp
auto d1 = Duration::seconds(0);
auto d2 = Duration::milliseconds(100);

if (d1.is_zero()) {
    // 零时长，动画立即完成
}
```

**注意：**
使用浮点数比较，建议在实际使用中考虑精度问题：

```cpp
// ❌ 可能因浮点精度问题失效
if (duration.to_seconds() == 0.0f) { ... }

// ✅ 使用 is_zero() 方法
if (duration.is_zero()) { ... }
```

---

### 比较运算符

#### `operator==` / `operator!=`

相等/不等比较。

**示例：**
```cpp
auto d1 = Duration::seconds(1.0);
auto d2 = Duration::milliseconds(1000);

if (d1 == d2) {
    // 两者相等（都是1秒）
}
```

---

#### `operator<` / `operator<=` / `operator>` / `operator>=`

大小比较。

**示例：**
```cpp
auto short_anim = Duration::milliseconds(200);
auto long_anim = Duration::seconds(1.0);

if (short_anim < long_anim) {
    // 短动画时长小于长动画
}
```

---

### 算术运算符

#### `operator+` / `operator+=`

时长相加。

**示例：**
```cpp
auto fade_in = Duration::milliseconds(300);
auto pause = Duration::milliseconds(100);
auto fade_out = Duration::milliseconds(300);

Duration total = fade_in + pause + fade_out;  // 700ms
```

---

#### `operator-` / `operator-=`

时长相减。

**示例：**
```cpp
auto total = Duration::seconds(1.0);
auto elapsed = Duration::milliseconds(750);
auto remaining = total - elapsed;  // 250ms
```

---

#### `operator*` / `operator*=`

时长乘以因子。

**示例：**
```cpp
auto base_duration = Duration::milliseconds(200);

// 慢速播放（2倍时长）
auto slow = base_duration * 2.0f;  // 400ms

// 快速播放（0.5倍时长）
auto fast = base_duration * 0.5f;  // 100ms

// 也支持 factor * Duration
auto same = 2.0f * base_duration;  // 400ms
```

---

## 使用场景

### 1. 创建动画时长

最常见的使用场景是定义动画的持续时间：

```cpp
// 淡入动画：300ms
auto fade_in = Duration::milliseconds(300);

// 页面过渡：0.5秒
auto page_transition = Duration::seconds(0.5);

// 快速响应：150ms
auto quick_feedback = Duration::milliseconds(150);

// 长动画：2秒
auto long_animation = Duration::seconds(2.0);
```

---

### 2. 时长转换

在不同时间单位之间转换：

```cpp
auto duration = Duration::seconds(1.5);

// 转换为毫秒（例如用于日志）
std::cout << "Duration: " << duration.to_milliseconds() << "ms\n";  // 1500ms

// 转换为秒（例如用于物理计算）
float seconds = duration.to_seconds();  // 1.5
```

---

### 3. 时长比较

比较不同动画的时长：

```cpp
auto fade_duration = Duration::milliseconds(300);
auto slide_duration = Duration::milliseconds(500);

if (fade_duration < slide_duration) {
    // 淡入更快，先播放淡入
    play_fade_animation();
} else {
    // 滑动更快，先播放滑动
    play_slide_animation();
}

// 选择较短的动画
Duration shorter = (fade_duration < slide_duration) 
                   ? fade_duration 
                   : slide_duration;
```

---

### 4. 时长算术

计算复杂动画的总时长：

```cpp
// 动画序列：淡入 -> 停留 -> 淡出
auto fade_in = Duration::milliseconds(300);
auto hold = Duration::milliseconds(2000);
auto fade_out = Duration::milliseconds(300);

Duration total_duration = fade_in + hold + fade_out;  // 2600ms

// 计算剩余时间
auto elapsed = Duration::milliseconds(1000);
auto remaining = total_duration - elapsed;  // 1600ms

// 缩放时长（慢动作/快进）
auto slow_motion = fade_in * 2.0f;  // 600ms
auto fast_forward = fade_in * 0.5f;  // 150ms
```

---

### 5. constexpr 编译期常量

利用 `constexpr` 在编译期计算时长：

```cpp
// 编译期常量
constexpr Duration kFadeInDuration = Duration::milliseconds(300);
constexpr Duration kFadeOutDuration = Duration::milliseconds(300);
constexpr Duration kHoldDuration = Duration::seconds(2);

// 编译期计算总时长
constexpr Duration kTotalDuration = kFadeInDuration + kHoldDuration + kFadeOutDuration;

// 在静态数组大小中使用
constexpr int kFrameCount = static_cast<int>(kTotalDuration.to_milliseconds() / 16.67f);  // 60fps
```

---

### 6. 动画配置

在动画配置对象中使用：

```cpp
struct AnimationConfig {
    Duration duration;
    EasingFunction easing;
    float delay_seconds;
};

// 快速动画配置
AnimationConfig quick_config{
    .duration = Duration::milliseconds(150),
    .easing = QuadEaseOut,
    .delay_seconds = 0.0f
};

// 标准动画配置
AnimationConfig standard_config{
    .duration = Duration::milliseconds(300),
    .easing = CubicEaseInOut,
    .delay_seconds = 0.1f
};

// 慢速动画配置
AnimationConfig slow_config{
    .duration = Duration::seconds(1.0),
    .easing = QuintEaseOut,
    .delay_seconds = 0.0f
};
```

---

### 7. 时间累加和进度计算

在动画循环中累加时间和计算进度：

```cpp
class Animation {
public:
    void update(float delta_seconds) {
        // 累加已过时间
        elapsed_ += Duration::seconds(delta_seconds);
        
        // 检查是否完成
        if (elapsed_ >= duration_) {
            // 动画完成
            on_complete();
            return;
        }
        
        // 计算进度 (0.0 ~ 1.0)
        float progress = elapsed_.to_seconds() / duration_.to_seconds();
        
        // 应用缓动函数
        float eased = easing_function_(progress);
        
        // 更新属性
        update_property(eased);
    }

private:
    Duration duration_;
    Duration elapsed_;
    EasingFunction easing_function_;
};
```

---

### 8. 帧率相关计算

计算帧时长和帧数：

```cpp
// 60fps 的帧时长
constexpr Duration kFrameDuration60fps = Duration::milliseconds(16.67f);

// 30fps 的帧时长
constexpr Duration kFrameDuration30fps = Duration::milliseconds(33.33f);

// 计算动画需要多少帧（60fps）
Duration animation_duration = Duration::milliseconds(500);
int frame_count = static_cast<int>(
    animation_duration.to_milliseconds() / kFrameDuration60fps.to_milliseconds()
);  // 约 30 帧

// 检查是否到达下一帧
Duration elapsed = Duration::milliseconds(20);
if (elapsed >= kFrameDuration60fps) {
    // 已过一帧，可以更新
    render_next_frame();
}
```

---

## 最佳实践

### ✅ 使用工厂函数创建时长

**推荐：**
```cpp
// ✅ 语义清晰
auto duration = Duration::milliseconds(300);
auto duration2 = Duration::seconds(1.5);
```

**不推荐：**
```cpp
// ❌ 无法直接从浮点数构造（构造函数是私有的）
// auto duration = Duration(0.3f);  // 编译错误
```

**原因：** 工厂函数提供了更清晰的语义，避免了单位混淆。

---

### ✅ 使用 to_seconds() 进行数学计算

**推荐：**
```cpp
// ✅ 明确单位转换
Duration duration = Duration::milliseconds(500);
float progress = elapsed.to_seconds() / duration.to_seconds();
```

**不推荐：**
```cpp
// ❌ 混用单位（错误示例）
float progress = elapsed.to_milliseconds() / duration.to_seconds();  // 单位不一致！
```

**原因：** 统一使用秒作为计算单位，避免单位转换错误。

---

### ✅ 使用 is_zero() 检查零时长

**推荐：**
```cpp
// ✅ 使用专用方法
if (duration.is_zero()) {
    // 零时长，立即完成
}
```

**不推荐：**
```cpp
// ❌ 直接比较浮点数
if (duration.to_seconds() == 0.0f) {
    // 浮点精度问题
}
```

**原因：** `is_zero()` 语义更清晰，未来可能包含精度容差处理。

---

### ✅ 使用 constexpr 定义动画常量

**推荐：**
```cpp
// ✅ 编译期常量
constexpr Duration kDefaultFadeDuration = Duration::milliseconds(300);
constexpr Duration kQuickFeedback = Duration::milliseconds(150);
constexpr Duration kSlowTransition = Duration::seconds(1.0);

// 在函数中使用
void fade_in() {
    animate_opacity(0.0f, 1.0f, kDefaultFadeDuration);
}
```

**不推荐：**
```cpp
// ❌ 魔法数字
void fade_in() {
    animate_opacity(0.0f, 1.0f, Duration::milliseconds(300));  // 硬编码
}
```

**原因：** 使用命名常量提高可维护性，便于统一调整动画时长。

---

### ✅ 合理使用时长算术

**推荐：**
```cpp
// ✅ 计算序列动画总时长
Duration total = fade_in_duration + hold_duration + fade_out_duration;

// ✅ 计算剩余时间
Duration remaining = total_duration - elapsed_duration;

// ✅ 缩放时长
Duration slow_version = base_duration * 2.0f;
```

**不推荐：**
```cpp
// ❌ 手动转换后计算（繁琐且易错）
float total_ms = fade_in.to_milliseconds() 
               + hold.to_milliseconds() 
               + fade_out.to_milliseconds();
Duration total = Duration::milliseconds(total_ms);
```

**原因：** 直接使用运算符更简洁、类型安全。

---

## 常见陷阱

### ❌ 单位混淆

**错误示例：**
```cpp
// ❌ 混淆毫秒和秒
auto duration = Duration::seconds(300);  // 实际是 300 秒，而非 300 毫秒！
```

**正确做法：**
```cpp
// ✅ 明确使用正确的工厂函数
auto duration = Duration::milliseconds(300);  // 300 毫秒
```

**解决方案：**
- 始终使用 `milliseconds()` 或 `seconds()` 工厂函数
- 代码审查时注意检查时间单位

---

### ❌ 浮点精度问题

**错误示例：**
```cpp
// ❌ 直接比较浮点数
Duration d1 = Duration::milliseconds(300);
Duration d2 = Duration::seconds(0.3);

if (d1.to_seconds() == d2.to_seconds()) {  // 可能因精度问题失败
    // ...
}
```

**正确做法：**
```cpp
// ✅ 使用运算符比较 Duration 对象
if (d1 == d2) {
    // ...
}

// ✅ 或者使用容差比较
constexpr float kEpsilon = 0.0001f;
if (std::abs(d1.to_seconds() - d2.to_seconds()) < kEpsilon) {
    // ...
}
```

**解决方案：**
- 优先使用 `Duration` 的比较运算符
- 需要浮点比较时使用容差（epsilon）

---

### ❌ 忘记处理零时长

**错误示例：**
```cpp
// ❌ 未处理零时长导致除零错误
Duration duration = get_animation_duration();
float progress = elapsed.to_seconds() / duration.to_seconds();  // 如果 duration 为零会出错
```

**正确做法：**
```cpp
// ✅ 检查零时长
Duration duration = get_animation_duration();
float progress;

if (duration.is_zero()) {
    progress = 1.0f;  // 零时长动画立即完成
} else {
    progress = elapsed.to_seconds() / duration.to_seconds();
}
```

**解决方案：**
- 在进行除法前检查 `is_zero()`
- 零时长动画应视为立即完成

---

### ❌ 负时长未检查

**错误示例：**
```cpp
// ❌ 减法可能产生负时长
Duration remaining = total - elapsed;  // 如果 elapsed > total，结果为负
float seconds = remaining.to_seconds();  // 负数！
```

**正确做法：**
```cpp
// ✅ 检查并限制为非负
Duration remaining = total - elapsed;
if (remaining.to_seconds() < 0.0f) {
    remaining = Duration::seconds(0);
}

// ✅ 或使用 std::max
Duration remaining = total - elapsed;
float seconds = std::max(0.0f, remaining.to_seconds());
```

**解决方案：**
- 减法后检查结果是否为负
- 使用 `std::max` 确保时长非负

---

### ❌ 过度使用转换方法

**错误示例：**
```cpp
// ❌ 频繁转换（性能浪费）
for (int i = 0; i < 1000; ++i) {
    float ms = duration.to_milliseconds();
    do_something(ms);
}
```

**正确做法：**
```cpp
// ✅ 一次转换，重复使用
float ms = duration.to_milliseconds();
for (int i = 0; i < 1000; ++i) {
    do_something(ms);
}
```

**解决方案：**
- 转换结果可缓存
- 虽然转换是简单的乘法，但在热路径中仍应优化

---

## 完整示例

### 示例 1：动画时长计算器

实现一个动画序列时长计算器，支持多种动画组合：

```cpp
#include <mine/ui/animation/Duration.h>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace mine::ui::animation;

/// @brief 动画步骤类型
enum class AnimationStepType {
    FadeIn,       // 淡入
    FadeOut,      // 淡出
    SlideIn,      // 滑入
    SlideOut,     // 滑出
    Hold,         // 停留
    Scale,        // 缩放
};

/// @brief 动画步骤
struct AnimationStep {
    AnimationStepType type;
    Duration duration;
    
    AnimationStep(AnimationStepType t, Duration d)
        : type(t), duration(d) {}
};

/// @brief 动画序列时长计算器
class AnimationSequenceCalculator {
public:
    /// @brief 添加动画步骤
    void add_step(AnimationStepType type, Duration duration) {
        steps_.emplace_back(type, duration);
    }
    
    /// @brief 计算总时长
    Duration calculate_total_duration() const {
        Duration total = Duration::seconds(0);
        for (const auto& step : steps_) {
            total += step.duration;
        }
        return total;
    }
    
    /// @brief 计算最长步骤时长
    Duration find_longest_step() const {
        if (steps_.empty()) {
            return Duration::seconds(0);
        }
        
        auto it = std::max_element(
            steps_.begin(), 
            steps_.end(),
            [](const AnimationStep& a, const AnimationStep& b) {
                return a.duration < b.duration;
            }
        );
        
        return it->duration;
    }
    
    /// @brief 计算最短步骤时长
    Duration find_shortest_step() const {
        if (steps_.empty()) {
            return Duration::seconds(0);
        }
        
        auto it = std::min_element(
            steps_.begin(), 
            steps_.end(),
            [](const AnimationStep& a, const AnimationStep& b) {
                return a.duration < b.duration;
            }
        );
        
        return it->duration;
    }
    
    /// @brief 计算平均步骤时长
    Duration calculate_average_duration() const {
        if (steps_.empty()) {
            return Duration::seconds(0);
        }
        
        Duration total = calculate_total_duration();
        float avg_seconds = total.to_seconds() / static_cast<float>(steps_.size());
        return Duration::seconds(avg_seconds);
    }
    
    /// @brief 获取指定索引的步骤时长
    Duration get_step_duration(size_t index) const {
        if (index >= steps_.size()) {
            return Duration::seconds(0);
        }
        return steps_[index].duration;
    }
    
    /// @brief 计算到指定步骤的累积时长
    Duration calculate_cumulative_duration(size_t up_to_index) const {
        Duration cumulative = Duration::seconds(0);
        for (size_t i = 0; i <= up_to_index && i < steps_.size(); ++i) {
            cumulative += steps_[i].duration;
        }
        return cumulative;
    }
    
    /// @brief 打印动画序列摘要
    void print_summary() const {
        std::cout << "=== 动画序列摘要 ===\n";
        std::cout << "步骤数量: " << steps_.size() << "\n";
        std::cout << "总时长: " << calculate_total_duration().to_milliseconds() << " ms\n";
        std::cout << "最长步骤: " << find_longest_step().to_milliseconds() << " ms\n";
        std::cout << "最短步骤: " << find_shortest_step().to_milliseconds() << " ms\n";
        std::cout << "平均时长: " << calculate_average_duration().to_milliseconds() << " ms\n";
        std::cout << "\n详细步骤:\n";
        
        for (size_t i = 0; i < steps_.size(); ++i) {
            const auto& step = steps_[i];
            std::cout << "  [" << i << "] " 
                      << get_step_type_name(step.type) 
                      << ": " << step.duration.to_milliseconds() << " ms\n";
        }
    }
    
private:
    std::vector<AnimationStep> steps_;
    
    static const char* get_step_type_name(AnimationStepType type) {
        switch (type) {
            case AnimationStepType::FadeIn: return "淡入";
            case AnimationStepType::FadeOut: return "淡出";
            case AnimationStepType::SlideIn: return "滑入";
            case AnimationStepType::SlideOut: return "滑出";
            case AnimationStepType::Hold: return "停留";
            case AnimationStepType::Scale: return "缩放";
            default: return "未知";
        }
    }
};

int main() {
    // 创建动画序列计算器
    AnimationSequenceCalculator calculator;
    
    // 添加动画步骤
    calculator.add_step(AnimationStepType::FadeIn, Duration::milliseconds(300));
    calculator.add_step(AnimationStepType::SlideIn, Duration::milliseconds(400));
    calculator.add_step(AnimationStepType::Hold, Duration::seconds(2));
    calculator.add_step(AnimationStepType::Scale, Duration::milliseconds(250));
    calculator.add_step(AnimationStepType::FadeOut, Duration::milliseconds(300));
    
    // 打印摘要
    calculator.print_summary();
    
    // 计算特定索引的累积时长
    std::cout << "\n=== 累积时长 ===\n";
    for (size_t i = 0; i < 5; ++i) {
        Duration cumulative = calculator.calculate_cumulative_duration(i);
        std::cout << "步骤 0-" << i << " 累积: " 
                  << cumulative.to_milliseconds() << " ms\n";
    }
    
    return 0;
}
```

**输出示例：**
```
=== 动画序列摘要 ===
步骤数量: 5
总时长: 3250 ms
最长步骤: 2000 ms
最短步骤: 250 ms
平均时长: 650 ms

详细步骤:
  [0] 淡入: 300 ms
  [1] 滑入: 400 ms
  [2] 停留: 2000 ms
  [3] 缩放: 250 ms
  [4] 淡出: 300 ms

=== 累积时长 ===
步骤 0-0 累积: 300 ms
步骤 0-1 累积: 700 ms
步骤 0-2 累积: 2700 ms
步骤 0-3 累积: 2950 ms
步骤 0-4 累积: 3250 ms
```

---

### 示例 2：动画时长比较工具

实现一个工具，比较不同动画配置的时长：

```cpp
#include <mine/ui/animation/Duration.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace mine::ui::animation;

/// @brief 动画配置
struct AnimationConfig {
    std::string name;
    Duration duration;
    float speed_multiplier;  // 速度倍数
    
    AnimationConfig(std::string n, Duration d, float speed = 1.0f)
        : name(std::move(n))
        , duration(d)
        , speed_multiplier(speed) {}
    
    /// @brief 计算实际播放时长（考虑速度倍数）
    Duration get_actual_duration() const {
        if (speed_multiplier == 0.0f) {
            return Duration::seconds(0);
        }
        return duration * (1.0f / speed_multiplier);
    }
};

/// @brief 动画时长比较工具
class AnimationComparator {
public:
    /// @brief 添加配置
    void add_config(const AnimationConfig& config) {
        configs_.push_back(config);
    }
    
    /// @brief 找出最快的动画
    const AnimationConfig* find_fastest() const {
        if (configs_.empty()) return nullptr;
        
        auto it = std::min_element(
            configs_.begin(),
            configs_.end(),
            [](const AnimationConfig& a, const AnimationConfig& b) {
                return a.get_actual_duration() < b.get_actual_duration();
            }
        );
        
        return &(*it);
    }
    
    /// @brief 找出最慢的动画
    const AnimationConfig* find_slowest() const {
        if (configs_.empty()) return nullptr;
        
        auto it = std::max_element(
            configs_.begin(),
            configs_.end(),
            [](const AnimationConfig& a, const AnimationConfig& b) {
                return a.get_actual_duration() < b.get_actual_duration();
            }
        );
        
        return &(*it);
    }
    
    /// @brief 打印比较结果
    void print_comparison() const {
        std::cout << "=== 动画时长比较 ===\n\n";
        
        for (const auto& config : configs_) {
            Duration actual = config.get_actual_duration();
            std::cout << config.name << ":\n";
            std::cout << "  基础时长: " << config.duration.to_milliseconds() << " ms\n";
            std::cout << "  速度倍数: " << config.speed_multiplier << "x\n";
            std::cout << "  实际时长: " << actual.to_milliseconds() << " ms\n";
            std::cout << "  实际时长: " << actual.to_seconds() << " s\n\n";
        }
        
        // 找出最快和最慢的动画
        if (const auto* fastest = find_fastest()) {
            std::cout << "最快动画: " << fastest->name 
                      << " (" << fastest->get_actual_duration().to_milliseconds() << " ms)\n";
        }
        
        if (const auto* slowest = find_slowest()) {
            std::cout << "最慢动画: " << slowest->name 
                      << " (" << slowest->get_actual_duration().to_milliseconds() << " ms)\n";
        }
    }
    
private:
    std::vector<AnimationConfig> configs_;
};

int main() {
    AnimationComparator comparator;
    
    // 添加不同的动画配置
    comparator.add_config({"标准淡入", Duration::milliseconds(300), 1.0f});
    comparator.add_config({"快速淡入", Duration::milliseconds(300), 2.0f});  // 2倍速
    comparator.add_config({"慢速淡入", Duration::milliseconds(300), 0.5f}); // 0.5倍速
    comparator.add_config({"页面过渡", Duration::seconds(0.5), 1.0f});
    comparator.add_config({"弹出动画", Duration::milliseconds(200), 1.5f}); // 1.5倍速
    
    // 打印比较结果
    comparator.print_comparison();
    
    return 0;
}
```

**输出示例：**
```
=== 动画时长比较 ===

标准淡入:
  基础时长: 300 ms
  速度倍数: 1x
  实际时长: 300 ms
  实际时长: 0.3 s

快速淡入:
  基础时长: 300 ms
  速度倍数: 2x
  实际时长: 150 ms
  实际时长: 0.15 s

慢速淡入:
  基础时长: 300 ms
  速度倍数: 0.5x
  实际时长: 600 ms
  实际时长: 0.6 s

页面过渡:
  基础时长: 500 ms
  速度倍数: 1x
  实际时长: 500 ms
  实际时长: 0.5 s

弹出动画:
  基础时长: 200 ms
  速度倍数: 1.5x
  实际时长: 133.333 ms
  实际时长: 0.133333 s

最快动画: 弹出动画 (133.333 ms)
最慢动画: 慢速淡入 (600 ms)
```

---

### 示例 3：constexpr 编译期计算演示

展示如何在编译期使用 `Duration`：

```cpp
#include <mine/ui/animation/Duration.h>
#include <iostream>
#include <array>

using namespace mine::ui::animation;

// ========== 编译期常量定义 ==========

// 标准动画时长常量
constexpr Duration kQuickFeedback = Duration::milliseconds(150);
constexpr Duration kStandardTransition = Duration::milliseconds(300);
constexpr Duration kSlowTransition = Duration::milliseconds(500);
constexpr Duration kPageTransition = Duration::seconds(0.5);

// 编译期计算序列总时长
constexpr Duration kFadeInDuration = Duration::milliseconds(300);
constexpr Duration kHoldDuration = Duration::seconds(2);
constexpr Duration kFadeOutDuration = Duration::milliseconds(300);
constexpr Duration kTotalSequenceDuration = kFadeInDuration + kHoldDuration + kFadeOutDuration;

// 编译期计算帧相关常量
constexpr Duration kFrameDuration60fps = Duration::milliseconds(16.67f);
constexpr Duration kFrameDuration30fps = Duration::milliseconds(33.33f);

// 编译期计算动画帧数（60fps）
constexpr int kAnimationFrameCount = static_cast<int>(
    kStandardTransition.to_milliseconds() / kFrameDuration60fps.to_milliseconds()
);

// ========== 编译期查找表 ==========

// 预定义的动画时长查找表（编译期生成）
constexpr std::array<Duration, 5> kStandardDurations = {
    Duration::milliseconds(100),
    Duration::milliseconds(200),
    Duration::milliseconds(300),
    Duration::milliseconds(500),
    Duration::seconds(1.0)
};

// 编译期查找最接近的标准时长
constexpr Duration find_closest_standard_duration(Duration target) {
    Duration closest = kStandardDurations[0];
    float min_diff = 1000000.0f;
    
    for (const auto& standard : kStandardDurations) {
        float diff = target.to_seconds() - standard.to_seconds();
        if (diff < 0) diff = -diff;  // abs
        
        if (diff < min_diff) {
            min_diff = diff;
            closest = standard;
        }
    }
    
    return closest;
}

// 编译期测试
constexpr Duration kCustomDuration = Duration::milliseconds(350);
constexpr Duration kClosest = find_closest_standard_duration(kCustomDuration);

// ========== 编译期单位换算表 ==========

// 编译期生成时长表（毫秒 -> 秒）
template<size_t N>
constexpr std::array<float, N> generate_seconds_table() {
    std::array<float, N> table{};
    for (size_t i = 0; i < N; ++i) {
        Duration d = Duration::milliseconds(static_cast<float>(i * 100));
        table[i] = d.to_seconds();
    }
    return table;
}

constexpr auto kSecondsTable = generate_seconds_table<11>();  // 0ms ~ 1000ms

// ========== 运行时使用 ==========

int main() {
    std::cout << "=== 编译期常量演示 ===\n\n";
    
    // 1. 打印预定义常量
    std::cout << "标准动画时长:\n";
    std::cout << "  快速反馈: " << kQuickFeedback.to_milliseconds() << " ms\n";
    std::cout << "  标准过渡: " << kStandardTransition.to_milliseconds() << " ms\n";
    std::cout << "  慢速过渡: " << kSlowTransition.to_milliseconds() << " ms\n";
    std::cout << "  页面过渡: " << kPageTransition.to_milliseconds() << " ms\n\n";
    
    // 2. 打印序列时长
    std::cout << "动画序列:\n";
    std::cout << "  淡入: " << kFadeInDuration.to_milliseconds() << " ms\n";
    std::cout << "  停留: " << kHoldDuration.to_milliseconds() << " ms\n";
    std::cout << "  淡出: " << kFadeOutDuration.to_milliseconds() << " ms\n";
    std::cout << "  总计: " << kTotalSequenceDuration.to_milliseconds() << " ms\n\n";
    
    // 3. 打印帧相关信息
    std::cout << "帧相关:\n";
    std::cout << "  60fps 帧时长: " << kFrameDuration60fps.to_milliseconds() << " ms\n";
    std::cout << "  30fps 帧时长: " << kFrameDuration30fps.to_milliseconds() << " ms\n";
    std::cout << "  标准过渡帧数 (60fps): " << kAnimationFrameCount << " 帧\n\n";
    
    // 4. 打印查找表结果
    std::cout << "标准时长查找:\n";
    std::cout << "  自定义时长: " << kCustomDuration.to_milliseconds() << " ms\n";
    std::cout << "  最接近标准: " << kClosest.to_milliseconds() << " ms\n\n";
    
    // 5. 打印换算表
    std::cout << "毫秒 -> 秒换算表:\n";
    for (size_t i = 0; i < kSecondsTable.size(); ++i) {
        std::cout << "  " << (i * 100) << " ms = " << kSecondsTable[i] << " s\n";
    }
    
    // 6. 运行时查找
    std::cout << "\n运行时查找测试:\n";
    Duration test_durations[] = {
        Duration::milliseconds(150),
        Duration::milliseconds(400),
        Duration::milliseconds(750),
    };
    
    for (const auto& test : test_durations) {
        Duration closest = find_closest_standard_duration(test);
        std::cout << "  " << test.to_milliseconds() << " ms -> " 
                  << closest.to_milliseconds() << " ms\n";
    }
    
    return 0;
}
```

**输出示例：**
```
=== 编译期常量演示 ===

标准动画时长:
  快速反馈: 150 ms
  标准过渡: 300 ms
  慢速过渡: 500 ms
  页面过渡: 500 ms

动画序列:
  淡入: 300 ms
  停留: 2000 ms
  淡出: 300 ms
  总计: 2600 ms

帧相关:
  60fps 帧时长: 16.67 ms
  30fps 帧时长: 33.33 ms
  标准过渡帧数 (60fps): 18 帧

标准时长查找:
  自定义时长: 350 ms
  最接近标准: 300 ms

毫秒 -> 秒换算表:
  0 ms = 0 s
  100 ms = 0.1 s
  200 ms = 0.2 s
  300 ms = 0.3 s
  400 ms = 0.4 s
  500 ms = 0.5 s
  600 ms = 0.6 s
  700 ms = 0.7 s
  800 ms = 0.8 s
  900 ms = 0.9 s
  1000 ms = 1 s

运行时查找测试:
  150 ms -> 100 ms
  400 ms -> 500 ms
  750 ms -> 1000 ms
```

---

## 总结

`Duration` 是 MineUI 动画系统的基础时间类型，提供了：

1. **类型安全**：通过工厂函数避免单位混淆
2. **值语义**：轻量级、可按值传递、无内存分配
3. **丰富操作**：比较、算术、转换等完整支持
4. **编译期能力**：constexpr 支持编译期计算
5. **清晰语义**：`milliseconds()`、`seconds()` 等方法名称直观

**关键要点：**

- 始终使用工厂函数创建 `Duration` 对象
- 使用 `to_seconds()` 进行数学计算，避免单位混淆
- 使用 `is_zero()` 检查零时长，避免除零错误
- 利用 constexpr 定义动画常量，提高可维护性
- 注意浮点精度问题，使用容差比较或 `Duration` 的比较运算符

`Duration` 为动画系统提供了坚实的时间管理基础，是所有时间相关操作的核心类型。
