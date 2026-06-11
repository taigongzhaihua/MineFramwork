# Page 类 API 文档

## 1. 概述

`Page` 是 MineFramework 导航系统(mine.nav)中的页面基类,继承自 `UserControl`,专门设计用于与 `Frame` 控件协同工作,实现类似 WPF/UWP 的页面导航模式。它提供了三个核心导航生命周期虚函数,允许页面响应导航事件、接收导航参数、执行数据加载/保存、以及拦截用户的离开操作。

**核心职责:**

- **导航生命周期管理**:提供 `on_navigated_to()`、`on_navigated_from()`、`on_navigate_away()` 三个虚函数,由 Frame 在导航过程中显式调用
- **导航参数传递**:通过 `on_navigated_to(param)` 接收来自前一页面或导航命令的参数(core::Variant 类型)
- **导航拦截**:`on_navigate_away()` 返回 false 可阻止用户离开当前页面(例如表单未保存时弹出确认对话框)
- **页面状态管理**:在生命周期方法中加载/保存页面状态、订阅/取消订阅事件、启动/停止动画
- **与 Frame 协作**:作为 Frame 的 Content 子元素,由 Frame 管理其添加/移除、生命周期调用、导航历史栈

**继承关系:**

```
DependencyObject
    └── Visual
            └── UIElement
                    └── FrameworkElement
                            └── Control
                                    └── ContentControl
                                            └── UserControl
                                                    └── Page  ← 本文档
```

**架构意义:**

1. **SPA 单页应用模式**:Page + Frame 实现单窗口多页面导航,避免创建多个 Window 的开销
2. **MVVM 友好**:Page 与 UserControl 一样支持 DataContext 属性,可绑定 ViewModel
3. **导航历史管理**:Frame 维护导航栈,支持前进/后退,Page 负责状态保存/恢复
4. **统一生命周期**:所有页面遵循统一的导航生命周期,便于状态管理和资源清理

**典型用途:**

- 应用的主要内容区域(首页、设置页、详情页等)
- 向导式流程(安装向导、注册流程)
- 选项卡页面(TabControl 的每个选项卡对应一个 Page)
- 对话框替代方案(使用 Frame.Navigate() 而非模态对话框)

**设计原则:**

- **轻量级导航**:Page 作为 Frame.Content,切换页面只需替换 Content,无需创建新窗口
- **状态分离**:页面状态存储在 ViewModel 或 Page 字段中,独立于 Frame
- **生命周期明确**:导航生命周期方法命名清晰,调用顺序确定
- **参数类型安全**:导航参数使用 Variant,支持任意类型(int、string、对象指针等)

**性能特性:**

- **延迟加载**:Page 可在 `on_navigated_to()` 中异步加载数据,避免阻塞导航
- **缓存策略**:Frame 支持页面缓存(CacheSize 属性),避免重复创建
- **资源释放**:在 `on_navigated_from()` 中释放重资源(图片、视频等)

**相关类型:**

- `Frame`:页面容器控件,管理导航栈和生命周期调用
- `UserControl`:Page 的直接基类,提供 DataContext 和组件复用能力
- `NavigationService`:导航服务(静态),提供全局导航方法
- `NavigateCommand`:导航命令,MVVM 架构中触发页面跳转

**与 UserControl 的区别:**

| 特性 | UserControl | Page |
|-----|------------|------|
| 用途 | 可复用的 UI 组件 | 导航系统的页面单元 |
| 生命周期 | on_initialized/on_loaded/on_unloaded | + on_navigated_to/on_navigated_from/on_navigate_away |
| 导航参数 | 无 | 有(on_navigated_to 参数) |
| 导航拦截 | 无 | 有(on_navigate_away 返回 bool) |
| 容器要求 | 任意容器 | 必须由 Frame 管理 |
| 典型场景 | 自定义控件、对话框内容 | 应用主页面、选项卡内容 |

**导航流程示例:**

```
用户点击"详情"按钮
    ↓
触发 Frame.Navigate("DetailPage", item_id)
    ↓
Frame 调用当前页面 CurrentPage->on_navigate_away()
    ↓ (返回 true 允许离开)
Frame 调用当前页面 CurrentPage->on_navigated_from()
    ↓
Frame 创建 DetailPage 实例并加入视觉树
    ↓
Frame 调用 DetailPage->on_navigated_to(item_id)
    ↓
DetailPage 加载数据并渲染
```

---

## 2. 文件位置

**头文件:**
```
src/mine/ui/controls/Page.h
```

