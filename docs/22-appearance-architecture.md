# 22 — 控件外观架构（Appearance Architecture）

> 本文档定义 MineFramework 控件外观（视觉呈现与可定制化）的**最终架构**。
> 它取代早期基于 `ControlTemplate` 的运行时模板方案（已于 2026-06 废除），
> 确立以 **「继承式基元绘制 + 组合式装配 + DP↔DP 绑定」** 为核心的 C++ 原生外观体系。
>
> 前置阅读：
> - [09-property-binding.md](09-property-binding.md) — 依赖属性与绑定基础
> - [20-style-template.md](20-style-template.md) — 样式 / 资源字典 / 主题（其 ControlTemplate 章节见本文 §22.9）
> - [21-controls-architecture.md](21-controls-architecture.md) — 控件七大子系统集成
> - [21-control-authoring.md](21-control-authoring.md) — Control 派生类开发范式

---

## 22.1 背景：两条极端路线的失败

在确立本架构前，项目经历了两种对立方案，二者各自失败，原因恰好对称：

| 方案 | 本质 | 失败原因 |
|------|------|----------|
| **WPF 全套 ControlTemplate** | 运行时解析标记 → 反射建树 → 运行时类型系统驱动外观 | 依赖 RTTI / 运行时反射 / 装箱，与 C++「AOT 优先、禁 RTTI、禁异常」根本冲突；模板注册表、懒构建、`bind_template`、`find_template_child` 等机制复杂且易错 |
| **每控件硬编码 `on_render`** | 外观逻辑写死在每个控件的绘制函数里 | 外观与控件代码焊死；改圆角、换配色、调布局都要改 C++ 并重编译；**定制外观极其艰难** |

**结论**：正确答案不在两端，而在中间。

- WPF 模板把「外观描述」做成了**运行时动态解析**——太重，逆着 C++ 写。
- 纯硬画把「外观描述」**彻底消灭**——太死，定制即改源码。

本架构的核心主张：

> **保留直接绘制的高性能与简单性（用于基元），同时引入「代码组合 + 属性绑定」让复合控件的外观可被结构化定制（用于复合控件）——全部用 C++ 原生表达，不引入任何运行时标记解析。**

---

## 22.2 三类元素的职责划分

外观体系把所有可视元素分为**三类**，各自的绘制策略与定制方式完全不同：

```
┌─────────────────────────────────────────────────────────────────────┐
│  ① 基元控件 Primitive（叶子，唯一真正接触 Canvas 的层）            │
│     Border / Rectangle / Ellipse / Shape / TextBlock / Image        │
│     → 在 on_render() 内直接绘制；从 DP 读取所有视觉参数             │
│     → 足够小、足够通用，几乎无需再定制                              │
├─────────────────────────────────────────────────────────────────────┤
│  ② 复合控件 Composite（装配工，不直接画图或仅画极少）             │
│     Button / TextBox / CheckBox / Slider / ScrollBar / ...          │
│     → 外观 = 一棵由基元控件组成的视觉子树                          │
│     → 控件类只负责语义与交互（状态、命令、内容属性）              │
│     → 用 DP↔DP 绑定把宿主属性同步到子元素                          │
├─────────────────────────────────────────────────────────────────────┤
│  ③ 容器 / 面板 Panel（布局，不绘制内容）                          │
│     StackPanel / Grid / Canvas / DockPanel / ...                    │
│     → 只测量与排列子元素，外观由子元素自身负责                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 设计要点

1. **只有基元控件直接画图。** 这是整个系统中唯一调用 `Canvas` 绘制 API 的地方，集中、可控、易优化。
2. **复合控件不画图，只组装。** 它的 `on_render` 应为空或极简；外观由其内部组装的基元子树呈现。
3. **容器只管布局。** 不参与外观呈现。

> **判定法则**：一个控件如果在问「我该怎么把自己画得更好看」，先反问「我能不能由更小的基元拼出来」。
> 能拼就拼（复合控件），不能拼的最小单位才下沉为基元控件去画。

---

## 22.3 外观定制的四个层次（从轻到重）

面对「让一个控件长成想要的样子」这个需求，按成本从低到高，依次有四种手段。**优先用低层手段，不够再上升。**

| 层次 | 手段 | 改什么 | 是否需重编译 | 优先级层 |
|------|------|--------|--------------|----------|
| **L1 属性** | 设置 DP（颜色 / 圆角 / 边距 / 字体…） | 单个外观参数 | 否（运行时可改） | Local(P50) |
| **L2 样式** | `Style` 的 StyleSetter + 状态 setter | 一组属性的基线与状态值 | 否（可换 Style） | StyleSetter(P20) / StyleTrigger(P30) |
| **L3 状态动画** | `VisualStateManager` + `Storyboard` | 状态切换时的过渡动画 | 否 | Animation(P60) |
| **L4 结构** | 继承重写 `on_render`（基元）或 组合子树 + `bind_property`（复合） | 视觉树结构本身 | 是（改 C++） | TemplateBind(P40) / Local(P50) |

```
需求来了
  │
  ├─ 只想换个颜色/圆角/边距？           → L1：set_xxx() 或 DP
  ├─ 想统一一批控件的外观与状态外观？   → L2：定义/替换 Style
  ├─ 想要状态切换有平滑过渡动画？       → L3：VSM 配 Storyboard
  └─ 想彻底改变控件的视觉结构？
        ├─ 基元控件：继承 + 重写 on_render
        └─ 复合控件：组合基元子树 + bind_property 同步属性
