# TextBox 类 API 文档

## 1. 概述

`TextBox` 是 MineFramework 中的单行/多行文本输入控件,提供完整的文本编辑功能,包括光标定位、选择、复制粘贴、撤销重做等。采用 Material Design 3 Filled Text Field 风格设计。

**核心职责：**

- **文本输入**：接收键盘输入,支持字符插入、删除、退格
- **光标管理**：光标定位、闪烁动画、键盘导航
- **文本选择**：鼠标拖拽选择、Shift+方向键选择
- **剪贴板操作**：Ctrl+C/X/V 复制/剪切/粘贴
- **视觉状态**：Normal/Hovered/Focused/Disabled 四种状态
- **占位文字**：未输入时显示提示文字

**继承关系：**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── TextBox  ← 本文档
```

**架构意义：**

1. **输入控件基类**：为其他输入控件(如 PasswordBox)提供基础
2. **焦点管理**：集成焦点系统,支持 Tab 键导航
3. **路由事件**：通过 TextChanged 事件通知文本变更
4. **MVVM 支持**：文本属性双向绑定,实时同步 ViewModel

**典型用途：**

- **表单输入**：用户名、密码、邮箱等表单字段
- **搜索框**：搜索关键词输入
- **多行编辑**：设置 `AcceptsReturn=true` 实现多行文本编辑
- **只读展示**：设置 `IsReadOnly=true` 禁用编辑

**设计原则：**

- **单一职责**：专注文本输入编辑,不包含验证逻辑(验证通过 Validation 系统)
- **可定制性**：通过 ControlTemplate 支持完全自定义外观
- **性能优先**：增量文本更新,避免全文重新整形
- **无障碍支持**：与屏幕阅读器集成,支持键盘导航

**性能特性：**

- **增量更新**：仅重新整形变更部分文本
- **虚拟化选择**：大文本场景下使用虚拟化渲染
- **延迟验证**：文本变更后延迟触发验证,避免频繁计算
- **光标闪烁优化**：使用定时器触发,最小化重绘区域

---

## 2. 文件位置

**头文件：**
```
src/mine/ui/controls/TextBox.h
```

**实现文件：**
```
src/mine/ui/controls/TextBox.cpp
```

**模块归属：**
```
mine.ui.controls
```

**命名空间：**
```cpp
namespace mine::ui
```

**依赖头文件：**
```cpp
#include <mine/ui/Control.h>
#include <mine/ui/DependencyProperty.h>
#include <mine/ui/RoutedEvent.h>
#include <mine/paint/Brush.h>
#include <mine/core/InlineString.h>
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// 单行/多行文本输入控件
/// 
/// 提供完整的文本编辑功能:
/// - 键盘输入、光标定位、文本选择
/// - 复制/剪切/粘贴、撤销/重做
/// - 视觉状态管理 (Normal/Hovered/Focused/Disabled)
/// - 占位文字、只读模式
/// 
/// Material Design 3 Filled Text Field 风格设计
class MINE_API TextBox : public Control {
    MINE_RTTI_DECLARE(TextBox, Control)

public:
    /// TextChanged 路由事件
    static const RoutedEvent& TextChangedEvent();

    /// Text 依赖属性 (InlineString 类型)
    static const DependencyProperty& TextProperty;

    /// Placeholder 依赖属性 (InlineString 类型)
    static const DependencyProperty& PlaceholderProperty;

    /// IsReadOnly 依赖属性 (bool 类型)
    static const DependencyProperty& IsReadOnlyProperty;

    /// AcceptsReturn 依赖属性 (bool 类型)
    static const DependencyProperty& AcceptsReturnProperty;

    /// TextWrapping 依赖属性 (TextWrapping 类型)
    static const DependencyProperty& TextWrappingProperty;

    /// Background 依赖属性 (Brush 类型)
    static const DependencyProperty& BackgroundProperty;

    /// Foreground 依赖属性 (Brush 类型)
    static const DependencyProperty& ForegroundProperty;

    /// BorderColor 依赖属性 (Brush 类型)
    static const DependencyProperty& BorderColorProperty;

    /// PlaceholderForeground 依赖属性 (Brush 类型)
    static const DependencyProperty& PlaceholderForegroundProperty;

    /// Padding 依赖属性 (Thickness 类型)
    static const DependencyProperty& PaddingProperty;

    /// CornerRadius 依赖属性 (CornerRadii 类型)
    static const DependencyProperty& CornerRadiusProperty;

    /// 构造函数
    TextBox();