**实现文件:**
```
src/mine/ui/controls/Page.cpp
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
#include <mine/ui/controls/UserControl.h>
#include <mine/core/Variant.h>
#include <mine/core/StringView.h>
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

/// @brief 导航系统页面基类,由 Frame 管理生命周期
///
/// 继承自 UserControl,新增三个导航生命周期虚函数:
/// - on_navigate_away(): 离开前拦截(返回 false 阻止导航)
/// - on_navigated_from(): 离开通知(页面即将从 Frame 移除)
/// - on_navigated_to(param): 到达通知(页面已加入 Frame 视觉树)
///
/// @example 基础页面
/// @code
/// class HomePage : public Page {
/// protected:
///     void on_navigated_to(const core::Variant& param) override {
///         // 加载首页数据
///         load_home_data();
///     }
/// };
/// @endcode
///
/// @example 带参数的详情页
/// @code
/// class DetailPage : public Page {
/// protected:
///     void on_navigated_to(const core::Variant& param) override {
///         int item_id = param.as<int>();
///         load_item_details(item_id);
///     }
/// };
/// @endcode
class MINE_UI_CONTROLS_API Page : public UserControl {
public:
    // ========== 构造/析构 ==========
    
    /// @brief 默认构造函数,初始化页面
    Page();
    
    /// @brief 析构函数,清理资源
    ~Page() override;
    
    // 禁止拷贝和移动
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&) = delete;
    Page& operator=(Page&&) = delete;
    
protected:
    // ========== 导航生命周期虚函数 ==========
    
    /// @brief 导航离开前调用,返回 false 可拦截导航
    /// @details 由 Frame.Navigate() 在离开当前页面前调用。
    ///          典型用途:检查表单是否保存、弹出确认对话框。
    /// @return true 允许离开,false 阻止导航
    /// @note 此方法在 on_navigated_from() 之前调用
    virtual bool on_navigate_away() noexcept;
    
    /// @brief 导航离开通知,页面即将从 Frame 移除
    /// @details 由 Frame.Navigate() 在切换到新页面前调用。
    ///          典型用途:保存页面状态、释放重资源、取消订阅事件。
    /// @note 此方法在页面从视觉树移除前调用
    virtual void on_navigated_from() noexcept;
    
    /// @brief 导航到达通知,页面已加入 Frame 视觉树
    /// @details 由 Frame.Navigate() 在页面加入视觉树后调用。
    ///          典型用途:接收导航参数、加载数据、启动动画。
    /// @param param 导航参数,类型由调用方指定(可以是 int、string、对象指针等)
    /// @note 此方法在页面加入视觉树后调用,可以安全地访问父元素
    virtual void on_navigated_to(const core::Variant& param) noexcept;
    
    // ========== 辅助方法 ==========
    
    /// @brief 获取当前页面所在的 Frame 控件
    /// @return Frame 指针,如果页面未加入 Frame 则返回 nullptr
    [[nodiscard]] Frame* get_frame() const noexcept;
    
    /// @brief 导航到指定页面(便捷方法)
    /// @tparam TPage 目标页面类型
    /// @param param 导航参数
    /// @return true 导航成功,false 导航失败(on_navigate_away 返回 false)
    template<typename TPage>
    bool navigate_to(const core::Variant& param = {}) {
        auto frame = get_frame();
        return frame ? frame->navigate<TPage>(param) : false;
    }
    
    /// @brief 导航到指定页面(便捷方法,按名称)
    /// @param page_name 页面类型名称(注册在 Frame 的类型映射中)
    /// @param param 导航参数
    /// @return true 导航成功,false 导航失败
    bool navigate_to(core::StringView page_name, const core::Variant& param = {});
    
    /// @brief 返回上一页(便捷方法)
    /// @return true 返回成功,false 已在栈底
    bool go_back();
    
    /// @brief 前进到下一页(便捷方法)
    /// @return true 前进成功,false 已在栈顶
    bool go_forward();
    
private:
    // 私有成员(实现细节)
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mine::ui
```

### 3.2 依赖类型

```cpp
/// @brief Frame 页面容器控件
class Frame : public ContentControl {
public:
    /// @brief 导航到指定页面类型
    /// @tparam TPage 页面类型,必须继承自 Page
    /// @param param 导航参数
    /// @return true 导航成功,false 失败
    template<typename TPage>
    bool navigate(const core::Variant& param = {});
    
    /// @brief 导航到指定页面(按名称)
    /// @param page_name 页面类型名称
    /// @param param 导航参数
    /// @return true 导航成功,false 失败
    bool navigate(core::StringView page_name, const core::Variant& param = {});
    
    /// @brief 返回上一页
    /// @return true 返回成功,false 已在栈底
    bool go_back();
    
    /// @brief 前进到下一页
    /// @return true 前进成功,false 已在栈顶
    bool go_forward();
    
    /// @brief 获取当前页面
    [[nodiscard]] Page* current_page() const noexcept;
    
    /// @brief 设置导航历史栈大小
    void set_cache_size(int size);
};

/// @brief 导航服务(静态单例)
class NavigationService {
public:
    /// @brief 获取全局导航服务实例
    static NavigationService& instance();
    
    /// @brief 注册页面类型
    /// @tparam TPage 页面类型
    /// @param page_name 页面名称(用于 MML 和字符串导航)
    template<typename TPage>
    void register_page(core::StringView page_name);
    
    /// @brief 创建页面实例
    /// @param page_name 页面名称
    /// @return Page 唯一指针
    std::unique_ptr<Page> create_page(core::StringView page_name);
};
```

### 3.3 内部实现细节

Page 内部使用 PImpl 模式隐藏实现细节:

```cpp
struct Page::Impl {
    Frame* owner_frame_;  ///< 所属的 Frame 控件
    core::Variant navigation_param_;  ///< 导航参数缓存
    bool has_navigated_to_;  ///< 是否已调用 on_navigated_to
};
```

---

## 4. 成员方法

### 4.1 构造与析构

#### 4.1.1 Page()

```cpp
Page();
```

**功能:** 构造函数,初始化页面。

**初始化内容:**
- 设置 impl_ 结构体
- 初始化 owner_frame_ 为 nullptr
- 继承 UserControl 的初始化(DataContext、生命周期)

**注意事项:**
- 不要在构造函数中访问 Frame(此时页面尚未加入 Frame)
- 不要在构造函数中加载数据,应在 `on_navigated_to()` 中加载

---

#### 4.1.2 ~Page()

```cpp
~Page() override;
```

**功能:** 析构函数,清理资源。

**清理内容:**
- 释放 impl_ 结构体
- 继承自 UserControl 的清理

---

### 4.2 导航生命周期虚函数

#### 4.2.1 on_navigate_away()

```cpp
virtual bool on_navigate_away() noexcept;
```

**功能:** 导航离开前调用,返回 false 可拦截导航。

**返回值:** true 允许离开,false 阻止导航。

**调用时机:** Frame.Navigate() 在离开当前页面前调用,在 on_navigated_from() 之前。

**典型用途:**

1. **表单未保存检查:**
```cpp
bool EditPage::on_navigate_away() noexcept {
    if (has_unsaved_changes()) {
        auto result = MessageBox::show(
            "您有未保存的更改,确定要离开吗?",
            "确认",
            MessageBoxButton::YesNo
        );
        return result == MessageBoxResult::Yes;
    }
    return true;
}
```

2. **异步操作检查:**
```cpp
bool UploadPage::on_navigate_away() noexcept {
    if (is_uploading_) {
        MessageBox::show("文件正在上传,请等待完成");
        return false;
    }
    return true;
}
```

