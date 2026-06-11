# CheckBox 类 API 文档

## 1. 概述

`CheckBox` 是 MineFramework 中的勾选框控件,实现了 Material Design 3 (MD3) Checkbox 规范。它采用组合式视觉树架构,通过双 VSM(Visual State Manager)状态机分离管理交互状态(Normal/Hovered/Pressed/Disabled)和勾选状态(Unchecked/Checked),提供流畅的状态切换动画和完整的 MVVM 数据绑定支持。

**核心职责:**

- **双态切换**:通过 `IsCheckedProperty` 依赖属性管理勾选状态,点击自动切换并触发 `CheckedChangedEvent` 路由事件
- **组合式视觉树**:StackPanel → Border(icon_border) → Border(state_border) → CheckMarkElement + TextBlock(label),各基元独立控制样式
- **双 VSM 状态机**:交互组(Control::vsm)管理鼠标交互,勾选组(owned_checked_vsm_)管理勾选状态,互相停止对方动画防止冲突
- **MD3 规范实现**:默认图标尺寸 18x18dp,State Layer 尺寸 40x40dp,支持 Hover/Press 状态层动画和勾选标记淡入/淡出
- **MVVM 友好**:IsCheckedProperty 支持双向数据绑定,自动同步 ViewModel 布尔属性

**继承关系:**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── CheckBox  ← 本文档
```

**架构意义:**

1. **组合优于继承**:CheckBox 不继承 ContentControl,而是通过内部 StackPanel 组合 Icon 和 Label,避免 ContentControl 的单内容限制
2. **VSM 分离关注点**:交互状态和勾选状态由两个独立 VSM 管理,状态切换时互相停止对方动画,避免视觉冲突
3. **基元化设计**:图标背景、边框、勾选标记、文本标签都是独立的基元,样式可单独定制
4. **无障碍支持**:State Layer 扩展到 40x40dp 确保足够的点击区域,键盘 Space 键触发切换

**典型用途:**

- 表单多选项、同意条款勾选
- 设置面板开关选项
- 列表项批量选择
- MVVM 架构中双向绑定布尔属性

**设计原则:**

- **视觉分层**:Icon(18x18) → State Layer(40x40) → Check Mark,各层独立动画
- **状态隔离**:交互状态和勾选状态由不同的 VSM 管理,避免相互干扰
- **动画连贯性**:状态切换时先停止旧动画,再启动新动画,避免视觉跳变

**性能特性:**

- **增量渲染**:仅在状态变化时标记为 InvalidateVisual
- **GPU 加速**:CheckMark 使用 Path 几何体,由 GPU 光栅化
- **布局缓存**:文本测量结果缓存在 TextBlock 中

**相关类型:**

- `CheckedChangedEvent`:勾选状态变化路由事件
- `ControlVisualState`:控件视觉状态枚举
- `VisualStateManager`:状态机管理器
- `CheckMarkElement`:勾选标记基元(内部类)

**MD3 规范要点:**

- **尺寸**:图标 18x18dp,State Layer 40x40dp,文本间距 8dp
- **颜色**:未勾选边框 On Surface Variant,勾选背景 Primary,勾选标记 On Primary
- **动画**:Hover 淡入 150ms,勾选标记缩放+淡入 200ms
- **无障碍**:最小点击区域 40x40dp,色彩对比度 > 4.5:1

**实现细节:**

- **双 VSM 冲突处理**:在 `on_visual_state_changed()` 中,新状态启动前先调用 `owned_checked_vsm_->stop_all_animations()`
- **CheckMark 动画**:使用 ScaleTransform + OpacityAnimation 实现缩放淡入效果
- **IsChecked 同步**:点击时先切换 IsCheckedProperty,然后触发 CheckedChangedEvent,最后启动勾选组动画

---

## 2. 文件位置

**头文件:**
```
src/mine/ui/controls/CheckBox.h
```

**实现文件:**
```
src/mine/ui/controls/CheckBox.cpp
```

**模块归属:**
```
mine.ui.controls
```

**命名空间:**
```cpp
namespace mine::ui
```

**依赖头文件:**
```cpp
#include <mine/ui/Control.h>
#include <mine/ui/DependencyProperty.h>
#include <mine/ui/RoutedEvent.h>
#include <mine/ui/animation/VisualStateManager.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/visual/Border.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Canvas.h>
#include <mine/core/StringView.h>
```

**链接依赖:**
```
mine.ui.controls.lib
mine.ui.animation.lib
mine.ui.layout.lib
mine.ui.visual.lib
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// @brief MD3 Checkbox 勾选框控件,支持双态切换和 MVVM 绑定
///
/// 采用组合式视觉树架构(StackPanel + Border + CheckMark + TextBlock),
/// 通过双 VSM 状态机分离管理交互状态和勾选状态。
///
/// @example 基础用法
/// @code
/// auto checkbox = std::make_unique<CheckBox>();
/// checkbox->set_text("同意用户协议");
/// checkbox->set_checked(true);
/// checkbox->add_checked_changed_handler([](auto& sender, auto& args) {
///     bool checked = static_cast<CheckBox&>(sender).is_checked();
///     std::cout << "勾选状态: " << (checked ? "是" : "否") << std::endl;
/// });
/// @endcode
///
/// @example MVVM 双向绑定
/// @code
/// // MML 中绑定 ViewModel 的布尔属性
/// <CheckBox text="记住密码" isChecked="{bind:vm.RememberPassword, mode:TwoWay}" />
/// @endcode
class MINE_UI_CONTROLS_API CheckBox : public Control {
public:
    // ========== 路由事件 ==========
    
    /// @brief 勾选状态变化事件,Bubble 策略向上冒泡
    /// @details 在 IsCheckedProperty 值变化后触发,
    ///          事件参数包含 old_value 和 new_value。
    /// @return RoutedEvent 常量引用
    [[nodiscard]] static const RoutedEvent& CheckedChangedEvent();
    
    // ========== 依赖属性 ==========
    
    /// @brief 勾选状态属性,true 为勾选,false 为未勾选
    [[nodiscard]] static const DependencyProperty& IsCheckedProperty;
    
    /// @brief 图标背景画刷属性,勾选时显示 Primary 色
    [[nodiscard]] static const DependencyProperty& IconBackgroundProperty;
    
    /// @brief 图标边框画刷属性,未勾选时显示 On Surface Variant 色
    [[nodiscard]] static const DependencyProperty& IconBorderBrushProperty;
    
    /// @brief 状态层画刷属性,Hover/Press 时叠加的蒙版颜色
    [[nodiscard]] static const DependencyProperty& StateLayerBrushProperty;
    
    /// @brief 勾选标记画刷属性,勾选时显示 On Primary 色
    [[nodiscard]] static const DependencyProperty& CheckMarkBrushProperty;
    
    /// @brief 文本前景画刷属性,默认 On Surface 色
    [[nodiscard]] static const DependencyProperty& TextForegroundProperty;
    
    /// @brief 启用状态属性,false 时禁用交互
    [[nodiscard]] static const DependencyProperty& IsEnabledProperty;
    
    // ========== 构造/析构 ==========
    
    /// @brief 默认构造函数,初始化组合式视觉树和双 VSM
    CheckBox();
    
    /// @brief 析构函数,清理 VSM 和事件订阅
    ~CheckBox() override;
    
    // 禁止拷贝和移动
    CheckBox(const CheckBox&) = delete;
    CheckBox& operator=(const CheckBox&) = delete;
    CheckBox(CheckBox&&) = delete;
    CheckBox& operator=(CheckBox&&) = delete;
    
    // ========== 属性访问器 ==========
    
    /// @brief 获取勾选状态
    /// @return true 勾选,false 未勾选
    [[nodiscard]] bool is_checked() const noexcept;
    
