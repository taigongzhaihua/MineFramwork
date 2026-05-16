# 03 — MML 标记语言规格

> Mine Markup Language（`.mml`）：声明 UI 树、样式、绑定、动画、组件契约。
> 设计灵感：Slint（声明式、AOT、组件即类型）+ QML（信号/属性/JS 表达式）+ XAML（依赖属性、模板、资源）。
> 关键差异：**强类型、AOT 编译为 C++、不内嵌脚本语言**，所有"表达式"都是受限的 MML 表达式子集，可被静态求值或代码生成。

## 3.1 文件结构

一个 `.mml` 文件描述一个**组件**或**资源集**：

```mml
// 顶部指令
@module app.views;                // 命名空间，对应 C++ namespace app::views
@using mine.ui.controls.basic.*;
@using mine.ui.controls.input.*;
@import "./Theme.mml" as Theme;   // 引入其它文件
@viewmodel app::vm::LoginVM;      // 关联的 C++ ViewModel 类型（强类型）

// 资源（编译期常量）
resources {
    Color  AccentColor = #1976D2;
    Brush  PrimaryBrush = SolidColorBrush { Color: AccentColor; }
}

// 组件定义：组件名即类型名
component LoginView : UserControl {

    // 属性声明：自动生成依赖属性
    property string Title = "登录";
    property bool   Busy = false;

    // 信号
    signal loginRequested(string user);

    // 视觉树
    StackPanel {
        spacing: 8;
        padding: 16;

        TextBlock {
            text: Title;
            font.size: 24;
        }
        TextBox#userBox {
            placeholder: "用户名";
            text <=> vm.UserName;          // 双向绑定
        }
        PasswordBox#pwdBox {
            text <=> vm.Password;
        }
        Button {
            content: Busy ? "登录中…" : "登录";
            enabled: !Busy && vm.CanLogin;
            click => vm.LoginCommand.execute(userBox.text);
        }
    }

    // 状态机
    states {
        state Normal  when !Busy;
        state Loading when  Busy;
        transition Normal -> Loading {
            animate Busy duration: 200ms easing: ease-out;
        }
    }
}
```

## 3.2 词法

* 单行注释 `// ...`，多行 `/* ... */`。
* 标识符：`[A-Za-z_][A-Za-z0-9_]*`。
* 数字字面量：`123`, `1.5`, `12px`, `1.5em`, `200ms`, `#RRGGBB[AA]`。
* 字符串：双引号 `"..."`，支持 `${expr}` 插值（编译期或绑定）。
* 关键字：`component`, `property`, `signal`, `state`, `transition`, `when`, `animate`, `if`, `else`, `for`, `in`, `resources`, `states`, `using`, `module`, `viewmodel`, `template`, `style`, `null`, `true`, `false`。
* `@` 前缀**顶部指令**（编译器指令，词法上独立于关键字）：`@module`, `@using`, `@import`, `@viewmodel`。顶部指令只允许出现在文件开头，不参与表达式/语句语法，词法器在行首遇到 `@` 时切换到指令模式识别。
* 操作符：`= == != < <= > >= && || ! + - * / % ?: ?? -> => <=>`。
* `=>` 表示**事件处理体**，`<=>` 表示**双向绑定**，`->` 表示状态/动画箭头。

## 3.3 语法（节选 BNF）

```
file       := directive* declaration*
directive  := '@module'    qid ';'
            | '@using'     qid ('.*')? ';'
            | '@import'    string 'as' ident ';'
            | '@viewmodel' qid ';'
            // 注：directive 前缀 '@' 由词法器在文件头部特殊处理，不属于普通关键字
declaration:= component | resourceBlock | styleBlock | templateBlock
component  := 'component' ident (':' qid)? '{' member* '}'
member     := propertyDecl | signalDecl | childElement | statesBlock
            | resourceBlock | styleBlock
propertyDecl:= 'property' type ident ('=' expr)? ';'
signalDecl  := 'signal' ident '(' paramList? ')' ';'
childElement:= qid ('#' ident)? '{' attribute* '}'
attribute   := ident ':' expr ';'                  // 单向赋值/属性
            |  ident '<=>' expr ';'                // 双向绑定
            |  ident '=>' block                    // 事件
            |  childElement                        // 嵌套元素
expr        := /* 见 §3.5 */
```