**注意事项:**
- 此方法必须是同步的,不能异步等待用户输入(使用模态对话框是同步的)
- 返回 false 后,Frame 不会调用 on_navigated_from()
- 不要在此方法中执行耗时操作

---

#### 4.2.2 on_navigated_from()

```cpp
virtual void on_navigated_from() noexcept;
```

**功能:** 导航离开通知,页面即将从 Frame 移除。

**调用时机:** Frame.Navigate() 在切换到新页面前调用,在页面从视觉树移除前。

**典型用途:**

1. **保存页面状态:**
```cpp
void ListPage::on_navigated_from() noexcept {
    // 保存滚动位置
    auto scroll_viewer = find_child<ScrollViewer>("MainScrollViewer");
    if (scroll_viewer) {
        saved_scroll_offset_ = scroll_viewer->vertical_offset();
    }
    
    UserControl::on_navigated_from();
}
```

2. **释放重资源:**
```cpp
void VideoPage::on_navigated_from() noexcept {
    // 停止视频播放
    if (video_player_) {
        video_player_->stop();
        video_player_->release_resources();
    }
    
    UserControl::on_navigated_from();
}
```

3. **取消事件订阅:**
```cpp
void ChatPage::on_navigated_from() noexcept {
    // 取消网络事件订阅
    if (message_received_token_.is_valid()) {
        chat_service_->message_received.remove(message_received_token_);
        message_received_token_ = {};
    }
    
    UserControl::on_navigated_from();
}
```

**注意事项:**
- 必须调用基类的 `UserControl::on_navigated_from()`
- 此方法在页面仍在视觉树中时调用,可以安全访问子元素
- 不要在此方法中执行耗时操作

---

#### 4.2.3 on_navigated_to()

```cpp
virtual void on_navigated_to(const core::Variant& param) noexcept;
```

**功能:** 导航到达通知,页面已加入 Frame 视觉树。

**参数:**
- `param`: 导航参数,类型由调用方指定

**调用时机:** Frame.Navigate() 在页面加入视觉树后调用。

**典型用途:**

1. **接收导航参数:**
```cpp
void DetailPage::on_navigated_to(const core::Variant& param) noexcept {
    UserControl::on_navigated_to(param);
    
    // 参数类型 1: int(项目 ID)
    if (param.is<int>()) {
        int item_id = param.as<int>();
        load_item(item_id);
    }
    
    // 参数类型 2: string(查询关键词)
    else if (param.is<core::InlineString>()) {
        auto keyword = param.as<core::InlineString>();
        search(keyword);
    }
    
    // 参数类型 3: 对象指针
    else if (param.is<ItemData*>()) {
        auto item = param.as<ItemData*>();
        display_item(item);
    }
}
```

2. **异步加载数据:**
```cpp
void HomePage::on_navigated_to(const core::Variant& param) noexcept {
    UserControl::on_navigated_to(param);
    
    // 显示加载指示器
    set_is_loading(true);
    
    // 异步加载数据
    async::post_task([this]() {
        auto data = api_client_->fetch_home_data();
        
        async::invoke_on_ui_thread([this, data = std::move(data)]() {
            set_home_data(data);
            set_is_loading(false);
        });
    });
}
```

3. **恢复页面状态:**
```cpp
void ListPage::on_navigated_to(const core::Variant& param) noexcept {
    UserControl::on_navigated_to(param);
    
    // 恢复滚动位置(返回导航场景)
    auto scroll_viewer = find_child<ScrollViewer>("MainScrollViewer");
    if (scroll_viewer && saved_scroll_offset_ > 0) {
        scroll_viewer->scroll_to_vertical_offset(saved_scroll_offset_);
    }
}
```

**注意事项:**
- 必须调用基类的 `UserControl::on_navigated_to(param)`
- 此方法在页面已加入视觉树后调用,可以安全访问父元素和 Frame
- 异步操作应保存 weak_ptr 避免页面析构后回调崩溃

---

### 4.3 辅助方法

#### 4.3.1 get_frame()

```cpp
[[nodiscard]] Frame* get_frame() const noexcept;
```

**功能:** 获取当前页面所在的 Frame 控件。

**返回值:** Frame 指针,如果页面未加入 Frame 则返回 nullptr。

**用法:**
```cpp
auto frame = get_frame();
if (frame) {
    // 访问 Frame 的方法...
    auto current_page = frame->current_page();
}
```

**注意:** 在构造函数中调用此方法返回 nullptr,应在 `on_navigated_to()` 中调用。

---

#### 4.3.2 navigate_to<TPage>()

```cpp
template<typename TPage>
bool navigate_to(const core::Variant& param = {});
```

**功能:** 导航到指定页面的便捷方法。

**模板参数:**
- `TPage`: 目标页面类型,必须继承自 Page

**参数:**
- `param`: 导航参数,默认为空

**返回值:** true 导航成功,false 导航失败。

**用法:**
```cpp
// 无参数导航
navigate_to<HomePage>();

// 带参数导航
navigate_to<DetailPage>(core::Variant::from_int(42));
```

**等价写法:**
```cpp
auto frame = get_frame();
frame->navigate<DetailPage>(param);
```

---

#### 4.3.3 navigate_to(page_name)

```cpp
bool navigate_to(core::StringView page_name, const core::Variant& param = {});
```

**功能:** 导航到指定页面的便捷方法(按名称)。

**参数:**
- `page_name`: 页面类型名称(注册在 Frame 的类型映射中)
- `param`: 导航参数,默认为空

**返回值:** true 导航成功,false 导航失败。

**用法:**
```cpp
// MML 中注册的页面名称
navigate_to("DetailPage", core::Variant::from_int(42));
```

**注意:** 页面名称必须事先通过 `NavigationService::register_page()` 注册。

---

#### 4.3.4 go_back()

```cpp
bool go_back();
```

**功能:** 返回上一页的便捷方法。

**返回值:** true 返回成功,false 已在栈底。

**用法:**
```cpp
// 返回按钮点击事件
back_button->add_click_handler([this](auto&, auto&) {
    if (!go_back()) {
        // 已在栈底,关闭窗口或显示提示
        get_window()->close();
    }
});
```