    /// @brief 设置勾选状态
    /// @param checked 新状态
    void set_checked(bool checked);
    
    /// @brief 切换勾选状态(取反)
    void toggle();
    
    /// @brief 获取文本标签内容
    [[nodiscard]] core::StringView text() const noexcept;
    
    /// @brief 设置文本标签内容
    /// @param text 文本字符串
    void set_text(core::StringView text);
    
    /// @brief 设置文本字体
    /// @param font_face HarfBuzz hb_font_t 指针
    void set_font_face(void* font_face) noexcept;
    
    /// @brief 设置文本字号
    /// @param size_px 字体大小,单位像素
    void set_font_size(float size_px) noexcept;
    
    /// @brief 获取启用状态
    [[nodiscard]] bool is_enabled() const noexcept;
    
    /// @brief 设置启用状态
    /// @param enabled true 启用,false 禁用
    void set_enabled(bool enabled) noexcept;
    
    /// @brief 添加勾选状态变化事件处理器
    /// @param handler 事件处理函数
    void add_checked_changed_handler(
        std::function<void(UIElement&, PropertyChangedEventArgs&)> handler);
    
protected:
    // ========== 布局虚函数 ==========
    
    /// @brief 测量阶段,计算所需尺寸
    /// @param available_size 可用空间
    void on_measure(math::Size available_size) override;
    
    // ========== 视觉状态虚函数 ==========
    
    /// @brief 计算当前视觉状态(交互组)
    /// @return ControlVisualState 枚举值
    [[nodiscard]] ControlVisualState compute_visual_state() const noexcept override;
    
    /// @brief 视觉状态变化回调,启动交互组动画并停止勾选组动画
    /// @param old_state 旧状态
    /// @param new_state 新状态
    void on_visual_state_changed(ControlVisualState old_state, 
                                   ControlVisualState new_state) override;
    
    // ========== 输入事件虚函数 ==========
    
    /// @brief 鼠标释放事件,触发勾选状态切换
    void on_mouse_up(input::MouseButtonEventArgs& args) override;
    
    /// @brief 键盘按下事件,处理 Space 键切换
    void on_key_down(input::KeyEventArgs& args) override;
    
private:
    // ========== 内部实现 ==========
    
    /// @brief 初始化组合式视觉树
    void init_visual_tree();
    
    /// @brief 初始化交互组 VSM
    void init_interaction_vsm();
    
    /// @brief 初始化勾选组 VSM
    void init_checked_vsm();
    
    /// @brief 执行勾选状态切换逻辑
    void execute_toggle();
    
    /// @brief IsCheckedProperty 变化回调
    void on_is_checked_changed(const core::Variant& old_value, 
                                const core::Variant& new_value);
    
    // 私有成员
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mine::ui
```

### 3.2 依赖类型

```cpp
/// @brief 属性变化事件参数
struct PropertyChangedEventArgs : public RoutedEventArgs {
    core::Variant old_value;  ///< 旧值
    core::Variant new_value;  ///< 新值
};

/// @brief 勾选标记基元(内部使用)
class CheckMarkElement : public UIElement {
public:
    /// @brief 设置勾选标记颜色
    void set_brush(paint::Brush brush);
    
protected:
    void on_render(paint::Canvas& canvas) override;
    
private:
    paint::Brush brush_;
    // 勾选标记路径数据: "M 4 8 L 7 11 L 14 4"
};
```

### 3.3 内部实现细节

CheckBox 内部使用 PImpl 模式隐藏实现细节:

```cpp
struct CheckBox::Impl {
    // 视觉树组件
    std::unique_ptr<StackPanel> root_panel;        ///< 根容器(Horizontal)
    Border* icon_border;                           ///< 图标外边框
    Border* state_border;                          ///< 状态层边框
    CheckMarkElement* check_mark;                  ///< 勾选标记
    TextBlock* label;                              ///< 文本标签
    
    // 双 VSM 状态机
    std::unique_ptr<VisualStateManager> owned_checked_vsm_;  ///< 勾选组 VSM
    
