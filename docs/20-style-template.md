# 20 — 样式、控件模板与主题系统（mine.ui.style）

> 本文档补充 [09-property-binding.md](09-property-binding.md)（§9.7 资源与样式驱动）和
> [03-mml-language.md](03-mml-language.md)（§3.7 样式与模板）的完整架构细节。
> 与 WPF/WinUI 的核心差异：**AOT 优先、无运行时 XAML 解析、无 RTTI 依赖**。

> ⚠️ **架构变更提示（2026-06）**：本文的 **Style / ResourceDictionary / 主题 / VisualStateManager**
> 部分仍然有效；但 **ControlTemplate 运行时模板方案已废弃**。控件外观的最终架构请以
> [22-appearance-architecture.md](22-appearance-architecture.md) 为准（继承式基元绘制 +
> 组合式装配 + DP↔DP 绑定），本文中 `ControlTemplate` 相关内容仅作历史参考。

---

## 20.1 设计目标与约束

### 目标

1. **可主题化（Themeable）**：支持浅色/深色/自定义主题，不改一行 C++ 代码。
2. **可样式化（Styleable）**：同一控件类型可通过不同 `Style` 呈现不同外观。
3. **可模板化（Templatable）**：控件的视觉树结构可通过 `ControlTemplate` 完全替换。
4. **AOT 优先**：`style` / `template` 块由 `mmlc` 在编译期处理，生成 C++ 代码；运行时只执行生成的函数，不解析标记字符串。
5. **性能**：模板树延迟构建（首次 `measure` 时），避免无效开销；状态切换走 `Storyboard` 管理，避免每帧全量重绘。

### 约束

- 禁用 RTTI：类型标识依赖 `mine::meta::TypeId`（编译期哈希），不使用 `typeid`。
- 禁用异常：所有失败路径通过 `mine::Result<T,E>` 或静默降级表达。
- ABI 安全：`Style` / `ControlTemplate` 对象可跨 DLL 边界传递，使用 PIMPL 包装。
- 线程约束：所有操作必须在拥有该元素的 UI 线程上调用。

---

## 20.2 三层架构总览

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 3：ResourceDictionary / Theme                        │
│  树形作用域键值存储；StaticResource（编译期）               │
│  DynamicResource（运行时查找 + 变更订阅）                   │
├─────────────────────────────────────────────────────────────┤
│  Layer 2：Style                                             │
│  属性 setter 集合（含状态伪类）；支持样式继承（BasedOn）    │
│  mmlc 生成 apply_<Name>() 函数                              │
├─────────────────────────────────────────────────────────────┤
│  Layer 1：ControlTemplate + VisualStateManager              │
│  控件视觉树结构描述；ContentPresenter 内容占位              │
│  mmlc 生成 build_<Name>() 函数；状态机驱动动画             │
└─────────────────────────────────────────────────────────────┘
         ↑ 依赖属性优先级链（见 §20.5）将三层整合
```

---

## 20.3 ResourceDictionary（资源字典）

### 20.3.1 树形作用域

资源字典挂载在 UI 树的各层节点上，查找时从当前节点向上逐层搜索：

```
Application（全局资源 + 主题资源）
  └─ Window
       └─ Page / UserControl
            └─ StackPanel
                 └─ Button   ← 查找从此开始，逐层向上
```

```cpp
// mine/ui/style/ResourceDictionary.h
class MINE_API ResourceDictionary {
public:
    // 写入本层资源（静态值）
    void set(StringView key, Variant value);

    // 写入动态资源（值改变时向下广播 resource_changed）
    void set_dynamic(StringView key, Variant value);

    // 合并另一个资源字典（主题切换时使用）
    void merge(const ResourceDictionary& other);

    // 清除已合并的字典（主题切换前调用）
    void clear_merged() noexcept;

    // 向祖先链查找（未找到返回 nullopt）
    [[nodiscard]] Optional<Variant> find(StringView key) const noexcept;

    // 严格只查本层
    [[nodiscard]] Optional<Variant> find_local(StringView key) const noexcept;

    // DynamicResource 订阅：key 变化时回调（返回取消令牌）
    HandlerId subscribe(StringView key, Function<void(const Variant&)> callback);
    void unsubscribe(HandlerId id) noexcept;

    // 连接到父字典（由布局系统在 attach 时设置）
    void set_parent(ResourceDictionary* parent) noexcept;

