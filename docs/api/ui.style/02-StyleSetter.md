# StyleSetter 详细接口文档

## 概述

`StyleSetter` 是 `mine.ui.style` 模块的**依赖属性设置器**。

**核心职责：**
- **属性设置**：描述"将某个依赖属性设置为指定值"的操作
- **双模式**：支持静态值和 DynamicResource（动态资源）
- **视觉状态**：配合 VisualStateSetters 实现状态驱动样式

**核心特性：**
- **静态值模式**：res_key 为空，直接设置 value
- **DynamicResource 模式**：res_key 非空，运行时从资源字典查找
- **轻量结构**：POD 类型，零开销

**使用场景：**
- 在 Style 中定义属性设置
- 在 VisualStateSetters 中定义状态样式
- 由 mmlc 编译器生成（MML → C++ 代码）

---

## 文件位置

```
src/mine/ui/style/api/include/mine/ui/style/StyleSetter.h
```

---

## 类型定义

### StyleSetter

```cpp
/**
 * @brief 单个依赖属性设置器。
 *
 * StyleSetter 描述"将某个依赖属性设置为指定值"的操作。
 *
 * 两种模式（互斥）：
 *   - 静态值（res_key 为空）：直接将 value 写入目标属性
 *   - DynamicResource（res_key 非空）：在运行时从资源字典查找 res_key 对应的值，
 *     并订阅其变更（由 mmlc 生成的 apply_fn_ 处理；程序化路径暂不实现）
 */
struct MINE_UI_STYLE_API StyleSetter {
    /// 目标依赖属性（非空；指向全局唯一注册实例）
    const ui::DependencyProperty* property{nullptr};
    /// 静态值（res_key 为空时生效）
    core::Variant                 value;
    /// DynamicResource 键（非空时优先于 value；典型长度 < 24 字节）
    containers::InlineString      res_key;
};
```

---

### VisualStateSetters

```cpp
/**
 * @brief 单个视觉状态下的属性设置器集合。
 *
 * 由 Style 在 apply_state() 时查找并应用到目标元素（StyleTrigger 优先级）。
 */
struct MINE_UI_STYLE_API VisualStateSetters {
    /// 视觉状态名（如 "Normal"、"Hovered"、"Pressed"、"Focused"、"Disabled"）
    containers::InlineString           state_name;
    /// 该状态下需要应用的属性 setter 列表（典型 2-8 个）
    containers::SmallVector<StyleSetter, 8> setters;
};
```

---

## 成员说明

### StyleSetter 成员

#### property

```cpp
const ui::DependencyProperty* property{nullptr};
```

**描述**：目标依赖属性（非空）。

**说明**：
- 指向全局唯一的依赖属性注册实例
- 如 `Button::BackgroundProperty`、`Button::ForegroundProperty`
- 不应为 nullptr（否则无法应用）

---

#### value

```cpp
core::Variant value;
```

**描述**：静态值（res_key 为空时生效）。

**说明**：
- 直接设置的属性值（如颜色、尺寸、字符串）
- 仅在 res_key 为空时使用
- 支持任意类型（通过 Variant 包装）

---

#### res_key

```cpp
containers::InlineString res_key;
```

**描述**：DynamicResource 键（非空时优先于 value）。

**说明**：
- 运行时从资源字典查找对应值
- 非空时优先于 value（忽略静态值）
- 典型长度 < 24 字节（InlineString 优化）

---

### VisualStateSetters 成员

#### state_name

```cpp
containers::InlineString state_name;
```

**描述**：视觉状态名。

**常见状态**：
- `"Normal"` - 正常状态
- `"Hovered"` - 鼠标悬停
- `"Pressed"` - 按下
- `"Focused"` - 获得焦点
- `"Disabled"` - 禁用

---

#### setters

```cpp
containers::SmallVector<StyleSetter, 8> setters;
```

**描述**：该状态下需要应用的属性 setter 列表。

**说明**：
- 典型包含 2-8 个 setter
- 按顺序应用到目标元素

---

## 使用场景

### 1. 静态值设置器