```

这四层与依赖属性优先级链精确对应，互不打架（见 §22.4）。

---

## 22.4 依赖属性优先级链（外观值的最终裁决）

所有外观参数都是 `DependencyProperty`，其最终生效值由优先级链裁决（高 → 低）：

| 优先级 | 符号 | 来源 | 对应外观层次 |
|--------|------|------|--------------|
| P1 = 60 | `Animation` | `Storyboard` 正在插值的运行时值 | L3 状态动画 |
| P2 = 50 | `Local` | 用户 `set_xxx()` / **OneWay 绑定源、TwoWay 绑定两端** | L1 属性 / L4(复合) |
| P3 = 40 | `TemplateBind` | **OneWay/OneTime 的 DP↔DP / VM 绑定写入目标** | L4(组合同步) |
| P4 = 30 | `StyleTrigger` | `Style::apply_state()` 状态 setter | L2 状态外观 |
| P5 = 20 | `StyleSetter` | `Style::apply()` 基线 setter | L2 基线外观 |
| P6 = 10 | `Inherited` | 沿视觉树继承（如 DataContext、字体） | — |
| P7 = 0  | `Default` | DP 注册默认值 | — |

`get_value(prop)` 返回**最高优先级的有效层**。

> **关键设计**：
> - **OneWay 绑定写 `TemplateBind(P40)`** —— 低于 `Local(P50)`，因此用户显式 `set_xxx()` 永远能覆盖绑定（「绑定是默认外观，本地可手动改」）。
> - **TwoWay 绑定两端都写 `Local(P50)`** —— 两端对等，避免目标端的 Local 残留遮盖正向更新导致 TwoWay 退化为单向（见 §22.6.4）。

---

## 22.5 组合式复合控件范式（核心实践）

这是本架构区别于「纯硬画」的关键能力。以一个 MD3 Outlined 风格的 `TextBox` 为例。

### 22.5.1 反面：硬画（旧方式，不推荐）

```cpp
// ❌ 外观与控件焊死：改外框样式必须改这段并重编译
void TextBox::on_render(paint::Canvas& canvas) {
    canvas.stroke_complex_rounded_rect(bounds(), corner_radii(),
        get_value(BorderBrushProperty).get<Brush>(),
        get_value(BorderThicknessProperty).get<float>());
    // 还要手动画文本、光标、占位符……全堆在这里
}
```

### 22.5.2 正面：组合（推荐方式）

复合控件在**构造时**组装一棵基元子树，并用 `bind_property` 把宿主属性同步到子元素：

```cpp
TextBox::TextBox() {
    // ① 组装基元子树：外框 Border + 内容呈现器
    auto border = core::make_shared<Border>();
    auto content = core::make_shared<ContentPresenter>();
    border->set_child(content);
    set_inner_element(border);          // 复合控件持有内部子树（生命周期托管）

    // ② DP↔DP 绑定：把宿主外观属性「单向同步」到 Border（TemplateBind 优先级）
    border->bind_property(Border::BorderBrushProperty,
                          *this, TextBox::BorderBrushProperty);
    border->bind_property(Border::BorderThicknessProperty,
                          *this, TextBox::BorderThicknessProperty);
    border->bind_property(Border::CornerRadiusProperty,
                          *this, TextBox::CornerRadiusProperty);
    content->bind_property(ContentPresenter::MarginProperty,
                           *this, TextBox::PaddingProperty);

    // ③ VSM：状态切换驱动 Border 的描边颜色/粗细过渡（见 §22.3 L3）
    configure_visual_states();          // Normal / Hovered / Focused

    // TextBox 自身 on_render 不再绘制外框——外框由 Border 画
}
```

### 22.5.3 这套范式解决了什么

| 痛点（硬画时代） | 组合范式如何解决 |
|------------------|------------------|
| 改外框样式要改控件源码 | 改 `Border` 一处，所有用 Border 的控件统一受益 |
| 外观参数散落在 `on_render` 各处 | 全部声明为 DP，可被 L1/L2/L3 统一驱动 |
| 状态动画要手写插值 | VSM 驱动 Border 的 DP，复用动画系统 |
| 子元素拿不到宿主属性值 | `bind_property` 建立宿主 DP → 子元素 DP 的同步通道 |

> **`bind_property` 是这套范式的粘合剂。** 没有它，组合子树就是「死」的——子元素无法随宿主属性变化，
> 你只能退回 `on_render` 手动把值画上去。这正是早期「一改成直接画图，定制就极其艰难」的根因。

---

## 22.6 DP↔DP 属性绑定系统

> 本节描述本架构落地所依赖的属性到属性绑定能力（`mine.ui.binding`），
> 它既服务于 §22.5 的复合控件组装，也支持任意控件间的外观联动（等价 WPF 的 `ElementName` 绑定）。

### 22.6.1 绑定方向（BindingMode）

| 模式 | 数据流 | 典型场景 |
|------|--------|----------|
| `OneWay` | 源 → 目标 | 子元素外观随宿主属性变化；只读展示 |
| `OneTime` | 源 → 目标（仅一次） | 静态外观初始化，之后不再联动 |
| `TwoWay` | 源 ↔ 目标 | 两个控件值互相同步（如 Slider ↔ ProgressBar） |
| `OneWayToSource` | 目标 → 源 | 只把目标变化写回源，源变化不影响目标 |

### 22.6.2 两套调用接口

**底层自由工厂**（`BindingExpression::bind_property`）—— 完全控制生命周期：

```cpp
BindingExpression expr;   // 调用方负责保持其存活
BindingExpression::bind_property(
    expr,
    /*source*/ slider,   Slider::ValueProperty,
    /*target*/ progress, ProgressBar::ValueProperty,
    BindingMode::TwoWay);