## 3.4 类型系统

| 类型 | 对应 C++ |
|------|----------|
| `int` | `int32_t` |
| `int64` | `int64_t` |
| `float` | `float` |
| `double` | `double` |
| `bool` | `bool` |
| `string` | `mine::String` |
| `length` | `mine::ui::Length`（支持 `12px / 1em / 50%`） |
| `duration` | `mine::Duration` |
| `color` | `mine::Color` |
| `brush` | `mine::paint::Brush` |
| `vec2/3/4` | `mine::math::Vec*` |
| `enum<E>` | C++ enum class |
| `array<T>` | `mine::Vector<T>` |
| `T?` | `mine::Optional<T>` |
| 组件类型 | 该组件的 C++ class |

每个**组件即一个类型**，可作为属性类型、集合元素、模板参数。

## 3.5 表达式语言

子集，明确**禁止**：循环、while、自由函数定义。
**允许**：

* 字面量、属性访问 `a.b.c`、索引 `arr[i]`、命名空间访问 `Theme::AccentColor`。
* 一元/二元/三元运算、`??`、`?:`。
* 插值字符串 `${...}`。
* 函数调用：仅限于在 `@using` 暴露的纯函数集合及组件方法。
* `for ... in ...` 仅在 `Repeater` 元素的 `items` 上下文使用：
  ```mml
  Repeater { items: vm.Users; ItemTemplate: UserItem { user: $item; } }
  ```

所有表达式被分类为：

1. **常量表达式**：编译期求值，落到 const 数据。
2. **绑定表达式**：含动态属性引用，被 `mmlc` 转换成 `BindingExpression` 实例（编译期生成的 lambda + 依赖列表）。
3. **事件表达式**：仅在 `=>` 事件体允许，编译为 C++ lambda。

## 3.6 绑定语义

| 语法 | 模式 | 触发方向 |
|------|------|----------|
| `prop: expr` | OneWay（依赖追踪） | 数据 → UI |
| `prop <=> expr` | TwoWay | 双向 |
| `prop: bind(expr, mode: OneTime)` | 一次性 | — |
| `prop: bind(expr, converter: X, fallback: Y)` | 显式 | — |

绑定路径中的**每一段**都参与依赖订阅；中间节点为 `null` 时绑定保持 `Unset` 状态并使用回退值。

## 3.7 样式与模板

> 完整架构设计见 [20-style-template.md](20-style-template.md)，本节为 MML 语法速查。

### 3.7.1 资源字典（resources 块）

```mml
// 全局资源文件（Theme.mml）或组件内嵌 resources 块
resources {
    Color  AccentColor   = #1976D2;
    Color  AccentHover   = #1565C0;
    Brush  PrimaryBrush  = SolidColorBrush { color: AccentColor; }
    string AppTitle      = "MineUI 示例";

    // 主题分支（Application::set_theme("Dark") 时切换到此组）
    theme Light {
        Color PanelBg    = #FFFFFF;
        Color TextPrimary = #212121;
    }
    theme Dark {
        Color PanelBg    = #1E1E1E;
        Color TextPrimary = #FFFFFF;
    }
}
```

引用语法：

| 类型 | 语法 | 编译行为 |
|------|------|---------|
| 静态资源（编译期固定） | `{StaticResource Key}` | mmlc 内联为常量 |
| 动态资源（随主题变化） | `{DynamicResource Key}` | 生成订阅代码，主题切换时自动更新 |

### 3.7.2 style 块

```mml
// 基础样式：指定目标类型，包含属性 setter + 状态伪类
style AccentButton for Button {
    background:      {DynamicResource AccentColor};  // 动态资源
    foreground:      White;
    corner-radius:   6;
    padding:         { left: 16; right: 16; top: 8; bottom: 8; }
    border-color:    Transparent;
    border-thickness: 0;

    // 状态伪类（:hover / :pressed / :focused / :disabled）
    // 伪类 setter 无过渡动画，过渡动画在 template 的 states 块中定义
    :hover    { background: {DynamicResource AccentHover}; }
    :pressed  { background: {DynamicResource AccentPress}; }
    :focused  { border-color: {DynamicResource AccentColor}; border-thickness: 2; }
    :disabled { background: #BDBDBD; foreground: #9E9E9E; opacity: 0.6; }
}

// 样式继承（BasedOn）：未覆盖的属性继承自父样式
style LargeAccentButton : AccentButton for Button {
    font.size:  18;
    min-width:  120;
    padding:    { left: 24; right: 24; top: 12; bottom: 12; }
}
```

