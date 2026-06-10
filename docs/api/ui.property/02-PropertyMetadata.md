# PropertyMetadata 详细接口文档

## 概述

`PropertyMetadata` 是 `mine.ui.property` 模块的**依赖属性元数据结构体**。

**核心特性：**
- **布局/绘制失效标志**：affects_measure、affects_arrange、affects_render
- **值继承标志**：inherits（值沿视觉树向下继承）
- **变更回调**：changed（属性变更时的自定义回调函数）

---

## 文件位置

```
src/mine/ui/property/api/include/mine/ui/property/PropertyMetadata.h
```

---

## 类型定义

### PropertyChangedFn

```cpp
using PropertyChangedFn = void (*)(
    DependencyObject*       obj,
    const DependencyProperty& prop,
    const mine::core::Variant& old_value,
    const mine::core::Variant& new_value);
```

**描述**：属性变更回调函数指针类型。

**参数**：
- `obj`：发生属性变更的 DependencyObject 实例
- `prop`：发生变更的属性描述符
- `old_value`：变更前的有效值
- `new_value`：变更后的有效值

**特征**：
- 回调运行在 UI 线程
- 不得在回调内部再次调用同属性的 set_value()（会被防递归保护忽略）

---

### PropertyMetadata

```cpp
struct PropertyMetadata {
    /// 属性变更后是否触发 DependencyObject::invalidate_measure()
    bool              affects_measure = false;
    /// 属性变更后是否触发 DependencyObject::invalidate_arrange()
    bool              affects_arrange = false;
    /// 属性变更后是否触发 DependencyObject::invalidate_render()
    bool              affects_render  = false;
    /// 属性值是否沿可视化树向下继承（如 FontSize、TextColor）
    bool              inherits        = false;
    /// 属性变更回调（nullptr 表示不需要自定义回调），在 on_property_changed() 之后调用
    PropertyChangedFn changed         = nullptr;
};
```

**描述**：依赖属性元数据，描述属性的行为特征和变更回调。

**使用方式**：使用 C++20 指定初始化语法（designated initializers）构造：
```cpp
PropertyMetadata meta{
    .affects_measure = true,
    .changed = &MyClass::s_on_content_changed
};
```

---

## 成员字段

### affects_measure

```cpp
bool affects_measure = false;
```

**描述**：属性变更后是否触发 DependencyObject::invalidate_measure()。

**用途**：
- 影响布局测量的属性（如 Width、Height、FontSize）
- 变更后需要重新测量控件尺寸

---

### affects_arrange

```cpp
bool affects_arrange = false;
```

**描述**：属性变更后是否触发 DependencyObject::invalidate_arrange()。

**用途**：
- 影响布局排列的属性（如 HorizontalAlignment、VerticalAlignment）
- 变更后需要重新排列控件位置

---

### affects_render

```cpp
bool affects_render = false;
```

**描述**：属性变更后是否触发 DependencyObject::invalidate_render()。

**用途**：
- 影响绘制的属性（如 Background、Foreground、BorderBrush）
- 变更后需要重新绘制控件

---

### inherits

```cpp
bool inherits = false;
```

**描述**：属性值是否沿可视化树向下继承。

**用途**：
- 沿视觉树向下传播的属性（如 FontSize、TextColor、DataContext）
- 子元素未设置值时自动继承父元素的值

---

### changed

```cpp
PropertyChangedFn changed = nullptr;
```

**描述**：属性变更回调（nullptr 表示不需要自定义回调），在 on_property_changed() 之后调用。

**用途**：
- 属性变更时的自定义处理逻辑
- 在 on_property_changed() 虚方法之后调用

---

## 使用场景

### 1. 注册影响布局测量的属性

```cpp
static const DependencyProperty& WidthProp =
    DependencyProperty::register_property<Button, double>(
        "Width",
        0.0,
        PropertyMetadata{.affects_measure = true});
```

---

### 2. 注册影响布局排列的属性

```cpp
static const DependencyProperty& HorizontalAlignmentProp =
    DependencyProperty::register_property<Control, HorizontalAlignment>(
        "HorizontalAlignment",
        HorizontalAlignment::Stretch,
        PropertyMetadata{.affects_arrange = true});
```

---

### 3. 注册影响绘制的属性

```cpp
static const DependencyProperty& BackgroundProp =
    DependencyProperty::register_property<Control, Brush>(
        "Background",
        nullptr,
        PropertyMetadata{.affects_render = true});
```

