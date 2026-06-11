# ContentControl 类 API 文档

## 1. 概述

`ContentControl` 是 MineFramework UI 控件层次结构中的关键基类,位于 `Control` 和具体内容控件(如 `Button`)之间。它引入了"内容"(Content)概念,允许控件承载任意类型的内容——既可以是简单的文本字符串,也可以是复杂的 `UIElement` 子树。

**核心职责：**

- **内容承载**：通过 `ContentProperty` 依赖属性存储 `core::Variant`,支持 `InlineString` 或 `UIElement*` 两种类型
- **类型切换**：提供类型安全的访问器 `content_text()` 和 `content_element()`,自动处理 Variant 类型判断
- **变更通知**：虚方法 `on_content_changed()` 允许派生类响应内容变化,实现自定义逻辑(如重新测量布局或触发动画)
- **统一接口**：为所有单内容控件(Button、CheckBox、RadioButton 等)提供一致的内容管理模式

**继承关系：**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── ContentControl  ← 本文档
                                            ├── Button
                                            ├── CheckBox
                                            ├── RadioButton
                                            ├── ToggleButton
                                            └── ...
```

**架构意义：**

1. **模板化支持**：ContentControl 与 `ContentPresenter` 协同工作,ControlTemplate 视觉树中的 ContentPresenter 自动绑定到 ContentControl.ContentProperty,实现内容与外观分离
2. **数据绑定友好**：Content 作为依赖属性,完美支持 MVVM 数据绑定,可直接绑定到 ViewModel 的字符串属性或视图模型对象
3. **扩展性基础**：派生类只需重写 `on_content_changed()` 即可添加内容相关的行为(如 Button 在内容为 Icon 时调整 Padding)

**典型用途：**

- 作为 Button、MenuItem 等控件的基类,提供内容承载能力
- 在自定义控件中快速实现"单内容容器"语义
- 结合 ControlTemplate 实现外观与内容的完全分离

**设计原则：**

- **Variant 透明化**：用户无需直接操作 `core::Variant`,通过类型化的 setter/getter 自动处理类型转换
- **生命周期管理**：当内容为 `UIElement*` 时,ContentControl 负责将其加入视觉树并管理其测量/布局/渲染周期
- **向后兼容**：保持与 WPF ContentControl 相似的 API 设计,降低移植成本

**性能特性：**

- **零拷贝字符串**：文本内容使用 `InlineString`(SSO 优化),小字符串(<23 字节)无堆分配
- **延迟布局**：仅在内容变更时标记为 InvalidateMeasure,避免不必要的重新测量
- **指针存储**：UIElement 内容以原始指针存储在 Variant 中,无引用计数开销(由视觉树父子关系管理生命周期)

**相关类型：**

- `ContentPresenter`：ControlTemplate 中的内容占位符,自动渲染 ContentControl.Content
- `ItemsControl`：多内容控件基类,扩展了 ContentControl 的单内容模型
- `DependencyProperty`：依赖属性系统,ContentProperty 基于此实现

---

## 2. 文件位置

**头文件：**
```
src/mine/ui/controls/ContentControl.h
```

**实现文件：**
```
src/mine/ui/controls/ContentControl.cpp
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
#include <mine/core/Variant.h>
#include <mine/core/StringView.h>
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// 单内容控件基类
/// 
/// 提供承载单个子元素或文本字符串的能力。Content 属性存储为 Variant,
/// 支持两种类型：
/// - core::InlineString: 纯文本内容
/// - UIElement*: 任意可视化元素
/// 
/// 派生类通过重写 on_content_changed() 响应内容变更。
class MINE_API ContentControl : public Control {
    MINE_RTTI_DECLARE(ContentControl, Control)

public:
    /// Content 依赖属性 (Variant 类型)
    /// 默认值: 空 Variant
    /// 继承性: 否
    /// 动画性: 否 (内容切换不支持平滑动画,但可通过 VSM 切换过渡动画)
    static const DependencyProperty& ContentProperty;

    /// 构造函数
    ContentControl();

    /// 析构函数
    ~ContentControl() override;

    // ==================== 内容设置接口 ====================

    /// 设置内容为 UIElement
    /// @param element 要显示的视觉元素,可为 nullptr 清空内容
    /// @note 调用后会将旧内容从视觉树移除,新内容加入视觉树
    void set_content(UIElement* element);

    /// 设置内容为文本字符串
    /// @param text 要显示的文本,使用 InlineString 存储
    /// @note 空字符串视为有效内容,不同于 nullptr 语义
    void set_content(core::StringView text);

    // ==================== 内容访问接口 ====================

    /// 获取原始内容 Variant
    /// @return 存储 InlineString 或 UIElement* 的 Variant 引用
    [[nodiscard]] const core::Variant& content() const noexcept;

    /// 获取内容元素 (类型安全访问器)
    /// @return 如果内容为 UIElement* 返回指针,否则返回 nullptr
    [[nodiscard]] UIElement* content_element() const noexcept;

    /// 获取内容文本 (类型安全访问器)
    /// @return 如果内容为 InlineString 返回字符串视图,否则返回空视图
    [[nodiscard]] core::StringView content_text() const noexcept;

protected:
    // ==================== 虚方法扩展点 ====================

    /// 内容变更回调
    /// @param old_v 旧内容 Variant
    /// @param new_v 新内容 Variant
    /// @note 派生类可重写此方法响应内容变化,例如:
    ///       - Button 根据内容类型调整内边距
    ///       - ContentPresenter 切换文本渲染模式
    ///       - 自定义控件触发内容变更动画
    virtual void on_content_changed(
        const core::Variant& old_v,
        const core::Variant& new_v
    ) noexcept;

private:
    // 内部实现细节(隐藏在 .cpp 中)
    void add_content_to_visual_tree(UIElement* element);
    void remove_content_from_visual_tree(UIElement* element);
};

} // namespace mine::ui
```

### 3.2 RTTI 宏说明

```cpp
MINE_RTTI_DECLARE(ContentControl, Control)
```

此宏展开后生成运行时类型信息:
- `TypeInfo()` 静态方法返回类型元数据
- `GetType()` 虚方法返回当前实例类型
- 支持 `dynamic_cast` 等效的类型转换

### 3.3 导出标记

```cpp
class MINE_API ContentControl
```

`MINE_API` 宏在 Windows 上展开为 `__declspec(dllexport/dllimport)`,在其他平台使用 `__attribute__((visibility("default")))`,确保符号正确导出到动态库。

### 3.4 关键类型定义

```cpp
namespace mine::core {
    // Variant 可存储多种类型的联合体容器
    class Variant {
    public:
        template<typename T>
        bool is() const noexcept;

