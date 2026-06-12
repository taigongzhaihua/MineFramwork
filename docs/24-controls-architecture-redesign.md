# 24 — 控件架构重新设计（Control Architecture Redesign）

> **状态**：设计提案
> **前置阅读**：
> - [21-controls-architecture.md](21-controls-architecture.md) — 当前控件架构
> - [22-appearance-architecture.md](22-appearance-architecture.md) — 外观架构（基元+组合+绑定）
> - [21-control-authoring.md](21-control-authoring.md) — 控件开发规范
>
> 本文档提出对 `mine.ui.controls` 控件继承体系的全面重新设计，
> 解决当前架构中积累的若干结构性问题。

---

## 24.1 现状分析：当前架构存在的问题

### 24.1.1 当前控件继承树

```
DependencyObject
└── Visual
    └── UIElement
        └── FrameworkElement
            ├── Control (mine.ui.visual) ────────────────────────── 基础控件
            │   ├── ContentControl (mine.ui.controls) ───────────── "有内容"控件
            │   │   ├── Button         ← 唯一使用 ContentControl 的控件
            │   │   └── UserControl
            │   │       └── Page
            │   ├── Border            ← 基元控件
            │   ├── TextBlock         ← 基元控件
            │   ├── TextBox           ← 复合控件
            │   ├── CheckBox          ← 复合控件
            │   └── ContentPresenter  ← 辅助元素
            └── Panel (mine.ui.layout)
                ├── StackPanel
                └── Grid
```

### 24.1.2 问题清单

#### 问题 1：ContentControl 语义模糊、单点使用

`ContentControl` 的设计意图是「有单一可替换内容的控件」。但实际情况：

| 控件 | 继承 | 实际内容模型 | 问题 |
|------|------|-------------|------|
| `Button` | `ContentControl` | 单内容（文字或元素） | ✅ 匹配 |
| `CheckBox` | `Control` | 固定双元素（图标+文字） | ❌ 想加图标只能用 text 拼 |
| `TextBox` | `Control` | 固定结构（编辑区） | ❌ 不能放入富文本元素 |
| `Border` | `Control` | 单子元素 | ❓ 没有 ContentProperty |

**核心矛盾**：ContentControl 只有 Button 在用，而 CheckBox/TextBox 这些"明明也有内容"的控件却被排除在外。

#### 问题 2：Control 层次职责过重

`Control`（定义在 `mine.ui.visual`）集成了太多职责：

```
Control
├── 视觉状态机（VSM / ControlVisualState）
├── 样式槽位（style_slot_）
├── 内部子元素管理（inner_element / set_inner_element）
├── 布局委托（measure_override / arrange_override）
├── 视觉状态计算（compute_visual_state / compute_state_name）
└── 渲染入口（on_render）  ← 继承自 Visual
```

基元控件（`Border`、`TextBlock`）不需要 VSM 和样式槽位，但被迫继承了这些。复合控件（`Button`、`CheckBox`）需要 VSM，但 content 管理又被抽到了 `ContentControl`。

#### 问题 3：ContentPresenter 与 ContentControl 功能重叠

| 类型 | ContentProperty | ForegroundProperty | PaddingProperty | set_content() |
|------|:---:|:---:|:---:|:---:|
| `ContentControl` | ✅ | ❌ | ❌ | ✅ |
| `ContentPresenter` | ✅ | ✅ | ✅ | ❌ |

两者都有 `ContentProperty`，但不同的子属性分散在两个类中。Button 需要同时用这两个类——ContentControl 管理 content 语义，ContentPresenter 负责渲染。

#### 问题 4：CheckBox 的内容模型不一致

```cpp
// Button：可以用任意内容
button.set_content("点击我");           // ✅ 文字
button.set_content(someImageElement);   // ✅ 元素

// CheckBox：只能用 text 字符串
checkbox.set_text("同意条款");          // ✅ 文字
// checkbox.set_content(richElement);  // ❌ 不存在！无法嵌入图标
```

这导致 CheckBox 无法支持「图标+文字」「链接文字」等现代 UI 需求。

#### 问题 5：控件分层混乱——"基元"和"复合"在同一层级

外观架构文档（22-appearance-architecture.md）明确区分了三类元素：

