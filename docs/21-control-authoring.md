# 21 — Control 派生类开发范式（Control Authoring）

> 本文档规定 MineFramework 中所有 `Control` 派生类的**标准开发范式**。
> 遵循本规范的控件能够与样式系统、模板系统、状态机系统、动画系统
> 以及数据绑定系统**无缝协作**，同时保持各层职责清晰、可独立替换。
>
> 前置阅读：[09-property-binding.md](09-property-binding.md)、
> [20-style-template.md](20-style-template.md)

---

## 21.1 核心问题与设计目标

一个控件涉及六个关注点：

| 关注点 | 问题 |
|--------|------|
| **属性** | 哪些属性可被样式、绑定、动画覆盖？ |
| **视觉树** | 控件的内部结构如何构建？由谁构建？ |
| **后台 → 模板** | 控件逻辑如何将值同步到模板内部元素？ |
| **状态机 → 模板** | 交互状态变化如何触发模板内的动画？ |
| **样式触发器** | Style 中的状态 setter 何时生效、优先级如何？ |
| **渲染** | 控件外观从哪里读取最终值？ |

**设计目标：三层完全分离、每层职责单一、拼装规则明确。**

---

## 21.2 三层架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│  层 3 — 样式层（mine.ui.style / mmlc 生成）                    │
│                                                                 │
│  Style::apply_fn_()        写入 StyleSetter   优先级 P5=20     │
│  Style::state_setters_     apply_state() 时   优先级 P4=30     │
│                                                                 │
│  ★ 样式层只管"值"，不管"过渡动画"                             │
├─────────────────────────────────────────────────────────────────┤
│  层 2 — 模板层（ControlTemplate build_fn / mmlc 生成）         │
│                                                                 │
│  构建视觉元素树                                                 │
│  bind_template()           宿主 DP → 子元素 DP   P3=40        │
│  VisualStateManager        过渡动画配置（Storyboard）          │
│  vsm.set_style(&style)     连接样式层，使 go_to_state          │
│                            自动调用 style->apply_state()       │
│                                                                 │
│  ★ 模板层管"结构"和"过渡动画"，不管"业务逻辑"               │
├─────────────────────────────────────────────────────────────────┤
│  层 1 — 控件层（Control 子类，手写 C++）                       │
│                                                                 │
│  DependencyProperty        声明外观属性                        │
│  bool is_xxx_              交互状态字段                        │
│  on_mouse_xxx()            更新 bool → update_visual_state()   │
│  compute_state_name()      布尔字段 → 状态名字符串             │
│  on_apply_template()       find_template_child() 缓存指针      │
│                                                                 │
│  ★ 控件层管"语义"和"交互逻辑"，不直接操作 Storyboard         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 21.3 依赖属性值优先级链（复习）

| 优先级 | 符号 | 来源 |
|--------|------|------|
| P1 = 60 | `Animation` | Storyboard 正在插值的运行时值 |
| P2 = 50 | `Local` | 用户代码 `set_xxx()` / MML 内联属性 |
| P3 = 40 | `TemplateBind` | `bind_template()` 从宿主属性同步 |
| P4 = 30 | `StyleTrigger` | `Style::apply_state()` 写入的状态值 |
| P5 = 20 | `StyleSetter` | `Style::apply()` 写入的基线值 |
| P6 = 10 | `Inherited` | 沿视觉树继承的值 |
| P7 = 0  | `Default` | DP 注册时的默认值 |

`get_value(prop)` 返回**最高优先级的有效层**。

---

## 21.4 层 1 — 控件类规范

### 21.4.1 DependencyProperty 声明

**规则 L1-1**：所有可被**样式写入**或**模板绑定**的属性，必须注册为 `DependencyProperty`。

```cpp
// ✅ 正确：外观属性必须为 DP
static const DependencyProperty& BackgroundProperty;
static const DependencyProperty& ForegroundProperty;
static const DependencyProperty& PaddingProperty;
static const DependencyProperty& CornerRadiusProperty;

// ✅ 正确：状态专用目标值也用 DP（样式可覆盖）
static const DependencyProperty& HoveredBackgroundProperty;
static const DependencyProperty& PressedBackgroundProperty;
static const DependencyProperty& DisabledBackgroundProperty;

// ✅ 正确：交互状态用普通 bool 字段，不用 DP（避免 set_value 开销）
bool is_hovered_{ false };
bool is_pressed_{ false };
bool is_enabled_{ true };
```