    // 资源变更广播（自动向子节点传播）
    Signal<StringView /*key*/> resource_changed;

private:
    HashMap<String, Variant>             local_;
    SmallVector<ResourceDictionary*, 4>  merged_;
    ResourceDictionary*                  parent_{nullptr};
    HashMap<String, SmallVector<std::pair<HandlerId, Function<void(const Variant&)>>, 4>> subs_;
    uint32_t                             next_id_{1};
};
```

### 20.3.2 StaticResource 与 DynamicResource

| 类型 | MML 语法 | 编译期行为 | 运行时行为 |
|------|----------|-----------|-----------|
| `StaticResource` | `{StaticResource Key}` | mmlc 在编译期将 key 内联为对应值（常量路径）| 无动态查找，值固定 |
| `DynamicResource` | `{DynamicResource Key}` | mmlc 生成订阅代码 | 查找最近祖先字典；订阅 `resource_changed`；主题切换时自动更新 |

> **设计原则**：颜色、字体等随主题变化的值用 `DynamicResource`；应用名称、版本号等固定值用 `StaticResource`。

### 20.3.3 主题系统

主题是一组命名资源字典，在 Application 层切换：

```mml
// Theme.mml — 主题资源文件
resources {
    theme Light {
        Color  PanelBg    = #FFFFFF;
        Color  TextPrimary = #212121;
        Color  TextSecondary = #757575;
        Color  AccentColor = #1976D2;
        Color  AccentHover = #1565C0;
        Color  AccentPress = #0D47A1;
        Brush  PrimaryBrush = SolidColorBrush { color: AccentColor; }
    }
    theme Dark {
        Color  PanelBg    = #1E1E1E;
        Color  TextPrimary = #FFFFFF;
        Color  TextSecondary = #BDBDBD;
        Color  AccentColor = #42A5F5;
        Color  AccentHover = #1E88E5;
        Color  AccentPress = #1565C0;
        Brush  PrimaryBrush = SolidColorBrush { color: AccentColor; }
    }
}
```

```cpp
// C++ 侧切换主题
Application::instance().set_theme("Dark");
// → 内部调用 global_resources_.clear_merged()
//            global_resources_.merge(theme_Dark_dict)
//            global_resources_.resource_changed.emit("*")  // 广播全量更新
```

---

## 20.4 Style（样式）

### 20.4.1 概念

`Style` 是一组针对特定控件类型的**属性 setter 集合**，可附带**状态伪类 setter**（Normal/Hovered/Pressed/Focused/Disabled 等）。

`Style` 支持继承（`BasedOn`）：子样式的 setter 覆盖父样式中同属性的 setter。

### 20.4.2 MML 语法

```mml
// 基础样式（匿名，通过 for 指定目标类型）
style AccentButton for Button {
    background:     {DynamicResource AccentColor};
    foreground:     White;
    corner-radius:  6;
    padding:        { left: 16; right: 16; top: 8; bottom: 8; }
    border-color:   Transparent;
    border-thickness: 0;

    // 状态伪类（无过渡；过渡动画在 ControlTemplate 的 states 块中定义）
    :hover    { background: {DynamicResource AccentHover}; }
    :pressed  { background: {DynamicResource AccentPress}; }
    :focused  { border-color: {DynamicResource AccentColor}; border-thickness: 2; }
    :disabled { background: #BDBDBD; foreground: #9E9E9E; opacity: 0.6; }
}

// 样式继承（BasedOn）
style LargeAccentButton : AccentButton for Button {
    font.size:  18;
    min-width:  120;
    padding:    { left: 24; right: 24; top: 12; bottom: 12; }
    // 未覆盖的属性继承自 AccentButton
}
```

### 20.4.3 C++ API

```cpp
// mine/ui/style/Style.h

/// @brief 单个属性设置器（静态值或 DynamicResource 键）
struct StyleSetter {
    const DependencyProperty* property;  ///< 目标属性（非空）
    Variant                   value;     ///< 静态值（res_key 为空时有效）
    String                    res_key;   ///< DynamicResource 键（非空时优先）
};

/// @brief 单个视觉状态下的 setter 集合
struct VisualStateSetters {
    String                     state_name;  ///< 如 "Hovered"、"Pressed"
    SmallVector<StyleSetter, 8> setters;
};

/// @brief 样式对象（mmlc 生成构造 + 注册，运行时只调用 apply()）
class MINE_API Style {
public:
    // mmlc 生成：注册到全局样式注册表（通过 StyleRegistry）
    using ApplyFn = void (*)(Control&);
    ApplyFn apply_fn_{nullptr};  ///< 由 mmlc 生成的 apply 函数

    [[nodiscard]] meta::TypeId target_type() const noexcept { return target_type_; }
    [[nodiscard]] Style*       base()        const noexcept { return base_;        }
    [[nodiscard]] StringView   name()        const noexcept { return name_;        }

    /// @brief 将样式应用到控件（依次调用父样式、本样式的 apply_fn_）
    void apply(Control& target) const;

    /// @brief 应用特定状态的 setter（不触发动画，仅设置属性值到样式优先级层）
    void apply_state(Control& target, StringView state_name) const;

private:
    meta::TypeId              target_type_;
    String                    name_;
    Style*                    base_{nullptr};   ///< BasedOn 父样式
    SmallVector<StyleSetter, 16>          setters_;
    SmallVector<VisualStateSetters, 8>    state_setters_;
};
```

### 20.4.4 mmlc 生成代码示例

输入 MML：
```mml
style AccentButton for Button {
    background: {DynamicResource AccentColor};
    foreground: White;
    :hover { background: {DynamicResource AccentHover}; }
}
```

生成 C++（`AccentButton.style.g.cpp`）：
```cpp
// 自动生成，勿手动修改

static void apply_AccentButton_impl(mine::ui::Control& target) {
    using namespace mine::ui;
    auto& btn = static_cast<Button&>(target);

    // 静态 setter
    btn.set_style_value(Button::ForegroundProperty,
                         mine::math::Color::White);
    // DynamicResource setter（运行时从资源字典查找并订阅）
    btn.set_style_dynamic(Button::BackgroundProperty, "AccentColor");

    // 状态 setter 注册
    btn.register_style_state("Hovered", {
        StyleSetter{ &Button::BackgroundProperty, {}, "AccentHover" },
    });
}

// 全局初始化注册（在 .g.cpp 文件中通过静态对象注册）
static const auto s_AccentButton_reg =
    StyleRegistry::instance().register_style(
        "AccentButton",
        mine::meta::TypeId::of<mine::ui::Button>(),
        &apply_AccentButton_impl,
        /*base=*/nullptr
    );
```

---

## 20.5 依赖属性优先级链整合

Style/Template 系统通过依赖属性的**值优先级链**与绑定系统整合（见 §9.1）。
完整优先级从高到低：

| 优先级 | 来源 | 说明 |
|--------|------|------|
| 1（最高）| **动画值** | Storyboard 正在插值的值 |
| 2 | **本地值** | `set_value()` / MML 内联属性 |
| 3 | **TemplateBinding** | 模板内元素绑定到宿主控件属性 |
| 4 | **样式状态 setter** | 当前视觉状态（Hovered/Pressed 等）的样式值 |
| 5 | **样式 setter** | `Style::apply()` 写入的基线值 |
| 6 | **继承值** | 沿视觉树向下传播的 `inherits=true` 属性 |
| 7（最低）| **默认值** | `DependencyProperty` 注册时指定的 `default_value` |

> 实现要点：`DependencyObject::get_value()` 按优先级链从高到低取第一个有效层。
> `set_style_value()` 写入优先级 5；`set_style_dynamic()` 订阅资源变更后也写入优先级 5。
> 动画通过 `set_animation_value()` 写入优先级 1，动画结束后清除（回落到下一层）。

---

## 20.6 ControlTemplate（控件模板）

### 20.6.1 概念

`ControlTemplate` 描述控件的**视觉树结构**，通过 `ContentPresenter` 承载控件的内容属性。
控件实例在首次 `measure()` 时调用 `build_template()` 构建模板树，并将模板根节点作为视觉子元素。

与 WPF 的关键差异：
- 无运行时 XAML 解析，`build_template()` 是 mmlc 预生成的 C++ 函数。
- `TemplateBinding` 在 `build_template()` 时静态连接，不参与依赖追踪（单向、同步刷新）。
- 模板树的 `VisualStateManager` 由 mmlc 预配置，控件自身只需调用 `go_to_state()`。

### 20.6.2 MML 语法

```mml
template DefaultButton for Button {
    // 模板根元素（通常是 Border）
    Border#border {
        // TemplateBinding：冒号后跟 control.属性名
        background:        control.background;
        corner-radius:     control.corner-radius;
        border-color:      control.border-color;
        border-thickness:  control.border-thickness;

        // ContentPresenter：承载控件的 Content 属性
        ContentPresenter#content {
            content:  control.content;
            padding:  control.padding;
            h-align:  Center;
            v-align:  Center;
        }
    }

    // 模板级状态机（覆盖/扩展控件内置状态）
    states {
        state Normal;
        state Hovered  when control.is-hovered;
        state Pressed  when control.is-pressed;
        state Focused  when control.is-focused;
        state Disabled when !control.is-enabled;

        // 状态过渡（定义动画，区别于 Style 的静态 setter）
        transition * -> Hovered {
            animate #border.background duration: 120ms easing: ease-out;
        }
        transition * -> Pressed {
            animate #border.background duration: 60ms easing: ease-in;
            animate transform.scale to: 0.97 duration: 60ms easing: ease-in;
        }
        transition Pressed -> * {
            animate transform.scale to: 1.0 duration: 100ms easing: ease-out;
        }
        transition * -> Focused {
            animate #border.border-thickness to: 2 duration: 80ms;
        }
    }
}
```

### 20.6.3 ContentPresenter

`ContentPresenter` 是模板树中的**内容占位元素**，根据 `content` 属性的类型渲染内容：

| `content` 类型 | 渲染方式 |
|---------------|---------|
| `string` / `StringView` | 创建内联 `TextBlock` 渲染文本 |
| `FrameworkElement*` | 将元素加入视觉树（控件 content 子树） |
| `nullptr` | 不渲染 |

```cpp
// mine/ui/controls/basic/ContentPresenter.h
class MINE_API ContentPresenter : public FrameworkElement {
public:
    static const DependencyProperty& ContentProperty;
    static const DependencyProperty& PaddingProperty;