---

### 4. 注册可继承的属性

```cpp
static const DependencyProperty& FontSizeProp =
    DependencyProperty::register_property<Control, double>(
        "FontSize",
        14.0,
        PropertyMetadata{
            .affects_measure = true,
            .inherits = true});  // 子元素自动继承父元素的 FontSize
```

---

### 5. 注册带变更回调的属性

```cpp
// 静态回调函数
static void on_content_changed(
    DependencyObject* obj,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    auto* button = static_cast<Button*>(obj);
    // ... 处理 Content 变更
}

// 注册属性
static const DependencyProperty& ContentProp =
    DependencyProperty::register_property<Button, Variant>(
        "Content",
        Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .changed = &on_content_changed});
```

---

### 6. 综合示例：注册复杂属性

```cpp
static const DependencyProperty& DataContextProp =
    DependencyProperty::register_property<Control, Variant>(
        "DataContext",
        Variant{},
        PropertyMetadata{
            .inherits = true,  // 沿视觉树向下继承
            .changed = &on_data_context_changed});  // 变更回调

static void on_data_context_changed(
    DependencyObject* obj,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    // 处理 DataContext 变更
    // - 通知绑定系统更新
    // - 触发子元素的 DataContext 继承
}
```

---

## 属性变更通知流程

### 完整通知流程

当属性值变更时，DependencyObject::set_value() 按以下顺序触发通知：

```
1. on_property_changed()（虚方法，子类可覆盖）
2. invalidate_measure()（如果 affects_measure = true）
3. invalidate_arrange()（如果 affects_arrange = true）
4. invalidate_render()（如果 affects_render = true）
5. PropertyMetadata.changed 回调（如果已设置）
6. 所有订阅者回调（通过 subscribe_property_changed() 注册）
```

---

### 防递归保护

```cpp
// ❌ 问题：回调内部再次调用同属性的 set_value()
static void on_width_changed(
    DependencyObject* obj,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    // 这将被防递归保护忽略
    obj->set_value(prop, Variant{100});
}

// ✅ 解决：回调内部设置其他属性
static void on_width_changed(
    DependencyObject* obj,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    // 允许：设置其他属性
    obj->set_value(HeightProp, Variant{new_value.get<double>() * 0.5});
}
```

---

## 最佳实践

### 1. 正确设置 affects_* 标志

```cpp
// ✅ 推荐：Width 影响测量
PropertyMetadata{.affects_measure = true}

// ✅ 推荐：Background 影响绘制
PropertyMetadata{.affects_render = true}

// ✅ 推荐：HorizontalAlignment 影响排列
PropertyMetadata{.affects_arrange = true}

// ❌ 不推荐：过度标记（所有标志都为 true）
PropertyMetadata{
    .affects_measure = true,
    .affects_arrange = true,
    .affects_render = true}  // 性能损失
```

---

### 2. 谨慎使用 inherits 标志

```cpp
// ✅ 推荐：FontSize 需要继承
PropertyMetadata{
    .affects_measure = true,
    .inherits = true}

// ✅ 推荐：DataContext 需要继承
PropertyMetadata{.inherits = true}

// ❌ 不推荐：Width 不应继承（每个控件有自己的宽度）
PropertyMetadata{
    .affects_measure = true,
    .inherits = true}  // 错误
```

---

### 3. 变更回调使用静态函数

```cpp
// ✅ 推荐：使用静态成员函数
class Button {
    static void s_on_content_changed(
        DependencyObject* obj,
        const DependencyProperty& prop,
        const Variant& old_value,
        const Variant& new_value);
};

PropertyMetadata{.changed = &Button::s_on_content_changed}

// ❌ 不推荐：使用 lambda（无法赋值给函数指针）
PropertyMetadata{.changed = [](auto*, auto&, auto&, auto&) {}}  // 编译错误
```

---

### 4. 回调内避免递归

```cpp
// ✅ 推荐：回调内设置其他属性
static void on_width_changed(...) {
    obj->set_value(HeightProp, Variant{...});  // 允许
}

// ❌ 不推荐：回调内设置同一属性
static void on_width_changed(...) {
    obj->set_value(WidthProp, Variant{...});  // 被忽略
}
```

---

## 常见陷阱

### 1. affects_* 标志设置错误

