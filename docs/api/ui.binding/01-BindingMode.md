# BindingMode 详细接口文档

## 概述

`BindingMode` 是 `mine.ui.binding` 模块的**绑定方向枚举**。

**核心特性：**
- **OneWay**：单向绑定（源 → 目标）
- **TwoWay**：双向绑定（源 ↔ 目标）
- **OneWayToSource**：单向回写绑定（目标 → 源）
- **OneTime**：一次性绑定（仅初始化时读取一次）

---

## 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/BindingMode.h
```

---

## 枚举定义

```cpp
enum class BindingMode : uint8_t {
    /// 单向绑定（源 → 目标）
    OneWay,
    /// 双向绑定（源 ↔ 目标）
    TwoWay,
    /// 单向回写绑定（目标 → 源）
    OneWayToSource,
    /// 一次性绑定（仅初始化时读取一次）
    OneTime,
};
```

**描述**：绑定方向，决定 BindingExpression 的数据流动模式。

**特征**：
- 值优先级在 DependencyObject 中通过 ValuePriority::TemplateBind 管理
- TwoWay 的回写以 ValuePriority::Local 写入源对象

---

## 枚举值说明

### OneWay

**描述**：单向绑定（源 → 目标）。

**行为**：
- 源属性变更时自动更新目标属性
- 目标属性的变更不影响源

**典型场景**：
- ViewModel 属性绑定到 UI 控件（单向数据流）
- 只读数据展示（如标签、文本块）

---

### TwoWay

**描述**：双向绑定（源 ↔ 目标）。

**行为**：
- 源或目标任一端变更均触发对方更新
- 内置防循环保护（is_updating 标志）
- 支持 INotifyPropertyChanged 与 DependencyProperty ↔ DependencyProperty 两类来源
- 正向与反向均以 ValuePriority::Local 写入，两端对等

**典型场景**：
- 可编辑控件（如 TextBox、CheckBox）
- 用户输入需要同步回 ViewModel

---

### OneWayToSource

**描述**：单向回写绑定（目标 → 源）。

**行为**：
- 目标属性变更时自动写回源
- 源变更不影响目标
- attach 时把目标当前值初始回写一次

**典型场景**：
- UI 状态反向绑定到 ViewModel（如窗口尺寸、滚动位置）
- 用户交互结果需要反馈到数据源

---

### OneTime

**描述**：一次性绑定（仅初始化时读取一次）。

**行为**：
- attach() 时求值并写入目标
- 之后不再订阅源变更
- 比 OneWay 性能更好，适用于静态内容
- M1.1 支持（attach 后立即 detach）

**典型场景**：
- 静态内容（如应用标题、版本号）
- 初始化时加载的配置项

---

## 使用场景

### 1. OneWay 绑定（只读数据展示）

```cpp
// ViewModel
class MyViewModel : public INotifyPropertyChanged {
    String name_ = "Alice";
public:
    const String& name() const { return name_; }
    void set_name(String n) {
        name_ = std::move(n);
        notify_property_changed("name");
    }
};

// MML 或代码
// <TextBlock text: bind(vm.name) />  // 默认 OneWay
textBlock.set_binding(TextBlock::TextProperty, 
    Binding::create("vm.name", BindingMode::OneWay));
```

---

### 2. TwoWay 绑定（可编辑控件）

```cpp
// ViewModel
class LoginViewModel : public INotifyPropertyChanged {
    String username_;
public:
    const String& username() const { return username_; }
    void set_username(String u) {
        username_ = std::move(u);
        notify_property_changed("username");
    }
};

// MML 或代码
// <TextBox text: bind(vm.username, mode: TwoWay) />
textBox.set_binding(TextBox::TextProperty,
    Binding::create("vm.username", BindingMode::TwoWay));

// 用户输入 "Bob" -> vm.username() 自动更新为 "Bob"
// vm.set_username("Alice") -> textBox.text() 自动更新为 "Alice"
```

---

### 3. OneWayToSource 绑定（UI 状态反向绑定）

```cpp
// ViewModel
class WindowViewModel : public INotifyPropertyChanged {
    double window_width_ = 800.0;
public:
    double window_width() const { return window_width_; }
    void set_window_width(double w) {
        window_width_ = w;
        notify_property_changed("window_width");
    }
};

// MML 或代码
// <Window width: bind(vm.window_width, mode: OneWayToSource) />
window.set_binding(Window::WidthProperty,
    Binding::create("vm.window_width", BindingMode::OneWayToSource));