| 类型 | 代表 | 当前继承 | 
|------|------|---------|
| 基元 Primitive | Border, TextBlock | 直接继承 Control |
| 复合 Composite | Button, CheckBox, TextBox | 直接继承 Control 或 ContentControl |
| 容器 Panel | StackPanel, Grid | 继承 Panel → FrameworkElement |

三类元素在继承树中没有对应的抽象层，混在同一级别。

#### 问题 6：Button 与 CheckBox 的状态机结构重复

Button 和 CheckBox 各自实现了：
- 交互 VSM（Normal / Hovered / Pressed / Disabled）
- State Layer 蒙版渐变动画
- 边框在交互状态下的颜色变化

但 CheckBox 额外还有"勾选组"VSM（Checked / Unchecked），Button 有 Ripple 效果。两者在交互态这块几乎完全重复。

#### 问题 7：可点击控件缺乏公共抽象

`Button`、`CheckBox` 都有：
- `is_pressed_` / `is_hovered_` 状态标志
- `on_mouse_enter` / `on_mouse_leave` / `on_mouse_down` / `on_mouse_up` 路由
- `compute_visual_state()` 的相同逻辑

但没有一个公共的「可交互控件」基类来承载这些。

---

## 24.2 设计目标

1. **继承树清晰分层**：基元 → 交互 → 内容 → 复合，每层有明确职责
2. **统一内容模型**：所有带内容的控件都能接受文字或任意 UIElement
3. **消除重复**：交互状态管理、VSM 配置、State Layer 等提取为可复用组件
4. **保持向下兼容**：现有 Button/Border/TextBlock API 尽量不变
5. **符合外观架构**：与 22-appearance-architecture.md 的三类元素模型一致

---

## 24.3 新架构设计

### 24.3.1 新继承树

```
DependencyObject
└── Visual
    └── UIElement
        └── FrameworkElement
            │
            ├── Panel (mine.ui.layout) ─────────────── 容器基类
            │   ├── StackPanel
            │   ├── Grid
            │   ├── Canvas
            │   └── ...
            │
            └── Control (mine.ui.visual) ───────────── 控件基类（精简）
                │
                ├── Primitive (mine.ui.controls) ───── 基元控件（新增！）
                │   ├── Border
                │   ├── TextBlock
                │   ├── Rectangle
                │   └── Image
                │
                └── InteractableControl (mine.ui.controls) ─ 可交互控件（新增！）
                    │
                    ├── ContentControl ─────────────── 内容控件
                    │   ├── Button
                    │   ├── CheckBox      ← 改为继承 ContentControl！
                    │   ├── RadioButton
                    │   └── Label
                    │
                    └── ItemsControl ───────────────── 集合控件
                        ├── ListBox
                        ├── ComboBox
                        └── TabControl
```

### 24.3.2 各层详细定义

#### Layer 0：Control（精简后）

**职责**：所有控件的**最小公共基础**——仅保留布局委托和渲染入口。

```cpp
class Control : public FrameworkElement {
public:
    // ── 布局委托 ──────────────────────────────────────────────
    // measure_override / arrange_override：委托给子元素树
    // （仅保留布局能力，移除 VSM / 样式槽位 / inner_element）

    // ── 渲染入口 ──────────────────────────────────────────────
    // on_render()：空实现，子类覆盖

    // ── 键盘焦点 ──────────────────────────────────────────────
    // is_focused() / focus() / unfocus()
};
```

**移除内容**（下沉到 InteractableControl）：
- ~~`ControlVisualState` / `visual_state()` / `update_visual_state()`~~
- ~~`VisualStateManager` / `vsm()` / `set_visual_state_manager()`~~
- ~~`style_slot_` / `set_style_slot()`~~
- ~~`inner_element()` / `set_inner_element()`~~
- ~~`compute_visual_state()` / `compute_state_name()`~~

#### Layer 1a：Primitive（新增基元控件基类）

**职责**：叶子绘制控件——直接操作 Canvas，不包含交互逻辑。

```cpp
class Primitive : public Control {
public:
    // Primitive 无 VSM、无交互状态、无子元素管理
    // 子类只需覆盖 on_render() 直接绘制
    
    // 提供「绘制缓存失效」标记
    void invalidate_appearance();
};
```

