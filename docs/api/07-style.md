# mine.ui.style —— 样式与视觉状态管理模块

## 模块概述

`mine.ui.style` 提供控件外观定义与状态驱动的样式系统：`Style` 定义属性 setter 集合，`VisualStateManager` 管理状态切换与动画过渡。

| 核心类型 | 职责 |
|---------|------|
| `Style` | 样式对象：基线 setter（P20）+ 状态 setter（P30） |
| `StyleSetter` | 单个属性 setter（属性指针 + 值 Variant） |
| `VisualStateSetters` | 一个视觉状态对应的 setter 集合 |
| `VisualStateManager` | 状态机：状态注册、过渡配置、`go_to_state`、动画推进 |
| `VisualStateTransition` | 状态过渡描述（from、to、Storyboard 配置函数） |

**依赖**：`mine.core`、`mine.ui.property`、`mine.ui.animation`。

---

## 1. Style —— 样式对象

**文件**：`<mine/ui/style/Style.h>`

### 优先级体系

```
Animation(60) > Local(50) > StyleTrigger(30) > StyleSetter(20)
└─ Storyboard ─┘ └─ set_xxx ─┘ └─ 状态 setter ──┘ └─ 样式基线 ──┘
```

### 类摘要

```cpp
namespace mine::ui::style {

class Style {
public:
    Style();
    // 可拷贝、可移动

    // ── 查询 ──────────────────────────────────────────────────────────
    TypeId      target_type() const noexcept;   // 适用控件类型
    Style*      base()        const noexcept;   // BasedOn 父样式
    StringView  name()        const noexcept;   // 样式名称

    // ── 构建器 ────────────────────────────────────────────────────────
    Style& set_name(StringView name);
    Style& set_target_type(TypeId) noexcept;
    Style& set_base(Style* base) noexcept;
    Style& add_setter(StyleSetter setter);                        // P20 基线
    Style& add_state_setters(VisualStateSetters state_setters);   // P30 状态

    // ── 应用 ──────────────────────────────────────────────────────────
    void apply(DependencyObject& target) const;                   // 写入 P20 基线值
    void apply_state(DependencyObject& target, StringView state_name) const;  // 写入 P30
    void apply_state_animation(DependencyObject& target, StringView state_name) const; // P60
    void clear_all_state_values(DependencyObject& target) const;  // 清除全部 P30
    void clear_state_values(DependencyObject& target, StringView name) const;  // 清除指定状态 P30
    void clear_state_animation(DependencyObject& target, StringView name) const; // 清除指定状态 P60
};

} // namespace mine::ui::style
```

### StyleSetter / VisualStateSetters

```cpp
struct StyleSetter {
    const DependencyProperty* property;   // 目标属性
    Variant                   value;      // 写入值
    InlineString              res_key;    // DynamicResource 键（空 = 静态值）
};

struct VisualStateSetters {
    InlineString              state_name;      // 状态名（如 "Hovered"）
    SmallVector<StyleSetter, 8> setters;       // 该状态下的 setter 列表
};
```

### 使用示例（程序化构建样式）

```cpp
Style s;
s.set_target_type(TypeId::of<CheckBox>());
s.set_name("DefaultCheckBox");

// P5 基线
s.add_setter({ &CheckBox::IconBorderBrushProperty,
               Variant{ Brush::solid_rgb(0x79747E) } });

// P4 Hovered 状态
VisualStateSetters hovered;
hovered.state_name = "Hovered";
hovered.setters.push_back({ &CheckBox::StateLayerBrushProperty,
    Variant{ Brush::solid(Color{1,1,1,0.08f}) } });
s.add_state_setters(std::move(hovered));
```

---

## 2. VisualStateManager —— 视觉状态管理器

**文件**：`<mine/ui/style/VisualStateManager.h>`

### 类摘要

