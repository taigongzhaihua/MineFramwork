# mine.ui.property —— 依赖属性系统模块

## 模块概述

`mine.ui.property` 是 UI 框架的数据绑定基础层，提供类似 WPF/UWP 的**依赖属性**（Dependency Property）系统：支持多优先级值覆盖、属性变更通知、继承属性、动画驱动值。

| 核心类型 | 职责 |
|---------|------|
| `DependencyProperty` | 全局唯一的属性描述符（名称、类型、默认值、元数据） |
| `DependencyObject` | 属性值存储容器，支持按优先级读写和变更通知 |
| `PropertyMetadata` | 属性的行为配置（影响布局/渲染、继承性、变更回调） |
| `ValuePriority` | 属性值的优先级枚举 |

**依赖**：`mine.core`（`Variant`、`TypeId`、`StringView`、`Pimpl`）。

---

## 1. 优先级体系 —— `ValuePriority`

**文件**：`<mine/ui/property/ValuePriority.h>`

每个依赖属性可同时存储多个不同优先级的"槽位"，系统按优先级从高到低选择生效值。

```cpp
namespace mine::ui {

enum class ValuePriority : uint8_t {
    Default        = 0,   // 属性默认值（定义在 DependencyProperty 中）
    Inherited      = 10,  // 从父元素继承的值
    StyleSetter    = 20,  // 样式 P5 基线 setter（Style::apply）
    StyleTrigger   = 30,  // 样式 P4 状态触发器（Style::apply_state）
    Local          = 50,  // 用户通过 set_xxx() 直接设置的值
    Animation      = 60,  // 动画驱动的值（Storyboard 逐帧写入）
};

} // namespace mine::ui
```

优先级链：`Animation(60)` > `Local(50)` > `StyleTrigger(30)` > `StyleSetter(20)` > `Inherited(10)` > `Default(0)`

---

## 2. 属性描述符 —— `DependencyProperty`

**文件**：`<mine/ui/property/DependencyProperty.h>`

全局唯一的属性定义，通过 `register_property<T>()` 模板注册。

### 类摘要

```cpp
namespace mine::ui {

class DependencyProperty {
public:
    // 不可拷贝/移动（全局唯一，身份 = 地址）
    DependencyProperty(const DependencyProperty&) = delete;
    DependencyProperty& operator=(const DependencyProperty&) = delete;

    // 查询
    StringView           name()          const noexcept;  // 属性名（如 "Content"）
    TypeId               owner_type()    const noexcept;  // 所有者类型
    TypeId               value_type()    const noexcept;  // 值类型
    const Variant&       default_value() const noexcept;  // 默认值（无有效槽时返回）
    const PropertyMetadata& metadata()   const noexcept;  // 元数据
    bool                 is_attached()   const noexcept;  // 是否附加属性

    // 身份比较（地址比较，O(1)）
    bool operator==(const DependencyProperty&) const noexcept;
    bool operator!=(const DependencyProperty&) const noexcept;
};

// ── 注册函数 ─────────────────────────────────────────────────────────────

// 完整参数注册（需显式指定所有者类型、值类型、名称、默认值、元数据）
const DependencyProperty& register_property(
    StringView name,
    TypeId owner_type,
    TypeId value_type,
    Variant default_value,
    PropertyMetadata metadata);

// 模板便捷注册（自动推导 owner_type 和 value_type）
template<typename TOwner>
const DependencyProperty& register_property(
    StringView name,
    Variant default_value,
    PropertyMetadata metadata);

// 注册附加属性
template<typename TOwner>
const DependencyProperty& register_attached_property(
    StringView name,
    Variant default_value,
    PropertyMetadata metadata);

} // namespace mine::ui
```

### 使用示例

```cpp
// 头文件声明
class Button : public Control {
public:
    static const DependencyProperty& ContentProperty;
};

// 实现文件注册
const DependencyProperty& Button::ContentProperty =
    register_property<Button>(
        "Content",
        Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .changed = &Button::on_content_changed,
        });
```

---

## 3. 属性元数据 —— `PropertyMetadata`

**文件**：`<mine/ui/property/PropertyMetadata.h>`

```cpp
namespace mine::ui {

struct PropertyMetadata {
    // ── 自动失效标志 ─────────────────────────────────────────────────────
    bool affects_measure = false;   // 值变更后自动 invalidate_measure()
    bool affects_arrange = false;   // 值变更后自动 invalidate_arrange()
    bool affects_render  = false;   // 值变更后自动 invalidate_render()

    // ── 继承性 ───────────────────────────────────────────────────────────
    bool inherits = false;          // true 时值自动向下传播到子元素（Inherited 优先级）

    // ── 变更回调 ─────────────────────────────────────────────────────────
    // 属性值变更时调用的函数指针（nullptr = 无回调）
    // 签名：void(DependencyObject*, const DependencyProperty&,
    //            const Variant& old_value, const Variant& new_value)
    void (*changed)(DependencyObject*,
                    const DependencyProperty&,
                    const Variant&,
                    const Variant&) noexcept = nullptr;
};

} // namespace mine::ui
```