// expr 析构时自动解除所有订阅
```

**便捷成员方法**（`FrameworkElement::bind_property`）—— 生命周期由元素内置存储托管：

```cpp
// 绑定写入 this 的内置 bindings_，无需在外部声明 BindingExpression
border->bind_property(Border::CornerRadiusProperty,   // 本元素目标属性
                      *host, TextBox::CornerRadiusProperty,  // 源对象 + 源属性
                      BindingMode::OneWay);
```

完整签名：

```cpp
// 自由工厂（mine/ui/binding/BindingExpression.h）
static void BindingExpression::bind_property(
    BindingExpression&        out,
    DependencyObject&         source,
    const DependencyProperty& source_prop,
    DependencyObject&         target,
    const DependencyProperty& target_prop,
    BindingMode               mode       = BindingMode::OneWay,
    IConverter*               converter  = nullptr,
    core::StringView          conv_param = {}) noexcept;

// 便捷成员（mine/ui/visual/FrameworkElement.h）
void FrameworkElement::bind_property(
    const DependencyProperty& target_prop,
    DependencyObject&         source,
    const DependencyProperty& source_prop,
    BindingMode               mode      = BindingMode::OneWay,
    IConverter*               converter = nullptr) noexcept;
```

### 22.6.3 命名约定（与既有绑定 API 的关系）

`mine.ui.binding` 现有两类绑定，命名按**源的种类**区分：

| 工厂 | 源 | 用途 | 谁写入目标的优先级 |
|------|-----|------|-------------------|
| `bind(...)` / `set_binding(...)` | `INotifyPropertyChanged`（按属性名） | **VM → UI**（MVVM 数据绑定） | TemplateBind |
| `bind_property(...)` | `DependencyObject` + `DependencyProperty` | **DP ↔ DP**（元素间外观联动） | OneWay=TemplateBind，TwoWay=Local |

> 选择标准：源是 **ViewModel** 用 `bind`/`set_binding`；源是**另一个控件/依赖对象**用 `bind_property`。

### 22.6.4 写入优先级规则（重要）

| 模式 | 正向（源→目标）写入优先级 | 反向（目标→源）写入优先级 |
|------|--------------------------|--------------------------|
| OneWay / OneTime | `TemplateBind(P40)` | —（无反向） |
| TwoWay | `Local(P50)` | `Local(P50)` |
| OneWayToSource | —（无正向） | `Local(P50)` |

- **OneWay 用 TemplateBind**：让用户显式 `set_xxx()`（Local）能覆盖绑定值。
- **TwoWay 用 Local（两端对等）**：若正向仍写 TemplateBind，则用户改目标（Local）后，目标端残留的 Local 会遮盖后续正向更新（TemplateBind < Local），使 TwoWay 退化为单向。两端都用 Local 可对等覆盖，保证双向持续生效。

### 22.6.5 防循环机制

TwoWay / OneWayToSource 通过 `is_updating` 标志阻断回环：

```
改源:   源订阅 → re_evaluate(is_updating: F→T) → 写目标 → 目标订阅 → write_back(被T挡住) → 复位F
改目标: 目标订阅 → write_back(is_updating: F→T) → 写源 → 源订阅 → re_evaluate(被T挡住) → 复位F
```

任一方向更新期间，对端回调被 `is_updating` 挡住，不会无限递归；更新结束复位，后续变更仍能正常触发。

### 22.6.6 值转换器（IConverter）

跨类型绑定（如 `int` → `Brush`、`bool` → `Visibility`）通过 `IConverter` 在写入目标前转换：

```cpp
border->bind_property(Border::BackgroundProperty,
                      *vm_state, StateObj::LevelProperty,
                      BindingMode::OneWay, &level_to_brush_converter);