    // 状态缓存
    bool is_checked_;                              ///< 当前勾选状态
};
```

**视觉树结构:**

```
CheckBox (Control)
└── StackPanel (Horizontal, Spacing=8)
    ├── Border (icon_border, 40x40, 居中对齐)
    │   └── Border (state_border, 18x18, 圆角 2dp)
    │       └── CheckMarkElement (勾选标记)
    └── TextBlock (label, VerticalAlignment=Center)
```

**双 VSM 结构:**

1. **交互组 VSM (Control::vsm_)** - 管理 Normal/Hovered/Pressed/Disabled
   - Normal: StateLayerBrush Opacity = 0
   - Hovered: StateLayerBrush Opacity = 0.08
   - Pressed: StateLayerBrush Opacity = 0.12
   - Disabled: 整体 Opacity = 0.38

2. **勾选组 VSM (owned_checked_vsm_)** - 管理 Unchecked/Checked
   - Unchecked:
     - IconBackgroundProperty = Transparent
     - IconBorderBrushProperty = On Surface Variant
     - CheckMarkBrushProperty Opacity = 0 (隐藏)
   - Checked:
     - IconBackgroundProperty = Primary
     - IconBorderBrushProperty = Primary
     - CheckMarkBrushProperty Opacity = 1 (显示,带缩放动画)

---

## 4. 成员方法

### 4.1 静态方法

#### 4.1.1 CheckedChangedEvent()

```cpp
[[nodiscard]] static const RoutedEvent& CheckedChangedEvent();
```

**功能:** 返回勾选状态变化路由事件的全局单例。

**返回值:** `RoutedEvent` 常量引用,事件名称为 "CheckBox.CheckedChanged",路由策略为 Bubble。

**用法:**
```cpp
checkbox->add_event_handler(CheckBox::CheckedChangedEvent(), 
    [](UIElement& sender, RoutedEventArgs& args) {
        auto& evt_args = static_cast<PropertyChangedEventArgs&>(args);
        bool old_checked = evt_args.old_value.as<bool>();
        bool new_checked = evt_args.new_value.as<bool>();
        std::cout << "勾选状态从 " << old_checked << " 变为 " << new_checked << std::endl;
    });
```

**触发时机:**
- 用户点击 CheckBox 切换状态
- 调用 `set_checked()` 或 `toggle()` 方法
- 数据绑定导致 IsCheckedProperty 值变化

**事件参数:** `PropertyChangedEventArgs`,包含 `old_value` 和 `new_value`。

---

#### 4.1.2 依赖属性访问器

```cpp
[[nodiscard]] static const DependencyProperty& IsCheckedProperty;
[[nodiscard]] static const DependencyProperty& IconBackgroundProperty;
[[nodiscard]] static const DependencyProperty& IconBorderBrushProperty;
[[nodiscard]] static const DependencyProperty& StateLayerBrushProperty;
[[nodiscard]] static const DependencyProperty& CheckMarkBrushProperty;
[[nodiscard]] static const DependencyProperty& TextForegroundProperty;
[[nodiscard]] static const DependencyProperty& IsEnabledProperty;
```

**功能:** 返回对应依赖属性的全局注册对象。

**属性元数据:**

| 属性 | 类型 | 默认值 | 影响 | VSM 控制 |
|-----|------|--------|------|---------|
| IsCheckedProperty | bool | false | 触发 CheckedChangedEvent | 勾选组 |
| IconBackgroundProperty | Brush | Transparent | InvalidateVisual | 勾选组 |
| IconBorderBrushProperty | Brush | On Surface Variant | InvalidateVisual | 交互组 + 勾选组 |
| StateLayerBrushProperty | Brush | 黑色 8% | InvalidateVisual | 交互组 |
| CheckMarkBrushProperty | Brush | On Primary | InvalidateVisual | 勾选组 |
| TextForegroundProperty | Brush | On Surface | InvalidateVisual | 无 |
| IsEnabledProperty | bool | true | 影响视觉状态 | 交互组 |

**用法:**
```cpp
// 数据绑定
checkbox->set_binding(CheckBox::IsCheckedProperty, 
                      Binding(view_model, "RememberPassword"));

// 样式设置
checkbox->set_value(CheckBox::CheckMarkBrushProperty, 
                    Variant::from(SolidColorBrush(0xFF00FF)));
```

---

### 4.2 构造与析构

#### 4.2.1 CheckBox()

```cpp
CheckBox();
```

**功能:** 构造函数,初始化组合式视觉树和双 VSM。

**初始化内容:**
1. 创建 StackPanel 根容器(Horizontal,Spacing=8)
2. 创建 Border(icon_border,40x40) → Border(state_border,18x18) → CheckMarkElement
3. 创建 TextBlock(label)
4. 初始化交互组 VSM(继承自 Control)
5. 初始化勾选组 VSM(owned_checked_vsm_)
6. 注册 IsCheckedProperty 变化回调
7. 注册 PointerReleased/KeyDown 事件处理器

**伪代码:**
```cpp
CheckBox::CheckBox() {
    // 1. 创建视觉树
    init_visual_tree();
    
    // 2. 初始化 VSM
    init_interaction_vsm();
    init_checked_vsm();
    
    // 3. 注册属性变化回调
    IsCheckedProperty.add_changed_callback(this, 
        &CheckBox::on_is_checked_changed);
}
```

---

#### 4.2.2 ~CheckBox()

```cpp
~CheckBox() override;
```

**功能:** 析构函数,清理资源。

**清理内容:**
- 停止并释放勾选组 VSM
- 释放内部视觉树组件
- 取消属性变化回调订阅

---

### 4.3 属性访问器

#### 4.3.1 is_checked() / set_checked()

```cpp
[[nodiscard]] bool is_checked() const noexcept;
void set_checked(bool checked);
```

**功能:** 获取/设置勾选状态。

**参数:**
- `checked`: true 勾选,false 未勾选

**返回值:** 当前勾选状态。

**实现细节:**
- `set_checked()` 内部调用 `set_value(IsCheckedProperty, Variant::from_bool(checked))`
- `is_checked()` 直接读取 `impl_->is_checked_` 缓存,避免重复查询依赖属性系统

**用法:**
```cpp
checkbox->set_checked(true);
if (checkbox->is_checked()) {
    // 处理勾选逻辑
}
```

**触发行为:**
- 调用 `set_checked()` 会触发 `CheckedChangedEvent`
- 启动勾选组 VSM 动画(勾选标记淡入/淡出)

---

#### 4.3.2 toggle()

```cpp
void toggle();
```

**功能:** 切换勾选状态(取反)。

**实现:**
```cpp
void CheckBox::toggle() {
    set_checked(!is_checked());
}
```

**用法:**
```cpp
// 点击事件中切换状态
checkbox->add_click_handler([](auto& sender, auto&) {
    static_cast<CheckBox&>(sender).toggle();
});
```

---

#### 4.3.3 text() / set_text()

```cpp
[[nodiscard]] core::StringView text() const noexcept;
void set_text(core::StringView text);
```

**功能:** 获取/设置文本标签内容。

**参数:**
- `text`: 文本字符串,支持 UTF-8 编码

**返回值:** 文本字符串视图。

**实现细节:**
- `set_text()` 调用内部 TextBlock 的 `set_text()` 方法
- `text()` 从内部 TextBlock 读取文本

**用法:**
```cpp
checkbox->set_text("同意用户协议");
auto current_text = checkbox->text();  // "同意用户协议"
```

---

#### 4.3.4 is_enabled() / set_enabled()

```cpp
[[nodiscard]] bool is_enabled() const noexcept;
void set_enabled(bool enabled) noexcept;
```

**功能:** 获取/设置启用状态。

**参数:**
- `enabled`: true 启用,false 禁用

**行为:**
- 禁用时,控件切换到 Disabled 视觉状态(整体 Opacity 0.38)
- 禁用时,不响应鼠标和键盘事件

**用法:**
```cpp
// 根据条件禁用 CheckBox
if (!form_is_valid()) {
    checkbox->set_enabled(false);
}
```

---

#### 4.3.5 add_checked_changed_handler()

```cpp
void add_checked_changed_handler(
    std::function<void(UIElement&, PropertyChangedEventArgs&)> handler);
```

**功能:** 添加勾选状态变化事件处理器的便捷方法。

**参数:**
- `handler`: Lambda 或函数对象,签名为 `void(UIElement&, PropertyChangedEventArgs&)`

**用法:**
```cpp
checkbox->add_checked_changed_handler([](UIElement& sender, 
                                         PropertyChangedEventArgs& args) {
    auto& cb = static_cast<CheckBox&>(sender);
    bool checked = args.new_value.as<bool>();
    std::cout << "勾选状态变为: " << (checked ? "是" : "否") << std::endl;
});
```

**等价写法:**
```cpp
checkbox->add_event_handler(CheckBox::CheckedChangedEvent(), handler);
```

---

### 4.4 布局虚函数

#### 4.4.1 on_measure()

```cpp
void on_measure(math::Size available_size) override;
```

**功能:** 测量 CheckBox 所需尺寸。

**参数:**
- `available_size`: 父容器提供的可用空间

**返回值:** 通过 `set_desired_size()` 设置所需尺寸。

**测量逻辑:**
1. 测量内部 StackPanel(Icon + Text)所需尺寸
2. 应用最小尺寸约束(最小高度 40dp,确保 State Layer 完整)
3. 返回最终尺寸

**示例:**
```cpp
// 假设文本测量为 80x16,Icon 为 40x40
// StackPanel 总宽度 = 40 + 8(spacing) + 80 = 128
// 高度取最大值 = max(40, 16) = 40
// 最终 DesiredSize = {128, 40}
```

---

### 4.5 视觉状态虚函数

#### 4.5.1 compute_visual_state()

```cpp
[[nodiscard]] ControlVisualState compute_visual_state() const noexcept override;
```

**功能:** 计算当前视觉状态(交互组)。

**返回值:** `ControlVisualState` 枚举值。

**判断逻辑:**
```cpp
ControlVisualState CheckBox::compute_visual_state() const noexcept {
    if (!is_enabled()) return ControlVisualState::Disabled;
    if (is_pressed()) return ControlVisualState::Pressed;
    if (is_hovered()) return ControlVisualState::Hovered;
    return ControlVisualState::Normal;
}
```

**注意:** 此方法仅管理交互状态,勾选状态由勾选组 VSM 独立管理。

---

#### 4.5.2 on_visual_state_changed()

```cpp
void on_visual_state_changed(ControlVisualState old_state, 
                               ControlVisualState new_state) override;
```

**功能:** 视觉状态变化时的回调,启动交互组动画并停止勾选组动画。

**参数:**
- `old_state`: 旧状态
- `new_state`: 新状态

**动画启动逻辑:**
```cpp
void CheckBox::on_visual_state_changed(ControlVisualState old_state, 
                                        ControlVisualState new_state) {
    // 1. 停止勾选组动画,防止与交互组动画冲突
    if (impl_->owned_checked_vsm_) {
        impl_->owned_checked_vsm_->stop_all_animations();
    }
    
    // 2. 启动交互组 VSM 动画(State Layer 淡入/淡出)
    vsm_->go_to_state(state_name(new_state));
}
```

**VSM 状态名称映射:**
- Normal → "Normal"
- Hovered → "PointerOver"
- Pressed → "Pressed"
- Disabled → "Disabled"

---

### 4.6 输入事件虚函数

#### 4.6.1 on_mouse_up()

```cpp
void on_mouse_up(input::MouseButtonEventArgs& args) override;
```

**功能:** 鼠标释放事件,触发勾选状态切换。

**实现:**
```cpp
void CheckBox::on_mouse_up(MouseButtonEventArgs& args) {
    if (args.button == MouseButton::Left && bounds().contains(args.position)) {
        execute_toggle();
        args.handled = true;
    }
    Control::on_mouse_up(args);
}
```

**注意:** CheckBox 不捕获鼠标,释放位置必须在控件边界内才触发切换。

---

#### 4.6.2 on_key_down()

```cpp
void on_key_down(input::KeyEventArgs& args) override;
```

**功能:** 键盘按下事件,处理 Space 键切换。

**实现:**
```cpp
void CheckBox::on_key_down(KeyEventArgs& args) {
    if (args.key == Key::Space) {
        execute_toggle();
        args.handled = true;
    }
    Control::on_key_down(args);
}
```

---

### 4.7 私有方法

#### 4.7.1 init_visual_tree()

```cpp
void init_visual_tree();
```

**功能:** 初始化组合式视觉树。

**伪代码:**
```cpp
void CheckBox::init_visual_tree() {
    // 1. 创建根容器
    impl_->root_panel = std::make_unique<StackPanel>();
    impl_->root_panel->set_orientation(Orientation::Horizontal);
    impl_->root_panel->set_spacing(8);
    
    // 2. 创建图标外边框(40x40,居中对齐)
    auto icon_border = std::make_unique<Border>();
    icon_border->set_width(40);
    icon_border->set_height(40);
    icon_border->set_horizontal_alignment(HorizontalAlignment::Center);
    icon_border->set_vertical_alignment(VerticalAlignment::Center);
    
    // 3. 创建状态层边框(18x18,圆角 2dp)
    auto state_border = std::make_unique<Border>();
    state_border->set_width(18);
    state_border->set_height(18);
    state_border->set_corner_radius(CornerRadii::uniform(2.0f));
    state_border->set_border_thickness(Thickness::uniform(2.0f));
    
    // 4. 创建勾选标记
    auto check_mark = std::make_unique<CheckMarkElement>();
    check_mark->set_brush(SolidColorBrush(0xFFFFFF));  // On Primary
    state_border->set_child(std::move(check_mark));
    
    icon_border->set_child(std::move(state_border));
    impl_->root_panel->add_child(std::move(icon_border));
    
    // 5. 创建文本标签
    auto label = std::make_unique<TextBlock>();
    label->set_vertical_alignment(VerticalAlignment::Center);
    impl_->root_panel->add_child(std::move(label));
    
    // 6. 设置为控件的视觉子元素
    set_visual_child(impl_->root_panel.get());
}
```

---

#### 4.7.2 init_interaction_vsm()

```cpp
void init_interaction_vsm();
```

**功能:** 初始化交互组 VSM(继承自 Control::vsm_)。

**状态定义:**
```cpp
void CheckBox::init_interaction_vsm() {
    auto vsm = visual_state_manager();
    
    // Normal 状态
    auto normal_state = std::make_unique<VisualState>("Normal");
    normal_state->add_animation(DoubleAnimation(
        "StateLayerBrush.Opacity", 0.0, 150ms
    ));
    vsm->add_state(std::move(normal_state));
    
    // Hovered 状态
    auto hovered_state = std::make_unique<VisualState>("PointerOver");
    hovered_state->add_animation(DoubleAnimation(
        "StateLayerBrush.Opacity", 0.08, 150ms
    ));
    vsm->add_state(std::move(hovered_state));
    
    // Pressed 状态
    auto pressed_state = std::make_unique<VisualState>("Pressed");
    pressed_state->add_animation(DoubleAnimation(
        "StateLayerBrush.Opacity", 0.12, 100ms
    ));
    vsm->add_state(std::move(pressed_state));
    
    // Disabled 状态
    auto disabled_state = std::make_unique<VisualState>("Disabled");
    disabled_state->add_animation(DoubleAnimation(
        "Opacity", 0.38, 0ms
    ));
    vsm->add_state(std::move(disabled_state));
}
```

---

#### 4.7.3 init_checked_vsm()

```cpp
void init_checked_vsm();
```

**功能:** 初始化勾选组 VSM(独立的 owned_checked_vsm_)。

**状态定义:**
```cpp
void CheckBox::init_checked_vsm() {
    impl_->owned_checked_vsm_ = std::make_unique<VisualStateManager>(this);
    
    // Unchecked 状态
    auto unchecked_state = std::make_unique<VisualState>("Unchecked");
    unchecked_state->add_animation(ColorAnimation(
        "IconBackgroundProperty", Colors::Transparent, 200ms
    ));
    unchecked_state->add_animation(ColorAnimation(
        "IconBorderBrushProperty", Colors::OnSurfaceVariant, 200ms
    ));
    unchecked_state->add_animation(DoubleAnimation(
        "CheckMarkBrush.Opacity", 0.0, 150ms
    ));
    impl_->owned_checked_vsm_->add_state(std::move(unchecked_state));
    
    // Checked 状态
    auto checked_state = std::make_unique<VisualState>("Checked");
    checked_state->add_animation(ColorAnimation(
        "IconBackgroundProperty", Colors::Primary, 200ms
    ));
    checked_state->add_animation(ColorAnimation(
        "IconBorderBrushProperty", Colors::Primary, 200ms
    ));
    // 勾选标记缩放+淡入动画
    checked_state->add_animation(DoubleAnimation(
        "CheckMark.ScaleX", 0.8, 1.0, 200ms, EasingFunction::EaseOut
    ));
    checked_state->add_animation(DoubleAnimation(
        "CheckMark.ScaleY", 0.8, 1.0, 200ms, EasingFunction::EaseOut
    ));
    checked_state->add_animation(DoubleAnimation(
        "CheckMarkBrush.Opacity", 1.0, 200ms
    ));
    impl_->owned_checked_vsm_->add_state(std::move(checked_state));
}
```

---

#### 4.7.4 execute_toggle()

```cpp
void execute_toggle();
```

**功能:** 执行勾选状态切换逻辑。

**执行顺序:**
1. 切换 IsCheckedProperty 值
2. 触发 CheckedChangedEvent 路由事件
3. 停止交互组动画,启动勾选组动画

**伪代码:**
```cpp
void CheckBox::execute_toggle() {
    bool old_checked = is_checked();
    bool new_checked = !old_checked;
    
    // 1. 更新属性
    set_value(IsCheckedProperty, Variant::from_bool(new_checked));
    
    // 2. 触发事件
    auto args = PropertyChangedEventArgs(this);
    args.old_value = Variant::from_bool(old_checked);
    args.new_value = Variant::from_bool(new_checked);
    raise_event(CheckedChangedEvent(), args);
    
    // 3. 启动勾选组动画
    if (!args.handled) {
        // 停止交互组动画
        vsm_->stop_all_animations();
        
        // 启动勾选组动画
        impl_->owned_checked_vsm_->go_to_state(new_checked ? "Checked" : "Unchecked");
    }
}
```

---

#### 4.7.5 on_is_checked_changed()

```cpp
void on_is_checked_changed(const core::Variant& old_value, 
                            const core::Variant& new_value);
```

**功能:** IsCheckedProperty 变化回调(由依赖属性系统自动调用)。

**实现:**
```cpp
void CheckBox::on_is_checked_changed(const Variant& old_value, 
                                      const Variant& new_value) {
    // 更新内部缓存
    impl_->is_checked_ = new_value.as<bool>();
    
    // 启动勾选组动画
    impl_->owned_checked_vsm_->go_to_state(
        impl_->is_checked_ ? "Checked" : "Unchecked"
    );
}
```

---

## 5. 使用场景

### 5.1 基础勾选框

**场景:** 表单多选项、同意条款。

**代码:**
```cpp
auto agree_checkbox = std::make_unique<CheckBox>();
agree_checkbox->set_text("我已阅读并同意用户协议");
agree_checkbox->add_checked_changed_handler([](auto& sender, auto& args) {
    bool agreed = args.new_value.as<bool>();
    submit_button->set_enabled(agreed);
});
```

**MML 写法:**
```xml
<CheckBox text="我已阅读并同意用户协议" checkedChanged="on_agree_changed" />
```

---

### 5.2 MVVM 双向绑定

**场景:** MVVM 架构,自动同步 ViewModel 属性。

**ViewModel:**
```cpp
class SettingsViewModel : public ViewModelBase {
public:
    bool remember_password() const { return remember_password_; }
    void set_remember_password(bool value) {
        if (remember_password_ != value) {
            remember_password_ = value;
            notify_property_changed("RememberPassword");
            save_settings();
        }
    }
    
private:
    bool remember_password_ = false;
};
```

**View:**
```cpp
auto view_model = std::make_shared<SettingsViewModel>();

auto remember_cb = std::make_unique<CheckBox>();
remember_cb->set_text("记住密码");
remember_cb->set_binding(CheckBox::IsCheckedProperty, 
                         Binding(view_model.get(), "RememberPassword", BindingMode::TwoWay));
```

**MML 写法:**
```xml
<CheckBox text="记住密码" 
          isChecked="{bind:vm.RememberPassword, mode:TwoWay}" />
```

**优势:** 用户点击 CheckBox 自动更新 ViewModel,ViewModel 变化自动更新 CheckBox。

---

### 5.3 列表项批量选择

**场景:** 文件管理器、邮件列表。

**代码:**
```cpp
// 创建可选择的列表项
class SelectableListItem {
public:
    SelectableListItem(std::string name) : name_(std::move(name)) {}
    
