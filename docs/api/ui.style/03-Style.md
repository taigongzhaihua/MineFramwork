# mine::ui::style::Style

## 概述

`Style` 是 MineUI 样式系统的核心类，代表一组针对特定控件类型的**属性 setter 集合**。它支持两种互补的工作模式：

1. **mmlc 生成路径**（`apply_fn_` 非空）：  
   当样式由 `.mml` 文件编译生成时，mmlc 会生成一个优化的 `apply_fn_` 函数，包含资源查找、DynamicResource 订阅等完整逻辑。调用 `apply()` 时直接执行 `apply_fn_`，无需遍历 `setters_`。

2. **程序化路径**（`apply_fn_` 为 `nullptr`）：  
   在运行时通过构建器接口（`add_setter`、`add_state_setters`）动态构建样式时，`apply()` 遍历 `setters_`，对每个静态 setter 以 `ValuePriority::StyleSetter (20)` 写入属性值。DynamicResource setter 在程序化路径下暂不处理。

---

### 核心特性

| 特性 | 说明 |
|------|------|
| **优先级链** | `Animation(60)` > `Local(50)` > `TemplateBind(40)` > `StyleTrigger(30)` > `StyleSetter(20)` > `Inherited(10)` > `Default(0)` |
| **BasedOn 继承** | 通过 `set_base()` 设置父样式，`apply()` 时先应用父样式，再应用本样式（子覆盖父） |
| **视觉状态支持** | `apply_state()` 以 `StyleTrigger(30)` 应用状态 setter，高于 `StyleSetter(20)` |
| **即时状态** | `apply_state_animation()` 以 `Animation(60)` 应用 Disabled 等即时状态，可覆盖 `Local(50)` |
| **状态清理** | `clear_all_state_values()` / `clear_state_values()` 清除 `StyleTrigger(30)` 槽，避免状态残留 |
| **双路径设计** | mmlc 路径高性能（预生成函数 + 资源订阅），程序化路径灵活（运行时动态构建） |

---

### 优先级系统详解

```
┌─────────────────────────────────────────────────────────────────┐
│ Animation(60)  ← apply_state_animation() / VSM 动画              │  最高
├─────────────────────────────────────────────────────────────────┤
│ Local(50)      ← 用户调用 set_background()、set_foreground()     │
├─────────────────────────────────────────────────────────────────┤
│ TemplateBind(40) ← 控件模板内的 {TemplateBinding Prop}         │
├─────────────────────────────────────────────────────────────────┤
│ StyleTrigger(30) ← apply_state() 应用视觉状态 setter           │
├─────────────────────────────────────────────────────────────────┤
│ StyleSetter(20)  ← apply() 应用基线样式 setter                  │
├─────────────────────────────────────────────────────────────────┤
│ Inherited(10)    ← 继承属性从父元素传递下来的值                 │
├─────────────────────────────────────────────────────────────────┤
│ Default(0)       ← 依赖属性注册时的默认值                       │  最低
└─────────────────────────────────────────────────────────────────┘
```

- **apply()** 写入 `StyleSetter(20)`，不会覆盖 `Local(50)` 及以上的值（用户设置始终优先）
- **apply_state()** 写入 `StyleTrigger(30)`，可覆盖 `StyleSetter(20)`（状态覆盖基线），但不会覆盖 `Local(50)`
- **apply_state_animation()** 写入 `Animation(60)`，可覆盖 `Local(50)`（Disabled 等即时状态强制生效）

---

### BasedOn 继承机制

```cpp
// 父样式
auto base_btn_style = Style()
    .set_name("BaseButton")
    .set_target_type(Button::type_id())
    .add_setter(StyleSetter{Control::background_property(), Colors::White})
    .add_setter(StyleSetter{Control::foreground_property(), Colors::Black});

// 子样式（覆盖父样式的 background）
auto primary_btn_style = Style()
    .set_name("PrimaryButton")
    .set_target_type(Button::type_id())
    .set_base(&base_btn_style)  // ← 继承父样式
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});  // 覆盖父

// 应用子样式时：
// 1. 先应用 base_btn_style（background=White, foreground=Black）
// 2. 再应用 primary_btn_style（background=Blue 覆盖 White；foreground 保持 Black）
primary_btn_style.apply(button);  // 结果：background=Blue, foreground=Black
```

---

## 文件位置

| 项 | 路径 |
|----|------|
| **头文件** | `src/mine/ui/style/api/include/mine/ui/style/Style.h` |
| **实现文件** | `src/mine/ui/style/src/Style.cpp` |
| **命名空间** | `mine::ui::style` |
| **依赖** | `mine.ui.property`（DependencyProperty）、`mine.core`（Variant）、`mine.containers`（SmallVector） |

---

## 类定义

```cpp
namespace mine::ui::style {

/**
 * @brief 样式对象：一组针对特定控件类型的属性 setter 集合。
 *
 * Style 支持两种工作模式（互斥选择）：
 *   1. **mmlc 生成路径**（apply_fn_ 非空）：调用 apply() 时直接执行 apply_fn_，
 *      apply_fn_ 由 mmlc 编译 `.mml` 文件时预生成，包含资源查找和订阅逻辑。
 *   2. **程序化路径**（apply_fn_ 为 nullptr）：调用 apply() 时遍历 setters_，
 *      对每个静态 setter 在 ValuePriority::StyleSetter (20) 写入属性值。
 *      DynamicResource setter（res_key 非空）在程序化路径下暂不处理。
 *
 * 优先级链（高 → 低）：
 *   Animation(60) > Local(50) > TemplateBind(40) > StyleTrigger(30)
 *   > StyleSetter(20) > Inherited(10) > Default(0)
 *
 * apply()：以 StyleSetter(20) 写入，不会覆盖 Local(50) 及以上的值。
 * apply_state()：以 StyleTrigger(30) 写入，可覆盖 StyleSetter(20) 的值，
 *               但不会覆盖 Local(50) 及以上的值。
 *
 * BasedOn 继承：
 *   apply() 先调用 base_->apply()，再应用本样式的 setter，
 *   因此本样式的 setter 覆盖父样式中同属性的 setter。
 *
 * 线程安全：不提供，调用方负责在 UI 线程使用。
 */
class MINE_UI_STYLE_API Style {
public:
    /// mmlc 生成的 apply 函数类型（接受 DependencyObject& 以解耦 mine.ui.visual）
    using ApplyFn = void (*)(ui::DependencyObject&);

    /// mmlc 生成的 apply 函数（nullptr = 使用程序化 setters_）
    ApplyFn apply_fn_{nullptr};

    // ── 生命周期 ──────────────────────────────────────────────────────────

    Style()                            = default;
    ~Style()                           = default;
    Style(const Style&)                = default;
    Style& operator=(const Style&)     = default;
    Style(Style&&) noexcept            = default;
    Style& operator=(Style&&) noexcept = default;

    // ── 查询 ──────────────────────────────────────────────────────────────

    /// 返回此样式适用的控件类型（注册时设置）
    [[nodiscard]] core::TypeId     target_type() const noexcept { return target_type_; }

    /// 返回父样式（BasedOn；可为 nullptr）
    [[nodiscard]] Style*           base()        const noexcept { return base_; }

    /// 返回样式名称（用于日志/调试，可为空）
    [[nodiscard]] core::StringView name()        const noexcept { return name_; }

    // ── 应用 ──────────────────────────────────────────────────────────────

    /**
     * @brief 将样式基线值应用到目标元素（StyleSetter 优先级）。
     *
     * 若有父样式（BasedOn），先应用父样式，再应用本样式（子样式覆盖同属性）。
     * 若 apply_fn_ 非空，则使用 mmlc 路径（调用 apply_fn_）；
     * 否则遍历 setters_，对静态 setter 以 ValuePriority::StyleSetter 写入。
     *
     * @param target 目标 DependencyObject（通常为 Control 实例）
     */
    void apply(ui::DependencyObject& target) const;

    /**
     * @brief 将指定视觉状态的 setter 应用到目标元素（StyleTrigger 优先级）。
     *
     * 查找 state_name 对应的 VisualStateSetters，依次以 ValuePriority::StyleTrigger
     * 写入各 setter 的静态值。若状态不存在，则为空操作。
     *
     * @param target     目标 DependencyObject
     * @param state_name 视觉状态名（如 "Hovered"、"Pressed"）
     */
    void apply_state(ui::DependencyObject& target, core::StringView state_name) const;

    /**
     * @brief 清除此样式所有状态 setter 曾写入的 StyleTrigger(30) 槽。
     *
     * 在 go_to_state 切换状态前调用，确保旧状态的 StyleTrigger 值不残留，
     * 使得过渡到无 setter 的状态（如 Normal）时属性能正确回退到 StyleSetter(20)。
     *
     * @param target 目标 DependencyObject
     */
    void clear_all_state_values(ui::DependencyObject& target) const;

    /**
     * @brief 仅清除指定视觉状态的 setter 所写入的 StyleTrigger(30) 槽。
     *
     * 与 clear_all_state_values 不同，此方法只遍历指定状态的 setter，
     * 不清除其他状态或其他样式写入的 StyleTrigger 值。
     * 用于多状态组（多 VSM）场景，避免一个 VSM 的状态切换误清另一组的值。
     *
     * @param target     目标 DependencyObject
     * @param state_name 要清除的视觉状态名
     */
    void clear_state_values(ui::DependencyObject& target, core::StringView state_name) const;

    /**
     * @brief 将指定视觉状态的 setter 以 Animation(P60) 优先级写入目标元素。
     *
     * 用于无动画的即时状态切换（如 Disabled）：Animation(P60) 高于 Local(P50)，
     * 可覆盖用户通过 set_xxx() 设置的本地值，确保禁用外观正确呈现。
     *
     * @param target     目标 DependencyObject
     * @param state_name 视觉状态名（如 "Disabled"）
     */
    void apply_state_animation(ui::DependencyObject& target, core::StringView state_name) const;

    /**
     * @brief 清除指定视觉状态曾以 Animation(P60) 写入的所有属性槽。
     *
     * 在 go_to_state 时清理上一个即时状态留下的 P60 值，
     * 确保属性恢复到 Local(P50) 或更低优先级的生效值。
     *
     * @param target     目标 DependencyObject
     * @param state_name 即时状态名（如 "Disabled"）
     */
    void clear_state_animation(ui::DependencyObject& target, core::StringView state_name) const;

    // ── 构建器接口（程序化构造；mmlc 路径不使用此接口）─────────────────────

    /// 设置样式名称
    Style& set_name(core::StringView name);

    /// 设置目标控件类型
    Style& set_target_type(core::TypeId type_id) noexcept;

    /// 设置父样式（BasedOn；传 nullptr 清除继承）
    Style& set_base(Style* base) noexcept;

    /// 追加一个基线属性 setter（静态值或 DynamicResource 键）
    Style& add_setter(StyleSetter setter);

    /// 追加一组视觉状态 setter
    Style& add_state_setters(VisualStateSetters state_setters);

private:
    /// 目标控件类型（用于 StyleRegistry 查找匹配样式）
    core::TypeId             target_type_{};
    /// 样式名称（InlineString，典型 < 24 字节）
    containers::InlineString name_;
    /// 父样式（BasedOn；弱引用，不拥有生命周期）
    Style*                   base_{nullptr};
    /// 基线属性 setter 列表（程序化路径）
    containers::SmallVector<StyleSetter, 16>       setters_;
    /// 视觉状态 setter 列表（程序化路径）
    containers::SmallVector<VisualStateSetters, 8> state_setters_;
};

}  // namespace mine::ui::style
```

