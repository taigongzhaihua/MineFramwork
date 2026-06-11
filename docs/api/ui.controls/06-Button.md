# Button 类 API 文档

## 1. 概述

`Button` 是 MineFramework 中最常用的交互控件,实现了 Material Design 3 (MD3) Filled Button 规范。它继承自 `ContentControl`,支持显示文本或任意 `UIElement` 内容,并提供完整的点击交互、命令绑定、视觉状态管理和动画反馈能力。

**核心职责:**

- **点击交互**:通过 `ClickEvent` 路由事件(Bubble 策略)向上传播点击行为,支持事件处理器订阅和命令绑定两种响应模式
- **命令绑定**:提供 `CommandProperty` 和 `CommandParameterProperty` 依赖属性,无缝集成 MVVM 模式的 `ICommand` 接口
- **视觉状态管理**:实现四种 MD3 规范状态 (Normal/Hovered/Pressed/Disabled),通过 VSM (Visual State Manager) 自动切换样式
- **动画反馈**:内置 Ripple 涟漪动画和 State Layer 状态层动画,提供流畅的触摸反馈
- **外观定制**:支持背景色、前景色、边框、圆角、内边距等丰富的样式属性

**继承关系:**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── ContentControl
                                            └── Button  ← 本文档
```

**架构意义:**

1. **MD3 规范实现**:严格遵循 Material Design 3 Filled Button 指南,默认背景色为 Primary (#6750A4),前景色为 On Primary,圆角为胶囊形,水平内边距 24dp,垂直内边距 10dp
2. **三层动画架构**:背景层(Background) + 状态层(State Layer) + 涟漪层(Ripple),三者协同工作实现丰富的交互效果
3. **双驱动模式**:State Layer 由 VSM Storyboard 驱动,Ripple 由 AnimationClock 直接驱动,在 `on_visual_state_changed()` 中统一注册 tick 回调
4. **命令模式集成**:CommandProperty 提供声明式的命令绑定,自动处理 CanExecute 状态同步到 IsEnabled 属性

**典型用途:**

- 表单提交按钮、对话框确认/取消按钮
- 工具栏操作按钮、导航按钮
- MVVM 架构中通过 CommandBinding 触发业务逻辑
- 自定义样式按钮(图标按钮、文本按钮、悬浮按钮等)

**设计原则:**

- **内容灵活性**:继承 ContentControl 的 Content 属性,支持纯文本、Icon+Text、自定义 UIElement 等多种内容形式
- **无障碍支持**:点击区域自动扩展到控件边界,键盘导航支持 Enter/Space 键触发点击
- **性能优化**:Ripple 动画使用 GPU 加速的 Canvas 绘制,State Layer 使用 Opacity 插值避免重绘

**性能特性:**

- **增量渲染**:仅在视觉状态变化或动画播放时标记为 InvalidateVisual,避免不必要的重绘
- **硬件加速**:Ripple 圆形扩散使用 DrawCircle 基元,由 GPU 光栅化
- **布局缓存**:文本测量结果缓存在 TextBlock 中,重复点击不触发重新测量

**相关类型:**

- `ICommand`:命令接口,定义 Execute() 和 CanExecute() 方法
- `RoutedEvent`:路由事件系统,ClickEvent 基于此实现
- `ControlVisualState`:控件视觉状态枚举 (Normal/Hovered/Pressed/Disabled)
- `AnimationClock`:动画时钟,驱动 Ripple 涟漪效果

**MD3 规范要点:**

- **尺寸**:最小宽度 64dp,高度 40dp,圆角半径 20dp(胶囊形)
- **颜色**:Primary 背景 + On Primary 文本,Hover 叠加 8% 黑色,Pressed 叠加 12% 黑色
- **动画**:Hover 淡入 150ms,Ripple 扩散 300ms + 淡出 200ms
- **无障碍**:最小点击区域 48x48dp,色彩对比度 > 4.5:1

**实现细节:**

- **StateLayerBrushProperty**:独立的状态层画刷,在 Hover/Press 时通过 VSM 插值改变 Opacity
- **CornerRadiusProperty**:支持四个角分别设置圆角半径(CornerRadii 结构体),默认胶囊形(半高度)
- **Ripple 起点**:鼠标点击位置作为涟漪扩散中心,触摸事件取触摸点坐标
- **命令同步**:CommandProperty 变更时自动订阅 CanExecuteChanged 事件,实时更新 IsEnabled 状态

---

## 2. 文件位置

**头文件:**
```
src/mine/ui/controls/Button.h
```

**实现文件:**
```
src/mine/ui/controls/Button.cpp
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
#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/DependencyProperty.h>
#include <mine/ui/RoutedEvent.h>
#include <mine/ui/ICommand.h>
#include <mine/ui/animation/AnimationClock.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Canvas.h>
#include <mine/math/Size.h>
#include <mine/math/Rect.h>
#include <mine/core/Variant.h>
```

**链接依赖:**
```
mine.ui.controls.lib
mine.ui.animation.lib
mine.paint.lib
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// @brief MD3 Filled Button 控件,支持点击交互、命令绑定和 Ripple 动画
///
/// 继承自 ContentControl,可承载文本或任意 UIElement 内容。
/// 实现四种视觉状态(Normal/Hovered/Pressed/Disabled)的自动切换,
/// 内置 State Layer 和 Ripple 两种动画效果。
///
/// @example 基础用法
/// @code
/// auto btn = std::make_unique<Button>();
/// btn->set_text("提交");
/// btn->add_click_handler([](auto& sender, auto& args) {
///     // 处理点击事件
/// });
/// @endcode
///
/// @example 命令绑定
/// @code
/// auto btn = std::make_unique<Button>();
/// btn->set_command(view_model->submit_command());
/// btn->set_command_parameter(core::Variant::from_int(42));
/// @endcode
class MINE_UI_CONTROLS_API Button : public ContentControl {
public:
    // ========== 路由事件 ==========
    
    /// @brief 点击事件,Bubble 策略向上冒泡
    /// @details 在鼠标/触摸按下并在控件边界内释放时触发。
    ///          键盘 Enter/Space 也会触发此事件。
    /// @return RoutedEvent 常量引用
    [[nodiscard]] static const RoutedEvent& ClickEvent();
    
    // ========== 依赖属性 ==========
    
    /// @brief 内容属性(继承自 ContentControl)
    using ContentControl::ContentProperty;
    
    /// @brief 背景画刷属性,默认 MD3 Primary (#6750A4)
    [[nodiscard]] static const DependencyProperty& BackgroundProperty;
    
    /// @brief 内边距属性,默认水平 24dp,垂直 10dp
    [[nodiscard]] static const DependencyProperty& PaddingProperty;
    
    /// @brief 前景画刷属性,默认 MD3 On Primary
    [[nodiscard]] static const DependencyProperty& ForegroundProperty;
    
    /// @brief 边框颜色属性,默认透明
    [[nodiscard]] static const DependencyProperty& BorderColorProperty;
    
    /// @brief 状态层画刷属性,Hover/Press 时叠加的蒙版颜色
    [[nodiscard]] static const DependencyProperty& StateLayerBrushProperty;
    
    /// @brief 命令属性,绑定 ICommand 接口
    [[nodiscard]] static const DependencyProperty& CommandProperty;
    
