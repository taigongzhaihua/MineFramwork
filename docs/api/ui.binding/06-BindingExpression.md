# BindingExpression 详细接口文档

## 概述

`BindingExpression` 是 `mine.ui.binding` 模块的**运行时数据绑定表达式**，是绑定系统的核心运行时对象。

**核心特性：**
- **getter/setter**：源值求值函数 / 值回写函数
- **deps**：依赖源属性列表
- **mode/converter/fallback**：绑定方向 / 值转换器 / 回退值
- **attach/detach**：激活 / 分离绑定
- **便捷工厂**：bind() / bind_inpc() / bind_property()

---

## 文件位置

```
src/mine/ui/binding/api/include/mine/ui/binding/BindingExpression.h
```

---

## 类定义

```cpp
class MINE_UI_BINDING_API BindingExpression {
public:
    using Getter = core::Function<core::Variant()>;
    using Setter = core::Function<void(const core::Variant&)>;

    // ── 配置字段 ─────────────────────────────────────────────────────────
    Getter getter;
    Setter setter;
    containers::Vector<PropertyDependency> deps;
    BindingMode mode = BindingMode::OneWay;
    IConverter* converter = nullptr;
    core::StringView conv_param;
    core::Variant fallback;

    // ── 生命周期 ─────────────────────────────────────────────────────────
    BindingExpression() noexcept;
    ~BindingExpression();
    BindingExpression(BindingExpression&&) noexcept;
    BindingExpression& operator=(BindingExpression&&) noexcept;

    // ── 激活 / 停用 ──────────────────────────────────────────────────────
    void attach(DependencyObject& target, const DependencyProperty& target_prop) noexcept;
    void detach() noexcept;
    void evaluate() noexcept;
    [[nodiscard]] bool is_attached() const noexcept;

    // ── 便捷工厂 ─────────────────────────────────────────────────────────
    static void bind_inpc(...);
    static void bind(...);  // 多个重载
    static void bind_property(...);
};
```

**描述**：运行时绑定表达式。

**特征**：
- 持有绑定所需的全部运行时状态
- 使用 PIMPL 模式隐藏实现细节，保证 ABI 稳定
- M1.1 实现 OneWay 和 OneTime
- M2 实现 TwoWay 和 OneWayToSource

---

## 成员类型

### Getter

```cpp
using Getter = core::Function<core::Variant()>;
```

**描述**：getter 类型：无参数、返回 Variant 的可调用对象（noexcept）。

---

### Setter

```cpp
using Setter = core::Function<void(const core::Variant&)>;
```

**描述**：setter 类型：接收 const Variant&、返回 void 的可调用对象（TwoWay 时，M2）。

---

## 配置字段

### getter

```cpp
Getter getter;
```

**描述**：源值求值函数（必填）。

**特征**：
- 通常为捕获了源对象引用的 lambda
- attach() 后将被移动到内部实现，此字段变为空

---

### setter

```cpp
Setter setter;
```

**描述**：值回写函数（TwoWay 时填写，M2 实现）。

**特征**：
- M1.1 中此字段不会被调用（即使填写了也会被忽略）
- attach() 后将被移动到内部实现

---

### deps

```cpp
containers::Vector<PropertyDependency> deps;
```

**描述**：依赖源属性列表（必填，至少包含一个依赖项）。

**特征**：
- 列表中每个 PropertyDependency 描述一个需要订阅的源属性
- 任意一个依赖源发生变更时，绑定系统重新求值并更新目标
- attach() 后此列表被清空（依赖已转移到内部订阅记录）

---

### mode

```cpp
BindingMode mode = BindingMode::OneWay;
```

**描述**：绑定方向，默认为 OneWay。

---

### converter

```cpp
IConverter* converter = nullptr;
```

**描述**：值转换器（可选）。

**特征**：
- 非 nullptr 时，getter 的返回值先经过 convert() 转换，再写入目标
- 不拥有此指针，调用方须保证其生命周期覆盖 BindingExpression

---

### conv_param

```cpp
core::StringView conv_param;
```

**描述**：传递给 converter.convert() 的参数字符串（可选）。

**特征**：
- 须为字符串字面量或长期存活的字符串
- attach() 后内部只存储 StringView

---

### fallback

```cpp
core::Variant fallback;
```

**描述**：回退值（可选）。

**特征**：
- 当 getter 返回空 Variant（或 converter 返回空 Variant）时，使用此值写入目标