// 用户调整窗口宽度 -> vm.window_width() 自动更新
// vm.set_window_width(1024) -> window.width() 不变
```

---

### 4. OneTime 绑定（静态内容）

```cpp
// ViewModel
class AppViewModel : public INotifyPropertyChanged {
    String app_name_ = "MyApp";
public:
    const String& app_name() const { return app_name_; }
};

// MML 或代码
// <TextBlock text: bind(vm.app_name, mode: OneTime) />
textBlock.set_binding(TextBlock::TextProperty,
    Binding::create("vm.app_name", BindingMode::OneTime));

// 初始化时读取一次 "MyApp"
// vm.set_app_name("NewApp") -> textBlock.text() 不变（不再订阅）
```

---

## 数据流向对比

| 模式 | 源 → 目标 | 目标 → 源 | 初始化 | 性能 |
|------|-----------|-----------|--------|------|
| `OneWay` | ✅ 自动 | ❌ 不传播 | attach 时求值 | 中 |
| `TwoWay` | ✅ 自动 | ✅ 自动 | attach 时求值 | 低（双向订阅） |
| `OneWayToSource` | ❌ 不传播 | ✅ 自动 | attach 时回写 | 中 |
| `OneTime` | ⭕ 仅一次 | ❌ 不传播 | attach 时求值 | 高（无订阅） |

---

## 值优先级说明

### OneWay / OneTime / TwoWay（正向）

- 目标属性以 `ValuePriority::TemplateBind` (40) 写入
- 低于 `Local` (50)，用户手动设置可覆盖绑定值

### TwoWay（反向）/ OneWayToSource

- 源属性以 `ValuePriority::Local` (50) 写入
- 与用户手动设置同等优先级

---

## 防循环保护（TwoWay）

### 内置机制

```cpp
// BindingExpression 内部实现
class BindingExpression {
    bool is_updating_ = false;  // 防循环标志
    
    void on_source_changed() {
        if (is_updating_) return;  // 正在更新中，跳过
        is_updating_ = true;
        update_target();  // 源 → 目标
        is_updating_ = false;
    }
    
    void on_target_changed() {
        if (is_updating_) return;  // 正在更新中，跳过
        is_updating_ = true;
        update_source();  // 目标 → 源
        is_updating_ = false;
    }
};
```

**效果**：
- 源变更 → 目标更新 → 目标变更通知被忽略（is_updating_ = true）
- 目标变更 → 源更新 → 源变更通知被忽略（is_updating_ = true）

---

## 最佳实践

### 1. 根据场景选择合适的绑定模式

```cpp
// ✅ 推荐：只读数据使用 OneWay
// <TextBlock text: bind(vm.status) />

// ✅ 推荐：可编辑控件使用 TwoWay
// <TextBox text: bind(vm.username, mode: TwoWay) />

// ✅ 推荐：静态内容使用 OneTime
// <TextBlock text: bind(vm.app_name, mode: OneTime) />

// ✅ 推荐：UI 状态反向绑定使用 OneWayToSource
// <Window width: bind(vm.window_width, mode: OneWayToSource) />
```

---

### 2. 避免不必要的 TwoWay 绑定

```cpp
// ❌ 不推荐：只读数据使用 TwoWay（性能浪费）
// <TextBlock text: bind(vm.status, mode: TwoWay) />

// ✅ 推荐：只读数据使用 OneWay
// <TextBlock text: bind(vm.status) />
```

---

### 3. 静态内容优先使用 OneTime

```cpp
// ❌ 不推荐：静态内容使用 OneWay（持续订阅）
// <TextBlock text: bind(vm.app_name) />

// ✅ 推荐：静态内容使用 OneTime（性能更好）
// <TextBlock text: bind(vm.app_name, mode: OneTime) />
```

---

## 常见陷阱

### 1. TwoWay 绑定到只读属性

```cpp
// ❌ 问题：只读属性使用 TwoWay（回写失败）
class MyViewModel : public INotifyPropertyChanged {
    String status_ = "Ready";
public:
    const String& status() const { return status_; }
    // 没有 set_status()
};

// <TextBox text: bind(vm.status, mode: TwoWay) />
// 用户输入时尝试回写，但找不到 setter，回写失败

// ✅ 解决：只读属性使用 OneWay
// <TextBlock text: bind(vm.status) />
```

---

### 2. OneWayToSource 绑定源未实现 setter

```cpp
// ❌ 问题：OneWayToSource 源未实现 setter
class MyViewModel : public INotifyPropertyChanged {
    double width_ = 800.0;
public:
    double width() const { return width_; }
    // 没有 set_width()
};