    /// @brief 命令参数属性,传递给 ICommand::Execute() 的参数
    [[nodiscard]] static const DependencyProperty& CommandParameterProperty;
    
    /// @brief 圆角半径属性,默认胶囊形(半高度)
    [[nodiscard]] static const DependencyProperty& CornerRadiusProperty;
    
    // ========== 构造/析构 ==========
    
    /// @brief 默认构造函数,初始化 MD3 默认样式
    Button();
    
    /// @brief 析构函数,清理动画资源和事件订阅
    ~Button() override;
    
    // 禁止拷贝和移动
    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;
    Button(Button&&) = delete;
    Button& operator=(Button&&) = delete;
    
    // ========== 属性访问器 ==========
    
    /// @brief 获取按钮文本(仅当 Content 为文本时有效)
    /// @return 文本字符串视图
    [[nodiscard]] core::StringView text() const noexcept;
    
    /// @brief 设置按钮文本,内部转换为 InlineString 存入 ContentProperty
    /// @param text 文本内容
    void set_text(core::StringView text);
    
    /// @brief 获取背景画刷
    [[nodiscard]] paint::Brush background() const noexcept;
    
    /// @brief 设置背景画刷
    /// @param brush 画刷对象
    void set_background(paint::Brush brush);
    
    /// @brief 获取前景画刷
    [[nodiscard]] paint::Brush foreground() const noexcept;
    
    /// @brief 设置前景画刷
    /// @param brush 画刷对象
    void set_foreground(paint::Brush brush);
    
    /// @brief 设置字体(仅当 Content 为文本时有效)
    /// @param font_face HarfBuzz hb_font_t 指针
    void set_font_face(void* font_face) noexcept;
    
    /// @brief 设置字体大小(仅当 Content 为文本时有效)
    /// @param size_px 字体大小,单位像素
    void set_font_size(float size_px) noexcept;
    
    /// @brief 设置命令
    /// @param cmd ICommand 接口指针,生命周期由调用方管理
    void set_command(ICommand* cmd) noexcept;
    
    /// @brief 设置命令参数
    /// @param param 任意 Variant 值,传递给 ICommand::Execute()
    void set_command_parameter(core::Variant param) noexcept;
    
    /// @brief 添加点击事件处理器
    /// @param handler 事件处理函数
    void add_click_handler(std::function<void(UIElement&, RoutedEventArgs&)> handler);
    
protected:
    // ========== 布局虚函数 ==========
    
    /// @brief 测量阶段,计算所需尺寸
    /// @param available_size 可用空间
    void on_measure(math::Size available_size) override;
    
    /// @brief 排列阶段,确定最终位置和大小
    /// @param final_rect 最终矩形区域
    void on_arrange(math::Rect final_rect) override;
    
    // ========== 渲染虚函数 ==========
    
    /// @brief 渲染按钮,绘制背景、状态层、内容和 Ripple
    /// @param canvas 绘图画布
    void on_render(paint::Canvas& canvas) override;
    
    // ========== 视觉状态虚函数 ==========
    
    /// @brief 计算当前视觉状态
    /// @return ControlVisualState 枚举值
    [[nodiscard]] ControlVisualState compute_visual_state() const noexcept override;
    
    /// @brief 视觉状态变化回调,启动 VSM 动画和 Ripple 动画
    /// @param old_state 旧状态
    /// @param new_state 新状态
    void on_visual_state_changed(ControlVisualState old_state, 
                                   ControlVisualState new_state) override;
    
    // ========== 输入事件虚函数 ==========
    
    /// @brief 鼠标按下事件,记录 Ripple 起点
    void on_mouse_down(input::MouseButtonEventArgs& args) override;
    
    /// @brief 鼠标释放事件,触发 Click 事件
    void on_mouse_up(input::MouseButtonEventArgs& args) override;
    
    /// @brief 键盘按下事件,处理 Enter/Space 键
    void on_key_down(input::KeyEventArgs& args) override;
    
private:
    // ========== 内部实现 ==========
    
    /// @brief 执行点击逻辑,触发 ClickEvent 和 Command
    void execute_click();
    
    /// @brief 启动 Ripple 动画
    /// @param origin 涟漪起点(控件坐标系)
    void start_ripple(math::Point origin);
    
    /// @brief Ripple 动画 tick 回调
    void on_ripple_tick(float progress);
    
    /// @brief State Layer 动画 tick 回调
    void on_state_layer_tick();
    
    /// @brief 命令 CanExecuteChanged 事件处理器
    void on_command_can_execute_changed();
    
    // 私有成员(实现细节)
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mine::ui
```

### 3.2 依赖类型

```cpp
/// @brief 控件视觉状态枚举
enum class ControlVisualState {
    Normal,    ///< 正常状态
    Hovered,   ///< 鼠标悬停
    Pressed,   ///< 按下状态
    Disabled   ///< 禁用状态
};

/// @brief 命令接口
class ICommand {
public:
    virtual ~ICommand() = default;
    
    /// @brief 执行命令
    /// @param parameter 命令参数
    virtual void execute(const core::Variant& parameter) = 0;
    
    /// @brief 查询命令是否可执行
    /// @param parameter 命令参数
    /// @return true 可执行,false 禁用
    virtual bool can_execute(const core::Variant& parameter) const = 0;
    
    /// @brief 可执行状态变化事件
    event::Event<> can_execute_changed;
};

/// @brief 圆角半径结构体
struct CornerRadii {
    float top_left;      ///< 左上角
    float top_right;     ///< 右上角
    float bottom_right;  ///< 右下角
    float bottom_left;   ///< 左下角
    
    /// @brief 统一圆角构造
    static CornerRadii uniform(float radius);
};
```

### 3.3 内部实现细节

Button 内部使用 PImpl 模式隐藏实现细节,主要包含:

```cpp
struct Button::Impl {
    // Ripple 动画状态
    math::Point ripple_origin;          ///< 涟漪起点
    float ripple_progress;              ///< 进度 0.0 ~ 1.0
    float ripple_max_radius;            ///< 最大半径
    anim::AnimationClock ripple_clock;  ///< 动画时钟
    
    // State Layer 状态
    float state_layer_opacity;          ///< 当前透明度
    
    // 命令绑定
    ICommand* bound_command;            ///< 绑定的命令
    event::EventToken command_token;    ///< 事件订阅令牌
    