**规则 L1-2**：命令、内容等行为属性也用 DP，优先由用户写入 Local 层（P2），不由样式层控制。

### 21.4.2 事件处理与状态更新

**规则 L1-3**：所有交互事件处理只做两件事——更新布尔字段，调用 `update_visual_state()`。

```cpp
// ✅ 正确范式
void MyControl::on_mouse_enter() {
    if (!is_enabled_) return;
    is_hovered_ = true;
    update_visual_state();          // 驱动 VSM → 样式触发器 → 动画
}

void MyControl::on_mouse_leave() {
    is_hovered_ = false;
    is_pressed_ = false;
    update_visual_state();
}

void MyControl::on_mouse_down(input::MouseEventArgs& args) {
    if (!is_enabled_ || args.button() != input::MouseButton::Left) return;
    is_pressed_ = true;
    update_visual_state();
}

void MyControl::on_mouse_up(input::MouseEventArgs& args) {
    if (!is_enabled_ || args.button() != input::MouseButton::Left) return;
    const bool was_pressed = is_pressed_;
    is_pressed_ = false;
    update_visual_state();
    if (was_pressed) { raise_click(); }
}
```

**规则 L1-4**：事件处理中**禁止直接创建 Storyboard 或调用 AnimationClock**。

### 21.4.3 视觉状态计算

**规则 L1-5**：`compute_visual_state()` / `compute_state_name()` 只基于布尔字段，不访问 DP。

```cpp
// ✅ 正确
ControlVisualState MyControl::compute_visual_state() const {
    if (!is_enabled_) return ControlVisualState::Disabled;
    if (is_pressed_)  return ControlVisualState::Pressed;
    if (is_hovered_)  return ControlVisualState::Hovered;
    return ControlVisualState::Normal;
}

// 若控件需要自定义状态名（不在标准 5 个之内），覆盖此方法：
core::StringView MyControl::compute_state_name() const noexcept {
    // 例如：CheckBox 有 Checked/Unchecked/Indeterminate 状态
    if (!is_enabled_)   return "Disabled";
    if (is_checked_)    return is_hovered_ ? "CheckedHovered" : "Checked";
    return is_hovered_ ? "UncheckedHovered" : "Unchecked";
}
```

### 21.4.4 on_apply_template()

**规则 L1-6**：`on_apply_template()` 是模板构建完成后的唯一合法时机，用于缓存模板子元素指针。

```cpp
void MyControl::on_apply_template() noexcept {
    // ✅ 正确：缓存命名模板子元素（名称与 build_fn 中 set_template_name() 一致）
    border_part_ = static_cast<Border*>(find_template_child("border"));
    icon_part_   = static_cast<Image*>(find_template_child("icon"));

    // ✅ 可选：对取到的元素做额外初始化（如订阅元素级事件）
    if (border_part_) {
        // ...
    }

    // ❌ 禁止：在此处写入 DP Local 值（应由 Style 或用户代码负责）
    // ❌ 禁止：在此处调用 update_visual_state()（布局尚未完成）
    // ❌ 禁止：在此处操作 Storyboard
}
```

**注意**：`find_template_child()` 在 `on_apply_template()` 调用时才能保证有效；不应在构造函数或其他位置调用。

### 21.4.5 on_visual_state_changed()

**规则 L1-7**：`on_visual_state_changed()` 是控件层的**最后防线**，仅用于处理**模板层无法覆盖**的控件特有行为。

```cpp
// ✅ 正确：仅做控件特有行为（如 Ripple 涟漪启停）
void MyControl::on_visual_state_changed(ControlVisualState old_state,
                                         ControlVisualState new_state) {
    // 父类调用（触发 invalidate_render）
    Control::on_visual_state_changed(old_state, new_state);

    // 控件特有行为（Ripple 不属于通用模板能描述的动画）
    if (new_state == ControlVisualState::Pressed) {
        ripple_.begin(last_press_point_);
    } else {
        ripple_.stop();
    }
}

// ❌ 错误：在此处手动创建背景色 Storyboard（应由模板层 VSM 负责）
// void MyControl::on_visual_state_changed(...) {
//     bg_storyboard_ = make_owned<Storyboard>();   // ← 违反范式
//     bg_storyboard_->animate_dp(...)
// }
```

---

## 21.5 层 2 — 控件模板规范

