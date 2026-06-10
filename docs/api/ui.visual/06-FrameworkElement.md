# FrameworkElement 详细接口文档

## 概述

`FrameworkElement` 是 `mine.ui.visual` 模块的**具有 Margin、尺寸约束和对齐方式的布局元素基类**。

**核心特性：**
- **尺寸约束**：Width/Height/MinWidth/MaxWidth/MinHeight/MaxHeight（依赖属性）
- **Margin**：外边距（`math::Thickness`）
- **对齐方式**：HorizontalAlignment / VerticalAlignment（依赖属性）
- **数据绑定**：set_binding（WPF 风格）、bind_property（元素间绑定）
- **两阶段布局**：measure_override / arrange_override（子类覆盖点）
- **WPF 风格布局协议**：Measure（自顶向下 availableSize，从底向上 desiredSize）→ Arrange（自顶向下 finalRect）

**继承关系：**
```
Visual (mine.ui.visual)
    └─ UIElement (mine.ui.visual)
        └─ FrameworkElement (mine.ui.visual)
            └─ Control (mine.ui.visual)
            └─ Panel (mine.ui.layout)
```

---

## 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/FrameworkElement.h
```

---

## 类定义

```cpp
class MINE_UI_VISUAL_API FrameworkElement : public UIElement {
public:
    // 依赖属性声明
    static const DependencyProperty& WidthProperty;
    static const DependencyProperty& HeightProperty;
    static const DependencyProperty& MinWidthProperty;
    static const DependencyProperty& MaxWidthProperty;
    static const DependencyProperty& MinHeightProperty;
    static const DependencyProperty& MaxHeightProperty;
    static const DependencyProperty& MarginProperty;
    static const DependencyProperty& HorizontalAlignmentProperty;
    static const DependencyProperty& VerticalAlignmentProperty;

    // 生命周期
    FrameworkElement();
    ~FrameworkElement() override;
    FrameworkElement(const FrameworkElement&)            = delete;
    FrameworkElement& operator=(const FrameworkElement&) = delete;
    FrameworkElement(FrameworkElement&&) noexcept;
    FrameworkElement& operator=(FrameworkElement&&) noexcept;

    // 数据绑定 API
    void set_binding(
        const DependencyProperty& target_prop,
        core::StringView          prop_name,
        BindingMode               mode = BindingMode::OneWay) noexcept;

    void set_binding(
        const DependencyProperty& target_prop,
        const Binding&            binding) noexcept;

    void bind_property(
        const DependencyProperty& target_prop,
        DependencyObject&         source,
        const DependencyProperty& source_prop,
        BindingMode               mode      = BindingMode::OneWay,
        IConverter*               converter = nullptr) noexcept;

    void clear_all_bindings() noexcept;

    // 尺寸属性访问
    [[nodiscard]] float width() const noexcept;
    void set_width(float w);

    [[nodiscard]] float height() const noexcept;
    void set_height(float h);

    [[nodiscard]] float min_width()  const noexcept;
    void set_min_width(float v);

    [[nodiscard]] float max_width()  const noexcept;
    void set_max_width(float v);

    [[nodiscard]] float min_height() const noexcept;
    void set_min_height(float v);

    [[nodiscard]] float max_height() const noexcept;
    void set_max_height(float v);

    // 对齐与外边距
    [[nodiscard]] math::Thickness     margin()               const noexcept;
    void set_margin(math::Thickness m);

    [[nodiscard]] HorizontalAlignment horizontal_alignment() const noexcept;
    void set_horizontal_alignment(HorizontalAlignment ha);

    [[nodiscard]] VerticalAlignment   vertical_alignment()   const noexcept;
    void set_vertical_alignment(VerticalAlignment va);

    // Arrange 公共入口
    void arrange(math::Rect slot) override;

protected:
    // 布局扩展点（子类覆盖）
    virtual math::Size measure_override(math::Size available);
    virtual math::Size arrange_override(math::Size final_size);