    bool is_selected() const { return is_selected_; }
    void set_selected(bool selected) { is_selected_ = selected; }
    const std::string& name() const { return name_; }
    
private:
    std::string name_;
    bool is_selected_ = false;
};

// 创建列表
std::vector<SelectableListItem> items = {
    {"文件1.txt"}, {"文件2.txt"}, {"文件3.txt"}
};

auto list_panel = std::make_unique<StackPanel>();
for (auto& item : items) {
    auto row = std::make_unique<StackPanel>();
    row->set_orientation(Orientation::Horizontal);
    row->set_spacing(8);
    
    auto cb = std::make_unique<CheckBox>();
    cb->set_checked(item.is_selected());
    cb->add_checked_changed_handler([&item](auto&, auto& args) {
        item.set_selected(args.new_value.as<bool>());
    });
    row->add_child(std::move(cb));
    
    auto label = std::make_unique<TextBlock>();
    label->set_text(item.name());
    row->add_child(std::move(label));
    
    list_panel->add_child(std::move(row));
}

// 全选按钮
auto select_all_cb = std::make_unique<CheckBox>();
select_all_cb->set_text("全选");
select_all_cb->add_checked_changed_handler([&items](auto&, auto& args) {
    bool select_all = args.new_value.as<bool>();
    for (auto& item : items) {
        item.set_selected(select_all);
    }
    // 刷新列表 CheckBox 状态...
});
```

**MML 写法(使用数据绑定):**
```xml
<StackPanel>
    <CheckBox text="全选" isChecked="{bind:vm.IsAllSelected, mode:TwoWay}" />
    