**子类**：`Border`、`TextBlock`、`Rectangle`、`Image`

**设计要点**：
- 基元是系统中**唯一直接调用 Canvas API 的层**
- 所有外观参数从 DP 读取
- 不持有交互状态，不响应鼠标事件（由上层复合控件处理）

#### Layer 1b：InteractableControl（新增可交互控件基类）

**职责**：所有可交互控件的公共基础——交互状态管理、**多状态组** VSM 协调、State Layer。

**核心设计**：不同于单一 VSM，InteractableControl 引入 **StateGroupManager**，
管理**多个独立的状态组**——每个组对应一种正交的状态维度。

```cpp
// ── 状态组管理器 ──────────────────────────────────────────────

class StateGroupManager {
public:
    /// 注册一个状态组
    /// @param group_name  组名（如 "Common"、"Focus"、"Check"）
    /// @param vsm         该组的 VisualStateManager（移动所有权）
    /// @param conflict_dps 与该组动画冲突的 DP 列表（切换此组时停止这些 DP 的其他组动画）
    void add_group(core::StringView group_name,
                   style::VisualStateManager vsm,
                   core::Span<const DependencyProperty*> conflict_dps = {});

    /// 切换到指定组的指定状态
    void go_to_state(core::StringView group_name,
                     core::StringView state_name,
                     bool animate = true);

    /// 获取某组的当前状态
    [[nodiscard]] core::StringView current_state(core::StringView group_name) const;

    /// 推进所有组的动画
    void tick_animations(float dt_seconds);

    /// 不可拷贝/移动
    StateGroupManager(const StateGroupManager&) = delete;
    StateGroupManager& operator=(const StateGroupManager&) = delete;
};

// ── InteractableControl ───────────────────────────────────────

class InteractableControl : public Control {
public:
    // ── 交互状态 ──────────────────────────────────────────────
    [[nodiscard]] bool is_hovered() const noexcept;
    [[nodiscard]] bool is_pressed() const noexcept;
    [[nodiscard]] bool is_disabled() const noexcept;
    [[nodiscard]] bool is_focused() const noexcept;
    void set_disabled(bool disabled);

    // ── 多状态组管理 ──────────────────────────────────────────
    /// 获取状态组管理器（子类用于注册自定义状态组）
    [[nodiscard]] StateGroupManager& state_groups() noexcept;

    /// 触发交互状态刷新（鼠标事件等调用）
    void update_interaction_state();

    // ── 内部元素管理（组合式装配）──────────────────────────────
    void set_inner_element(core::OwnedPtr<UIElement> root);
    [[nodiscard]] UIElement* inner_element() const noexcept;

    // ── State Layer（交互蒙版）─────────────────────────────────
    static const DependencyProperty& StateLayerBrushProperty;
    // 由 Common 状态组驱动的半透明白色蒙版

protected:
    // ── 鼠标事件路由桩 ────────────────────────────────────────
    static void on_mouse_enter_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_leave_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_down_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_up_router(void*, RoutedEventArgs&, void*);

    // ── 虚方法（子类可覆盖以自定义状态计算）───────────────────
    [[nodiscard]] virtual core::StringView compute_common_state() const;
    // 默认：Disabled > Pressed > Hovered > Normal

    [[nodiscard]] virtual core::StringView compute_focus_state() const;
    // 默认：Focused / Unfocused

    virtual void on_interaction_state_changed();
};
```

**InteractableControl 构造函数内置的默认状态组**：

