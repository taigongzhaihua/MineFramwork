# INotifyPropertyChanged 详细接口文档

## 概述

`INotifyPropertyChanged` 是 `mine.ui.binding` 模块的**属性变更通知接口**（等价于 C# INotifyPropertyChanged）。

**核心特性：**
- **subscribe_property_changed()**：订阅属性变更事件
- **unsubscribe_property_changed()**：取消属性变更事件订阅
- **get_property()**：按属性名读取当前值（属性反射接口）
- **set_property()**：按属性名设置值（属性反射接口，用于 TwoWay 绑定反向写入）

---

## 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/INotifyPropertyChanged.h
```

---

## 接口定义

```cpp
class MINE_UI_BINDING_API INotifyPropertyChanged {
public:
    virtual ~INotifyPropertyChanged() = default;

    /**
     * @brief 属性变更回调函数类型。
     *
     * @param sender    触发通知的 INotifyPropertyChanged 实例
     * @param prop_name 发生变更的属性名（与注册时的属性名一致）
     * @param user_data 订阅时传入的用户数据指针
     */
    using ChangedFn = void (*)(INotifyPropertyChanged* sender,
                               core::StringView         prop_name,
                               void*                    user_data);

    /**
     * @brief 订阅属性变更事件。
     *
     * @param fn        属性变更时调用的回调函数
     * @param user_data 透传给回调的用户数据指针（可为 nullptr）
     * @return 订阅令牌（用于 unsubscribe_property_changed 取消订阅）
     */
    [[nodiscard]] virtual uint32_t subscribe_property_changed(
        ChangedFn fn, void* user_data) noexcept = 0;

    /**
     * @brief 取消属性变更事件订阅。
     *
     * @param token subscribe_property_changed 返回的令牌；无效令牌时为空操作
     */
    virtual void unsubscribe_property_changed(uint32_t token) noexcept = 0;

    /**
     * @brief 按属性名读取当前值（属性反射接口）。
     *
     * 默认实现返回空 Variant。
     *
     * @param name 属性名
     * @return 属性当前值的 Variant 封装；属性未注册时返回空 Variant
     */
    [[nodiscard]] virtual core::Variant get_property(
        core::StringView name) const noexcept {
        return core::Variant{};
    }

    /**
     * @brief 按属性名设置值（属性反射接口，用于 TwoWay 绑定反向写入）。
     *
     * 默认实现为空操作。
     *
     * @param name 属性名
     * @param value 新值
     * @return true 设置成功；false 属性未注册或类型不兼容
     */
    virtual bool set_property(
        core::StringView name,
        const core::Variant& value) noexcept {
        return false;
    }
};
```

**描述**：属性变更通知接口。

**特征**：
- ViewModel 层的对象实现此接口，以允许 UI 绑定系统订阅属性变更事件
- 配合 BindingExpression 的 PropertyDependency 使用
- 不依赖 RTTI 或异常
- 回调函数为原始函数指针 + void* user_data（避免 std::function）
- 通知时传递属性名，订阅方可按名过滤

---

## 成员类型

### ChangedFn

```cpp
using ChangedFn = void (*)(
    INotifyPropertyChanged* sender,
    core::StringView         prop_name,
    void*                    user_data);
```

**描述**：属性变更回调函数类型。

**参数**：
- `sender`：触发通知的 INotifyPropertyChanged 实例
- `prop_name`：发生变更的属性名（与注册时的属性名一致）
- `user_data`：订阅时传入的用户数据指针

---

## 成员方法

### subscribe_property_changed()

```cpp
[[nodiscard]] virtual uint32_t subscribe_property_changed(
    ChangedFn fn, void* user_data) noexcept = 0;
```

**描述**：订阅属性变更事件。

**参数**：
- `fn`：属性变更时调用的回调函数
- `user_data`：透传给回调的用户数据指针（可为 nullptr）

**返回值**：订阅令牌（用于 unsubscribe_property_changed 取消订阅）。

---

### unsubscribe_property_changed()

```cpp
virtual void unsubscribe_property_changed(uint32_t token) noexcept = 0;
```

**描述**：取消属性变更事件订阅。

**参数**：
- `token`：subscribe_property_changed 返回的令牌；无效令牌时为空操作

---

### get_property()

```cpp
[[nodiscard]] virtual core::Variant get_property(
    core::StringView name) const noexcept;
