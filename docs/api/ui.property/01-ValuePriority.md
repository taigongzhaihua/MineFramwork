# ValuePriority 详细接口文档

## 概述

`ValuePriority` 是 `mine.ui.property` 模块的**依赖属性值优先级枚举**。

**核心特性：**
- **优先级链**：定义依赖属性值的来源优先级（Animation > Local > TemplateBind > StyleTrigger > StyleSetter > Inherited > Default）
- **数值越大优先级越高**：Animation(60) 最高，Default(0) 最低
- **决定生效值**：DependencyObject::get_value() 返回最高优先级的有效值

---

## 文件位置

```
src/mine/ui/property/api/include/mine/ui/property/ValuePriority.h
```

---

## 枚举定义

```cpp
enum class ValuePriority : uint8_t {
    /// 属性默认值（PropertyMetadata.default_value），最低优先级，不存入槽
    Default      = 0,
    /// 值继承：来自可视化树祖先（inherits = true 属性）
    Inherited    = 10,
    /// 样式 setter（Style::setters 直接赋值）
    StyleSetter  = 20,
    /// 样式触发器 / 视觉状态 setter（当前视觉状态激活的 setter）
    StyleTrigger = 30,
    /// 控件模板绑定（TemplateBinding）
    TemplateBind = 40,
    /// 本地值：代码直接调用 set_value() 或 MML 属性赋值
    Local        = 50,
    /// 动画值：动画系统写入，最高优先级
    Animation    = 60,
};
```

**描述**：依赖属性值来源优先级（数值越大优先级越高）。

**优先级链（高 → 低）**：
```
Animation(60) > Local(50) > TemplateBind(40) > StyleTrigger(30) > StyleSetter(20) > Inherited(10) > Default(0)
```

---

## 枚举值说明

### Default (0)

**描述**：属性默认值（PropertyMetadata.default_value），最低优先级，不存入槽。

**来源**：DependencyProperty 注册时指定的 default_value。

**特征**：
- 最低优先级
- 不存入值槽（get_value() 找不到有效槽时返回默认值）

---

### Inherited (10)

**描述**：值继承：来自可视化树祖先（inherits = true 属性）。

**来源**：沿视觉树向下传播的 inherits=true 属性（如 DataContext、FontSize）。

**特征**：
- 子元素从父元素继承值
- 仅适用于 PropertyMetadata.inherits=true 的属性

---

### StyleSetter (20)

**描述**：样式 setter（Style::setters 直接赋值）。

**来源**：Style::apply() 写入的基线值。

**特征**：
- 样式基线值
- 低于 Local（用户可覆盖）

---

### StyleTrigger (30)

**描述**：样式触发器 / 视觉状态 setter（当前视觉状态激活的 setter）。

**来源**：Style::apply_state() 写入的状态值。

**特征**：
- 当前视觉状态激活的 setter（如 Hovered/Pressed）
- 高于 StyleSetter（状态值覆盖基线值）
- 低于 Local（用户可覆盖）

---

### TemplateBind (40)

**描述**：控件模板绑定（TemplateBinding）。

**来源**：TemplateBinding 从宿主控件属性同步。

**特征**：
- 模板内元素绑定到宿主控件属性
- 高于 StyleTrigger
- 低于 Local

---

### Local (50)

**描述**：本地值：代码直接调用 set_value() 或 MML 属性赋值。

**来源**：用户代码 set_xxx() / MML 内联属性。

**特征**：
- 用户设置的值
- 高于所有样式和绑定值
- 低于 Animation（动画可覆盖）

---

### Animation (60)

**描述**：动画值：动画系统写入，最高优先级。

**来源**：Storyboard 正在插值的运行时值。

**特征**：
- 最高优先级
- 动画结束后清除（回落到下一层）
- 可覆盖 Local 及以下所有优先级

---

## 使用场景

### 1. 设置本地值（Local）

```cpp
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/ValuePriority.h>

// 设置本地值（默认优先级）
obj.set_value(WidthProp, Variant{100});
// 等价于
obj.set_value(WidthProp, Variant{100}, ValuePriority::Local);
```

---

### 2. 样式系统设置值（StyleSetter）

```cpp
// Style::apply() 内部实现
obj.set_value(BackgroundProp, Variant{color}, ValuePriority::StyleSetter);
```

---

### 3. 视觉状态设置值（StyleTrigger）

```cpp
// VisualStateManager::apply_state() 内部实现
obj.set_value(BackgroundProp, Variant{hover_color}, ValuePriority::StyleTrigger);
```

---

### 4. 动画系统设置值（Animation）

```cpp
// Storyboard::tick() 内部实现
obj.set_value(OpacityProp, Variant{interpolated_value}, ValuePriority::Animation);
```

---

### 5. 优先级覆盖示例