```cpp
InteractableControl::InteractableControl() {
    // ── 组 1："Common" — 交互状态组（所有可交互控件通用）────────────

    style::VisualStateManager common_vsm{*this};
    common_vsm.define_state("Normal");
    common_vsm.define_state("Hovered");
    common_vsm.define_state("Pressed");
    common_vsm.define_state("Disabled");

    // 默认过渡：Hovered 120ms / Pressed 60ms
    common_vsm.add_transition("*", "Hovered", [](auto& sb) {
        sb.animate_dp(..., StateLayerBrushProperty, 120ms, QuadEaseOut);
    });
    // ... 其他过渡

    // 注册到管理器
    state_groups_.add_group("Common", std::move(common_vsm));

    // ── 组 2："Focus" — 焦点状态组（所有可交互控件通用）────────────

    style::VisualStateManager focus_vsm{*this};
    focus_vsm.define_state("Unfocused");
    focus_vsm.define_state("Focused");
    // Focused: 显示焦点指示器（通常由外部 FocusManager 管理）

    state_groups_.add_group("Focus", std::move(focus_vsm));

    // ── 注册鼠标事件 ────────────────────────────────────────────
    add_handler(input::MouseEnterEvent(), &on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &on_mouse_leave_router, this);
    add_handler(input::MouseDownEvent(),  &on_mouse_down_router,  this);
    add_handler(input::MouseUpEvent(),    &on_mouse_up_router,    this);
}
```

**子类如何添加自定义状态组**：

```cpp
// ── CheckBox 添加 "Check" 状态组 ──────────────────────────────

CheckBox::CheckBox() {
    // 基类已注册 Common 和 Focus 两个状态组
    // 无需重复写交互状态逻辑！

    // ── 组 3："Check" — 勾选状态组 ────────────────────────────────
    style::VisualStateManager check_vsm{*this};
    check_vsm.define_state("Unchecked");
    check_vsm.define_state("Checked");

    check_vsm.add_transition("*", "Checked", [this](auto& sb) {
        sb.animate_dp(*this, IconBackgroundProperty,  120ms, QuadEaseOut);
        sb.animate_dp(*this, IconBorderBrushProperty, 120ms, QuadEaseOut);
        sb.animate_dp(*this, CheckMarkBrushProperty,   120ms, QuadEaseOut);
    });
    check_vsm.add_transition("*", "Unchecked", [this](auto& sb) {
        sb.animate_dp(*this, IconBackgroundProperty,  100ms, QuadEaseOut);
        // ...
    });

    // 注册到管理器，声明与 Common 组在 IconBorderBrushProperty 上有动画冲突
    const DependencyProperty* conflicts[] = { &IconBorderBrushProperty };
    state_groups().add_group("Check", std::move(check_vsm),
                              Span<const DependencyProperty*>(conflicts, 1));

    // 初始勾选状态
    state_groups().go_to_state("Check",
        is_checked() ? "Checked" : "Unchecked", /*animate=*/false);
}
```

**ToggleButton 同理——只需加一个 "Check" 组**：

```cpp
ToggleButton::ToggleButton() {
    // 继承自 InteractableControl：
    //   ✅ Common 组（Normal/Hovered/Pressed/Disabled）
    //   ✅ Focus 组（Focused/Unfocused）

    // 只需添加：
    style::VisualStateManager toggle_vsm{*this};
    toggle_vsm.define_state("Unchecked");
    toggle_vsm.define_state("Checked");
    toggle_vsm.define_state("Indeterminate");  // 三态
    // ...

    state_groups().add_group("Toggle", std::move(toggle_vsm));
}
```

**这解决了什么**：

| 旧方式（每个控件手写） | 新方式（InteractableControl 内置） |
|------------------------|-------------------------------------|
| Button 手写交互 VSM | ✅ 继承 Common + Focus 组 |
| CheckBox 手写交互 VSM **+** 勾选 VSM | ✅ 继承 Common + Focus，再加 Check 组 |
| ToggleButton 手写交互 VSM **+** 勾选 VSM | ✅ 继承 Common + Focus，再加 Toggle 组 |
| 两个 VSM 手动协调（停止对方动画） | ✅ `conflict_dps` 声明自动协调 |
| 重复 ~200 行交互代码 × N 个控件 | ✅ 一次编写，全部继承 |

#### Layer 2：ContentControl（重定义）

**职责**：拥有**单一可替换内容**的控件。