---

## 成员方法详解

### 生命周期方法

| 方法 | 说明 |
|------|------|
| `Style()` | 默认构造函数，创建空样式（无 setter、无父样式、无名称） |
| `~Style()` | 默认析构函数（成员均为值类型或弱引用，无需手动清理） |
| `Style(const Style&)` | 拷贝构造（深拷贝 `setters_` 和 `state_setters_`，浅拷贝 `base_` 弱引用） |
| `Style& operator=(const Style&)` | 拷贝赋值 |
| `Style(Style&&) noexcept` | 移动构造（转移所有成员） |
| `Style& operator=(Style&&) noexcept` | 移动赋值 |

---

### 查询方法

#### `target_type()`

```cpp
[[nodiscard]] core::TypeId target_type() const noexcept;
```

**功能**：返回此样式适用的控件类型。

**参数**：无。

**返回值**：`core::TypeId`（控件类型 ID，如 `Button::type_id()`）。

**使用场景**：
- StyleRegistry 查找匹配样式时检查类型兼容性
- 运行时诊断样式应用失败（类型不匹配）
- 样式选择器（按类型过滤样式）

**示例**：

```cpp
auto button_style = Style()
    .set_target_type(Button::type_id())
    .set_name("MyButtonStyle");

// 检查类型
if (button_style.target_type() == Button::type_id()) {
    button_style.apply(my_button);
}
```

---

#### `base()`

```cpp
[[nodiscard]] Style* base() const noexcept;
```

**功能**：返回父样式（BasedOn）指针。

**参数**：无。

**返回值**：`Style*`（父样式指针，无父样式时返回 `nullptr`）。

**使用场景**：
- 遍历样式继承链（调试/诊断）
- 实现样式选择器的 BasedOn 查询
- 自动收集依赖样式（样式打包工具）

**示例**：

```cpp
// 打印样式继承链
void print_style_chain(const Style* style) {
    while (style) {
        fmt::print(" -> {}\n", style->name());
        style = style->base();
    }
}

auto base_style = Style().set_name("Base");
auto child_style = Style().set_name("Child").set_base(&base_style);
print_style_chain(&child_style);  // 输出：-> Child  -> Base
```

---

#### `name()`

```cpp
[[nodiscard]] core::StringView name() const noexcept;
```

**功能**：返回样式名称（可为空）。

**参数**：无。

**返回值**：`core::StringView`（样式名称视图）。

**使用场景**：
- 日志输出（记录当前应用的样式名称）
- 调试工具（VSM 状态跟踪面板显示样式名）
- 样式选择器（按名称查找样式）

**示例**：

```cpp
auto style = Style().set_name("PrimaryButton");
fmt::print("应用样式：{}\n", style.name());  // 输出：应用样式：PrimaryButton
```

---

### 应用方法

#### `apply()`

```cpp
void apply(ui::DependencyObject& target) const;
```

**功能**：将样式基线值应用到目标元素（`StyleSetter(20)` 优先级）。

**参数**：
- `target`：目标 `DependencyObject`（通常为 `Control` 实例）

**行为**：
1. 若有父样式（`base_`），先递归调用 `base_->apply(target)`
2. 若 `apply_fn_` 非空，调用 `apply_fn_(target)`（mmlc 路径）
3. 若 `apply_fn_` 为 `nullptr`，遍历 `setters_`：
   - 对于静态 setter（`res_key` 为空），调用 `target.set_value(setter.property, setter.value, ValuePriority::StyleSetter)`
   - 对于 DynamicResource setter（`res_key` 非空），暂不处理（程序化路径限制）

**优先级**：写入 `StyleSetter(20)`，不会覆盖 `Local(50)` 及以上的值。

**使用场景**：
- 控件实例化时应用默认样式
- 运行时动态切换样式（`control.set_style(new_style)`）
- 样式预览工具（实时查看样式效果）

**示例**：

```cpp
// 程序化路径：构建并应用样式
auto button_style = Style()
    .set_target_type(Button::type_id())
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue})
    .add_setter(StyleSetter{Control::foreground_property(), Colors::White});

button_style.apply(my_button);  // background=Blue, foreground=White（StyleSetter 优先级）

// 用户设置 Local 值不会被样式覆盖
my_button.set_background(Colors::Red);  // background=Red（Local 优先级高于 StyleSetter）
button_style.apply(my_button);          // background 仍为 Red（apply 不覆盖 Local）
```

---

#### `apply_state()`

```cpp
void apply_state(ui::DependencyObject& target, core::StringView state_name) const;
```

**功能**：将指定视觉状态的 setter 应用到目标元素（`StyleTrigger(30)` 优先级）。

**参数**：
- `target`：目标 `DependencyObject`
- `state_name`：视觉状态名（如 `"Hovered"`、`"Pressed"`）

**行为**：
1. 在 `state_setters_` 中查找 `state_name` 匹配的 `VisualStateSetters`
2. 若找到，遍历其 `setters`，调用 `target.set_value(setter.property, setter.value, ValuePriority::StyleTrigger)`
3. 若未找到，为空操作

**优先级**：写入 `StyleTrigger(30)`，可覆盖 `StyleSetter(20)`，但不会覆盖 `Local(50)` 及以上。

**使用场景**：
- VSM 触发视觉状态时应用状态 setter（如 `go_to_state("Hovered")`）
- 按钮鼠标悬停时改变背景色
- 文本框获得焦点时改变边框颜色