        template<typename T>
        const T& as() const noexcept;
    };

    // 小字符串优化的字符串类 (SSO: Small String Optimization)
    class InlineString {
        // < 23 字节直接存储在栈上
        // >= 23 字节动态分配堆内存
    };

    // 非所有权字符串视图
    class StringView {
        const char* data_;
        size_t length_;
    };
}
```

---

## 4. 成员方法

### 4.1 静态依赖属性

#### ContentProperty

```cpp
static const DependencyProperty& ContentProperty;
```

**描述：** 存储控件内容的依赖属性,类型为 `core::Variant`。

**注册信息：**
- 属性名: `"Content"`
- 所有者类型: `ContentControl`
- 值类型: `core::Variant`
- 默认值: 空 Variant (`Variant{}`)
- 元数据标志: `PropertyMetadata::AffectsMeasure | PropertyMetadata::AffectsRender`

**变更回调：** 当属性值变化时,依赖属性系统自动调用 `on_content_changed()`。

**绑定示例：**
```cpp
// MML 单向绑定
text: bind(vm.UserName);

// C++ 绑定
button->bind_property(
    ContentControl::ContentProperty,
    view_model,
    "UserName"
);
```

---

### 4.2 构造函数

#### ContentControl()

```cpp
ContentControl();
```

**描述：** 默认构造函数,初始化基类 `Control` 并将 `ContentProperty` 设置为空 Variant。

**后置条件：**
- `content().is<core::InlineString>() == false`
- `content().is<UIElement*>() == false`
- `content_element() == nullptr`
- `content_text().empty() == true`

**使用示例：**
```cpp
auto content_ctrl = core::make_owned<ContentControl>();
MINE_ASSERT(content_ctrl->content_element() == nullptr);
```

---

### 4.3 析构函数

#### ~ContentControl()

```cpp
~ContentControl() override;
```

**描述：** 虚析构函数,清理内容并从视觉树移除子元素。

**行为：**
1. 如果 `content_element() != nullptr`,调用 `remove_content_from_visual_tree()`
2. 释放可能持有的资源(通常由基类 `Control` 处理)

**异常安全性：** `noexcept`,保证析构过程不抛出异常。

---

### 4.4 内容设置方法

#### set_content(UIElement*)

```cpp
void set_content(UIElement* element);
```

**描述：** 将内容设置为可视化元素。

**参数：**
- `element`: 要显示的 UIElement 指针,可为 `nullptr` 表示清空内容

**行为：**
1. 创建包含 `element` 指针的 `Variant`
2. 调用 `SetValue(ContentProperty, variant)`
3. 触发 `on_content_changed()` 回调
4. 如果旧内容为 UIElement,从视觉树移除
5. 如果新内容非 nullptr,加入视觉树并设置逻辑父级

**生命周期：** ContentControl 不拥有 `element` 的所有权,但管理其在视觉树中的父子关系。元素的实际生命周期由外部 `OwnedPtr` 持有者管理。

**示例：**
```cpp
auto button = core::make_owned<Button>();
auto icon = core::make_owned<Image>();
icon->set_source("assets/icon.png");

button->set_content(icon.get());
// icon 现在是 button 的视觉子元素
```

---

#### set_content(core::StringView)

```cpp
void set_content(core::StringView text);
```

**描述：** 将内容设置为文本字符串。

**参数：**
- `text`: 要显示的文本,内部转换为 `InlineString` 存储

**行为：**
1. 从 `text` 构造 `InlineString`
2. 创建包含该字符串的 `Variant`
3. 调用 `SetValue(ContentProperty, variant)`
4. 触发 `on_content_changed()` 回调
5. 如果旧内容为 UIElement,从视觉树移除

**性能：** 小字符串(<23 字节)无堆分配,使用栈上 SSO 缓冲区。

**示例：**
```cpp
auto button = core::make_owned<Button>();
button->set_content("点击我");
// 内容存储为 InlineString,无需额外 TextBlock
```

---

### 4.5 内容访问方法

#### content()

```cpp
[[nodiscard]] const core::Variant& content() const noexcept;
```

**描述：** 获取原始内容 Variant,供高级用户直接操作。

**返回值：** 对 `ContentProperty` 当前值的常量引用。

**使用场景：**
- 检查内容是否为特定类型: `content().is<InlineString>()`
- 序列化/反序列化控件状态
- 实现自定义内容转换逻辑

**示例：**
```cpp
const auto& var = button->content();
if (var.is<UIElement*>()) {
    auto* elem = var.as<UIElement*>();
    // 处理元素内容
} else if (var.is<core::InlineString>()) {
    auto text = var.as<core::InlineString>();
    // 处理文本内容
}
```

---

#### content_element()

```cpp
[[nodiscard]] UIElement* content_element() const noexcept;
```

**描述：** 类型安全地获取元素内容,如果内容不是 UIElement 返回 `nullptr`。

**返回值：**
- 内容为 `UIElement*` 时: 返回该指针
- 内容为 `InlineString` 或空 Variant 时: 返回 `nullptr`

**示例：**
```cpp
if (auto* elem = button->content_element()) {
    // 确定内容是 UIElement,可安全使用
    elem->set_opacity(0.5f);
}
```

---

#### content_text()

```cpp
[[nodiscard]] core::StringView content_text() const noexcept;
```

**描述：** 类型安全地获取文本内容,如果内容不是字符串返回空视图。

**返回值：**
- 内容为 `InlineString` 时: 返回字符串视图
- 内容为 `UIElement*` 或空 Variant 时: 返回空 `StringView` (`length() == 0`)

**示例：**
```cpp
auto text = button->content_text();
if (!text.empty()) {
    // 确定内容是文本,可安全使用
    std::cout << "按钮文本: " << text << std::endl;
}
```

---

### 4.6 虚方法扩展点

#### on_content_changed()

```cpp
virtual void on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept;
```

**描述：** 内容变更时的虚回调,派生类可重写以实现自定义逻辑。

**参数：**
- `old_v`: 变更前的内容 Variant
- `new_v`: 变更后的内容 Variant

**调用时机：** 在 `ContentProperty` 的属性变更回调中被调用,此时新值已设置但视觉树尚未更新。

**基类行为：** 空实现,派生类可选择性重写。

**典型重写场景：**

1. **Button 根据内容类型调整布局：**
```cpp
void Button::on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept {
    // 调用基类
    ContentControl::on_content_changed(old_v, new_v);

    // 如果内容是图标,减少 Padding
    if (new_v.is<UIElement*>()) {
        auto* elem = new_v.as<UIElement*>();
        if (auto* img = dynamic_cast<Image*>(elem)) {
            set_padding(Thickness{4, 4, 4, 4});
            return;
        }
    }

    // 文本内容使用标准 Padding
    set_padding(Thickness{16, 8, 16, 8});
}
```

2. **自定义控件触发内容变更动画：**
```cpp
void AnimatedButton::on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept {
    ContentControl::on_content_changed(old_v, new_v);

    // 淡出旧内容,淡入新内容
    if (auto* elem = content_element()) {
        elem->set_opacity(0.0f);
        elem->animate_opacity(1.0f, 300ms);
    }
}
```

**注意事项：**
- 必须调用基类实现(除非明确知道基类为空实现)
- 标记为 `noexcept`,不能抛出异常
- 避免在此方法中再次调用 `set_content()`,可能导致无限递归

---

## 5. 使用场景

### 5.1 基础文本按钮

最简单的用法是将 Button 的内容设置为字符串:

```cpp
#include <mine/ui/controls/Button.h>

