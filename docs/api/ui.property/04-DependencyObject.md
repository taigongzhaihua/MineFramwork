# DependencyObject 详细接口文档

## 概述

`DependencyObject` 是 `mine.ui.property` 模块的**依赖属性值存储与通知基类**。

**核心特性：**
- **属性值读写**：get_value()、set_value()、clear_value()、has_value()
- **优先级覆盖**：支持多优先级值槽（Animation/Local/TemplateBind/StyleTrigger/StyleSetter/Inherited/Default）
- **变更通知**：属性值变更时自动触发通知（on_property_changed、invalidate_*、PropertyMetadata.changed、订阅者回调）
- **属性变更事件订阅**：subscribe_property_changed()、unsubscribe_property_changed()
- **PIMPL 模式**：使用 PIMPL 隐藏实现细节，保证 ABI 稳定性

---

## 文件位置

```
src/mine/ui/property/api/include/mine/ui/property/DependencyObject.h
```

---

## 类型定义

### PropertyChangedCallbackFn

```cpp
using PropertyChangedCallbackFn = void (*)(
    DependencyObject*       sender,
    const DependencyProperty& prop,
    const core::Variant&     old_value,
    const core::Variant&     new_value,
    void*                    user_data);
```

**描述**：属性变更事件订阅回调函数类型。

**参数**：
- `sender`：触发变更的 DependencyObject 实例
- `prop`：发生变更的属性描述符（可与特定 DependencyProperty 比较）
- `old_value`：变更前的生效值
- `new_value`：变更后的生效值
- `user_data`：订阅时传入的用户自定义数据指针

---

### DependencyObject

```cpp
class MINE_UI_PROPERTY_API DependencyObject {
public:
    DependencyObject();
    virtual ~DependencyObject();

    // 禁止拷贝（DependencyObject 具有身份，不可复制）
    DependencyObject(const DependencyObject&)            = delete;
    DependencyObject& operator=(const DependencyObject&) = delete;

    // 允许移动（子类需显式支持）
    DependencyObject(DependencyObject&&)            = default;
    DependencyObject& operator=(DependencyObject&&) = default;

    // 属性值读写
    [[nodiscard]] const core::Variant& get_value(const DependencyProperty& prop) const noexcept;
    [[nodiscard]] const core::Variant& get_value(const DependencyProperty& prop,
                                                  ValuePriority max_priority) const noexcept;
    void set_value(const DependencyProperty& prop,
                   core::Variant             value,
                   ValuePriority             priority = ValuePriority::Local);
    void clear_value(const DependencyProperty& prop,
                     ValuePriority             priority = ValuePriority::Local);
    [[nodiscard]] bool has_value(const DependencyProperty& prop,
                                 ValuePriority priority = ValuePriority::Local) const noexcept;

    // 属性变更事件订阅
    [[nodiscard]] uint32_t subscribe_property_changed(PropertyChangedCallbackFn callback,
                                                      void*                     user_data = nullptr);
    void unsubscribe_property_changed(uint32_t token);

protected:
    // 虚方法（子类可覆盖）
    virtual void on_property_changed(const DependencyProperty& prop,
                                     const core::Variant&      old_value,
                                     const core::Variant&      new_value);
    virtual void invalidate_measure();
    virtual void invalidate_arrange();
    virtual void invalidate_render();
};
```

**描述**：依赖属性值存储与通知基类。

**特征**：
- 子类继承此类并通过 get_value()/set_value() 访问依赖属性
- 可覆盖 on_property_changed() 及 invalidate_* 方法以响应属性变更
- 使用 PIMPL 模式隐藏实现细节，保证 ABI 稳定性
- 不可拷贝（具有身份），允许移动（子类需显式支持）

---

## 成员方法

### get_value() (基础版本)

```cpp
[[nodiscard]] const core::Variant& get_value(const DependencyProperty& prop) const noexcept;
```

**描述**：读取属性当前生效值。

**参数**：
- `prop`：要读取的属性描述符

**返回值**：当前生效的属性值（引用可能指向内部槽或属性默认值）。

**行为**：
- 返回所有有效槽中优先级最高的值
- 若无任何有效槽，则返回属性的 PropertyMetadata.default_value（即 DependencyProperty::default_value()）

---

### get_value() (带优先级上限)