```

**描述**：按属性名读取当前值（属性反射接口）。

**参数**：
- `name`：属性名

**返回值**：属性当前值的 Variant 封装；属性未注册时返回空 Variant。

**特征**：
- 默认实现返回空 Variant
- 继承自 ObservableObject 的 ViewModel 子类通过 MINE_OBSERVABLE 宏自动将每个属性的 getter 注册到内部查找表
- BindingExpression::bind() 在建立绑定时调用此接口

---

### set_property()

```cpp
virtual bool set_property(
    core::StringView name,
    const core::Variant& value) noexcept;
```

**描述**：按属性名设置值（属性反射接口，用于 TwoWay 绑定反向写入）。

**参数**：
- `name`：属性名
- `value`：新值

**返回值**：true 设置成功；false 属性未注册或类型不兼容。

**特征**：
- 默认实现为空操作（返回 false）
- 继承自 ObservableObject 的 ViewModel 子类通过 MINE_OBSERVABLE 宏自动将每个属性的 setter 注册到内部查找表
- BindingExpression 在 TwoWay 模式下，当目标属性变更时自动调用此接口将新值回写到源 ViewModel 属性

---

## 使用场景

### 1. 实现 INotifyPropertyChanged 接口（手动）

```cpp
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/core/Variant.h>
#include <mine/containers/Vector.h>

using namespace mine::ui;
using namespace mine::core;

class MyViewModel : public INotifyPropertyChanged {
public:
    // ── 订阅管理 ────────────────────────────────────────────────────────
    
    uint32_t subscribe_property_changed(
        ChangedFn fn, void* user_data) noexcept override {
        
        uint32_t token = next_token_++;
        subscribers_.push_back({token, fn, user_data});
        return token;
    }
    
    void unsubscribe_property_changed(uint32_t token) noexcept override {
        auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
            [token](const Subscriber& sub) { return sub.token == token; });
        
        if (it != subscribers_.end()) {
            subscribers_.erase(it);
        }
    }
    
    // ── 属性访问 ────────────────────────────────────────────────────────
    
    Variant get_property(StringView name) const noexcept override {
        if (name == "name") return Variant{name_};
        if (name == "age") return Variant{age_};
        return Variant{};
    }
    
    bool set_property(StringView name, const Variant& value) noexcept override {
        if (name == "name" && value.has<String>()) {
            set_name(value.get<String>());
            return true;
        }
        if (name == "age" && value.has<int>()) {
            set_age(value.get<int>());
            return true;
        }
        return false;
    }
    
    // ── 业务属性 ────────────────────────────────────────────────────────
    
    const String& name() const { return name_; }
    void set_name(String n) {
        name_ = std::move(n);
        notify_property_changed("name");
    }
    
    int age() const { return age_; }
    void set_age(int a) {
        age_ = a;
        notify_property_changed("age");
    }

protected:
    void notify_property_changed(StringView prop_name) {
        for (const auto& sub : subscribers_) {
            sub.fn(this, prop_name, sub.user_data);
        }
    }

private:
    struct Subscriber {
        uint32_t token;
        ChangedFn fn;
        void* user_data;
    };
    
    String name_ = "Alice";
    int age_ = 25;
    Vector<Subscriber> subscribers_;
    uint32_t next_token_ = 1;
};
```

---

### 2. 使用 ObservableObject 基类（推荐）

```cpp
#include <mine/mvvm/ObservableObject.h>

using namespace mine::mvvm;

// ObservableObject 已实现 INotifyPropertyChanged 接口
class MyViewModel : public ObservableObject {
public:
    // 使用 MINE_OBSERVABLE 宏自动生成 getter/setter/属性反射
    MINE_OBSERVABLE(String, name, "name", "Alice");
    MINE_OBSERVABLE(int, age, "age", 25);
};