void create_text_button() {
    auto button = core::make_owned<Button>();
    button->set_content("确认");
    button->set_width(120.0f);
    button->set_height(40.0f);
    
    // 点击事件
    button->add_handler(Button::ClickEvent(), [](RoutedEventArgs& e) {
        // 处理点击
    });
    
    // 添加到窗口
    window->set_content(button.release());
}
```

**MML 等效代码：**
```xml
<Button width="120" height="40" on_click="on_confirm_clicked">
    确认
</Button>
```

---

### 5.2 图标按钮

将按钮内容设置为 Image 元素:

```cpp
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Image.h>

void create_icon_button() {
    auto button = core::make_owned<Button>();
    
    // 创建图标
    auto icon = core::make_owned<Image>();
    icon->set_source("assets/icons/save.svg");
    icon->set_width(24.0f);
    icon->set_height(24.0f);
    
    // 设置为按钮内容
    button->set_content(icon.get());
    button->set_width(48.0f);
    button->set_height(48.0f);
    button->set_background(Brush::transparent());
    
    // 释放所有权给按钮
    icon.release();
    window->set_content(button.release());
}
```

**MML 等效代码：**
```xml
<Button width="48" height="48" background="transparent">
    <Image source="assets/icons/save.svg" width="24" height="24" />
</Button>
```

---

### 5.3 图标+文本组合按钮

使用 StackPanel 组合图标和文本:

```cpp
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Image.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/TextBlock.h>

void create_icon_text_button() {
    auto button = core::make_owned<Button>();
    
    // 创建水平布局容器
    auto panel = core::make_owned<StackPanel>();
    panel->set_orientation(Orientation::Horizontal);
    panel->set_spacing(8.0f);
    
    // 添加图标
    auto icon = core::make_owned<Image>();
    icon->set_source("assets/icons/download.svg");
    icon->set_width(20.0f);
    icon->set_height(20.0f);
    panel->add_child(icon.release());
    
    // 添加文本
    auto text = core::make_owned<TextBlock>();
    text->set_text("下载");
    text->set_vertical_alignment(VerticalAlignment::Center);
    panel->add_child(text.release());
    
    // 设置为按钮内容
    button->set_content(panel.get());
    button->set_padding(Thickness{12, 6, 12, 6});
    
    panel.release();
    window->set_content(button.release());
}
```

**MML 等效代码：**
```xml
<Button padding="12,6">
    <StackPanel orientation="Horizontal" spacing="8">
        <Image source="assets/icons/download.svg" width="20" height="20" />
        <TextBlock text="下载" vertical_alignment="Center" />
    </StackPanel>
</Button>
```

---

### 5.4 动态切换内容

根据状态在文本和加载动画之间切换:

```cpp
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/ProgressRing.h>

class SubmitButton : public Button {
public:
    void set_loading(bool loading) {
        if (loading) {
            // 保存原始文本
            original_text_ = content_text();
            
            // 切换为加载动画
            auto ring = core::make_owned<ProgressRing>();
            ring->set_is_active(true);
            ring->set_width(20.0f);
            ring->set_height(20.0f);
            set_content(ring.release());
            set_enabled(false);
        } else {
            // 恢复原始文本
            set_content(original_text_);
            set_enabled(true);
        }
    }

private:
    core::InlineString original_text_;
};

// 使用
void submit_form() {
    submit_button_->set_loading(true);
    
    // 异步提交表单
    async::post([this] {
        // ... 网络请求 ...
        
        // 完成后恢复按钮
        dispatch_to_ui_thread([this] {
            submit_button_->set_loading(false);
        });
    });
}
```

---

### 5.5 内容数据绑定 (MVVM)

将按钮内容绑定到 ViewModel 属性:

**ViewModel:**
```cpp
class ToolbarViewModel : public mvvm::ViewModelBase {
public:
    MINE_PROPERTY(core::InlineString, save_button_text, "保存")
    
    void on_document_modified() {
        // 文档修改后更新按钮文本
        set_save_button_text("保存*");
    }
    
    void on_document_saved() {
        // 保存后恢复文本
        set_save_button_text("保存");
    }
};
```

**View (C++):**
```cpp
void setup_toolbar(ToolbarViewModel* vm) {
    auto save_button = core::make_owned<Button>();
    
    // 绑定内容到 ViewModel.save_button_text
    save_button->bind_property(
        ContentControl::ContentProperty,
        vm,
        "save_button_text",
        BindingMode::OneWay
    );
    
    toolbar->add_child(save_button.release());
}
```

**View (MML):**
```xml
<Button content="{bind vm.save_button_text}" on_click="on_save" />
```

**效果：** ViewModel 属性变化时,按钮文本自动更新,无需手动调用 `set_content()`。

---

### 5.6 自定义 ContentControl 派生类

实现一个带过渡动画的内容控件:

```cpp
#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/animation/Storyboard.h>