    // 覆盖 UIElement 的布局虚方法
    void on_measure(math::Size available_size) override;
    void on_arrange(math::Rect final_rect) override;

private:
    containers::Vector<BindingExpression> bindings_;
};
```

---

## 依赖属性

### WidthProperty

```cpp
static const DependencyProperty& WidthProperty;
```

**描述**：宽度依赖属性。

**类型**：`float`

**默认值**：`NaN`（自动，由内容决定）

**特征**：
- `NaN` 表示由内容自动决定
- 明确值（如 `100.0f`）表示固定宽度
- 受 `MinWidth` / `MaxWidth` 约束

---

### HeightProperty

```cpp
static const DependencyProperty& HeightProperty;
```

**描述**：高度依赖属性。

**类型**：`float`

**默认值**：`NaN`（自动，由内容决定）

**特征**：
- `NaN` 表示由内容自动决定
- 明确值表示固定高度
- 受 `MinHeight` / `MaxHeight` 约束

---

### MinWidthProperty

```cpp
static const DependencyProperty& MinWidthProperty;
```

**描述**：最小宽度依赖属性。

**类型**：`float`

**默认值**：`0.0f`

---

### MaxWidthProperty

```cpp
static const DependencyProperty& MaxWidthProperty;
```

**描述**：最大宽度依赖属性。

**类型**：`float`

**默认值**：`无限大`

---

### MinHeightProperty

```cpp
static const DependencyProperty& MinHeightProperty;
```

**描述**：最小高度依赖属性。

**类型**：`float`

**默认值**：`0.0f`

---

### MaxHeightProperty

```cpp
static const DependencyProperty& MaxHeightProperty;
```

**描述**：最大高度依赖属性。

**类型**：`float`

**默认值**：`无限大`

---

### MarginProperty

```cpp
static const DependencyProperty& MarginProperty;
```

**描述**：外边距依赖属性。

**类型**：`math::Thickness`

**默认值**：`{0, 0, 0, 0}`（无外边距）

**特征**：
- Margin 在布局 Arrange 阶段从父节点分配的槽（slot）中减去
- 剩余区域为元素内容区域

---

### HorizontalAlignmentProperty

```cpp
static const DependencyProperty& HorizontalAlignmentProperty;
```

**描述**：水平对齐依赖属性。

**类型**：`HorizontalAlignment`

**默认值**：`Stretch`

**枚举值**：
- `Left`（靠左）
- `Center`（居中）
- `Right`（靠右）
- `Stretch`（拉伸填充，默认）

---

### VerticalAlignmentProperty

```cpp
static const DependencyProperty& VerticalAlignmentProperty;
```

**描述**：垂直对齐依赖属性。

**类型**：`VerticalAlignment`

**默认值**：`Stretch`

**枚举值**：
- `Top`（靠顶）
- `Center`（居中）
- `Bottom`（靠底）
- `Stretch`（拉伸填充，默认）

---

## 生命周期

### FrameworkElement

```cpp
FrameworkElement();
```

**描述**：构造 FrameworkElement 对象。

**初始状态**：
- `Width` = `NaN`（自动）
- `Height` = `NaN`（自动）
- `MinWidth` / `MinHeight` = `0.0f`
- `MaxWidth` / `MaxHeight` = 无限大
- `Margin` = `{0, 0, 0, 0}`
- `HorizontalAlignment` = `Stretch`
- `VerticalAlignment` = `Stretch`

---

### ~FrameworkElement

```cpp
~FrameworkElement() override;
```

**描述**：析构 FrameworkElement 对象，自动清理所有绑定。

---

### FrameworkElement (移动构造)

```cpp
FrameworkElement(FrameworkElement&&) noexcept;
```

**描述**：移动构造函数。

**行为**：
- 移动所有绑定
- 对每个绑定调用 `retarget(*this)` 修复 `target_obj` 指针

---

### operator= (移动赋值)

```cpp
FrameworkElement& operator=(FrameworkElement&&) noexcept;
```

**描述**：移动赋值运算符。

**行为**：
- 移动所有绑定
- 对每个绑定调用 `retarget(*this)` 修复 `target_obj` 指针

---

## 数据绑定 API

### set_binding (ViewModel 绑定)

```cpp
void set_binding(
    const DependencyProperty& target_prop,
    core::StringView          prop_name,
    BindingMode               mode = BindingMode::OneWay) noexcept;
```

**描述**：WPF 风格数据绑定，从控件 DataContext 自动解析 ViewModel，按属性名建立绑定。

**参数**：
- `target_prop`：目标依赖属性（如 `TextBlock::TextProperty`）
- `prop_name`：ViewModel 属性名（须与 `MINE_OBSERVABLE` 宏名称完全一致）
- `mode`：绑定方向（默认 `OneWay`）

**前提条件**：
1. `mine.ui.window` 已完成静态初始化（`DataContextProperty` 已注入）
2. 本元素或其祖先节点已通过 `Window::set_data_context()` 设置 DataContext

**特征**：
- 等价于 WPF 的 `element.SetBinding(property, new Binding("PropName"))`
- 绑定生命周期与控件绑定，不需要在外部声明任何 `BindingExpression` 成员变量
- 绑定存储在内部 `bindings_` 容器中，析构时自动清理

**示例**：
```cpp
// ViewModel
class MainViewModel {
    MINE_OBSERVABLE(std::string, UserName, "Admin");
};