```cpp
// ❌ 问题：Width 应影响测量，但未设置 affects_measure
static const DependencyProperty& WidthProp =
    DependencyProperty::register_property<Control, double>(
        "Width",
        0.0,
        PropertyMetadata{});  // 缺少 affects_measure

// ✅ 解决：正确设置 affects_measure
static const DependencyProperty& WidthProp =
    DependencyProperty::register_property<Control, double>(
        "Width",
        0.0,
        PropertyMetadata{.affects_measure = true});
```

---

### 2. 回调内部递归设置同一属性

```cpp
// ❌ 问题：回调内部再次设置同一属性（被忽略）
static void on_width_changed(
    DependencyObject* obj,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    // 这将被防递归保护忽略
    obj->set_value(WidthProp, Variant{100});
}

// ✅ 解决：在回调外部设置，或设置其他属性
```

---

### 3. inherits 标志误用

```cpp
// ❌ 问题：Width 不应继承
PropertyMetadata{
    .affects_measure = true,
    .inherits = true}  // 错误

// ✅ 解决：仅对需要继承的属性设置 inherits
PropertyMetadata{
    .affects_measure = true}  // Width 不继承
```

---

### 4. 回调函数签名错误

```cpp
// ❌ 问题：回调函数签名不匹配
static void on_changed(DependencyObject* obj) {  // 参数不足
    // ...
}
PropertyMetadata{.changed = &on_changed};  // 编译错误

// ✅ 解决：使用正确的签名
static void on_changed(
    DependencyObject* obj,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    // ...
}
PropertyMetadata{.changed = &on_changed};
```

---

## 完整示例

```cpp
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/core/Variant.h>

using namespace mine::ui;
using namespace mine::core;

// 定义控件类
class MyControl : public DependencyObject {
public:
    // 属性变更回调（静态成员函数）
    static void s_on_content_changed(
        DependencyObject* obj,
        const DependencyProperty& prop,
        const Variant& old_value,
        const Variant& new_value) {
        
        auto* control = static_cast<MyControl*>(obj);
        control->on_content_changed_impl(old_value, new_value);
    }
    
private:
    void on_content_changed_impl(
        const Variant& old_value,
        const Variant& new_value) {
        
        // 处理 Content 变更
        // - 移除旧内容的视觉子元素
        // - 添加新内容的视觉子元素
    }
};

// 注册属性
static const DependencyProperty& WidthProp =
    DependencyProperty::register_property<MyControl, double>(
        "Width",
        0.0,
        PropertyMetadata{
            .affects_measure = true});  // 影响布局测量

static const DependencyProperty& BackgroundProp =
    DependencyProperty::register_property<MyControl, Variant>(
        "Background",
        Variant{},
        PropertyMetadata{
            .affects_render = true});  // 影响绘制

static const DependencyProperty& FontSizeProp =
    DependencyProperty::register_property<MyControl, double>(
        "FontSize",
        14.0,
        PropertyMetadata{
            .affects_measure = true,
            .inherits = true});  // 影响测量 + 可继承

static const DependencyProperty& ContentProp =
    DependencyProperty::register_property<MyControl, Variant>(
        "Content",
        Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .changed = &MyControl::s_on_content_changed});  // 影响测量 + 变更回调

int main() {
    MyControl control;
    
    // 设置 Width -> 触发 invalidate_measure()
    control.set_value(WidthProp, Variant{100.0});
    
    // 设置 Background -> 触发 invalidate_render()
    control.set_value(BackgroundProp, Variant{Color{1.0f, 0.0f, 0.0f, 1.0f}});
    
    // 设置 FontSize -> 触发 invalidate_measure()，子元素自动继承
    control.set_value(FontSizeProp, Variant{16.0});
    
    // 设置 Content -> 触发 invalidate_measure() + s_on_content_changed 回调
    control.set_value(ContentProp, Variant{String{"Hello"}});
    
    return 0;
}
```

---

## 总结

`PropertyMetadata` 是 `mine.ui.property` 模块的依赖属性元数据结构体，具备：

1. **布局/绘制失效标志**：affects_measure、affects_arrange、affects_render
2. **值继承标志**：inherits（值沿视觉树向下继承）
3. **变更回调**：changed（属性变更时的自定义回调函数）

**使用建议**：
- 正确设置 affects_* 标志（避免过度标记）
- 谨慎使用 inherits 标志（仅对需要继承的属性设置）
- 变更回调使用静态成员函数
- 回调内避免递归设置同一属性
- 使用 C++20 指定初始化语法构造