class AnimatedContentControl : public ContentControl {
    MINE_RTTI_DECLARE(AnimatedContentControl, ContentControl)

public:
    AnimatedContentControl() = default;

protected:
    void on_content_changed(
        const core::Variant& old_v,
        const core::Variant& new_v
    ) noexcept override {
        ContentControl::on_content_changed(old_v, new_v);
        
        // 获取旧/新元素
        UIElement* old_elem = old_v.is<UIElement*>() 
            ? old_v.as<UIElement*>() 
            : nullptr;
        UIElement* new_elem = content_element();
        
        // 淡出旧元素
        if (old_elem) {
            auto fadeout = animation::Storyboard::create()
                .animate_double(old_elem, "Opacity", 1.0, 0.0, 200ms)
                .on_completed([old_elem] {
                    old_elem->set_visibility(Visibility::Collapsed);
                })
                .build();
            fadeout->begin();
        }
        
        // 淡入新元素
        if (new_elem) {
            new_elem->set_opacity(0.0);
            new_elem->set_visibility(Visibility::Visible);
            
            auto fadein = animation::Storyboard::create()
                .animate_double(new_elem, "Opacity", 0.0, 1.0, 200ms)
                .build();
            fadein->begin();
        }
    }
};

// 使用
auto animated_ctrl = core::make_owned<AnimatedContentControl>();
animated_ctrl->set_content("第一页内容");

// 1 秒后切换内容,自动播放淡入淡出动画
timer->schedule(1000ms, [=] {
    animated_ctrl->set_content("第二页内容");
});
```

---

### 5.7 ContentPresenter 模板绑定

在 ControlTemplate 中使用 ContentPresenter 展示 ContentControl 的内容:

**自定义按钮模板:**
```cpp
// 创建 ControlTemplate
auto template_root = core::make_owned<Border>();
template_root->set_background(Brush::from_color(Colors::Blue));
template_root->set_corner_radius(CornerRadii{4, 4, 4, 4});
template_root->set_padding(Thickness{16, 8, 16, 8});

// 添加 ContentPresenter 作为内容槽
auto presenter = core::make_owned<ContentPresenter>();
// presenter.Content 自动绑定到 TemplatedParent.Content
template_root->set_child(presenter.release());

// 应用模板
auto button = core::make_owned<Button>();
button->set_template(ControlTemplate::from_root(template_root.release()));
button->set_content("自定义样式按钮");
```

**MML 等效模板:**
```xml
<ControlTemplate target_type="Button">
    <Border background="#0078D4" corner_radius="4" padding="16,8">
        <ContentPresenter />
        <!-- ContentPresenter 自动展示 Button.Content -->
    </Border>
</ControlTemplate>
```

**工作原理：**
1. Button 继承自 ContentControl,持有 `ContentProperty`
2. ControlTemplate 应用时,ContentPresenter 查找 TemplatedParent 的 ContentProperty
3. 自动建立 `presenter.Content ← templatedParent.Content` 的模板绑定
4. Button 内容变化时,ContentPresenter 自动同步渲染

---

### 5.8 内容类型检测与处理

根据内容类型执行不同逻辑:

```cpp
void process_content(ContentControl* ctrl) {
    const auto& content = ctrl->content();
    
    if (content.is<core::InlineString>()) {
        // 文本内容
        auto text = ctrl->content_text();
        std::cout << "文本内容: " << text << std::endl;
        
        // 启用文本相关功能
        enable_text_search(text);
        
    } else if (content.is<UIElement*>()) {
        // 元素内容
        auto* elem = ctrl->content_element();
        std::cout << "元素内容: " << elem->GetType().name() << std::endl;
        
        // 递归处理子树
        traverse_visual_tree(elem, [](UIElement* node) {
            // 处理每个节点
        });
        
    } else {
        // 空内容
        std::cout << "内容为空" << std::endl;
    }
}
```

---

## 6. 最佳实践

### 6.1 优先使用类型安全的访问器

**推荐：**
```cpp
auto text = button->content_text();
if (!text.empty()) {
    process_text(text);
}

auto* elem = button->content_element();
if (elem) {
    process_element(elem);
}
```

**不推荐：**
```cpp
const auto& var = button->content();
if (var.is<core::InlineString>()) {
    auto text = var.as<core::InlineString>();
    process_text(text);
}
```

**理由：**
- `content_text()` 和 `content_element()` 封装了类型检查逻辑,代码更简洁
- 避免手动调用 `is<T>()` 和 `as<T>()`,减少出错可能
- 返回值语义清晰(`nullptr` 或空视图表示类型不匹配)

---

### 6.2 在派生类中始终调用基类 on_content_changed()

**推荐：**
```cpp
void MyControl::on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept {
    // 先调用基类
    ContentControl::on_content_changed(old_v, new_v);
    
    // 再执行派生类逻辑
    invalidate_custom_state();
}
```

**不推荐：**
```cpp
void MyControl::on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept {
    // 忘记调用基类!
    invalidate_custom_state();
}
```

**理由：**
- 基类可能有重要的清理或初始化逻辑(尽管当前 ContentControl 基类实现为空)
- 保持良好的继承规范,避免未来基类添加逻辑时出现 bug
- 符合 C++ 虚方法重写的最佳实践

---

### 6.3 避免在 on_content_changed() 中递归设置内容

**推荐：**
```cpp
void MyControl::on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept {
    ContentControl::on_content_changed(old_v, new_v);
    
    // 只读取内容,不修改
    if (auto text = content_text(); !text.empty()) {
        // 更新其他属性
        set_tooltip(text);
    }
}
```

**不推荐：**
```cpp
void MyControl::on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept {
    ContentControl::on_content_changed(old_v, new_v);
    
    // 再次调用 set_content,导致无限递归!
    if (content_text().empty()) {
        set_content("默认文本");
    }
}
```

**理由：**
- `set_content()` 会再次触发 `on_content_changed()`,导致栈溢出
- 如需设置默认值,应在构造函数中完成
- 如需条件化内容转换,应使用标志位防止递归:

```cpp
void MyControl::on_content_changed(
    const core::Variant& old_v,
    const core::Variant& new_v
) noexcept {
    if (is_setting_content_) return;
    
    is_setting_content_ = true;
    ContentControl::on_content_changed(old_v, new_v);
    
    // 安全地修改内容
    if (content_text().empty()) {
        set_content("默认文本");
    }
    
    is_setting_content_ = false;
}
```

---

### 6.4 使用 OwnedPtr 管理元素内容生命周期

**推荐：**
```cpp
void create_button() {
    auto button = core::make_owned<Button>();
    
    auto icon = core::make_owned<Image>();
    icon->set_source("icon.png");
    
    // 1. 先设置内容(建立父子关系)
    button->set_content(icon.get());
    
    // 2. 再释放所有权(视觉树现在管理生命周期)
    icon.release();
    
    // 3. 将按钮添加到窗口
    window->set_content(button.release());
}
```

**不推荐：**
```cpp
void create_button() {
    auto button = core::make_owned<Button>();
    
    auto icon = core::make_owned<Image>();
    icon->set_source("icon.png");
    
    // 错误: 先释放所有权,icon 被销毁
    button->set_content(icon.release());
    // button 现在持有悬垂指针!
}
```

**理由：**
- `set_content()` 接受原始指针,不转移所有权
- 必须确保元素在设置时仍有效
- 通过视觉树父子关系管理生命周期,避免内存泄漏

---

### 6.5 利用 ContentProperty 实现 MVVM 解耦

**推荐：** 将业务逻辑放在 ViewModel,View 只负责绑定

**ViewModel:**
```cpp
class UserProfileViewModel : public mvvm::ViewModelBase {
public:
    MINE_PROPERTY(core::InlineString, user_name, "匿名用户")
    MINE_PROPERTY(core::InlineString, avatar_url, "")
    