    // 文本缓存(优化性能)
    std::unique_ptr<TextBlock> text_block;
};
```

---

## 4. 成员方法

### 4.1 静态方法

#### 4.1.1 ClickEvent()

```cpp
[[nodiscard]] static const RoutedEvent& ClickEvent();
```

**功能:** 返回点击路由事件的全局单例。

**返回值:** `RoutedEvent` 常量引用,事件名称为 "Button.Click",路由策略为 Bubble。

**用法:**
```cpp
// 添加事件处理器
button->add_event_handler(Button::ClickEvent(), [](auto& sender, auto& args) {
    auto& btn = static_cast<Button&>(sender);
    std::cout << "按钮被点击: " << btn.text() << std::endl;
});
```

**触发时机:**
- 鼠标左键在按钮内按下并释放
- 触摸事件在按钮内开始并结束
- 键盘焦点在按钮上时按下 Enter 或 Space 键

**事件参数:** `RoutedEventArgs`,包含 `source`(事件源)和 `handled`(处理标记)。

**冒泡机制:** 事件从 Button 向上遍历视觉树,依次触发父元素上注册的处理器,直到到达根元素或被标记为 `handled`。

---

#### 4.1.2 依赖属性访问器

```cpp
[[nodiscard]] static const DependencyProperty& BackgroundProperty;
[[nodiscard]] static const DependencyProperty& PaddingProperty;
[[nodiscard]] static const DependencyProperty& ForegroundProperty;
[[nodiscard]] static const DependencyProperty& BorderColorProperty;
[[nodiscard]] static const DependencyProperty& StateLayerBrushProperty;
[[nodiscard]] static const DependencyProperty& CommandProperty;
[[nodiscard]] static const DependencyProperty& CommandParameterProperty;
[[nodiscard]] static const DependencyProperty& CornerRadiusProperty;
```

**功能:** 返回对应依赖属性的全局注册对象。

**用法:**
```cpp
// 通过依赖属性系统设置值
button->set_value(Button::BackgroundProperty, 
                  core::Variant::from<paint::Brush>(paint::SolidColorBrush(0xFF0000)));

// 数据绑定
button->set_binding(Button::CommandProperty, 
                    Binding(view_model, "SubmitCommand"));
```

**属性元数据:**

| 属性 | 类型 | 默认值 | 影响 |
|-----|------|--------|------|
| BackgroundProperty | paint::Brush | MD3 Primary (#6750A4) | InvalidateVisual |
| PaddingProperty | Thickness | {24, 10, 24, 10} | InvalidateMeasure |
| ForegroundProperty | paint::Brush | MD3 On Primary | InvalidateVisual |
| BorderColorProperty | paint::Brush | Transparent | InvalidateVisual |
| StateLayerBrushProperty | paint::Brush | 黑色 8% | InvalidateVisual |
| CommandProperty | ICommand* | nullptr | 影响 IsEnabled |
| CommandParameterProperty | Variant | Empty | 无 |
| CornerRadiusProperty | CornerRadii | 胶囊形 | InvalidateVisual |

---

### 4.2 构造与析构

#### 4.2.1 Button()

```cpp
Button();
```

**功能:** 构造函数,初始化 MD3 默认样式。

**初始化内容:**
- 设置默认背景色为 MD3 Primary (#6750A4)
- 设置默认前景色为 MD3 On Primary (白色)
- 设置内边距为 {24, 10, 24, 10}
- 设置圆角为胶囊形(高度的一半)
- 初始化 Ripple 动画时钟
- 注册 PointerPressed/PointerReleased 事件处理器

**注意事项:**
- 不要在构造函数中调用虚函数(如 `on_measure`)
- 控件必须加入视觉树后才能接收输入事件

---

#### 4.2.2 ~Button()

```cpp
~Button() override;
```

**功能:** 析构函数,清理资源。

**清理内容:**
- 取消订阅命令的 CanExecuteChanged 事件
- 停止并释放 Ripple 动画时钟
- 释放内部文本缓存

---

### 4.3 属性访问器

#### 4.3.1 text() / set_text()

```cpp
[[nodiscard]] core::StringView text() const noexcept;
void set_text(core::StringView text);
```

**功能:** 获取/设置按钮文本。

**参数:**
- `text`: 文本内容,支持 UTF-8 编码

**返回值:** `StringView` 零拷贝字符串视图。

**实现细节:**
- `set_text()` 内部调用 `set_value(ContentProperty, Variant::from<InlineString>(text))`
- `text()` 仅在 Content 为 InlineString 类型时返回有效值,否则返回空视图

**用法:**
```cpp
button->set_text("提交表单");
auto current_text = button->text();  // "提交表单"
```

**注意:** 如果 Content 被设置为 UIElement,`text()` 返回空字符串。

---

#### 4.3.2 background() / set_background()

```cpp
[[nodiscard]] paint::Brush background() const noexcept;
void set_background(paint::Brush brush);
```

**功能:** 获取/设置背景画刷。

**参数:**
- `brush`: 画刷对象,可以是 SolidColorBrush、LinearGradientBrush 等

**返回值:** 当前背景画刷的拷贝。

**用法:**
```cpp
// 纯色背景
button->set_background(paint::SolidColorBrush(0xFF5722));

// 渐变背景
auto gradient = paint::LinearGradientBrush(
    math::Point{0, 0}, math::Point{0, 40},
    {
        {0.0f, 0xFF6750A4},
        {1.0f, 0xFF9C27B0}
    }
);
button->set_background(gradient);
```

**性能提示:** 修改背景触发 InvalidateVisual,但不触发重新测量。

---

#### 4.3.3 foreground() / set_foreground()

```cpp
[[nodiscard]] paint::Brush foreground() const noexcept;
void set_foreground(paint::Brush brush);
```

**功能:** 获取/设置前景画刷(文本颜色)。

**用法:**
```cpp
button->set_foreground(paint::SolidColorBrush(0xFFFFFF));  // 白色文本
```

---

#### 4.3.4 set_command() / set_command_parameter()

```cpp
void set_command(ICommand* cmd) noexcept;
void set_command_parameter(core::Variant param) noexcept;
```

**功能:** 设置命令绑定。

**参数:**
- `cmd`: ICommand 接口指针,生命周期由调用方管理(通常存储在 ViewModel 中)
- `param`: 命令参数,传递给 `ICommand::Execute()`

**行为:**
- 设置 Command 后,Button 自动订阅 `can_execute_changed` 事件
- `can_execute()` 返回 false 时,Button 自动设置 `IsEnabled = false`
- 点击按钮时自动调用 `cmd->execute(param)`

**用法:**
```cpp
// ViewModel 中定义命令
class SubmitCommand : public ICommand {
public:
    void execute(const core::Variant& param) override {
        int id = param.as<int>();
        // 提交数据...
    }
    
