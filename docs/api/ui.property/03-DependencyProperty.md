# DependencyProperty 详细接口文档

## 概述

`DependencyProperty` 是 `mine.ui.property` 模块的**依赖属性描述符类**。

**核心特性：**
- **全局唯一**：每个属性在程序启动时通过 register_property() 注册，返回全局唯一实例
- **静态注册**：属性描述符在静态初始化阶段注册，地址稳定
- **身份比较**：使用地址比较（operator==），不可拷贝/移动
- **属性查询**：name()、owner_type()、value_type()、default_value()、metadata()、is_attached()
- **注册函数**：register_property() 注册普通属性，register_attached_property() 注册附加属性

---

## 文件位置

```
src/mine/ui/property/api/include/mine/ui/property/DependencyProperty.h
```

---

## 类型定义

### DependencyProperty

```cpp
class MINE_UI_PROPERTY_API DependencyProperty {
public:
    // 禁止拷贝与移动（全局唯一实例，身份即地址）
    DependencyProperty(const DependencyProperty&)            = delete;
    DependencyProperty& operator=(const DependencyProperty&) = delete;
    DependencyProperty(DependencyProperty&&)                 = delete;
    DependencyProperty& operator=(DependencyProperty&&)      = delete;

    // 属性查询
    [[nodiscard]] core::StringView        name()          const noexcept;
    [[nodiscard]] core::TypeId            owner_type()    const noexcept;
    [[nodiscard]] core::TypeId            value_type()    const noexcept;
    [[nodiscard]] const core::Variant&    default_value() const noexcept;
    [[nodiscard]] const PropertyMetadata& metadata()      const noexcept;
    [[nodiscard]] bool                    is_attached()   const noexcept;

    // 身份比较（地址比较）
    [[nodiscard]] bool operator==(const DependencyProperty& rhs) const noexcept;
    [[nodiscard]] bool operator!=(const DependencyProperty& rhs) const noexcept;
};
```

**描述**：依赖属性描述符（全局唯一，不可拷贝/移动）。

**特征**：
- 外部代码只通过 `const DependencyProperty&` 引用使用
- 实例由 register_property / register_attached_property 创建并由内部注册表管理生命周期
- 身份比较使用地址（operator==）

---

## 成员方法

### name()

```cpp
[[nodiscard]] core::StringView name() const noexcept;
```

**描述**：属性名称（对应注册时传入的 name 参数，通常为字符串字面量）。

**返回值**：属性名称的 StringView。

---

### owner_type()

```cpp
[[nodiscard]] core::TypeId owner_type() const noexcept;
```

**描述**：注册此属性的所有者类型（对于附加属性，为定义者类型如 Grid）。

**返回值**：所有者类型的 TypeId。

---

### value_type()

```cpp
[[nodiscard]] core::TypeId value_type() const noexcept;
```

**描述**：属性值的预期类型（用于类型检查和调试）。

**返回值**：属性值类型的 TypeId。

---

### default_value()

```cpp
[[nodiscard]] const core::Variant& default_value() const noexcept;
```

**描述**：属性默认值（当 DependencyObject 中无任何有效值槽时返回此值）。

**返回值**：属性默认值的 Variant 引用。

---

### metadata()

```cpp
[[nodiscard]] const PropertyMetadata& metadata() const noexcept;
```

**描述**：属性元数据（影响布局/渲染失效和继承行为）。

**返回值**：属性元数据的 PropertyMetadata 引用。

---

### is_attached()

```cpp
[[nodiscard]] bool is_attached() const noexcept;
```

**描述**：是否为附加属性（如 Grid::ColumnProperty、Canvas::LeftProperty）。

**返回值**：true 表示附加属性，false 表示普通属性。

---

### operator==() / operator!=()

```cpp
[[nodiscard]] bool operator==(const DependencyProperty& rhs) const noexcept;
[[nodiscard]] bool operator!=(const DependencyProperty& rhs) const noexcept;
```

**描述**：身份比较（地址比较）。

**返回值**：地址相等时返回 true。

---

## 注册函数

### register_property() (完整版本)