    <ItemsControl itemsSource="{bind:vm.Items}">
        <ItemsControl.ItemTemplate>
            <DataTemplate>
                <StackPanel orientation="Horizontal" spacing="8">
                    <CheckBox isChecked="{bind:item.IsSelected, mode:TwoWay}" />
                    <TextBlock text="{bind:item.Name}" />
                </StackPanel>
            </DataTemplate>
        </ItemsControl.ItemTemplate>
    </ItemsControl>
</StackPanel>
```

---

### 5.4 表单验证

**场景:** 表单提交前检查必填项。

**代码:**
```cpp
class FormValidator {
public:
    void add_required_checkbox(CheckBox* cb) {
        required_checkboxes_.push_back(cb);
        cb->add_checked_changed_handler([this](auto&, auto&) {
            validate();
        });
    }
    
    bool is_valid() const { return is_valid_; }
    
    void validate() {
        bool all_checked = std::all_of(
            required_checkboxes_.begin(), 
            required_checkboxes_.end(),
            [](CheckBox* cb) { return cb->is_checked(); }
        );
        
        is_valid_ = all_checked;
        validation_changed.raise(is_valid_);
    }
    
    event::Event<bool> validation_changed;
    
private:
    std::vector<CheckBox*> required_checkboxes_;
    bool is_valid_ = false;
};

// 使用
auto agree_terms_cb = std::make_unique<CheckBox>();
agree_terms_cb->set_text("同意服务条款");

auto agree_privacy_cb = std::make_unique<CheckBox>();
agree_privacy_cb->set_text("同意隐私政策");

FormValidator validator;
validator.add_required_checkbox(agree_terms_cb.get());
validator.add_required_checkbox(agree_privacy_cb.get());

validator.validation_changed.subscribe([submit_btn](bool valid) {
    submit_btn->set_enabled(valid);
});
```

---

### 5.5 三态 CheckBox(扩展)

**场景:** 父子层级选择(部分选中状态)。

**代码:**
```cpp
// 扩展 CheckBox 支持三态
class TriStateCheckBox : public CheckBox {
public:
    enum class CheckState {
        Unchecked,      // 未勾选
        Checked,        // 全部勾选
        Indeterminate   // 部分勾选
    };
    
    CheckState check_state() const { return check_state_; }
    
    void set_check_state(CheckState state) {
        if (check_state_ != state) {
            check_state_ = state;
            
            switch (state) {
            case CheckState::Unchecked:
                set_checked(false);
                set_indeterminate_visual(false);
                break;
            case CheckState::Checked:
                set_checked(true);
                set_indeterminate_visual(false);
                break;
            case CheckState::Indeterminate:
                set_checked(true);
                set_indeterminate_visual(true);  // 显示横线标记
                break;
            }
        }
    }
    
protected:
    void on_mouse_up(MouseButtonEventArgs& args) override {
        // 循环切换三态
        CheckState next_state;
        switch (check_state_) {
        case CheckState::Unchecked:
            next_state = CheckState::Checked;
            break;
        case CheckState::Checked:
            next_state = CheckState::Indeterminate;
            break;
        case CheckState::Indeterminate:
            next_state = CheckState::Unchecked;
            break;
        }
        set_check_state(next_state);
        args.handled = true;
    }
    
private:
    void set_indeterminate_visual(bool indeterminate) {
        // 修改 CheckMark 为横线形状...
    }
    