**等价写法:**
```cpp
auto frame = get_frame();
frame->go_back();
```

---

#### 4.3.5 go_forward()

```cpp
bool go_forward();
```

**功能:** 前进到下一页的便捷方法。

**返回值:** true 前进成功,false 已在栈顶。

**用法:**
```cpp
// 前进按钮点击事件
forward_button->add_click_handler([this](auto&, auto&) {
    if (!go_forward()) {
        // 已在栈顶,禁用前进按钮
        forward_button->set_enabled(false);
    }
});
```

---

## 5. 使用场景

### 5.1 基础页面导航

**场景:** 应用的主要内容区域,多个页面之间切换。

**代码:**

**HomePage.h:**
```cpp
class HomePage : public Page {
protected:
    void on_navigated_to(const core::Variant& param) override {
        // 加载首页数据
        load_news_feed();
    }
    
private:
    void load_news_feed() {
        // 加载新闻列表...
    }
};
```

**DetailPage.h:**
```cpp
class DetailPage : public Page {
protected:
    void on_navigated_to(const core::Variant& param) override {
        // 接收文章 ID
        int article_id = param.as<int>();
        load_article(article_id);
    }
    
private:
    void load_article(int id) {
        // 加载文章详情...
    }
};
```

**MainWindow:**
```cpp
auto window = std::make_unique<Window>();
auto frame = std::make_unique<Frame>();

// 导航到首页
frame->navigate<HomePage>();

// 用户点击文章时导航到详情页
article_list->add_item_click_handler([frame_ptr = frame.get()](auto& item) {
    frame_ptr->navigate<DetailPage>(core::Variant::from_int(item.id));
});

window->set_content(std::move(frame));
```

**MML 写法:**
```xml
<Window>
    <Frame x:name="MainFrame">
        <Frame.StartupPage>
            <local:HomePage />
        </Frame.StartupPage>
    </Frame>
</Window>
```

---

### 5.2 带导航参数的页面

**场景:** 详情页、编辑页需要接收来自前一页面的数据。

**代码:**

**UserProfilePage:**
```cpp
class UserProfilePage : public Page {
protected:
    void on_navigated_to(const core::Variant& param) override {
        // 支持多种参数类型
        if (param.is<int>()) {
            // 参数是用户 ID,需要加载数据
            int user_id = param.as<int>();
            load_user_from_server(user_id);
        }
        else if (param.is<UserData*>()) {
            // 参数是用户对象,直接显示
            auto user = param.as<UserData*>();
            display_user(user);
        }
    }
    
private:
    void load_user_from_server(int user_id) {
        set_is_loading(true);
        
        async::post_task([this, user_id]() {
            auto user = api_client_->get_user(user_id);
            
            async::invoke_on_ui_thread([this, user]() {
                display_user(user.get());
                set_is_loading(false);
            });
        });
    }
    
    void display_user(UserData* user) {
        user_name_label_->set_text(user->name);
        avatar_image_->set_source(user->avatar_url);
        // ...
    }
};
```

**导航调用:**
```cpp
// 方式 1: 传递 ID
frame->navigate<UserProfilePage>(core::Variant::from_int(user_id));

// 方式 2: 传递对象指针
frame->navigate<UserProfilePage>(core::Variant::from_ptr(user_data));

// 方式 3: 传递复杂对象(使用 Variant 自定义类型)
struct NavigationContext {
    int user_id;
    std::string source_page;
};
auto ctx = std::make_shared<NavigationContext>();
ctx->user_id = 42;
ctx->source_page = "FriendsList";
frame->navigate<UserProfilePage>(core::Variant::from_ptr(ctx.get()));
```

---

### 5.3 表单页面(支持离开拦截)

**场景:** 编辑表单,用户未保存时离开需要弹出确认对话框。

**代码:**

**EditFormPage:**
```cpp
class EditFormPage : public Page {
public:
    EditFormPage() {
        // 监听表单输入变化
        name_textbox_->add_text_changed_handler([this](auto&, auto&) {
            has_unsaved_changes_ = true;
        });
    }
    
protected:
    bool on_navigate_away() override {
        // 拦截离开操作
        if (has_unsaved_changes_) {
            auto result = MessageBox::show(
                "您有未保存的更改,确定要离开吗?\n点击\"是\"放弃更改,\"否\"继续编辑。",
                "确认",
                MessageBoxButton::YesNo,
                MessageBoxIcon::Warning
            );
            
            if (result == MessageBoxResult::No) {
                return false;  // 阻止导航
            }
        }
        return true;
    }
    
    void on_navigated_to(const core::Variant& param) override {
        // 加载表单数据
        if (param.is<int>()) {
            int item_id = param.as<int>();
            load_form_data(item_id);
        }
        else {
            // 新建模式
            clear_form();
        }
        
        has_unsaved_changes_ = false;
    }
    
    void on_navigated_from() override {
        // 清理表单状态
        clear_form();
        has_unsaved_changes_ = false;
    }
    
private:
    void on_save_click() {
        // 保存表单
        save_form_data();
        has_unsaved_changes_ = false;
        
        // 返回上一页
        go_back();
    }
    
    bool has_unsaved_changes_ = false;
};
```

---

### 5.4 页面状态保存与恢复

**场景:** 列表页面,用户浏览详情后返回,恢复滚动位置和选中项。

**代码:**

**ListPage:**
```cpp
class ListPage : public Page {
protected:
    void on_navigated_from() override {
        // 保存页面状态
        auto scroll_viewer = find_child<ScrollViewer>("MainScrollViewer");
        if (scroll_viewer) {
            saved_scroll_offset_ = scroll_viewer->vertical_offset();
        }
        
        auto list_view = find_child<ListView>("ItemsListView");
        if (list_view) {
            saved_selected_index_ = list_view->selected_index();
        }
        
        UserControl::on_navigated_from();
    }
    
    void on_navigated_to(const core::Variant& param) override {
        UserControl::on_navigated_to(param);
        
        // 恢复页面状态(仅在返回导航时)
        if (param.is_empty()) {  // 空参数表示返回导航
            restore_state();
        }
        else {
            // 新导航,重置状态
            reset_state();
        }
    }
    
private:
    void restore_state() {
        auto scroll_viewer = find_child<ScrollViewer>("MainScrollViewer");
        if (scroll_viewer && saved_scroll_offset_ > 0) {
            scroll_viewer->scroll_to_vertical_offset(saved_scroll_offset_);
        }
        
        auto list_view = find_child<ListView>("ItemsListView");
        if (list_view && saved_selected_index_ >= 0) {
            list_view->set_selected_index(saved_selected_index_);
        }
    }
    
    void reset_state() {
        saved_scroll_offset_ = 0;
        saved_selected_index_ = -1;
    }
    
    double saved_scroll_offset_ = 0;
    int saved_selected_index_ = -1;
};
```