在组件内应用样式：

```mml
Button {
    style: AccentButton;   // 按名称指定
    content: "点击我";
}
```

隐式默认样式约定：命名为 `Default<TypeName>` 的样式在控件创建时自动应用。

### 3.7.3 template 块

```mml
// 控件模板：描述视觉树结构，control. 访问宿主控件属性（TemplateBinding）
template DefaultButton for Button {
    // 模板根元素
    Border#border {
        background:        control.background;        // TemplateBinding
        corner-radius:     control.corner-radius;
        border-color:      control.border-color;
        border-thickness:  control.border-thickness;

        // ContentPresenter：承载控件的 Content 属性
        // 根据 content 类型（string / FrameworkElement）自动渲染
        ContentPresenter#content {
            content:  control.content;
            padding:  control.padding;
            h-align:  Center;
            v-align:  Center;
        }
    }

    // 模板级状态机（定义过渡动画，区别于 style 的静态跳变）
    states {
        state Normal;
        state Hovered  when control.is-hovered;
        state Pressed  when control.is-pressed;
        state Focused  when control.is-focused;
        state Disabled when !control.is-enabled;

        // transition：from -> to，可用 * 通配任意源状态
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

在组件内指定自定义模板：

```mml
Button {
    style:    AccentButton;
    template: RoundButton;   // 可单独替换视觉树，保留样式
    content:  "圆形";
}
```

### 3.7.4 mmlc 代码生成策略

| MML 块 | 生成产物 | 运行时调用方式 |
|--------|---------|--------------|
| `resources { theme ... }` | `Theme_Light.g.cpp` / `Theme_Dark.g.cpp`，注册资源字典 | `Application::set_theme("Dark")` |
| `style X for T { ... }` | `X.style.g.cpp`，调用 `StyleRegistry::register_style()` | `Control::set_style()` |
| `template X for T { ... }` | `X.template.g.cpp`，调用 `TemplateRegistry::register_template()` | `Control::set_template()`（或首次 measure 时自动应用默认模板） |

所有注册均通过静态对象在程序启动时完成，无运行时 XAML 解析开销。

## 3.8 状态与动画

```mml
states {
    state Normal;
    state Hover when isHovered;
    state Pressed when isPressed;

    transition * -> Hover { animate background duration: 120ms easing: ease-out; }
    transition * -> Pressed { animate transform.scale duration: 80ms; }
}
```

动画类型：

* `animate <prop> ...`：隐式 Tween。
* `Storyboard { ... }`：显式时间线。
* 缓动函数：`linear/ease/ease-in/ease-out/cubic-bezier(...)/spring(...)`。

## 3.9 国际化

```mml
text: tr("welcome.title", { user: vm.UserName });
```

`tr(key, params)` 在 `mmlc` 中登记到资源表，由 `mine.localization` 在运行期解析。

## 3.10 与 C++ 的双向接口

* `@viewmodel` 指定的 C++ 类型必须满足以下契约（被 mmlc 静态校验）：
  * 继承 `mine::mvvm::ViewModelBase`；
  * 暴露的属性使用 `MINE_PROP(...)` 反射宏；
  * 命令为 `mine::mvvm::ICommand*`。
* 组件可被其他 C++ 代码以 `app::views::LoginView` 类型直接构造、设置属性、订阅信号。
* C++ 端的属性/事件可在 MML 中作为目标。

## 3.11 设计期/热重载语义

* `@designer` 指令开启设计器模拟数据：
  ```mml
  @designer DesignData { vm: app::vm::LoginVM_Design; }
  ```
* 热重载下，结构变化触发**子树重建**，纯属性变化触发**值更新**。