    /// 析构函数
    ~TextBox() override;

    // ==================== 文本内容 ====================

    [[nodiscard]] core::StringView text() const noexcept;
    void set_text(core::StringView text);

    [[nodiscard]] core::StringView placeholder() const noexcept;
    void set_placeholder(core::StringView text);

    // ==================== 编辑模式 ====================

    [[nodiscard]] bool is_read_only() const noexcept;
    void set_read_only(bool readonly);

    [[nodiscard]] bool accepts_return() const noexcept;
    void set_accepts_return(bool accepts);

    // ==================== 选择范围 ====================

    [[nodiscard]] int selection_start() const noexcept;
    [[nodiscard]] int selection_length() const noexcept;

    void select(int start, int length);
    void select_all();
    void clear_selection();

    // ==================== 编辑操作 ====================

    void copy();
    void cut();
    void paste();
    void undo();
    void redo();

protected:
    void on_measure(math::Size available_size) override;
    void on_arrange(math::Rect final_rect) override;
    void on_render(paint::Canvas& canvas) override;

    // 输入事件处理
    void on_key_down(KeyEventArgs& e) override;
    void on_text_input(TextInputEventArgs& e) override;
    void on_mouse_down(MouseButtonEventArgs& e) override;
    void on_mouse_move(MouseEventArgs& e) override;
    void on_mouse_up(MouseButtonEventArgs& e) override;

    // 焦点事件处理
    void on_got_focus(RoutedEventArgs& e) override;
    void on_lost_focus(RoutedEventArgs& e) override;

private:
    core::InlineString text_content_;
    int cursor_position_ = 0;
    int selection_start_ = 0;
    int selection_length_ = 0;
    bool cursor_visible_ = true;
    