```cpp
MINE_UI_PROPERTY_API
const DependencyProperty& register_property(
    core::StringView   name,
    core::TypeId       owner_type,
    core::TypeId       value_type,
    core::Variant      default_value,
    PropertyMetadata   metadata = {});
```

**描述**：注册一个普通依赖属性（非附加属性）。

**参数**：
- `name`：属性名（应为字符串字面量，生命周期贯穿程序运行期）
- `owner_type`：所有者类型（通常为 TypeId::of<MyClass>()）
- `value_type`：属性值预期类型
- `default_value`：属性默认值
- `metadata`：属性元数据

**返回值**：对新注册属性描述符的常量引用（全局唯一，地址稳定）。

**注意**：
- 线程不安全，应在静态初始化阶段（程序启动时）调用
- 对同一 (owner_type, name) 重复注册将触发断言终止

---

### register_attached_property() (完整版本)

```cpp
MINE_UI_PROPERTY_API
const DependencyProperty& register_attached_property(
    core::StringView  name,
    core::TypeId      owner_type,
    core::TypeId      value_type,
    core::Variant     default_value,
    PropertyMetadata  metadata = {});
```

**描述**：注册一个附加属性（如 Grid::ColumnProperty）。

**参数**：同 register_property()。

**返回值**：对新注册属性描述符的常量引用（全局唯一，地址稳定）。

**特征**：附加属性可被任意 DependencyObject 持有，不限于 owner_type 的实例。

---

### register_property<TOwner>() (模板便捷版本1)

```cpp
template<typename TOwner>
const DependencyProperty& register_property(
    core::StringView  name,
    core::Variant     default_value = {},
    PropertyMetadata  metadata      = {});
```

**描述**：模板便捷版本：自动推导 owner_type。

**模板参数**：
- `TOwner`：属性所属类型（register_property<Button>("Content", ...)）

**参数**：
- `name`：属性名
- `default_value`：默认值
- `metadata`：元数据

**返回值**：对新注册属性描述符的常量引用。

---

### register_property<TOwner, TValue>() (模板便捷版本2)

```cpp
template<typename TOwner, typename TValue>
const DependencyProperty& register_property(
    core::StringView  name,
    TValue            default_value = {},
    PropertyMetadata  metadata      = {});
```

**描述**：模板便捷版本：自动推导 owner_type 和 value_type。

**模板参数**：
- `TOwner`：属性所属类型
- `TValue`：属性值类型（用于生成类型化 setter 时参考）

**参数**：
- `name`：属性名
- `default_value`：默认值
- `metadata`：元数据

**返回值**：对新注册属性描述符的常量引用。

---

## 使用场景

### 1. 注册普通依赖属性（完整版本）

```cpp
// Button.h
class Button : public Control {
public:
    static const mine::ui::DependencyProperty& ContentProperty;
    void set_content(mine::core::Variant v);
    mine::core::Variant content() const;
};

// Button.cpp
static void s_on_content_changed(
    mine::ui::DependencyObject* obj,
    const mine::ui::DependencyProperty& prop,
    const mine::core::Variant& old_v,
    const mine::core::Variant& new_v) {
    
    static_cast<Button*>(obj)->on_content_changed(old_v, new_v);
}

const DependencyProperty& Button::ContentProperty =
    mine::ui::register_property(
        "Content",
        mine::core::TypeId::of<Button>(),
        mine::core::TypeId::of<mine::core::Variant>(),
        mine::core::Variant{},
        mine::ui::PropertyMetadata{
            .affects_measure = true,
            .changed = s_on_content_changed});
```

---

### 2. 注册普通依赖属性（模板便捷版本）

```cpp
// Button.cpp
const DependencyProperty& Button::ContentProperty =
    mine::ui::register_property<Button>(
        "Content",
        mine::core::Variant{},
        mine::ui::PropertyMetadata{.affects_measure = true});
```

---

### 3. 注册带类型检查的依赖属性（模板便捷版本）

```cpp
// Button.cpp
const DependencyProperty& Button::WidthProperty =
    mine::ui::register_property<Button, double>(
        "Width",
        0.0,
        mine::ui::PropertyMetadata{.affects_measure = true});
```

