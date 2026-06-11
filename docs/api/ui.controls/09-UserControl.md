# UserControl 类 API 文档

## 1. 概述

`UserControl` 是 MineFramework 中可复用 UI 组件的基类,继承自 `ContentControl`,专门设计用于创建自定义控件和复合组件。它引入了 `DataContext` 属性用于 MVVM 数据绑定,提供了四个生命周期虚函数,并作为 MML 组件系统(component X : UserControl)的 C++ 对应基类。

**核心职责:**

- **组件复用**:作为自定义控件的基类,封装复杂的 UI 逻辑和视觉树,支持在多个地方复用
- **DataContext 支持**:提供 `DataContextProperty` 依赖属性,支持 MVVM 架构的数据绑定,DataContext 自动向下传播到子元素
- **生命周期管理**:提供 `on_initialized()`、`on_loaded()`、`on_unloaded()`、`on_data_context_changed()` 四个虚函数,允许派生类响应组件生命周期事件
- **视觉树根管理**:通过 `set_content(UIElement*)` 设置视觉树根,Content 自动加入 UserControl 的视觉子树
- **与 Page 的区别**:UserControl 用于可复用组件,Page 用于导航页面

**继承关系:**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── ContentControl
                                            └── UserControl  ← 本文档
                                                    └── Page
```

**架构意义:**

1. **MVVM 组件化**:UserControl 与 ViewModel 配合,实现视图与逻辑分离,便于单元测试和复用
2. **MML 组件系统**:MML 中的 `<component X : UserControl>` 编译为继承 UserControl 的 C++ 类
3. **控件库构建**:自定义控件库(如图表库、富文本编辑器)基于 UserControl 实现
4. **组合优于继承**:复杂控件通过组合多个 UserControl 实现,而非深度继承

**典型用途:**

- 自定义控件(日期选择器、颜色选择器、富文本编辑器)
- 复合控件(带标题的输入框、带验证的表单字段)
- 对话框内容(登录对话框、设置面板)
- 列表项模板(复杂的列表项布局)

**设计原则:**

- **单一职责**:每个 UserControl 负责一个独立的 UI 功能
- **松耦合**:通过 DataContext 和依赖属性与外部交互,避免直接依赖
- **可测试**:业务逻辑封装在 ViewModel 中,UserControl 仅负责 UI

**性能特性:**

- **延迟初始化**:`on_initialized()` 在首次 measure 后调用,避免不必要的初始化
- **生命周期优化**:`on_loaded()/on_unloaded()` 允许控件在加入/移除视觉树时启动/停止资源密集型操作
- **DataContext 继承**:子元素自动继承父元素的 DataContext,无需手动传递

**相关类型:**

- `ContentControl`:UserControl 的直接基类,提供 Content 属性
- `Page`:继承自 UserControl,新增导航生命周期
- `DataContext`:Variant 类型,存储 ViewModel 或数据对象
- `DependencyProperty`:依赖属性系统,DataContext 基于此实现

**与 ContentControl 的区别:**

| 特性 | ContentControl | UserControl |
|-----|---------------|-------------|
| 用途 | 单内容容器基类 | 可复用组件基类 |
| DataContext | 无 | 有(DataContextProperty) |
| 生命周期 | 无 | 有(on_initialized/on_loaded/on_unloaded) |
| MML 支持 | 无 | 有(component 语法) |
| 典型派生类 | Button、CheckBox | 自定义控件、对话框内容 |

**与 Page 的区别:**

| 特性 | UserControl | Page |
|-----|------------|------|
| 用途 | 可复用的 UI 组件 | 导航系统的页面单元 |
| 生命周期 | on_initialized/on_loaded/on_unloaded | + on_navigated_to/on_navigated_from/on_navigate_away |
| 导航参数 | 无 | 有(on_navigated_to 参数) |
| 导航拦截 | 无 | 有(on_navigate_away 返回 bool) |
| 容器要求 | 任意容器 | 必须由 Frame 管理 |

**生命周期顺序:**

```
UserControl 构造函数
    ↓
set_content(root_element)  // 设置视觉树根
    ↓
首次 on_measure() 调用
    ↓
on_initialized()  // 仅调用一次
    ↓
加入父节点视觉树
    ↓
on_loaded()
    ↓
(控件显示和交互)
    ↓
从父节点视觉树移除
    ↓
on_unloaded()
    ↓
~UserControl 析构函数
```

**DataContext 继承机制:**

```
Window (DataContext: MainViewModel)
    └── StackPanel (DataContext: 继承 MainViewModel)
            ├── TextBlock (DataContext: 继承 MainViewModel)
            └── LoginControl : UserControl (DataContext: 设置为 LoginViewModel)
                    └── StackPanel (DataContext: 继承 LoginViewModel)
                            ├── TextBox (DataContext: 继承 LoginViewModel)
                            └── Button (DataContext: 继承 LoginViewModel)
```

---

## 2. 文件位置

**头文件:**
```
src/mine/ui/controls/UserControl.h
```

**实现文件:**
```
src/mine/ui/controls/UserControl.cpp
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
#include <mine/core/Variant.h>
```

**链接依赖:**
```
mine.ui.controls.lib
mine.ui.lib
```

---

## 3. 类定义

### 3.1 完整类声明

```cpp
namespace mine::ui {

/// @brief 可复用 UI 组件基类,支持 DataContext 和生命周期管理
///
/// 继承自 ContentControl,新增:
/// - DataContextProperty: 数据上下文属性,用于 MVVM 数据绑定
/// - 四个生命周期虚函数: on_initialized、on_loaded、on_unloaded、on_data_context_changed
///
/// @example 基础自定义控件
/// @code
/// class LoginControl : public UserControl {
/// public:
///     LoginControl() {
///         // 构造视觉树
///         auto root = std::make_unique<StackPanel>();
///         
///         username_textbox_ = new TextBox();
///         root->add_child(username_textbox_);
///         
///         password_textbox_ = new TextBox();
///         password_textbox_->set_is_password(true);
///         root->add_child(password_textbox_);
///         
///         auto login_button = new Button();
///         login_button->set_text("登录");
///         login_button->add_click_handler([this](auto&, auto&) {
///             on_login();
///         });
///         root->add_child(login_button);
///         
///         set_content(std::move(root));
///     }
///     
/// private:
///     void on_login() {
///         auto vm = data_context().as<LoginViewModel*>();
///         vm->login(username_textbox_->text(), password_textbox_->text());
///     }
///     
///     TextBox* username_textbox_;
///     TextBox* password_textbox_;
/// };
/// @endcode
class MINE_UI_CONTROLS_API UserControl : public ContentControl {
public:
    // ========== 依赖属性 ==========
    