    void start_cursor_blink();
    void stop_cursor_blink();
    void insert_text_at_cursor(core::StringView text);
    void delete_selected_text();
};

} // namespace mine::ui
```

---

## 4. 成员方法

### 4.1 静态路由事件

#### TextChangedEvent()

```cpp
static const RoutedEvent& TextChangedEvent();
```

**描述：** 文本内容变更路由事件。

**策略：** Bubble(冒泡)

**示例：**
```cpp
text_box->add_handler(TextBox::TextChangedEvent(), [](RoutedEventArgs& e) {
    auto* box = dynamic_cast<TextBox*>(e.source());
    std::cout << "新文本: " << box->text() << std::endl;
});
```

---

### 4.2 静态依赖属性

#### TextProperty

```cpp
static const DependencyProperty& TextProperty;
```

**描述：** 文本内容依赖属性。

**类型：** `core::InlineString`

**默认值：** 空字符串

**绑定模式：** 支持双向绑定 (`TwoWay`)

**示例：**
```cpp
// 双向绑定到 ViewModel
text_box->bind_property(
    TextBox::TextProperty,
    view_model,
    "UserName",
    BindingMode::TwoWay  // 用户输入时自动更新 ViewModel
);
```

---

#### PlaceholderProperty

```cpp
static const DependencyProperty& PlaceholderProperty;
```

**描述：** 占位文字依赖属性。

**类型：** `core::InlineString`

**默认值：** 空字符串

**说明：** 当文本为空且未获得焦点时显示

---

#### IsReadOnlyProperty

```cpp
static const DependencyProperty& IsReadOnlyProperty;
```

**描述：** 只读模式依赖属性。

**类型：** `bool`

**默认值：** `false`

**说明：** 设置为 `true` 时禁用编辑,但仍可选择和复制

---

#### AcceptsReturnProperty

```cpp
static const DependencyProperty& AcceptsReturnProperty;
```

**描述：** 是否接受回车键依赖属性。

**类型：** `bool`

**默认值：** `false`

**说明：** `true` 时按回车插入换行符,`false` 时触发默认按钮点击

---

### 4.3 文本内容

#### text()

```cpp
[[nodiscard]] core::StringView text() const noexcept;
```

**描述：** 获取文本内容。

---

#### set_text()

```cpp
void set_text(core::StringView text);
```

**描述：** 设置文本内容。

**行为：**
- 更新 `TextProperty`
- 触发 `TextChangedEvent`
- 重置光标位置到末尾

---

#### placeholder()

```cpp
[[nodiscard]] core::StringView placeholder() const noexcept;
```

**描述：** 获取占位文字。

---

#### set_placeholder()

```cpp
void set_placeholder(core::StringView text);
```

**描述：** 设置占位文字。

**示例：**
```cpp
text_box->set_placeholder("请输入用户名");
```

---

### 4.4 编辑模式

#### is_read_only()

```cpp
[[nodiscard]] bool is_read_only() const noexcept;
```

**描述：** 获取是否只读模式。

---

#### set_read_only()

```cpp
void set_read_only(bool readonly);
```

**描述：** 设置只读模式。

**示例：**
```cpp
// 只读模式(可选择,不可编辑)
text_box->set_read_only(true);
```

---

#### accepts_return()

```cpp
[[nodiscard]] bool accepts_return() const noexcept;
```

**描述：** 获取是否接受回车键。

---

#### set_accepts_return()

```cpp
void set_accepts_return(bool accepts);
```

**描述：** 设置是否接受回车键。

**示例：**
```cpp
// 多行文本输入
text_box->set_accepts_return(true);
text_box->set_text_wrapping(TextWrapping::Wrap);
text_box->set_height(100.0f);
```

---

### 4.5 选择范围

#### selection_start()

```cpp
[[nodiscard]] int selection_start() const noexcept;
```

**描述：** 获取选择起始位置(字符索引)。

---

#### selection_length()

```cpp
[[nodiscard]] int selection_length() const noexcept;
```

**描述：** 获取选择长度(字符数)。

---

#### select()

```cpp
void select(int start, int length);
```

**描述：** 设置选择范围。

**参数：**
- `start`：起始位置(0-based)
- `length`：选择长度

**示例：**
```cpp
// 选择前5个字符
text_box->select(0, 5);
```

---

#### select_all()

```cpp
void select_all();
```

**描述：** 全选文本。

**示例：**
```cpp
// 焦点时全选
text_box->add_handler(GotFocusEvent(), [](RoutedEventArgs& e) {
    auto* box = dynamic_cast<TextBox*>(e.source());
    box->select_all();
});
```

---

#### clear_selection()

```cpp
void clear_selection();
```

**描述：** 清除选择。

---

### 4.6 编辑操作

#### copy()

```cpp
void copy();
```

**描述：** 复制选中文本到剪贴板。

---

#### cut()

```cpp
void cut();
```

**描述：** 剪切选中文本到剪贴板。

---

#### paste()

```cpp
void paste();
```

**描述：** 从剪贴板粘贴文本。

---

#### undo()

```cpp
void undo();
```

**描述：** 撤销上一步编辑操作。

---

#### redo()

```cpp
void redo();
```

**描述：** 重做上一步撤销的操作。

---

### 4.7 输入事件处理

#### on_key_down()

```cpp
void on_key_down(KeyEventArgs& e) override;
```

**描述：** 处理键盘按键事件。

**处理的按键：**
- `Backspace`：删除光标前字符
- `Delete`：删除光标后字符
- `Left/Right/Up/Down`：移动光标
- `Home/End`：光标移到行首/行尾
- `Ctrl+A`：全选
- `Ctrl+C/X/V`：复制/剪切/粘贴
- `Ctrl+Z/Y`：撤销/重做

---

#### on_text_input()

```cpp
void on_text_input(TextInputEventArgs& e) override;
```

**描述：** 处理文本输入事件。

**行为：**
- 在光标位置插入字符
- 删除选中文本(如果有)
- 触发 `TextChangedEvent`

---

#### on_mouse_down()

```cpp
void on_mouse_down(MouseButtonEventArgs& e) override;
```

**描述：** 处理鼠标按下事件。

**行为：**
- 点击定位光标
- 开始文本选择(拖拽)

---

#### on_mouse_move()

```cpp
void on_mouse_move(MouseEventArgs& e) override;
```

**描述：** 处理鼠标移动事件。

**行为：**
- 拖拽选择文本

---

#### on_mouse_up()

```cpp
void on_mouse_up(MouseButtonEventArgs& e) override;
```

**描述：** 处理鼠标释放事件。

**行为：**
- 结束文本选择

---

#### on_got_focus()

```cpp
void on_got_focus(RoutedEventArgs& e) override;
```

**描述：** 获得焦点时调用。

**行为：**
- 启动光标闪烁定时器
- 切换到 Focused 视觉状态

---

#### on_lost_focus()

```cpp
void on_lost_focus(RoutedEventArgs& e) override;
```

**描述：** 失去焦点时调用。

**行为：**
- 停止光标闪烁定时器
- 清除选择
- 切换到 Normal 视觉状态

---

## 5. 使用场景

### 5.1 基础单行输入

```cpp
auto text_box = core::make_owned<TextBox>();
text_box->set_placeholder("请输入文本");
text_box->set_width(200.0f);
```

### 5.2 密码输入

```cpp
auto password_box = core::make_owned<PasswordBox>();  // TextBox 派生类
password_box->set_placeholder("请输入密码");
password_box->set_password_char('•');
```

### 5.3 多行文本编辑

```cpp
auto editor = core::make_owned<TextBox>();
editor->set_accepts_return(true);
editor->set_text_wrapping(TextWrapping::Wrap);
editor->set_width(400.0f);
editor->set_height(200.0f);
```

### 5.4 搜索框

```cpp
auto search_box = core::make_owned<TextBox>();
search_box->set_placeholder("搜索...");