**示例**：

```cpp
auto button_style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Gray})  // 基线
    .add_state_setters(VisualStateSetters{
        .state_name = "Hovered",
        .setters = {
            StyleSetter{Control::background_property(), Colors::LightBlue}  // 悬停覆盖基线
        }
    });

button_style.apply(my_button);  // background=Gray（StyleSetter）

// 鼠标悬停时
button_style.apply_state(my_button, "Hovered");  // background=LightBlue（StyleTrigger 覆盖 StyleSetter）

// 鼠标离开时
button_style.clear_state_values(my_button, "Hovered");  // 清除 StyleTrigger 槽
// background 自动回退到 Gray（StyleSetter）
```

---

#### `clear_all_state_values()`

```cpp
void clear_all_state_values(ui::DependencyObject& target) const;
```

**功能**：清除此样式所有状态 setter 曾写入的 `StyleTrigger(30)` 槽。

**参数**：
- `target`：目标 `DependencyObject`

**行为**：
1. 遍历 `state_setters_` 中的所有状态
2. 对每个状态的每个 setter，调用 `target.clear_value(setter.property, ValuePriority::StyleTrigger)`

**使用场景**：
- VSM 切换状态前清理旧状态（确保无残留）
- 单 VSM 场景下的状态切换（Hovered → Normal → Pressed）
- 样式重置工具（清除所有状态应用痕迹）

**示例**：

```cpp
// 状态切换流程
void go_to_state(DependencyObject& target, const Style& style, StringView new_state) {
    // 1. 清除所有旧状态的 StyleTrigger(30) 槽
    style.clear_all_state_values(target);
    
    // 2. 应用新状态的 setter（若新状态为空，则回退到 StyleSetter(20)）
    if (!new_state.empty()) {
        style.apply_state(target, new_state);
    }
}

// 使用
go_to_state(my_button, button_style, "Hovered");   // 清除旧状态 + 应用 Hovered
go_to_state(my_button, button_style, "Normal");    // 清除 Hovered + 不应用新 setter（回退）
```

---

#### `clear_state_values()`

```cpp
void clear_state_values(ui::DependencyObject& target, core::StringView state_name) const;
```

**功能**：仅清除指定视觉状态的 setter 所写入的 `StyleTrigger(30)` 槽。

**参数**：
- `target`：目标 `DependencyObject`
- `state_name`：要清除的视觉状态名

**行为**：
1. 在 `state_setters_` 中查找 `state_name` 匹配的 `VisualStateSetters`
2. 若找到，遍历其 `setters`，调用 `target.clear_value(setter.property, ValuePriority::StyleTrigger)`

**使用场景**：
- 多 VSM 场景（一个控件有多个状态组，如 CommonStates + FocusStates）
- 精确控制状态清理（避免误清其他 VSM 的状态）
- 状态回退到基线（Hovered → Normal）

**示例**：

```cpp
// 多 VSM 场景：CommonStates（Normal/Hovered/Pressed）+ FocusStates（Unfocused/Focused）
auto button_style = Style()
    .add_state_setters(VisualStateSetters{
        .state_name = "Hovered",
        .setters = { StyleSetter{Control::background_property(), Colors::LightBlue} }
    })
    .add_state_setters(VisualStateSetters{
        .state_name = "Focused",
        .setters = { StyleSetter{Control::border_brush_property(), Colors::Orange} }
    });

// 鼠标悬停
button_style.apply_state(my_button, "Hovered");   // background=LightBlue
button_style.apply_state(my_button, "Focused");   // border=Orange（两个状态共存）

// 鼠标离开（只清除 Hovered，不影响 Focused）
button_style.clear_state_values(my_button, "Hovered");  // 清除 background 的 StyleTrigger 槽
// background 回退到 StyleSetter 基线，border 保持 Orange
```

---

#### `apply_state_animation()`

```cpp
void apply_state_animation(ui::DependencyObject& target, core::StringView state_name) const;
```

**功能**：将指定视觉状态的 setter 以 `Animation(60)` 优先级写入目标元素。

**参数**：
- `target`：目标 `DependencyObject`
- `state_name`：视觉状态名（如 `"Disabled"`）

**行为**：
1. 在 `state_setters_` 中查找 `state_name` 匹配的 `VisualStateSetters`
2. 若找到，遍历其 `setters`，调用 `target.set_value(setter.property, setter.value, ValuePriority::Animation)`

**优先级**：写入 `Animation(60)`，可覆盖 `Local(50)` 及以下的所有值。

**使用场景**：
- Disabled 等即时状态（无动画，需要强制覆盖用户设置）
- ReadOnly 状态（强制灰显文本框）
- 模态对话框背景遮罩（强制覆盖所有控件颜色）

**示例**：

```cpp
auto button_style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue})
    .add_state_setters(VisualStateSetters{
        .state_name = "Disabled",
        .setters = { StyleSetter{Control::background_property(), Colors::Gray} }
    });

button_style.apply(my_button);  // background=Blue

// 用户设置 Local 值
my_button.set_background(Colors::Red);  // background=Red（Local 优先级）

// 禁用按钮（使用 Animation 优先级强制覆盖 Local）
button_style.apply_state_animation(my_button, "Disabled");  // background=Gray（Animation 覆盖 Local）

// 启用按钮（清除 Animation 槽）
button_style.clear_state_animation(my_button, "Disabled");  // background 回退到 Red（Local）
```

---

#### `clear_state_animation()`

```cpp
void clear_state_animation(ui::DependencyObject& target, core::StringView state_name) const;
```

**功能**：清除指定视觉状态曾以 `Animation(60)` 写入的所有属性槽。

**参数**：
- `target`：目标 `DependencyObject`
- `state_name`：即时状态名（如 `"Disabled"`）

**行为**：
1. 在 `state_setters_` 中查找 `state_name` 匹配的 `VisualStateSetters`
2. 若找到，遍历其 `setters`，调用 `target.clear_value(setter.property, ValuePriority::Animation)`

**使用场景**：
- Disabled → Enabled 状态切换
- 清除即时状态的强制覆盖，恢复 Local 或更低优先级的值
- 状态机退出即时状态节点

**示例**：

```cpp
// 启用/禁用切换
void set_enabled(Button& button, const Style& style, bool enabled) {
    if (enabled) {
        style.clear_state_animation(button, "Disabled");  // 清除 Animation 槽
        // background 回退到 Local(50) 或 StyleSetter(20)
    } else {
        style.apply_state_animation(button, "Disabled");  // Animation 槽覆盖 Local
        // background 强制为灰色
    }
}
```

---

### 构建器接口

#### `set_name()`

```cpp
Style& set_name(core::StringView name);
```

**功能**：设置样式名称（用于日志/调试）。

**参数**：
- `name`：样式名称（如 `"PrimaryButton"`）

**返回值**：`Style&`（当前样式引用，支持链式调用）。

**使用场景**：
- 程序化构建样式时设置可读名称
- 样式注册到 StyleRegistry（按名称查找）
- 调试日志输出样式名称

**示例**：

```cpp
auto style = Style()
    .set_name("MyButtonStyle")  // ← 设置名称
    .set_target_type(Button::type_id())
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});

fmt::print("样式名称：{}\n", style.name());  // 输出：样式名称：MyButtonStyle
```

---

#### `set_target_type()`

```cpp
Style& set_target_type(core::TypeId type_id) noexcept;
```

**功能**：设置此样式适用的控件类型。

**参数**：
- `type_id`：控件类型 ID（如 `Button::type_id()`）

**返回值**：`Style&`（当前样式引用，支持链式调用）。

**使用场景**：
- 程序化构建样式时指定目标类型
- StyleRegistry 按类型匹配样式
- 类型安全检查（防止将 Button 样式应用到 TextBox）

**示例**：

```cpp
auto button_style = Style()
    .set_target_type(Button::type_id())  // ← 指定目标类型
    .add_setter(StyleSetter{Button::is_default_property(), true});

// 类型检查
if (button_style.target_type() == Button::type_id()) {
    button_style.apply(my_button);  // 类型匹配，应用成功
}
```

---

#### `set_base()`

```cpp
Style& set_base(Style* base) noexcept;
```

**功能**：设置父样式（BasedOn 继承）。

**参数**：
- `base`：父样式指针（传 `nullptr` 清除继承）

**返回值**：`Style&`（当前样式引用，支持链式调用）。