    /// @brief 数据上下文属性,存储 ViewModel 或数据对象
    /// @details DataContext 自动向下传播到子元素,
    ///          除非子元素显式设置自己的 DataContext。
    [[nodiscard]] static const DependencyProperty& DataContextProperty;
    
    // ========== 构造/析构 ==========
    
    /// @brief 默认构造函数,初始化 UserControl
    UserControl();
    
    /// @brief 析构函数,清理资源
    ~UserControl() override;
    
    // 禁止拷贝和移动
    UserControl(const UserControl&) = delete;
    UserControl& operator=(const UserControl&) = delete;
    UserControl(UserControl&&) = delete;
    UserControl& operator=(UserControl&&) = delete;
    
    // ========== 属性访问器 ==========
    
    /// @brief 设置数据上下文
    /// @param ctx 数据对象或 ViewModel,通常是 shared_ptr 或原始指针
    void set_data_context(const core::Variant& ctx);
    
    /// @brief 获取数据上下文
    /// @return 当前 DataContext 的 Variant 引用
    [[nodiscard]] const core::Variant& data_context() const noexcept;
    
protected:
    // ========== 生命周期虚函数 ==========
    
    /// @brief 初始化完成回调,仅调用一次
    /// @details 在首次 on_measure() 调用完成后触发。
    ///          此时视觉树已构建完成,可以安全地访问子元素。
    ///          典型用途:查找命名子元素、初始化控件状态、订阅事件。
    /// @note 仅调用一次,后续重新测量不会触发
    virtual void on_initialized() noexcept;
    
    /// @brief 加入视觉树回调
    /// @details 在控件加入父节点视觉树后调用。
    ///          典型用途:启动动画、订阅全局事件、启动定时器。
    /// @note 可能被多次调用(如果控件被移除后再次加入)
    virtual void on_loaded() noexcept;
    
    /// @brief 移除视觉树回调
    /// @details 在控件从父节点视觉树移除后调用。
    ///          典型用途:停止动画、取消事件订阅、释放重资源。
    /// @note 可能被多次调用(与 on_loaded 配对)
    virtual void on_unloaded() noexcept;
    
    /// @brief DataContext 变化回调
    /// @details 在 DataContextProperty 值变化后调用。
    ///          典型用途:更新 UI 绑定、订阅 ViewModel 事件。
    /// @param old_ctx 旧的 DataContext
    /// @param new_ctx 新的 DataContext
    virtual void on_data_context_changed(const core::Variant& old_ctx, 
                                          const core::Variant& new_ctx) noexcept;
    
    // ========== ContentControl 虚函数重写 ==========
    
    /// @brief Content 变化回调
    /// @details 当 ContentProperty 值变化时调用。
    ///          UserControl 通常在构造函数中通过 set_content() 设置根元素,
    ///          此回调用于处理外部修改 Content 的情况。
    /// @param old_v 旧的 Content
    /// @param new_v 新的 Content
    void on_content_changed(const core::Variant& old_v, 
                            const core::Variant& new_v) noexcept override;
    
    // ========== 辅助方法 ==========
    
    /// @brief 查找命名子元素
    /// @tparam T 子元素类型
    /// @param name 元素名称(通过 set_name() 设置)
    /// @return 子元素指针,未找到则返回 nullptr
    template<typename T>
    T* find_child(core::StringView name) const noexcept {
        return dynamic_cast<T*>(find_child_impl(name));
    }
    
private:
    // ========== 内部实现 ==========
    
    /// @brief 查找命名子元素的内部实现
    UIElement* find_child_impl(core::StringView name) const noexcept;
    
    /// @brief DataContextProperty 变化回调(内部)
    void on_data_context_property_changed(const core::Variant& old_ctx, 
                                           const core::Variant& new_ctx);
    
    // 私有成员
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mine::ui
```

### 3.2 依赖类型

```cpp
/// @brief Variant 类型,存储任意类型数据
class Variant {
public:
    /// @brief 从共享指针创建 Variant
    template<typename T>
    static Variant from_shared_ptr(std::shared_ptr<T> ptr);
    
    /// @brief 从原始指针创建 Variant
    template<typename T>
    static Variant from_ptr(T* ptr);
    
    /// @brief 转换为指定类型
    template<typename T>
    T as() const;
    
    /// @brief 检查是否为指定类型
    template<typename T>
    bool is() const;
    