---

## 成员方法

### attach()

```cpp
void attach(DependencyObject& target, const DependencyProperty& target_prop) noexcept;
```

**描述**：将绑定附加到目标属性，开始监听依赖源并更新目标。

**参数**：
- `target`：目标 DependencyObject
- `target_prop`：目标属性描述符

**行为**：
1. getter/setter/deps 字段被移动到内部实现（调用后字段为空）
2. 每个 dep 被订阅（PropertyChanged 或 INPC 回调）
3. 立即求值一次并以 ValuePriority::TemplateBind 写入目标
4. OneTime 模式：写入后立即取消所有订阅

**前提条件**：
- getter 非空
- target 的生命周期须覆盖此 BindingExpression
- 如果 is_attached() 已为 true，先调用 detach() 再 attach()

---

### detach()

```cpp
void detach() noexcept;
```

**描述**：分离绑定：取消所有依赖订阅，不修改目标属性的当前值。

**特征**：
- 分离后可重新填充 getter/setter/deps 并再次 attach()

---

### evaluate()

```cpp
void evaluate() noexcept;
```

**描述**：手动触发一次重新求值，更新目标属性值。

**特征**：
- 正常情况下不需要手动调用；源属性变更时自动触发
- 仅在已 attach() 后才有效，未 attach 时为空操作

---

### is_attached()

```cpp
[[nodiscard]] bool is_attached() const noexcept;
```

**描述**：检查绑定是否已激活（已调用 attach() 且未 detach()）。

---

## 便捷工厂方法

### bind_inpc()

```cpp
static void bind_inpc(
    BindingExpression&        out,
    INotifyPropertyChanged&   src,
    core::StringView          prop_name,
    Getter                    getter,
    DependencyObject&         target,
    const DependencyProperty& target_prop,
    BindingMode               mode = BindingMode::OneWay) noexcept;
```

**描述**：一步建立 INPC → DependencyProperty 的 OneWay 绑定。

**参数**：
- `out`：待激活的 BindingExpression（须未 attach）
- `src`：INotifyPropertyChanged 源对象（生命周期须覆盖 out）
- `prop_name`：要监听的属性名（须为字符串字面量或长期存活的字符串）
- `getter`：源值求值 lambda
- `target`：目标 DependencyObject（生命周期须覆盖 out）
- `target_prop`：目标属性描述符
- `mode`：绑定方向，默认 OneWay

---

### bind()（属性名版）

```cpp
static void bind(
    BindingExpression&        out,
    INotifyPropertyChanged&   src,
    core::StringView          prop_name,
    DependencyObject&         target,
    const DependencyProperty& target_prop,
    BindingMode               mode = BindingMode::OneWay) noexcept;
```

**描述**：WPF 风格：按属性名一步建立 INPC → DependencyProperty 的绑定。

**特征**：
- 等价于 bind_inpc() 但无需手写 getter lambda
- 内部通过 INotifyPropertyChanged::get_property(prop_name) 自动读取源值
- ObservableObject 子类（通过 MINE_OBSERVABLE 宏声明属性）自动实现此接口

---

### bind()（DataContext 版）

```cpp
static void bind(
    BindingExpression&        out,
    core::StringView          prop_name,
    DependencyObject&         target,
    const DependencyProperty& target_prop,
    BindingMode               mode = BindingMode::OneWay) noexcept;
```

**描述**：WPF 风格：从目标控件的 DataContext 自动解析源，按属性名建立绑定。

**特征**：
- 等价于 WPF 的 `{Binding PropName}` 语法
- 从 target 的 DataContext（由 Window::DataContextProperty inherits 传播而来）自动取出 INotifyPropertyChanged 指针

**前提条件**：
1. `register_data_context_property()` 已被调用（由 mine.ui.window 静态初始化时完成）
2. target 的 DataContextProperty 已持有 `INotifyPropertyChanged*` 值

---

### bind()（完整 Binding 描述符版）

```cpp
static void bind(
    BindingExpression&        out,
    INotifyPropertyChanged&   src,
    const Binding&            binding,
    DependencyObject&         target,
    const DependencyProperty& target_prop) noexcept;
```

**描述**：WPF 风格：完整 Binding 描述符版，支持 converter/conv_param/fallback。

**特征**：
- 等价于 WPF 的 `element.SetBinding(prop, new Binding("Name") { Converter=..., Mode=... })`

---