**使用场景**：
- 构建样式继承链（基础样式 → 变体样式）
- 复用公共属性 setter（减少重复代码）
- 主题系统（Dark 主题继承 Base 主题）

**示例**：

```cpp
// 基础样式
auto base_style = Style()
    .set_name("BaseButton")
    .add_setter(StyleSetter{Control::padding_property(), Thickness{10}})
    .add_setter(StyleSetter{Control::font_size_property(), 14.0});

// 主按钮样式（继承基础样式 + 覆盖颜色）
auto primary_style = Style()
    .set_name("PrimaryButton")
    .set_base(&base_style)  // ← 继承父样式
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue})
    .add_setter(StyleSetter{Control::foreground_property(), Colors::White});

// 应用主按钮样式时：
// 1. 先应用 base_style（padding=10, font_size=14）
// 2. 再应用 primary_style（background=Blue, foreground=White）
primary_style.apply(my_button);
```

---

#### `add_setter()`

```cpp
Style& add_setter(StyleSetter setter);
```

**功能**：追加一个基线属性 setter（静态值或 DynamicResource 键）。

**参数**：
- `setter`：`StyleSetter` 对象（包含 `property`、`value`、`res_key`）

**返回值**：`Style&`（当前样式引用，支持链式调用）。

**使用场景**：
- 程序化构建样式时添加属性 setter
- 运行时动态修改样式（追加新 setter）
- 样式编辑器（UI 工具添加/移除 setter）

**示例**：

```cpp
auto style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue})   // 静态值
    .add_setter(StyleSetter{Control::foreground_property(), Colors::White})  // 静态值
    .add_setter(StyleSetter{
        .property = Control::border_brush_property(),
        .value = {},
        .res_key = "PrimaryBorderBrush"  // DynamicResource（mmlc 路径处理）
    });
```

---

#### `add_state_setters()`

```cpp
Style& add_state_setters(VisualStateSetters state_setters);
```

**功能**：追加一组视觉状态 setter。

**参数**：
- `state_setters`：`VisualStateSetters` 对象（包含 `state_name` 和 `setters`）

**返回值**：`Style&`（当前样式引用，支持链式调用）。

**使用场景**：
- 程序化构建样式时添加视觉状态
- 运行时动态修改样式（追加新状态）
- VSM 编辑器（UI 工具添加/移除状态）

**示例**：

```cpp
auto style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Gray})  // 基线
    .add_state_setters(VisualStateSetters{
        .state_name = "Hovered",
        .setters = {
            StyleSetter{Control::background_property(), Colors::LightBlue},
            StyleSetter{Control::foreground_property(), Colors::Black}
        }
    })
    .add_state_setters(VisualStateSetters{
        .state_name = "Pressed",
        .setters = {
            StyleSetter{Control::background_property(), Colors::DarkBlue},
            StyleSetter{Control::foreground_property(), Colors::White}
        }
    });
```

---

## 使用场景

### 1. 程序化构建按钮样式

```cpp
// 构建主按钮样式（蓝色背景 + 白色文字）
auto primary_button_style = Style()
    .set_name("PrimaryButton")
    .set_target_type(Button::type_id())
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue})
    .add_setter(StyleSetter{Control::foreground_property(), Colors::White})
    .add_setter(StyleSetter{Control::padding_property(), Thickness{10, 5}})
    .add_setter(StyleSetter{Control::font_size_property(), 14.0});

// 应用到按钮
primary_button_style.apply(my_button);
```

---

### 2. BasedOn 继承构建样式变体

```cpp
// 基础按钮样式
auto base_button_style = Style()
    .set_name("BaseButton")
    .set_target_type(Button::type_id())
    .add_setter(StyleSetter{Control::padding_property(), Thickness{10}})
    .add_setter(StyleSetter{Control::font_family_property(), "Segoe UI"})
    .add_setter(StyleSetter{Control::font_size_property(), 14.0})
    .add_setter(StyleSetter{Control::cursor_property(), Cursors::Hand});

// 主按钮样式（继承基础样式 + 蓝色主题）
auto primary_button_style = Style()
    .set_name("PrimaryButton")
    .set_target_type(Button::type_id())
    .set_base(&base_button_style)  // ← 继承父样式
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue})
    .add_setter(StyleSetter{Control::foreground_property(), Colors::White});

// 次要按钮样式（继承基础样式 + 灰色主题）
auto secondary_button_style = Style()
    .set_name("SecondaryButton")
    .set_target_type(Button::type_id())
    .set_base(&base_button_style)  // ← 继承父样式
    .add_setter(StyleSetter{Control::background_property(), Colors::Gray})
    .add_setter(StyleSetter{Control::foreground_property(), Colors::Black});

// 应用主按钮样式时：
// 1. 先应用 base_button_style（padding、font、cursor）
// 2. 再应用 primary_button_style（background、foreground）
primary_button_style.apply(my_button);
```

---

### 3. 视觉状态切换（Normal/Hovered/Pressed）

```cpp
// 构建带视觉状态的按钮样式
auto button_style = Style()
    .set_name("InteractiveButton")
    .set_target_type(Button::type_id())
    // 基线（Normal 状态）
    .add_setter(StyleSetter{Control::background_property(), Colors::Gray})
    .add_setter(StyleSetter{Control::foreground_property(), Colors::Black})
    // Hovered 状态
    .add_state_setters(VisualStateSetters{
        .state_name = "Hovered",
        .setters = {
            StyleSetter{Control::background_property(), Colors::LightBlue},
            StyleSetter{Control::foreground_property(), Colors::Black}
        }
    })
    // Pressed 状态
    .add_state_setters(VisualStateSetters{
        .state_name = "Pressed",
        .setters = {
            StyleSetter{Control::background_property(), Colors::DarkBlue},
            StyleSetter{Control::foreground_property(), Colors::White}
        }
    });

// 初始化时应用基线
button_style.apply(my_button);  // background=Gray, foreground=Black

// 鼠标悬停时
button_style.clear_all_state_values(my_button);         // 清除旧状态
button_style.apply_state(my_button, "Hovered");         // background=LightBlue

// 鼠标按下时
button_style.clear_all_state_values(my_button);         // 清除 Hovered
button_style.apply_state(my_button, "Pressed");         // background=DarkBlue

// 鼠标释放（回到 Normal）
button_style.clear_all_state_values(my_button);         // 清除 Pressed
// background 自动回退到 Gray（StyleSetter 基线）
```

---

### 4. 多 VSM 场景（CommonStates + FocusStates）

```cpp
auto button_style = Style()
    .set_name("MultistateButton")
    .set_target_type(Button::type_id())
    .add_setter(StyleSetter{Control::background_property(), Colors::Gray})
    .add_setter(StyleSetter{Control::border_brush_property(), Colors::Transparent})
    // CommonStates
    .add_state_setters(VisualStateSetters{
        .state_name = "Hovered",
        .setters = { StyleSetter{Control::background_property(), Colors::LightBlue} }
    })
    // FocusStates
    .add_state_setters(VisualStateSetters{
        .state_name = "Focused",
        .setters = { StyleSetter{Control::border_brush_property(), Colors::Orange} }
    });

// 鼠标悬停
button_style.apply_state(my_button, "Hovered");   // background=LightBlue

// 获得键盘焦点（不清除 Hovered）
button_style.apply_state(my_button, "Focused");   // border=Orange（两个状态共存）

// 失去焦点（只清除 Focused，不影响 Hovered）
button_style.clear_state_values(my_button, "Focused");  // border 回退到 Transparent
// background 保持 LightBlue
```

---

### 5. 优先级演示（Local 优先于 StyleSetter）

```cpp
auto style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});

style.apply(my_button);  // background=Blue（StyleSetter 优先级）

// 用户设置 Local 值
my_button.set_background(Colors::Red);  // background=Red（Local 优先级高于 StyleSetter）

// 再次应用样式（不会覆盖 Local 值）
style.apply(my_button);  // background 仍为 Red

// 清除 Local 值后，样式生效
my_button.clear_value(Control::background_property(), ValuePriority::Local);
// background 回退到 Blue（StyleSetter 优先级）
```

---

### 6. 即时状态（Disabled）强制覆盖 Local