    CheckState check_state_ = CheckState::Unchecked;
};

// 使用
auto parent_cb = std::make_unique<TriStateCheckBox>();
parent_cb->set_text("全选");

std::vector<CheckBox*> child_checkboxes;
for (int i = 0; i < 3; ++i) {
    auto child_cb = std::make_unique<CheckBox>();
    child_cb->set_text(std::format("选项 {}", i + 1));
    
    child_cb->add_checked_changed_handler([parent_cb = parent_cb.get(), 
                                            &child_checkboxes](auto&, auto&) {
        // 更新父 CheckBox 状态
        int checked_count = std::count_if(
            child_checkboxes.begin(), child_checkboxes.end(),
            [](CheckBox* cb) { return cb->is_checked(); }
        );
        
        if (checked_count == 0) {
            parent_cb->set_check_state(TriStateCheckBox::CheckState::Unchecked);
        } else if (checked_count == child_checkboxes.size()) {
            parent_cb->set_check_state(TriStateCheckBox::CheckState::Checked);
        } else {
            parent_cb->set_check_state(TriStateCheckBox::CheckState::Indeterminate);
        }
    });
    
    child_checkboxes.push_back(child_cb.get());
}
```

---

### 5.6 自定义样式 CheckBox

**场景:** 品牌定制、特殊设计。

**代码:**
```cpp
// 创建开关样式 CheckBox
auto switch_cb = std::make_unique<CheckBox>();
switch_cb->set_text("启用通知");

// 自定义颜色
switch_cb->set_value(CheckBox::IconBackgroundProperty, 
                     Variant::from(SolidColorBrush(0xFF2196F3)));  // 蓝色背景
switch_cb->set_value(CheckBox::CheckMarkBrushProperty, 
                     Variant::from(SolidColorBrush(0xFFFFEB3B)));  // 黄色勾选标记

// 自定义尺寸(通过 ControlTemplate 实现开关样式)
auto custom_template = std::make_unique<ControlTemplate>();
// ... 定义开关样式的视觉树 ...
switch_cb->set_template(std::move(custom_template));
```

**MML 写法:**
```xml
<CheckBox text="启用通知" 
          iconBackground="#2196F3"
          checkMarkBrush="#FFEB3B">
    <CheckBox.Template>
        <ControlTemplate>
            <!-- 开关样式视觉树 -->
            <Border width="40" height="24" cornerRadius="12" background="#E0E0E0">
                <Ellipse width="20" height="20" fill="#FFFFFF" 
                         horizontalAlignment="{bind:IsChecked, converter:SwitchThumbAlignment}" />
            </Border>
        </ControlTemplate>
    </CheckBox.Template>
</CheckBox>
```

---

### 5.7 动画自定义

**场景:** 调整勾选动画速度、缓动函数。

**代码:**
```cpp
auto cb = std::make_unique<CheckBox>();
cb->set_text("快速勾选");

// 获取勾选组 VSM
auto checked_vsm = cb->get_checked_vsm();  // 需要暴露接口
auto checked_state = checked_vsm->find_state("Checked");

// 修改动画参数
auto check_mark_anim = checked_state->find_animation("CheckMark.ScaleX");
check_mark_anim->set_duration(100ms);  // 加快动画速度
check_mark_anim->set_easing_function(EasingFunction::EaseInOut);
```

**MML 写法:**
```xml
<CheckBox text="快速勾选">
    <CheckBox.Resources>
        <VisualStateGroup name="CheckStates">
            <VisualState name="Checked">
                <Storyboard>
                    <DoubleAnimation target="CheckMark" property="ScaleX"
                                     from="0.8" to="1.0" 
                                     duration="100ms" 
                                     easingFunction="EaseInOut" />
                    <DoubleAnimation target="CheckMark" property="ScaleY"
                                     from="0.8" to="1.0" 
                                     duration="100ms" 
                                     easingFunction="EaseInOut" />
                </Storyboard>
            </VisualState>
        </VisualStateGroup>
    </CheckBox.Resources>
</CheckBox>
```

---

## 6. 最佳实践

### 6.1 使用双向绑定而非事件处理器

**原因:**
- 双向绑定自动同步 ViewModel,减少手动代码
- ViewModel 可单独测试,不依赖 UI 控件
- 支持多个 UI 元素绑定到同一属性

**反模式:**
```cpp
// ❌ 不推荐:在事件处理器中手动同步 ViewModel
checkbox->add_checked_changed_handler([this](auto&, auto& args) {
    this->view_model_->set_remember_password(args.new_value.as<bool>());
});
```

**推荐模式:**
```cpp
// ✅ 推荐:使用双向绑定
checkbox->set_binding(CheckBox::IsCheckedProperty, 
                      Binding(view_model, "RememberPassword", BindingMode::TwoWay));
```

---

### 6.2 批量操作时暂停事件通知

**原因:**
- 批量修改 CheckBox 状态时,频繁触发事件导致性能下降
- 使用 BeginUpdate/EndUpdate 机制批量更新

**反模式:**
```cpp
// ❌ 不推荐:循环中频繁触发事件
for (auto& cb : checkboxes) {
    cb->set_checked(true);  // 每次都触发 CheckedChangedEvent
}
```

**推荐模式:**
```cpp
// ✅ 推荐:批量更新
for (auto& cb : checkboxes) {
    cb->begin_update();
    cb->set_checked(true);
    cb->end_update();
}
// 最后统一通知
notify_selection_changed();
```

---

### 6.3 避免在构造函数中设置 IsChecked

**原因:**
- 构造函数中 CheckedChangedEvent 订阅者可能尚未注册
- 初始状态应通过数据绑定或 MML 属性设置

**反模式:**
```cpp
// ❌ 不推荐:构造函数中设置状态
CheckBox::CheckBox() {
    set_checked(true);  // 事件处理器尚未注册,不会触发
}
```

**推荐模式:**
```cpp
// ✅ 推荐:通过 MML 或外部设置
<CheckBox text="默认勾选" isChecked="true" />

// 或者在 on_initialized() 中设置
void CheckBox::on_initialized() {
    if (default_checked_) {
        set_checked(true);
    }
}
```

---

### 6.4 自定义勾选标记时保持无障碍支持

**原因:**
- 自定义 CheckMark 可能导致屏幕阅读器无法识别
- 确保 AutomationProperties 正确设置

**反模式:**
```cpp
// ❌ 不推荐:纯视觉自定义,无无障碍支持
auto custom_mark = std::make_unique<Image>();
custom_mark->set_source("check_icon.png");
// 屏幕阅读器无法识别勾选状态
```

**推荐模式:**
```cpp
// ✅ 推荐:设置无障碍属性
auto custom_mark = std::make_unique<Image>();
custom_mark->set_source("check_icon.png");
custom_mark->set_automation_name("已勾选");
custom_mark->set_automation_role(AutomationRole::CheckBox);
custom_mark->set_automation_checked_state(
    checkbox->is_checked() ? AutomationCheckedState::Checked 
                            : AutomationCheckedState::Unchecked
);
```

---

## 7. 常见陷阱

### 7.1 忘记处理 IsChecked 初始值

**问题:** CheckBox 默认未勾选,但业务逻辑期望初始勾选。

**错误代码:**
```cpp
auto agree_cb = std::make_unique<CheckBox>();
agree_cb->set_text("同意条款");
// ❌ 忘记设置初始值,用户必须先取消勾选再勾选才能触发事件
```

**后果:**
- 用户期望默认同意,但实际默认未同意
- 导致表单提交失败

**正确做法:**
```cpp
// ✅ 设置初始值
auto agree_cb = std::make_unique<CheckBox>();
agree_cb->set_text("同意条款");
agree_cb->set_checked(true);  // 明确设置初始状态
```

---

### 7.2 双 VSM 动画冲突导致视觉错误

**问题:** 同时触发交互组和勾选组动画,导致属性冲突。

**错误场景:**
- 用户鼠标悬停(Hovered),同时点击切换勾选状态
- 交互组动画正在修改 IconBorderBrushProperty
- 勾选组动画也修改 IconBorderBrushProperty
- 两个动画同时插值,导致颜色闪烁

**解决方案(已在实现中):**
```cpp
void CheckBox::on_visual_state_changed(ControlVisualState old_state, 
                                        ControlVisualState new_state) {
    // ✅ 启动新动画前先停止勾选组动画
    if (impl_->owned_checked_vsm_) {
        impl_->owned_checked_vsm_->stop_all_animations();
    }
    vsm_->go_to_state(state_name(new_state));
}

void CheckBox::execute_toggle() {
    // ✅ 启动勾选组动画前先停止交互组动画
    vsm_->stop_all_animations();
    impl_->owned_checked_vsm_->go_to_state(is_checked() ? "Checked" : "Unchecked");
}
```

---

### 7.3 误用 toggle() 导致状态不同步

**问题:** 在数据绑定的 CheckBox 上调用 `toggle()`,导致 ViewModel 和 View 不同步。

**错误代码:**
```cpp
// CheckBox 通过双向绑定连接到 ViewModel
checkbox->set_binding(CheckBox::IsCheckedProperty, 
                      Binding(vm, "IsEnabled", BindingMode::TwoWay));

// ❌ 手动调用 toggle() 会绕过绑定系统
button->add_click_handler([checkbox](auto&, auto&) {
    checkbox->toggle();  // ViewModel.IsEnabled 不会更新!
});
```

**后果:**
- CheckBox 视觉状态变化,但 ViewModel 属性未更新
- 数据保存时丢失用户选择

**正确做法:**
```cpp
// ✅ 通过 ViewModel 修改状态
button->add_click_handler([vm](auto&, auto&) {
    vm->set_is_enabled(!vm->is_enabled());
    // 绑定系统自动更新 CheckBox
});
```

---

### 7.4 在循环中创建大量 CheckBox 导致性能问题

**问题:** 每个 CheckBox 包含 StackPanel + 2 个 Border + CheckMark + TextBlock + 双 VSM,内存占用约 2KB。

**错误代码:**
```cpp
// ❌ 创建 10000 个 CheckBox(约 20MB 内存)
for (int i = 0; i < 10000; ++i) {
    auto cb = std::make_unique<CheckBox>();
    cb->set_text(std::format("选项 {}", i));
    panel->add_child(std::move(cb));
}
```

**后果:**
- 内存占用高,启动慢
- 布局和渲染性能下降

**正确做法:**
```cpp
// ✅ 使用 ItemsControl + 虚拟化
auto list = std::make_unique<ItemsControl>();
list->set_items_source(items_);  // std::vector<ItemData>
list->enable_virtualization(true);  // 仅渲染可见项

list->set_item_template([](const Variant& item) {
    auto data = item.as<ItemData>();
    auto cb = std::make_unique<CheckBox>();
    cb->set_text(data.name);
    cb->set_checked(data.is_selected);
    return cb;
});
```

---

## 8. 完整示例

### 8.1 完整的设置面板应用

**场景:** 用户设置面板,包含多个开关选项、分组、数据持久化。

**ViewModel (SettingsViewModel.h):**
```cpp
#pragma once
#include <mine/mvvm/ViewModelBase.h>
#include <mine/config/ConfigManager.h>
#include <string>
#include <memory>

namespace myapp {

class SettingsViewModel : public mine::mvvm::ViewModelBase {
public:
    // ========== 构造函数 ==========
    