---

### 4. 注册附加属性（完整版本）

```cpp
// Grid.h
class Grid : public Panel {
public:
    static const mine::ui::DependencyProperty& ColumnProperty;
    static void set_column(DependencyObject& obj, int column);
    static int get_column(const DependencyObject& obj);
};

// Grid.cpp
const DependencyProperty& Grid::ColumnProperty =
    mine::ui::register_attached_property(
        "Column",
        mine::core::TypeId::of<Grid>(),
        mine::core::TypeId::of<int>(),
        mine::core::Variant{0},
        mine::ui::PropertyMetadata{.affects_arrange = true});

void Grid::set_column(DependencyObject& obj, int column) {
    obj.set_value(ColumnProperty, Variant{column});
}

int Grid::get_column(const DependencyObject& obj) {
    return obj.get_value(ColumnProperty).get<int>();
}
```

---

### 5. 属性身份比较

```cpp
// 比较属性身份（地址比较）
const DependencyProperty& prop1 = Button::ContentProperty;
const DependencyProperty& prop2 = Button::WidthProperty;

CHECK(prop1 == Button::ContentProperty);  // true（同一对象）
CHECK(prop1 != prop2);  // true（不同对象）
```

---

### 6. 属性查询

```cpp
const DependencyProperty& prop = Button::ContentProperty;

// 查询属性名称
CHECK(prop.name() == "Content");

// 查询所有者类型
CHECK(prop.owner_type() == TypeId::of<Button>());

// 查询属性值类型
CHECK(prop.value_type() == TypeId::of<Variant>());

// 查询默认值
CHECK(prop.default_value().is_empty());

// 查询元数据
CHECK(prop.metadata().affects_measure == true);

// 查询是否为附加属性
CHECK(prop.is_attached() == false);
```

---

## 属性注册最佳实践

### 1. 使用模板便捷版本

```cpp
// ✅ 推荐：使用模板便捷版本（自动推导类型）
const DependencyProperty& Button::WidthProperty =
    register_property<Button, double>(
        "Width",
        0.0,
        PropertyMetadata{.affects_measure = true});

// ❌ 不推荐：使用完整版本（冗余）
const DependencyProperty& Button::WidthProperty =
    register_property(
        "Width",
        TypeId::of<Button>(),
        TypeId::of<double>(),
        Variant{0.0},
        PropertyMetadata{.affects_measure = true});
```

---

### 2. 属性名使用字符串字面量

```cpp
// ✅ 推荐：使用字符串字面量
const DependencyProperty& Button::ContentProperty =
    register_property<Button>("Content", Variant{}, ...);

// ❌ 不推荐：使用临时字符串（生命周期问题）
String name = "Content";
const DependencyProperty& Button::ContentProperty =
    register_property<Button>(name, Variant{}, ...);  // 危险！
```

---

### 3. 属性注册在静态初始化阶段

```cpp
// ✅ 推荐：属性注册在 .cpp 文件的全局作用域
// Button.cpp
const DependencyProperty& Button::ContentProperty =
    register_property<Button>("Content", Variant{}, ...);

// ❌ 不推荐：属性注册在运行时
void Button::init() {
    static const DependencyProperty& ContentProperty =
        register_property<Button>("Content", Variant{}, ...);  // 可能有问题
}
```

---

### 4. 避免重复注册

```cpp
// ❌ 问题：对同一 (owner_type, name) 重复注册
const DependencyProperty& prop1 =
    register_property<Button>("Content", Variant{}, ...);
const DependencyProperty& prop2 =
    register_property<Button>("Content", Variant{}, ...);  // 断言终止
```

---

## 常见陷阱

### 1. 属性名生命周期问题

```cpp
// ❌ 问题：属性名使用临时字符串
String name = "Content";
const DependencyProperty& Button::ContentProperty =
    register_property<Button>(name, Variant{}, ...);  // 危险！

// ✅ 解决：使用字符串字面量
const DependencyProperty& Button::ContentProperty =
    register_property<Button>("Content", Variant{}, ...);
```

---