```cpp
auto button_style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue})
    .add_state_setters(VisualStateSetters{
        .state_name = "Disabled",
        .setters = { StyleSetter{Control::background_property(), Colors::Gray} }
    });

button_style.apply(my_button);  // background=Blue

// 用户设置 Local 值
my_button.set_background(Colors::Red);  // background=Red（Local 优先级）

// 禁用按钮（使用 Animation 优先级强制覆盖 Local）
button_style.apply_state_animation(my_button, "Disabled");  // background=Gray（覆盖 Red）

// 启用按钮（清除 Animation 槽）
button_style.clear_state_animation(my_button, "Disabled");  // background 回退到 Red（Local）
```

---

### 7. MML 生成路径（apply_fn_ 预生成）

```mml
// Button.mml
<Style TargetType="Button" x:Key="PrimaryButtonStyle">
  <Setter Property="Background" Value="{DynamicResource PrimaryBrush}"/>
  <Setter Property="Foreground" Value="White"/>
  <VisualState Name="Hovered">
    <Setter Property="Background" Value="{DynamicResource PrimaryHoverBrush}"/>
  </VisualState>
</Style>
```

```cpp
// mmlc 生成的 apply_fn_（伪代码）
void apply_primary_button_style(ui::DependencyObject& target) {
    auto& resources = Application::current().resources();
    
    // 订阅 DynamicResource "PrimaryBrush"
    auto bg_token = resources.subscribe("PrimaryBrush", [&target](const Variant& value) {
        target.set_value(Control::background_property(), value, ValuePriority::StyleSetter);
    });
    
    // 静态值
    target.set_value(Control::foreground_property(), Colors::White, ValuePriority::StyleSetter);
    
    // 存储订阅 token（用于清理）
    target.set_style_subscription_tokens({bg_token});
}

// 使用 mmlc 路径的样式
Style primary_button_style;
primary_button_style.apply_fn_ = apply_primary_button_style;
primary_button_style.apply(my_button);  // 调用 apply_fn_（包含资源订阅）
```

---

### 8. 样式注册与查找

```cpp
// 样式注册表
class StyleRegistry {
public:
    void register_style(StringView key, Style* style) {
        styles_[key] = style;
    }
    
    Style* find_style(StringView key) const {
        auto it = styles_.find(key);
        return it != styles_.end() ? it->second : nullptr;
    }

private:
    std::unordered_map<std::string, Style*> styles_;
};

// 使用
StyleRegistry registry;
auto primary_style = Style().set_name("PrimaryButton");
registry.register_style("PrimaryButton", &primary_style);

// 查找并应用
if (auto* style = registry.find_style("PrimaryButton")) {
    style->apply(my_button);
}
```

---

### 9. 样式编辑器（运行时动态修改）

```cpp
// 样式编辑器：允许运行时添加/移除 setter
class StyleEditor {
public:
    explicit StyleEditor(Style& style) : style_(style) {}
    
    // 添加 setter
    void add_property_setter(const DependencyProperty* prop, const Variant& value) {
        style_.add_setter(StyleSetter{prop, value});
        apply_to_preview();
    }
    
    // 添加状态 setter
    void add_state_setter(StringView state, const DependencyProperty* prop, const Variant& value) {
        // 查找或创建状态
        auto* state_setters = find_state_setters(state);
        if (!state_setters) {
            style_.add_state_setters(VisualStateSetters{.state_name = state});
            state_setters = find_state_setters(state);
        }
        state_setters->setters.push_back(StyleSetter{prop, value});
        apply_to_preview();
    }
    
    // 实时预览
    void apply_to_preview() {
        style_.apply(preview_button_);
    }

private:
    Style& style_;
    Button preview_button_;  // 预览控件
    
    VisualStateSetters* find_state_setters(StringView state) {
        // 实现略
        return nullptr;
    }
};
```

---

## 最佳实践

### ✅ 使用 BasedOn 复用公共属性

```cpp
// ✅ 好：基础样式 + 变体样式
auto base_style = Style()
    .set_name("BaseButton")
    .add_setter(StyleSetter{Control::padding_property(), Thickness{10}})
    .add_setter(StyleSetter{Control::font_size_property(), 14.0});

auto primary_style = Style()
    .set_name("PrimaryButton")
    .set_base(&base_style)  // ← 继承公共属性
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});

auto secondary_style = Style()
    .set_name("SecondaryButton")
    .set_base(&base_style)  // ← 继承公共属性
    .add_setter(StyleSetter{Control::background_property(), Colors::Gray});
```

```cpp
// ❌ 坏：重复定义公共属性
auto primary_style = Style()
    .add_setter(StyleSetter{Control::padding_property(), Thickness{10}})  // 重复
    .add_setter(StyleSetter{Control::font_size_property(), 14.0})         // 重复
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});

auto secondary_style = Style()
    .add_setter(StyleSetter{Control::padding_property(), Thickness{10}})  // 重复
    .add_setter(StyleSetter{Control::font_size_property(), 14.0})         // 重复
    .add_setter(StyleSetter{Control::background_property(), Colors::Gray});
```

---

### ✅ 状态切换前先清除旧状态

```cpp
// ✅ 好：清除旧状态 + 应用新状态
void go_to_state(DependencyObject& target, const Style& style, StringView new_state) {
    style.clear_all_state_values(target);  // ← 先清除旧状态
    if (!new_state.empty()) {
        style.apply_state(target, new_state);
    }
}

// 使用
go_to_state(my_button, button_style, "Hovered");
go_to_state(my_button, button_style, "Normal");  // 清除 Hovered + 回退到基线
```

```cpp
// ❌ 坏：不清除旧状态，导致残留
void go_to_state_bad(DependencyObject& target, const Style& style, StringView new_state) {
    style.apply_state(target, new_state);  // ❌ 未清除旧状态
}

// 问题：Hovered → Normal 时，Hovered 的 setter 仍然生效（残留）
go_to_state_bad(my_button, button_style, "Hovered");  // background=LightBlue
go_to_state_bad(my_button, button_style, "Normal");   // background 仍为 LightBlue（残留！）
```

---

### ✅ 多 VSM 场景使用 clear_state_values（精确清理）

```cpp
// ✅ 好：多 VSM 场景使用 clear_state_values（只清除当前组）
void go_to_common_state(DependencyObject& target, const Style& style, StringView state) {
    // 只清除 CommonStates 组的旧状态（Hovered、Pressed）
    style.clear_state_values(target, "Hovered");
    style.clear_state_values(target, "Pressed");
    
    if (!state.empty()) {
        style.apply_state(target, state);
    }
}

void go_to_focus_state(DependencyObject& target, const Style& style, StringView state) {
    // 只清除 FocusStates 组的旧状态（Focused、Unfocused）
    style.clear_state_values(target, "Focused");
    style.clear_state_values(target, "Unfocused");
    
    if (!state.empty()) {
        style.apply_state(target, state);
    }
}

// 使用
go_to_common_state(my_button, button_style, "Hovered");  // 只清除 CommonStates
go_to_focus_state(my_button, button_style, "Focused");   // 只清除 FocusStates（不影响 Hovered）
```

```cpp
// ❌ 坏：多 VSM 场景使用 clear_all_state_values（误清其他组）
void go_to_common_state_bad(DependencyObject& target, const Style& style, StringView state) {
    style.clear_all_state_values(target);  // ❌ 清除所有状态（包括 FocusStates）
    style.apply_state(target, state);
}

// 问题：切换 CommonStates 时误清 FocusStates 的值
go_to_common_state_bad(my_button, button_style, "Hovered");  // 误清 Focused 状态
```

---

### ✅ 即时状态使用 apply_state_animation（强制覆盖 Local）

```cpp
// ✅ 好：Disabled 状态使用 Animation 优先级
void set_enabled(Button& button, const Style& style, bool enabled) {
    if (enabled) {
        style.clear_state_animation(button, "Disabled");  // 清除 Animation 槽
    } else {
        style.apply_state_animation(button, "Disabled");  // Animation(60) 覆盖 Local(50)
    }
}

// 使用
button.set_background(Colors::Red);           // Local(50)
set_enabled(button, button_style, false);     // Disabled（background=Gray 覆盖 Red）
set_enabled(button, button_style, true);      // 恢复（background 回退到 Red）
```

```cpp
// ❌ 坏：Disabled 状态使用 apply_state（无法覆盖 Local）
void set_enabled_bad(Button& button, const Style& style, bool enabled) {
    if (enabled) {
        style.clear_state_values(button, "Disabled");
    } else {
        style.apply_state(button, "Disabled");  // ❌ StyleTrigger(30) < Local(50)
    }
}

// 问题：Disabled 状态无法覆盖用户设置的 Local 值
button.set_background(Colors::Red);           // Local(50)
set_enabled_bad(button, button_style, false); // Disabled 无效（background 仍为 Red）
```