    SettingsViewModel() {
        load_settings();
    }
    
    // ========== 通用设置 ==========
    
    bool auto_start() const { return auto_start_; }
    void set_auto_start(bool value) {
        if (auto_start_ != value) {
            auto_start_ = value;
            notify_property_changed("AutoStart");
            save_settings();
        }
    }
    
    bool minimize_to_tray() const { return minimize_to_tray_; }
    void set_minimize_to_tray(bool value) {
        if (minimize_to_tray_ != value) {
            minimize_to_tray_ = value;
            notify_property_changed("MinimizeToTray");
            save_settings();
        }
    }
    
    bool check_updates() const { return check_updates_; }
    void set_check_updates(bool value) {
        if (check_updates_ != value) {
            check_updates_ = value;
            notify_property_changed("CheckUpdates");
            save_settings();
        }
    }
    
    // ========== 通知设置 ==========
    
    bool enable_notifications() const { return enable_notifications_; }
    void set_enable_notifications(bool value) {
        if (enable_notifications_ != value) {
            enable_notifications_ = value;
            notify_property_changed("EnableNotifications");
            
            // 禁用通知时自动禁用子选项
            if (!value) {
                set_notification_sound(false);
                set_notification_badge(false);
            }
            
            save_settings();
        }
    }
    
    bool notification_sound() const { return notification_sound_; }
    void set_notification_sound(bool value) {
        if (notification_sound_ != value) {
            notification_sound_ = value;
            notify_property_changed("NotificationSound");
            save_settings();
        }
    }
    
    bool notification_badge() const { return notification_badge_; }
    void set_notification_badge(bool value) {
        if (notification_badge_ != value) {
            notification_badge_ = value;
            notify_property_changed("NotificationBadge");
            save_settings();
        }
    }
    
    // ========== 隐私设置 ==========
    
    bool analytics_enabled() const { return analytics_enabled_; }
    void set_analytics_enabled(bool value) {
        if (analytics_enabled_ != value) {
            analytics_enabled_ = value;
            notify_property_changed("AnalyticsEnabled");
            save_settings();
        }
    }
    
    bool crash_reports() const { return crash_reports_; }
    void set_crash_reports(bool value) {
        if (crash_reports_ != value) {
            crash_reports_ = value;
            notify_property_changed("CrashReports");
            save_settings();
        }
    }
    
private:
    // ========== 数据持久化 ==========
    
    void load_settings() {
        auto config = mine::config::ConfigManager::instance();
        auto_start_ = config->get_bool("general.auto_start", false);
        minimize_to_tray_ = config->get_bool("general.minimize_to_tray", true);
        check_updates_ = config->get_bool("general.check_updates", true);
        
        enable_notifications_ = config->get_bool("notifications.enabled", true);
        notification_sound_ = config->get_bool("notifications.sound", true);
        notification_badge_ = config->get_bool("notifications.badge", true);
        
        analytics_enabled_ = config->get_bool("privacy.analytics", true);
        crash_reports_ = config->get_bool("privacy.crash_reports", true);
    }
    
    void save_settings() {
        auto config = mine::config::ConfigManager::instance();
        config->set_bool("general.auto_start", auto_start_);
        config->set_bool("general.minimize_to_tray", minimize_to_tray_);
        config->set_bool("general.check_updates", check_updates_);
        
        config->set_bool("notifications.enabled", enable_notifications_);
        config->set_bool("notifications.sound", notification_sound_);
        config->set_bool("notifications.badge", notification_badge_);
        
        config->set_bool("privacy.analytics", analytics_enabled_);
        config->set_bool("privacy.crash_reports", crash_reports_);
        
        config->save();
    }
    
    // ========== 私有成员 ==========
    
    // 通用设置
    bool auto_start_ = false;
    bool minimize_to_tray_ = true;
    bool check_updates_ = true;
    
    // 通知设置
    bool enable_notifications_ = true;
    bool notification_sound_ = true;
    bool notification_badge_ = true;
    
    // 隐私设置
    bool analytics_enabled_ = true;
    bool crash_reports_ = true;
};

} // namespace myapp
```

**View (SettingsPage.mml):**
```xml
<Page x:class="myapp::SettingsPage"
      xmlns="http://mine-ui.org/mml"
      xmlns:x="http://mine-ui.org/mml/extensions">
    
    <Page.DataContext>
        <local:SettingsViewModel />
    </Page.DataContext>
    
    <ScrollViewer>
        <StackPanel width="600" 
                    padding="32"
                    spacing="24">
            
            <!-- 通用设置 -->
            <TextBlock text="通用设置" 
                       fontSize="24" 
                       fontWeight="Bold"
                       foreground="#6750A4"
                       marginBottom="16" />
            
            <StackPanel spacing="12">
                <CheckBox text="开机自动启动"
                          isChecked="{bind:vm.AutoStart, mode:TwoWay}" />
                
                <CheckBox text="最小化到系统托盘"
                          isChecked="{bind:vm.MinimizeToTray, mode:TwoWay}" />
                