### 21.5.1 build_fn 结构

模板构建函数（`BuildFn`）是 `ControlTemplate` 的核心，分四个阶段：

```
build_fn(Control& ctrl)
    ├─ 阶段 A：构建视觉元素树
    │   ├─ MINE_NEW / make_owned 创建各视觉元素
    │   └─ set_template_name("id")  ← 供 find_template_child / on_apply_template 使用
    │
    ├─ 阶段 B：建立 TemplateBinding（P3）
    │   └─ ctrl.bind_template(child, childProp, hostProp)
    │       ← 宿主 DP 变化时自动同步到模板子元素
    │
    ├─ 阶段 C：配置 VisualStateManager
    │   ├─ vsm.define_state("Normal" / "Hovered" / ...)
    │   ├─ vsm.add_transition("*", "Hovered", [border](Storyboard& sb) { ... })
    │   └─ vsm.set_style(&style)   ← 连接样式层，使 go_to_state 自动调用 apply_state
    │
    └─ 阶段 D：安装模板根 + 触发 on_apply_template
        └─ ctrl.set_template_root(std::move(root_ptr))
           ← 注意：on_apply_template() 由 Control::measure_override 在 build_fn 返回后调用
```

### 21.5.2 TemplateBinding（bind_template）

**规则 L2-1**：宿主控件所有**在模板内需要体现**的属性，必须通过 `bind_template()` 连接。

```cpp
// 示例：DefaultButton template build_fn

// 宿主控件外观属性 → 模板内 Border 对应属性（P3 自动同步）
ctrl.bind_template(*border, Border::BackgroundProperty,      Button::BackgroundProperty);
ctrl.bind_template(*border, Border::CornerRadiusProperty,    Button::CornerRadiusProperty);
ctrl.bind_template(*border, Border::BorderColorProperty,     Button::BorderColorProperty);
ctrl.bind_template(*border, Border::BorderThicknessProperty, Button::BorderThicknessProperty);

// 宿主控件内容属性 → ContentPresenter
ctrl.bind_template(*presenter, ContentPresenter::ContentProperty, Button::ContentProperty);
ctrl.bind_template(*presenter, ContentPresenter::PaddingProperty, Button::PaddingProperty);
```

**规则 L2-2**：`bind_template` 建立的是**单向**（宿主 → 子元素）同步，发生在 P3 层，不会覆盖子元素的 Local(P2) 值。

### 21.5.3 VisualStateManager 配置

**规则 L2-3**：所有视觉状态必须先用 `define_state()` 注册，状态名必须与控件类中
`compute_state_name()` 返回的名称一致。

**规则 L2-4**：`vsm.set_style(&style)` 必须在 `set_visual_state_manager()` 之前调用，
使得 `go_to_state()` 能自动触发 `style->apply_state()`（写入 P4 StyleTrigger 值）。

**规则 L2-5**：过渡 `Storyboard` 配置函数中，**只声明动画路径**（animate 哪个元素哪个属性），
**不提供具体数值**（具体的 from/to 颜色由样式层的 StyleTrigger 在 P4 写入，Storyboard 从
P4 值中 capture_from_values）。

```cpp
// ✅ 正确：只声明动画路径，不硬编码颜色
vsm.add_transition("*", "Hovered", [border](Storyboard& sb) {
    sb.animate_dp(border, Border::BackgroundProperty,
                  Duration::milliseconds(120), QuadEaseOut);
    // ← 起始值由 capture_from_values 从 P1/P2/P3 层采样
    // ← 目标值由 style->apply_state("Hovered") 写入的 P4 层提供
});

// ❌ 错误：模板层硬编码颜色
vsm.add_transition("*", "Hovered", [border](Storyboard& sb) {
    // 目标值不应在模板层硬编码，否则样式层无法换肤
    sb.animate_dp_to(border, Border::BackgroundProperty,
                      Brush::solid(Color{0.46f, 0.36f, 0.67f}),  // ← 违反范式
                      Duration::milliseconds(120), QuadEaseOut);
});
```

### 21.5.4 完整 build_fn 示例