// 实时搜索
search_box->add_handler(TextBox::TextChangedEvent(), [](RoutedEventArgs& e) {
    auto* box = dynamic_cast<TextBox*>(e.source());
    perform_search(box->text());
});
```

### 5.5 双向数据绑定

```cpp
// ViewModel
class LoginViewModel : public mvvm::ViewModelBase {
public:
    MINE_PROPERTY(core::InlineString, user_name, "")
    MINE_PROPERTY(core::InlineString, password, "")
};

// View
auto user_name_box = core::make_owned<TextBox>();
user_name_box->bind_property(
    TextBox::TextProperty,
    view_model,
    "user_name",
    BindingMode::TwoWay
);
// 用户输入时 ViewModel 自动更新
```

### 5.6 只读文本展示

```cpp
auto display_box = core::make_owned<TextBox>();
display_box->set_text("只读内容");
display_box->set_read_only(true);
display_box->set_background(Brush::from_color(Color{0xF5, 0xF5, 0xF5}));
```

---

## 6. 最佳实践

1. **设置占位文字**：提升用户体验,明确输入内容
2. **双向绑定**：MVVM 场景下使用 `TwoWay` 模式
3. **验证输入**：通过 `TextChanged` 事件实时验证
4. **只读模式**：展示场景使用 `IsReadOnly=true`
5. **多行编辑**：设置 `AcceptsReturn=true` + `TextWrapping=Wrap`

---

## 7. 常见陷阱

### 7.1 忘记设置 AcceptsReturn 导致回车无效

**问题：**
```cpp
auto text_box = core::make_owned<TextBox>();
text_box->set_height(100.0f);
// 按回车键无反应,无法输入多行文本!
```

**解决：**
```cpp
text_box->set_accepts_return(true);
```

### 7.2 只读模式下仍可通过代码修改文本

**说明：** `IsReadOnly` 仅禁用用户输入,代码仍可调用 `set_text()`

---

## 8. 完整示例

### 8.1 登录表单

```cpp
auto create_login_form() -> core::OwnedPtr<StackPanel> {
    auto panel = core::make_owned<StackPanel>();
    panel->set_padding(Thickness{24});
    panel->set_spacing(16.0f);
    panel->set_width(300.0f);
    
    // 用户名
    auto user_label = core::make_owned<TextBlock>();
    user_label->set_text("用户名");
    panel->add_child(user_label.release());
    
    auto user_box = core::make_owned<TextBox>();
    user_box->set_placeholder("请输入用户名");
    panel->add_child(user_box.release());
    
    // 密码
    auto pwd_label = core::make_owned<TextBlock>();
    pwd_label->set_text("密码");
    panel->add_child(pwd_label.release());
    
    auto pwd_box = core::make_owned<PasswordBox>();
    pwd_box->set_placeholder("请输入密码");
    panel->add_child(pwd_box.release());
    
    // 登录按钮
    auto login_btn = core::make_owned<Button>();
    login_btn->set_content("登录");
    login_btn->set_horizontal_alignment(HorizontalAlignment::Stretch);
    login_btn->add_handler(Button::ClickEvent(), [=](RoutedEventArgs& e) {
        auto user = user_box->text();
        auto pwd = pwd_box->text();
        // 执行登录...
    });
    panel->add_child(login_btn.release());
    
    return panel;
}
```

---

## 9. 总结

- **TextBox** 是单行/多行文本输入控件
- 支持光标、选择、复制粘贴、撤销重做
- 提供 Normal/Hovered/Focused/Disabled 视觉状态
- 集成焦点系统,支持 Tab 键导航
- 文本属性支持双向绑定,实时同步 ViewModel

---

**文档版本:** 1.0  
**最后更新:** 2025-01-10  
**作者:** MineUI 开发团队  
**许可证:** MIT License