    void set_content(Variant v) noexcept;
    [[nodiscard]] const Variant& content() const noexcept;

protected:
    math::Size2f measure_override(math::Size2f available) noexcept override;
    math::Size2f arrange_override(math::Size2f final_size) noexcept override;
    void         on_render(paint::Canvas& canvas) noexcept override;

private:
    // 实际持有的内容视觉树（string → TextBlock；Element → 直接引用）
    FrameworkElement* content_visual_{nullptr};
    // string 内容时使用（通过 MINE_NEW 延迟创建，首次 on_content_changed 时构造）
    TextBlock*        inline_text_block_{nullptr};
    bool              using_inline_{false};
};
```

### 20.6.4 C++ API（ControlTemplate）

```cpp
// mine/ui/style/ControlTemplate.h
class MINE_API ControlTemplate {
public:
    [[nodiscard]] meta::TypeId target_type() const noexcept { return target_type_; }
    [[nodiscard]] StringView   name()        const noexcept { return name_;        }

    // mmlc 生成的构建函数
    using BuildFn = void (*)(Control&);
    BuildFn build_fn_{nullptr};

    /// @brief 构建模板树，挂载到 control 的视觉子树
    void build(Control& target) const;

private:
    meta::TypeId target_type_;
    String       name_;
};
```

### 20.6.5 TemplateBinding

`TemplateBinding` 是模板内元素到宿主控件属性的单向绑定，在 `build_template()` 时建立，
不参与依赖追踪（因为宿主属性变化时已有 `invalidate()` 传播），只在 `arrange`/`render` 时同步：

```cpp
// Control::bind_template() 内部实现
void Control::bind_template(
    FrameworkElement&         template_child,
    const DependencyProperty& child_prop,
    const DependencyProperty& host_prop) noexcept
{
    // 1. 立即同步当前值
    template_child.set_value(child_prop,
                              get_value(host_prop), ValuePriority::TemplateBinding);
    // 2. 订阅宿主属性变更
    on_property_changed(host_prop, [&template_child, &child_prop](const Variant& v) {
        template_child.set_value(child_prop, v, ValuePriority::TemplateBinding);
    });
}
```

### 20.6.6 mmlc 生成代码示例

输入 MML（`template DefaultButton for Button { ... }`），生成：

```cpp
// DefaultButton.template.g.cpp（自动生成，勿手动修改）
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/basic/Border.h>
#include <mine/ui/controls/basic/ContentPresenter.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/animation/Storyboard.h>