```cpp
// 设置按钮背景为红色
StyleSetter setter;
setter.property = Button::BackgroundProperty;
setter.value = core::Variant::from(Color{255, 0, 0, 255});  // 红色
setter.res_key = "";  // 静态值模式
```

---

### 2. DynamicResource 设置器

```cpp
// 从资源字典获取背景色
StyleSetter setter;
setter.property = Button::BackgroundProperty;
setter.res_key = "ButtonBackground";  // DynamicResource 模式
// value 被忽略
```

---

### 3. 多个 setter 组合

```cpp
// 正常状态样式
VisualStateSetters normal_state;
normal_state.state_name = "Normal";

// 背景色
StyleSetter bg_setter;
bg_setter.property = Button::BackgroundProperty;
bg_setter.value = core::Variant::from(Color{200, 200, 200, 255});
normal_state.setters.push_back(bg_setter);

// 前景色
StyleSetter fg_setter;
fg_setter.property = Button::ForegroundProperty;
fg_setter.value = core::Variant::from(Color{0, 0, 0, 255});
normal_state.setters.push_back(fg_setter);
```

---

### 4. 悬停状态样式

```cpp
// 悬停状态样式
VisualStateSetters hovered_state;
hovered_state.state_name = "Hovered";

// 背景色变亮
StyleSetter bg_setter;
bg_setter.property = Button::BackgroundProperty;
bg_setter.value = core::Variant::from(Color{220, 220, 220, 255});
hovered_state.setters.push_back(bg_setter);
```

---

### 5. 按下状态样式

```cpp
// 按下状态样式
VisualStateSetters pressed_state;
pressed_state.state_name = "Pressed";

// 背景色变暗
StyleSetter bg_setter;
bg_setter.property = Button::BackgroundProperty;
bg_setter.value = core::Variant::from(Color{180, 180, 180, 255});
pressed_state.setters.push_back(bg_setter);
```

---

### 6. 禁用状态样式

```cpp
// 禁用状态样式
VisualStateSetters disabled_state;
disabled_state.state_name = "Disabled";

// 背景色变灰
StyleSetter bg_setter;
bg_setter.property = Button::BackgroundProperty;
bg_setter.value = core::Variant::from(Color{150, 150, 150, 255});
disabled_state.setters.push_back(bg_setter);

// 前景色变灰
StyleSetter fg_setter;
fg_setter.property = Button::ForegroundProperty;
fg_setter.value = core::Variant::from(Color{100, 100, 100, 255});
disabled_state.setters.push_back(fg_setter);
```

---

### 7. MML 编译器生成

```mml
<Style TargetType="Button">
    <VisualState Name="Normal">
        <Setter Property="Background" Value="#C8C8C8" />
        <Setter Property="Foreground" Value="#000000" />
    </VisualState>
    
    <VisualState Name="Hovered">
        <Setter Property="Background" Value="#DCDCDC" />
    </VisualState>
    
    <VisualState Name="Pressed">
        <Setter Property="Background" Value="#B4B4B4" />
    </VisualState>
</Style>
```

**编译为：**
```cpp
// mmlc 生成代码
std::vector<VisualStateSetters> button_states;

// Normal 状态
{
    VisualStateSetters normal;
    normal.state_name = "Normal";
    
    StyleSetter bg;
    bg.property = Button::BackgroundProperty;
    bg.value = core::Variant::from(Color{200, 200, 200, 255});
    normal.setters.push_back(bg);
    
    StyleSetter fg;
    fg.property = Button::ForegroundProperty;
    fg.value = core::Variant::from(Color{0, 0, 0, 255});
    normal.setters.push_back(fg);
    
    button_states.push_back(std::move(normal));
}

// Hovered 状态
{
    VisualStateSetters hovered;
    hovered.state_name = "Hovered";
    
    StyleSetter bg;
    bg.property = Button::BackgroundProperty;
    bg.value = core::Variant::from(Color{220, 220, 220, 255});
    hovered.setters.push_back(bg);
    
    button_states.push_back(std::move(hovered));
}
```

---

## 最佳实践

### 1. 优先使用 DynamicResource

