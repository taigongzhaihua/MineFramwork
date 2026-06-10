# PropertyDependency 详细接口文档

## 概述

`PropertyDependency` 是 `mine.ui.binding` 模块的**属性依赖项描述符**。

**核心特性：**
- **DependencyProperty 来源**：from_dep()（UI 元素间绑定）
- **INotifyPropertyChanged 来源**：from_inpc()（ViewModel → UI 绑定）
- **轻量值类型**：仅持有指针，不拥有所指对象

---

## 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/PropertyDependency.h
```

---

## 结构体定义

```cpp
struct PropertyDependency {
    /// 来源类型标识
    enum class Kind : uint8_t {
        DependencyProperty, ///< 来自 DependencyObject 的 DependencyProperty
        Inpc,               ///< 来自 INotifyPropertyChanged 的命名属性
    };

    Kind kind = Kind::DependencyProperty;

    // ── DependencyProperty 来源字段（kind == DependencyProperty 时有效）──
    DependencyObject*         dep_obj  = nullptr;
    const DependencyProperty* dep_prop = nullptr;

    // ── INotifyPropertyChanged 来源字段（kind == Inpc 时有效）────────────
    INotifyPropertyChanged* inpc_src  = nullptr;
    core::StringView        inpc_name;

    // ── 便捷工厂函数 ─────────────────────────────────────────────────────
    [[nodiscard]] static PropertyDependency from_dep(
        DependencyObject&        obj,
        const DependencyProperty& prop) noexcept;

    [[nodiscard]] static PropertyDependency from_inpc(
        INotifyPropertyChanged& inpc,
        core::StringView        name) noexcept;
};
```

**描述**：属性依赖项描述符，标识 BindingExpression 所依赖的源属性。

**特征**：
- 轻量值类型，仅持有指针，不拥有所指对象
- 调用方须确保源对象的生命周期覆盖 BindingExpression 的生命周期

---

## 枚举类型

### Kind

```cpp
enum class Kind : uint8_t {
    DependencyProperty, ///< 来自 DependencyObject 的 DependencyProperty
    Inpc,               ///< 来自 INotifyPropertyChanged 的命名属性
};
```

**描述**：来源类型标识。

**枚举值**：
- `DependencyProperty`：来自 DependencyObject 的 DependencyProperty（UI 元素间绑定）
- `Inpc`：来自 INotifyPropertyChanged 的命名属性（ViewModel → UI 绑定）

---

## 成员字段

### kind

```cpp
Kind kind = Kind::DependencyProperty;
```

**描述**：来源类型标识。

---

### dep_obj

```cpp
DependencyObject* dep_obj = nullptr;
```

**描述**：源 DependencyObject（kind == DependencyProperty 时有效）。

**特征**：
- 不拥有，须由调用方保证生命周期

---

### dep_prop

```cpp
const DependencyProperty* dep_prop = nullptr;
```

**描述**：源 DependencyProperty 描述符（kind == DependencyProperty 时有效）。

**特征**：
- 不拥有，全局静态生命周期

---

### inpc_src

```cpp
INotifyPropertyChanged* inpc_src = nullptr;
```

**描述**：源 INotifyPropertyChanged 对象（kind == Inpc 时有效）。

**特征**：
- 不拥有，须由调用方保证生命周期

---

### inpc_name

```cpp
core::StringView inpc_name;
```

**描述**：需要监听的属性名（kind == Inpc 时有效）。

**特征**：
- 必须指向稳定字符串，如字符串字面量

---

## 工厂函数

### from_dep()

```cpp
[[nodiscard]] static PropertyDependency from_dep(
    DependencyObject&        obj,
    const DependencyProperty& prop) noexcept;
```

**描述**：从 DependencyObject + DependencyProperty 构造依赖项。

**参数**：
- `obj`：源对象（生命周期须覆盖绑定）
- `prop`：源属性描述符（通常为静态全局变量）

**返回值**：PropertyDependency 实例（kind = DependencyProperty）。

---

### from_inpc()

```cpp
[[nodiscard]] static PropertyDependency from_inpc(
    INotifyPropertyChanged& inpc,
    core::StringView        name) noexcept;