```cpp
namespace mine::ui::style {

class VisualStateManager {
public:
    explicit VisualStateManager(DependencyObject& owner) noexcept;

    // 移动语义（不可拷贝）
    VisualStateManager(VisualStateManager&&) noexcept;
    VisualStateManager& operator=(VisualStateManager&&) noexcept;

    // ── 状态注册 ────────────────────────────────────────────────────
    void define_state(StringView name) noexcept;

    // ── 过渡注册 ────────────────────────────────────────────────────
    void add_transition(StringView from, StringView to) noexcept;          // 无动画
    void add_transition(StringView from, StringView to,
                        Function<void(Storyboard&)> configure_fn) noexcept; // 带动画

    // ── 状态切换 ────────────────────────────────────────────────────
    bool go_to_state(StringView state_name, bool use_transitions = true) noexcept;
    // return true = 状态已切换，false = 未注册或已是当前状态

    // ── 动画推进 ────────────────────────────────────────────────────
    bool tick_animations(float dt) noexcept;
    // return true = 仍有活跃 Storyboard

    void stop_all_storyboards() noexcept;  // 立即停止并清理所有动画

    // ── 查询 ──────────────────────────────────────────────────────────
    StringView  current_state()   const noexcept;
    bool        has_state(StringView name) const noexcept;
    bool        has_transition(StringView from, StringView to) const noexcept;

    // ── 关联样式 ──────────────────────────────────────────────────────
    void        set_style(const Style* style) noexcept;
    const Style* attached_style() const noexcept;
};

} // namespace mine::ui::style
```

### 状态切换流程

```
go_to_state("Hovered")
  ├─ 1. 验证目标状态已注册
  ├─ 2. 已是当前状态 → return false
  ├─ 3. 查找匹配过渡（精确 from → 通配 "*"）
  ├─ [带动画路径]
  │   ├─ create Storyboard → configure(sb)
  │   ├─ capture_from_values()        // 采样当前可见值
  │   ├─ stop 旧 Storyboard
  │   ├─ clear_state_values(old_state) // 仅清除前一状态的 P30
  │   ├─ apply_state(new_state)        // 写入新状态 P30
  │   ├─ resolve_and_begin()          // 解析 to 值，启动动画
  │   └─ active_storyboards_.push(sb)
  └─ [即时路径（无动画/首次切换）]
      ├─ clear_state_values(old_state)
      ├─ apply_state(new_state)
      └─ apply_state_animation(new_state)  // P60 覆盖 Local
```

### 多状态组（多 VSM）注意事项

当一个控件使用多个 VSM 实例（如 CheckBox 的交互组 + 勾选组），需注意：

1. **每个 VSM 配独立的 Style 对象**，避免 `clear_state_values` 误清另一组的 P30
2. **属性集尽量不重叠**（如交互组仅控制 `StateLayerBrush`，勾选组控制 `IconBackground`/`CheckMark`）
3. **若属性有重叠**（如 `IconBorderBrush`），需在状态切换时互相 `stop_all_storyboards()`

---

## 3. 集成方式

### 控件构造函数中的典型配置

```cpp
// 1. 创建 VSM
VisualStateManager vsm{ *this };
vsm.define_state("Normal");
vsm.define_state("Hovered");
vsm.define_state("Pressed");

// 2. 注册过渡（带缓动）
vsm.add_transition("*", "Hovered", [this](Storyboard& sb) {
    sb.animate_dp(*this, StateLayerBrushProperty,
                  Duration::milliseconds(120), QuadEaseOut);
});

// 3. 连接样式
Style& active_style = default_button_style();
vsm.set_style(&active_style);
set_visual_state_manager(std::move(vsm));
active_style.apply(*this);   // 应用 P5 基线

// 4. 在事件中驱动状态
void on_mouse_enter() {
    is_hovered_ = true;
    update_visual_state();   // → vsm()->go_to_state("Hovered")
}
```

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.ui.property` | 样式通过 `set_value(prop, value, priority)` 写入属性 |
| `mine.ui.animation` | VSM 通过 `Storyboard` 实现状态过渡动画 |
| `mine.ui.visual` | `Control` 持有 `VisualStateManager` 实例 |
| `mine.ui.controls` | 各控件构造函数中配置 VSM |