// View
TextBlock text_block;
text_block.set_binding(TextBlock::TextProperty, "UserName");

// text_block.text() 将自动与 ViewModel.UserName 同步
```

---

### set_binding (Binding 描述符)

```cpp
void set_binding(
    const DependencyProperty& target_prop,
    const Binding&            binding) noexcept;
```

**描述**：WPF 风格数据绑定，按 Binding 描述符建立绑定（支持 converter/conv_param/fallback）。

**参数**：
- `target_prop`：目标依赖属性
- `binding`：Binding 描述符（`prop_name` / `mode` / `converter` / `conv_param` / `fallback`）

**特征**：
- 等价于 WPF 的 `element.SetBinding(prop, new Binding("Name") { Converter=... })`
- C++20 指定初始化语法简洁表达绑定配置

**示例**：
```cpp
// 值转换器
core::Variant bytes_to_human_readable(
    const core::Variant& value, 
    const core::Variant& param) {
    
    uint64_t bytes = value.get<uint64_t>();
    std::string unit = param.get<std::string>();
    
    double result = 0.0;
    if (unit == "KB") result = bytes / 1024.0;
    else if (unit == "MB") result = bytes / (1024.0 * 1024.0);
    
    return core::Variant{ result };
}

// 使用
label_.set_binding(TextBlock::TextProperty, ui::Binding{
    .prop_name  = "byte_count",
    .converter  = &bytes_to_human_readable,
    .conv_param = core::Variant{ "MB" },
    .fallback   = core::Variant{ "N/A" },
});
```

---

### bind_property

```cpp
void bind_property(
    const DependencyProperty& target_prop,
    DependencyObject&         source,
    const DependencyProperty& source_prop,
    BindingMode               mode      = BindingMode::OneWay,
    IConverter*               converter = nullptr) noexcept;
```

**描述**：元素间绑定（DP ↔ DP），把本元素某属性绑定到另一个 DependencyObject 的属性。

**参数**：
- `target_prop`：本元素的目标属性
- `source`：源 DependencyObject（生命周期须覆盖本元素）
- `source_prop`：源属性描述符
- `mode`：绑定方向（默认 `OneWay`）
- `converter`：可选值转换器（不拥有）

**特征**：
- 等价于 WPF 的 ElementName 绑定
- 绑定生命周期由本元素的内置存储管理
- 无需在外部声明 `BindingExpression`
- 适用于复合控件把宿主属性同步到子元素

**示例**：
```cpp
// 让本 Border 的圆角随 host 的 CornerRadius 单向同步
border.bind_property(
    Border::CornerRadiusProperty,
    host, TextBox::CornerRadiusProperty
);

// host.CornerRadius 变化时，border.CornerRadius 自动同步
```

---

### clear_all_bindings

```cpp
void clear_all_bindings() noexcept;
```

**描述**：清除本元素上已建立的所有绑定（元素析构时自动调用）。

**特征**：
- 手动调用适用于需要整体替换数据源的场景
- 清空内部 `bindings_` 容器，逐一调用 `detach()`

**示例**：
```cpp
// 清除所有绑定
element.clear_all_bindings();