```

**描述**：从 INotifyPropertyChanged 对象 + 属性名构造依赖项。

**参数**：
- `inpc`：ViewModel 对象（生命周期须覆盖绑定）
- `name`：属性名字符串（须为稳定的字符串字面量或长期存活的字符串）

**返回值**：PropertyDependency 实例（kind = Inpc）。

---

## 使用场景

### 1. DependencyProperty 来源（UI 元素间绑定）

```cpp
#include <mine/ui/binding/PropertyDependency.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>

using namespace mine::ui;

// 源对象：Slider
Slider slider;
const DependencyProperty& ValueProp = Slider::ValueProperty;

// 创建依赖项
PropertyDependency dep = PropertyDependency::from_dep(slider, ValueProp);

// BindingExpression 使用此依赖项订阅 slider.ValueProperty 的变更
```

---

### 2. INotifyPropertyChanged 来源（ViewModel → UI 绑定）

```cpp
#include <mine/ui/binding/PropertyDependency.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>

using namespace mine::ui;

// 源对象：ViewModel
class MyViewModel : public INotifyPropertyChanged {
    String name_ = "Alice";
public:
    const String& name() const { return name_; }
    void set_name(String n) {
        name_ = std::move(n);
        notify_property_changed("name");
    }
    // ... 实现 INotifyPropertyChanged 接口
};

MyViewModel vm;

// 创建依赖项
PropertyDependency dep = PropertyDependency::from_inpc(vm, "name");

// BindingExpression 使用此依赖项订阅 vm.name 的变更
```

---

### 3. BindingExpression 使用 PropertyDependency

```cpp
// BindingExpression 内部实现
class BindingExpression {
    PropertyDependency source_dep_;
    uint32_t subscription_token_ = 0;
    
    void attach() {
        if (source_dep_.kind == PropertyDependency::Kind::DependencyProperty) {
            // 订阅 DependencyObject 属性变更
            subscription_token_ = source_dep_.dep_obj->subscribe_property_changed(
                &on_dep_changed, this);
        } else if (source_dep_.kind == PropertyDependency::Kind::Inpc) {
            // 订阅 INotifyPropertyChanged 属性变更
            subscription_token_ = source_dep_.inpc_src->subscribe_property_changed(
                &on_inpc_changed, this);
        }
    }
    
    void detach() {
        if (source_dep_.kind == PropertyDependency::Kind::DependencyProperty) {
            source_dep_.dep_obj->unsubscribe_property_changed(subscription_token_);
        } else if (source_dep_.kind == PropertyDependency::Kind::Inpc) {
            source_dep_.inpc_src->unsubscribe_property_changed(subscription_token_);
        }
    }
    
    static void on_dep_changed(...) {
        // DependencyObject 属性变更 → 重新求值 → 更新目标
    }
    
    static void on_inpc_changed(...) {
        // INotifyPropertyChanged 属性变更 → 重新求值 → 更新目标
    }
};
```

---

## 最佳实践

### 1. 属性名使用字符串字面量

```cpp
// ✅ 推荐：属性名使用字符串字面量
PropertyDependency dep = PropertyDependency::from_inpc(vm, "name");

// ❌ 不推荐：属性名使用临时字符串
String prop_name = "name";
PropertyDependency dep = PropertyDependency::from_inpc(vm, prop_name);  // 危险！
```

---

### 2. 确保源对象生命周期覆盖绑定

```cpp
// ✅ 推荐：源对象生命周期覆盖绑定
class MyView {
    MyViewModel vm_;  // ViewModel 对象
    TextBox textBox_;  // UI 控件
    
    void setup() {
        // vm_ 的生命周期覆盖 textBox_ 的绑定
        PropertyDependency dep = PropertyDependency::from_inpc(vm_, "name");
        // 使用 dep 创建绑定...
    }
};

// ❌ 不推荐：源对象生命周期短于绑定
void bad_setup() {
    MyViewModel vm;  // 临时对象
    PropertyDependency dep = PropertyDependency::from_inpc(vm, "name");
    // 函数返回后 vm 被销毁，dep 持有悬空指针
}
```

---

### 3. 根据来源类型选择工厂函数

```cpp
// ✅ 推荐：UI 元素间绑定使用 from_dep()
PropertyDependency dep1 = PropertyDependency::from_dep(slider, Slider::ValueProperty);