---

### ✅ 设置样式名称便于调试

```cpp
// ✅ 好：设置可读的样式名称
auto button_style = Style()
    .set_name("PrimaryButton")  // ← 设置名称
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});

// 日志输出
fmt::print("应用样式：{}\n", button_style.name());  // 输出：应用样式：PrimaryButton
```

```cpp
// ❌ 坏：不设置名称，调试困难
auto button_style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});

// 日志输出
fmt::print("应用样式：{}\n", button_style.name());  // 输出：应用样式：（空）
```

---

## 常见陷阱

### ❌ 陷阱 1：不清除旧状态导致残留

```cpp
// ❌ 错误：状态切换不清除旧状态
button_style.apply_state(my_button, "Hovered");   // background=LightBlue
button_style.apply_state(my_button, "Normal");    // ❌ Normal 无 setter，但 Hovered 未清除

// 问题：background 仍为 LightBlue（Hovered 的 StyleTrigger(30) 残留）
```

```cpp
// ✅ 正确：切换前先清除旧状态
button_style.clear_all_state_values(my_button);   // 清除 Hovered
button_style.apply_state(my_button, "Normal");    // background 回退到基线 Gray
```

---

### ❌ 陷阱 2：多 VSM 场景误用 clear_all_state_values

```cpp
// ❌ 错误：多 VSM 场景使用 clear_all_state_values
button_style.apply_state(my_button, "Hovered");   // CommonStates
button_style.apply_state(my_button, "Focused");   // FocusStates

// 切换 CommonStates 时误清 FocusStates
button_style.clear_all_state_values(my_button);   // ❌ 清除 Focused（误清！）
button_style.apply_state(my_button, "Pressed");   // border 被误清（丢失 Focused 样式）
```

```cpp
// ✅ 正确：多 VSM 场景使用 clear_state_values（精确清理）
button_style.clear_state_values(my_button, "Hovered");  // 只清除 Hovered
button_style.apply_state(my_button, "Pressed");         // border 保持 Focused 样式
```

---

### ❌ 陷阱 3：即时状态误用 apply_state（无法覆盖 Local）

```cpp
// ❌ 错误：Disabled 状态使用 apply_state（StyleTrigger 优先级不足）
button.set_background(Colors::Red);               // Local(50)
button_style.apply_state(button, "Disabled");     // ❌ StyleTrigger(30) < Local(50)

// 问题：background 仍为 Red（Disabled 无效）
```

```cpp
// ✅ 正确：Disabled 状态使用 apply_state_animation（Animation 优先级）
button.set_background(Colors::Red);                      // Local(50)
button_style.apply_state_animation(button, "Disabled");  // Animation(60) > Local(50)

// 结果：background=Gray（Disabled 强制生效）
```

---

### ❌ 陷阱 4：程序化路径使用 DynamicResource（暂不支持）

```cpp
// ❌ 错误：程序化路径使用 DynamicResource
auto style = Style()
    .add_setter(StyleSetter{
        .property = Control::background_property(),
        .value = {},
        .res_key = "PrimaryBrush"  // ❌ 程序化路径不处理 DynamicResource
    });

style.apply(my_button);  // DynamicResource setter 被忽略（background 无变化）
```

```cpp
// ✅ 正确：程序化路径使用静态值
auto style = Style()
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});  // ✅ 静态值

style.apply(my_button);  // background=Blue
```

```cpp
// ✅ 正确：DynamicResource 使用 mmlc 路径
// Button.mml
<Style TargetType="Button">
  <Setter Property="Background" Value="{DynamicResource PrimaryBrush}"/>  ✅
</Style>

// mmlc 生成 apply_fn_（包含资源订阅）
```

---

### ❌ 陷阱 5：父样式生命周期短于子样式

```cpp
// ❌ 错误：父样式局部变量，子样式持有悬空指针
Style create_child_style() {
    auto base_style = Style().set_name("Base");  // 局部变量
    auto child_style = Style()
        .set_name("Child")
        .set_base(&base_style);  // ❌ 悬空指针（base_style 即将销毁）
    return child_style;          // 返回后 base_style 已销毁
}

auto style = create_child_style();
style.apply(my_button);  // ❌ 崩溃（base_ 指向已销毁对象）
```

```cpp
// ✅ 正确：父样式生命周期至少与子样式相同
class StyleManager {
public:
    void init() {
        base_style_ = Style().set_name("Base");          // 成员变量（长生命周期）
        child_style_ = Style()
            .set_name("Child")
            .set_base(&base_style_);  // ✅ 安全（base_style_ 生命周期足够）
    }
    
    void apply_child_style(Button& button) {
        child_style_.apply(button);  // ✅ 安全
    }

private:
    Style base_style_;
    Style child_style_;
};
```

---

### ❌ 陷阱 6：忘记设置 target_type 导致样式无法匹配

```cpp
// ❌ 错误：未设置 target_type
auto style = Style()
    .set_name("MyButton")
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});
    // ❌ 未调用 set_target_type()

// StyleRegistry 按类型查找时无法匹配
registry.register_style("MyButton", &style);
auto* found = registry.find_style_for_type(Button::type_id());  // nullptr（类型不匹配）
```

```cpp
// ✅ 正确：设置 target_type
auto style = Style()
    .set_name("MyButton")
    .set_target_type(Button::type_id())  // ✅ 设置目标类型
    .add_setter(StyleSetter{Control::background_property(), Colors::Blue});

registry.register_style("MyButton", &style);
auto* found = registry.find_style_for_type(Button::type_id());  // ✅ 找到
```

---

## 完整示例：交互式按钮样式系统

以下示例展示 `Style` 的综合应用：BasedOn 继承、多状态切换、优先级演示、即时状态（Disabled）。