// 重新建立绑定
element.set_binding(TextBlock::TextProperty, "NewProperty");
```

---

## 尺寸属性访问

### width

```cpp
[[nodiscard]] float width() const noexcept;
```

**描述**：返回明确指定的宽度。

**返回值**：
- `NaN`：自动，由内容决定
- 明确值：固定宽度

---

### set_width

```cpp
void set_width(float w);
```

**描述**：设置宽度。

**参数**：
- `w`：宽度值（`NaN` = 自动）

**行为**：
- 调用 `invalidate_measure()` 触发重新测量

---

### height

```cpp
[[nodiscard]] float height() const noexcept;
```

**描述**：返回明确指定的高度。

**返回值**：
- `NaN`：自动，由内容决定
- 明确值：固定高度

---

### set_height

```cpp
void set_height(float h);
```

**描述**：设置高度。

**参数**：
- `h`：高度值（`NaN` = 自动）

**行为**：
- 调用 `invalidate_measure()` 触发重新测量

---

### min_width / set_min_width

```cpp
[[nodiscard]] float min_width() const noexcept;
void set_min_width(float v);
```

**描述**：最小宽度访问器。

---

### max_width / set_max_width

```cpp
[[nodiscard]] float max_width() const noexcept;
void set_max_width(float v);
```

**描述**：最大宽度访问器。

---

### min_height / set_min_height

```cpp
[[nodiscard]] float min_height() const noexcept;
void set_min_height(float v);
```

**描述**：最小高度访问器。

---

### max_height / set_max_height

```cpp
[[nodiscard]] float max_height() const noexcept;
void set_max_height(float v);
```

**描述**：最大高度访问器。

---

## 对齐与外边距

### margin

```cpp
[[nodiscard]] math::Thickness margin() const noexcept;
```

**描述**：返回外边距。

**返回值**：
- `math::Thickness`（`left` / `top` / `right` / `bottom`）

---

### set_margin

```cpp
void set_margin(math::Thickness m);
```

**描述**：设置外边距。

**参数**：
- `m`：外边距（`math::Thickness`）

**行为**：
- 调用 `invalidate_arrange()` 触发重新排列

---

### horizontal_alignment

```cpp
[[nodiscard]] HorizontalAlignment horizontal_alignment() const noexcept;
```

**描述**：返回水平对齐方式。

**返回值**：
- `Left` / `Center` / `Right` / `Stretch`（默认）

---

### set_horizontal_alignment

```cpp
void set_horizontal_alignment(HorizontalAlignment ha);
```

**描述**：设置水平对齐方式。

**参数**：
- `ha`：水平对齐方式

**行为**：
- 调用 `invalidate_arrange()` 触发重新排列

---

### vertical_alignment

```cpp
[[nodiscard]] VerticalAlignment vertical_alignment() const noexcept;
```

**描述**：返回垂直对齐方式。

**返回值**：
- `Top` / `Center` / `Bottom` / `Stretch`（默认）

---

### set_vertical_alignment

```cpp
void set_vertical_alignment(VerticalAlignment va);
```

**描述**：设置垂直对齐方式。

**参数**：
- `va`：垂直对齐方式

**行为**：
- 调用 `invalidate_arrange()` 触发重新排列

---

## Arrange 公共入口

### arrange

```cpp
void arrange(math::Rect slot) override;
```

**描述**：Arrange 公共入口，覆盖 `UIElement::arrange`。

**参数**：
- `slot`：父节点分配给本元素的完整矩形槽（含 Margin 区域）

**行为**：
1. 减去 Margin
2. 应用 HorizontalAlignment / VerticalAlignment 对齐
3. 调用 `set_bounds_rect(content_rect)`
4. 进而触发 `on_arrange(content_rect)` → `arrange_override(content_size)`

---

## 受保护方法

### measure_override

```cpp
virtual math::Size measure_override(math::Size available);
```

**描述**：Measure 覆盖点，计算元素内容区域的期望尺寸。

**参数**：
- `available`：可用内容区域尺寸（已减去 Margin 并应用了 Width/Height/Min/Max 约束）

**返回值**：
- 内容期望尺寸（不含 Margin）

**默认实现**：
- 返回 `{0, 0}`（叶子元素无需测量）

**子类实现建议**：
- Panel 子类（StackPanel、Grid）覆盖此方法实现自定义测量逻辑
- 递归测量子节点（调用子节点的 `measure()`）
- 返回内容区域期望尺寸

**示例**：
```cpp
class StackPanel : public FrameworkElement {
protected:
    math::Size measure_override(math::Size available) override {
        // 测量所有子节点
        double total_height = 0.0;
        double max_width = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                child->measure(available);
                math::Size child_desired = child->desired_size();
                total_height += child_desired.height;
                max_width = std::max(max_width, child_desired.width);
            }
        }
        
        return math::Size{max_width, total_height};
    }
};
```

---

### arrange_override

```cpp
virtual math::Size arrange_override(math::Size final_size);
```

**描述**：Arrange 覆盖点，在确定的内容区域内安排子元素。

**参数**：
- `final_size`：分配给内容区域的最终尺寸（已减去 Margin 并经对齐处理）

**返回值**：
- 实际占用的内容尺寸（通常与 `final_size` 相同）

**默认实现**：
- 直接返回 `final_size`（叶子元素无子元素需排列）

**子类实现建议**：
- Panel 子类覆盖此方法调用子元素的 `arrange()` 方法进行递归排列
- 返回实际占用的内容尺寸

**示例**：
```cpp
class StackPanel : public FrameworkElement {
protected:
    math::Size arrange_override(math::Size final_size) override {
        // 垂直排列所有子节点
        double y_offset = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                math::Size child_desired = child->desired_size();
                math::Rect child_rect{
                    0, y_offset,
                    final_size.width, child_desired.height
                };
                child->arrange(child_rect);
                y_offset += child_desired.height;
            }
        }
        
        return final_size;
    }
};
```

---

### on_measure

```cpp
void on_measure(math::Size available_size) override;
```

**描述**：覆盖 `UIElement::on_measure`，处理 Margin 和尺寸约束后调用 `measure_override`。

**参数**：
- `available_size`：父节点提供的可用空间

**行为**：
1. 减去 Margin
2. 应用 Width / Height / Min / Max 约束
3. 调用 `measure_override(constrained)`
4. 加回 Margin，调用 `set_desired_size(total)`

---

### on_arrange

```cpp
void on_arrange(math::Rect final_rect) override;
```

**描述**：覆盖 `UIElement::on_arrange`，调用 `arrange_override`。

**参数**：
- `final_rect`：内容区域最终矩形（与 `bounds_rect()` 相同）

**行为**：
- 由 `UIElement::set_bounds_rect()` 在设置 `bounds_rect_` 后回调
- 此时 `bounds_rect()` 已是经过 Margin 和对齐处理的内容矩形
- 调用 `arrange_override(final_rect.size())`

---

## 两阶段布局协议

### Measure 阶段流程

```
父节点调用 child->measure(available)
    ↓