    /// @brief 检查是否为空
    bool is_empty() const;
};
```

### 3.3 内部实现细节

UserControl 内部使用 PImpl 模式隐藏实现细节:

```cpp
struct UserControl::Impl {
    core::Variant data_context_;       ///< 数据上下文缓存
    bool is_initialized_;              ///< 是否已调用 on_initialized
    bool is_loaded_;                   ///< 是否已加入视觉树
};
```

**DataContext 继承规则:**

1. 如果元素显式设置了 DataContext,使用自己的 DataContext
2. 否则,继承父元素的 DataContext
3. 子元素的 DataContext 变化时,递归通知所有子元素

**伪代码:**
```cpp
void UIElement::update_inherited_data_context(const Variant& parent_ctx) {
    if (!has_local_data_context_) {
        // 继承父元素的 DataContext
        if (data_context_ != parent_ctx) {
            auto old_ctx = data_context_;
            data_context_ = parent_ctx;
            on_data_context_changed(old_ctx, parent_ctx);
        }
    }
    
    // 递归更新子元素
    for (auto& child : visual_children_) {
        child->update_inherited_data_context(data_context_);
    }
}
```

---

## 4. 成员方法

### 4.1 静态方法

#### 4.1.1 DataContextProperty

```cpp
[[nodiscard]] static const DependencyProperty& DataContextProperty;
```

**功能:** 返回数据上下文依赖属性的全局单例。

**返回值:** `DependencyProperty` 常量引用。

**用法:**
```cpp
// 设置 DataContext
user_control->set_value(UserControl::DataContextProperty, 
                        core::Variant::from_ptr(view_model.get()));

// 数据绑定
user_control->set_binding(TextBlock::TextProperty, 
                          Binding("Name"));  // 隐式绑定到 DataContext.Name
```

**属性元数据:**

| 属性 | 类型 | 默认值 | 影响 | 继承性 |
|-----|------|--------|------|--------|
| DataContextProperty | Variant | Empty | 触发 on_data_context_changed | 继承(FrameworkPropertyMetadata.Inherits) |

**继承机制:**

DataContext 是可继承属性,子元素自动继承父元素的 DataContext:

```cpp
// 父元素设置 DataContext
parent->set_data_context(view_model);

// 子元素自动继承
child->data_context();  // 返回 view_model
```

---

### 4.2 构造与析构

#### 4.2.1 UserControl()

```cpp
UserControl();
```

**功能:** 构造函数,初始化 UserControl。

**初始化内容:**
- 设置 impl_ 结构体
- 初始化 is_initialized_ 为 false
- 初始化 is_loaded_ 为 false
- 注册 DataContextProperty 变化回调

**注意事项:**
- 派生类应在构造函数中通过 `set_content()` 设置视觉树根
- 不要在构造函数中调用虚函数(如 `on_initialized()`)

**典型用法:**
```cpp
class CustomControl : public UserControl {
public:
    CustomControl() {
        // 构造视觉树
        auto root = std::make_unique<StackPanel>();
        // ... 添加子元素 ...
        set_content(std::move(root));
    }
};
```

---

#### 4.2.2 ~UserControl()

```cpp
~UserControl() override;
```

**功能:** 析构函数,清理资源。

**清理内容:**
- 释放 impl_ 结构体
- 取消 DataContextProperty 回调订阅
- 继承自 ContentControl 的清理

---

### 4.3 属性访问器

#### 4.3.1 set_data_context()

```cpp
void set_data_context(const core::Variant& ctx);
```

**功能:** 设置数据上下文。

**参数:**
- `ctx`: 数据对象或 ViewModel,类型可以是 `shared_ptr<T>` 或 `T*`

**用法:**

**方式 1:使用 shared_ptr(推荐):**
```cpp
auto view_model = std::make_shared<LoginViewModel>();
user_control->set_data_context(core::Variant::from_shared_ptr(view_model));
```

**方式 2:使用原始指针(生命周期需手动管理):**
```cpp
LoginViewModel* view_model = new LoginViewModel();
user_control->set_data_context(core::Variant::from_ptr(view_model));
// 注意:需确保 view_model 在 user_control 生命周期内有效
```

**方式 3:使用栈对象(不推荐):**
```cpp
// ❌ 错误:栈对象在函数返回后失效
LoginViewModel view_model;
user_control->set_data_context(core::Variant::from_ptr(&view_model));
```

**触发行为:**
- 调用 `set_data_context()` 会触发 `on_data_context_changed()` 虚函数
- 递归更新所有子元素的继承 DataContext

---

#### 4.3.2 data_context()

```cpp
[[nodiscard]] const core::Variant& data_context() const noexcept;
```

**功能:** 获取数据上下文。

**返回值:** 当前 DataContext 的 Variant 引用。

**用法:**
```cpp
// 类型转换
auto vm = user_control->data_context().as<LoginViewModel*>();

// 检查类型
if (user_control->data_context().is<LoginViewModel*>()) {
    // 处理...
}

// 检查是否为空
if (user_control->data_context().is_empty()) {
    // DataContext 未设置
}
```

---

### 4.4 生命周期虚函数

#### 4.4.1 on_initialized()

```cpp
virtual void on_initialized() noexcept;
```

**功能:** 初始化完成回调,仅调用一次。

**调用时机:** 首次 `on_measure()` 调用完成后。

**典型用途:**

1. **查找命名子元素:**
```cpp
void CustomControl::on_initialized() noexcept {
    // 查找 MML 中定义的命名元素
    username_textbox_ = find_child<TextBox>("UsernameTextBox");
    password_textbox_ = find_child<TextBox>("PasswordTextBox");
    login_button_ = find_child<Button>("LoginButton");
    
    // 订阅事件
    login_button_->add_click_handler([this](auto&, auto&) {
        on_login();
    });
}
```

2. **初始化控件状态:**
```cpp
void ChartControl::on_initialized() noexcept {
    // 初始化图表引擎
    chart_engine_ = std::make_unique<ChartEngine>();
    chart_engine_->initialize(canvas_);
}
```

**注意事项:**
- 仅调用一次,后续重新测量不会触发
- 此时视觉树已构建完成,可以安全地访问子元素
- 不要在此方法中执行耗时操作

---

#### 4.4.2 on_loaded()

```cpp
virtual void on_loaded() noexcept;
```

**功能:** 加入视觉树回调。

**调用时机:** 控件加入父节点视觉树后。

**典型用途:**

1. **启动动画:**
```cpp
void AnimatedControl::on_loaded() noexcept {
    // 启动入场动画
    fade_in_animation_->start();
}
```

2. **订阅全局事件:**
```cpp
void ClockControl::on_loaded() noexcept {
    // 订阅定时器
    timer_token_ = Timer::instance().tick.subscribe([this](auto) {
        update_time();
    });
}
```

3. **启动后台任务:**
```cpp
void LiveDataControl::on_loaded() noexcept {
    // 启动实时数据更新
    start_data_polling();
}
```

**注意事项:**
- 可能被多次调用(如果控件被移除后再次加入)
- 与 `on_unloaded()` 配对,确保资源正确释放

---

#### 4.4.3 on_unloaded()

```cpp
virtual void on_unloaded() noexcept;
```

**功能:** 移除视觉树回调。

**调用时机:** 控件从父节点视觉树移除后。

**典型用途:**

1. **停止动画:**
```cpp
void AnimatedControl::on_unloaded() noexcept {
    // 停止所有动画
    animation_clock_->stop_all();
}
```

2. **取消事件订阅:**
```cpp
void ClockControl::on_unloaded() noexcept {
    // 取消定时器订阅
    if (timer_token_.is_valid()) {
        Timer::instance().tick.remove(timer_token_);
        timer_token_ = {};
    }
}
```

3. **释放重资源:**
```cpp
void VideoPlayerControl::on_unloaded() noexcept {
    // 停止视频播放
    if (video_player_) {
        video_player_->stop();
        video_player_->release_buffers();
    }
}
```

**注意事项:**
- 可能被多次调用(与 `on_loaded()` 配对)
- 必须释放在 `on_loaded()` 中分配的资源

---

#### 4.4.4 on_data_context_changed()

```cpp
virtual void on_data_context_changed(const core::Variant& old_ctx, 
                                      const core::Variant& new_ctx) noexcept;
```

**功能:** DataContext 变化回调。

**参数:**
- `old_ctx`: 旧的 DataContext
- `new_ctx`: 新的 DataContext

**调用时机:** DataContextProperty 值变化后。

**典型用途:**

1. **更新 UI 绑定:**
```cpp
void UserProfileControl::on_data_context_changed(
    const Variant& old_ctx, const Variant& new_ctx) noexcept {
    
    if (!new_ctx.is_empty()) {
        auto user = new_ctx.as<UserData*>();
        username_label_->set_text(user->name);
        avatar_image_->set_source(user->avatar_url);
    }
}
```

2. **订阅 ViewModel 事件:**
```cpp
void ProductListControl::on_data_context_changed(
    const Variant& old_ctx, const Variant& new_ctx) noexcept {
    
    // 取消旧 ViewModel 的订阅
    if (!old_ctx.is_empty()) {
        auto old_vm = old_ctx.as<ProductListViewModel*>();
        old_vm->products_changed.remove(products_changed_token_);
    }
    
    // 订阅新 ViewModel 的事件
    if (!new_ctx.is_empty()) {
        auto new_vm = new_ctx.as<ProductListViewModel*>();
        products_changed_token_ = new_vm->products_changed.subscribe([this](auto& products) {
            refresh_product_list(products);
        });
    }
}
```

---

### 4.5 ContentControl 虚函数重写

#### 4.5.1 on_content_changed()

```cpp
void on_content_changed(const core::Variant& old_v, 
                        const core::Variant& new_v) noexcept override;
```

**功能:** Content 变化回调。

**参数:**
- `old_v`: 旧的 Content
- `new_v`: 新的 Content

**调用时机:** ContentProperty 值变化后。

**实现细节:**

UserControl 通常在构造函数中通过 `set_content()` 设置根元素,此回调用于处理外部修改 Content 的情况:

```cpp
void UserControl::on_content_changed(const Variant& old_v, 
                                      const Variant& new_v) noexcept {
    // 调用基类处理
    ContentControl::on_content_changed(old_v, new_v);
    
    // UserControl 特定逻辑
    // (通常不需要重写此方法)
}
```

---

### 4.6 辅助方法

#### 4.6.1 find_child<T>()

```cpp
template<typename T>
T* find_child(core::StringView name) const noexcept;
```

**功能:** 查找命名子元素。

**模板参数:**
- `T`: 子元素类型

**参数:**
- `name`: 元素名称(通过 `set_name()` 设置)

**返回值:** 子元素指针,未找到或类型不匹配则返回 nullptr。

**用法:**

**方式 1:C++ 代码中查找:**
```cpp
class CustomControl : public UserControl {
public:
    CustomControl() {
        auto root = std::make_unique<StackPanel>();
        
        auto textbox = std::make_unique<TextBox>();
        textbox->set_name("UsernameTextBox");  // 设置名称
        root->add_child(std::move(textbox));
        
        set_content(std::move(root));
    }
    
protected:
    void on_initialized() override {
        // 通过名称查找
        username_textbox_ = find_child<TextBox>("UsernameTextBox");
        if (username_textbox_) {
            username_textbox_->set_placeholder("请输入用户名");
        }
    }
    
private:
    TextBox* username_textbox_ = nullptr;
};
```

**方式 2:MML 中定义名称:**
```xml
<UserControl x:class="CustomControl">
    <StackPanel>
        <TextBox x:name="UsernameTextBox" placeholder="请输入用户名" />
        <TextBox x:name="PasswordTextBox" placeholder="请输入密码" isPassword="true" />
        <Button x:name="LoginButton" text="登录" />
    </StackPanel>
</UserControl>
```

```cpp
void CustomControl::on_initialized() override {
    username_textbox_ = find_child<TextBox>("UsernameTextBox");
    password_textbox_ = find_child<TextBox>("PasswordTextBox");
    login_button_ = find_child<Button>("LoginButton");
}
```

**注意事项:**
- 仅在 `on_initialized()` 之后调用,否则视觉树可能未构建完成
- 返回 nullptr 表示未找到或类型不匹配

---

## 5. 使用场景

### 5.1 基础自定义控件

**场景:** 创建可复用的登录控件。

**代码:**

**LoginControl.h:**
```cpp
#pragma once
#include <mine/ui/controls/UserControl.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/layout/StackPanel.h>

class LoginControl : public mine::ui::UserControl {
public:
    LoginControl();
    
