# mine.ui.visual —— 可视化树与控件基类模块

## 模块概述

`mine.ui.visual` 定义 UI 框架的可视化树节点体系：从最底层的 `Visual`（渲染/变换/裁剪）到 `UIElement`（布局/命中测试）再到 `Control`（样式/视觉状态/内部元素）。这是所有 UI 控件的直接基类模块。

| 类型 | 层级 | 核心职责 |
|------|------|---------|
| `Visual` | 第1层 | 可视化树节点、变换、裁剪、渲染管线接入、事件处理器注册 |
| `UIElement` | 第2层 | 布局边界（bounds/desired）、命中测试、命中穿透 |
| `FrameworkElement` | 第3层 | Margin、Width/Height 约束、对齐方式（两遗布局） |
| `Control` | 第4层 | VSM 管理、内部子元素管理、视觉状态枚举 |

**依赖**：`mine.core`、`mine.math`、`mine.ui.property`、`mine.ui.event`、`mine.ui.style`。

---

## 1. Visual —— 可视化树节点基类

**文件**：`<mine/ui/visual/Visual.h>`

所有可见元素的最终基类，负责可视化树管理、渲染裁剪、事件处理器。

### 继承链

```
DependencyObject (property)
  └─ Visual (visual tree, render, event handler)
      └─ UIElement (layout, hit test)
          └─ FrameworkElement (margin, alignment)
              └─ Control (VSM, inner element)
```

### 类摘要

```cpp
namespace mine::ui {

// 依赖属性
static const DependencyProperty& OpacityProperty;       // float [0,1]
static const DependencyProperty& VisibilityProperty;    // Visibility 枚举

class Visual : public DependencyObject, public IRoutedEventTarget {
public:
    Visual();
    ~Visual() override;

    // ── 可视化树 ────────────────────────────────────────────────────
    Visual*  parent()           const noexcept;
    uint32_t child_count()      const noexcept;
    Visual*  child_at(uint32_t) const noexcept;
    void     add_child(Visual* child);
    void     remove_child(Visual* child);
    void     remove_all_children();

    // ── 变换 ────────────────────────────────────────────────────────
    const Transform2D& render_transform() const noexcept;
    void set_render_transform(const Transform2D& t);

    // ── 裁剪 ────────────────────────────────────────────────────────
    bool has_clip_rect()              const noexcept;
    Rect clip_rect()                  const noexcept;
    void set_clip_rect(Rect rect);
    void clear_clip_rect();

    bool has_clip_rounded_rect()                  const noexcept;
    RoundedRect clip_rounded_rect()               const noexcept;
    void set_clip_rounded_rect(RoundedRect rrect);
    void clear_clip_rounded_rect();

    bool has_clip_complex_rounded_rect()              const noexcept;
    ComplexRoundedRect clip_complex_rounded_rect()    const noexcept;
    void set_clip_complex_rounded_rect(ComplexRoundedRect rrect);
    void clear_clip_complex_rounded_rect();

    // ── 渲染 ────────────────────────────────────────────────────────
    void render_to_canvas(Canvas& canvas);

    // ── 脏区 ────────────────────────────────────────────────────────
    bool is_render_dirty() const noexcept;
    void invalidate_render() override;

    // ── 事件处理器 ──────────────────────────────────────────────────
    void add_handler(const RoutedEvent& event,
                     RoutedEventHandlerFn fn,
                     void* user_data = nullptr);
    void remove_handler(const RoutedEvent& event,
                        RoutedEventHandlerFn fn,
                        void* user_data = nullptr);

    // IRoutedEventTarget 实现
    IRoutedEventTarget* parent_target() const noexcept override;
    void invoke_handlers(const RoutedEvent&, RoutedEventArgs&) noexcept override;

    // 无 RTTI 类型探查
    virtual UIElement* as_element() noexcept;   // 返回 this（若为 UIElement）或 nullptr

protected:
    virtual void on_render(Canvas& canvas);     // 子类重写实现自绘
    virtual void on_parent_changed(Visual* old_parent, Visual* new_parent) noexcept;
    virtual void on_property_changed(const DependencyProperty& prop,
                                     const Variant& old_value,
                                     const Variant& new_value) override;
};

} // namespace mine::ui
```

---

## 2. UIElement —— 布局与命中测试

**文件**：`<mine/ui/visual/UIElement.h>`

### 类摘要

```cpp
namespace mine::ui {

class UIElement : public Visual {
public:
    UIElement();
    ~UIElement() override;

    // ── 布局边界 ────────────────────────────────────────────────────
    Rect bounds_rect()   const noexcept;      // 排列矩形（父坐标）
    void set_bounds_rect(Rect rect);
    Size desired_size()  const noexcept;      // 期望尺寸

    // ── 命中测试 ────────────────────────────────────────────────────
    UIElement* hit_test(Point screen_point);    // 递归命中测试（父坐标）
    bool       hit_test_local(Point local_point) const;  // 局部命中（子类可覆盖）

    // ── 命中穿透 ────────────────────────────────────────────────────
    void set_hit_transparent(bool transparent) noexcept;
    bool is_hit_transparent() const noexcept;

    // ── 可聚焦 ──────────────────────────────────────────────────────
    void set_focusable(bool focusable) noexcept;
    bool is_focusable() const noexcept;

    // ── 布局入口 ────────────────────────────────────────────────────
    void measure(Size available);
    void arrange(Rect slot);

    // ── 失效标记 ────────────────────────────────────────────────────
    void invalidate_measure() override;
    void invalidate_arrange() override;

    // 无 RTTI
    UIElement* as_element() noexcept override;

protected:
    virtual void on_measure(Size available);
    virtual void on_arrange(Rect final_rect);
    void set_desired_size(Size size) noexcept;
};

} // namespace mine::ui
```