```

`convert()` 用于正向；`convert_back()` 预留给 TwoWay 反向转换（当前 DP↔DP 反向暂按同类型直传，跨类型 TwoWay 请确保源/目标类型一致或自行处理）。

---

## 22.7 控件作者检查清单

新增一个控件时，按本架构依次自问：

- [ ] 它是**基元**还是**复合**？（能否由更小的基元拼出来？）
- [ ] 所有外观参数都声明为 `DependencyProperty` 了吗？（颜色/圆角/边距/字体…）
- [ ] 交互状态用 `bool` 字段（`is_hovered_` 等），不是 DP？
- [ ] 基元控件：`on_render` 只从 DP 读值绘制，不写死常量？
- [ ] 复合控件：构造时组装基元子树 + `bind_property` 同步宿主属性，`on_render` 为空？
- [ ] 状态外观差异交给 `Style` 的状态 setter，过渡动画交给 `VSM`？
- [ ] 提供了默认 `Style`（基线 + 状态），使控件开箱即用？

---

## 22.8 端到端示例：Button 的外观组装（目标形态）

```cpp
Button::Button() {
    // 复合控件：外框 Border + 内容 ContentPresenter
    auto bg = core::make_shared<Border>();
    content_part_ = core::make_shared<ContentPresenter>();
    bg->set_child(content_part_);
    set_inner_element(bg);

    // 宿主外观属性 → Border（OneWay，TemplateBind 优先级，可被 Local 覆盖）
    bg->bind_property(Border::BackgroundProperty,    *this, Button::BackgroundProperty);
    bg->bind_property(Border::CornerRadiusProperty,  *this, Button::CornerRadiusProperty);
    bg->bind_property(Border::BorderBrushProperty,   *this, Button::BorderColorProperty);

    // 内容前景/字体 → ContentPresenter
    content_part_->bind_property(ContentPresenter::ForegroundProperty,
                                 *this, Button::ForegroundProperty);

    // 状态外观：VSM + Style（Normal/Hovered/Pressed/Disabled），过渡走 Storyboard
    set_visual_state_manager(make_button_vsm());
    default_button_style().apply(*this);          // P20 基线
    update_visual_state();                        // 进入初始状态
}