---

### 5.5 向导式流程

**场景:** 安装向导、注册流程,多个步骤页面顺序导航。

**代码:**

**WizardBasePage:**
```cpp
// 向导基类,提供通用的上一步/下一步功能
class WizardBasePage : public Page {
protected:
    void on_navigated_to(const core::Variant& param) override {
        // 接收向导上下文
        if (param.is<WizardContext*>()) {
            wizard_context_ = param.as<WizardContext*>();
        }
    }
    
    void go_to_next_step() {
        if (validate_current_step()) {
            // 保存当前步骤数据到上下文
            save_to_context();
            
            // 导航到下一步
            navigate_to_next();
        }
    }
    
    void go_to_previous_step() {
        go_back();
    }
    
    virtual bool validate_current_step() = 0;
    virtual void save_to_context() = 0;
    virtual void navigate_to_next() = 0;
    
protected:
    WizardContext* wizard_context_ = nullptr;
};
```

**WelcomePage:**
```cpp
class WelcomePage : public WizardBasePage {
protected:
    bool validate_current_step() override {
        return true;  // 欢迎页无需验证
    }
    
    void save_to_context() override {
        // 无需保存数据
    }
    
    void navigate_to_next() override {
        navigate_to<LicenseAgreementPage>(
            core::Variant::from_ptr(wizard_context_)
        );
    }
};
```

**LicenseAgreementPage:**
```cpp
class LicenseAgreementPage : public WizardBasePage {
protected:
    bool validate_current_step() override {
        if (!agree_checkbox_->is_checked()) {
            MessageBox::show("请阅读并同意许可协议");
            return false;
        }
        return true;
    }
    
    void save_to_context() override {
        wizard_context_->license_agreed = true;
    }
    
    void navigate_to_next() override {
        navigate_to<InstallLocationPage>(
            core::Variant::from_ptr(wizard_context_)
        );
    }
};
```

**主窗口:**
```cpp
auto window = std::make_unique<Window>();
auto frame = std::make_unique<Frame>();

// 创建向导上下文
auto wizard_ctx = std::make_shared<WizardContext>();

// 导航到第一步
frame->navigate<WelcomePage>(core::Variant::from_ptr(wizard_ctx.get()));

window->set_content(std::move(frame));
```

---

### 5.6 选项卡页面

**场景:** 设置面板的多个选项卡,每个选项卡对应一个 Page。

**代码:**

**MainWindow:**
```cpp
auto tab_control = std::make_unique<TabControl>();

// 添加选项卡,每个选项卡内嵌一个 Frame
auto general_tab = std::make_unique<TabItem>();
general_tab->set_header("通用");
auto general_frame = std::make_unique<Frame>();
general_frame->navigate<GeneralSettingsPage>();
general_tab->set_content(std::move(general_frame));
tab_control->add_item(std::move(general_tab));

auto advanced_tab = std::make_unique<TabItem>();
advanced_tab->set_header("高级");
auto advanced_frame = std::make_unique<Frame>();
advanced_frame->navigate<AdvancedSettingsPage>();
advanced_tab->set_content(std::move(advanced_frame));
tab_control->add_item(std::move(advanced_tab));
```

**MML 写法:**
```xml
<TabControl>
    <TabItem header="通用">
        <Frame>
            <local:GeneralSettingsPage />
        </Frame>
    </TabItem>
    
    <TabItem header="高级">
        <Frame>
            <local:AdvancedSettingsPage />
        </Frame>
    </TabItem>
</TabControl>
```

---

### 5.7 MVVM 架构中的页面

**场景:** 页面与 ViewModel 分离,便于单元测试和逻辑复用。

**ViewModel:**
```cpp
class ProductListViewModel : public ViewModelBase {
public:
    const std::vector<ProductData>& products() const { return products_; }
    
    void load_products() {
        is_loading_ = true;
        notify_property_changed("IsLoading");
        
        async::post_task([this]() {
            auto data = api_client_->get_products();
            
            async::invoke_on_ui_thread([this, data = std::move(data)]() {
                products_ = data;
                is_loading_ = false;
                notify_property_changed("Products");
                notify_property_changed("IsLoading");
            });
        });
    }
    
    void navigate_to_detail(int product_id) {
        navigation_service_->navigate<ProductDetailPage>(
            core::Variant::from_int(product_id)
        );
    }
    
private:
    std::vector<ProductData> products_;
    bool is_loading_ = false;
};
```

**View:**
```cpp
class ProductListPage : public Page {
public:
    ProductListPage() {
        set_data_context(std::make_shared<ProductListViewModel>());
    }
    
protected:
    void on_navigated_to(const core::Variant& param) override {
        auto vm = data_context().as<ProductListViewModel*>();
        vm->load_products();
    }
};
```

**MML 写法:**
```xml
<Page x:class="ProductListPage">
    <Page.DataContext>
        <local:ProductListViewModel />
    </Page.DataContext>
    
    <StackPanel>
        <ProgressBar visibility="{bind:vm.IsLoading, converter:BoolToVisibility}" />
        
        <ListView itemsSource="{bind:vm.Products}">
            <ListView.ItemTemplate>
                <DataTemplate>
                    <StackPanel orientation="Horizontal" spacing="8">
                        <Image source="{bind:item.ImageUrl}" width="50" height="50" />
                        <TextBlock text="{bind:item.Name}" />
                        <Button text="查看详情" 
                                command="{bind:vm.NavigateToDetailCommand}"
                                commandParameter="{bind:item.Id}" />
                    </StackPanel>
                </DataTemplate>
            </ListView.ItemTemplate>
        </ListView>
    </StackPanel>
</Page>
```