    // 暴露登录成功事件
    mine::event::Event<std::string /*username*/> login_success;
    
protected:
    void on_initialized() override;
    
private:
    void on_login_click();
    bool validate_input();
    
    mine::ui::TextBox* username_textbox_;
    mine::ui::TextBox* password_textbox_;
    mine::ui::Button* login_button_;
    mine::ui::TextBlock* error_label_;
};
```

**LoginControl.cpp:**
```cpp
#include "LoginControl.h"
#include <mine/ui/controls/TextBlock.h>

LoginControl::LoginControl() {
    // 构造视觉树
    auto root = std::make_unique<mine::ui::StackPanel>();
    root->set_spacing(12);
    root->set_padding(mine::ui::Thickness{16, 16, 16, 16});
    
    // 用户名输入框
    auto username_box = std::make_unique<mine::ui::TextBox>();
    username_box->set_name("UsernameTextBox");
    username_box->set_placeholder("用户名");
    root->add_child(std::move(username_box));
    
    // 密码输入框
    auto password_box = std::make_unique<mine::ui::TextBox>();
    password_box->set_name("PasswordTextBox");
    password_box->set_placeholder("密码");
    password_box->set_is_password(true);
    root->add_child(std::move(password_box));
    
    // 错误提示标签
    auto error_label = std::make_unique<mine::ui::TextBlock>();
    error_label->set_name("ErrorLabel");
    error_label->set_foreground(mine::paint::SolidColorBrush(0xFFB00020));
    error_label->set_visibility(mine::ui::Visibility::Collapsed);
    root->add_child(std::move(error_label));
    
    // 登录按钮
    auto login_btn = std::make_unique<mine::ui::Button>();
    login_btn->set_name("LoginButton");
    login_btn->set_text("登录");
    root->add_child(std::move(login_btn));
    
    set_content(std::move(root));
}

void LoginControl::on_initialized() {
    // 查找命名元素
    username_textbox_ = find_child<mine::ui::TextBox>("UsernameTextBox");
    password_textbox_ = find_child<mine::ui::TextBox>("PasswordTextBox");
    login_button_ = find_child<mine::ui::Button>("LoginButton");
    error_label_ = find_child<mine::ui::TextBlock>("ErrorLabel");
    
    // 订阅登录按钮点击事件
    login_button_->add_click_handler([this](auto&, auto&) {
        on_login_click();
    });
}

void LoginControl::on_login_click() {
    if (!validate_input()) {
        return;
    }
    
    // 模拟登录逻辑
    auto username = username_textbox_->text();
    auto password = password_textbox_->text();
    
    if (username == "admin" && password == "123456") {
        // 登录成功
        login_success.raise(std::string(username));
    }
    else {
        // 登录失败
        error_label_->set_text("用户名或密码错误");
        error_label_->set_visibility(mine::ui::Visibility::Visible);
    }
}

bool LoginControl::validate_input() {
    if (username_textbox_->text().empty()) {
        error_label_->set_text("请输入用户名");
        error_label_->set_visibility(mine::ui::Visibility::Visible);
        return false;
    }
    
    if (password_textbox_->text().size() < 6) {
        error_label_->set_text("密码至少6位");
        error_label_->set_visibility(mine::ui::Visibility::Visible);
        return false;
    }
    
    error_label_->set_visibility(mine::ui::Visibility::Collapsed);
    return true;
}
```

**使用:**
```cpp
auto login_control = std::make_unique<LoginControl>();
login_control->login_success.subscribe([](const std::string& username) {
    std::cout << "用户 " << username << " 登录成功!" << std::endl;
    // 导航到主页...
});

window->set_content(std::move(login_control));
```

---

### 5.2 MVVM 架构中的 UserControl

**场景:** UserControl 与 ViewModel 分离,支持数据绑定。

**ViewModel:**
```cpp
class ProductCardViewModel : public mine::mvvm::ViewModelBase {
public:
    const std::string& name() const { return name_; }
    void set_name(std::string value) {
        if (name_ != value) {
            name_ = std::move(value);
            notify_property_changed("Name");
        }
    }
    