### bind()（DataContext + Binding 描述符版）

```cpp
static void bind(
    BindingExpression&        out,
    const Binding&            binding,
    DependencyObject&         target,
    const DependencyProperty& target_prop) noexcept;
```

**描述**：WPF 风格：从 DataContext 自动解析源 + 完整 Binding 描述符版。

---

### bind_property()

```cpp
static void bind_property(
    BindingExpression&        out,
    DependencyObject&         source,
    const DependencyProperty& source_prop,
    DependencyObject&         target,
    const DependencyProperty& target_prop,
    BindingMode               mode       = BindingMode::OneWay,
    IConverter*               converter  = nullptr,
    core::StringView          conv_param = {}) noexcept;
```

**描述**：一步建立 DependencyProperty → DependencyProperty 的绑定（元素间 / 控件间绑定）。

**特征**：
- 等价于 WPF 的 ElementName 绑定（`{Binding ElementName=src, Path=Prop}`）

**各模式行为**：
- `OneWay`：source.source_prop 变更 → 写入 target.target_prop（TemplateBind 优先级）
- `OneTime`：attach 时求值一次写入目标，随后不再订阅
- `TwoWay`：双向同步；目标变更回写源（Local 优先级），内置防循环保护
- `OneWayToSource`：仅 目标 → 源；attach 时把目标当前值回写源，随后订阅目标变更

---

## 使用场景

### 1. 手动构造 BindingExpression（DependencyObject 来源）

```cpp
#include <mine/ui/binding/BindingExpression.h>
#include <mine/ui/property/DependencyObject.h>

using namespace mine::ui;

Slider slider;
TextBlock label;

BindingExpression expr;

// 设置 getter
expr.getter = [&slider]() noexcept -> core::Variant {
    return slider.get_value(Slider::ValueProperty);
};

// 设置依赖
expr.deps.push_back(PropertyDependency::from_dep(slider, Slider::ValueProperty));

// 设置绑定方向
expr.mode = BindingMode::OneWay;

// 激活绑定
expr.attach(label, TextBlock::TextProperty);
```

---

### 2. 使用 bind_inpc()（ViewModel → UI）

```cpp
MyViewModel vm;
TextBlock label;

BindingExpression expr;
BindingExpression::bind_inpc(
    expr,
    vm, "name",
    [&vm]() noexcept -> core::Variant { return core::Variant{vm.name()}; },
    label, TextBlock::TextProperty);
```

---

### 3. 使用 bind()（属性名反射，推荐）

```cpp
// ObservableObject 子类通过 MINE_OBSERVABLE 宏声明属性，自动实现 get_property()
MyViewModel vm;
TextBlock label;

BindingExpression expr;
BindingExpression::bind(
    expr,
    vm, "name",  // 无需手写 getter lambda
    label, TextBlock::TextProperty);
```

---

### 4. 使用 bind()（DataContext 版，WPF 风格）

```cpp
// 假设 Window 已设置 DataContext
Window window;
window.set_data_context(&vm);

TextBlock label;  // label 继承 window 的 DataContext

BindingExpression expr;
BindingExpression::bind(
    expr,
    "name",  // 从 label 的 DataContext 自动解析源
    label, TextBlock::TextProperty);
```

---

### 5. 使用 bind()（完整 Binding 描述符）

```cpp
static BytesToHumanReadableConverter bytes_converter;

MyViewModel vm;
TextBlock label;

BindingExpression expr;
BindingExpression::bind(
    expr,
    vm,
    Binding{
        .prop_name = "file_size",
        .converter = &bytes_converter,
        .conv_param = "MB",
        .fallback = Variant{String{"N/A"}}
    },
    label, TextBlock::TextProperty);
```

---

### 6. 使用 bind_property()（控件间绑定）

```cpp
Slider slider;
ProgressBar progress;

BindingExpression expr;
BindingExpression::bind_property(
    expr,
    slider, Slider::ValueProperty,
    progress, ProgressBar::ValueProperty,
    BindingMode::TwoWay);  // 双向同步
```

---

## 生命周期管理

### attach() 工作流程

```
1. 验证前提条件（getter 非空、未 attach）
2. 移动 getter/setter/deps 到内部实现
3. 订阅每个 dep（PropertyChanged 或 INPC 回调）
4. 立即求值一次：
   - 调用 getter() → 源值
   - 如果 converter != nullptr：调用 converter->convert()
   - 如果结果为空 Variant：使用 fallback
   - 以 ValuePriority::TemplateBind 写入目标
5. 如果 mode == OneTime：立即取消所有订阅
```