static void build_DefaultButton_impl(mine::ui::Control& ctrl) {
    using namespace mine::ui;
    auto& button = static_cast<Button&>(ctrl);

    // ── 构建视觉树 ────────────────────────────────────────────────
    auto* border = MINE_NEW(Border);
    border->set_template_name("border");
    button.bind_template(*border, Border::BackgroundProperty,       Button::BackgroundProperty);
    button.bind_template(*border, Border::CornerRadiusProperty,     Button::CornerRadiusProperty);
    button.bind_template(*border, Border::BorderColorProperty,      Button::BorderColorProperty);
    button.bind_template(*border, Border::BorderThicknessProperty,  Button::BorderThicknessProperty);

    auto* presenter = MINE_NEW(ContentPresenter);
    presenter->set_template_name("content");
    button.bind_template(*presenter, ContentPresenter::ContentProperty, Button::ContentProperty);
    button.bind_template(*presenter, ContentPresenter::PaddingProperty, Button::PaddingProperty);
    presenter->set_h_align(HorizontalAlignment::Center);
    presenter->set_v_align(VerticalAlignment::Center);
    border->add_visual_child(presenter);

    button.set_template_root(border);  // 将 border 加入 button 的视觉子树

    // ── VisualStateManager ────────────────────────────────────────
    VisualStateManager vsm{button};

    // 状态定义（条件由控件驱动，VSM 仅注册名称）
    vsm.define_state("Normal");
    vsm.define_state("Hovered");
    vsm.define_state("Pressed");
    vsm.define_state("Focused");
    vsm.define_state("Disabled");

    // 过渡动画（from="*" 表示任意源状态）
    vsm.add_transition("*", "Hovered", [border](Storyboard& sb) {
        sb.animate_dp(border, Border::BackgroundProperty,
                      Duration::milliseconds(120), CubicEaseOut);
    });
    vsm.add_transition("*", "Pressed", [border, &button](Storyboard& sb) {
        sb.animate_dp(border, Border::BackgroundProperty,
                      Duration::milliseconds(60), CubicEaseIn);
        sb.animate_dp_to(&button, UIElement::LocalTransformProperty,
                          math::Transform2D::scale(0.97f),
                          Duration::milliseconds(60), CubicEaseIn);
    });
    vsm.add_transition("Pressed", "*", [&button](Storyboard& sb) {
        sb.animate_dp_to(&button, UIElement::LocalTransformProperty,
                          math::Transform2D::identity(),
                          Duration::milliseconds(100), CubicEaseOut);
    });
    vsm.add_transition("*", "Focused", [border](Storyboard& sb) {
        sb.animate_dp_to(border, Border::BorderThicknessProperty,
                          Thickness{2}, Duration::milliseconds(80));
    });

    button.set_visual_state_manager(std::move(vsm));
}