```cpp
#include <mine/ui/style/Style.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/visual/SolidColorBrush.h>
#include <mine/paint/Color.h>
#include <fmt/core.h>

using namespace mine;
using namespace mine::ui;
using namespace mine::ui::style;
using namespace mine::ui::controls;
using namespace mine::ui::visual;
using namespace mine::paint;

// ═══════════════════════════════════════════════════════════════════════════
// 1. 基础样式：定义公共属性
// ═══════════════════════════════════════════════════════════════════════════

Style create_base_button_style() {
    return Style()
        .set_name("BaseButton")
        .set_target_type(Button::type_id())
        // 公共属性
        .add_setter(StyleSetter{Control::padding_property(), Thickness{12, 6}})
        .add_setter(StyleSetter{Control::font_family_property(), "Segoe UI"})
        .add_setter(StyleSetter{Control::font_size_property(), 14.0})
        .add_setter(StyleSetter{Control::border_thickness_property(), Thickness{1}})
        .add_setter(StyleSetter{Control::cursor_property(), Cursors::Hand})
        .add_setter(StyleSetter{Control::horizontal_alignment_property(), HorizontalAlignment::Center})
        .add_setter(StyleSetter{Control::vertical_alignment_property(), VerticalAlignment::Center});
}

// ═══════════════════════════════════════════════════════════════════════════
// 2. 主按钮样式：继承基础样式 + 蓝色主题 + 视觉状态
// ═══════════════════════════════════════════════════════════════════════════

Style create_primary_button_style(Style* base_style) {
    return Style()
        .set_name("PrimaryButton")
        .set_target_type(Button::type_id())
        .set_base(base_style)  // ← 继承基础样式
        
        // 基线（Normal 状态）
        .add_setter(StyleSetter{Control::background_property(), Color{0xFF0078D4}})  // Blue
        .add_setter(StyleSetter{Control::foreground_property(), Color{0xFFFFFFFF}})  // White
        .add_setter(StyleSetter{Control::border_brush_property(), Color{0xFF0078D4}})
        
        // Hovered 状态（鼠标悬停）
        .add_state_setters(VisualStateSetters{
            .state_name = "Hovered",
            .setters = {
                StyleSetter{Control::background_property(), Color{0xFF106EBE}},      // Darker Blue
                StyleSetter{Control::border_brush_property(), Color{0xFF106EBE}}
            }
        })
        
        // Pressed 状态（鼠标按下）
        .add_state_setters(VisualStateSetters{
            .state_name = "Pressed",
            .setters = {
                StyleSetter{Control::background_property(), Color{0xFF005A9E}},      // Even Darker
                StyleSetter{Control::border_brush_property(), Color{0xFF005A9E}},
                StyleSetter{Control::foreground_property(), Color{0xFFE0E0E0}}       // Slightly Gray
            }
        })
        
        // Disabled 状态（即时状态，使用 Animation 优先级）
        .add_state_setters(VisualStateSetters{
            .state_name = "Disabled",
            .setters = {
                StyleSetter{Control::background_property(), Color{0xFFF3F3F3}},      // Light Gray
                StyleSetter{Control::foreground_property(), Color{0xFFA0A0A0}},      // Gray
                StyleSetter{Control::border_brush_property(), Color{0xFFD0D0D0}},
                StyleSetter{Control::opacity_property(), 0.6}
            }
        });
}

// ═══════════════════════════════════════════════════════════════════════════
// 3. 次要按钮样式：继承基础样式 + 灰色主题
// ═══════════════════════════════════════════════════════════════════════════

Style create_secondary_button_style(Style* base_style) {
    return Style()
        .set_name("SecondaryButton")
        .set_target_type(Button::type_id())
        .set_base(base_style)
        
        // 基线
        .add_setter(StyleSetter{Control::background_property(), Color{0xFFEEEEEE}})  // Light Gray
        .add_setter(StyleSetter{Control::foreground_property(), Color{0xFF000000}})  // Black
        .add_setter(StyleSetter{Control::border_brush_property(), Color{0xFFCCCCCC}})
        
        // Hovered 状态
        .add_state_setters(VisualStateSetters{
            .state_name = "Hovered",
            .setters = {
                StyleSetter{Control::background_property(), Color{0xFFE0E0E0}},
                StyleSetter{Control::border_brush_property(), Color{0xFFB0B0B0}}
            }
        })
        
        // Pressed 状态
        .add_state_setters(VisualStateSetters{
            .state_name = "Pressed",
            .setters = {
                StyleSetter{Control::background_property(), Color{0xFFD0D0D0}},
                StyleSetter{Control::border_brush_property(), Color{0xFF909090}},
                StyleSetter{Control::foreground_property(), Color{0xFF303030}}
            }
        })
        
        // Disabled 状态
        .add_state_setters(VisualStateSetters{
            .state_name = "Disabled",
            .setters = {
                StyleSetter{Control::background_property(), Color{0xFFF8F8F8}},
                StyleSetter{Control::foreground_property(), Color{0xFFC0C0C0}},
                StyleSetter{Control::border_brush_property(), Color{0xFFE0E0E0}},
                StyleSetter{Control::opacity_property(), 0.5}
            }
        });
}

// ═══════════════════════════════════════════════════════════════════════════
// 4. 状态管理器：处理视觉状态切换
// ═══════════════════════════════════════════════════════════════════════════

class ButtonStateManager {
public:
    explicit ButtonStateManager(Button& button, const Style& style)
        : button_(button), style_(style) {}
    
    // 切换到新状态（CommonStates：Normal/Hovered/Pressed）
    void go_to_common_state(core::StringView new_state) {
        fmt::print("[状态切换] {} -> {}\n", current_state_, new_state);
        
        // 清除旧状态的 StyleTrigger(30) 槽
        style_.clear_all_state_values(button_);
        
        // 应用新状态（若为 Normal，则回退到基线）
        if (!new_state.empty() && new_state != "Normal") {
            style_.apply_state(button_, new_state);
        }
        
        current_state_ = new_state;
    }
    
    // 设置启用/禁用状态（即时状态，使用 Animation 优先级）
    void set_enabled(bool enabled) {
        if (enabled == is_enabled_) return;
        
        fmt::print("[启用状态] {} -> {}\n", is_enabled_ ? "Enabled" : "Disabled",
                   enabled ? "Enabled" : "Disabled");
        
        if (enabled) {
            // 清除 Disabled 的 Animation(60) 槽
            style_.clear_state_animation(button_, "Disabled");
        } else {
            // 以 Animation(60) 应用 Disabled 状态（覆盖 Local）
            style_.apply_state_animation(button_, "Disabled");
        }
        
        is_enabled_ = enabled;
    }
    
    [[nodiscard]] core::StringView current_state() const { return current_state_; }
    [[nodiscard]] bool is_enabled() const { return is_enabled_; }

private:
    Button&     button_;
    const Style& style_;
    core::StringView current_state_{"Normal"};
    bool        is_enabled_{true};
};

// ═══════════════════════════════════════════════════════════════════════════
// 5. 优先级演示工具
// ═══════════════════════════════════════════════════════════════════════════

void demonstrate_priority(Button& button, const Style& style) {
    fmt::print("\n╔════════════════════════════════════════════════════════════════╗\n");
    fmt::print("║           优先级演示：Local vs StyleSetter vs StyleTrigger      ║\n");
    fmt::print("╚════════════════════════════════════════════════════════════════╝\n");
    
    // 1. 应用样式（StyleSetter 优先级）
    fmt::print("\n[1] 应用样式（StyleSetter 优先级 20）\n");
    style.apply(button);
    auto bg1 = button.background();
    fmt::print("    background = {}\n", bg1 ? "Blue" : "null");
    
    // 2. 用户设置 Local 值
    fmt::print("\n[2] 用户设置 Local 值（Local 优先级 50）\n");
    button.set_background(Color{0xFFFF0000});  // Red
    auto bg2 = button.background();
    fmt::print("    background = Red (Local 覆盖 StyleSetter)\n");
    
    // 3. 再次应用样式（不会覆盖 Local）
    fmt::print("\n[3] 再次应用样式（StyleSetter 20 < Local 50）\n");
    style.apply(button);
    auto bg3 = button.background();
    fmt::print("    background = Red (样式不覆盖 Local)\n");
    
    // 4. 应用 Hovered 状态（StyleTrigger 优先级）
    fmt::print("\n[4] 应用 Hovered 状态（StyleTrigger 30 < Local 50）\n");
    style.apply_state(button, "Hovered");
    auto bg4 = button.background();
    fmt::print("    background = Red (StyleTrigger 不覆盖 Local)\n");
    
    // 5. 清除 Local 值
    fmt::print("\n[5] 清除 Local 值（移除优先级 50 槽）\n");
    button.clear_value(Control::background_property(), ValuePriority::Local);
    auto bg5 = button.background();
    fmt::print("    background = LightBlue (回退到 StyleTrigger)\n");
    
    // 6. 清除 Hovered 状态
    fmt::print("\n[6] 清除 Hovered 状态（移除优先级 30 槽）\n");
    style.clear_state_values(button, "Hovered");
    auto bg6 = button.background();
    fmt::print("    background = Blue (回退到 StyleSetter)\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 6. 即时状态演示（Disabled 覆盖 Local）
// ═══════════════════════════════════════════════════════════════════════════

void demonstrate_instant_state(Button& button, const Style& style) {
    fmt::print("\n╔════════════════════════════════════════════════════════════════╗\n");
    fmt::print("║       即时状态演示：Disabled (Animation 60) 覆盖 Local (50)     ║\n");
    fmt::print("╚════════════════════════════════════════════════════════════════╝\n");
    
    // 1. 应用样式 + 用户设置 Local 值
    fmt::print("\n[1] 应用样式 + 用户设置 Local 值\n");
    style.apply(button);
    button.set_background(Color{0xFFFF0000});  // Red (Local)
    fmt::print("    background = Red (Local 50)\n");
    
    // 2. 禁用按钮（Animation 优先级覆盖 Local）
    fmt::print("\n[2] 禁用按钮（apply_state_animation 使用 Animation 60）\n");
    style.apply_state_animation(button, "Disabled");
    fmt::print("    background = LightGray (Animation 60 覆盖 Local 50)\n");
    
    // 3. 启用按钮（清除 Animation 槽）
    fmt::print("\n[3] 启用按钮（clear_state_animation 移除 Animation 60 槽）\n");
    style.clear_state_animation(button, "Disabled");
    fmt::print("    background = Red (回退到 Local 50)\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 7. 多状态交互演示
// ═══════════════════════════════════════════════════════════════════════════

void demonstrate_state_transitions(Button& button, const Style& style) {
    fmt::print("\n╔════════════════════════════════════════════════════════════════╗\n");
    fmt::print("║                    多状态交互演示                                ║\n");
    fmt::print("╚════════════════════════════════════════════════════════════════╝\n");
    
    ButtonStateManager state_mgr(button, style);
    
    // 1. 初始化（Normal）
    fmt::print("\n[1] 初始化（Normal 状态）\n");
    style.apply(button);
    fmt::print("    background = Blue (StyleSetter 基线)\n");
    
    // 2. 鼠标悬停
    fmt::print("\n[2] 鼠标悬停（Normal -> Hovered）\n");
    state_mgr.go_to_common_state("Hovered");
    fmt::print("    background = LightBlue (StyleTrigger 覆盖 StyleSetter)\n");
    
    // 3. 鼠标按下
    fmt::print("\n[3] 鼠标按下（Hovered -> Pressed）\n");
    state_mgr.go_to_common_state("Pressed");
    fmt::print("    background = DarkBlue\n");
    fmt::print("    foreground = LightGray\n");
    
    // 4. 鼠标释放（回到 Hovered）
    fmt::print("\n[4] 鼠标释放（Pressed -> Hovered）\n");
    state_mgr.go_to_common_state("Hovered");
    fmt::print("    background = LightBlue (清除 Pressed + 应用 Hovered)\n");
    
    // 5. 鼠标离开（回到 Normal）
    fmt::print("\n[5] 鼠标离开（Hovered -> Normal）\n");
    state_mgr.go_to_common_state("Normal");
    fmt::print("    background = Blue (清除 Hovered，回退到 StyleSetter 基线)\n");
    
    // 6. 禁用按钮（即时状态）
    fmt::print("\n[6] 禁用按钮（Enabled -> Disabled）\n");
    state_mgr.set_enabled(false);
    fmt::print("    background = LightGray (Animation 60 强制覆盖)\n");
    fmt::print("    opacity = 0.6\n");
    
    // 7. 启用按钮
    fmt::print("\n[7] 启用按钮（Disabled -> Enabled）\n");
    state_mgr.set_enabled(true);
    fmt::print("    background = Blue (清除 Animation，回退到 StyleSetter)\n");
    fmt::print("    opacity = 1.0\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 8. 主函数：综合演示
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    fmt::print("╔════════════════════════════════════════════════════════════════╗\n");
    fmt::print("║              mine::ui::style::Style 完整示例                    ║\n");
    fmt::print("║  BasedOn 继承 + 多状态切换 + 优先级演示 + 即时状态（Disabled） ║\n");
    fmt::print("╚════════════════════════════════════════════════════════════════╝\n");
    
    // ═══════════════════════════════════════════════════════════════════════
    // 创建样式
    // ═══════════════════════════════════════════════════════════════════════
    
    auto base_style = create_base_button_style();
    auto primary_style = create_primary_button_style(&base_style);
    auto secondary_style = create_secondary_button_style(&base_style);
    
    Button primary_button;
    Button secondary_button;
    
    // ═══════════════════════════════════════════════════════════════════════
    // 演示 1：优先级系统
    // ═══════════════════════════════════════════════════════════════════════
    
    demonstrate_priority(primary_button, primary_style);
    
    // ═══════════════════════════════════════════════════════════════════════
    // 演示 2：即时状态（Disabled）
    // ═══════════════════════════════════════════════════════════════════════
    
    demonstrate_instant_state(primary_button, primary_style);
    
    // ═══════════════════════════════════════════════════════════════════════
    // 演示 3：多状态交互
    // ═══════════════════════════════════════════════════════════════════════
    
    demonstrate_state_transitions(primary_button, primary_style);
    
    // ═══════════════════════════════════════════════════════════════════════
    // 演示 4：BasedOn 继承
    // ═══════════════════════════════════════════════════════════════════════
    
    fmt::print("\n╔════════════════════════════════════════════════════════════════╗\n");
    fmt::print("║                   BasedOn 继承演示                               ║\n");
    fmt::print("╚════════════════════════════════════════════════════════════════╝\n");
    
    fmt::print("\n[主按钮] 继承基础样式（padding、font）+ 蓝色主题\n");
    primary_style.apply(primary_button);
    fmt::print("    padding = 12,6 (继承自 BaseButton)\n");
    fmt::print("    font_size = 14 (继承自 BaseButton)\n");
    fmt::print("    background = Blue (PrimaryButton 覆盖)\n");
    
    fmt::print("\n[次要按钮] 继承基础样式（padding、font）+ 灰色主题\n");
    secondary_style.apply(secondary_button);
    fmt::print("    padding = 12,6 (继承自 BaseButton)\n");
    fmt::print("    font_size = 14 (继承自 BaseButton)\n");
    fmt::print("    background = LightGray (SecondaryButton 覆盖)\n");
    
    fmt::print("\n╔════════════════════════════════════════════════════════════════╗\n");
    fmt::print("║                        演示完成                                  ║\n");
    fmt::print("╚════════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
```