```cpp
[[nodiscard]] const core::Variant& get_value(const DependencyProperty& prop,
                                              ValuePriority max_priority) const noexcept;
```

**描述**：读取属性在指定优先级上限及以下的生效值。

**参数**：
- `prop`：要读取的属性描述符
- `max_priority`：允许读取的最高优先级（含）

**返回值**：满足优先级限制的生效属性值。

**行为**：
- 返回所有 priority <= max_priority 的槽中优先级最高的值
- 若无满足条件的有效槽，则返回属性的默认值

**用途**：
- 主要供动画系统在 StyleTrigger 写入后读取动画终止值时使用
- 避免被 Local(P50) 等更高优先级的值遮盖

---

### set_value()

```cpp
void set_value(const DependencyProperty& prop,
               core::Variant             value,
               ValuePriority             priority = ValuePriority::Local);
```

**描述**：在指定优先级写入属性值。

**参数**：
- `prop`：目标属性描述符
- `value`：要写入的值
- `priority`：值优先级（默认为本地值 ValuePriority::Local）

**行为**：
- 若此优先级的槽已存在，则更新其值
- 否则创建新槽
- 若新写入导致生效值发生变更，则触发属性变更通知：
  1. 调用 on_property_changed()（虚方法，子类可覆盖）
  2. 根据 PropertyMetadata 调用 invalidate_measure/arrange/render()
  3. 调用 PropertyMetadata.changed 回调（如已设置）
  4. 回调所有通过 subscribe_property_changed() 注册的订阅者

---

### clear_value()

```cpp
void clear_value(const DependencyProperty& prop,
                 ValuePriority             priority = ValuePriority::Local);
```

**描述**：清除指定优先级的属性值槽。

**参数**：
- `prop`：目标属性描述符
- `priority`：要清除的优先级（默认为本地值 ValuePriority::Local）

**行为**：
- 若清除后生效值发生变更（退回到下一优先级的值或默认值），则触发属性变更通知
- 若该优先级无有效槽，则为空操作

---

### has_value()

```cpp
[[nodiscard]] bool has_value(const DependencyProperty& prop,
                             ValuePriority priority = ValuePriority::Local) const noexcept;
```

**描述**：检查指定优先级是否存在有效值槽。

**参数**：
- `prop`：目标属性描述符
- `priority`：要检查的优先级（默认为本地值）

**返回值**：true 表示存在该优先级的有效值槽。

---

### subscribe_property_changed()

```cpp
[[nodiscard]] uint32_t subscribe_property_changed(
    PropertyChangedCallbackFn callback,
    void*                     user_data = nullptr);
```

**描述**：订阅此对象上任意属性的变更事件。

**参数**：
- `callback`：回调函数指针（不可为 nullptr）
- `user_data`：用户自定义数据，原样传回回调

**返回值**：订阅 token（非零；零表示订阅失败）。

**行为**：
- 每当任意属性的生效值发生变更时，callback 均会被调用
- 返回的订阅 token 可传入 unsubscribe_property_changed() 取消订阅

---

### unsubscribe_property_changed()

```cpp
void unsubscribe_property_changed(uint32_t token);
```

**描述**：取消属性变更事件订阅。

**参数**：
- `token`：subscribe_property_changed() 返回的订阅 token

---

### on_property_changed() (protected)

```cpp
virtual void on_property_changed(const DependencyProperty& prop,
                                 const core::Variant&      old_value,
                                 const core::Variant&      new_value);
```

**描述**：属性生效值发生变更时的虚方法通知（在订阅者回调之前调用）。

**参数**：
- `prop`：发生变更的属性
- `old_value`：变更前的生效值
- `new_value`：变更后的生效值

**用途**：
- 子类可覆盖此方法响应具体属性变更
- 例如更新内部状态或触发重绘
- 默认实现为空

---

### invalidate_measure() (protected)

```cpp
virtual void invalidate_measure();
```

**描述**：标记布局测量失效（属性 affects_measure = true 时自动调用）。

**用途**：
- 子类（如 UIElement）应覆盖此方法通知布局系统
- 默认实现为空

---

### invalidate_arrange() (protected)

```cpp
virtual void invalidate_arrange();
```

**描述**：标记布局排列失效（属性 affects_arrange = true 时自动调用）。

**用途**：
- 子类应覆盖此方法通知布局系统
- 默认实现为空

---

### invalidate_render() (protected)