    bool can_execute(const core::Variant& param) const override {
        return form_is_valid_;
    }
};

// 视图中绑定
auto cmd = std::make_shared<SubmitCommand>();
button->set_command(cmd.get());
button->set_command_parameter(core::Variant::from_int(42));
```

**注意:** Command 为空指针时,按钮仍可正常触发 ClickEvent。

---

#### 4.3.5 add_click_handler()

```cpp
void add_click_handler(std::function<void(UIElement&, RoutedEventArgs&)> handler);
```

**功能:** 添加点击事件处理器的便捷方法。

**参数:**
- `handler`: Lambda 或函数对象,签名为 `void(UIElement&, RoutedEventArgs&)`

**用法:**
```cpp
button->add_click_handler([](UIElement& sender, RoutedEventArgs& args) {
    auto& btn = static_cast<Button&>(sender);
    std::cout << "按钮被点击!" << std::endl;
    args.handled = true;  // 阻止事件继续冒泡
});
```

**等价写法:**
```cpp
button->add_event_handler(Button::ClickEvent(), handler);
```

---

### 4.4 布局虚函数

#### 4.4.1 on_measure()

```cpp
void on_measure(math::Size available_size) override;
```

**功能:** 测量按钮所需尺寸。

**参数:**
- `available_size`: 父容器提供的可用空间,可能为无限大(`Infinity`)

**返回值:** 通过 `set_desired_size()` 设置所需尺寸。

**测量逻辑:**
1. 测量内容(文本或 UIElement)所需尺寸
2. 加上 Padding 和 BorderThickness
3. 应用最小尺寸约束(MinWidth/MinHeight)
4. 遵循 MD3 最小宽度 64dp,最小高度 40dp

**示例:**
```cpp
// 假设文本测量为 80x20,Padding 为 {24,10,24,10}
// 最终 DesiredSize = {80+24+24, 20+10+10} = {128, 40}
```

---

#### 4.4.2 on_arrange()

```cpp
void on_arrange(math::Rect final_rect) override;
```

**功能:** 确定按钮最终位置和大小。

**参数:**
- `final_rect`: 父容器分配的最终矩形区域

**布局逻辑:**
1. 计算内容区域(减去 Padding)
2. 居中对齐内容(文本或 UIElement)
3. 设置 Ripple 最大半径为对角线长度

**示例:**
```cpp
// final_rect = {0, 0, 150, 50}
// Padding = {24, 10, 24, 10}
// content_rect = {24, 10, 102, 30}
```

---

### 4.5 渲染虚函数

#### 4.5.1 on_render()

```cpp
void on_render(paint::Canvas& canvas) override;
```

**功能:** 绘制按钮。

**绘制顺序:**
1. **背景层**: 使用 BackgroundProperty 填充圆角矩形
2. **状态层**: 使用 StateLayerBrushProperty 叠加半透明蒙版(Hover/Press 时可见)
3. **内容层**: 绘制文本或子元素
4. **Ripple 层**: 绘制涟漪圆形(按下时可见)
5. **边框**: 使用 BorderColorProperty 描边圆角矩形

**伪代码:**
```cpp
void Button::on_render(Canvas& canvas) {
    auto bg_brush = get_value<Brush>(BackgroundProperty);
    auto corner = get_value<CornerRadii>(CornerRadiusProperty);
    
    // 1. 绘制背景
    canvas.draw_rounded_rect(bounds(), corner, bg_brush);
    
    // 2. 绘制状态层(Hover/Press)
    if (impl_->state_layer_opacity > 0) {
        auto layer_brush = get_value<Brush>(StateLayerBrushProperty);
        layer_brush.set_opacity(impl_->state_layer_opacity);
        canvas.draw_rounded_rect(bounds(), corner, layer_brush);
    }
    
    // 3. 绘制内容
    render_content(canvas);
    
    // 4. 绘制 Ripple
    if (impl_->ripple_progress > 0) {
        float radius = impl_->ripple_max_radius * impl_->ripple_progress;
        float alpha = 1.0f - impl_->ripple_progress;
        canvas.draw_circle(impl_->ripple_origin, radius, 
                          SolidColorBrush(0x20FFFFFF).with_opacity(alpha));
    }
}
```

---

### 4.6 视觉状态虚函数

#### 4.6.1 compute_visual_state()

```cpp
[[nodiscard]] ControlVisualState compute_visual_state() const noexcept override;
```

**功能:** 计算当前视觉状态。

**返回值:** `ControlVisualState` 枚举值。

**判断逻辑:**
```cpp
ControlVisualState Button::compute_visual_state() const noexcept {
    if (!is_enabled()) return ControlVisualState::Disabled;
    if (is_pressed()) return ControlVisualState::Pressed;
    if (is_hovered()) return ControlVisualState::Hovered;
    return ControlVisualState::Normal;
}
```

**调用时机:** 鼠标进入/离开、按下/释放、IsEnabled 属性变化时自动调用。

---

#### 4.6.2 on_visual_state_changed()

```cpp
void on_visual_state_changed(ControlVisualState old_state, 
                               ControlVisualState new_state) override;
```

**功能:** 视觉状态变化时的回调,启动动画。

**参数:**
- `old_state`: 旧状态
- `new_state`: 新状态

**动画启动逻辑:**
```cpp
void Button::on_visual_state_changed(ControlVisualState old, 
                                      ControlVisualState new_state) {
    // 1. 启动 VSM 状态组动画(State Layer 淡入/淡出)
    vsm_->go_to_state(state_name(new_state));
    
    // 2. 如果切换到 Pressed,启动 Ripple 动画
    if (new_state == ControlVisualState::Pressed) {
        start_ripple(last_mouse_position_);
    }
    
    // 3. 注册动画 tick 回调到 AnimationClock
    AnimationClock::instance().register_callback([this]() {
        on_state_layer_tick();
        on_ripple_tick();
    });
}
```

**VSM 状态名称映射:**
- Normal → "Normal"
- Hovered → "PointerOver"
- Pressed → "Pressed"
- Disabled → "Disabled"

---

### 4.7 输入事件虚函数

#### 4.7.1 on_mouse_down()

```cpp
void on_mouse_down(input::MouseButtonEventArgs& args) override;
```

**功能:** 鼠标按下事件,记录 Ripple 起点。

**实现:**
```cpp
void Button::on_mouse_down(MouseButtonEventArgs& args) {
    if (args.button == MouseButton::Left) {
        capture_mouse();  // 捕获鼠标
        impl_->ripple_origin = args.position;  // 记录起点
        args.handled = true;
    }
    Control::on_mouse_down(args);
}
```

---

#### 4.7.2 on_mouse_up()

```cpp
void on_mouse_up(input::MouseButtonEventArgs& args) override;
```

**功能:** 鼠标释放事件,触发 Click。

**实现:**
```cpp
void Button::on_mouse_up(MouseButtonEventArgs& args) {
    if (args.button == MouseButton::Left && is_mouse_captured()) {
        release_mouse_capture();
        
        // 检查释放位置是否在控件内
        if (bounds().contains(args.position)) {
            execute_click();
        }
        
        args.handled = true;
    }
    Control::on_mouse_up(args);
}
```

---

#### 4.7.3 on_key_down()

```cpp
void on_key_down(input::KeyEventArgs& args) override;
```

**功能:** 键盘按下事件,处理 Enter/Space。

**实现:**
```cpp
void Button::on_key_down(KeyEventArgs& args) {
    if (args.key == Key::Enter || args.key == Key::Space) {
        execute_click();
        args.handled = true;
    }
    Control::on_key_down(args);
}
```

---

### 4.8 私有方法

#### 4.8.1 execute_click()

```cpp
void execute_click();
```

**功能:** 执行点击逻辑。

**执行顺序:**
1. 触发 ClickEvent 路由事件
2. 如果事件未被标记为 handled,检查 CommandProperty
3. 如果 Command 不为空且 can_execute() 返回 true,调用 execute()

**伪代码:**
```cpp
void Button::execute_click() {
    auto args = RoutedEventArgs(this);
    raise_event(ClickEvent(), args);
    
    if (!args.handled && impl_->bound_command) {
        auto param = get_value(CommandParameterProperty);
        if (impl_->bound_command->can_execute(param)) {
            impl_->bound_command->execute(param);
        }
    }
}
```

---

#### 4.8.2 start_ripple()

```cpp
void start_ripple(math::Point origin);
```

**功能:** 启动 Ripple 涟漪动画。

**参数:**
- `origin`: 涟漪起点(控件坐标系)

**动画参数:**
- 持续时间: 300ms 扩散 + 200ms 淡出
- 缓动函数: EaseOut
- 最大半径: 控件对角线长度

**实现:**
```cpp
void Button::start_ripple(math::Point origin) {
    impl_->ripple_origin = origin;
    impl_->ripple_progress = 0.0f;
    
    impl_->ripple_clock.start(500ms, [this](float t) {
        impl_->ripple_progress = t;
        invalidate_visual();
        return t < 1.0f;  // 返回 false 停止动画
    });
}
```

---

#### 4.8.3 on_command_can_execute_changed()

```cpp
void on_command_can_execute_changed();
```

**功能:** 命令可执行状态变化时的回调。

**实现:**
```cpp
void Button::on_command_can_execute_changed() {
    if (impl_->bound_command) {
        auto param = get_value(CommandParameterProperty);
        bool can_exec = impl_->bound_command->can_execute(param);
        set_is_enabled(can_exec);
    }
}
```

---

## 5. 使用场景

### 5.1 基础文本按钮

**场景:** 表单提交、对话框确认。

**代码:**
```cpp
auto submit_btn = std::make_unique<Button>();
submit_btn->set_text("提交");
submit_btn->add_click_handler([](auto& sender, auto& args) {
    // 提交表单逻辑
    submit_form();
});
```

**MML 写法:**
```xml
<Button text="提交" click="on_submit_click" />
```

---

### 5.2 MVVM 命令绑定

**场景:** MVVM 架构,分离视图与业务逻辑。

**ViewModel:**
```cpp
class FormViewModel {
public:
    class SubmitCommand : public ICommand {
    public:
        SubmitCommand(FormViewModel* vm) : vm_(vm) {}
        