```cpp
static void build_DefaultButton_impl(mine::ui::Control& ctrl) {
    using namespace mine::ui;
    auto& button = static_cast<Button&>(ctrl);

    // ── 阶段 A：构建视觉树 ─────────────────────────────────────────────
    auto border_owned    = core::make_owned<Border>();
    auto presenter_owned = core::make_owned<ContentPresenter>();
    Border*           border    = border_owned.get();
    ContentPresenter* presenter = presenter_owned.get();

    border->set_template_name("border");
    presenter->set_template_name("content");
    presenter->set_h_align(HorizontalAlignment::Center);
    presenter->set_v_align(VerticalAlignment::Center);
    border_owned->add_visual_child(std::move(presenter_owned));

    // ── 阶段 B：TemplateBinding（P3）──────────────────────────────────
    button.bind_template(*border, Border::BackgroundProperty,
                                   Button::BackgroundProperty);
    button.bind_template(*border, Border::CornerRadiusProperty,
                                   Button::CornerRadiusProperty);
    button.bind_template(*presenter, ContentPresenter::ContentProperty,
                                      Button::ContentProperty);
    button.bind_template(*presenter, ContentPresenter::PaddingProperty,
                                      Button::PaddingProperty);

    // ── 阶段 C：VisualStateManager ────────────────────────────────────
    style::VisualStateManager vsm{ button };

    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Focused");
    vsm.define_state("Disabled");

    // 过渡动画：只声明动画路径，目标值由 style->apply_state 提供
    vsm.add_transition("*", "Hovered", [border](Storyboard& sb) {
        sb.animate_dp(border, Border::BackgroundProperty,
                      Duration::milliseconds(120), QuadEaseOut);
    });
    vsm.add_transition("*", "Pressed", [border](Storyboard& sb) {
        sb.animate_dp(border, Border::BackgroundProperty,
                      Duration::milliseconds(60), QuadEaseIn);
    });
    vsm.add_transition("Pressed", "*", [border](Storyboard& sb) {
        sb.animate_dp(border, Border::BackgroundProperty,
                      Duration::milliseconds(100), QuadEaseOut);
    });

    // ★ 连接样式层：go_to_state 会自动调用 style->apply_state()（P4）
    // 注意：style 在 TemplateRegistry 注册时传入，或通过 ctrl.style() 获取
    if (auto* s = button.style()) {
        vsm.set_style(s);
    }

    button.set_visual_state_manager(std::move(vsm));

    // ── 阶段 D：安装模板根 ────────────────────────────────────────────
    // on_apply_template() 由 Control::measure_override() 在 build_fn 返回后调用
    button.set_template_root(std::move(border_owned));
}
```

---

## 21.6 层 3 — 控件样式规范

### 21.6.1 apply_fn 与 state_setters

**规则 L3-1**：`apply_fn_()` 只向 StyleSetter(P5) 写入**基线值**（Normal 状态下的外观）。

**规则 L3-2**：`state_setters_` 中的状态名**必须与 VSM 注册的状态名完全一致**。

**规则 L3-3**：状态 setter 只写入**终值**，不描述动画曲线（动画曲线在模板的 VSM 过渡中配置）。

```cpp
// ✅ 正确：样式层的状态 setter 只写终值
static Style make_accent_button_style() {
    Style s;
    s.set_target_type(TypeId::of<Button>());
    s.set_name("AccentButton");

    // 基线值（P5）：Normal 状态
    s.add_setter({ &Button::BackgroundProperty,
                   Variant{ Brush::solid(Color{0.41f, 0.31f, 0.64f}) } });     // #6750A4
    s.add_setter({ &Button::ForegroundProperty,
                   Variant{ Brush::solid(Color::White) } });

    // 状态终值（P4）：Hovered 状态
    s.add_state_setters({ "Hovered", {
        { &Button::BackgroundProperty,
          Variant{ Brush::solid(Color{0.45f, 0.36f, 0.68f}) } },               // #735BAC
    }});

    // 状态终值（P4）：Pressed 状态
    s.add_state_setters({ "Pressed", {
        { &Button::BackgroundProperty,
          Variant{ Brush::solid(Color{0.48f, 0.40f, 0.69f}) } },               // #7A65AF
    }});

    // 状态终值（P4）：Disabled 状态
    s.add_state_setters({ "Disabled", {
        { &Button::BackgroundProperty,
          Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.12f}) } },
        { &Button::ForegroundProperty,
          Variant{ Brush::solid(Color{0.11f, 0.11f, 0.12f, 0.38f}) } },
    }});

    return s;
}
```

---

## 21.7 完整数据流（go_to_state 触发链）

以鼠标悬停为例，完整的数据流如下：