    void load_user_profile(int user_id) {
        // 从 API 加载用户信息
        api::get_user_profile(user_id, [this](auto profile) {
            set_user_name(profile.name);
            set_avatar_url(profile.avatar_url);
        });
    }
};
```

**View (MML):**
```xml
<StackPanel orientation="Vertical" spacing="8">
    <!-- 头像 -->
    <Border width="64" height="64" corner_radius="32">
        <Image source="{bind vm.avatar_url}" />
    </Border>
    
    <!-- 用户名按钮 -->
    <Button content="{bind vm.user_name}" on_click="on_user_clicked" />
</StackPanel>
```

**理由：**
- ViewModel 无需引用 Button 控件,完全解耦
- View 布局变更不影响 ViewModel 逻辑
- 易于单元测试 ViewModel(无需创建 UI 控件)

---

## 7. 常见陷阱

### 7.1 混淆内容所有权管理

**问题代码：**
```cpp
void create_button() {
    auto button = core::make_owned<Button>();
    
    {
        auto icon = core::make_owned<Image>();
        button->set_content(icon.get());
        // icon 在此处被销毁!
    }
    
    window->set_content(button.release());
    // 运行时崩溃: button.Content 指向已释放内存
}
```

**正确做法：**
```cpp
void create_button() {
    auto button = core::make_owned<Button>();
    
    auto icon = core::make_owned<Image>();
    button->set_content(icon.get());
    icon.release();  // 将所有权转移给视觉树
    
    window->set_content(button.release());
}
```

**解释：**
- `set_content()` 不获取所有权,仅存储原始指针
- 必须确保内容元素的生命周期覆盖 ContentControl
- 通过 `release()` 将所有权转移给父元素(视觉树管理)

---

### 7.2 忘记处理内容类型切换

**问题代码：**
```cpp
void update_button(Button* button, const std::string& new_text) {
    // 假设内容始终是文本
    auto current_text = button->content_text();
    if (current_text == new_text) return;  // 避免重复设置
    
    button->set_content(new_text);
}

// 使用
button->set_content(icon.get());  // 设置为图标
update_button(button, "文本");    // content_text() 返回空视图,逻辑错误!
```

**正确做法：**
```cpp
void update_button(Button* button, const std::string& new_text) {
    // 检查当前内容类型
    auto current_text = button->content_text();
    if (!current_text.empty() && current_text == new_text) {
        return;  // 内容未变
    }
    
    // 无论当前是什么类型,都设置为新文本
    button->set_content(new_text);
}
```

**解释：**
- `content_text()` 在内容为 UIElement 时返回空视图,不能假设内容类型
- 始终检查返回值是否为空,或使用 `content().is<T>()` 显式检查类型
- 类型切换时,旧内容会自动从视觉树移除

---

### 7.3 在多线程环境中直接修改 Content

**问题代码：**
```cpp
// 后台线程
std::thread([button] {
    auto result = network::fetch_data();
    
    // 错误: 从非 UI 线程修改 UI 属性
    button->set_content(result.message);
}).detach();
```

**正确做法：**
```cpp
// 后台线程
std::thread([button, dispatcher = Dispatcher::current_dispatcher()] {
    auto result = network::fetch_data();
    
    // 正确: 通过 Dispatcher 调度到 UI 线程
    dispatcher->invoke([button, message = result.message] {
        button->set_content(message);
    });
}).detach();
```

**解释：**
- 依赖属性系统不是线程安全的,必须在 UI 线程修改
- 使用 `Dispatcher::invoke()` 或 `Dispatcher::begin_invoke()` 调度到 UI 线程
- 或使用 `async::post_to_ui_thread()` 简化调度

---

### 7.4 过度依赖 dynamic_cast 检查内容类型

**问题代码：**
```cpp
void process_content(ContentControl* ctrl) {
    auto* elem = ctrl->content_element();
    
    // 繁琐且效率低
    if (auto* text_block = dynamic_cast<TextBlock*>(elem)) {
        process_text_block(text_block);
    } else if (auto* image = dynamic_cast<Image*>(elem)) {
        process_image(image);
    } else if (auto* panel = dynamic_cast<Panel*>(elem)) {
        process_panel(panel);
    }
}
```

**更好的做法：** 使用访问者模式或类型标签

```cpp
// 方案 1: 访问者模式
class ContentVisitor {
public:
    virtual void visit(TextBlock& block) = 0;
    virtual void visit(Image& image) = 0;
    virtual void visit(Panel& panel) = 0;
};

void process_content(ContentControl* ctrl, ContentVisitor& visitor) {
    if (auto* elem = ctrl->content_element()) {
        elem->accept(visitor);
    }
}

// 方案 2: 检查文本内容优先
void process_content(ContentControl* ctrl) {
    if (auto text = ctrl->content_text(); !text.empty()) {
        // 简单文本,快速路径
        process_text(text);
        return;
    }
    
    // 复杂元素内容,必要时才使用 dynamic_cast
    if (auto* elem = ctrl->content_element()) {
        process_element(elem);
    }
}
```

**解释：**
- 大量 `dynamic_cast` 影响性能(RTTI 查询开销)
- 先检查 `content_text()`,文本内容无需类型转换
- 使用多态(虚方法)代替类型检查,符合 OOP 原则

---

### 7.5 误用 ContentProperty 作为数据存储

**问题代码：**
```cpp
// 试图将业务数据存储在 Content 中
button->set_content(user_id);  // 编译错误: Variant 不支持 int
```

**正确做法：** 使用附加属性或自定义字段

```cpp
// 方案 1: 使用附加属性
class UserIdProperty {
public:
    static const DependencyProperty& Property;
};