// <Window width: bind(vm.width, mode: OneWayToSource) />
// 目标变更时尝试回写，但找不到 setter，回写失败

// ✅ 解决：实现 setter
void set_width(double w) {
    width_ = w;
    notify_property_changed("width");
}
```

---

### 3. OneTime 绑定后期望更新

```cpp
// ❌ 问题：OneTime 绑定后源变更，目标不更新
// <TextBlock text: bind(vm.name, mode: OneTime) />
vm.set_name("Alice");  // 初始化时读取
// ...
vm.set_name("Bob");  // 目标不更新（OneTime 不再订阅）

// ✅ 解决：需要持续更新时使用 OneWay
// <TextBlock text: bind(vm.name) />
```

---

## 完整示例

```cpp
#include <mine/ui/binding/BindingMode.h>
#include <mine/ui/binding/Binding.h>
#include <mine/ui/binding/INotifyPropertyChanged.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/controls/TextBlock.h>

using namespace mine::ui;

// ────────────────────────────────────────────────────────────────────────────
// ViewModel 定义
// ────────────────────────────────────────────────────────────────────────────

class UserViewModel : public INotifyPropertyChanged {
    String name_ = "Alice";
    int age_ = 25;
    String app_title_ = "MyApp v1.0";

public:
    // OneWay / TwoWay 绑定的属性（需要 getter + setter）
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
    
    // OneTime 绑定的属性（只需 getter）
    const String& app_title() const { return app_title_; }
};

// ────────────────────────────────────────────────────────────────────────────
// UI 绑定示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    UserViewModel vm;
    
    // ── OneWay 绑定（只读数据展示） ──────────────────────────────────────
    TextBlock nameLabel;
    nameLabel.set_binding(TextBlock::TextProperty,
        Binding::create("vm.name", BindingMode::OneWay));
    
    // 源变更 → 目标更新
    vm.set_name("Bob");
    CHECK(nameLabel.text() == "Bob");
    
    // ── TwoWay 绑定（可编辑控件） ────────────────────────────────────────
    TextBox nameInput;
    nameInput.set_binding(TextBox::TextProperty,
        Binding::create("vm.name", BindingMode::TwoWay));
    
    // 源变更 → 目标更新
    vm.set_name("Charlie");
    CHECK(nameInput.text() == "Charlie");
    
    // 目标变更 → 源更新
    nameInput.set_text("David");
    CHECK(vm.name() == "David");
    
    // ── OneWayToSource 绑定（UI 状态反向绑定） ───────────────────────────
    TextBox ageInput;
    ageInput.set_binding(TextBox::TextProperty,
        Binding::create("vm.age", BindingMode::OneWayToSource));
    
    // 目标变更 → 源更新
    ageInput.set_text("30");
    CHECK(vm.age() == 30);
    
    // 源变更 → 目标不更新
    vm.set_age(35);
    CHECK(ageInput.text() == "30");  // 目标不变
    
    // ── OneTime 绑定（静态内容） ─────────────────────────────────────────
    TextBlock titleLabel;
    titleLabel.set_binding(TextBlock::TextProperty,
        Binding::create("vm.app_title", BindingMode::OneTime));
    
    CHECK(titleLabel.text() == "MyApp v1.0");  // 初始化时读取
    
    // 源变更 → 目标不更新（不再订阅）
    // vm.set_app_title("MyApp v2.0");  // 只读属性，无 setter
    
    return 0;
}
```

---

## 总结

`BindingMode` 是 `mine.ui.binding` 模块的绑定方向枚举，具备：

1. **OneWay**：单向绑定（源 → 目标），适用于只读数据展示
2. **TwoWay**：双向绑定（源 ↔ 目标），适用于可编辑控件
3. **OneWayToSource**：单向回写绑定（目标 → 源），适用于 UI 状态反向绑定
4. **OneTime**：一次性绑定（仅初始化时读取一次），适用于静态内容

**使用建议**：
- 根据场景选择合适的绑定模式
- 避免不必要的 TwoWay 绑定（性能损失）
- 静态内容优先使用 OneTime（性能最优）
- TwoWay 绑定需确保源和目标都有 getter + setter
- OneWayToSource 绑定需确保源有 setter
- TwoWay 内置防循环保护（is_updating 标志）