```
① 用户鼠标移入控件
      │
      ▼
② MouseEnter 路由事件 → on_mouse_enter()
      │  is_hovered_ = true
      ▼
③ update_visual_state()
      │  compute_state_name() → "Hovered"
      │
      ├─ [VSM 已安装] → vsm()->go_to_state("Hovered")
      │       │
      │       ├─ ④ style_->apply_state(owner, "Hovered")
      │       │       → BackgroundProperty[P4] = HoveredColor  ← StyleTrigger
      │       │
      │       ├─ ⑤ 查找过渡配置（"* → Hovered"）→ 找到
      │       │       → Storyboard::capture_from_values()
      │       │             → BackgroundProperty 当前值（可能是 P1/P2/P3/P4/P5/P0）
      │       │       → Storyboard::begin()
      │       │             → 每帧 tick：计算插值 → set_value(P1, lerp_brush)
      │       │
      │       └─ ⑥ 更新 current_state_ = "Hovered"
      │
      └─ on_visual_state_changed(Normal, Hovered)  ← 控件特有行为（如 Ripple）
              → Control::on_visual_state_changed()
                    → invalidate_render()

⑦ 渲染阶段
      │  on_render 或模板树的 Border::on_render 调用 canvas.fill_rounded_rect
      │  使用 get_value(BackgroundProperty)
      │       → 返回 P1（Animation）= Storyboard 当前插值画刷
      │           [动画结束后 P1 清除 → 回落到 P4 = HoveredColor]
      ▼
⑧ 画面呈现
```

---

## 21.8 样式触发器（StyleTrigger）工作原理

StyleTrigger（P4）的"触发"本质是由 **`VisualStateManager::go_to_state()`** 驱动的：

```
go_to_state("Hovered")
    └─ style_->apply_state(owner, "Hovered")
           └─ 遍历 state_setters_ 找到 name == "Hovered" 的条目
                  └─ 对每个 setter：owner.set_value(prop, value, StyleTrigger(P4))
```

关键特性：

1. **P4 不覆盖 P2（Local）**：用户代码 `set_background(myColor)` 写入 P2，即使在 Hovered 状态下，
   背景色也会是 myColor（因为 P2 > P4）。这与 WPF 的 Trigger 行为完全一致。

2. **P4 被 P1（Animation）覆盖**：Storyboard 插值期间，`get_value` 返回 P1 层的插值画刷，
   P4 的终值作为动画结束后的稳定状态。

3. **离开状态时 P4 回退**：当 `go_to_state("Normal")` 时，样式写入 Normal 状态的 P4 值
   （即基线值，因为 Normal 通常没有 state_setters，此时 P4 回退为空，
   `get_value` 回落到 P5 StyleSetter 层的基线颜色）。

   > **实现注意**：`apply_state()` 应在切换状态时清除上一状态的 P4 值，或通过
   > P4 复写的方式（将 P4 写为新状态的值，旧状态的 P4 自动被覆盖）。

---

## 21.9 两类控件路径对比

### 路径 A：模板化控件（推荐，可换肤）

| 步骤 | 执行者 |
|------|--------|
| 注册 DependencyProperty | 控件类 |
| 处理事件 → 更新 bool → `update_visual_state()` | 控件类 |
| `on_apply_template()` 缓存模板子元素指针 | 控件类 |
| 构建视觉树 + `bind_template` + VSM 配置 | ControlTemplate build_fn |
| 写入 StyleSetter(P5) + StyleTrigger(P4) | Style apply_fn |
| Storyboard 驱动 P1 动画 | VisualStateManager（自动） |
| `on_render()` 委托模板树 | Control 基类（自动） |

> 适用：Button、CheckBox、RadioButton、Slider、TextBox 等标准控件。

### 路径 B：手写渲染控件（叶子控件）

不使用 `template_slot_`，直接在 `on_render()` 中绘制，覆盖 `measure_override` 返回内容尺寸。
适用于高度定制化的原子控件（如 `Border`、`TextBlock`、`ContentPresenter`）。

路径 B 控件仍应遵守状态计算规范（L1-3 ~ L1-5），但可在 `on_visual_state_changed()` 中
直接读取 DP 值绘制，无需经过 VSM/样式层。

---

## 21.10 新增控件 Checklist

创建新 `Control` 派生类前，逐项确认：