    const std::string& price() const { return price_; }
    void set_price(std::string value) {
        if (price_ != value) {
            price_ = std::move(value);
            notify_property_changed("Price");
        }
    }
    
    const std::string& image_url() const { return image_url_; }
    void set_image_url(std::string value) {
        if (image_url_ != value) {
            image_url_ = std::move(value);
            notify_property_changed("ImageUrl");
        }
    }
    
    class AddToCartCommand : public mine::ui::ICommand {
    public:
        AddToCartCommand(ProductCardViewModel* vm) : vm_(vm) {}
        void execute(const mine::core::Variant&) override {
            vm_->add_to_cart();
        }
        bool can_execute(const mine::core::Variant&) const override {
            return true;
        }
    private:
        ProductCardViewModel* vm_;
    };
    
    AddToCartCommand* add_to_cart_command() { return &add_to_cart_cmd_; }
    
private:
    void add_to_cart() {
        std::cout << "添加商品: " << name_ << std::endl;
    }
    
    std::string name_;
    std::string price_;
    std::string image_url_;
    AddToCartCommand add_to_cart_cmd_{this};
};
```

**View:**
```cpp
class ProductCard : public mine::ui::UserControl {
public:
    ProductCard() {
        // 通过 MML 加载视觉树
        load_from_mml("ProductCard.mml");
    }
    
protected:
    void on_data_context_changed(const mine::core::Variant& old_ctx, 
                                  const mine::core::Variant& new_ctx) override {
        if (!new_ctx.is_empty()) {
            auto vm = new_ctx.as<ProductCardViewModel*>();
            
            // 手动绑定(或通过 MML 自动绑定)
            auto name_label = find_child<mine::ui::TextBlock>("NameLabel");
            name_label->set_binding(mine::ui::TextBlock::TextProperty, 
                                    mine::ui::Binding(vm, "Name"));
            
            auto price_label = find_child<mine::ui::TextBlock>("PriceLabel");
            price_label->set_binding(mine::ui::TextBlock::TextProperty, 
                                     mine::ui::Binding(vm, "Price"));
            
            auto image = find_child<mine::ui::Image>("ProductImage");
            image->set_binding(mine::ui::Image::SourceProperty, 
                              mine::ui::Binding(vm, "ImageUrl"));
            
            auto add_button = find_child<mine::ui::Button>("AddButton");
            add_button->set_command(vm->add_to_cart_command());
        }
    }
};
```

**MML (ProductCard.mml):**
```xml
<UserControl x:class="ProductCard">
    <Border width="200" height="300" 
            background="#FFFFFF" 
            cornerRadius="8"
            borderThickness="1"
            borderBrush="#E0E0E0">
        <StackPanel spacing="8" padding="12">
            <!-- 商品图片 -->
            <Image x:name="ProductImage" 
                   width="176" 
                   height="176"
                   source="{bind:vm.ImageUrl}" />
            
            <!-- 商品名称 -->
            <TextBlock x:name="NameLabel"
                       text="{bind:vm.Name}"
                       fontSize="16"
                       fontWeight="Bold"
                       textWrapping="Wrap" />
            
            <!-- 价格 -->
            <TextBlock x:name="PriceLabel"
                       text="{bind:vm.Price}"
                       fontSize="18"
                       foreground="#E91E63"
                       fontWeight="Bold" />
            
            <!-- 加入购物车按钮 -->
            <Button x:name="AddButton"
                    text="加入购物车"
                    command="{bind:vm.AddToCartCommand}"
                    horizontalAlignment="Stretch" />
        </StackPanel>
    </Border>
</UserControl>
```

**使用:**
```cpp
auto view_model = std::make_shared<ProductCardViewModel>();
view_model->set_name("MineFramework 教程书");
view_model->set_price("¥99.00");
view_model->set_image_url("https://example.com/book.jpg");

auto product_card = std::make_unique<ProductCard>();
product_card->set_data_context(mine::core::Variant::from_shared_ptr(view_model));

panel->add_child(std::move(product_card));
```

---

### 5.3 复合控件(带标题的输入框)

**场景:** 创建带标题、占位符、验证的输入框组件。

**代码:**

**LabeledTextBox.h:**
```cpp
#pragma once
#include <mine/ui/controls/UserControl.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/controls/TextBlock.h>
#include <string>
#include <functional>

class LabeledTextBox : public mine::ui::UserControl {
public:
    LabeledTextBox();
    
    // 属性
    void set_label(mine::core::StringView label);
    void set_placeholder(mine::core::StringView placeholder);
    void set_is_required(bool required);
    void set_validator(std::function<std::string(mine::core::StringView)> validator);
    
    // 方法
    mine::core::StringView text() const;
    void set_text(mine::core::StringView text);
    bool validate();
    
protected:
    void on_initialized() override;
    
private:
    void on_text_changed();
    
    mine::ui::TextBlock* label_;
    mine::ui::TextBox* textbox_;
    mine::ui::TextBlock* error_label_;
    