```cpp
// 先设置样式值（StyleSetter = 20）
obj.set_value(WidthProp, Variant{10}, ValuePriority::StyleSetter);
CHECK(obj.get_value(WidthProp).get<int>() == 10);

// 再设置本地值（Local = 50）-> 生效值变为 20
obj.set_value(WidthProp, Variant{20}, ValuePriority::Local);
CHECK(obj.get_value(WidthProp).get<int>() == 20);  // Local 覆盖 StyleSetter

// 设置动画值（Animation = 60）-> 生效值变为 30
obj.set_value(WidthProp, Variant{30}, ValuePriority::Animation);
CHECK(obj.get_value(WidthProp).get<int>() == 30);  // Animation 覆盖 Local
```

---

### 6. 清除指定优先级的值

```cpp
// 设置多个优先级的值
obj.set_value(WidthProp, Variant{10}, ValuePriority::StyleSetter);
obj.set_value(WidthProp, Variant{20}, ValuePriority::Local);

// 清除 Local 优先级 -> 退回到 StyleSetter
obj.clear_value(WidthProp, ValuePriority::Local);
CHECK(obj.get_value(WidthProp).get<int>() == 10);  // 退回到 StyleSetter
```

---

### 7. 检查指定优先级是否有值

```cpp
// 设置本地值
obj.set_value(WidthProp, Variant{100}, ValuePriority::Local);

// 检查是否存在本地值
bool has_local = obj.has_value(WidthProp, ValuePriority::Local);
CHECK(has_local == true);

// 检查是否存在动画值
bool has_animation = obj.has_value(WidthProp, ValuePriority::Animation);
CHECK(has_animation == false);
```

---

### 8. 读取指定优先级或更低的值

```cpp
// 设置多个优先级的值
obj.set_value(WidthProp, Variant{10}, ValuePriority::StyleSetter);
obj.set_value(WidthProp, Variant{20}, ValuePriority::Local);
obj.set_value(WidthProp, Variant{30}, ValuePriority::Animation);

// 读取所有优先级中最高的值
CHECK(obj.get_value(WidthProp).get<int>() == 30);  // Animation

// 读取 Local 或更低优先级的值（忽略 Animation）
const Variant& val = obj.get_value(WidthProp, ValuePriority::Local);
CHECK(val.get<int>() == 20);  // Local（忽略 Animation）
```

---

## 优先级链详解

### 完整优先级链

| 优先级 | 符号 | 数值 | 来源 | 说明 |
|--------|------|------|------|------|
| 最高 | `Animation` | 60 | Storyboard 插值 | 动画运行时值，动画结束后清除 |
| ↓ | `Local` | 50 | 用户代码 set_xxx() | 用户设置的值 |
| ↓ | `TemplateBind` | 40 | TemplateBinding | 模板内元素绑定到宿主属性 |
| ↓ | `StyleTrigger` | 30 | Style::apply_state() | 当前视觉状态的 setter |
| ↓ | `StyleSetter` | 20 | Style::apply() | 样式基线值 |
| ↓ | `Inherited` | 10 | 可视化树祖先 | inherits=true 属性的继承值 |
| 最低 | `Default` | 0 | PropertyMetadata | 属性默认值 |

---

### 优先级覆盖规则

1. **高优先级覆盖低优先级**：设置高优先级的值会覆盖低优先级的值
2. **低优先级不覆盖高优先级**：设置低优先级的值不会影响当前生效值（如果存在更高优先级的值）
3. **清除高优先级后退回到下一级**：清除某个优先级的值后，get_value() 返回次高优先级的值

---

### 典型使用场景

| 场景 | 优先级 | 说明 |
|------|--------|------|
| 用户设置控件属性 | Local(50) | 用户代码调用 set_xxx() |
| 样式应用 | StyleSetter(20) | Style::apply() 设置基线外观 |
| 状态切换（无动画） | StyleTrigger(30) | 状态切换时设置外观 |
| 状态切换（有动画） | Animation(60) | 动画插值，结束后清除 |
| 模板绑定 | TemplateBind(40) | 模板内元素同步宿主属性 |
| 值继承 | Inherited(10) | 字体、DataContext 等沿树传播 |

---

## 最佳实践

### 1. 用户代码使用 Local 优先级

```cpp
// ✅ 推荐：用户代码设置本地值（默认优先级）
obj.set_value(WidthProp, Variant{100});

// ❌ 不推荐：用户代码显式指定 Local 优先级（冗余）
obj.set_value(WidthProp, Variant{100}, ValuePriority::Local);
```

---

### 2. 样式系统使用 StyleSetter 优先级

```cpp
// ✅ 推荐：样式系统使用 StyleSetter 优先级
void Style::apply(DependencyObject& target) const {
    for (const auto& setter : setters_) {
        target.set_value(*setter.property, setter.value, ValuePriority::StyleSetter);
    }
}
```

---

### 3. 状态系统使用 StyleTrigger 优先级