        void execute(const Variant& param) override {
            vm_->submit();
        }
        
        bool can_execute(const Variant& param) const override {
            return vm_->is_form_valid();
        }
        
    private:
        FormViewModel* vm_;
    };
    
    SubmitCommand* submit_command() { return &submit_cmd_; }
    
    bool is_form_valid() const { return !name_.empty(); }
    
    void submit() {
        // 提交数据到服务器
        api_client_->post("/submit", {{"name", name_}});
    }
    
private:
    SubmitCommand submit_cmd_{this};
    std::string name_;
};
```

**View:**
```cpp
auto view_model = std::make_shared<FormViewModel>();

auto submit_btn = std::make_unique<Button>();
submit_btn->set_text("提交");
submit_btn->set_command(view_model->submit_command());
```

**MML 写法:**
```xml
<Button text="提交" command="{bind:vm.SubmitCommand}" />
```

**优势:** 当表单无效时,`is_form_valid()` 返回 false,按钮自动变为禁用状态。

---

### 5.3 自定义样式按钮

**场景:** 品牌定制、特殊设计。

**代码:**
```cpp
// 创建渐变背景按钮
auto gradient_btn = std::make_unique<Button>();
gradient_btn->set_text("Gradient");

auto gradient = paint::LinearGradientBrush(
    math::Point{0, 0}, math::Point{0, 50},
    {
        {0.0f, 0xFFE91E63},  // Pink
        {1.0f, 0xFF9C27B0}   // Purple
    }
);
gradient_btn->set_background(gradient);
gradient_btn->set_foreground(paint::SolidColorBrush(0xFFFFFF));

// 创建圆形按钮
auto circle_btn = std::make_unique<Button>();
circle_btn->set_text("+");
circle_btn->set_value(Button::CornerRadiusProperty, 
                      Variant::from(CornerRadii::uniform(25.0f)));
circle_btn->set_width(50);
circle_btn->set_height(50);
```

**MML 写法:**
```xml
<Button text="Gradient">
    <Button.Background>
        <LinearGradientBrush startPoint="0,0" endPoint="0,50">
            <GradientStop offset="0" color="#E91E63" />
            <GradientStop offset="1" color="#9C27B0" />
        </LinearGradientBrush>
    </Button.Background>
    <Button.Foreground>
        <SolidColorBrush color="#FFFFFF" />
    </Button.Foreground>
</Button>
```

---

### 5.4 图标按钮

**场景:** 工具栏、导航栏。

**代码:**
```cpp
// 使用 SVG 图标作为内容
auto icon = std::make_unique<Path>();
icon->set_data("M12 2l3.09 6.26L22 9.27l-5 4.87 1.18 6.88L12 17.77l-6.18 3.25L7 14.14 2 9.27l6.91-1.01L12 2z");
icon->set_fill(paint::SolidColorBrush(0xFFFFFF));

auto icon_btn = std::make_unique<Button>();
icon_btn->set_content(std::move(icon));  // 使用 UIElement 作为内容
icon_btn->set_width(48);
icon_btn->set_height(48);
icon_btn->set_value(Button::CornerRadiusProperty, 
                    Variant::from(CornerRadii::uniform(24.0f)));
```

**MML 写法:**
```xml
<Button width="48" height="48" cornerRadius="24">
    <Path data="M12 2l3.09 6.26L22 9.27..." fill="#FFFFFF" />
</Button>
```

---

### 5.5 Ripple 动画自定义

**场景:** 调整涟漪颜色、速度。

**代码:**
```cpp
auto btn = std::make_unique<Button>();
btn->set_text("Custom Ripple");

// 自定义状态层颜色(默认黑色 8%)
btn->set_value(Button::StateLayerBrushProperty, 
               Variant::from(paint::SolidColorBrush(0x20FFFFFF)));  // 白色 12%

// 通过 VSM 调整动画速度
auto vsm = btn->visual_state_manager();
auto pressed_state = vsm->find_state("Pressed");
auto ripple_anim = pressed_state->find_animation("RippleAnimation");
ripple_anim->set_duration(200ms);  // 加快涟漪速度
```

**MML 写法:**
```xml
<Button text="Custom Ripple" stateLayerBrush="#20FFFFFF">
    <Button.Resources>
        <VisualStateGroup name="CommonStates">
            <VisualState name="Pressed">
                <Storyboard>
                    <DoubleAnimation target="RippleLayer" property="Opacity"
                                     to="1.0" duration="200ms" />
                </Storyboard>
            </VisualState>
        </VisualStateGroup>
    </Button.Resources>
</Button>
```

---

### 5.6 异步操作与加载状态

**场景:** 网络请求、文件上传。

**代码:**
```cpp
auto upload_btn = std::make_unique<Button>();
upload_btn->set_text("上传文件");

upload_btn->add_click_handler([btn_ptr = upload_btn.get()](auto&, auto&) {
    // 禁用按钮,防止重复点击
    btn_ptr->set_is_enabled(false);
    btn_ptr->set_text("上传中...");
    
    // 启动异步操作
    async::post_task([btn_ptr]() {
        // 模拟文件上传
        std::this_thread::sleep_for(2s);
        upload_file();
        
        // 回到 UI 线程恢复状态
        async::invoke_on_ui_thread([btn_ptr]() {
            btn_ptr->set_text("上传完成");
            btn_ptr->set_is_enabled(true);
        });
    });
});
```

**改进版(使用命令模式):**
```cpp
class UploadCommand : public ICommand {
public:
    void execute(const Variant& param) override {
        is_uploading_ = true;
        can_execute_changed.raise();
        
        async::post_task([this]() {
            upload_file();
            is_uploading_ = false;
            async::invoke_on_ui_thread([this]() {
                can_execute_changed.raise();
            });
        });
    }
    