// 设置附加属性
button->SetValue(UserIdProperty::Property, user_id);

// 读取附加属性
int user_id = button->GetValue(UserIdProperty::Property).as<int>();

// 方案 2: 派生类添加字段
class UserButton : public Button {
public:
    void set_user_id(int id) { user_id_ = id; }
    int user_id() const { return user_id_; }

private:
    int user_id_ = 0;
};
```

**解释：**
- `ContentProperty` 的语义是"显示内容",不是任意数据存储
- Variant 只支持 `InlineString` 和 `UIElement*`,不能存储业务数据
- 使用附加属性或自定义字段存储元数据

---

## 8. 完整示例

### 8.1 多功能工具栏按钮

实现一个支持图标、文本、徽章的工具栏按钮:

```cpp
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Image.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/StackPanel.h>

class ToolbarButton : public Button {
    MINE_RTTI_DECLARE(ToolbarButton, Button)

public:
    ToolbarButton() {
        // 设置默认样式
        set_padding(Thickness{8, 4, 8, 4});
        set_background(Brush::transparent());
        set_border_thickness(Thickness{0});
    }

    /// 设置图标 + 文本内容
    void set_content(core::StringView icon_path, core::StringView text) {
        auto panel = core::make_owned<StackPanel>();
        panel->set_orientation(Orientation::Vertical);
        panel->set_spacing(4.0f);
        panel->set_horizontal_alignment(HorizontalAlignment::Center);
        
        // 图标
        auto icon = core::make_owned<Image>();
        icon->set_source(icon_path);
        icon->set_width(24.0f);
        icon->set_height(24.0f);
        icon->set_horizontal_alignment(HorizontalAlignment::Center);
        panel->add_child(icon.release());
        
        // 文本
        auto label = core::make_owned<TextBlock>();
        label->set_text(text);
        label->set_font_size(11.0f);
        label->set_horizontal_alignment(HorizontalAlignment::Center);
        panel->add_child(label.release());
        
        // 设置为按钮内容
        Button::set_content(panel.get());
        panel.release();
    }

    /// 设置通知徽章数量
    void set_badge_count(int count) {
        badge_count_ = count;
        update_badge();
    }

private:
    void update_badge() {
        // 获取当前内容布局
        auto* panel = dynamic_cast<StackPanel*>(content_element());
        if (!panel) return;
        
        // 移除旧徽章(如果存在)
        if (badge_) {
            panel->remove_child(badge_);
            badge_ = nullptr;
        }
        
        // 添加新徽章
        if (badge_count_ > 0) {
            auto badge_grid = core::make_owned<Grid>();
            badge_grid->set_width(20.0f);
            badge_grid->set_height(20.0f);
            badge_grid->set_horizontal_alignment(HorizontalAlignment::Right);
            badge_grid->set_vertical_alignment(VerticalAlignment::Top);
            badge_grid->set_margin(Thickness{0, -8, -8, 0});
            
            // 红色圆形背景
            auto bg = core::make_owned<Border>();
            bg->set_background(Brush::from_color(Colors::Red));
            bg->set_corner_radius(CornerRadii{10, 10, 10, 10});
            badge_grid->add_child(bg.release());
            
            // 白色数字文本
            auto text = core::make_owned<TextBlock>();
            text->set_text(std::to_string(badge_count_));
            text->set_font_size(10.0f);
            text->set_foreground(Brush::from_color(Colors::White));
            text->set_horizontal_alignment(HorizontalAlignment::Center);
            text->set_vertical_alignment(VerticalAlignment::Center);
            badge_grid->add_child(text.release());
            
            badge_ = badge_grid.get();
            panel->add_child(badge_grid.release());
        }
    }

    int badge_count_ = 0;
    Grid* badge_ = nullptr;
};

// 使用示例
void create_toolbar(Panel* toolbar) {
    // 保存按钮
    auto save_btn = core::make_owned<ToolbarButton>();
    save_btn->set_content("assets/icons/save.svg", "保存");
    save_btn->add_handler(Button::ClickEvent(), [](auto& e) {
        // 保存逻辑
    });
    toolbar->add_child(save_btn.release());
    
    // 消息按钮(带徽章)
    auto msg_btn = core::make_owned<ToolbarButton>();
    msg_btn->set_content("assets/icons/message.svg", "消息");
    msg_btn->set_badge_count(5);  // 5 条未读消息
    msg_btn->add_handler(Button::ClickEvent(), [=](auto& e) {
        // 打开消息面板
        msg_btn->set_badge_count(0);  // 清除徽章
    });
    toolbar->add_child(msg_btn.release());
    
    // 设置按钮
    auto settings_btn = core::make_owned<ToolbarButton>();
    settings_btn->set_content("assets/icons/settings.svg", "设置");
    settings_btn->add_handler(Button::ClickEvent(), [](auto& e) {
        // 打开设置对话框
    });
    toolbar->add_child(settings_btn.release());
}
```

---

### 8.2 内容切换动画控件

实现一个支持淡入淡出过渡的内容控件:

```cpp
#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/animation/DoubleAnimation.h>
#include <mine/ui/animation/Storyboard.h>

class TransitionContentControl : public ContentControl {
    MINE_RTTI_DECLARE(TransitionContentControl, ContentControl)

public:
    /// 设置过渡动画时长
    void set_transition_duration(std::chrono::milliseconds duration) {
        transition_duration_ = duration;
    }

protected:
    void on_content_changed(
        const core::Variant& old_v,
        const core::Variant& new_v
    ) noexcept override {
        ContentControl::on_content_changed(old_v, new_v);
        
        // 获取旧/新内容元素
        UIElement* old_elem = old_v.is<UIElement*>() 
            ? old_v.as<UIElement*>() 
            : nullptr;
        UIElement* new_elem = content_element();
        
        // 无旧内容,直接显示新内容
        if (!old_elem && new_elem) {
            new_elem->set_opacity(1.0f);
            return;
        }
        
        // 无新内容,直接隐藏旧内容
        if (old_elem && !new_elem) {
            animate_fadeout(old_elem);
            return;
        }
        
        // 有旧有新,播放交叉淡入淡出
        if (old_elem && new_elem) {
            animate_crossfade(old_elem, new_elem);
        }
    }

private:
    void animate_fadeout(UIElement* elem) {
        auto sb = animation::Storyboard::create();
        
        auto anim = core::make_owned<animation::DoubleAnimation>();
        anim->set_from(1.0);
        anim->set_to(0.0);
        anim->set_duration(transition_duration_);
        anim->set_easing_function(animation::EasingFunction::ease_in_out());
        
        sb.add_animation(elem, UIElement::OpacityProperty, anim.release());
        
        auto storyboard = sb.build();
        storyboard->set_completed_callback([elem] {
            elem->set_visibility(Visibility::Collapsed);
        });
        
        storyboard->begin();
    }