    bool is_required_ = false;
    std::function<std::string(mine::core::StringView)> validator_;
};
```

**LabeledTextBox.cpp:**
```cpp
#include "LabeledTextBox.h"
#include <mine/ui/layout/StackPanel.h>

LabeledTextBox::LabeledTextBox() {
    auto root = std::make_unique<mine::ui::StackPanel>();
    root->set_spacing(4);
    
    // 标题标签
    auto label = std::make_unique<mine::ui::TextBlock>();
    label->set_name("Label");
    label->set_font_weight(mine::ui::FontWeight::Medium);
    root->add_child(std::move(label));
    
    // 输入框
    auto textbox = std::make_unique<mine::ui::TextBox>();
    textbox->set_name("TextBox");
    root->add_child(std::move(textbox));
    
    // 错误提示标签
    auto error_label = std::make_unique<mine::ui::TextBlock>();
    error_label->set_name("ErrorLabel");
    error_label->set_font_size(12);
    error_label->set_foreground(mine::paint::SolidColorBrush(0xFFB00020));
    error_label->set_visibility(mine::ui::Visibility::Collapsed);
    root->add_child(std::move(error_label));
    
    set_content(std::move(root));
}

void LabeledTextBox::on_initialized() {
    label_ = find_child<mine::ui::TextBlock>("Label");
    textbox_ = find_child<mine::ui::TextBox>("TextBox");
    error_label_ = find_child<mine::ui::TextBlock>("ErrorLabel");
    
    // 订阅文本变化事件
    textbox_->add_text_changed_handler([this](auto&, auto&) {
        on_text_changed();
    });
}

void LabeledTextBox::set_label(mine::core::StringView label) {
    if (label_) {
        label_->set_text(label);
    }
}

void LabeledTextBox::set_placeholder(mine::core::StringView placeholder) {
    if (textbox_) {
        textbox_->set_placeholder(placeholder);
    }
}

void LabeledTextBox::set_is_required(bool required) {
    is_required_ = required;
    
    if (label_ && required) {
        // 在标题后添加 * 标记
        label_->set_text(std::string(label_->text()) + " *");
    }
}

void LabeledTextBox::set_validator(
    std::function<std::string(mine::core::StringView)> validator) {
    validator_ = std::move(validator);
}

mine::core::StringView LabeledTextBox::text() const {
    return textbox_ ? textbox_->text() : mine::core::StringView();
}

void LabeledTextBox::set_text(mine::core::StringView text) {
    if (textbox_) {
        textbox_->set_text(text);
    }
}

bool LabeledTextBox::validate() {
    auto text = textbox_->text();
    
    // 检查必填项
    if (is_required_ && text.empty()) {
        error_label_->set_text("此项为必填项");
        error_label_->set_visibility(mine::ui::Visibility::Visible);
        return false;
    }
    
    // 自定义验证
    if (validator_ && !text.empty()) {
        auto error = validator_(text);
        if (!error.empty()) {
            error_label_->set_text(error);
            error_label_->set_visibility(mine::ui::Visibility::Visible);
            return false;
        }
    }
    
    // 验证通过
    error_label_->set_visibility(mine::ui::Visibility::Collapsed);
    return true;
}

void LabeledTextBox::on_text_changed() {
    // 文本变化时清除错误提示
    error_label_->set_visibility(mine::ui::Visibility::Collapsed);
}
```

**使用:**
```cpp
// 用户名输入框
auto username_input = std::make_unique<LabeledTextBox>();
username_input->set_label("用户名");
username_input->set_placeholder("请输入用户名");
username_input->set_is_required(true);
username_input->set_validator([](mine::core::StringView text) -> std::string {
    if (text.size() < 3) {
        return "用户名至少3个字符";
    }
    return "";
});

// 邮箱输入框
auto email_input = std::make_unique<LabeledTextBox>();
email_input->set_label("邮箱");
email_input->set_placeholder("请输入邮箱地址");
email_input->set_is_required(true);
email_input->set_validator([](mine::core::StringView text) -> std::string {
    if (text.find('@') == std::string::npos) {
        return "邮箱格式不正确";
    }
    return "";
});

// 验证表单
auto submit_button = std::make_unique<mine::ui::Button>();
submit_button->set_text("提交");
submit_button->add_click_handler([&](auto&, auto&) {
    bool is_valid = username_input->validate() && email_input->validate();
    if (is_valid) {
        // 提交表单...
    }
});
```

---

### 5.4 列表项模板

**场景:** 创建复杂的列表项布局作为 ItemTemplate。

**代码:**

**ChatMessageItem.h:**
```cpp
#pragma once
#include <mine/ui/controls/UserControl.h>
#include <string>

struct ChatMessage {
    std::string sender;
    std::string avatar_url;
    std::string content;
    std::string timestamp;
    bool is_self;
};

class ChatMessageItem : public mine::ui::UserControl {
public:
    ChatMessageItem();
    
    void set_message(const ChatMessage& msg);
    
protected:
    void on_initialized() override;
    
private:
    mine::ui::Image* avatar_image_;
    mine::ui::TextBlock* sender_label_;
    mine::ui::TextBlock* content_label_;
    mine::ui::TextBlock* time_label_;
};
```

**ChatMessageItem.cpp:**
```cpp
#include "ChatMessageItem.h"
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/controls/Image.h>
#include <mine/ui/visual/Border.h>

ChatMessageItem::ChatMessageItem() {
    auto root = std::make_unique<mine::ui::StackPanel>();
    root->set_orientation(mine::ui::Orientation::Horizontal);
    root->set_spacing(8);
    root->set_padding(mine::ui::Thickness{12, 8, 12, 8});
    
    // 头像
    auto avatar = std::make_unique<mine::ui::Image>();
    avatar->set_name("AvatarImage");
    avatar->set_width(40);
    avatar->set_height(40);
    avatar->set_stretch(mine::ui::Stretch::UniformToFill);
    root->add_child(std::move(avatar));
    
    // 消息内容容器
    auto content_panel = std::make_unique<mine::ui::StackPanel>();
    content_panel->set_spacing(4);
    
    // 发送者名称
    auto sender = std::make_unique<mine::ui::TextBlock>();
    sender->set_name("SenderLabel");
    sender->set_font_weight(mine::ui::FontWeight::Bold);
    content_panel->add_child(std::move(sender));
    
    // 消息内容
    auto content = std::make_unique<mine::ui::TextBlock>();
    content->set_name("ContentLabel");
    content->set_text_wrapping(mine::ui::TextWrapping::Wrap);
    content_panel->add_child(std::move(content));
    
    // 时间戳
    auto time = std::make_unique<mine::ui::TextBlock>();
    time->set_name("TimeLabel");
    time->set_font_size(12);
    time->set_foreground(mine::paint::SolidColorBrush(0xFF999999));
    content_panel->add_child(std::move(time));
    
    root->add_child(std::move(content_panel));
    
    set_content(std::move(root));
}

void ChatMessageItem::on_initialized() {
    avatar_image_ = find_child<mine::ui::Image>("AvatarImage");
    sender_label_ = find_child<mine::ui::TextBlock>("SenderLabel");
    content_label_ = find_child<mine::ui::TextBlock>("ContentLabel");
    time_label_ = find_child<mine::ui::TextBlock>("TimeLabel");
}

void ChatMessageItem::set_message(const ChatMessage& msg) {
    if (avatar_image_) {
        avatar_image_->set_source(msg.avatar_url);
    }
    
    if (sender_label_) {
        sender_label_->set_text(msg.sender);
        sender_label_->set_foreground(
            msg.is_self ? mine::paint::SolidColorBrush(0xFF6750A4) 
                        : mine::paint::SolidColorBrush(0xFF333333)
        );
    }
    
    if (content_label_) {
        content_label_->set_text(msg.content);
    }
    
    if (time_label_) {
        time_label_->set_text(msg.timestamp);
    }
}
```

**使用(在 ListView 中):**
```cpp
auto chat_list = std::make_unique<mine::ui::ListView>();

// 设置消息数据源
std::vector<ChatMessage> messages = {
    {"张三", "avatar1.jpg", "你好!", "10:30", false},
    {"我", "avatar_self.jpg", "你好,有什么事吗?", "10:31", true},
    {"张三", "avatar1.jpg", "周末一起吃饭?", "10:32", false}
};
chat_list->set_items_source(messages);

// 设置 ItemTemplate
chat_list->set_item_template([](const mine::core::Variant& item) {
    auto msg = item.as<ChatMessage>();
    auto item_control = std::make_unique<ChatMessageItem>();
    item_control->set_message(msg);
    return item_control;
});
```

---

### 5.5 对话框内容

**场景:** 创建确认对话框的内容控件。

**代码:**

**ConfirmDialog.h:**
```cpp
#pragma once
#include <mine/ui/controls/UserControl.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>
#include <string>

class ConfirmDialog : public mine::ui::UserControl {
public:
    ConfirmDialog();
    