```cpp
// ✅ 推荐：使用 DynamicResource（支持主题切换）
StyleSetter setter;
setter.property = Button::BackgroundProperty;
setter.res_key = "ButtonBackground";

// ❌ 不推荐：硬编码颜色（无法主题化）
StyleSetter setter;
setter.property = Button::BackgroundProperty;
setter.value = core::Variant::from(Color{255, 0, 0, 255});
```

---

### 2. 检查 property 非空

```cpp
// ✅ 推荐：检查 property 非空
void apply_setter(const StyleSetter& setter, UIElement* element) {
    if (setter.property) {
        element->set_value(setter.property, setter.value);
    }
}

// ❌ 不推荐：不检查（可能崩溃）
void apply_setter(const StyleSetter& setter, UIElement* element) {
    element->set_value(setter.property, setter.value);  // ❌ property 可能为 nullptr
}
```

---

### 3. res_key 优先于 value

```cpp
// ✅ 推荐：检查 res_key 优先级
void apply_setter(const StyleSetter& setter, UIElement* element, ResourceDictionary* dict) {
    if (!setter.res_key.empty()) {
        // DynamicResource 模式
        core::Variant res_value = dict->get_resource(setter.res_key);
        element->set_value(setter.property, res_value);
    } else {
        // 静态值模式
        element->set_value(setter.property, setter.value);
    }
}

// ❌ 不推荐：忽略 res_key（优先级错误）
void apply_setter(const StyleSetter& setter, UIElement* element) {
    element->set_value(setter.property, setter.value);  // ❌ 忽略 res_key
}
```

---

### 4. 使用 SmallVector 减少分配

```cpp
// ✅ 推荐：使用 SmallVector（典型 2-8 个 setter）
VisualStateSetters state;
state.setters.reserve(4);  // ✅ 预留容量

// ❌ 不推荐：使用 std::vector（小规模分配开销大）
struct VisualStateSetters {
    std::string state_name;
    std::vector<StyleSetter> setters;  // ❌ 小规模场景分配开销大
};
```

---

## 常见陷阱

### 1. property 为 nullptr

```cpp
// ❌ 问题：property 为 nullptr
StyleSetter setter;
setter.property = nullptr;  // ❌ 无效
setter.value = core::Variant::from(Color{255, 0, 0, 255});

// 应用时崩溃
element->set_value(setter.property, setter.value);  // ❌ nullptr 解引用

// ✅ 解决：检查非空
if (setter.property) {
    element->set_value(setter.property, setter.value);
}
```

---

### 2. 同时设置 value 和 res_key

```cpp
// ⚠️ 注意：res_key 优先（value 被忽略）
StyleSetter setter;
setter.property = Button::BackgroundProperty;
setter.value = core::Variant::from(Color{255, 0, 0, 255});  // ⚠️ 被忽略
setter.res_key = "ButtonBackground";  // ✅ 优先使用

// ✅ 解决：仅设置一种模式
// 静态值模式
setter.value = core::Variant::from(Color{255, 0, 0, 255});
setter.res_key = "";

// DynamicResource 模式
setter.res_key = "ButtonBackground";
// value 不需要设置
```

---

### 3. res_key 不存在

```cpp
// ❌ 问题：res_key 在资源字典中不存在
StyleSetter setter;
setter.property = Button::BackgroundProperty;
setter.res_key = "NonExistentKey";  // ❌ 资源不存在

// 应用时失败
core::Variant value = dict->get_resource(setter.res_key);  // ⚠️ 返回空 Variant

// ✅ 解决：检查资源是否存在
if (dict->has_resource(setter.res_key)) {
    core::Variant value = dict->get_resource(setter.res_key);
    element->set_value(setter.property, value);
} else {
    std::cerr << "资源不存在: " << setter.res_key << std::endl;
}
```

---

### 4. 状态名拼写错误

```cpp
// ❌ 问题：状态名拼写错误
VisualStateSetters state;
state.state_name = "Hoverd";  // ❌ 应为 "Hovered"

// 查找状态时失败
auto it = std::find_if(states.begin(), states.end(), [](auto& s) {
    return s.state_name == "Hovered";  // ❌ 找不到
});

// ✅ 解决：使用常量定义状态名
namespace StateNames {
    inline constexpr const char* Normal = "Normal";
    inline constexpr const char* Hovered = "Hovered";
    inline constexpr const char* Pressed = "Pressed";
    inline constexpr const char* Focused = "Focused";
    inline constexpr const char* Disabled = "Disabled";
}

state.state_name = StateNames::Hovered;  // ✅ 编译时检查
```