FrameworkElement::measure(available)
    ↓
FrameworkElement::on_measure(available)
    ↓
1. 减去 Margin
2. 应用 Width/Height/Min/Max 约束
    ↓
measure_override(constrained)  ← 子类覆盖点
    ↓
返回内容期望尺寸
    ↓
加回 Margin，调用 set_desired_size(total)
    ↓
desired_size() 可供父节点读取
```

---

### Arrange 阶段流程

```
父节点调用 child->arrange(slot)
    ↓
FrameworkElement::arrange(slot)
    ↓
1. 减去 Margin
2. 应用 HorizontalAlignment / VerticalAlignment 对齐
    ↓
set_bounds_rect(content_rect)
    ↓
UIElement::set_bounds_rect(content_rect)
    ↓
on_arrange(content_rect)  ← UIElement 回调
    ↓
FrameworkElement::on_arrange(content_rect)
    ↓
arrange_override(content_rect.size())  ← 子类覆盖点
    ↓
子类安排子元素（调用子元素的 arrange()）
```

---

## 使用场景

### 1. 自定义面板（覆盖 measure_override / arrange_override）

```cpp
#include <mine/ui/visual/FrameworkElement.h>

using namespace mine::ui;

class VerticalPanel : public FrameworkElement {
protected:
    math::Size measure_override(math::Size available) override {
        // 测量所有子节点
        double total_height = 0.0;
        double max_width = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                child->measure(available);
                math::Size child_desired = child->desired_size();
                total_height += child_desired.height;
                max_width = std::max(max_width, child_desired.width);
            }
        }
        
        return math::Size{max_width, total_height};
    }
    
    math::Size arrange_override(math::Size final_size) override {
        // 垂直排列所有子节点
        double y_offset = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                math::Size child_desired = child->desired_size();
                math::Rect child_rect{
                    0, y_offset,
                    final_size.width, child_desired.height
                };
                child->arrange(child_rect);
                y_offset += child_desired.height;
            }
        }
        
        return final_size;
    }
};
```

---

### 2. 设置尺寸约束

```cpp
// 固定宽度，自动高度
element.set_width(200);
element.set_height(std::nanf(""));  // NaN = 自动

// 最小/最大尺寸约束
element.set_min_width(100);
element.set_max_width(400);
element.set_min_height(50);
element.set_max_height(300);
```

---

### 3. 设置 Margin 和对齐方式

```cpp
// Margin（左、上、右、下）
element.set_margin(math::Thickness{10, 5, 10, 5});

// 居中对齐
element.set_horizontal_alignment(HorizontalAlignment::Center);
element.set_vertical_alignment(VerticalAlignment::Center);

// 左上角对齐
element.set_horizontal_alignment(HorizontalAlignment::Left);
element.set_vertical_alignment(VerticalAlignment::Top);

// 拉伸填充（默认）
element.set_horizontal_alignment(HorizontalAlignment::Stretch);
element.set_vertical_alignment(VerticalAlignment::Stretch);
```

---

### 4. WPF 风格数据绑定

```cpp
// ViewModel
class UserViewModel {
public:
    MINE_OBSERVABLE(std::string, UserName, "Admin");
    MINE_OBSERVABLE(int, Age, 25);
};

// View
TextBlock name_label;
TextBlock age_label;

// 单向绑定
name_label.set_binding(TextBlock::TextProperty, "UserName");
age_label.set_binding(TextBlock::TextProperty, "Age");

// 双向绑定（适用于输入控件）
TextBox name_input;
name_input.set_binding(TextBox::TextProperty, "UserName", BindingMode::TwoWay);
```

---

### 5. 带值转换器的绑定

```cpp
// 值转换器（字节 → 人类可读）
core::Variant bytes_to_human(
    const core::Variant& value, 
    const core::Variant& param) {
    
    uint64_t bytes = value.get<uint64_t>();
    std::string unit = param.get<std::string>();
    
    double result = 0.0;
    if (unit == "KB") result = bytes / 1024.0;
    else if (unit == "MB") result = bytes / (1024.0 * 1024.0);
    else if (unit == "GB") result = bytes / (1024.0 * 1024.0 * 1024.0);
    
    return core::Variant{ result };
}