---

### detach() 工作流程

```
1. 取消所有依赖订阅
2. 清空内部订阅记录
3. 不修改目标属性的当前值
4. 可重新填充 getter/setter/deps 并再次 attach()
```

---

### OneWay 绑定生命周期

```
1. attach()
   - 订阅源属性变更
   - 初始求值并写入目标

2. 源属性变更
   - 触发回调
   - 重新求值并更新目标

3. detach() 或析构
   - 取消订阅
   - 绑定失效
```

---

### TwoWay 绑定生命周期（M2）

```
1. attach()
   - 订阅源属性变更
   - 订阅目标属性变更
   - 初始求值并写入目标

2. 源属性变更
   - 触发正向回调
   - 重新求值并更新目标
   - 防循环保护（is_updating 标志）

3. 目标属性变更
   - 触发反向回调
   - 调用 setter() 回写源
   - 防循环保护（is_updating 标志）

4. detach() 或析构
   - 取消所有订阅
   - 绑定失效
```

---

## 最佳实践

### 1. 使用便捷工厂方法

```cpp
// ✅ 推荐：使用 bind()（属性名反射）
BindingExpression::bind(expr, vm, "name", label, TextBlock::TextProperty);

// ❌ 不推荐：手动构造（冗余）
expr.getter = [&vm]() { return vm.get_property("name"); };
expr.deps.push_back(PropertyDependency::from_inpc(vm, "name"));
expr.attach(label, TextBlock::TextProperty);
```

---

### 2. 确保源对象生命周期覆盖 BindingExpression

```cpp
// ✅ 推荐：源对象生命周期覆盖 BindingExpression
class MyView {
    MyViewModel vm_;  // 成员对象
    TextBlock label_;
    BindingExpression expr_;  // 成员对象
    
    void setup() {
        BindingExpression::bind(expr_, vm_, "name", label_, TextBlock::TextProperty);
    }
};

// ❌ 不推荐：源对象生命周期短于 BindingExpression
void bad_setup() {
    MyViewModel vm;  // 临时对象
    BindingExpression expr;
    BindingExpression::bind(expr, vm, "name", label, TextBlock::TextProperty);
}  // vm 被销毁，expr 持有悬空指针
```

---

### 3. 使用 DataContext 版 bind()（WPF 风格）

```cpp
// ✅ 推荐：使用 DataContext 版 bind()（视图层无需显式引用 ViewModel）
Window window;
window.set_data_context(&vm);

TextBlock label;
BindingExpression expr;
BindingExpression::bind(expr, "name", label, TextBlock::TextProperty);

// ❌ 不推荐：视图层显式引用 ViewModel（耦合）
BindingExpression::bind(expr, vm, "name", label, TextBlock::TextProperty);
```

---

### 4. 转换器使用静态对象或单例

```cpp
// ✅ 推荐：转换器使用静态对象
static BoolToVisibilityConverter converter;

BindingExpression::bind(expr, vm, Binding{
    .prop_name = "is_visible",
    .converter = &converter
}, label, TextBlock::VisibilityProperty);

// ❌ 不推荐：转换器使用局部对象
void bad_setup() {
    BoolToVisibilityConverter converter;  // 局部对象
    BindingExpression::bind(expr, vm, Binding{
        .prop_name = "is_visible",
        .converter = &converter  // 危险！函数返回后 converter 被销毁
    }, label, TextBlock::VisibilityProperty);
}
```

---

## 常见陷阱

### 1. attach() 后访问配置字段

```cpp
// ❌ 问题：attach() 后访问配置字段
BindingExpression expr;
expr.getter = [&vm]() { return vm.get_property("name"); };
expr.deps.push_back(PropertyDependency::from_inpc(vm, "name"));
expr.attach(label, TextBlock::TextProperty);

// expr.getter 已被移动到内部实现，此时为空
if (expr.getter) {  // ❌ false
    // ...
}

// ✅ 解决：attach() 后不要访问配置字段
```

---

### 2. 重复 attach()

```cpp
// ❌ 问题：重复 attach()
BindingExpression expr;
BindingExpression::bind(expr, vm, "name", label1, TextBlock::TextProperty);
BindingExpression::bind(expr, vm, "age", label2, TextBlock::TextProperty);  // ❌ 错误

// ✅ 解决：每个绑定使用独立的 BindingExpression
BindingExpression expr1, expr2;
BindingExpression::bind(expr1, vm, "name", label1, TextBlock::TextProperty);
BindingExpression::bind(expr2, vm, "age", label2, TextBlock::TextProperty);
```