---

## 6. 最佳实践

### 6.1 使用 on_navigated_to 异步加载数据

**原因:**
- 避免阻塞导航,保持 UI 响应
- 在数据加载期间显示加载指示器,提升用户体验

**反模式:**
```cpp
// ❌ 不推荐:在 on_navigated_to 中同步加载数据
void DetailPage::on_navigated_to(const Variant& param) override {
    int id = param.as<int>();
    auto data = api_client_->get_item(id);  // 阻塞 UI 线程
    display_item(data);
}
```

**推荐模式:**
```cpp
// ✅ 推荐:异步加载数据
void DetailPage::on_navigated_to(const Variant& param) override {
    int id = param.as<int>();
    
    // 显示加载指示器
    loading_spinner_->set_visibility(Visibility::Visible);
    content_panel_->set_visibility(Visibility::Collapsed);
    
    // 异步加载数据
    async::post_task([this, id]() {
        auto data = api_client_->get_item(id);
        
        async::invoke_on_ui_thread([this, data]() {
            display_item(data);
            loading_spinner_->set_visibility(Visibility::Collapsed);
            content_panel_->set_visibility(Visibility::Visible);
        });
    });
}
```

---

### 6.2 在 on_navigated_from 中释放重资源

**原因:**
- 页面离开后不再需要的资源(图片、视频、网络连接)应及时释放
- 避免内存泄漏和资源浪费

**反模式:**
```cpp
// ❌ 不推荐:忘记释放资源
void VideoPage::on_navigated_from() override {
    // 视频播放器继续运行,消耗CPU和内存
}
```

**推荐模式:**
```cpp
// ✅ 推荐:释放重资源
void VideoPage::on_navigated_from() override {
    // 停止视频播放
    if (video_player_) {
        video_player_->pause();
        video_player_->release_buffers();
    }
    
    // 取消网络请求
    if (download_task_) {
        download_task_->cancel();
    }
    
    // 释放大图片
    if (image_cache_) {
        image_cache_->clear();
    }
    
    UserControl::on_navigated_from();
}
```

---

### 6.3 使用 weak_ptr 避免异步回调崩溃

**原因:**
- 页面可能在异步操作完成前被析构
- 异步回调中访问 this 指针导致崩溃

**反模式:**
```cpp
// ❌ 不推荐:直接捕获 this 指针
void ListPage::on_navigated_to(const Variant& param) override {
    async::post_task([this]() {
        auto data = load_data();
        async::invoke_on_ui_thread([this, data]() {
            // 如果页面已析构,this 是野指针!
            update_list(data);
        });
    });
}
```

**推荐模式:**
```cpp
// ✅ 推荐:使用 weak_ptr
class ListPage : public Page, 
                 public std::enable_shared_from_this<ListPage> {
protected:
    void on_navigated_to(const Variant& param) override {
        auto weak_this = weak_from_this();
        
        async::post_task([weak_this]() {
            auto data = load_data();
            
            async::invoke_on_ui_thread([weak_this, data]() {
                auto strong_this = weak_this.lock();
                if (strong_this) {
                    // 页面仍存在,安全访问
                    strong_this->update_list(data);
                }
            });
        });
    }
};
```

---

### 6.4 导航参数使用强类型封装

**原因:**
- Variant 是弱类型,容易出错
- 强类型封装提高代码可读性和类型安全

**反模式:**
```cpp
// ❌ 不推荐:使用裸 int/string 作为参数
frame->navigate<DetailPage>(core::Variant::from_int(42));

void DetailPage::on_navigated_to(const Variant& param) override {
    int id = param.as<int>();  // 不知道这个 int 代表什么
}
```

**推荐模式:**
```cpp
// ✅ 推荐:使用强类型参数
struct DetailPageParams {
    int item_id;
    std::string source_page;
    bool show_edit_button;
};

frame->navigate<DetailPage>(
    core::Variant::from_ptr(new DetailPageParams{42, "HomePage", true})
);

void DetailPage::on_navigated_to(const Variant& param) override {
    auto params = param.as<DetailPageParams*>();
    load_item(params->item_id);
    
    if (params->show_edit_button) {
        edit_button_->set_visibility(Visibility::Visible);
    }
    
    // 记得释放内存
    delete params;
}
```

---

## 7. 常见陷阱

### 7.1 在构造函数中访问 Frame

**问题:** Page 构造函数执行时尚未加入 Frame,`get_frame()` 返回 nullptr。

**错误代码:**
```cpp
// ❌ 构造函数中访问 Frame
HomePage::HomePage() {
    auto frame = get_frame();  // 返回 nullptr!
    frame->set_cache_size(10);  // 崩溃!
}
```

**后果:** 程序崩溃。

**正确做法:**
```cpp
// ✅ 在 on_navigated_to 中访问 Frame
void HomePage::on_navigated_to(const Variant& param) override {
    auto frame = get_frame();
    if (frame) {
        frame->set_cache_size(10);
    }
}
```

---

### 7.2 忘记调用基类的生命周期方法

**问题:** 重写生命周期虚函数时忘记调用基类方法,导致基类逻辑丢失。

**错误代码:**
```cpp
// ❌ 忘记调用基类方法
void DetailPage::on_navigated_to(const Variant& param) override {
    load_data();
    // 忘记调用 UserControl::on_navigated_to(param)!
}
```

**后果:** UserControl 的 DataContext 绑定等逻辑不执行。

**正确做法:**
```cpp
// ✅ 调用基类方法
void DetailPage::on_navigated_to(const Variant& param) override {
    UserControl::on_navigated_to(param);  // 先调用基类
    load_data();
}
```

---

### 7.3 on_navigate_away 中执行异步操作

**问题:** `on_navigate_away()` 必须同步返回 bool,不能异步等待。