### 控件类（层 1）
- [ ] 所有可外观化属性已声明为 `DependencyProperty`
- [ ] 交互状态用 `bool` 字段（`is_hovered_`、`is_pressed_` 等），非 DP
- [ ] 事件处理只更新 bool 字段 + 调用 `update_visual_state()`，不直接操作 Storyboard
- [ ] `compute_visual_state()` / `compute_state_name()` 只依赖 bool 字段
- [ ] `on_apply_template()` 中调用 `find_template_child()` 缓存元素指针
- [ ] `on_visual_state_changed()` 仅处理**控件特有**行为（Ripple 等），并调用基类

### 控件模板（层 2）
- [ ] 所有模板子元素已调用 `set_template_name()`（与 `find_template_child` 的参数对应）
- [ ] 所有应同步的宿主属性已通过 `bind_template()` 连接
- [ ] 所有在 VSM 中 `define_state()` 注册的状态名与 `compute_state_name()` 返回值一致
- [ ] `vsm.set_style()` 已连接对应样式
- [ ] 过渡 Storyboard 中**未硬编码颜色**（只声明动画路径，目标值由 P4 提供）
- [ ] `set_template_root()` 使用 `OwnedPtr` 版本，转移所有权

### 控件样式（层 3）
- [ ] `apply_fn_` 只写 P5（StyleSetter）基线值
- [ ] `state_setters_` 中的 state_name 与 VSM 注册的名称一致
- [ ] 状态 setter 只写终值，不描述动画曲线
- [ ] 样式已注册到 `StyleRegistry`（通常由 `.g.cpp` 静态对象完成）

---

## 21.11 反模式（Anti-Patterns）

### ❌ 在控件类中操作 Storyboard

```cpp
// ❌ 错误：控件类直接驱动背景色动画
void MyControl::on_visual_state_changed(ControlVisualState, ControlVisualState new_s) {
    bg_anim_ = make_owned<Storyboard>();
    bg_anim_->animate_dp_to(*this, BackgroundProperty, target_color, 120ms, QuadEaseOut);
    bg_anim_->resolve_and_begin();
    // → 换肤后模板里的颜色不变，因为动画目标值硬编码在控件里
}
```

### ❌ 在模板层硬编码颜色

```cpp
// ❌ 错误：模板直接指定颜色
vsm.add_transition("*", "Hovered", [](Storyboard& sb) {
    sb.animate_dp_to(&border, BackgroundProperty,
                      Brush::solid({0.45f, 0.36f, 0.68f}),  // ← 换肤失效
                      Duration::ms(120), QuadEaseOut);
});
```

### ❌ 绕过 DP 系统读取属性值

```cpp
// ❌ 错误：on_render 直接读取成员变量而非 get_value()
void MyControl::on_render(Canvas& canvas) {
    canvas.fill_rect(bounds_rect(), background_color_);  // ← Style/Animation 层被绕过
}

// ✅ 正确
void MyControl::on_render(Canvas& canvas) {
    const auto& v = get_value(BackgroundProperty);
    if (v.has<Brush>()) {
        canvas.fill_rect(bounds_rect(), v.get<Brush>());
    }
}
```

### ❌ 在构造函数中使用 find_template_child()

```cpp
// ❌ 错误：模板尚未构建
MyControl::MyControl() {
    border_part_ = static_cast<Border*>(find_template_child("border"));  // 返回 nullptr
}

// ✅ 正确：在 on_apply_template() 中缓存
void MyControl::on_apply_template() noexcept {
    border_part_ = static_cast<Border*>(find_template_child("border"));
}
```

---

## 21.12 与现有代码的过渡策略

`Button` 当前使用**路径 B 变体**：手写模板构建 + `on_visual_state_changed` 直接操作 Storyboard。
在 F2.3（ContentControl / UserControl）完成后，应按以下步骤迁移至标准范式：

1. 将 `build_DefaultButton_impl` 注册到 `TemplateRegistry`
2. Button 构造函数改为 `set_template_slot("DefaultButton")`
3. 删除 Button 中的 `on_visual_state_changed` 动画逻辑，改为 VSM 过渡配置
4. 增加 `DefaultButtonStyle`，将硬编码颜色迁移到样式层
5. 增加 `on_apply_template()` 缓存 `border_part_` / `content_part_` 指针

此迁移应作为独立任务在 F3 阶段完成，迁移前后行为一致（样式可应用，动画效果不变）。