**运行输出（部分）**：

```
╔════════════════════════════════════════════════════════════════╗
║           优先级演示：Local vs StyleSetter vs StyleTrigger      ║
╚════════════════════════════════════════════════════════════════╝

[1] 应用样式（StyleSetter 优先级 20）
    background = Blue

[2] 用户设置 Local 值（Local 优先级 50）
    background = Red (Local 覆盖 StyleSetter)

[3] 再次应用样式（StyleSetter 20 < Local 50）
    background = Red (样式不覆盖 Local)

[4] 应用 Hovered 状态（StyleTrigger 30 < Local 50）
    background = Red (StyleTrigger 不覆盖 Local)

[5] 清除 Local 值（移除优先级 50 槽）
    background = LightBlue (回退到 StyleTrigger)

[6] 清除 Hovered 状态（移除优先级 30 槽）
    background = Blue (回退到 StyleSetter)

╔════════════════════════════════════════════════════════════════╗
║       即时状态演示：Disabled (Animation 60) 覆盖 Local (50)     ║
╚════════════════════════════════════════════════════════════════╝

[1] 应用样式 + 用户设置 Local 值
    background = Red (Local 50)

[2] 禁用按钮（apply_state_animation 使用 Animation 60）
    background = LightGray (Animation 60 覆盖 Local 50)

[3] 启用按钮（clear_state_animation 移除 Animation 60 槽）
    background = Red (回退到 Local 50)

╔════════════════════════════════════════════════════════════════╗
║                    多状态交互演示                                ║
╚════════════════════════════════════════════════════════════════╝

[1] 初始化（Normal 状态）
    background = Blue (StyleSetter 基线)

[2] 鼠标悬停（Normal -> Hovered）
[状态切换] Normal -> Hovered
    background = LightBlue (StyleTrigger 覆盖 StyleSetter)

[3] 鼠标按下（Hovered -> Pressed）
[状态切换] Hovered -> Pressed
    background = DarkBlue
    foreground = LightGray

[4] 鼠标释放（Pressed -> Hovered）
[状态切换] Pressed -> Hovered
    background = LightBlue (清除 Pressed + 应用 Hovered)

[5] 鼠标离开（Hovered -> Normal）
[状态切换] Hovered -> Normal
    background = Blue (清除 Hovered，回退到 StyleSetter 基线)

[6] 禁用按钮（Enabled -> Disabled）
[启用状态] Enabled -> Disabled
    background = LightGray (Animation 60 强制覆盖)
    opacity = 0.6

[7] 启用按钮（Disabled -> Enabled）
[启用状态] Disabled -> Enabled
    background = Blue (清除 Animation，回退到 StyleSetter)
    opacity = 1.0

╔════════════════════════════════════════════════════════════════╗
║                        演示完成                                  ║
╚════════════════════════════════════════════════════════════════╝
```

---

## 总结

`Style` 是 MineUI 样式系统的核心类，提供以下关键能力：

1. **双路径设计**：
   - mmlc 路径：高性能（预生成函数 + 资源订阅）
   - 程序化路径：灵活（运行时动态构建）

2. **优先级系统**：
   - 7 级优先级链（Animation > Local > TemplateBind > StyleTrigger > StyleSetter > Inherited > Default）
   - 明确的覆盖规则（高优先级覆盖低优先级）

3. **BasedOn 继承**：
   - 复用公共属性 setter（减少重复代码）
   - 构建样式变体（基础样式 → 主题变体）

4. **视觉状态支持**：
   - `apply_state()`：StyleTrigger(30) 应用状态 setter
   - `clear_all_state_values()` / `clear_state_values()`：清除状态残留
   - 多 VSM 场景（CommonStates + FocusStates）

5. **即时状态**：
   - `apply_state_animation()`：Animation(60) 强制覆盖 Local(50)
   - 用于 Disabled、ReadOnly 等必须生效的状态

6. **构建器接口**：
   - 流式 API（链式调用）
   - 运行时动态修改样式
   - 样式编辑器集成

通过合理使用 `Style`，可以构建灵活、可复用、高性能的控件样式系统。