**错误代码:**
```cpp
// ❌ 尝试异步等待用户输入
bool EditPage::on_navigate_away() override {
    if (has_unsaved_changes_) {
        // ❌ 无法异步等待
        show_confirm_dialog_async([](bool confirmed) {
            return confirmed;
        });
    }
    return true;
}
```

**后果:** 编译错误或逻辑错误。

**正确做法:**
```cpp
// ✅ 使用模态对话框(同步)
bool EditPage::on_navigate_away() override {
    if (has_unsaved_changes_) {
        auto result = MessageBox::show(
            "确定要离开吗?",
            "确认",
            MessageBoxButton::YesNo
        );
        return result == MessageBoxResult::Yes;
    }
    return true;
}
```

---

### 7.4 导航参数内存泄漏

**问题:** 使用 `Variant::from_ptr()` 传递堆对象,接收方忘记释放内存。

**错误代码:**
```cpp
// ❌ 传递方分配内存
frame->navigate<DetailPage>(
    Variant::from_ptr(new ItemData{42, "Item Name"})
);

// ❌ 接收方忘记释放
void DetailPage::on_navigated_to(const Variant& param) override {
    auto item = param.as<ItemData*>();
    display_item(item);
    // 内存泄漏!
}
```

**后果:** 内存泄漏。

**正确做法(方案 1:接收方释放):**
```cpp
// ✅ 接收方负责释放
void DetailPage::on_navigated_to(const Variant& param) override {
    auto item = param.as<ItemData*>();
    display_item(item);
    delete item;  // 释放内存
}
```

**正确做法(方案 2:使用智能指针):**
```cpp
// ✅ 使用 shared_ptr
auto item = std::make_shared<ItemData>(42, "Item Name");
frame->navigate<DetailPage>(Variant::from_ptr(item.get()));

void DetailPage::on_navigated_to(const Variant& param) override {
    auto item = param.as<ItemData*>();
    // shared_ptr 自动管理生命周期,无需手动释放
}
```

---

## 8. 完整示例

### 8.1 完整的新闻应用(多页面导航)

**场景:** 新闻应用,包含首页(文章列表)、详情页(文章内容)、评论页。

**HomePage.h:**
```cpp
#pragma once
#include <mine/ui/controls/Page.h>
#include <mine/ui/controls/ListView.h>
#include <mine/ui/controls/Button.h>
#include <vector>

struct ArticleData {
    int id;
    std::string title;
    std::string summary;
    std::string thumbnail_url;
};

class HomePage : public mine::ui::Page {
public:
    HomePage();
    
protected:
    void on_navigated_to(const mine::core::Variant& param) override;
    void on_navigated_from() override;
    
private:
    void load_articles();
    void on_article_click(int article_id);
    void on_refresh_click();
    
    mine::ui::ListView* articles_list_;
    mine::ui::Button* refresh_button_;
    std::vector<ArticleData> articles_;
    double saved_scroll_offset_ = 0;
};
```

**HomePage.cpp:**
```cpp
#include "HomePage.h"
#include "DetailPage.h"
#include <mine/async/Task.h>
#include <mine/ui/controls/ScrollViewer.h>

using namespace mine;

HomePage::HomePage() {
    // 初始化视觉树(或通过 MML 加载)
    auto root = std::make_unique<ui::StackPanel>();
    
    refresh_button_ = new ui::Button();
    refresh_button_->set_text("刷新");
    refresh_button_->add_click_handler([this](auto&, auto&) {
        on_refresh_click();
    });
    root->add_child(refresh_button_);
    
    articles_list_ = new ui::ListView();
    articles_list_->add_item_click_handler([this](auto& item) {
        auto article = item.as<ArticleData>();
        on_article_click(article.id);
    });
    root->add_child(articles_list_);
    
    set_content(std::move(root));
}

void HomePage::on_navigated_to(const core::Variant& param) {
    ui::Page::on_navigated_to(param);
    
    if (param.is_empty()) {
        // 返回导航,恢复滚动位置
        auto scroll_viewer = find_child<ui::ScrollViewer>();
        if (scroll_viewer && saved_scroll_offset_ > 0) {
            scroll_viewer->scroll_to_vertical_offset(saved_scroll_offset_);
        }
    }
    else {
        // 新导航,加载数据
        load_articles();
    }
}

void HomePage::on_navigated_from() {
    // 保存滚动位置
    auto scroll_viewer = find_child<ui::ScrollViewer>();
    if (scroll_viewer) {
        saved_scroll_offset_ = scroll_viewer->vertical_offset();
    }
    
    ui::Page::on_navigated_from();
}

void HomePage::load_articles() {
    refresh_button_->set_enabled(false);
    
    async::post_task([this]() {
        // 模拟网络请求
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::vector<ArticleData> data = {
            {1, "MineFramework 发布 v0.1", "首个预览版本发布", "thumb1.jpg"},
            {2, "UI 控件开发进展", "Button、CheckBox 已完成", "thumb2.jpg"},
            {3, "导航系统设计", "Page + Frame 架构详解", "thumb3.jpg"}
        };
        
        async::invoke_on_ui_thread([this, data = std::move(data)]() {
            articles_ = data;
            articles_list_->set_items_source(articles_);
            refresh_button_->set_enabled(true);
        });
    });
}

void HomePage::on_article_click(int article_id) {
    navigate_to<DetailPage>(core::Variant::from_int(article_id));
}

void HomePage::on_refresh_click() {
    load_articles();
}
```

**DetailPage.h:**
```cpp
#pragma once
#include <mine/ui/controls/Page.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <string>

struct ArticleDetail {
    int id;
    std::string title;
    std::string author;
    std::string content;
    int comment_count;
};

class DetailPage : public mine::ui::Page {
public:
    DetailPage();
    
protected:
    void on_navigated_to(const mine::core::Variant& param) override;
    bool on_navigate_away() override;
    
private:
    void load_article(int article_id);
    void on_comments_click();
    void on_share_click();
    
    int current_article_id_ = 0;
    mine::ui::TextBlock* title_label_;
    mine::ui::TextBlock* author_label_;
    mine::ui::TextBlock* content_label_;
    mine::ui::Button* comments_button_;
    mine::ui::Button* share_button_;
    bool is_loading_ = false;
};
```