    void animate_crossfade(UIElement* old_elem, UIElement* new_elem) {
        // 设置新元素初始状态
        new_elem->set_opacity(0.0f);
        new_elem->set_visibility(Visibility::Visible);
        
        auto sb = animation::Storyboard::create();
        
        // 旧元素淡出
        auto fadeout = core::make_owned<animation::DoubleAnimation>();
        fadeout->set_from(1.0);
        fadeout->set_to(0.0);
        fadeout->set_duration(transition_duration_);
        fadeout->set_easing_function(animation::EasingFunction::ease_in_out());
        sb.add_animation(old_elem, UIElement::OpacityProperty, fadeout.release());
        
        // 新元素淡入
        auto fadein = core::make_owned<animation::DoubleAnimation>();
        fadein->set_from(0.0);
        fadein->set_to(1.0);
        fadein->set_duration(transition_duration_);
        fadein->set_easing_function(animation::EasingFunction::ease_in_out());
        sb.add_animation(new_elem, UIElement::OpacityProperty, fadein.release());
        
        auto storyboard = sb.build();
        storyboard->set_completed_callback([old_elem] {
            old_elem->set_visibility(Visibility::Collapsed);
        });
        
        storyboard->begin();
    }

    std::chrono::milliseconds transition_duration_{300};
};

// 使用示例
void create_animated_panel() {
    auto ctrl = core::make_owned<TransitionContentControl>();
    ctrl->set_transition_duration(500ms);
    
    // 初始内容
    auto page1 = core::make_owned<TextBlock>();
    page1->set_text("第一页内容");
    ctrl->set_content(page1.release());
    
    window->set_content(ctrl.get());
    ctrl.release();
    
    // 2 秒后切换到第二页
    auto timer = core::make_owned<Timer>();
    timer->set_interval(2000ms);
    timer->set_tick_callback([ctrl_ptr = ctrl.get()] {
        auto page2 = core::make_owned<TextBlock>();
        page2->set_text("第二页内容");
        ctrl_ptr->set_content(page2.release());
    });
    timer->start();
}
```

---

### 8.3 MVVM 内容绑定示例

完整的 MVVM 示例,演示 ContentControl 与 ViewModel 的绑定:

**ViewModel:**
```cpp
#include <mine/mvvm/ViewModelBase.h>
#include <mine/core/InlineString.h>

class DashboardViewModel : public mvvm::ViewModelBase {
    MINE_RTTI_DECLARE(DashboardViewModel, mvvm::ViewModelBase)

public:
    // 属性宏自动生成 getter/setter 和变更通知
    MINE_PROPERTY(core::InlineString, welcome_text, "欢迎使用 MineUI")
    MINE_PROPERTY(core::InlineString, status_text, "就绪")
    MINE_PROPERTY(int, unread_count, 0)
    
    DashboardViewModel() {
        // 模拟异步加载数据
        load_dashboard_data();
    }

    void load_dashboard_data() {
        // 模拟网络请求
        async::post([this] {
            std::this_thread::sleep_for(1s);
            
            // 更新 UI (自动触发 PropertyChanged 事件)
            dispatch_to_ui_thread([this] {
                set_welcome_text("你好,张三");
                set_status_text("数据已更新");
                set_unread_count(12);
            });
        });
    }

    // 命令: 标记所有消息为已读
    void mark_all_read() {
        set_unread_count(0);
        set_status_text("所有消息已读");
    }
};
```

**View (MML):**
```xml
<Window title="仪表板" width="800" height="600">
    <Grid>
        <Grid.RowDefinitions>
            <RowDefinition height="Auto" />  <!-- 标题栏 -->
            <RowDefinition height="*" />     <!-- 内容区域 -->
            <RowDefinition height="Auto" />  <!-- 状态栏 -->
        </Grid.RowDefinitions>
        
        <!-- 标题栏 -->
        <Border grid_row="0" background="#F5F5F5" padding="16">
            <TextBlock text="{bind vm.welcome_text}" 
                       font_size="24" 
                       font_weight="Bold" />
        </Border>
        
        <!-- 内容区域 -->
        <StackPanel grid_row="1" padding="16" spacing="12">
            <!-- 消息按钮(内容绑定到 ViewModel) -->
            <Button width="200" height="48">
                <!-- Button.Content 绑定到 vm.unread_count -->
                <Button.Content>
                    <StackPanel orientation="Horizontal" spacing="8">
                        <Image source="assets/icons/mail.svg" width="20" height="20" />
                        <TextBlock vertical_alignment="Center">
                            <TextBlock.Text>
                                {bind vm.unread_count, converter: 'CountToText'}
                            </TextBlock.Text>
                        </TextBlock>
                    </StackPanel>
                </Button.Content>
                
                <!-- 点击事件绑定到 ViewModel 命令 -->
                <Button.Command>
                    {bind vm.MarkAllReadCommand}
                </Button.Command>
            </Button>
            
            <!-- 其他控件... -->
        </StackPanel>
        
        <!-- 状态栏 -->
        <Border grid_row="2" background="#E0E0E0" padding="8">
            <TextBlock text="{bind vm.status_text}" font_size="12" />
        </Border>
    </Grid>
</Window>
```

**值转换器:**
```cpp
#include <mine/ui/binding/ValueConverter.h>

class CountToTextConverter : public mvvm::IValueConverter {
public:
    core::Variant convert(
        const core::Variant& value,
        const TypeInfo* target_type,
        const core::Variant& parameter
    ) override {
        if (value.is<int>()) {
            int count = value.as<int>();
            if (count == 0) {
                return core::InlineString{"没有未读消息"};
            } else {
                return core::InlineString{
                    std::format("{} 条未读消息", count)
                };
            }
        }
        return core::InlineString{"消息"};
    }