---

## 4. 属性值容器 —— `DependencyObject`

**文件**：`<mine/ui/property/DependencyObject.h>`

所有 UI 元素的最终基类，提供多优先级属性值存储与变更通知。

### 类摘要

```cpp
namespace mine::ui {

using PropertyChangedCallbackFn = void (*)(DependencyObject*       sender,
                                           const DependencyProperty& prop,
                                           const Variant&           old_value,
                                           const Variant&           new_value,
                                           void*                    user_data);

class DependencyObject {
public:
    DependencyObject();
    virtual ~DependencyObject();

    // 禁止拷贝，允许移动
    DependencyObject(const DependencyObject&) = delete;
    DependencyObject& operator=(const DependencyObject&) = delete;
    DependencyObject(DependencyObject&&) = default;
    DependencyObject& operator=(DependencyObject&&) = default;

    // ── 属性值读写 ──────────────────────────────────────────────────────

    // 读取当前生效值（最高优先级）
    const Variant& get_value(const DependencyProperty& prop) const noexcept;

    // 读取指定优先级上限以下的生效值（供动画系统使用）
    const Variant& get_value(const DependencyProperty& prop,
                             ValuePriority max_priority) const noexcept;

    // 在指定优先级写入值
    void set_value(const DependencyProperty& prop,
                   const Variant& value,
                   ValuePriority priority = ValuePriority::Local);

    // 检查指定优先级是否有值
    bool has_value(const DependencyProperty& prop,
                   ValuePriority priority) const noexcept;

    // 清除指定优先级的值
    void clear_value(const DependencyProperty& prop,
                     ValuePriority priority);

    // ── 事件订阅 ────────────────────────────────────────────────────────

    // 订阅此对象上所有属性的变更通知
    // 返回订阅 token（用于取消订阅）
    uint64_t subscribe_property_changed(PropertyChangedCallbackFn fn,
                                        void* user_data = nullptr) noexcept;
    void unsubscribe_property_changed(uint64_t token) noexcept;

    // ── bind_property（DP↔DP 绑定）───────────────────────────────────────

    // 将本对象的 target_prop 单向绑定到 source_obj.source_prop
    // source 变更时自动同步到 target
    void bind_property(const DependencyProperty& target_prop,
                       DependencyObject& source_obj,
                       const DependencyProperty& source_prop);

protected:
    // ── 属性变更钩子（子类可覆盖）────────────────────────────────────────

    // 任意属性变更时调用
    virtual void on_property_changed(const DependencyProperty& prop,
                                     const Variant& old_value,
                                     const Variant& new_value);

    // ── 失效标记（子类可覆盖以连接布局/渲染管线）─────────────────────────

    virtual void invalidate_measure();
    virtual void invalidate_arrange();
    virtual void invalidate_render();
};

} // namespace mine::ui
```

### 使用示例

```cpp
// 写入属性
btn->set_value(Button::BackgroundProperty,
               Variant{Brush::solid_rgb(0x6750A4)},
               ValuePriority::Local);

// 读取生效值（考虑所有优先级层）
const Variant& bg = btn->get_value(Button::BackgroundProperty);

// 清除本地值，回退到样式/默认值
btn->clear_value(Button::BackgroundProperty, ValuePriority::Local);

// DP↔DP 绑定（源属性变更自动同步目标属性）
label->bind_property(TextBlock::ForegroundProperty,
                     *btn, Button::ForegroundProperty);

// 订阅变更通知
btn->subscribe_property_changed([](DependencyObject* sender,
                                    const DependencyProperty& prop,
                                    const Variant& old_v,
                                    const Variant& new_v,
                                    void*) {
    // 响应变更
}, nullptr);
```

---

## 5. 架构设计决策

### 为什么要多优先级？

不同来源的值应具有不同的优先级，使得"用户设置的值"不被"样式基线的值"覆盖，但"动画驱动的值"可以覆盖"用户设置的值"。

```
Animation(60)  ← Storyboard 逐帧驱动
Local(50)      ← 用户 set_xxx()
StyleTrigger(30) ← 状态机 Hovered/Pressed
StyleSetter(20)  ← 默认样式基线
Inherited(10)    ← 父元素传播
Default(0)       ← 属性注册默认值
```

### 为什么 bind_property 是单向的？

`bind_property` 实现为单向绑定（source → target），避免循环依赖。双向绑定（`<=>`）由 `mine.ui.binding` 模块在更高层提供。

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.core` | 基础依赖（`Variant`、`TypeId`） |
| `mine.ui.style` | 样式 setter 通过 `StyleSetter`/`StyleTrigger` 优先级写入属性 |
| `mine.ui.animation` | `Storyboard` 通过 `Animation` 优先级逐帧写入 |
| `mine.ui.binding` | 在 `bind_property` 之上提供双向绑定和表达式绑定 |