```cpp
virtual void invalidate_render();
```

**描述**：标记渲染失效（属性 affects_render = true 时自动调用）。

**用途**：
- 子类应覆盖此方法通知渲染系统
- 默认实现为空

---

## 使用场景

### 1. 读取属性值

```cpp
DependencyObject obj;
const Variant& value = obj.get_value(WidthProp);
if (value.has<double>()) {
    double width = value.get<double>();
}
```

---

### 2. 写入属性值（本地值）

```cpp
DependencyObject obj;
obj.set_value(WidthProp, Variant{100.0});  // 默认 Local 优先级
```

---

### 3. 写入属性值（指定优先级）

```cpp
DependencyObject obj;
// 样式系统写入 StyleSetter 优先级
obj.set_value(BackgroundProp, Variant{color}, ValuePriority::StyleSetter);

// 动画系统写入 Animation 优先级
obj.set_value(OpacityProp, Variant{0.5f}, ValuePriority::Animation);
```

---

### 4. 清除属性值（退回到下一优先级）

```cpp
DependencyObject obj;
obj.set_value(WidthProp, Variant{100.0}, ValuePriority::Local);
obj.set_value(WidthProp, Variant{50.0}, ValuePriority::StyleSetter);

// 清除本地值 -> 退回到 StyleSetter 值（50.0）
obj.clear_value(WidthProp, ValuePriority::Local);
CHECK(obj.get_value(WidthProp).get<double>() == 50.0);
```

---

### 5. 检查指定优先级是否有值

```cpp
DependencyObject obj;
obj.set_value(WidthProp, Variant{100.0}, ValuePriority::Local);

// 检查是否存在本地值
bool has_local = obj.has_value(WidthProp, ValuePriority::Local);
CHECK(has_local == true);

// 检查是否存在动画值
bool has_animation = obj.has_value(WidthProp, ValuePriority::Animation);
CHECK(has_animation == false);
```

---

### 6. 读取指定优先级或更低的值

```cpp
DependencyObject obj;
obj.set_value(WidthProp, Variant{10.0}, ValuePriority::StyleSetter);
obj.set_value(WidthProp, Variant{20.0}, ValuePriority::Local);
obj.set_value(WidthProp, Variant{30.0}, ValuePriority::Animation);

// 读取所有优先级中最高的值
CHECK(obj.get_value(WidthProp).get<double>() == 30.0);  // Animation

// 读取 Local 或更低优先级的值（忽略 Animation）
const Variant& val = obj.get_value(WidthProp, ValuePriority::Local);
CHECK(val.get<double>() == 20.0);  // Local（忽略 Animation）
```

---

### 7. 订阅属性变更事件

```cpp
// 回调函数
void on_property_changed(
    DependencyObject* sender,
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value,
    void* user_data) {
    
    if (prop == WidthProp) {
        // 处理 Width 变更
    }
}

// 订阅
DependencyObject obj;
uint32_t token = obj.subscribe_property_changed(&on_property_changed, nullptr);

// 触发变更
obj.set_value(WidthProp, Variant{100.0});  // 回调被调用

// 取消订阅
obj.unsubscribe_property_changed(token);
```

---

### 8. 子类覆盖 on_property_changed()

```cpp
class MyControl : public DependencyObject {
protected:
    void on_property_changed(
        const DependencyProperty& prop,
        const Variant& old_value,
        const Variant& new_value) override {
        
        // 调用基类实现
        DependencyObject::on_property_changed(prop, old_value, new_value);
        
        // 处理特定属性变更
        if (prop == WidthProp) {
            // 更新内部状态
        }
    }
};
```

---

### 9. 子类覆盖 invalidate_* 方法

```cpp
class MyElement : public DependencyObject {
protected:
    void invalidate_measure() override {
        // 通知布局系统重新测量
        if (layout_manager_) {
            layout_manager_->invalidate_measure(this);
        }
    }
    
    void invalidate_render() override {
        // 通知渲染系统重绘
        if (render_manager_) {
            render_manager_->invalidate_render(this);
        }
    }
};
```

---

## 属性变更通知流程

### 完整通知流程

当属性值变更时（set_value() 或 clear_value()），按以下顺序触发通知：

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
// ❌ 问题：on_property_changed 内部再次调用同属性的 set_value()
void MyControl::on_property_changed(
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    if (prop == WidthProp) {
        // 这将被防递归保护忽略
        set_value(WidthProp, Variant{100.0});
    }
}