---

## 完整示例

```cpp
#include <mine/ui/style/StyleSetter.h>
#include <mine/ui/style/ResourceDictionary.h>
#include <mine/ui/controls/Button.h>
#include <iostream>

using namespace mine::ui;
using namespace mine::ui::style;

// ────────────────────────────────────────────────────────────────────────────
// 状态名常量
// ────────────────────────────────────────────────────────────────────────────

namespace StateNames {
    inline constexpr const char* Normal   = "Normal";
    inline constexpr const char* Hovered  = "Hovered";
    inline constexpr const char* Pressed  = "Pressed";
    inline constexpr const char* Focused  = "Focused";
    inline constexpr const char* Disabled = "Disabled";
}

// ────────────────────────────────────────────────────────────────────────────
// 样式应用器
// ────────────────────────────────────────────────────────────────────────────

class StyleApplier {
public:
    static void apply_setter(const StyleSetter& setter, UIElement* element, ResourceDictionary* dict) {
        if (!setter.property) {
            std::cerr << "错误: setter.property 为空" << std::endl;
            return;
        }
        
        // DynamicResource 模式优先
        if (!setter.res_key.empty()) {
            if (dict->has_resource(setter.res_key)) {
                core::Variant value = dict->get_resource(setter.res_key);
                element->set_value(setter.property, value);
                std::cout << "应用 DynamicResource: " << setter.res_key << std::endl;
            } else {
                std::cerr << "警告: 资源不存在: " << setter.res_key << std::endl;
            }
        } else {
            // 静态值模式
            element->set_value(setter.property, setter.value);
            std::cout << "应用静态值" << std::endl;
        }
    }
    
    static void apply_state(const VisualStateSetters& state, UIElement* element, ResourceDictionary* dict) {
        std::cout << "应用状态: " << state.state_name << std::endl;
        for (const auto& setter : state.setters) {
            apply_setter(setter, element, dict);
        }
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 按钮样式构建器
// ────────────────────────────────────────────────────────────────────────────

class ButtonStyleBuilder {
public:
    static std::vector<VisualStateSetters> build() {
        std::vector<VisualStateSetters> states;
        
        // ════════════════════════════════════════════════════════════════════
        // Normal 状态
        // ════════════════════════════════════════════════════════════════════
        {
            VisualStateSetters normal;
            normal.state_name = StateNames::Normal;
            
            // 背景色
            StyleSetter bg;
            bg.property = Button::BackgroundProperty;
            bg.value = core::Variant::from(Color{200, 200, 200, 255});
            normal.setters.push_back(bg);
            
            // 前景色
            StyleSetter fg;
            fg.property = Button::ForegroundProperty;
            fg.value = core::Variant::from(Color{0, 0, 0, 255});
            normal.setters.push_back(fg);
            
            states.push_back(std::move(normal));
        }
        
        // ════════════════════════════════════════════════════════════════════
        // Hovered 状态
        // ════════════════════════════════════════════════════════════════════
        {
            VisualStateSetters hovered;
            hovered.state_name = StateNames::Hovered;
            
            // 背景色变亮
            StyleSetter bg;
            bg.property = Button::BackgroundProperty;
            bg.value = core::Variant::from(Color{220, 220, 220, 255});
            hovered.setters.push_back(bg);
            
            states.push_back(std::move(hovered));
        }
        
        // ════════════════════════════════════════════════════════════════════
        // Pressed 状态
        // ════════════════════════════════════════════════════════════════════
        {
            VisualStateSetters pressed;
            pressed.state_name = StateNames::Pressed;
            
            // 背景色变暗
            StyleSetter bg;
            bg.property = Button::BackgroundProperty;
            bg.value = core::Variant::from(Color{180, 180, 180, 255});
            pressed.setters.push_back(bg);
            
            states.push_back(std::move(pressed));
        }
        
        // ════════════════════════════════════════════════════════════════════
        // Disabled 状态
        // ════════════════════════════════════════════════════════════════════
        {
            VisualStateSetters disabled;
            disabled.state_name = StateNames::Disabled;
            
            // 背景色变灰
            StyleSetter bg;
            bg.property = Button::BackgroundProperty;
            bg.value = core::Variant::from(Color{150, 150, 150, 255});
            disabled.setters.push_back(bg);
            
            // 前景色变灰
            StyleSetter fg;
            fg.property = Button::ForegroundProperty;
            fg.value = core::Variant::from(Color{100, 100, 100, 255});
            disabled.setters.push_back(fg);
            
            states.push_back(std::move(disabled));
        }
        
        return states;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 主函数
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // 创建按钮和资源字典
    Button* button = new Button();
    button->set_content("确定");
    
    ResourceDictionary* dict = new ResourceDictionary();
    dict->set_resource("ButtonBackground", Color{255, 0, 0, 255});  // 红色
    
    // 构建按钮样式
    auto states = ButtonStyleBuilder::build();
    
    std::cout << "按钮样式状态数: " << states.size() << std::endl;  // 4
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 应用 Normal 状态
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 应用 Normal 状态 ===" << std::endl;
    auto normal_it = std::find_if(states.begin(), states.end(), [](auto& s) {
        return s.state_name == StateNames::Normal;
    });
    
    if (normal_it != states.end()) {
        StyleApplier::apply_state(*normal_it, button, dict);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 应用 Hovered 状态
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 应用 Hovered 状态 ===" << std::endl;
    auto hovered_it = std::find_if(states.begin(), states.end(), [](auto& s) {
        return s.state_name == StateNames::Hovered;
    });
    
    if (hovered_it != states.end()) {
        StyleApplier::apply_state(*hovered_it, button, dict);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 应用 Disabled 状态
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 应用 Disabled 状态 ===" << std::endl;
    auto disabled_it = std::find_if(states.begin(), states.end(), [](auto& s) {
        return s.state_name == StateNames::Disabled;
    });
    
    if (disabled_it != states.end()) {
        StyleApplier::apply_state(*disabled_it, button, dict);
    }
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. DynamicResource 示例
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== DynamicResource 示例 ===" << std::endl;
    
    StyleSetter dynamic_setter;
    dynamic_setter.property = Button::BackgroundProperty;
    dynamic_setter.res_key = "ButtonBackground";  // DynamicResource
    
    StyleApplier::apply_setter(dynamic_setter, button, dict);
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 统计信息
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 统计信息 ===" << std::endl;
    for (const auto& state : states) {
        std::cout << "状态: " << state.state_name 
                  << ", Setter 数量: " << state.setters.size() << std::endl;
    }
    
    // 清理
    delete button;
    delete dict;
    
    return 0;
}
```