                <CheckBox text="自动检查更新"
                          isChecked="{bind:vm.CheckUpdates, mode:TwoWay}" />
            </StackPanel>
            
            <!-- 分隔线 -->
            <Border height="1" 
                    background="#E0E0E0" 
                    marginTop="8" 
                    marginBottom="8" />
            
            <!-- 通知设置 -->
            <TextBlock text="通知设置" 
                       fontSize="24" 
                       fontWeight="Bold"
                       foreground="#6750A4"
                       marginBottom="16" />
            
            <StackPanel spacing="12">
                <CheckBox text="启用通知"
                          isChecked="{bind:vm.EnableNotifications, mode:TwoWay}" />
                
                <!-- 子选项(缩进 32dp) -->
                <StackPanel spacing="12" marginLeft="32">
                    <CheckBox text="通知声音"
                              isChecked="{bind:vm.NotificationSound, mode:TwoWay}"
                              isEnabled="{bind:vm.EnableNotifications}" />
                    
                    <CheckBox text="通知角标"
                              isChecked="{bind:vm.NotificationBadge, mode:TwoWay}"
                              isEnabled="{bind:vm.EnableNotifications}" />
                </StackPanel>
            </StackPanel>
            
            <!-- 分隔线 -->
            <Border height="1" 
                    background="#E0E0E0" 
                    marginTop="8" 
                    marginBottom="8" />
            
            <!-- 隐私设置 -->
            <TextBlock text="隐私设置" 
                       fontSize="24" 
                       fontWeight="Bold"
                       foreground="#6750A4"
                       marginBottom="16" />
            
            <StackPanel spacing="12">
                <StackPanel spacing="4">
                    <CheckBox text="发送匿名使用统计"
                              isChecked="{bind:vm.AnalyticsEnabled, mode:TwoWay}" />
                    <TextBlock text="帮助我们改进产品体验"
                               fontSize="12"
                               foreground="#666666"
                               marginLeft="32" />
                </StackPanel>
                
                <StackPanel spacing="4">
                    <CheckBox text="自动发送崩溃报告"
                              isChecked="{bind:vm.CrashReports, mode:TwoWay}" />
                    <TextBlock text="帮助我们快速修复问题"
                               fontSize="12"
                               foreground="#666666"
                               marginLeft="32" />
                </StackPanel>
            </StackPanel>
            
            <!-- 重置按钮 -->
            <Button text="恢复默认设置"
                    click="on_reset_click"
                    horizontalAlignment="Left"
                    marginTop="16"
                    background="#E0E0E0"
                    foreground="#333333" />
            
        </StackPanel>
    </ScrollViewer>
    
</Page>
```

**View Code-Behind (SettingsPage.cpp):**
```cpp
#include "SettingsPage.h"
#include "SettingsViewModel.h"
#include <mine/ui/controls/MessageBox.h>

namespace myapp {

SettingsPage::SettingsPage() {
    // MML 解析器自动加载 SettingsPage.mml 并设置 DataContext
}

void SettingsPage::on_reset_click(mine::ui::UIElement& sender, 
                                   mine::ui::RoutedEventArgs& args) {
    // 显示确认对话框
    auto result = mine::ui::MessageBox::show(
        "确定要恢复默认设置吗?此操作无法撤销。",
        "确认",
        mine::ui::MessageBoxButton::YesNo,
        mine::ui::MessageBoxIcon::Question
    );
    
    if (result == mine::ui::MessageBoxResult::Yes) {
        // 重置 ViewModel
        auto vm = data_context().as<SettingsViewModel*>();
        vm->set_auto_start(false);
        vm->set_minimize_to_tray(true);
        vm->set_check_updates(true);
        vm->set_enable_notifications(true);
        vm->set_notification_sound(true);
        vm->set_notification_badge(true);
        vm->set_analytics_enabled(true);
        vm->set_crash_reports(true);
        
        // 显示成功提示
        mine::ui::MessageBox::show(
            "已恢复默认设置",
            "成功",
            mine::ui::MessageBoxButton::OK,
            mine::ui::MessageBoxIcon::Information
        );
    }
}

} // namespace myapp
```

**CMakeLists.txt:**
```cmake
add_executable(SettingsApp
    main.cpp
    SettingsPage.cpp
    SettingsViewModel.cpp
)

target_link_libraries(SettingsApp PRIVATE
    mine::ui::controls
    mine::ui::binding
    mine::ui::layout
    mine::mvvm
    mine::config
    mine::mml
)

install(FILES SettingsPage.mml DESTINATION resources)
```

**main.cpp:**
```cpp
#include <mine/ui/app/Application.h>
#include <mine/ui/Window.h>
#include "SettingsPage.h"

int main(int argc, char* argv[]) {
    mine::ui::Application app(argc, argv);
    
    // 创建主窗口
    auto window = std::make_unique<mine::ui::Window>();
    window->set_title("应用设置");
    window->set_width(700);
    window->set_height(800);
    
    // 创建设置页面
    auto settings_page = std::make_unique<myapp::SettingsPage>();
    window->set_content(std::move(settings_page));
    
    // 显示窗口并运行消息循环
    window->show();
    return app.run();
}
```

**运行效果:**
1. 启动应用,显示设置面板
2. 所有 CheckBox 状态自动从配置文件加载
3. 用户点击任意 CheckBox:
   - 立即触发 VSM 动画(勾选标记淡入/淡出)
   - ViewModel 属性自动更新
   - 配置文件自动保存
4. 禁用"启用通知"时,"通知声音"和"通知角标"自动禁用
5. 点击"恢复默认设置"按钮:
   - 显示确认对话框
   - 所有 CheckBox 恢复到默认状态
   - 数据绑定自动刷新 UI

---

## 9. 总结

### 9.1 核心要点

- **CheckBox 是 MD3 Checkbox 的完整实现**,采用组合式视觉树架构,支持双 VSM 状态机分离管理交互状态和勾选状态。
- **双 VSM 冲突处理机制**:状态切换时互相停止对方动画,避免属性冲突和视觉错误。
- **MVVM 友好**:IsCheckedProperty 支持双向数据绑定,自动同步 ViewModel 布尔属性。
- **组合式视觉树**:StackPanel + Border + CheckMark + TextBlock,各基元独立控制样式,扩展性强。
- **无障碍支持**:State Layer 扩展到 40x40dp,键盘 Space 键触发切换,屏幕阅读器友好。

### 9.2 使用建议

- **MVVM 架构中优先使用双向绑定**,避免手动同步 ViewModel 和 View。
- **批量操作时使用 BeginUpdate/EndUpdate**,减少事件触发次数。
- **自定义样式时保持无障碍支持**,设置 AutomationProperties。
- **避免在循环中创建大量 CheckBox**,使用 ItemsControl + 虚拟化优化性能。

### 9.3 扩展方向

- **三态 CheckBox**:支持 Unchecked/Checked/Indeterminate 状态,用于父子层级选择。
- **开关样式 CheckBox**:通过 ControlTemplate 实现 iOS 风格的开关控件。
- **自定义 CheckMark**:使用图片或 SVG 路径替换默认勾选标记。
- **动画自定义**:调整勾选动画速度、缓动函数、Ripple 效果。

### 9.4 相关文档

- [Button 类 API 文档](06-Button.md)
- [Control 类 API 文档](../ui/01-Control.md)
- [VisualStateManager 状态管理器](../ui.animation/03-VisualStateManager.md)
- [数据绑定系统](../ui.binding/01-Binding.md)
- [Material Design 3 Checkbox 规范](https://m3.material.io/components/checkbox)

---

**文档版本:** v0.1.0  
**最后更新:** 2026-06-11  
**维护者:** MineFramework Team