**DetailPage.cpp:**
```cpp
#include "DetailPage.h"
#include "CommentsPage.h"
#include <mine/async/Task.h>
#include <mine/ui/controls/MessageBox.h>

using namespace mine;

DetailPage::DetailPage() {
    // 初始化视觉树
    auto root = std::make_unique<ui::StackPanel>();
    
    title_label_ = new ui::TextBlock();
    title_label_->set_font_size(24);
    title_label_->set_font_weight(ui::FontWeight::Bold);
    root->add_child(title_label_);
    
    author_label_ = new ui::TextBlock();
    author_label_->set_font_size(14);
    author_label_->set_foreground(paint::SolidColorBrush(0xFF666666));
    root->add_child(author_label_);
    
    content_label_ = new ui::TextBlock();
    content_label_->set_text_wrapping(ui::TextWrapping::Wrap);
    root->add_child(content_label_);
    
    auto button_panel = std::make_unique<ui::StackPanel>();
    button_panel->set_orientation(ui::Orientation::Horizontal);
    button_panel->set_spacing(8);
    
    comments_button_ = new ui::Button();
    comments_button_->set_text("查看评论");
    comments_button_->add_click_handler([this](auto&, auto&) {
        on_comments_click();
    });
    button_panel->add_child(comments_button_);
    
    share_button_ = new ui::Button();
    share_button_->set_text("分享");
    share_button_->add_click_handler([this](auto&, auto&) {
        on_share_click();
    });
    button_panel->add_child(share_button_);
    
    root->add_child(std::move(button_panel));
    
    set_content(std::move(root));
}

void DetailPage::on_navigated_to(const core::Variant& param) {
    ui::Page::on_navigated_to(param);
    
    int article_id = param.as<int>();
    current_article_id_ = article_id;
    load_article(article_id);
}

bool DetailPage::on_navigate_away() {
    if (is_loading_) {
        ui::MessageBox::show("文章正在加载,请稍候");
        return false;
    }
    return true;
}

void DetailPage::load_article(int article_id) {
    is_loading_ = true;
    title_label_->set_text("加载中...");
    
    async::post_task([this, article_id]() {
        // 模拟网络请求
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        ArticleDetail detail{
            article_id,
            "MineFramework 发布 v0.1",
            "作者:MineFramework Team",
            "今天我们很高兴地宣布 MineFramework 首个预览版本发布...",
            42
        };
        
        async::invoke_on_ui_thread([this, detail]() {
            title_label_->set_text(detail.title);
            author_label_->set_text(detail.author);
            content_label_->set_text(detail.content);
            comments_button_->set_text(
                std::format("查看评论({})", detail.comment_count)
            );
            is_loading_ = false;
        });
    });
}

void DetailPage::on_comments_click() {
    navigate_to<CommentsPage>(core::Variant::from_int(current_article_id_));
}

void DetailPage::on_share_click() {
    ui::MessageBox::show("分享功能开发中");
}
```

**CommentsPage.h / .cpp:**
(类似实现,省略)

**main.cpp:**
```cpp
#include <mine/ui/app/Application.h>
#include <mine/ui/Window.h>
#include <mine/ui/controls/Frame.h>
#include "HomePage.h"

int main(int argc, char* argv[]) {
    mine::ui::Application app(argc, argv);
    
    // 创建主窗口
    auto window = std::make_unique<mine::ui::Window>();
    window->set_title("新闻应用");
    window->set_width(800);
    window->set_height(600);
    
    // 创建 Frame 并导航到首页
    auto frame = std::make_unique<mine::ui::Frame>();
    frame->set_cache_size(3);  // 缓存最近 3 个页面
    frame->navigate<HomePage>(core::Variant::from_int(0));
    
    window->set_content(std::move(frame));
    window->show();
    
    return app.run();
}
```

**运行效果:**
1. 启动应用,显示首页(文章列表)
2. 点击文章,导航到详情页(带文章 ID 参数)
3. 详情页异步加载文章内容
4. 点击"查看评论",导航到评论页
5. 点击返回按钮,依次返回详情页 → 首页,恢复滚动位置

---

## 9. 总结

### 9.1 核心要点

- **Page 是导航系统的页面基类**,继承自 UserControl,提供三个导航生命周期虚函数。
- **与 Frame 协作**:Page 作为 Frame.Content,由 Frame 管理其生命周期和导航历史栈。
- **导航参数传递**:通过 `on_navigated_to(param)` 接收 Variant 类型的参数,支持任意类型。
- **导航拦截**:`on_navigate_away()` 返回 false 可阻止用户离开(例如表单未保存)。
- **状态管理**:在 `on_navigated_from()` 中保存状态,在 `on_navigated_to()` 中恢复状态。

### 9.2 使用建议

- **异步加载数据**:在 `on_navigated_to()` 中异步加载数据,避免阻塞 UI。
- **释放重资源**:在 `on_navigated_from()` 中释放不再需要的资源(图片、视频、网络连接)。
- **使用 weak_ptr**:异步回调中使用 weak_ptr 避免页面析构后崩溃。
- **强类型参数**:导航参数使用强类型封装,提高代码可读性和类型安全。

### 9.3 扩展方向

- **页面转场动画**:Frame 支持自定义页面切换动画(淡入淡出、滑动等)。
- **页面缓存策略**:Frame 支持 LRU 缓存,避免频繁创建页面。
- **深度链接**:支持通过 URI 导航到指定页面(如 `myapp://article/42`)。
- **导航拦截器**:全局导航拦截器,统一处理权限检查、日志记录等。

### 9.4 相关文档

- [UserControl 类 API 文档](09-UserControl.md)
- [Frame 控件](../ui.window/02-Frame.md)
- [导航服务](../nav/01-NavigationService.md)
- [数据绑定系统](../ui.binding/01-Binding.md)
- [MVVM 架构指南](../../10-mvvm.md)

---

**文档版本:** v0.1.0  
**最后更新:** 2026-06-11  
**维护者:** MineFramework Team