### 2. 重复注册同一属性

```cpp
// ❌ 问题：对同一 (owner_type, name) 重复注册
const DependencyProperty& prop1 =
    register_property<Button>("Content", Variant{}, ...);
const DependencyProperty& prop2 =
    register_property<Button>("Content", Variant{}, ...);  // 断言终止

// ✅ 解决：每个属性只注册一次
```

---

### 3. 使用值比较而非地址比较

```cpp
// ❌ 问题：属性比较使用 name() 而非地址
bool is_same = (prop1.name() == prop2.name());  // 可能相等但不是同一属性

// ✅ 解决：使用地址比较（operator==）
bool is_same = (prop1 == prop2);
```

---

### 4. 尝试拷贝或移动属性描述符

```cpp
// ❌ 问题：尝试拷贝属性描述符
const DependencyProperty& prop = Button::ContentProperty;
DependencyProperty copy = prop;  // 编译错误（已删除拷贝构造）

// ✅ 解决：使用常量引用
const DependencyProperty& prop = Button::ContentProperty;
```

---

## 完整示例

```cpp
// Button.h
#pragma once
#include <mine/ui/controls/Control.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/core/Variant.h>

namespace mine::ui {

class Button : public Control {
public:
    // 依赖属性静态声明
    static const DependencyProperty& ContentProperty;
    static const DependencyProperty& WidthProperty;
    
    // 类型化 getter/setter
    void set_content(core::Variant v);
    core::Variant content() const;
    
    void set_width(double w);
    double width() const;

protected:
    // 属性变更处理
    void on_content_changed(
        const core::Variant& old_value,
        const core::Variant& new_value);
};

} // namespace mine::ui
```

```cpp
// Button.cpp
#include "Button.h"
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/core/TypeId.h>

using namespace mine::ui;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 属性变更回调（静态函数）
// ────────────────────────────────────────────────────────────────────────────

static void s_on_content_changed(
    DependencyObject* obj,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    auto* button = static_cast<Button*>(obj);
    button->on_content_changed(old_value, new_value);
}

// ────────────────────────────────────────────────────────────────────────────
// 属性注册（静态初始化阶段）
// ────────────────────────────────────────────────────────────────────────────

const DependencyProperty& Button::ContentProperty =
    register_property<Button>(
        "Content",
        Variant{},
        PropertyMetadata{
            .affects_measure = true,
            .changed = s_on_content_changed});

const DependencyProperty& Button::WidthProperty =
    register_property<Button, double>(
        "Width",
        0.0,
        PropertyMetadata{.affects_measure = true});

// ────────────────────────────────────────────────────────────────────────────
// 类型化 getter/setter
// ────────────────────────────────────────────────────────────────────────────

void Button::set_content(Variant v) {
    set_value(ContentProperty, std::move(v));
}

Variant Button::content() const {
    return get_value(ContentProperty);
}

void Button::set_width(double w) {
    set_value(WidthProperty, Variant{w});
}

double Button::width() const {
    return get_value(WidthProperty).get<double>();
}

// ────────────────────────────────────────────────────────────────────────────
// 属性变更处理
// ────────────────────────────────────────────────────────────────────────────

void Button::on_content_changed(
    const Variant& old_value,
    const Variant& new_value) {
    
    // 处理 Content 变更
    // - 更新内容元素
    // - 触发布局测量
}
```

---

## 总结

`DependencyProperty` 是 `mine.ui.property` 模块的依赖属性描述符类，具备：

1. **全局唯一**：每个属性在程序启动时注册，返回全局唯一实例
2. **静态注册**：属性描述符在静态初始化阶段注册，地址稳定
3. **身份比较**：使用地址比较（operator==），不可拷贝/移动
4. **属性查询**：name()、owner_type()、value_type()、default_value()、metadata()、is_attached()
5. **注册函数**：register_property() 注册普通属性，register_attached_property() 注册附加属性

**使用建议**：
- 使用模板便捷版本（自动推导类型）
- 属性名使用字符串字面量
- 属性注册在静态初始化阶段
- 避免重复注册同一属性
- 使用地址比较（operator==）而非名称比较