    void set_message(mine::core::StringView message);
    void set_confirm_text(mine::core::StringView text);
    void set_cancel_text(mine::core::StringView text);
    
    // 事件
    mine::event::Event<> confirmed;
    mine::event::Event<> cancelled;
    
protected:
    void on_initialized() override;
    
private:
    void on_confirm_click();
    void on_cancel_click();
    
    mine::ui::TextBlock* message_label_;
    mine::ui::Button* confirm_button_;
    mine::ui::Button* cancel_button_;
};
```

**ConfirmDialog.cpp:**
```cpp
#include "ConfirmDialog.h"
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/visual/Border.h>

ConfirmDialog::ConfirmDialog() {
    auto root = std::make_unique<mine::ui::Border>();
    root->set_background(mine::paint::SolidColorBrush(0xFFFFFFFF));
    root->set_corner_radius(mine::ui::CornerRadii::uniform(8));
    root->set_padding(mine::ui::Thickness{24, 24, 24, 24});
    root->set_width(400);
    
    auto content = std::make_unique<mine::ui::StackPanel>();
    content->set_spacing(16);
    
    // 消息文本
    auto message = std::make_unique<mine::ui::TextBlock>();
    message->set_name("MessageLabel");
    message->set_font_size(16);
    message->set_text_wrapping(mine::ui::TextWrapping::Wrap);
    content->add_child(std::move(message));
    
    // 按钮容器
    auto button_panel = std::make_unique<mine::ui::StackPanel>();
    button_panel->set_orientation(mine::ui::Orientation::Horizontal);
    button_panel->set_spacing(8);
    button_panel->set_horizontal_alignment(mine::ui::HorizontalAlignment::Right);
    
    // 取消按钮
    auto cancel_btn = std::make_unique<mine::ui::Button>();
    cancel_btn->set_name("CancelButton");
    cancel_btn->set_text("取消");
    cancel_btn->set_background(mine::paint::SolidColorBrush(0xFFE0E0E0));
    cancel_btn->set_foreground(mine::paint::SolidColorBrush(0xFF333333));
    button_panel->add_child(std::move(cancel_btn));
    
    // 确认按钮
    auto confirm_btn = std::make_unique<mine::ui::Button>();
    confirm_btn->set_name("ConfirmButton");
    confirm_btn->set_text("确认");
    button_panel->add_child(std::move(confirm_btn));
    
    content->add_child(std::move(button_panel));
    root->set_child(std::move(content));
    
    set_content(std::move(root));
}

void ConfirmDialog::on_initialized() {
    message_label_ = find_child<mine::ui::TextBlock>("MessageLabel");
    confirm_button_ = find_child<mine::ui::Button>("ConfirmButton");
    cancel_button_ = find_child<mine::ui::Button>("CancelButton");
    
    confirm_button_->add_click_handler([this](auto&, auto&) {
        on_confirm_click();
    });
    
    cancel_button_->add_click_handler([this](auto&, auto&) {
        on_cancel_click();
    });
}

void ConfirmDialog::set_message(mine::core::StringView message) {
    if (message_label_) {
        message_label_->set_text(message);
    }
}

void ConfirmDialog::set_confirm_text(mine::core::StringView text) {
    if (confirm_button_) {
        confirm_button_->set_text(text);
    }
}

void ConfirmDialog::set_cancel_text(mine::core::StringView text) {
    if (cancel_button_) {
        cancel_button_->set_text(text);
    }
}

void ConfirmDialog::on_confirm_click() {
    confirmed.raise();
}

void ConfirmDialog::on_cancel_click() {
    cancelled.raise();
}
```

**使用(在 Dialog 中):**
```cpp
auto dialog = std::make_unique<mine::ui::Dialog>();
auto confirm_content = std::make_unique<ConfirmDialog>();

confirm_content->set_message("确定要删除这个文件吗?此操作无法撤销。");
confirm_content->set_confirm_text("删除");
confirm_content->set_cancel_text("取消");

confirm_content->confirmed.subscribe([dialog_ptr = dialog.get()]() {
    // 执行删除操作
    delete_file();
    dialog_ptr->close(mine::ui::DialogResult::OK);
});

confirm_content->cancelled.subscribe([dialog_ptr = dialog.get()]() {
    dialog_ptr->close(mine::ui::DialogResult::Cancel);
});

dialog->set_content(std::move(confirm_content));
dialog->show();
```

---

## 6. 最佳实践

### 6.1 在 on_initialized 中查找命名元素

**原因:**
- 构造函数中视觉树可能未构建完成
- `on_initialized()` 确保所有子元素已创建

**反模式:**
```cpp
// ❌ 不推荐:在构造函数中查找
CustomControl::CustomControl() {
    auto root = std::make_unique<StackPanel>();
    // ... 添加子元素 ...
    set_content(std::move(root));
    
    // ❌ 此时 find_child 可能返回 nullptr
    username_textbox_ = find_child<TextBox>("UsernameTextBox");
}
```

**推荐模式:**
```cpp
// ✅ 推荐:在 on_initialized 中查找
CustomControl::CustomControl() {
    auto root = std::make_unique<StackPanel>();
    // ... 添加子元素 ...
    set_content(std::move(root));
}

void CustomControl::on_initialized() {
    // ✅ 此时视觉树已构建完成
    username_textbox_ = find_child<TextBox>("UsernameTextBox");
}
```

---

### 6.2 在 on_loaded/on_unloaded 中管理资源

**原因:**
- 控件可能被多次加入/移除视觉树
- 确保资源正确分配和释放

**反模式:**
```cpp
// ❌ 不推荐:在构造函数中启动动画
CustomControl::CustomControl() {
    animation_->start();  // ❌ 即使控件未显示也启动
}
```

**推荐模式:**
```cpp
// ✅ 推荐:在 on_loaded 中启动,on_unloaded 中停止
void CustomControl::on_loaded() {
    animation_->start();
}

void CustomControl::on_unloaded() {
    animation_->stop();
}
```

---

### 6.3 使用 MML 定义视觉树

**原因:**
- MML 可视化编辑,支持设计器拖拽
- 数据绑定语法更简洁
- 样式和模板可复用

**反模式:**
```cpp
// ❌ 不推荐:纯代码构造复杂视觉树
CustomControl::CustomControl() {
    auto root = std::make_unique<StackPanel>();
    // ... 100 行代码创建视觉树 ...
    set_content(std::move(root));
}
```

**推荐模式:**
```cpp
// ✅ 推荐:使用 MML
CustomControl::CustomControl() {
    load_from_mml("CustomControl.mml");
}
```

**CustomControl.mml:**
```xml
<UserControl x:class="CustomControl">
    <StackPanel spacing="12" padding="16">
        <TextBox x:name="UsernameTextBox" placeholder="用户名" />
        <TextBox x:name="PasswordTextBox" placeholder="密码" isPassword="true" />
        <Button x:name="LoginButton" text="登录" />
    </StackPanel>
</UserControl>
```

---

### 6.4 优先使用 DataContext 而非直接属性设置

**原因:**
- DataContext 支持数据绑定,自动更新
- 分离视图与逻辑,便于测试

**反模式:**
```cpp
// ❌ 不推荐:直接设置属性
auto user_card = std::make_unique<UserCard>();
user_card->set_name("张三");
user_card->set_avatar("avatar.jpg");
```

**推荐模式:**
```cpp
// ✅ 推荐:使用 DataContext
auto view_model = std::make_shared<UserViewModel>();
view_model->set_name("张三");
view_model->set_avatar("avatar.jpg");

auto user_card = std::make_unique<UserCard>();
user_card->set_data_context(Variant::from_shared_ptr(view_model));
```

---

## 7. 常见陷阱

### 7.1 在构造函数中调用虚函数

**问题:** C++ 构造函数中调用虚函数不会调用派生类的实现。

**错误代码:**
```cpp
// ❌ 基类构造函数中调用虚函数
UserControl::UserControl() {
    on_initialized();  // ❌ 不会调用派生类的 on_initialized
}

class CustomControl : public UserControl {
protected:
    void on_initialized() override {
        // 永远不会被调用!
    }
};
```

**后果:** 派生类的初始化逻辑不执行。

**正确做法:**
```cpp
// ✅ 不要在构造函数中调用虚函数
UserControl::UserControl() {
    // 不调用虚函数
}

// on_initialized 由框架在合适时机调用
```

---

### 7.2 忘记调用基类的生命周期方法

**问题:** 重写生命周期虚函数时忘记调用基类方法,导致基类逻辑丢失。

**错误代码:**
```cpp
// ❌ 忘记调用基类方法
void CustomControl::on_loaded() override {
    animation_->start();
    // 忘记调用 UserControl::on_loaded()!
}
```

**后果:** 基类的生命周期逻辑不执行(例如 DataContext 继承)。

**正确做法:**
```cpp
// ✅ 调用基类方法
void CustomControl::on_loaded() override {
    UserControl::on_loaded();  // 先调用基类
    animation_->start();
}
```

---

### 7.3 DataContext 使用栈对象导致悬空指针

**问题:** DataContext 存储栈对象指针,对象析构后指针失效。

**错误代码:**
```cpp
// ❌ 使用栈对象
void setup_control() {
    ViewModel view_model;  // 栈对象
    control->set_data_context(Variant::from_ptr(&view_model));
    // 函数返回后 view_model 析构,DataContext 指针失效!
}
```

**后果:** 访问 DataContext 时崩溃。

**正确做法:**
```cpp
// ✅ 使用 shared_ptr
void setup_control() {
    auto view_model = std::make_shared<ViewModel>();
    control->set_data_context(Variant::from_shared_ptr(view_model));
    // shared_ptr 管理生命周期,安全
}
```

---

### 7.4 find_child 类型转换失败返回 nullptr

**问题:** `find_child<T>()` 类型不匹配时返回 nullptr,未检查导致崩溃。

**错误代码:**
```cpp
// ❌ 未检查返回值
void CustomControl::on_initialized() {
    auto textbox = find_child<TextBox>("MyTextBox");
    textbox->set_text("Hello");  // ❌ 如果未找到或类型不匹配,崩溃!
}
```

**后果:** 空指针解引用崩溃。

**正确做法:**
```cpp
// ✅ 检查返回值
void CustomControl::on_initialized() {
    auto textbox = find_child<TextBox>("MyTextBox");
    if (textbox) {
        textbox->set_text("Hello");
    }
    else {
        // 记录错误或抛出异常
        std::cerr << "找不到 MyTextBox 元素" << std::endl;
    }
}
```

---

## 8. 完整示例

(由于篇幅限制,完整示例已在"使用场景"章节中展示,包括登录控件、MVVM 架构、复合控件、列表项模板、对话框内容等)

---

## 9. 总结

### 9.1 核心要点

- **UserControl 是可复用 UI 组件的基类**,继承自 ContentControl,新增 DataContext 属性和四个生命周期虚函数。
- **MVVM 友好**:DataContext 支持数据绑定,自动向下传播到子元素。
- **生命周期管理**:on_initialized(仅一次)、on_loaded/on_unloaded(配对)、on_data_context_changed(DataContext 变化)。
- **视觉树根管理**:通过 `set_content(UIElement*)` 设置视觉树根,Content 自动加入 UserControl 的视觉子树。
- **与 Page 的区别**:UserControl 用于可复用组件,Page 用于导航页面(新增导航生命周期)。

### 9.2 使用建议

- **在 on_initialized 中查找命名元素**,确保视觉树已构建完成。
- **在 on_loaded/on_unloaded 中管理资源**,确保资源正确分配和释放。
- **使用 MML 定义视觉树**,代码更简洁易维护。
- **优先使用 DataContext 而非直接属性设置**,支持数据绑定和 MVVM 架构。

### 9.3 扩展方向

- **自定义控件库**:基于 UserControl 创建图表库、富文本编辑器等。
- **主题系统**:通过 DataContext 传递主题对象,支持动态换肤。
- **国际化支持**:DataContext 绑定语言包,自动切换界面语言。
- **可访问性增强**:在 UserControl 中统一处理无障碍属性。

### 9.4 相关文档

- [ContentControl 类 API 文档](01-ContentControl.md)
- [Page 类 API 文档](08-Page.md)
- [数据绑定系统](../ui.binding/01-Binding.md)
- [MVVM 架构指南](../../10-mvvm.md)
- [MML 语言参考](../../03-mml-language.md)

---

**文档版本:** v0.1.0  
**最后更新:** 2026-06-11  
**维护者:** MineFramework Team