// 全局注册
static const auto s_DefaultButton_template_reg =
    TemplateRegistry::instance().register_template(
        "DefaultButton",
        mine::meta::TypeId::of<mine::ui::Button>(),
        &build_DefaultButton_impl
    );
```

---

## 20.7 VisualStateManager（视觉状态管理器）

### 20.7.1 状态驱动流程

```
控件属性变化（is_hovered / is_pressed / is_focused 等）
    │
    ▼
Control::update_visual_state()   ← 控件自身调用
    │ 计算当前应进入的状态名
    ▼
VisualStateManager::go_to_state(state_name, use_transitions)
    │
    ├─ 查找从 current_state_ → state_name 的过渡配置
    │   存在 → 启动 Storyboard（动画过渡）
    │   不存在 → 立即切换（用 Style 状态 setter 更新属性）
    │
    └─ 更新 current_state_ = state_name
```

### 20.7.2 C++ API

```cpp
// mine/ui/style/VisualStateManager.h
class MINE_API VisualStateManager {
public:
    explicit VisualStateManager(Control& owner) noexcept;

    // 状态注册（由 mmlc 生成的 build_template 函数调用）
    void define_state(StringView name) noexcept;

    // 过渡注册（from/to 可为 "*" 表示通配）
    void add_transition(StringView from, StringView to,
                        Function<void(Storyboard&)> configure_fn) noexcept;

    // 状态切换（由控件在 update_visual_state() 中调用）
    bool go_to_state(StringView state_name, bool use_transitions = true) noexcept;

    [[nodiscard]] StringView current_state() const noexcept { return current_state_; }

private:
    Control&    owner_;
    String      current_state_;
    SmallVector<String, 8>                                     states_;
    SmallVector<VisualStateTransition, 16>                     transitions_;
    // 当前正在播放的过渡动画（使用 mine::core::OwnedPtr 而非 std::unique_ptr）
    SmallVector<mine::core::OwnedPtr<Storyboard>, 4>           active_storyboards_;
};