    core::Variant convert_back(
        const core::Variant& value,
        const TypeInfo* target_type,
        const core::Variant& parameter
    ) override {
        // 不支持反向转换
        throw std::runtime_error("CountToTextConverter: convert_back not supported");
    }
};

// 注册转换器
void register_converters() {
    mvvm::ValueConverterRegistry::register_converter(
        "CountToText",
        core::make_owned<CountToTextConverter>()
    );
}
```

**C++ View 代码(等效于 MML):**
```cpp
void setup_dashboard_view(Window* window, DashboardViewModel* vm) {
    auto grid = core::make_owned<Grid>();
    
    // 行定义
    grid->add_row_definition(GridLength::auto_size());
    grid->add_row_definition(GridLength::star(1.0f));
    grid->add_row_definition(GridLength::auto_size());
    
    // 标题栏
    auto title_border = core::make_owned<Border>();
    title_border->set_background(Brush::from_color(Color{0xF5, 0xF5, 0xF5}));
    title_border->set_padding(Thickness{16});
    Grid::set_row(title_border.get(), 0);
    
    auto title_text = core::make_owned<TextBlock>();
    title_text->bind_property(
        TextBlock::TextProperty,
        vm,
        "welcome_text"
    );
    title_text->set_font_size(24.0f);
    title_border->set_child(title_text.release());
    grid->add_child(title_border.release());
    
    // 内容区域
    auto content_panel = core::make_owned<StackPanel>();
    content_panel->set_padding(Thickness{16});
    content_panel->set_spacing(12.0f);
    Grid::set_row(content_panel.get(), 1);
    
    // 消息按钮
    auto msg_button = core::make_owned<Button>();
    msg_button->set_width(200.0f);
    msg_button->set_height(48.0f);
    
    // 按钮内容(图标 + 文本)
    auto button_panel = core::make_owned<StackPanel>();
    button_panel->set_orientation(Orientation::Horizontal);
    button_panel->set_spacing(8.0f);
    
    auto icon = core::make_owned<Image>();
    icon->set_source("assets/icons/mail.svg");
    icon->set_width(20.0f);
    icon->set_height(20.0f);
    button_panel->add_child(icon.release());
    
    auto label = core::make_owned<TextBlock>();
    label->set_vertical_alignment(VerticalAlignment::Center);
    label->bind_property(
        TextBlock::TextProperty,
        vm,
        "unread_count",
        BindingMode::OneWay,
        core::make_owned<CountToTextConverter>()
    );
    button_panel->add_child(label.release());
    
    msg_button->set_content(button_panel.get());
    button_panel.release();
    
    // 绑定命令
    msg_button->add_handler(Button::ClickEvent(), [vm](auto& e) {
        vm->mark_all_read();
    });
    
    content_panel->add_child(msg_button.release());
    grid->add_child(content_panel.release());
    
    // 状态栏
    auto status_border = core::make_owned<Border>();
    status_border->set_background(Brush::from_color(Color{0xE0, 0xE0, 0xE0}));
    status_border->set_padding(Thickness{8});
    Grid::set_row(status_border.get(), 2);
    
    auto status_text = core::make_owned<TextBlock>();
    status_text->bind_property(
        TextBlock::TextProperty,
        vm,
        "status_text"
    );
    status_text->set_font_size(12.0f);
    status_border->set_child(status_text.release());
    grid->add_child(status_border.release());
    
    // 设置窗口内容
    window->set_content(grid.release());
}
```

---

## 9. 总结

### 9.1 核心要点

- **ContentControl** 是所有单内容控件的基类,引入了"内容"概念
- **ContentProperty** 存储 `core::Variant`,支持 `InlineString` 或 `UIElement*`
- **类型安全访问器** `content_text()` 和 `content_element()` 简化类型检查
- **on_content_changed()** 虚方法允许派生类响应内容变更
- **所有权管理** 通过视觉树父子关系实现,外部使用 `OwnedPtr` 控制生命周期

### 9.2 与 WPF 的对比

| 特性 | MineFramework ContentControl | WPF ContentControl |
|------|------------------------------|---------------------|
| 内容存储类型 | `core::Variant` (InlineString/UIElement*) | `object` (任意对象) |
| 类型安全性 | 编译期类型检查 | 运行期类型转换 |
| 小字符串优化 | InlineString (SSO) | string (总是堆分配) |
| 模板绑定 | ContentPresenter 自动绑定 | 相同 |
| 生命周期管理 | 视觉树 + OwnedPtr | 引用计数 (GC) |

### 9.3 性能特性

- **零拷贝字符串**：< 23 字节字符串栈上存储,无堆分配
- **指针存储**：UIElement 内容以原始指针存储,无引用计数开销
- **延迟布局**：仅在内容变更时标记 InvalidateMeasure
- **类型化访问**：`content_text()` 和 `content_element()` 避免反复类型检查

### 9.4 使用建议

1. **优先使用类型安全访问器** (`content_text()`/`content_element()`) 而非直接操作 Variant
2. **派生类必须调用基类 `on_content_changed()`**,保持继承规范
3. **避免在 `on_content_changed()` 中递归设置内容**,使用标志位防护
4. **使用 OwnedPtr 管理元素生命周期**,先 `set_content()` 再 `release()`
5. **MVVM 场景下绑定 ContentProperty**,实现 View 与 ViewModel 解耦

### 9.5 后续学习

- **ContentPresenter**：ControlTemplate 中的内容占位符,自动渲染 ContentControl.Content
- **ItemsControl**：多内容控件基类,扩展了 ContentControl 的单内容模型
- **ControlTemplate**：控件模板系统,实现外观与逻辑分离
- **DataBinding**：依赖属性绑定系统,支持 MVVM 模式

### 9.6 常见问题快速索引

| 问题 | 解决方案 | 参考章节 |
|------|----------|---------|
| 内容元素被提前销毁 | 先 `set_content()` 再 `release()` | 7.1 |
| 内容类型切换未处理 | 使用 `content_text().empty()` 检查 | 7.2 |
| 跨线程修改内容崩溃 | 使用 `Dispatcher::invoke()` 调度到 UI 线程 | 7.3 |
| 大量 `dynamic_cast` 性能差 | 优先检查 `content_text()`,使用访问者模式 | 7.4 |
| Content 不能存储业务数据 | 使用附加属性或自定义字段 | 7.5 |

---

**文档版本:** 1.0  
**最后更新:** 2025-01-10  
**作者:** MineUI 开发团队  
**许可证:** MIT License