    bool can_execute(const Variant& param) const override {
        return !is_uploading_;
    }
    
private:
    bool is_uploading_ = false;
};

// 使用
auto cmd = std::make_shared<UploadCommand>();
upload_btn->set_command(cmd.get());
upload_btn->set_text("上传文件");
```

---

### 5.7 动态按钮列表

**场景:** 标签页、选项卡。

**代码:**
```cpp
// 创建动态按钮列表
auto panel = std::make_unique<StackPanel>();
panel->set_orientation(Orientation::Horizontal);
panel->set_spacing(8);

std::vector<std::string> tabs = {"首页", "消息", "我的"};
for (size_t i = 0; i < tabs.size(); ++i) {
    auto tab_btn = std::make_unique<Button>();
    tab_btn->set_text(tabs[i]);
    
    // 使用 CommandParameter 传递索引
    tab_btn->set_command(switch_tab_cmd_.get());
    tab_btn->set_command_parameter(Variant::from_int(static_cast<int>(i)));
    
    // 高亮当前选中标签
    if (i == current_tab_index_) {
        tab_btn->set_background(SolidColorBrush(0xFF6750A4));
    } else {
        tab_btn->set_background(SolidColorBrush(0xFFE0E0E0));
    }
    
    panel->add_child(std::move(tab_btn));
}
```

**MML 写法(使用数据绑定):**
```xml
<StackPanel orientation="Horizontal" spacing="8">
    <ItemsControl itemsSource="{bind:vm.Tabs}">
        <ItemsControl.ItemTemplate>
            <DataTemplate>
                <Button text="{bind:item.Name}" 
                        command="{bind:vm.SwitchTabCommand}"
                        commandParameter="{bind:item.Index}"
                        background="{bind:item.IsSelected, converter:SelectedBgConverter}" />
            </DataTemplate>
        </ItemsControl.ItemTemplate>
    </ItemsControl>
</StackPanel>
```

---

### 5.8 可访问性支持

**场景:** 键盘导航、屏幕阅读器。

**代码:**
```cpp
auto btn = std::make_unique<Button>();
btn->set_text("提交");

// 设置可访问性属性
btn->set_automation_name("提交表单按钮");
btn->set_automation_help_text("点击此按钮将提交当前表单数据");
btn->set_tab_index(1);  // 键盘 Tab 顺序

// 添加快捷键
btn->set_access_key("S");  // Alt+S 触发
```

**键盘交互:**
- **Tab**: 移动焦点到按钮
- **Enter/Space**: 触发点击
- **Esc**: 取消焦点(如果在对话框中)

**屏幕阅读器朗读:** "提交表单按钮,按钮,按 Alt+S 激活"

---

## 6. 最佳实践

### 6.1 优先使用命令绑定而非事件处理器

**原因:**
- 命令模式自动处理 CanExecute 逻辑,按钮状态自动同步
- 业务逻辑集中在 ViewModel,便于单元测试
- 支持多个按钮共享同一命令(如工具栏和菜单项)

**反模式:**
```cpp
// ❌ 不推荐:在事件处理器中直接写业务逻辑
button->add_click_handler([this](auto&, auto&) {
    if (this->validate_form()) {
        this->submit_to_server();
    }
});
```

**推荐模式:**
```cpp
// ✅ 推荐:使用命令绑定
class ViewModel {
public:
    class SubmitCommand : public ICommand {
        void execute(const Variant&) override { vm_->submit(); }
        bool can_execute(const Variant&) const override { return vm_->is_valid(); }
    };
    