// 等价于手动实现：
// - const String& name() const;
// - void set_name(String n);
// - int age() const;
// - void set_age(int a);
// - Variant get_property(StringView name) const noexcept override;
// - bool set_property(StringView name, const Variant& value) noexcept override;
// - 自动调用 notify_property_changed("name") / notify_property_changed("age");
```

---

### 3. 订阅属性变更事件

```cpp
// 回调函数
void on_property_changed(
    INotifyPropertyChanged* sender,
    StringView prop_name,
    void* user_data) {
    
    if (prop_name == "name") {
        // 处理 name 属性变更
        auto* vm = static_cast<MyViewModel*>(sender);
        String new_name = vm->get_property("name").get<String>();
        std::cout << "Name changed to: " << new_name << std::endl;
    }
}

// 订阅
MyViewModel vm;
uint32_t token = vm.subscribe_property_changed(&on_property_changed, nullptr);

// 触发变更
vm.set_name("Bob");  // 回调被调用

// 取消订阅
vm.unsubscribe_property_changed(token);
```

---

### 4. BindingExpression 使用（自动订阅）

```cpp
// BindingExpression 内部实现
class BindingExpression {
    void attach() {
        // 订阅源对象的属性变更事件
        if (auto* inpc = dynamic_cast<INotifyPropertyChanged*>(source_)) {
            token_ = inpc->subscribe_property_changed(
                &on_source_changed, this);
        }
    }
    
    void detach() {
        // 取消订阅
        if (auto* inpc = dynamic_cast<INotifyPropertyChanged*>(source_)) {
            inpc->unsubscribe_property_changed(token_);
        }
    }
    
    static void on_source_changed(
        INotifyPropertyChanged* sender,
        StringView prop_name,
        void* user_data) {
        
        auto* self = static_cast<BindingExpression*>(user_data);
        // 属性变更时重新求值
        self->update_target();
    }
};
```

---

### 5. get_property() / set_property() 用于反射

```cpp
MyViewModel vm;

// get_property() 读取属性值
Variant name_value = vm.get_property("name");
CHECK(name_value.has<String>());
CHECK(name_value.get<String>() == "Alice");

// set_property() 写入属性值
bool success = vm.set_property("age", Variant{30});
CHECK(success == true);
CHECK(vm.age() == 30);

// 属性不存在时返回失败
bool failed = vm.set_property("unknown", Variant{42});
CHECK(failed == false);
```

---

## 工作流程

### OneWay 绑定流程

```
1. BindingExpression::attach()
   - 调用 source->get_property("name") 读取初始值
   - 订阅 source->subscribe_property_changed()

2. ViewModel 属性变更
   - vm.set_name("Bob")
   - 触发 notify_property_changed("name")
   - 回调 BindingExpression::on_source_changed()

3. BindingExpression 更新目标
   - 调用 source->get_property("name") 读取新值
   - 写入目标属性 target->set_value(TextProperty, Variant{"Bob"})
```

---

### TwoWay 绑定流程

```
正向（源 → 目标）：
1. vm.set_name("Bob")
2. 触发 notify_property_changed("name")
3. BindingExpression::on_source_changed()
4. 调用 source->get_property("name") 读取新值
5. 写入目标属性 target->set_value(TextProperty, Variant{"Bob"})

反向（目标 → 源）：
1. textBox.set_text("Charlie")
2. 触发 target 属性变更通知
3. BindingExpression::on_target_changed()
4. 调用 source->set_property("name", Variant{"Charlie"})
5. vm.set_name("Charlie") 内部触发 notify_property_changed("name")
6. 防循环保护（is_updating_ 标志）阻止再次正向更新
```

---

## 最佳实践

### 1. 使用 ObservableObject 基类

```cpp
// ✅ 推荐：继承 ObservableObject，使用 MINE_OBSERVABLE 宏
class MyViewModel : public ObservableObject {
public:
    MINE_OBSERVABLE(String, name, "name", "Alice");
    MINE_OBSERVABLE(int, age, "age", 25);
};