```cpp
class ContentControl : public InteractableControl {
public:
    // ── 内容属性 ──────────────────────────────────────────────
    static const DependencyProperty& ContentProperty;
    // Variant: InlineString（文字）或 UIElement*（元素）

    static const DependencyProperty& ForegroundProperty;
    static const DependencyProperty& FontSizeProperty;
    static const DependencyProperty& PaddingProperty;

    // ── 内容接口 ──────────────────────────────────────────────
    void set_content(core::StringView text);
    void set_content(UIElement* element);
    void set_content(core::OwnedPtr<UIElement> element);  // 转移所有权
    [[nodiscard]] core::Variant content() const noexcept;

    // ── 外观访问器 ────────────────────────────────────────────
    void set_foreground(paint::Brush brush);
    void set_font_size(float size_px);
    void set_padding(math::Thickness padding);

protected:
    virtual void on_content_changed(const core::Variant& old_val,
                                    const core::Variant& new_val);
};
```

**设计要点**：
- **合并 ContentPresenter 的功能**：ContentControl 内部直接管理 ContentPresenter 的渲染逻辑
- **统一内容接口**：`set_content(StringView)` 和 `set_content(UIElement*)` 语义清晰
- **CheckBox 改为继承 ContentControl**：`checkbox.set_content("同意条款")` — 文字内容；`checkbox.set_content(richElement)` — 富文本内容
- **新增外观属性**：Foreground/FontSize/Padding 上提至此层，所有内容控件共享

#### Layer 2b：ItemsControl（新增集合控件基类）

**职责**：拥有**多个子项目**的控件。

```cpp
class ItemsControl : public InteractableControl {
public:
    static const DependencyProperty& ItemsSourceProperty;

    void set_items_source(core::Span<core::Variant> items);
    [[nodiscard]] core::Span<const core::Variant> items() const noexcept;

    // 虚拟化支持
    void set_virtualizing(bool enabled);

protected:
    virtual UIElement* create_item_container();
    virtual void prepare_item_container(UIElement* container, size_t index);
};
```

**子类**：`ListBox`、`ComboBox`、`TabControl`、`TreeView`

### 24.3.3 现有控件迁移方案

| 控件 | 当前继承 | 新继承 | 迁移变更 |
|------|---------|--------|----------|
| `Border` | `Control` | `Primitive` | 移除 VSM 相关代码（本就不需要） |
| `TextBlock` | `Control` | `Primitive` | 移除 VSM 相关代码 |
| `Button` | `ContentControl` | `ContentControl` | 交互逻辑下沉到 `InteractableControl` |
| `CheckBox` | `Control` | `ContentControl` | **重大变更**：可接内容、交互逻辑下沉 |
| `TextBox` | `Control` | `ContentControl` | 文本编辑内容纳入 ContentProperty |
| `ContentPresenter` | `Control` | **废弃** | 功能合并到 ContentControl |
| `Page` | `UserControl` | `UserControl` | 无变化 |
| `StackPanel` | `Panel` | `Panel` | 无变化 |

### 24.3.4 迁移后的 CheckBox 示例

```cpp
// ── 迁移前 ────────────────────────────────────────────────────
class CheckBox : public Control {
    void set_text(StringView text);         // 专用 text 属性
    // 无法 set_content(UIElement*)
};

// ── 迁移后 ────────────────────────────────────────────────────
class CheckBox : public ContentControl {
    // 继承自 ContentControl:
    //   set_content("同意条款")       // ✅ 文字（等价于旧 set_text）
    //   set_content(richElement)     // ✅ 任意元素
    //   set_foreground(brush)
    //   set_font_size(14)
    //   set_padding(padding)

    // 继承自 InteractableControl:
    //   State Layer 蒙版自动管理
    //   Common 交互状态组（Normal/Hovered/Pressed/Disabled）自动配置
    //   鼠标事件自动路由到 StateGroupManager

    // CheckBox 自身只需关注：
    //   1. "Check" 状态组（Checked/Unchecked，通过 state_groups().add_group() 注册）
    //   2. 勾号绘制
    //   3. IsCheckedProperty
};
```

### 24.3.5 迁移后的 Button 示例