struct VisualStateTransition {
    String from;  // 空或 "*" 匹配任意
    String to;
    Function<void(Storyboard&)> configure;
};
```

### 20.7.3 控件侧集成

控件基类需要实现以下接口：

```cpp
// mine/ui/controls/Control.h（扩展）
class MINE_API Control : public FrameworkElement {
public:
    // ── 样式 / 模板 ──────────────────────────────────────────────

    /// @brief 获取/设置当前应用的样式
    [[nodiscard]] Style*          style()    const noexcept { return style_;    }
    [[nodiscard]] ControlTemplate* templ()   const noexcept { return template_; }
    void set_style(Style* s)              noexcept;
    void set_template(ControlTemplate* t) noexcept;

    /// @brief 在模板树中按名称查找元素（#name 对应 set_template_name()）
    [[nodiscard]] FrameworkElement* find_template_child(StringView name) const noexcept;

    /// @brief 设置模板根（由 build_template 调用；将根节点加入视觉子树）
    void set_template_root(FrameworkElement* root) noexcept;

    /// @brief 将模板内子元素绑定到宿主控件属性（TemplateBinding）
    void bind_template(FrameworkElement&        template_child,
                       const DependencyProperty& child_prop,
                       const DependencyProperty& host_prop) noexcept;

    // ── VisualStateManager ───────────────────────────────────────

    void set_visual_state_manager(VisualStateManager vsm) noexcept;
    [[nodiscard]] VisualStateManager* vsm() noexcept { return vsm_.has_value() ? &vsm_.value() : nullptr; }

    // ── 样式值写入（区分优先级）────────────────────────────────

    /// @brief 写入样式基线值（优先级 5）
    void set_style_value(const DependencyProperty& prop, Variant value) noexcept;

    /// @brief 写入 DynamicResource 订阅（优先级 5，资源变化时自动更新）
    void set_style_dynamic(const DependencyProperty& prop, StringView res_key) noexcept;

    /// @brief 注册某状态下的样式 setter（切换状态时写入优先级 4）
    void register_style_state(StringView state_name,
                               SmallVector<StyleSetter, 8> setters) noexcept;

protected:
    // ── 子类重写 ─────────────────────────────────────────────────

    /// @brief 模板构建完成后回调（子类可在此做 find_template_child 查找）
    virtual void on_apply_template() noexcept {}

    /// @brief 计算当前应进入的视觉状态名（子类实现，例如："Hovered"/"Normal"）
    /// 默认实现：根据 is_enabled / is_hovered / is_pressed / is_focused 返回状态名
    virtual StringView compute_visual_state() const noexcept;

    /// @brief 在鼠标进入/离开/按下等事件中调用，触发状态机
    void update_visual_state(bool use_transitions = true) noexcept;

private:
    Style*                    style_{nullptr};
    ControlTemplate*          template_{nullptr};
    FrameworkElement*         template_root_{nullptr};
    Optional<VisualStateManager> vsm_;
    // DynamicResource 订阅令牌列表（析构时自动取消）
    SmallVector<HandlerId, 8> dynamic_res_subs_;
};
```

---

## 20.8 StyleRegistry 与 TemplateRegistry

所有 `Style` / `ControlTemplate` 对象在程序启动时通过静态对象注册，由 mmlc 生成的 `.g.cpp` 文件负责注册：

```cpp
// mine/ui/style/StyleRegistry.h
class MINE_API StyleRegistry {
public:
    static StyleRegistry& instance() noexcept;

    /// @brief 注册样式（由 mmlc 生成代码调用，程序启动时执行）
    const Style& register_style(StringView      name,
                                 meta::TypeId    target_type,
                                 Style::ApplyFn  apply_fn,
                                 const Style*    base = nullptr) noexcept;

    /// @brief 按名称查找样式
    [[nodiscard]] const Style* find(StringView name) const noexcept;

    /// @brief 查找某类型的默认样式（名称约定：`Default<TypeName>`）
    [[nodiscard]] const Style* find_default(meta::TypeId type) const noexcept;

private:
    HashMap<String, Style> styles_;
};

// mine/ui/style/TemplateRegistry.h
class MINE_API TemplateRegistry {
public:
    static TemplateRegistry& instance() noexcept;

    const ControlTemplate& register_template(StringView             name,
                                              meta::TypeId           target_type,
                                              ControlTemplate::BuildFn build_fn) noexcept;