    SubmitCommand* submit_command() { return &submit_cmd_; }
};

button->set_command(view_model->submit_command());
```

---

### 6.2 使用 MML 声明式定义而非代码构造

**原因:**
- MML 可视化编辑,支持设计器拖拽
- 数据绑定语法更简洁
- 样式和模板可复用

**反模式:**
```cpp
// ❌ 不推荐:纯代码创建复杂按钮
auto btn = std::make_unique<Button>();
btn->set_text("提交");
btn->set_background(SolidColorBrush(0xFF6750A4));
btn->set_foreground(SolidColorBrush(0xFFFFFF));
btn->set_width(120);
btn->set_height(40);
btn->set_value(Button::CornerRadiusProperty, Variant::from(CornerRadii::uniform(20.0f)));
btn->set_command(view_model->submit_command());
```

**推荐模式:**
```xml
<!-- ✅ 推荐:使用 MML -->
<Button text="提交" 
        background="#6750A4" 
        foreground="#FFFFFF"
        width="120" 
        height="40"
        cornerRadius="20"
        command="{bind:vm.SubmitCommand}" />
```

---

### 6.3 避免在循环中创建大量按钮

**原因:**
- 每个 Button 实例占用约 1KB 内存(包括 VSM、动画时钟)
- 大量控件导致布局和渲染性能下降

**反模式:**
```cpp
// ❌ 不推荐:创建 1000 个按钮
for (int i = 0; i < 1000; ++i) {
    auto btn = std::make_unique<Button>();
    btn->set_text(std::format("按钮 {}", i));
    panel->add_child(std::move(btn));
}
```

**推荐模式:**
```cpp
// ✅ 推荐:使用 ItemsControl + 虚拟化
auto list = std::make_unique<ItemsControl>();
list->set_items_source(button_data_);  // std::vector<ButtonData>
list->enable_virtualization(true);     // 仅渲染可见项

list->set_item_template([](const Variant& item) {
    auto data = item.as<ButtonData>();
    auto btn = std::make_unique<Button>();
    btn->set_text(data.text);
    return btn;
});
```

---

### 6.4 自定义 Ripple 颜色以匹配背景

**原因:**
- 默认 Ripple 为白色半透明,在浅色背景上不明显
- MD3 规范要求 Ripple 颜色与背景对比度 > 3:1

**反模式:**
```cpp
// ❌ 不推荐:浅色背景 + 白色 Ripple(看不见)
button->set_background(SolidColorBrush(0xFFE0E0E0));  // 浅灰背景
// StateLayerBrush 默认白色,几乎不可见
```

**推荐模式:**
```cpp
// ✅ 推荐:根据背景亮度调整 Ripple 颜色
auto bg_color = 0xFFE0E0E0;
auto brightness = calculate_brightness(bg_color);

if (brightness > 0.5f) {
    // 浅色背景用深色 Ripple
    button->set_value(Button::StateLayerBrushProperty, 
                      Variant::from(SolidColorBrush(0x20000000)));  // 黑色 12%
} else {
    // 深色背景用浅色 Ripple
    button->set_value(Button::StateLayerBrushProperty, 
                      Variant::from(SolidColorBrush(0x20FFFFFF)));  // 白色 12%
}
```

**辅助函数:**
```cpp
float calculate_brightness(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    return (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
}
```

---

## 7. 常见陷阱

### 7.1 忘记检查 Command 的 CanExecute

**问题:** Command 绑定后,CanExecute 返回 false,但开发者通过 set_is_enabled(true) 强制启用按钮。

**错误代码:**
```cpp
button->set_command(view_model->submit_command());
button->set_is_enabled(true);  // ❌ 覆盖了 Command 的 CanExecute 逻辑
```

**后果:**
- 用户可以点击按钮,但 Command.Execute() 不会执行(因为内部检查 CanExecute)
- 导致按钮"点击无反应"的 Bug

**正确做法:**
```cpp
// ✅ 让 Command 自动控制 IsEnabled
button->set_command(view_model->submit_command());
// 不要手动设置 IsEnabled

// 如果需要临时禁用,应该在 Command 中修改逻辑
view_model->set_form_valid(false);  // Command.CanExecute() 返回 false
view_model->notify_can_execute_changed();  // 触发按钮更新
```

---

### 7.2 在析构函数中访问依赖属性

**问题:** Button 析构时,依赖属性系统可能已清理,访问导致崩溃。

**错误代码:**
```cpp
Button::~Button() {
    // ❌ 依赖属性可能已失效
    auto cmd = get_value<ICommand*>(CommandProperty);
    if (cmd) {
        // 取消订阅...
    }
}
```

**后果:**
- 访问空指针或已释放内存
- 程序崩溃

**正确做法:**
```cpp
// ✅ 在 on_unloaded() 虚函数中清理
void Button::on_unloaded() {
    auto cmd = get_value<ICommand*>(CommandProperty);
    if (cmd && impl_->command_token.is_valid()) {
        cmd->can_execute_changed.remove(impl_->command_token);
        impl_->command_token = {};
    }
    Control::on_unloaded();
}

Button::~Button() {
    // 析构函数仅释放独占资源(如 PImpl)
    // 不访问依赖属性
}
```

---

### 7.3 忽略 Ripple 起点导致动画错位

**问题:** 手动调用 execute_click() 触发点击,但未设置 Ripple 起点。

**错误代码:**
```cpp
// ❌ 直接调用 execute_click(),Ripple 起点为 (0, 0)
button->execute_click();
```

**后果:**
- Ripple 从左上角开始扩散,而非点击位置
- 视觉效果不自然

**正确做法:**
```cpp
// ✅ 通过 raise_event 触发,让控件自己处理
auto args = RoutedEventArgs(button);
button->raise_event(Button::ClickEvent(), args);

// 或者在调用前设置 Ripple 起点
button->impl_->ripple_origin = math::Point{button->width() / 2, button->height() / 2};
button->execute_click();
```

---

### 7.4 混淆 Content 和 Text 属性

**问题:** 使用 set_text() 后又调用 set_content(UIElement*),导致文本丢失。

**错误代码:**
```cpp
button->set_text("提交");  // Content = InlineString("提交")

auto icon = std::make_unique<Path>();
button->set_content(std::move(icon));  // ❌ Content 被覆盖为 Path*,文本丢失
```

**后果:**
- 按钮显示图标,但文本不见了
- 用户困惑

**正确做法:**
```cpp
// ✅ 方案 1:使用 StackPanel 组合文本和图标
auto panel = std::make_unique<StackPanel>();
panel->set_orientation(Orientation::Horizontal);
panel->set_spacing(8);

auto icon = std::make_unique<Path>();
icon->set_data("M12 2l3.09 6.26...");
panel->add_child(std::move(icon));

auto text = std::make_unique<TextBlock>();
text->set_text("提交");
panel->add_child(std::move(text));

button->set_content(std::move(panel));

// ✅ 方案 2:使用 ButtonBase 派生类支持 IconProperty
class IconButton : public Button {
    static const DependencyProperty& IconProperty;
};
```

---

## 8. 完整示例

### 8.1 完整的登录表单应用

**场景:** 用户名/密码登录,支持表单验证、异步登录、错误提示。

**ViewModel (LoginViewModel.h):**
```cpp
#pragma once
#include <mine/mvvm/ViewModelBase.h>
#include <mine/ui/ICommand.h>
#include <mine/async/Task.h>
#include <mine/net/HttpClient.h>
#include <string>
#include <memory>

namespace myapp {

class LoginViewModel : public mine::mvvm::ViewModelBase {
public:
    // ========== 命令定义 ==========
    
    class LoginCommand : public mine::ui::ICommand {
    public:
        explicit LoginCommand(LoginViewModel* vm) : vm_(vm) {}
        
        void execute(const mine::core::Variant& param) override {
            vm_->login_async();
        }
        
        bool can_execute(const mine::core::Variant& param) const override {
            return vm_->can_login();
        }
        
    private:
        LoginViewModel* vm_;
    };
    
    // ========== 构造函数 ==========
    
    LoginViewModel() : login_cmd_(this) {}
    
    // ========== 属性 ==========
    
    const std::string& username() const { return username_; }
    void set_username(std::string value) {
        if (username_ != value) {
            username_ = std::move(value);
            notify_property_changed("Username");
            login_cmd_.can_execute_changed.raise();
        }
    }
    
    const std::string& password() const { return password_; }
    void set_password(std::string value) {
        if (password_ != value) {
            password_ = std::move(value);
            notify_property_changed("Password");
            login_cmd_.can_execute_changed.raise();
        }
    }
    
    const std::string& error_message() const { return error_message_; }
    void set_error_message(std::string value) {
        if (error_message_ != value) {
            error_message_ = std::move(value);
            notify_property_changed("ErrorMessage");
        }
    }
    
    bool is_logging_in() const { return is_logging_in_; }
    void set_is_logging_in(bool value) {
        if (is_logging_in_ != value) {
            is_logging_in_ = value;
            notify_property_changed("IsLoggingIn");
            login_cmd_.can_execute_changed.raise();
        }
    }
    
    // ========== 命令访问器 ==========
    
    LoginCommand* login_command() { return &login_cmd_; }
    
private:
    // ========== 业务逻辑 ==========
    
    bool can_login() const {
        return !username_.empty() && 
               password_.size() >= 6 && 
               !is_logging_in_;
    }
    
    void login_async() {
        set_is_logging_in(true);
        set_error_message("");
        
        mine::async::post_task([this]() {
            // 异步网络请求
            mine::net::HttpClient client;
            auto response = client.post("https://api.example.com/login", {
                {"username", username_},
                {"password", password_}
            });
            
            // 回到 UI 线程更新状态
            mine::async::invoke_on_ui_thread([this, response]() {
                set_is_logging_in(false);
                
                if (response.status == 200) {
                    // 登录成功
                    on_login_success(response.body);
                } else {
                    // 登录失败
                    set_error_message("用户名或密码错误");
                }
            });
        });
    }
    
    void on_login_success(const std::string& token) {
        // 保存令牌,导航到主页
        app_->save_auth_token(token);
        app_->navigate_to("MainPage");
    }
    
    // ========== 私有成员 ==========
    
    std::string username_;
    std::string password_;
    std::string error_message_;
    bool is_logging_in_ = false;
    LoginCommand login_cmd_;
};

} // namespace myapp
```

**View (LoginPage.mml):**
```xml
<Page x:class="myapp::LoginPage"
      xmlns="http://mine-ui.org/mml"
      xmlns:x="http://mine-ui.org/mml/extensions">
    
    <Page.DataContext>
        <local:LoginViewModel />
    </Page.DataContext>
    
    <StackPanel width="400" height="500"
                horizontalAlignment="Center"
                verticalAlignment="Center"
                spacing="16"
                padding="32">
        
        <!-- 标题 -->
        <TextBlock text="登录" 
                   fontSize="32" 
                   fontWeight="Bold"
                   foreground="#6750A4"
                   horizontalAlignment="Center" />
        
        <!-- 用户名输入框 -->
        <TextBox placeholder="用户名"
                 text="{bind:vm.Username, mode:TwoWay}"
                 height="48" />
        
        <!-- 密码输入框 -->
        <TextBox placeholder="密码"
                 text="{bind:vm.Password, mode:TwoWay}"
                 isPassword="true"
                 height="48" />
        
        <!-- 错误提示 -->
        <TextBlock text="{bind:vm.ErrorMessage}"
                   foreground="#B00020"
                   fontSize="14"
                   visibility="{bind:vm.ErrorMessage, converter:EmptyToCollapsed}" />
        
        <!-- 登录按钮 -->
        <Button text="{bind:vm.IsLoggingIn, converter:LoginButtonText}"
                command="{bind:vm.LoginCommand}"
                height="48"
                horizontalAlignment="Stretch"
                background="#6750A4"
                foreground="#FFFFFF"
                cornerRadius="24" />
        
        <!-- 注册链接 -->
        <TextBlock horizontalAlignment="Center">
            <Run text="还没有账号? " />
            <Hyperlink text="立即注册" 
                       click="on_register_click"
                       foreground="#6750A4" />
        </TextBlock>
        
    </StackPanel>
    
</Page>
```

**View Code-Behind (LoginPage.cpp):**
```cpp
#include "LoginPage.h"
#include "LoginViewModel.h"

namespace myapp {

LoginPage::LoginPage() {
    // MML 解析器自动加载 LoginPage.mml 并设置 DataContext
}

void LoginPage::on_register_click(mine::ui::UIElement& sender, 
                                   mine::ui::RoutedEventArgs& args) {
    // 导航到注册页面
    auto frame = find_ancestor<mine::ui::Frame>();
    frame->navigate("RegisterPage");
}

} // namespace myapp
```

**值转换器 (Converters.cpp):**
```cpp
#include <mine/ui/binding/IValueConverter.h>

namespace myapp {

// 将 IsLoggingIn 转换为按钮文本
class LoginButtonTextConverter : public mine::ui::IValueConverter {
public:
    mine::core::Variant convert(const mine::core::Variant& value, 
                                 mine::core::Variant parameter) override {
        bool is_logging_in = value.as<bool>();
        return mine::core::Variant::from<mine::core::InlineString>(
            is_logging_in ? "登录中..." : "登录"
        );
    }
    
    mine::core::Variant convert_back(const mine::core::Variant& value, 
                                       mine::core::Variant parameter) override {
        // 单向绑定,不需要实现
        return {};
    }
};

// 将空字符串转换为 Collapsed 可见性
class EmptyToCollapsedConverter : public mine::ui::IValueConverter {
public:
    mine::core::Variant convert(const mine::core::Variant& value, 
                                 mine::core::Variant parameter) override {
        auto str = value.as<mine::core::InlineString>();
        return mine::core::Variant::from<mine::ui::Visibility>(
            str.empty() ? mine::ui::Visibility::Collapsed : mine::ui::Visibility::Visible
        );
    }
};

} // namespace myapp
```

**CMakeLists.txt:**
```cmake
add_executable(LoginApp
    main.cpp
    LoginPage.cpp
    LoginViewModel.cpp
    Converters.cpp
)

target_link_libraries(LoginApp PRIVATE
    mine::ui::controls
    mine::ui::binding
    mine::mvvm
    mine::async
    mine::net
    mine::mml
)

# 安装 MML 资源文件
install(FILES LoginPage.mml DESTINATION resources)
```

**main.cpp:**
```cpp
#include <mine/ui/app/Application.h>
#include "LoginPage.h"

int main(int argc, char* argv[]) {
    mine::ui::Application app(argc, argv);
    
    // 注册值转换器
    app.register_converter<myapp::LoginButtonTextConverter>("LoginButtonText");
    app.register_converter<myapp::EmptyToCollapsedConverter>("EmptyToCollapsed");
    
    // 创建主窗口
    auto window = std::make_unique<mine::ui::Window>();
    window->set_title("登录示例");
    window->set_width(500);
    window->set_height(600);
    
    // 创建 Frame 并导航到登录页
    auto frame = std::make_unique<mine::ui::Frame>();
    frame->navigate<myapp::LoginPage>();
    window->set_content(std::move(frame));
    
    // 显示窗口并运行消息循环
    window->show();
    return app.run();
}
```

**运行效果:**
1. 启动应用,显示登录页面
2. 输入用户名和密码,登录按钮实时启用/禁用(根据 CanExecute)
3. 点击登录按钮:
   - 按钮文本变为"登录中...",禁用状态
   - Ripple 动画播放
   - 异步网络请求
4. 登录失败:显示错误提示,按钮恢复"登录"文本
5. 登录成功:保存令牌,导航到主页

---

## 9. 总结

### 9.1 核心要点

- **Button 是 MD3 Filled Button 的完整实现**,支持四种视觉状态、Ripple 涟漪动画、State Layer 状态层动画。
- **继承自 ContentControl**,既可以显示文本,也可以承载任意 UIElement 内容(图标、自定义布局)。
- **命令绑定优于事件处理器**,命令模式自动处理 CanExecute 逻辑,按钮状态自动同步。
- **三层动画架构**:背景层 + 状态层(VSM 驱动) + 涟漪层(AnimationClock 驱动),在 `on_visual_state_changed()` 中统一注册。
- **依赖属性系统**:BackgroundProperty、ForegroundProperty、CommandProperty 等支持数据绑定和样式设置。

### 9.2 使用建议

- **MVVM 架构中优先使用 MML 声明式定义**,代码更简洁易维护。
- **自定义样式时注意 Ripple 颜色与背景对比度**,遵循 MD3 无障碍规范。
- **避免在循环中创建大量按钮**,使用 ItemsControl + 虚拟化优化性能。
- **异步操作时通过 Command.CanExecute 禁用按钮**,防止重复点击。

### 9.3 扩展方向

- **自定义 VSM 状态组**:添加 Focused、Checked 等新状态。
- **派生新按钮类型**:IconButton、ToggleButton、SplitButton。
- **实现其他 MD3 按钮变体**:Outlined Button、Text Button、Elevated Button。
- **集成无障碍功能**:AutomationPeer、键盘导航、高对比度模式。

### 9.4 相关文档

- [ContentControl 类 API 文档](01-ContentControl.md)
- [RoutedEvent 路由事件系统](../ui.event/01-RoutedEvent.md)
- [ICommand 命令接口](../mvvm/02-ICommand.md)
- [Visual State Manager 状态管理器](../ui.animation/03-VisualStateManager.md)
- [Material Design 3 Button 规范](https://m3.material.io/components/buttons)

---

**文档版本:** v0.1.0  
**最后更新:** 2026-06-11  
**维护者:** MineFramework Team