---

### 3. 源对象生命周期短于 BindingExpression

```cpp
// ❌ 问题：源对象生命周期短于 BindingExpression
void bad_setup() {
    MyViewModel vm;  // 临时对象
    BindingExpression expr;
    BindingExpression::bind(expr, vm, "name", label, TextBlock::TextProperty);
}  // vm 被销毁，expr 持有悬空指针

// ✅ 解决：确保源对象生命周期覆盖 BindingExpression
class MyView {
    MyViewModel vm_;
    BindingExpression expr_;
    void setup() {
        BindingExpression::bind(expr_, vm_, "name", label, TextBlock::TextProperty);
    }
};
```

---

## 完整示例

```cpp
#include <mine/ui/binding/BindingExpression.h>
#include <mine/ui/binding/IConverter.h>
#include <mine/mvvm/ObservableObject.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/TextBox.h>

using namespace mine::ui;
using namespace mine::mvvm;
using namespace mine::core;

// ────────────────────────────────────────────────────────────────────────────
// ViewModel 定义
// ────────────────────────────────────────────────────────────────────────────

class MyViewModel : public ObservableObject {
public:
    MINE_OBSERVABLE(String, name, "name", "Alice");
    MINE_OBSERVABLE(int, age, "age", 25);
    MINE_OBSERVABLE(int64_t, file_size, "file_size", 1048576);
};

// ────────────────────────────────────────────────────────────────────────────
// 转换器定义
// ────────────────────────────────────────────────────────────────────────────

static BytesToHumanReadableConverter bytes_converter;

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    MyViewModel vm;
    TextBlock nameLabel, ageLabel, sizeLabel;
    TextBox nameInput;
    
    // ── 基本绑定（属性名反射） ──────────────────────────────────────────
    BindingExpression expr1;
    BindingExpression::bind(
        expr1,
        vm, "name",
        nameLabel, TextBlock::TextProperty);
    
    // ── TwoWay 绑定 ─────────────────────────────────────────────────────
    BindingExpression expr2;
    BindingExpression::bind(
        expr2,
        vm, "name",
        nameInput, TextBox::TextProperty,
        BindingMode::TwoWay);
    
    // ── 使用转换器 + 参数 ───────────────────────────────────────────────
    BindingExpression expr3;
    BindingExpression::bind(
        expr3,
        vm,
        Binding{
            .prop_name = "file_size",
            .converter = &bytes_converter,
            .conv_param = "MB",
            .fallback = Variant{String{"N/A"}}
        },
        sizeLabel, TextBlock::TextProperty);
    
    // ── 控件间绑定 ──────────────────────────────────────────────────────
    Slider slider;
    ProgressBar progress;
    
    BindingExpression expr4;
    BindingExpression::bind_property(
        expr4,
        slider, Slider::ValueProperty,
        progress, ProgressBar::ValueProperty,
        BindingMode::TwoWay);
    
    // ── DataContext 版绑定（WPF 风格） ──────────────────────────────────
    Window window;
    window.set_data_context(&vm);
    
    TextBlock label;  // 继承 window 的 DataContext
    
    BindingExpression expr5;
    BindingExpression::bind(
        expr5,
        "name",  // 从 label 的 DataContext 自动解析源
        label, TextBlock::TextProperty);
    
    return 0;
}
```

---

## 总结

`BindingExpression` 是 `mine.ui.binding` 模块的运行时数据绑定表达式，具备：

1. **配置字段**：getter/setter/deps/mode/converter/fallback
2. **生命周期**：attach() → 激活绑定 → detach() → 分离绑定
3. **便捷工厂**：bind() / bind_inpc() / bind_property()

**使用建议**：
- 使用便捷工厂方法（bind() / bind_inpc() / bind_property()）而非手动构造
- 确保源对象生命周期覆盖 BindingExpression（避免悬空指针）
- 使用 DataContext 版 bind()（视图层无需显式引用 ViewModel）
- 转换器使用静态对象或单例（确保生命周期覆盖绑定）
- attach() 后不要访问配置字段（已被移动到内部实现）
- 每个绑定使用独立的 BindingExpression（不要重复 attach()）