// ✅ 解决：on_property_changed 内部设置其他属性
void MyControl::on_property_changed(
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    if (prop == WidthProp) {
        // 允许：设置其他属性
        set_value(HeightProp, Variant{new_value.get<double>() * 0.5});
    }
}
```

---

## 最佳实践

### 1. 使用类型化 getter/setter 封装

```cpp
// ✅ 推荐：提供类型化 getter/setter
class Button : public DependencyObject {
public:
    void set_width(double w) {
        set_value(WidthProperty, Variant{w});
    }
    
    double width() const {
        return get_value(WidthProperty).get<double>();
    }
};

// 用户代码更清晰
button.set_width(100.0);
double w = button.width();
```

---

### 2. 子类覆盖 on_property_changed() 响应变更

```cpp
// ✅ 推荐：覆盖 on_property_changed() 处理属性变更
class MyControl : public DependencyObject {
protected:
    void on_property_changed(
        const DependencyProperty& prop,
        const Variant& old_value,
        const Variant& new_value) override {
        
        DependencyObject::on_property_changed(prop, old_value, new_value);
        
        if (prop == ContentProperty) {
            on_content_changed(old_value, new_value);
        }
    }
    
private:
    void on_content_changed(const Variant& old_value, const Variant& new_value) {
        // 处理 Content 变更
    }
};
```

---

### 3. 子类覆盖 invalidate_* 方法通知系统

```cpp
// ✅ 推荐：覆盖 invalidate_* 方法通知布局/渲染系统
class UIElement : public DependencyObject {
protected:
    void invalidate_measure() override {
        if (parent_) {
            parent_->invalidate_measure();
        }
    }
    
    void invalidate_render() override {
        if (render_target_) {
            render_target_->invalidate();
        }
    }
};
```

---

### 4. 订阅属性变更事件时记得取消订阅

```cpp
// ✅ 推荐：析构时取消订阅
class MyObserver {
public:
    MyObserver(DependencyObject& obj) : obj_(obj) {
        token_ = obj_.subscribe_property_changed(&on_changed, this);
    }
    
    ~MyObserver() {
        obj_.unsubscribe_property_changed(token_);
    }
    
private:
    static void on_changed(...) { /* ... */ }
    
    DependencyObject& obj_;
    uint32_t token_;
};
```

---

## 常见陷阱

### 1. 在 on_property_changed 内部递归设置同一属性

```cpp
// ❌ 问题：on_property_changed 内部再次设置同一属性
void MyControl::on_property_changed(
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    if (prop == WidthProp) {
        // 这将被防递归保护忽略
        set_value(WidthProp, Variant{100.0});
    }
}

// ✅ 解决：设置其他属性
void MyControl::on_property_changed(
    const DependencyProperty& prop,
    const Variant& old_value,
    const Variant& new_value) {
    
    if (prop == WidthProp) {
        set_value(HeightProp, Variant{new_value.get<double>() * 0.5});
    }
}
```

---

### 2. 订阅后忘记取消订阅

```cpp
// ❌ 问题：订阅后忘记取消订阅（内存泄漏）
void some_function(DependencyObject& obj) {
    uint32_t token = obj.subscribe_property_changed(&on_changed, nullptr);
    // token 丢失，无法取消订阅
}

// ✅ 解决：保存 token 并在适当时机取消订阅
class MyObserver {
    uint32_t token_;
    DependencyObject& obj_;
public:
    MyObserver(DependencyObject& obj) : obj_(obj) {
        token_ = obj_.subscribe_property_changed(&on_changed, this);
    }
    ~MyObserver() {
        obj_.unsubscribe_property_changed(token_);
    }
};
```

---

### 3. 低优先级无法覆盖高优先级

```cpp
// ❌ 问题：先设置 Local，再设置 StyleSetter，无效
obj.set_value(WidthProp, Variant{100.0}, ValuePriority::Local);
obj.set_value(WidthProp, Variant{50.0}, ValuePriority::StyleSetter);
CHECK(obj.get_value(WidthProp).get<double>() == 100.0);  // 仍然是 Local 的值