// ✅ 推荐：ViewModel → UI 绑定使用 from_inpc()
PropertyDependency dep2 = PropertyDependency::from_inpc(vm, "name");

// ❌ 不推荐：混淆来源类型
PropertyDependency dep3;
dep3.kind = PropertyDependency::Kind::Inpc;
dep3.dep_obj = &slider;  // ❌ 错误：kind 为 Inpc，但设置 dep_obj
```

---

## 常见陷阱

### 1. 属性名使用临时字符串

```cpp
// ❌ 问题：属性名使用临时字符串
String prop_name = "name";
PropertyDependency dep = PropertyDependency::from_inpc(vm, prop_name);
// prop_name 被销毁后，dep.inpc_name 成为悬空引用

// ✅ 解决：使用字符串字面量
PropertyDependency dep = PropertyDependency::from_inpc(vm, "name");
```

---

### 2. 源对象生命周期短于绑定

```cpp
// ❌ 问题：源对象生命周期短于绑定
void bad_setup() {
    MyViewModel vm;  // 临时对象
    PropertyDependency dep = PropertyDependency::from_inpc(vm, "name");
    // 创建绑定...
}  // vm 被销毁，dep 持有悬空指针

// ✅ 解决：确保源对象生命周期覆盖绑定
class MyView {
    MyViewModel vm_;  // 成员对象
    void setup() {
        PropertyDependency dep = PropertyDependency::from_inpc(vm_, "name");
        // 创建绑定...
    }
};
```

---

### 3. 混淆来源类型

```cpp
// ❌ 问题：kind 为 Inpc，但设置 dep_obj
PropertyDependency dep;
dep.kind = PropertyDependency::Kind::Inpc;
dep.dep_obj = &slider;  // ❌ 错误字段
dep.dep_prop = &Slider::ValueProperty;  // ❌ 错误字段

// ✅ 解决：使用工厂函数
PropertyDependency dep = PropertyDependency::from_dep(slider, Slider::ValueProperty);
// 或
PropertyDependency dep = PropertyDependency::from_inpc(vm, "name");
```

---

## 完整示例

```cpp
#include <mine/ui/binding/PropertyDependency.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// ViewModel 定义
// ────────────────────────────────────────────────────────────────────────────

class MyViewModel : public INotifyPropertyChanged {
    String name_ = "Alice";
public:
    const String& name() const { return name_; }
    void set_name(String n) {
        name_ = std::move(n);
        notify_property_changed("name");
    }
    // ... 实现 INotifyPropertyChanged 接口
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    MyViewModel vm;
    Slider slider;
    
    // ── DependencyProperty 来源（UI 元素间绑定） ───────────────────────
    PropertyDependency dep1 = PropertyDependency::from_dep(
        slider, Slider::ValueProperty);
    
    CHECK(dep1.kind == PropertyDependency::Kind::DependencyProperty);
    CHECK(dep1.dep_obj == &slider);
    CHECK(dep1.dep_prop == &Slider::ValueProperty);
    
    // ── INotifyPropertyChanged 来源（ViewModel → UI 绑定） ─────────────
    PropertyDependency dep2 = PropertyDependency::from_inpc(vm, "name");
    
    CHECK(dep2.kind == PropertyDependency::Kind::Inpc);
    CHECK(dep2.inpc_src == &vm);
    CHECK(dep2.inpc_name == "name");
    
    return 0;
}
```

---

## 总结

`PropertyDependency` 是 `mine.ui.binding` 模块的属性依赖项描述符，具备：

1. **DependencyProperty 来源**：from_dep()（UI 元素间绑定）
2. **INotifyPropertyChanged 来源**：from_inpc()（ViewModel → UI 绑定）
3. **轻量值类型**：仅持有指针，不拥有所指对象

**使用建议**：
- 属性名使用字符串字面量（避免生命周期问题）
- 确保源对象生命周期覆盖绑定（避免悬空指针）
- 根据来源类型选择工厂函数（from_dep / from_inpc）
- 使用工厂函数而非手动构造（避免字段混淆）