// 使用
TextBlock size_label;
size_label.set_binding(TextBlock::TextProperty, ui::Binding{
    .prop_name  = "file_size",
    .converter  = &bytes_to_human,
    .conv_param = core::Variant{ "MB" },
    .fallback   = core::Variant{ "N/A" },
});
```

---

### 6. 元素间绑定

```cpp
// 宿主控件
class TextBox : public Control {
public:
    static const DependencyProperty& CornerRadiusProperty;
    
    TextBox() {
        // 内部边框（Border）
        border_ = std::make_unique<Border>();
        
        // 将 border 的 CornerRadius 绑定到宿主的 CornerRadius
        border_->bind_property(
            Border::CornerRadiusProperty,
            *this, TextBox::CornerRadiusProperty
        );
    }

private:
    std::unique_ptr<Border> border_;
};

// host.set_corner_radius(5.0f) 后，border_.corner_radius() 自动同步为 5.0f
```

---

### 7. 清除所有绑定

```cpp
// 清除所有绑定（适用于替换数据源）
element.clear_all_bindings();

// 重新建立绑定
element.set_binding(TextBlock::TextProperty, "NewProperty");
```

---

## 最佳实践

### 1. 覆盖 measure_override / arrange_override 而非 on_measure / on_arrange

```cpp
// ✅ 推荐：覆盖 measure_override
class MyPanel : public FrameworkElement {
protected:
    math::Size measure_override(math::Size available) override {
        // 自定义测量逻辑
    }
};

// ❌ 不推荐：覆盖 on_measure（破坏 Margin 和约束处理）
class MyPanel : public FrameworkElement {
protected:
    void on_measure(math::Size available) override {
        // ❌ 破坏基类 Margin 和约束处理
    }
};
```

---

### 2. 使用 NaN 表示自动尺寸

```cpp
// ✅ 推荐：使用 NaN 表示自动尺寸
element.set_width(std::nanf(""));  // 自动宽度
element.set_height(std::nanf(""));  // 自动高度

// ❌ 不推荐：使用 0 表示自动（会被误解为固定 0 尺寸）
element.set_width(0);  // ❌ 固定 0 宽度，不是自动
```

---

### 3. 设置 Margin 而非手动计算位置

```cpp
// ✅ 推荐：设置 Margin
button.set_margin(math::Thickness{10, 5, 10, 5});

// ❌ 不推荐：手动计算位置（破坏布局系统）
button.set_bounds_rect(math::Rect{10, 5, 180, 40});  // ❌ 破坏布局
```

---

### 4. 使用 set_binding 而非手动同步

```cpp
// ✅ 推荐：使用 set_binding
text_block.set_binding(TextBlock::TextProperty, "UserName");

// ❌ 不推荐：手动同步（容易遗漏、代码冗余）
void on_user_name_changed(const std::string& new_name) {
    text_block.set_text(new_name);  // ❌ 手动同步
}
```

---

### 5. 元素间绑定使用 bind_property

```cpp
// ✅ 推荐：使用 bind_property
border_->bind_property(
    Border::CornerRadiusProperty,
    *this, TextBox::CornerRadiusProperty
);

// ❌ 不推荐：在属性更改回调中手动同步（容易遗漏）
void on_corner_radius_changed(float new_radius) {
    border_->set_corner_radius(new_radius);  // ❌ 手动同步
}
```

---

## 常见陷阱

### 1. 覆盖 on_measure / on_arrange 破坏基类逻辑

```cpp
// ❌ 问题：覆盖 on_measure 破坏 Margin 和约束处理
class MyPanel : public FrameworkElement {
protected:
    void on_measure(math::Size available) override {
        // ❌ 破坏基类 Margin 和约束处理
        // Margin 未减去，Width/Height/Min/Max 约束未应用
    }
};

// ✅ 解决：覆盖 measure_override
class MyPanel : public FrameworkElement {
protected:
    math::Size measure_override(math::Size available) override {
        // 输入 available 已减去 Margin 并应用约束
        // 返回内容期望尺寸（不含 Margin）
    }
};
```

---

### 2. 使用 0 表示自动尺寸

```cpp
// ❌ 问题：使用 0 表示自动尺寸
element.set_width(0);  // ❌ 固定 0 宽度，不是自动

// 元素宽度被强制为 0，不显示