**预期输出：**
```
按钮样式状态数: 4

=== 应用 Normal 状态 ===
应用状态: Normal
应用静态值
应用静态值

=== 应用 Hovered 状态 ===
应用状态: Hovered
应用静态值

=== 应用 Disabled 状态 ===
应用状态: Disabled
应用静态值
应用静态值

=== DynamicResource 示例 ===
应用 DynamicResource: ButtonBackground

=== 统计信息 ===
状态: Normal, Setter 数量: 2
状态: Hovered, Setter 数量: 1
状态: Pressed, Setter 数量: 1
状态: Disabled, Setter 数量: 2
```

---

## 总结

`StyleSetter` 是 `mine.ui.style` 模块的依赖属性设置器，具备：

1. **双模式**：静态值（value）和 DynamicResource（res_key）
2. **轻量结构**：POD 类型，零开销
3. **视觉状态**：配合 VisualStateSetters 实现状态驱动样式
4. **MML 支持**：由 mmlc 编译器生成

**使用建议**：
- **优先使用 DynamicResource**（支持主题切换）
- **检查 property 非空**（避免崩溃）
- **res_key 优先于 value**（优先级正确）
- **使用 SmallVector 减少分配**（典型 2-8 个 setter）
- 避免 property 为 nullptr
- 避免同时设置 value 和 res_key
- 避免 res_key 不存在
- 避免状态名拼写错误（使用常量定义）