// Button 自身不绘制外框/背景——全部由 Border 完成
```

外观定制路径（无需改 Button 源码）：

```cpp
btn.set_corner_radius(20);                                  // L1
btn.set_background(Brush::solid_rgb(0x6750A4));             // L1
accent_button_style().apply(btn);                          // L2 换一套样式
// 想要彻底不同的结构？派生 MyFancyButton 重新组装子树（L4）
```

---

## 22.9 与已废弃 ControlTemplate 的对照

| 维度 | 旧 ControlTemplate（已废弃） | 本架构 |
|------|------------------------------|--------|
| 视觉树来源 | `TemplateRegistry` 注册的 `BuildFn`，首次 measure 懒构建 | 控件**构造函数**直接组装（确定、可调试） |
| 宿主→子元素同步 | `bind_template()`（TemplateBind 专用） | `bind_property()`（通用 DP↔DP，TemplateBind 仅为其 OneWay 默认优先级） |
| 子元素访问 | `find_template_child("name")`（字符串查名） | 直接持有 `core::shared_ptr` 成员（类型安全） |
| 模板替换 | 替换注册的 BuildFn | 派生控件重新组装子树 |
| 运行时开销 | 注册表查找 + 懒构建 + 字符串匹配 | 无（纯构造期组装） |
| 标记语言 | 依赖 MML/mmlc 生成 BuildFn | 不依赖；MML 可选地生成等价组装代码 |

> 早期 `ControlTemplate` / `TemplateRegistry` / `bind_template` / `find_template_child`
> 已于 2026-06 移除（见 [CHANGELOG.md](../CHANGELOG.md)）。其中**唯一需要保留的能力**——
> 「宿主属性同步到子元素」——已由更通用的 `bind_property` 取代并增强（支持四种绑定方向）。

---

## 22.10 与 MML / mmlc 的关系（演进方向）

本架构**不要求**标记语言即可完整工作（构造期 C++ 组装 + `bind_property`）。未来引入 MML 时：

- MML 的 `style` 块 → `mmlc` 生成 `Style::apply_fn`（已有路径，见 [20-style-template.md](20-style-template.md) §20.4）。
- MML 描述的视觉子树 → `mmlc` 生成与 §22.5.2 **形态完全一致**的构造期组装代码（`make_shared` + `set_child` + `bind_property`）。

即：**手写组装与 mmlc 生成产物同构**，因此「先手写跑通架构、后让 mmlc 自动生成」是平滑过渡，不会推倒重来。

---

## 22.11 决策记录

| 决策 | 取舍 |
|------|------|
| 不恢复运行时 ControlTemplate | 尊重 2026-06 的 QWidget 继承决策；避免 RTTI/反射/标记解析与 C++ 约束冲突 |
| 复合控件构造期组装而非懒构建 | 确定性、可调试、零注册表开销；代价是组装代码显式可见（视为优点） |
| 外观参数一律 DP 化 | 使 L1~L3 四层定制手段统一作用于同一真相源 |
| 新增 `bind_property` 而非复活 `bind_template` | 通用化（四种方向 + converter），同时覆盖元素间联动与组合同步两类需求 |
| OneWay=TemplateBind / TwoWay=Local | 兼顾「本地可覆盖绑定」与「双向对等不退化」两个语义目标 |

---

## 22.12 相关源码

| 能力 | 文件 |
|------|------|
| DP↔DP 绑定工厂 | `src/mine/ui/binding/api/include/mine/ui/binding/BindingExpression.h`（`bind_property`） |
| 绑定运行时（求值/回写/防循环） | `src/mine/ui/binding/src/BindingExpression.cpp` |
| 绑定方向 | `src/mine/ui/binding/api/include/mine/ui/binding/BindingMode.h` |
| 便捷成员方法 | `src/mine/ui/visual/api/include/mine/ui/visual/FrameworkElement.h`（`bind_property`） |
| 样式 / 状态机 | `src/mine/ui/style/api/include/mine/ui/style/{Style,VisualStateManager}.h` |
| 属性系统 | `src/mine/ui/property/api/include/mine/ui/property/` |
| 单元测试 | `src/mine/ui/binding/test/BindingTest.cpp`（`binding_bind_property_*`） |