// ✅ 解决：使用 NaN 表示自动
element.set_width(std::nanf(""));  // 自动宽度
```

---

### 3. 忘记调用 invalidate_measure / invalidate_arrange

```cpp
// ❌ 问题：内容变更但未触发布局失效
void set_content(const std::string& content) {
    content_ = content;
    // ❌ 忘记触发 invalidate_measure
}

// 布局系统不知道需要重新测量

// ✅ 解决：触发 invalidate_measure
void set_content(const std::string& content) {
    if (content_ != content) {
        content_ = content;
        invalidate_measure();
        invalidate_render();
    }
}
```

---

### 4. 手动设置 bounds_rect 破坏布局系统

```cpp
// ❌ 问题：手动设置 bounds_rect 破坏布局系统
element.set_bounds_rect(math::Rect{10, 5, 180, 40});
// ❌ 布局系统 Arrange 阶段会覆盖此设置

// ✅ 解决：使用 Margin 和对齐方式
element.set_margin(math::Thickness{10, 5, 0, 0});
element.set_horizontal_alignment(HorizontalAlignment::Left);
element.set_vertical_alignment(VerticalAlignment::Top);
```

---

### 5. 绑定属性名拼写错误

```cpp
// ❌ 问题：绑定属性名拼写错误
text_block.set_binding(TextBlock::TextProperty, "UserNam");  // ❌ 拼写错误

// 绑定失败，运行时无法找到 ViewModel 属性

// ✅ 解决：确保属性名与 MINE_OBSERVABLE 宏名称完全一致
class UserViewModel {
public:
    MINE_OBSERVABLE(std::string, UserName, "Admin");  // 正确名称
};

text_block.set_binding(TextBlock::TextProperty, "UserName");  // ✅ 正确
```

---

## 完整示例

```cpp
#include <mine/ui/visual/FrameworkElement.h>
#include <mine/ui/visual/Control.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Paint.h>
#include <mine/paint/Color.h>
#include <mine/math/Thickness.h>
#include <mine/math/Rect.h>
#include <mine/math/Size.h>
#include <iostream>

using namespace mine::ui;
using namespace mine::paint;
using namespace mine::math;

// ────────────────────────────────────────────────────────────────────────────
// ViewModel
// ────────────────────────────────────────────────────────────────────────────

class UserViewModel {
public:
    MINE_OBSERVABLE(std::string, UserName, "Admin");
    MINE_OBSERVABLE(int, Age, 25);
    MINE_OBSERVABLE(uint64_t, FileSize, 1048576);  // 1 MB
};

// ────────────────────────────────────────────────────────────────────────────
// 值转换器
// ────────────────────────────────────────────────────────────────────────────

core::Variant bytes_to_human(
    const core::Variant& value, 
    const core::Variant& param) {
    
    uint64_t bytes = value.get<uint64_t>();
    std::string unit = param.get<std::string>();
    
    double result = 0.0;
    if (unit == "KB") result = bytes / 1024.0;
    else if (unit == "MB") result = bytes / (1024.0 * 1024.0);
    else if (unit == "GB") result = bytes / (1024.0 * 1024.0 * 1024.0);
    
    return core::Variant{ result };
}

// ────────────────────────────────────────────────────────────────────────────
// 自定义垂直面板
// ────────────────────────────────────────────────────────────────────────────