// ✅ 解决：清除 Local 后再设置 StyleSetter
obj.clear_value(WidthProp, ValuePriority::Local);
obj.set_value(WidthProp, Variant{50.0}, ValuePriority::StyleSetter);
CHECK(obj.get_value(WidthProp).get<double>() == 50.0);
```

---

## 完整示例

```cpp
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/ui/property/PropertyMetadata.h>
#include <mine/core/Variant.h>

using namespace mine::ui;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 定义控件类
// ────────────────────────────────────────────────────────────────────────────

class Button : public DependencyObject {
public:
    // 依赖属性静态声明
    static const DependencyProperty& WidthProperty;
    static const DependencyProperty& ContentProperty;
    
    // 类型化 getter/setter
    void set_width(double w) {
        set_value(WidthProperty, Variant{w});
    }
    
    double width() const {
        return get_value(WidthProperty).get<double>();
    }
    
    void set_content(Variant v) {
        set_value(ContentProperty, std::move(v));
    }
    
    Variant content() const {
        return get_value(ContentProperty);
    }

protected:
    // 覆盖 on_property_changed() 响应属性变更
    void on_property_changed(
        const DependencyProperty& prop,
        const Variant& old_value,
        const Variant& new_value) override {
        
        DependencyObject::on_property_changed(prop, old_value, new_value);
        
        if (prop == ContentProperty) {
            on_content_changed(old_value, new_value);
        }
    }
    
    // 覆盖 invalidate_measure() 通知布局系统
    void invalidate_measure() override {
        // 通知布局系统重新测量
    }
    
private:
    void on_content_changed(const Variant& old_value, const Variant& new_value) {
        // 处理 Content 变更
        // - 移除旧内容的视觉子元素
        // - 添加新内容的视觉子元素
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 属性注册
// ────────────────────────────────────────────────────────────────────────────

const DependencyProperty& Button::WidthProperty =
    register_property<Button, double>(
        "Width",
        0.0,
        PropertyMetadata{.affects_measure = true});

const DependencyProperty& Button::ContentProperty =
    register_property<Button>(
        "Content",
        Variant{},
        PropertyMetadata{.affects_measure = true});

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    Button button;
    
    // 设置属性值（本地值）
    button.set_width(100.0);
    CHECK(button.width() == 100.0);
    
    // 设置内容
    button.set_content(Variant{String{"Click Me"}});
    CHECK(button.content().get<String>() == "Click Me");
    
    // 设置样式值（StyleSetter 优先级）
    button.set_value(Button::WidthProperty, Variant{50.0}, ValuePriority::StyleSetter);
    CHECK(button.width() == 100.0);  // 本地值（100.0）优先级更高
    
    // 清除本地值 -> 退回到样式值
    button.clear_value(Button::WidthProperty, ValuePriority::Local);
    CHECK(button.width() == 50.0);  // 样式值生效
    
    // 订阅属性变更事件
    uint32_t token = button.subscribe_property_changed(
        [](DependencyObject* sender,
           const DependencyProperty& prop,
           const Variant& old_value,
           const Variant& new_value,
           void* user_data) {
            
            if (prop == Button::WidthProperty) {
                // 处理 Width 变更
            }
        },
        nullptr);
    
    // 触发变更
    button.set_width(200.0);  // 回调被调用
    
    // 取消订阅
    button.unsubscribe_property_changed(token);
    
    return 0;
}
```

---

## 总结

`DependencyObject` 是 `mine.ui.property` 模块的依赖属性值存储与通知基类，具备：

1. **属性值读写**：get_value()、set_value()、clear_value()、has_value()
2. **优先级覆盖**：支持多优先级值槽（Animation/Local/TemplateBind/StyleTrigger/StyleSetter/Inherited/Default）
3. **变更通知**：属性值变更时自动触发通知（on_property_changed、invalidate_*、PropertyMetadata.changed、订阅者回调）
4. **属性变更事件订阅**：subscribe_property_changed()、unsubscribe_property_changed()
5. **PIMPL 模式**：使用 PIMPL 隐藏实现细节，保证 ABI 稳定性

**使用建议**：
- 使用类型化 getter/setter 封装
- 子类覆盖 on_property_changed() 响应变更
- 子类覆盖 invalidate_* 方法通知系统
- 订阅属性变更事件时记得取消订阅
- 避免在 on_property_changed 内部递归设置同一属性
- 高优先级覆盖低优先级（清除高优先级后退回到下一级）