```cpp
// ── 迁移前 ────────────────────────────────────────────────────
class Button : public ContentControl {
    // 手写所有交互逻辑：
    //   on_mouse_enter / on_mouse_leave / on_mouse_down / on_mouse_up
    //   is_pressed_ / is_hovered_ 成员变量
    //   compute_visual_state() 手动实现
    //   VSM 配置（三个过渡）
    //   State Layer 蒙版手动管理
    //   Ripple 效果手动实现

// ── 迁移后 ────────────────────────────────────────────────────
class Button : public ContentControl {
    // 继承自 InteractableControl:
    //   ✅ 交互状态自动管理（is_hovered/is_pressed）
    //   ✅ Common 状态组 + Focus 状态组自动配置
    //   ✅ State Layer 蒙版自动驱动
    //   ✅ 鼠标事件自动路由

    // 继承自 ContentControl:
    //   ✅ 内容管理（文字/元素）
    //   ✅ 前景色/字号/内边距

    // Button 自身只需关注：
    //   1. Click 事件
    //   2. Ripple 效果（如果需要）
    //   3. 外观个性化（不影响架构）
};
```

---

## 24.4 分层职责总表

| 层 | 类型 | 拥有 | 不拥有 | 子类数 |
|----|------|------|--------|--------|
| **L0** | `Control` | 布局委托、渲染入口、焦点 | VSM、内容、交互 | ~20 |
| **L1a** | `Primitive` | 直接绘制（Canvas API） | 交互、VSM | ~6 |
| **L1b** | `InteractableControl` | 交互状态、VSM、State Layer、inner_element | 内容语义 | ~10 |
| **L2a** | `ContentControl` | 单内容(Content)、前景色/字号/内边距 | — | ~15 |
| **L2b** | `ItemsControl` | 多项目、虚拟化、选择 | 内容语义 | ~5 |
| **L3** | 具体控件 | 自身语义（勾选、文本编辑、命令…） | — | — |

---

## 24.5 InteractableControl + StateGroupManager 的复用价值

这是本次重构**性价比最高**的改动。当前 Button 和 CheckBox 的交互逻辑几乎完全相同（~200 行重复代码），提取到 InteractableControl 后：

```cpp
// InteractableControl 内置的默认行为：
InteractableControl::InteractableControl() {
    // 1. 注册鼠标事件路由
    input::InputRouter::register_mouse_target(this,
        &InteractableControl::on_mouse_enter_router,
        &InteractableControl::on_mouse_leave_router,
        // ...
    );

    // 2. 配置 Common + Focus 两个默认状态组（子类可覆盖过渡参数）
    // Common 状态组*this};
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Disabled");
    // 默认过渡：120ms hover / 60ms press
    // ...

        state_groups_.add_group("Common", std::move(vsm));

    // 2. 配置 Focus 状态组
    style::VisualStateManager focus_vsm{*this};
    focus_vsm.define_state("Unfocused");
    focus_vsm.define_state("Focused");
    state_groups_.add_group("Focus", std::move(focus_vsm));

    // 3. 配置 State Layer 蒙版（Common 组驱动）
    vsm.set_state_brush("Hovered", StateLayerBrushProperty,
        paint::Brush::solid(Color{1,1,1,0.08f}));
    vsm.set_state_brush("Pressed", StateLayerBrushProperty,
        paint::Brush::solid(Color{1,1,1,0.12f}));

    set_visual_state_manager(std::move(vsm));
}

// 子类如有特殊需求，可在构造函数末尾覆盖：
Button::Button() {
    // 基类 InteractableControl 已配置好交互 VSM
    // ...

    // Button 特有：添加 Ripple 效果
    enable_ripple(true);

    // 修改过渡时长（如有需要）
    auto* vsm = this->vsm();
    vsm->set_transition_duration("Normal", "Hovered", 150.0f);
}
```

---

## 24.6 迁移计划

### 阶段 1：提取 InteractableControl（低风险）

1. 新建 `InteractableControl` 类（`src/mine/ui/controls/`）
2. 将 Control 中的 VSM/交互状态/inner_element 移动到 InteractableControl
3. Control 精简为纯布局委托 + 渲染入口 + 焦点
4. Button 改为继承 InteractableControl，删除重复的交互代码
5. **验证**：Button 行为不变，所有现有测试通过

### 阶段 2：新增 Primitive 层（低风险）

1. 新建 `Primitive` 类
2. Border、TextBlock 改为继承 Primitive
3. **验证**：外观渲染不变，所有现有测试通过

### 阶段 3：重构 ContentControl（中风险）

1. 合并 ContentPresenter 功能到 ContentControl
2. ContentPresenter 标记为 deprecated
3. Button 内部的 ContentPresenter 替换为 ContentControl 内置逻辑
4. **验证**：Button 文字/元素内容渲染不变