    [[nodiscard]] const ControlTemplate* find(StringView name) const noexcept;
    [[nodiscard]] const ControlTemplate* find_default(meta::TypeId type) const noexcept;

private:
    HashMap<String, ControlTemplate> templates_;
};
```

---

## 20.9 完整流程示例

以 `Button` 为例，从创建到渲染的完整流程：

```
1. 程序启动
   └─ .g.cpp 中的静态对象执行 → StyleRegistry/TemplateRegistry 注册

2. build_ui()（用户代码）
   auto* btn = MINE_NEW(Button);
   btn->set_style(StyleRegistry::instance().find("AccentButton"));
   // → style_->apply(*btn) 调用 apply_AccentButton_impl()
   //   → set_style_value(BackgroundProperty, ...)（优先级 5）
   //   → set_style_dynamic(BackgroundProperty, "AccentColor")（订阅资源）
   //   → register_style_state("Hovered", {...})

3. 首次 measure()（布局系统调用）
   └─ Control::measure() 检测到 template_root_ == nullptr
      → find template：TemplateRegistry::instance().find_default(TypeId::of<Button>())
      → template_->build(*btn)（执行 build_DefaultButton_impl）
        → 创建 Border / ContentPresenter
        → bind_template(...)（TemplateBinding 连接）
        → set_visual_state_manager(...)（注册状态机）
        → on_apply_template()（子类钩子）
      → 递归 measure 模板树

4. 鼠标悬停
   └─ MouseEnter 事件 → Button::on_mouse_enter()
      → update_visual_state()
      → compute_visual_state() → "Hovered"
      → vsm_->go_to_state("Hovered", use_transitions=true)
        → 查找 "* -> Hovered" 过渡 → 启动 Storyboard
          → Storyboard::animate_dp(border, BackgroundProperty, 120ms, CubicEaseOut)
          → 从当前值（AccentColor）动画到 style["Hovered"].background（AccentHover）

5. 主题切换
   └─ Application::set_theme("Dark")
      → global_resources_.merge(dark_dict)
      → resource_changed.emit("AccentColor")
        → 所有订阅了 "AccentColor" 的控件回调
        → btn.set_style_value(BackgroundProperty, dark_AccentColor)（更新优先级 5）
        → invalidate() → 下一帧重绘
```

---

## 20.10 MML 中应用样式与模板

### 20.10.1 元素内联应用

```mml
component MainView : UserControl {
    StackPanel {
        // 直接指定样式名
        Button {
            style: AccentButton;
            content: "强调按钮";
        }

        // 同时指定自定义模板
        Button {
            style:    AccentButton;
            template: RoundButton;   // 自定义圆形按钮模板
            content:  "圆形";
        }
    }
}
```

### 20.10.2 隐式默认样式

框架约定：样式/模板名以 `Default` 开头时作为该类型的默认样式/模板：

```mml
style  DefaultButton  for Button { ... }   // Button 默认样式
template DefaultButton for Button { ... }  // Button 默认模板
```

控件创建时若未显式设置 `style`，自动查找 `Default<TypeName>` 样式应用。

### 20.10.3 类选择器（批量应用）

```mml
// 在资源或样式文件中定义
style .accent for Button {
    background: {DynamicResource AccentColor};
    foreground: White;
}

// 在视图中使用（MML 扩展，M3+ 阶段）
Button[class="accent"] { content: "蓝色按钮"; }
```

> **M2 阶段限制**：类选择器暂不实现，仅支持类型选择器（`for TypeName`）和命名样式（`style: StyleName`）。

---

## 20.11 模块结构（mine.ui.style）

```
src/mine/ui/style/
├─ xmake.lua
├─ api/include/mine/ui/style/
│   ├─ ResourceDictionary.h    # §20.3
│   ├─ Style.h                 # §20.4
│   ├─ StyleSetter.h           # StyleSetter / VisualStateSetters 结构体
│   ├─ StyleRegistry.h         # §20.8
│   ├─ ControlTemplate.h       # §20.6
│   ├─ TemplateRegistry.h      # §20.8
│   ├─ VisualStateManager.h    # §20.7
│   └─ ContentPresenter.h      # §20.6.3（可归入 controls.basic）
└─ src/
    ├─ ResourceDictionary.cpp
    ├─ Style.cpp
    ├─ StyleRegistry.cpp
    ├─ ControlTemplate.cpp
    ├─ TemplateRegistry.cpp
    └─ VisualStateManager.cpp