### 命中测试流程

```
hit_test(screen_point)
  ├─ 逆变换 screen_point → local_point（render_transform.inverted）
  ├─ 裁剪检测（clip_rect / clip_rounded_rect / clip_complex_rounded_rect）
  ├─ 逆序遍历子节点
  │   ├─ 跳过 is_hit_transparent() == true 的子节点
  │   └─ 递归 child->hit_test(local_point) → 命中则返回
  └─ hit_test_local(local_point) → 命中则返回 this
```

### 命中穿透的使用

```cpp
// 控件内部实现元素不应作为命中目标
layout_root_->set_hit_transparent(true);
// 确保鼠标事件派发给 CheckBox 而非其内部的 StackPanel
```

---

## 3. FrameworkElement —— 两遗布局

**文件**：`<mine/ui/visual/FrameworkElement.h>`

在 `UIElement` 基础上添加 Margin、Width/Height 约束、HorizontalAlignment/VerticalAlignment（WPF 两遗布局协议）。

### 类摘要

```cpp
namespace mine::ui {

enum class HorizontalAlignment : uint8_t { Stretch, Left, Center, Right };
enum class VerticalAlignment   : uint8_t { Stretch, Top, Center, Bottom };

class FrameworkElement : public UIElement {
public:
    // ── 边距 ────────────────────────────────────────────────────────
    Thickness margin() const noexcept;
    void set_margin(Thickness m);

    // ── 尺寸约束 ────────────────────────────────────────────────────
    float width()  const noexcept;
    float height() const noexcept;
    void set_width(float w);
    void set_height(float h);

    // ── 对齐 ────────────────────────────────────────────────────────
    HorizontalAlignment horizontal_alignment() const noexcept;
    VerticalAlignment   vertical_alignment()   const noexcept;
    void set_horizontal_alignment(HorizontalAlignment ha);
    void set_vertical_alignment(VerticalAlignment va);

protected:
    // 覆盖 arrange：处理 Margin + 对齐后委托 arrange_override
    void on_arrange(Rect final_rect) override;
    virtual Size arrange_override(Size final_size);  // 子类覆盖
    virtual Size measure_override(Size available);   // 子类覆盖
};

} // namespace mine::ui
```

---

## 4. Control —— 控件基类

**文件**：`<mine/ui/visual/Control.h>`

### 视觉状态枚举

```cpp
enum class ControlVisualState : uint8_t {
    Normal   = 0,
    Hovered  = 1,
    Pressed  = 2,
    Focused  = 3,
    Disabled = 4,
};
```

### 类摘要

```cpp
namespace mine::ui {

class Control : public FrameworkElement {
public:
    Control();
    ~Control() override;

    // ── VSM ──────────────────────────────────────────────────────────
    void set_visual_state_manager(style::VisualStateManager vsm) noexcept;
    style::VisualStateManager*       vsm() noexcept;
    const style::VisualStateManager* vsm() const noexcept;

    // ── 视觉状态 ────────────────────────────────────────────────────
    ControlVisualState visual_state() const noexcept;
    void update_visual_state();  // compute → go_to_state → on_visual_state_changed

    // ── 样式槽位 ────────────────────────────────────────────────────
    void set_style_slot(StringView slot);
    StringView style_slot() const noexcept;

protected:
    // ── 内部子元素 ──────────────────────────────────────────────────
    void set_inner_element(UIElement* root) noexcept;
    void set_inner_element(OwnedPtr<UIElement> root) noexcept;  // 接管所有权
    UIElement* inner_element() const noexcept;

    // ── 布局覆盖 ────────────────────────────────────────────────────
    Size measure_override(Size available) override;   // 委托给 inner_element
    Size arrange_override(Size final_size) override;   // 排列 inner_element

    // ── 状态覆盖 ────────────────────────────────────────────────────
    virtual ControlVisualState compute_visual_state() const;       // 默认 Normal
    virtual StringView          compute_state_name()  const noexcept;  // 枚举→字符串
    virtual void on_visual_state_changed(ControlVisualState old,
                                         ControlVisualState new_state);
};

} // namespace mine::ui
```

### update_visual_state() 内部流程

```
update_visual_state()
  ├─ next = compute_visual_state()               // 子类计算新状态
  ├─ vsm()->go_to_state(compute_state_name())    // 驱动 VSM 状态机
  ├─ if (next == current) return
  ├─ old = visual_state_; visual_state_ = next
  └─ on_visual_state_changed(old, next)          // 子类钩子
```

---

## 5. 自定义控件模式

继承 `Control` 并组合现有控件基元（`Border`、`TextBlock`、`StackPanel` 等）是 MineFramework 推荐的自定义控件方式：

```cpp
class MyControl : public Control {
public:
    MyControl() {
        auto border = make_owned<Border>();
        auto text   = make_owned<TextBlock>();
        border->set_child(text.get());
        // ...
        set_inner_element(std::move(border));
    }

protected:
    void on_measure(Size available) override {
        // 自定义测量逻辑
    }
};
```

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.ui.controls` | `Button`、`CheckBox` 等控件继承 `Control` |
| `mine.ui.layout` | `StackPanel` 等布局面板继承 `UIElement`/`FrameworkElement` |
| `mine.ui.input` | `InputRouter` 通过 `hit_test` 确定事件目标 |
| `mine.paint` | `render_to_canvas` 将可视化树渲染到 `Canvas` |