### 阶段 4：CheckBox 迁移到 ContentControl（中风险）

1. CheckBox 改为继承 ContentControl
2. `set_text()` 改为 `set_content()` 的包装（或直接移除）
3. 视觉树保持不变
4. **验证**：CheckBox 勾选、文字、交互行为不变

### 阶段 5：新增 ItemsControl（低优先级）

1. 新建 `ItemsControl` 基类
2. 为后续 ListBox/ComboBox 提供基础

---

## 24.7 API 兼容性承诺

| API | 兼容性 | 说明 |
|-----|--------|------|
| `button.set_content(x)` | ✅ 不变 | |
| `checkbox.set_text(x)` | ⚠️ 新增 `set_content(x)` 替代 | `set_text` 保留为 deprecate wrapper |
| `checkbox.set_checked(b)` | ✅ 不变 | |
| `border.set_background(b)` | ✅ 不变 | |
| `textblock.set_text(x)` | ✅ 不变 | |
| `control.set_inner_element(x)` | ⚠️ 移至 InteractableControl | 仅 Button/CheckBox 等交互控件使用 |
| `control.vsm()` | ⚠️ 移至 InteractableControl | 基元控件不再有 VSM |
| `ContentPresenter` | ❌ 废弃 | 功能合并到 ContentControl |

---

## 24.8 附录：与主流框架对比

| 框架 | 内容基类 | 可交互基类 | 基元 | 集合基类 |
|------|---------|-----------|------|---------|
| **WPF** | `ContentControl` | `ButtonBase` | `Shape` 体系 | `ItemsControl` |
| **Avalonia** | `ContentControl` | `InputElement` | `Shape` 体系 | `ItemsControl` |
| **Flutter** | `Widget`（一切皆 Widget） | `StatefulWidget` | `RenderObjectWidget` | `Sliver` 体系 |
| **MineUI（新）** | `ContentControl` | `InteractableControl` | `Primitive` | `ItemsControl` |
| **MineUI（旧）** | `ContentControl`（仅 Button） | 无 | 无（混在 Control） | 无 |

---

## 24.9 StateGroupManager 状态组协调机制

### 24.9.1 为什么需要多状态组

一个可交互控件通常同时处于多个正交的状态维度：

| 状态组 | 示例状态 | 典型控件 |
|--------|---------|----------|
| Common | Normal, Hovered, Pressed, Disabled | Button, CheckBox, TextBox |
| Focus | Focused, Unfocused | 所有可交互控件 |
| Check | Checked, Unchecked, Indeterminate | CheckBox, ToggleButton |
| Selection | Selected, Unselected | ListBoxItem, TreeViewItem |
| Expansion | Expanded, Collapsed | Expander, TreeViewItem |

这些组是**正交的**：一个控件可以同时是 Hovered + Focused + Checked。
单个 VSM 无法表达这种多维状态。

### 24.9.2 conflict_dps 机制

当两个状态组驱动同一个 DP 时（如 CheckBox 的 Common 组和 Check 组都驱动 IconBorderBrushProperty），
需要协调以避免双动画冲突。

``cpp
// 注册 Check 组时声明冲突 DP
const DependencyProperty* conflicts[] = { &IconBorderBrushProperty };
state_groups().add_group("Check", std::move(check_vsm),
                         Span<const DependencyProperty*>(conflicts, 1));
``n
当 Check 组开始播放动画时，StateGroupManager 自动停止 Common 组中
对这些 DP 的动画，确保只有一组在驱动。

## 24.10 决策记录

| 决策 | 理由 |
|------|------|
| 不单独保留 ContentPresenter | 与 ContentControl 功能高度重叠，合并减少概念数量 |
| CheckBox 继承 ContentControl | 统一"有内容"控件的 API 契约；支持富文本内容 |
| 提取 InteractableControl | 消除 Button/CheckBox 间 ~200 行重复交互代码 |
| 新增 Primitive 层 | 让基元控件更轻（无 VSM），外观架构的三分类落到实处 |
| Control 保留 布局委托+焦点 | 这些是所有控件（包括 Panel）都需要的，不应移除 |