```

### 依赖关系

```
mine.ui.style
  ├─ mine.ui.property     # DependencyProperty / DependencyObject
  ├─ mine.ui.binding      # BindingExpression（DynamicResource 订阅）
  ├─ mine.ui.animation    # Storyboard（状态过渡动画）
  ├─ mine.ui.layout       # FrameworkElement（TemplateBinding）
  └─ mine.meta            # TypeId（无 RTTI 类型标识）
```

---

## 20.12 M2 阶段最小实现范围

M2 阶段实现以**能够替换当前手绘控件的外观**为目标，分三步落地：

### 阶段 A：ResourceDictionary + 主题切换（M2.2 优先）

- [ ] `ResourceDictionary`：本地存储、祖先查找、`resource_changed` 信号
- [ ] `Application::set_theme()`：合并/切换主题资源字典
- [ ] `Theme.mml` → 生成 C++ 主题资源注册代码
- [ ] `DynamicResource` 订阅机制（`Control::set_style_dynamic()`）

### 阶段 B：Style 系统（M2.2 跟进）

- [ ] `Style` / `StyleSetter` / `StyleRegistry`
- [ ] `Control::set_style_value()`（优先级 5）
- [ ] `mmlc` 支持 `style ... for ...` 块代码生成
- [ ] 状态 setter：`:hover`、`:pressed`、`:disabled`（无过渡动画，仅跳变）
- [ ] `Control::update_visual_state()` + `compute_visual_state()` 默认实现

### 阶段 C：ControlTemplate + VisualStateManager（M2.2 或 M3 初）

- [ ] `ContentPresenter`（支持 string / FrameworkElement 两种内容类型）
- [ ] `ControlTemplate` / `TemplateRegistry`
- [ ] `Control::set_template_root()` / `bind_template()` / `find_template_child()`
- [ ] `VisualStateManager`：状态定义、过渡动画配置、`go_to_state()`
- [ ] `mmlc` 支持 `template ... for ...` 块代码生成（含 `states` 子块）
- [ ] 将现有 Button/CheckBox/RadioButton 等控件的 `on_render` 手绘逻辑迁移到 DefaultXxx 模板

### M2 阶段不实现（延迟到 M3+）

- 类选择器（`.accent` 等）
- 数据触发器（DataTrigger，依赖 ViewModel 属性驱动样式）
- 样式覆盖/优先级冲突仲裁 UI（工具层）
- 控件模板的热重载（M7）

---

## 20.13 与现有手绘控件的过渡策略

当前（M2）控件在 `on_render()` 中直接用 `Canvas` 手绘。引入 Style/Template 系统后，过渡方案：

1. **优先级保持向后兼容**：控件的 `on_render()` 手绘逻辑等同于"内置默认外观"，在没有 `ControlTemplate` 时仍然有效。
2. **渐进迁移**：为每个控件新增 `DefaultXxx.mml` → 生成 `build_template()` → `on_render()` 改为渲染模板树（而非直接手绘）。
3. **opt-in**：先迁移 `Button`，验证端到端流程后再推广到其他控件。
4. **过渡期并存**：`has_custom_template()` 返回 `true` 时走模板树渲染，否则走 `on_render()` 手绘（临时开关，M3 结束后移除）。

---

## 20.14 参考与对比

| 特性 | WPF / WinUI 3 | MineFramework |
|------|--------------|---------------|
| 模板实例化 | 运行时解析 XAML，展开元素树 | AOT 生成 C++ `build_template()` 函数 |
| 资源查找 | `ResourceDictionary` 运行时遍历 | 编译期常量（`StaticResource`）+ 运行时订阅（`DynamicResource`） |
| 触发器 | Trigger / DataTrigger（运行时反射） | `VisualStateManager` + mmlc 生成的状态机 |
| 模板绑定 | `{TemplateBinding Prop}` 运行时解析 | 编译期 `bind_template()` 调用（强类型） |
| RTTI 依赖 | 高（大量 `dynamic_cast`） | 无（使用 `mine::meta::TypeId`） |
| 主题切换 | `ResourceDictionary.MergedDictionaries` | `Application::set_theme()` + `ResourceDictionary::merge()` |
| 类选择器 | 无原生 CSS 类选择器 | M3+ 计划（有限支持） |
| 适用场景 | 桌面 Windows 应用 | 桌面 + 嵌入式 + 游戏 UI，跨平台 |