```cpp
// ✅ 推荐：状态系统使用 StyleTrigger 优先级
void VisualStateManager::apply_state(DependencyObject& target, const VisualState& state) const {
    for (const auto& setter : state.setters) {
        target.set_value(*setter.property, setter.value, ValuePriority::StyleTrigger);
    }
}
```

---

### 4. 动画系统使用 Animation 优先级

```cpp
// ✅ 推荐：动画系统使用 Animation 优先级
void Storyboard::tick(float dt) {
    for (auto& anim : animations_) {
        Variant interpolated = lerp_variant(anim.from, anim.to, anim.elapsed / anim.duration.seconds());
        anim.target->set_value(*anim.prop, interpolated, ValuePriority::Animation);
    }
}
```

---

## 常见陷阱

### 1. 低优先级无法覆盖高优先级

```cpp
// ❌ 问题：先设置 Local，再设置 StyleSetter，无效
obj.set_value(WidthProp, Variant{100}, ValuePriority::Local);
obj.set_value(WidthProp, Variant{50}, ValuePriority::StyleSetter);
CHECK(obj.get_value(WidthProp).get<int>() == 100);  // 仍然是 Local 的值

// ✅ 解决：清除 Local 后再设置 StyleSetter
obj.clear_value(WidthProp, ValuePriority::Local);
obj.set_value(WidthProp, Variant{50}, ValuePriority::StyleSetter);
CHECK(obj.get_value(WidthProp).get<int>() == 50);
```

---

### 2. 动画结束后未清除 Animation 优先级

```cpp
// ❌ 问题：动画结束后未清除 Animation 优先级
void Storyboard::tick(float dt) {
    for (auto& anim : animations_) {
        if (anim.elapsed >= anim.duration.seconds()) {
            // 动画结束，但未清除 Animation 优先级
            continue;
        }
        // ...
    }
}

// ✅ 解决：动画结束后清除 Animation 优先级
void Storyboard::stop() {
    for (auto& anim : animations_) {
        anim.target->clear_value(*anim.prop, ValuePriority::Animation);
    }
}
```

---

### 3. 混淆优先级数值和优先级枚举

```cpp
// ❌ 问题：使用数值而非枚举
obj.set_value(WidthProp, Variant{100}, 50);  // 编译错误

// ✅ 解决：使用枚举
obj.set_value(WidthProp, Variant{100}, ValuePriority::Local);
```

---

## 完整示例

```cpp
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/ValuePriority.h>
#include <mine/core/Variant.h>

using namespace mine::ui;
using namespace mine::core;

// 定义测试对象
class TestObject : public DependencyObject {};

// 注册属性
static const DependencyProperty& WidthProp = 
    DependencyProperty::register_property<int>(
        "Width", 
        type_id<TestObject>(), 
        PropertyMetadata{Variant{0}});

int main() {
    TestObject obj;
    
    // 场景1：设置默认值
    CHECK(obj.get_value(WidthProp).get<int>() == 0);  // Default
    
    // 场景2：设置样式值
    obj.set_value(WidthProp, Variant{10}, ValuePriority::StyleSetter);
    CHECK(obj.get_value(WidthProp).get<int>() == 10);  // StyleSetter
    
    // 场景3：设置本地值（覆盖样式值）
    obj.set_value(WidthProp, Variant{20});  // 默认 Local 优先级
    CHECK(obj.get_value(WidthProp).get<int>() == 20);  // Local
    
    // 场景4：设置动画值（覆盖本地值）
    obj.set_value(WidthProp, Variant{30}, ValuePriority::Animation);
    CHECK(obj.get_value(WidthProp).get<int>() == 30);  // Animation
    
    // 场景5：清除动画值（退回到本地值）
    obj.clear_value(WidthProp, ValuePriority::Animation);
    CHECK(obj.get_value(WidthProp).get<int>() == 20);  // Local
    
    // 场景6：清除本地值（退回到样式值）
    obj.clear_value(WidthProp, ValuePriority::Local);
    CHECK(obj.get_value(WidthProp).get<int>() == 10);  // StyleSetter
    
    // 场景7：清除样式值（退回到默认值）
    obj.clear_value(WidthProp, ValuePriority::StyleSetter);
    CHECK(obj.get_value(WidthProp).get<int>() == 0);  // Default
    
    return 0;
}
```

---

## 总结

`ValuePriority` 是 `mine.ui.property` 模块的依赖属性值优先级枚举，具备：

1. **优先级链**：Animation > Local > TemplateBind > StyleTrigger > StyleSetter > Inherited > Default
2. **数值越大优先级越高**：Animation(60) 最高，Default(0) 最低
3. **决定生效值**：DependencyObject::get_value() 返回最高优先级的有效值

**使用建议**：
- 用户代码使用 Local 优先级（默认）
- 样式系统使用 StyleSetter 优先级
- 状态系统使用 StyleTrigger 优先级
- 动画系统使用 Animation 优先级
- 高优先级覆盖低优先级
- 清除高优先级后退回到下一级