// ❌ 不推荐：手动实现 INotifyPropertyChanged（冗余）
class MyViewModel : public INotifyPropertyChanged {
    // 手动实现订阅管理、属性反射...
};
```

---

### 2. 属性名字符串使用字符串字面量

```cpp
// ✅ 推荐：属性名使用字符串字面量
void set_name(String n) {
    name_ = std::move(n);
    notify_property_changed("name");  // 字符串字面量
}

// ❌ 不推荐：属性名使用临时字符串
void set_name(String n) {
    name_ = std::move(n);
    String prop_name = "name";
    notify_property_changed(prop_name);  // 临时字符串（生命周期问题）
}
```

---

### 3. get_property() / set_property() 类型检查

```cpp
// ✅ 推荐：严格类型检查
Variant get_property(StringView name) const noexcept override {
    if (name == "age") {
        return Variant{age_};  // 返回正确类型
    }
    return Variant{};
}

bool set_property(StringView name, const Variant& value) noexcept override {
    if (name == "age" && value.has<int>()) {  // 类型检查
        set_age(value.get<int>());
        return true;
    }
    return false;
}

// ❌ 不推荐：不检查类型
bool set_property(StringView name, const Variant& value) noexcept override {
    if (name == "age") {
        set_age(value.get<int>());  // ❌ 可能崩溃（value 不一定是 int）
        return true;
    }
    return false;
}
```

---

### 4. 订阅时记得取消订阅

```cpp
// ✅ 推荐：析构时取消订阅
class MyObserver {
public:
    MyObserver(INotifyPropertyChanged& vm) : vm_(vm) {
        token_ = vm_.subscribe_property_changed(&on_changed, this);
    }
    
    ~MyObserver() {
        vm_.unsubscribe_property_changed(token_);
    }
    
private:
    static void on_changed(...) { /* ... */ }
    
    INotifyPropertyChanged& vm_;
    uint32_t token_;
};

// ❌ 不推荐：订阅后忘记取消订阅（内存泄漏）
void some_function(INotifyPropertyChanged& vm) {
    uint32_t token = vm.subscribe_property_changed(&on_changed, nullptr);
    // token 丢失，无法取消订阅
}
```

---

## 常见陷阱

### 1. 属性名拼写错误

```cpp
// ❌ 问题：属性名拼写错误
void set_name(String n) {
    name_ = std::move(n);
    notify_property_changed("Name");  // ❌ 首字母大写（错误）
}

Variant get_property(StringView name) const noexcept override {
    if (name == "name") {  // ✅ 小写
        return Variant{name_};
    }
    return Variant{};
}

// BindingExpression 订阅 "name"，但通知 "Name"，导致更新失败

// ✅ 解决：统一属性名拼写
void set_name(String n) {
    name_ = std::move(n);
    notify_property_changed("name");  // ✅ 小写
}
```

---

### 2. get_property() 返回错误类型

```cpp
// ❌ 问题：get_property() 返回错误类型
Variant get_property(StringView name) const noexcept override {
    if (name == "age") {
        return Variant{String{std::to_string(age_)}};  // ❌ 返回 String 而非 int
    }
    return Variant{};
}

// BindingExpression 期望 int，但得到 String，类型不匹配

// ✅ 解决：返回正确类型
Variant get_property(StringView name) const noexcept override {
    if (name == "age") {
        return Variant{age_};  // ✅ 返回 int
    }
    return Variant{};
}
```

---

### 3. 忘记调用 notify_property_changed()

```cpp
// ❌ 问题：setter 忘记调用 notify_property_changed()
void set_name(String n) {
    name_ = std::move(n);
    // 忘记调用 notify_property_changed("name");
}

// BindingExpression 不会收到通知，目标不更新

// ✅ 解决：setter 内调用 notify_property_changed()
void set_name(String n) {
    name_ = std::move(n);
    notify_property_changed("name");  // ✅ 通知订阅者
}
```

---

## 完整示例

```cpp
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/ui/binding/Binding.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/core/Variant.h>
#include <mine/containers/Vector.h>

using namespace mine::ui;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// 手动实现 INotifyPropertyChanged
// ────────────────────────────────────────────────────────────────────────────

class MyViewModel : public INotifyPropertyChanged {
public:
    // ── 订阅管理 ────────────────────────────────────────────────────────
    