class VerticalPanel : public FrameworkElement {
protected:
    Size measure_override(Size available) override {
        // 测量所有子节点
        double total_height = 0.0;
        double max_width = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                child->measure(available);
                Size child_desired = child->desired_size();
                total_height += child_desired.height;
                max_width = std::max(max_width, child_desired.width);
            }
        }
        
        return Size{max_width, total_height};
    }
    
    Size arrange_override(Size final_size) override {
        // 垂直排列所有子节点
        double y_offset = 0.0;
        
        for (uint32_t i = 0; i < child_count(); ++i) {
            UIElement* child = child_at(i)->as_element();
            if (child != nullptr) {
                Size child_desired = child->desired_size();
                Rect child_rect{
                    0, y_offset,
                    final_size.width, child_desired.height
                };
                child->arrange(child_rect);
                y_offset += child_desired.height;
            }
        }
        
        return final_size;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // ════════════════════════════════════════════════════════════════════════
    // 1. 创建 ViewModel
    // ════════════════════════════════════════════════════════════════════════
    UserViewModel view_model;
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 创建 View 元素
    // ════════════════════════════════════════════════════════════════════════
    VerticalPanel panel;
    
    TextBlock name_label;
    TextBlock age_label;
    TextBlock size_label;
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 设置尺寸约束和 Margin
    // ════════════════════════════════════════════════════════════════════════
    name_label.set_margin(Thickness{10, 5, 10, 5});
    name_label.set_horizontal_alignment(HorizontalAlignment::Left);
    
    age_label.set_margin(Thickness{10, 5, 10, 5});
    age_label.set_horizontal_alignment(HorizontalAlignment::Left);
    
    size_label.set_margin(Thickness{10, 5, 10, 5});
    size_label.set_horizontal_alignment(HorizontalAlignment::Left);
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 建立数据绑定
    // ════════════════════════════════════════════════════════════════════════
    
    // 简单绑定
    name_label.set_binding(TextBlock::TextProperty, "UserName");
    age_label.set_binding(TextBlock::TextProperty, "Age");
    
    // 带值转换器的绑定
    size_label.set_binding(TextBlock::TextProperty, ui::Binding{
        .prop_name  = "FileSize",
        .converter  = &bytes_to_human,
        .conv_param = core::Variant{ "MB" },
        .fallback   = core::Variant{ "N/A" },
    });
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 构建元素树
    // ════════════════════════════════════════════════════════════════════════
    panel.add_child(&name_label);
    panel.add_child(&age_label);
    panel.add_child(&size_label);
    
    // ════════════════════════════════════════════════════════════════════════
    // 6. 布局：Measure 阶段
    // ════════════════════════════════════════════════════════════════════════
    Size available_size{400, 300};
    panel.measure(available_size);
    
    Size panel_desired = panel.desired_size();
    std::cout << "面板期望尺寸: " << panel_desired.width << "x" << panel_desired.height << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 7. 布局：Arrange 阶段
    // ════════════════════════════════════════════════════════════════════════
    Rect panel_rect{0, 0, 400, 300};
    panel.arrange(panel_rect);
    
    // 检查子节点布局
    Rect name_bounds = name_label.bounds_rect();
    Rect age_bounds = age_label.bounds_rect();
    Rect size_bounds = size_label.bounds_rect();
    
    std::cout << "name_label 位置: x=" << name_bounds.x << ", y=" << name_bounds.y << std::endl;
    std::cout << "age_label 位置: x=" << age_bounds.x << ", y=" << age_bounds.y << std::endl;
    std::cout << "size_label 位置: x=" << size_bounds.x << ", y=" << size_bounds.y << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 8. 验证绑定
    // ════════════════════════════════════════════════════════════════════════
    std::cout << "name_label.text: " << name_label.text() << std::endl;
    // 输出：name_label.text: Admin
    
    std::cout << "age_label.text: " << age_label.text() << std::endl;
    // 输出：age_label.text: 25
    
    std::cout << "size_label.text: " << size_label.text() << std::endl;
    // 输出：size_label.text: 1.0 MB
    
    // ════════════════════════════════════════════════════════════════════════
    // 9. 修改 ViewModel 属性，验证自动同步
    // ════════════════════════════════════════════════════════════════════════
    view_model.set_UserName("John");
    view_model.set_Age(30);
    view_model.set_FileSize(2097152);  // 2 MB
    
    std::cout << "修改后 name_label.text: " << name_label.text() << std::endl;
    // 输出：修改后 name_label.text: John
    
    std::cout << "修改后 age_label.text: " << age_label.text() << std::endl;
    // 输出：修改后 age_label.text: 30
    
    std::cout << "修改后 size_label.text: " << size_label.text() << std::endl;
    // 输出：修改后 size_label.text: 2.0 MB
    
    return 0;
}
```

---

## 总结

`FrameworkElement` 是 `mine.ui.visual` 模块的具有 Margin、尺寸约束和对齐方式的布局元素基类，具备：

1. **尺寸约束**：Width/Height/MinWidth/MaxWidth/MinHeight/MaxHeight（依赖属性）
2. **Margin**：外边距（`math::Thickness`）
3. **对齐方式**：HorizontalAlignment / VerticalAlignment（依赖属性）
4. **数据绑定**：set_binding（WPF 风格）、bind_property（元素间绑定）、clear_all_bindings
5. **两阶段布局**：measure_override / arrange_override（子类覆盖点）
6. **WPF 风格布局协议**：Measure（减去 Margin，应用约束 → measure_override → 加回 Margin）→ Arrange（减去 Margin，应用对齐 → arrange_override）

**使用建议**：
- 覆盖 **measure_override / arrange_override** 而非 on_measure / on_arrange
- 使用 **NaN** 表示自动尺寸
- 设置 **Margin** 而非手动计算位置
- 使用 **set_binding** 而非手动同步
- 元素间绑定使用 **bind_property**
- 内容变更时触发 **invalidate_measure**
- 对齐方式变更时触发 **invalidate_arrange**
- 绑定属性名确保与 **MINE_OBSERVABLE** 宏名称完全一致