    uint32_t subscribe_property_changed(
        ChangedFn fn, void* user_data) noexcept override {
        
        uint32_t token = next_token_++;
        subscribers_.push_back({token, fn, user_data});
        return token;
    }
    
    void unsubscribe_property_changed(uint32_t token) noexcept override {
        auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
            [token](const Subscriber& sub) { return sub.token == token; });
        
        if (it != subscribers_.end()) {
            subscribers_.erase(it);
        }
    }
    
    // ── 属性反射 ────────────────────────────────────────────────────────
    
    Variant get_property(StringView name) const noexcept override {
        if (name == "name") return Variant{name_};
        if (name == "age") return Variant{age_};
        return Variant{};
    }
    
    bool set_property(StringView name, const Variant& value) noexcept override {
        if (name == "name" && value.has<String>()) {
            set_name(value.get<String>());
            return true;
        }
        if (name == "age" && value.has<int>()) {
            set_age(value.get<int>());
            return true;
        }
        return false;
    }
    
    // ── 业务属性 ────────────────────────────────────────────────────────
    
    const String& name() const { return name_; }
    void set_name(String n) {
        name_ = std::move(n);
        notify_property_changed("name");
    }
    
    int age() const { return age_; }
    void set_age(int a) {
        age_ = a;
        notify_property_changed("age");
    }

protected:
    void notify_property_changed(StringView prop_name) {
        for (const auto& sub : subscribers_) {
            sub.fn(this, prop_name, sub.user_data);
        }
    }

private:
    struct Subscriber {
        uint32_t token;
        ChangedFn fn;
        void* user_data;
    };
    
    String name_ = "Alice";
    int age_ = 25;
    Vector<Subscriber> subscribers_;
    uint32_t next_token_ = 1;
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

void on_property_changed(
    INotifyPropertyChanged* sender,
    StringView prop_name,
    void* user_data) {
    
    if (prop_name == "name") {
        auto* vm = static_cast<MyViewModel*>(sender);
        String new_name = vm->get_property("name").get<String>();
        std::cout << "Name changed to: " << new_name << std::endl;
    }
}

int main() {
    MyViewModel vm;
    
    // ── 订阅属性变更 ────────────────────────────────────────────────────
    uint32_t token = vm.subscribe_property_changed(&on_property_changed, nullptr);
    
    // 触发变更
    vm.set_name("Bob");  // 输出: Name changed to: Bob
    vm.set_age(30);
    
    // 取消订阅
    vm.unsubscribe_property_changed(token);
    
    // ── 属性反射 ────────────────────────────────────────────────────────
    
    // 读取属性
    Variant name_value = vm.get_property("name");
    CHECK(name_value.get<String>() == "Bob");
    
    // 写入属性
    bool success = vm.set_property("name", Variant{String{"Charlie"}});
    CHECK(success == true);
    CHECK(vm.name() == "Charlie");
    
    // ── BindingExpression 使用 ─────────────────────────────────────────
    
    TextBox textBox;
    textBox.set_binding(TextBox::TextProperty,
        Binding::create("vm.name", BindingMode::TwoWay));
    
    // 正向：vm.set_name("David") -> textBox.text() = "David"
    // 反向：textBox.set_text("Eve") -> vm.name() = "Eve"
    
    return 0;
}
```

---

## 总结

`INotifyPropertyChanged` 是 `mine.ui.binding` 模块的属性变更通知接口，具备：

1. **subscribe_property_changed()**：订阅属性变更事件
2. **unsubscribe_property_changed()**：取消属性变更事件订阅
3. **get_property()**：按属性名读取当前值（属性反射接口）
4. **set_property()**：按属性名设置值（属性反射接口，用于 TwoWay 绑定反向写入）

**使用建议**：
- 使用 ObservableObject 基类（推荐，自动实现接口）
- 属性名字符串使用字符串字面量（避免生命周期问题）
- get_property() / set_property() 严格类型检查
- 订阅时记得取消订阅（避免内存泄漏）
- setter 内调用 notify_property_changed()（通知订阅者）
- 属性名拼写保持一致（通知名与反射名一致）
