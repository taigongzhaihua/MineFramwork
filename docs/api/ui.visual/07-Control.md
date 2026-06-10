# Control 详细接口文档

## 概述

`Control` 是 `mine.ui.visual` 模块的**具有样式属性和视觉状态支持的控件基类**。

**核心特性：**
- **样式槽位**：set_style_slot / style_slot（存储样式槽位名，如 "DefaultButton"）
- **视觉状态**：visual_state（当前视觉状态枚举：Normal/Hovered/Pressed/Focused/Disabled）
- **视觉状态管理器**：set_visual_state_manager / vsm（VisualStateManager 负责状态切换和动画）
- **内部子元素**：set_inner_element / inner_element（管理内部实现元素，如 ContentPresenter）
- **自定义外观**：继承控件并重写 on_render() 实现自定义绘制（类似 QWidget 风格）

**继承关系：**
```
DependencyObject (mine.ui.property)
    └─ Visual (mine.ui.visual)
        └─ UIElement (mine.ui.visual)
            └─ FrameworkElement (mine.ui.visual)
                └─ Control (mine.ui.visual)
                    └─ Button / TextBlock / ...（mine.ui.controls.*）
```

---

## 文件位置

```
src/mine/ui/visual/api/include/mine/ui/visual/Control.h
```

---

## 类定义

```cpp
enum class ControlVisualState : uint8_t {
    Normal = 0,
    Hovered,
    Pressed,
    Focused,
    Disabled,
};

class MINE_UI_VISUAL_API Control : public FrameworkElement {
public:
    // 生命周期
    Control();
    ~Control() override;
    Control(const Control&)            = delete;
    Control& operator=(const Control&) = delete;
    Control(Control&&)                 = default;
    Control& operator=(Control&&)      = default;

    // 样式槽位
    void set_style_slot(core::StringView slot);
    [[nodiscard]] core::StringView style_slot() const noexcept;

    // 视觉状态
    [[nodiscard]] ControlVisualState visual_state() const noexcept;
    void update_visual_state();

    // VisualStateManager
    void set_visual_state_manager(style::VisualStateManager vsm) noexcept;
    [[nodiscard]] style::VisualStateManager* vsm() noexcept;
    [[nodiscard]] const style::VisualStateManager* vsm() const noexcept;

protected:
    // 内部子元素管理
    void set_inner_element(UIElement* root) noexcept;
    void set_inner_element(core::OwnedPtr<UIElement> root) noexcept;
    
    template<typename TElement,
             std::enable_if_t<std::is_base_of_v<UIElement, TElement> &&
                              !std::is_same_v<UIElement, TElement>, int> = 0>
    void set_inner_element(core::OwnedPtr<TElement> root) noexcept;
    
    [[nodiscard]] UIElement* inner_element() const noexcept;

    // 布局覆盖
    math::Size measure_override(math::Size available) override;
    math::Size arrange_override(math::Size final_size) override;

    // 视觉状态计算
    [[nodiscard]] virtual ControlVisualState compute_visual_state() const;
    [[nodiscard]] virtual core::StringView compute_state_name() const noexcept;
    virtual void on_visual_state_changed(ControlVisualState old_state,
                                         ControlVisualState new_state);

private:
    struct Impl;
    core::Pimpl<Impl> cp_;
    containers::InlineString style_slot_;
    ControlVisualState visual_state_{ ControlVisualState::Normal };
};
```

---

## 视觉状态枚举

### ControlVisualState

```cpp
enum class ControlVisualState : uint8_t {
    Normal = 0,
    Hovered,
    Pressed,
    Focused,
    Disabled,
};
```

**描述**：控件视觉状态枚举。

**枚举值**：
- `Normal`（0）：正常状态（默认）
- `Hovered`：鼠标悬停
- `Pressed`：鼠标按下
- `Focused`：键盘焦点
- `Disabled`：禁用状态

---

## 生命周期

### Control

```cpp
Control();
```

**描述**：构造 Control 对象。

**初始状态**：
- `style_slot_` = 空字符串
- `visual_state_` = `ControlVisualState::Normal`
- 无内部子元素

---

### ~Control

```cpp
~Control() override;
```

**描述**：析构 Control 对象，清理内部子元素。

---

## 样式槽位

### set_style_slot

```cpp
void set_style_slot(core::StringView slot);
```

**描述**：设置样式槽位名（例如 "DefaultButton"）。

**参数**：
- `slot`：样式槽位名

**特征**：
- 当前仅存储槽位，不执行样式解析
- 样式系统后续扩展后，此槽位用于样式查找

**示例**：
```cpp
button.set_style_slot("PrimaryButton");
```

---

### style_slot

```cpp
[[nodiscard]] core::StringView style_slot() const noexcept;
```

**描述**：返回样式槽位名。

**返回值**：
- 样式槽位名（`core::StringView`）

**示例**：
```cpp
core::StringView slot = button.style_slot();
std::cout << "样式槽位: " << slot << std::endl;
```

---

## 视觉状态

### visual_state

```cpp
[[nodiscard]] ControlVisualState visual_state() const noexcept;
```

**描述**：返回当前视觉状态。

**返回值**：
- 当前视觉状态枚举（`ControlVisualState`）

**示例**：
```cpp
ControlVisualState state = button.visual_state();
if (state == ControlVisualState::Hovered) {
    std::cout << "鼠标悬停" << std::endl;
}
```

---

### update_visual_state

```cpp
void update_visual_state();
```

**描述**：刷新视觉状态（调用 `compute_visual_state` + `on_visual_state_changed`）。

**行为**：
1. 调用 `compute_visual_state()` 计算新状态
2. 若新状态与旧状态不同，调用 `on_visual_state_changed(old_state, new_state)`
3. 更新内部 `visual_state_` 字段

**使用时机**：
- 鼠标进入/离开控件时（MouseEnter / MouseLeave）
- 鼠标按下/释放时（MouseDown / MouseUp）
- 键盘焦点变化时（GotFocus / LostFocus）
- 控件启用/禁用时（IsEnabled 属性变化）

**示例**：
```cpp
// 鼠标进入事件
void Button::on_mouse_enter(MouseEventArgs& args) {
    update_visual_state();  // 刷新状态为 Hovered
}

// 鼠标离开事件
void Button::on_mouse_leave(MouseEventArgs& args) {
    update_visual_state();  // 刷新状态为 Normal
}
```

---

## VisualStateManager

### set_visual_state_manager

```cpp
void set_visual_state_manager(style::VisualStateManager vsm) noexcept;
```

**描述**：安装视觉状态管理器。

**参数**：
- `vsm`：已配置好状态和过渡的 `VisualStateManager` 实例（移动所有权）

**特征**：
- VSM 负责管理控件的视觉状态切换（Normal/Hovered/Pressed 等）
- 安装后可通过 `vsm()` 获取并调用 `go_to_state()` 驱动状态机
- 支持状态切换动画（由 VSM 配置）

**示例**：
```cpp
style::VisualStateManager vsm;

// 添加状态（Normal、Hovered、Pressed）
vsm.add_state("Normal");
vsm.add_state("Hovered");
vsm.add_state("Pressed");

// 添加过渡（状态切换时的动画）
vsm.add_transition("Normal", "Hovered", 0.2f);  // 0.2 秒过渡
vsm.add_transition("Hovered", "Normal", 0.2f);
vsm.add_transition("Hovered", "Pressed", 0.1f);
vsm.add_transition("Pressed", "Hovered", 0.1f);

// 安装到控件
button.set_visual_state_manager(std::move(vsm));
```

---

### vsm

```cpp
[[nodiscard]] style::VisualStateManager* vsm() noexcept;
[[nodiscard]] const style::VisualStateManager* vsm() const noexcept;
```

**描述**：返回已安装的 `VisualStateManager` 指针。

**返回值**：
- 已安装的 VSM 指针；未安装时返回 `nullptr`

**示例**：
```cpp
style::VisualStateManager* vsm = button.vsm();
if (vsm != nullptr) {
    vsm->go_to_state("Hovered", true);  // 切换到 Hovered 状态
}
```

---

## 内部子元素管理

### set_inner_element (非拥有版本)

```cpp
void set_inner_element(UIElement* root) noexcept;
```

**描述**：设置内部子元素（不拥有所有权）。

**参数**：
- `root`：内部子元素指针（`nullptr` 表示清除）

**行为**：
- 将 `root` 加入控件的视觉子树
- 若此前已有内部元素，先将旧元素从子树中移除
- 供 Button 等控件在构造函数中直接安装 ContentPresenter 等内部实现元素

**示例**：
```cpp
class MyButton : public Control {
public:
    MyButton() {
        content_presenter_ = std::make_unique<ContentPresenter>();
        set_inner_element(content_presenter_.get());  // 不转移所有权
    }

private:
    std::unique_ptr<ContentPresenter> content_presenter_;
};
```

---

### set_inner_element (拥有版本)

```cpp
void set_inner_element(core::OwnedPtr<UIElement> root) noexcept;

template<typename TElement,
         std::enable_if_t<std::is_base_of_v<UIElement, TElement> &&
                          !std::is_same_v<UIElement, TElement>, int> = 0>
void set_inner_element(core::OwnedPtr<TElement> root) noexcept;
```

**描述**：设置内部子元素并转移所有权。

**参数**：
- `root`：内部子元素（`OwnedPtr<UIElement>` 或 `OwnedPtr<TElement>`，转移所有权）

**行为**：
- 将 `root` 加入控件的视觉子树
- 控件拥有 `root` 的所有权，析构时自动销毁
- 模板重载支持子类型（`OwnedPtr<Border>` 等），内部进行类型擦除

**示例**：
```cpp
class MyButton : public Control {
public:
    MyButton() {
        auto border = core::make_owned<Border>();
        set_inner_element(std::move(border));  // 转移所有权
    }
};
```

---

### inner_element

```cpp
[[nodiscard]] UIElement* inner_element() const noexcept;
```

**描述**：返回当前内部子元素指针。

**返回值**：
- 内部子元素指针；未安装时返回 `nullptr`

**用途**：
- 子类在 `measure_override` / `on_render` 中访问内部元素

**示例**：
```cpp
math::Size measure_override(math::Size available) override {
    UIElement* inner = inner_element();
    if (inner != nullptr) {
        inner->measure(available);
        return inner->desired_size();
    }
    return {0, 0};
}
```

---

## 布局覆盖

### measure_override

```cpp
math::Size measure_override(math::Size available) override;
```

**描述**：Measure 覆盖，将测量委托给内部子元素。

**参数**：
- `available`：经约束处理后的可用内容区域尺寸

**返回值**：
- 内容期望尺寸（不含 Margin）

**默认实现**：
- 若内部元素已安装，委托其 `measure()` 并返回其期望尺寸
- 否则返回零尺寸 `{0, 0}`

**子类覆盖建议**：
- 简单控件通常不需要覆盖此方法
- 复杂控件（如自定义面板）可覆盖实现自定义测量逻辑

---

### arrange_override

```cpp
math::Size arrange_override(math::Size final_size) override;
```

**描述**：Arrange 覆盖，将内部子元素排列至内容区域。

**参数**：
- `final_size`：分配给内容区域的最终尺寸（已减去 Margin）

**返回值**：
- 实际占用的内容尺寸

**默认实现**：
- 若内部元素已安装，将其排列至整个内容区域（`{0, 0, final_size.width, final_size.height}`）
- 返回 `final_size`

**子类覆盖建议**：
- 简单控件通常不需要覆盖此方法
- 复杂控件（如自定义面板）可覆盖实现自定义排列逻辑

---

## 受保护虚方法

### compute_visual_state

```cpp
[[nodiscard]] virtual ControlVisualState compute_visual_state() const;
```

**描述**：由子类计算当前视觉状态（枚举）。

**返回值**：
- 当前视觉状态（`ControlVisualState`）

**默认实现**：
- 返回 `Normal`

**子类覆盖建议**：
- Button 等控件覆盖此方法以反映 Hovered/Pressed 等状态
- 根据鼠标位置、按下状态、焦点状态等计算视觉状态

**示例**：
```cpp
class Button : public Control {
protected:
    ControlVisualState compute_visual_state() const override {
        if (is_pressed_) {
            return ControlVisualState::Pressed;
        }
        if (is_hovered_) {
            return ControlVisualState::Hovered;
        }
        if (is_focused()) {
            return ControlVisualState::Focused;
        }
        return ControlVisualState::Normal;
    }

private:
    bool is_pressed_{ false };
    bool is_hovered_{ false };
};
```

---

### compute_state_name

```cpp
[[nodiscard]] virtual core::StringView compute_state_name() const noexcept;
```

**描述**：由子类计算当前视觉状态名字符串（供 `VisualStateManager` 使用）。

**返回值**：
- 状态名字符串（`core::StringView`）

**默认实现**：
- 将 `compute_visual_state()` 返回的枚举映射为字符串：
  - `Normal` → "Normal"
  - `Hovered` → "Hovered"
  - `Pressed` → "Pressed"
  - `Focused` → "Focused"
  - `Disabled` → "Disabled"

**子类覆盖建议**：
- 通常不需要覆盖此方法
- 若需要自定义状态名（如 "Checked"、"Unchecked"），可直接重写此函数

**示例**：
```cpp
class CheckBox : public Control {
protected:
    core::StringView compute_state_name() const noexcept override {
        if (is_checked_) {
            return "Checked";
        }
        return "Unchecked";
    }

private:
    bool is_checked_{ false };
};
```

---

### on_visual_state_changed

```cpp
virtual void on_visual_state_changed(ControlVisualState old_state,
                                     ControlVisualState new_state);
```

**描述**：视觉状态变更回调。

**参数**：
- `old_state`：旧视觉状态
- `new_state`：新视觉状态

**默认实现**：
- 触发重绘（`invalidate_render()`）

**子类覆盖建议**：
- 覆盖此方法追加逻辑（如触发 VSM 状态切换）
- 调用基类实现以保留默认重绘行为

**示例**：
```cpp
void Button::on_visual_state_changed(ControlVisualState old_state,
                                     ControlVisualState new_state) {
    // 调用基类实现（触发重绘）
    Control::on_visual_state_changed(old_state, new_state);
    
    // 触发 VSM 状态切换
    if (vsm() != nullptr) {
        core::StringView state_name = compute_state_name();
        vsm()->go_to_state(state_name, true);  // 带动画
    }
}
```

---

## 使用场景

### 1. 自定义按钮控件

```cpp
#include <mine/ui/visual/Control.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Paint.h>
#include <mine/paint/Color.h>

using namespace mine::ui;
using namespace mine::paint;

class MyButton : public Control {
public:
    MyButton() {
        set_focusable(true);  // 可聚焦
    }

protected:
    ControlVisualState compute_visual_state() const override {
        if (is_pressed_) {
            return ControlVisualState::Pressed;
        }
        if (is_hovered_) {
            return ControlVisualState::Hovered;
        }
        if (is_focused()) {
            return ControlVisualState::Focused;
        }
        return ControlVisualState::Normal;
    }
    
    void on_render(Canvas& canvas) override {
        math::Rect bounds = bounds_rect();
        
        // 根据视觉状态选择背景色
        Paint paint;
        switch (visual_state()) {
            case ControlVisualState::Pressed:
                paint.set_color(Color::blue());
                break;
            case ControlVisualState::Hovered:
                paint.set_color(Color::light_blue());
                break;
            default:
                paint.set_color(Color::gray());
                break;
        }
        
        // 绘制按钮背景
        canvas.draw_rect(bounds, paint);
    }
    
    void on_mouse_enter(MouseEventArgs& args) override {
        is_hovered_ = true;
        update_visual_state();
    }
    
    void on_mouse_leave(MouseEventArgs& args) override {
        is_hovered_ = false;
        update_visual_state();
    }
    
    void on_mouse_down(MouseEventArgs& args) override {
        is_pressed_ = true;
        update_visual_state();
    }
    
    void on_mouse_up(MouseEventArgs& args) override {
        is_pressed_ = false;
        update_visual_state();
    }

private:
    bool is_pressed_{ false };
    bool is_hovered_{ false };
};
```

---

### 2. 设置样式槽位

```cpp
// 创建按钮
MyButton primary_button;
MyButton secondary_button;

// 设置样式槽位
primary_button.set_style_slot("PrimaryButton");
secondary_button.set_style_slot("SecondaryButton");

// 样式系统后续扩展后，根据槽位查找样式并应用
```

---

### 3. 安装 VisualStateManager

```cpp
style::VisualStateManager vsm;

// 添加状态
vsm.add_state("Normal");
vsm.add_state("Hovered");
vsm.add_state("Pressed");

// 添加过渡动画（状态切换时）
vsm.add_transition("Normal", "Hovered", 0.2f);  // 0.2 秒过渡
vsm.add_transition("Hovered", "Normal", 0.2f);
vsm.add_transition("Hovered", "Pressed", 0.1f);
vsm.add_transition("Pressed", "Hovered", 0.1f);

// 安装到按钮
button.set_visual_state_manager(std::move(vsm));

// 后续状态变化时自动触发过渡动画
button.update_visual_state();
```

---

### 4. 使用内部子元素（不拥有所有权）

```cpp
class MyButton : public Control {
public:
    MyButton() {
        // 创建内部元素
        content_presenter_ = std::make_unique<ContentPresenter>();
        
        // 设置内部元素（不转移所有权）
        set_inner_element(content_presenter_.get());
    }

protected:
    math::Size measure_override(math::Size available) override {
        // 基类已委托给内部元素，通常不需要覆盖
        return Control::measure_override(available);
    }

private:
    std::unique_ptr<ContentPresenter> content_presenter_;
};
```

---

### 5. 使用内部子元素（转移所有权）

```cpp
class MyButton : public Control {
public:
    MyButton() {
        // 创建内部元素并转移所有权
        auto border = core::make_owned<Border>();
        border->set_corner_radius(5.0f);
        set_inner_element(std::move(border));
    }
};
```

---

### 6. 手动驱动 VSM 状态切换

```cpp
// 获取 VSM
style::VisualStateManager* vsm = button.vsm();
if (vsm != nullptr) {
    // 切换到 Hovered 状态（带动画）
    vsm->go_to_state("Hovered", true);
    
    // 切换到 Normal 状态（不带动画）
    vsm->go_to_state("Normal", false);
}
```

---

## 最佳实践

### 1. 覆盖 compute_visual_state 而非直接设置 visual_state_

```cpp
// ✅ 推荐：覆盖 compute_visual_state
class MyButton : public Control {
protected:
    ControlVisualState compute_visual_state() const override {
        if (is_pressed_) {
            return ControlVisualState::Pressed;
        }
        if (is_hovered_) {
            return ControlVisualState::Hovered;
        }
        return ControlVisualState::Normal;
    }
};

// ❌ 不推荐：直接设置 visual_state_（无法访问，私有成员）
// visual_state_ = ControlVisualState::Hovered;  // ❌ 无法访问
```

---

### 2. 状态变化时调用 update_visual_state

```cpp
// ✅ 推荐：状态变化时调用 update_visual_state
void on_mouse_enter(MouseEventArgs& args) override {
    is_hovered_ = true;
    update_visual_state();  // 自动计算新状态并触发重绘
}

// ❌ 不推荐：手动触发重绘（遗漏 VSM 状态切换）
void on_mouse_enter(MouseEventArgs& args) override {
    is_hovered_ = true;
    invalidate_render();  // ❌ 遗漏 VSM 状态切换
}
```

---

### 3. 使用 set_inner_element 管理内部元素

```cpp
// ✅ 推荐：使用 set_inner_element
class MyButton : public Control {
public:
    MyButton() {
        auto border = core::make_owned<Border>();
        set_inner_element(std::move(border));  // 自动管理生命周期
    }
};

// ❌ 不推荐：手动管理子树（容易遗漏）
class MyButton : public Control {
public:
    MyButton() {
        border_ = std::make_unique<Border>();
        add_child(border_.get());  // ❌ 手动管理子树
    }

private:
    std::unique_ptr<Border> border_;
};
```

---

### 4. 覆盖 on_visual_state_changed 时调用基类实现

```cpp
// ✅ 推荐：调用基类实现
void on_visual_state_changed(ControlVisualState old_state,
                             ControlVisualState new_state) override {
    Control::on_visual_state_changed(old_state, new_state);  // 触发重绘
    
    // 追加逻辑
    if (vsm() != nullptr) {
        vsm()->go_to_state(compute_state_name(), true);
    }
}

// ❌ 不推荐：不调用基类实现（遗漏默认重绘）
void on_visual_state_changed(ControlVisualState old_state,
                             ControlVisualState new_state) override {
    // ❌ 遗漏 Control::on_visual_state_changed
    // 不触发重绘
}
```

---

## 常见陷阱

### 1. 忘记调用 update_visual_state

```cpp
// ❌ 问题：状态变化但未调用 update_visual_state
void on_mouse_enter(MouseEventArgs& args) override {
    is_hovered_ = true;
    // ❌ 忘记调用 update_visual_state
}

// 视觉状态未更新，不触发重绘

// ✅ 解决：调用 update_visual_state
void on_mouse_enter(MouseEventArgs& args) override {
    is_hovered_ = true;
    update_visual_state();
}
```

---

### 2. 覆盖 on_visual_state_changed 但不调用基类实现

```cpp
// ❌ 问题：覆盖 on_visual_state_changed 但不调用基类实现
void on_visual_state_changed(ControlVisualState old_state,
                             ControlVisualState new_state) override {
    // ❌ 不调用 Control::on_visual_state_changed
    // 不触发重绘
}

// ✅ 解决：调用基类实现
void on_visual_state_changed(ControlVisualState old_state,
                             ControlVisualState new_state) override {
    Control::on_visual_state_changed(old_state, new_state);  // 触发重绘
}
```

---

### 3. 手动管理内部元素子树

```cpp
// ❌ 问题：手动管理内部元素子树
class MyButton : public Control {
public:
    MyButton() {
        border_ = std::make_unique<Border>();
        add_child(border_.get());  // ❌ 手动管理子树
    }

private:
    std::unique_ptr<Border> border_;
};

// ✅ 解决：使用 set_inner_element
class MyButton : public Control {
public:
    MyButton() {
        auto border = core::make_owned<Border>();
        set_inner_element(std::move(border));  // 自动管理生命周期
    }
};
```

---

### 4. 未安装 VSM 但调用 vsm()->go_to_state()

```cpp
// ❌ 问题：未安装 VSM 但调用 vsm()->go_to_state()
button.vsm()->go_to_state("Hovered", true);  // ❌ vsm() 返回 nullptr，崩溃

// ✅ 解决：检查 vsm() 是否为 nullptr
style::VisualStateManager* vsm = button.vsm();
if (vsm != nullptr) {
    vsm->go_to_state("Hovered", true);
}
```

---

### 5. 样式槽位拼写错误

```cpp
// ❌ 问题：样式槽位拼写错误
button.set_style_slot("PrimaryButon");  // ❌ 拼写错误

// 样式系统无法找到对应样式

// ✅ 解决：确保样式槽位名正确
button.set_style_slot("PrimaryButton");
```

---

## 完整示例

```cpp
#include <mine/ui/visual/Control.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Paint.h>
#include <mine/paint/Color.h>
#include <mine/math/Rect.h>
#include <iostream>

using namespace mine::ui;
using namespace mine::paint;
using namespace mine::math;
using namespace mine::style;

// ────────────────────────────────────────────────────────────────────────────
// 自定义按钮控件
// ────────────────────────────────────────────────────────────────────────────

class MyButton : public Control {
public:
    MyButton() {
        set_focusable(true);  // 可聚焦
        
        // 安装 VisualStateManager
        VisualStateManager vsm;
        vsm.add_state("Normal");
        vsm.add_state("Hovered");
        vsm.add_state("Pressed");
        vsm.add_transition("Normal", "Hovered", 0.2f);
        vsm.add_transition("Hovered", "Normal", 0.2f);
        vsm.add_transition("Hovered", "Pressed", 0.1f);
        vsm.add_transition("Pressed", "Hovered", 0.1f);
        set_visual_state_manager(std::move(vsm));
    }

protected:
    ControlVisualState compute_visual_state() const override {
        if (is_pressed_) {
            return ControlVisualState::Pressed;
        }
        if (is_hovered_) {
            return ControlVisualState::Hovered;
        }
        if (is_focused()) {
            return ControlVisualState::Focused;
        }
        return ControlVisualState::Normal;
    }
    
    void on_visual_state_changed(ControlVisualState old_state,
                                 ControlVisualState new_state) override {
        // 调用基类实现（触发重绘）
        Control::on_visual_state_changed(old_state, new_state);
        
        // 触发 VSM 状态切换
        if (vsm() != nullptr) {
            core::StringView state_name = compute_state_name();
            vsm()->go_to_state(state_name, true);  // 带动画
        }
        
        // 打印状态变化
        std::cout << "视觉状态变化: " 
                  << static_cast<int>(old_state) << " → " 
                  << static_cast<int>(new_state) << std::endl;
    }
    
    void on_render(Canvas& canvas) override {
        Rect bounds = bounds_rect();
        
        // 根据视觉状态选择背景色
        Paint paint;
        switch (visual_state()) {
            case ControlVisualState::Pressed:
                paint.set_color(Color::blue());
                break;
            case ControlVisualState::Hovered:
                paint.set_color(Color::light_blue());
                break;
            case ControlVisualState::Focused:
                paint.set_color(Color::yellow());
                break;
            default:
                paint.set_color(Color::gray());
                break;
        }
        
        // 绘制按钮背景
        canvas.draw_rect(bounds, paint);
        
        // 绘制边框
        Paint border_paint;
        border_paint.set_color(Color::black());
        border_paint.set_style(Paint::Style::Stroke);
        border_paint.set_stroke_width(2.0f);
        canvas.draw_rect(bounds, border_paint);
    }
    
    void on_mouse_enter(MouseEventArgs& args) override {
        is_hovered_ = true;
        update_visual_state();
    }
    
    void on_mouse_leave(MouseEventArgs& args) override {
        is_hovered_ = false;
        update_visual_state();
    }
    
    void on_mouse_down(MouseEventArgs& args) override {
        is_pressed_ = true;
        update_visual_state();
    }
    
    void on_mouse_up(MouseEventArgs& args) override {
        is_pressed_ = false;
        update_visual_state();
    }

private:
    bool is_pressed_{ false };
    bool is_hovered_{ false };
};

// ────────────────────────────────────────────────────────────────────────────
// 使用示例
// ────────────────────────────────────────────────────────────────────────────

int main() {
    // ════════════════════════════════════════════════════════════════════════
    // 1. 创建按钮
    // ════════════════════════════════════════════════════════════════════════
    MyButton button;
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. 设置样式槽位
    // ════════════════════════════════════════════════════════════════════════
    button.set_style_slot("PrimaryButton");
    
    std::cout << "样式槽位: " << button.style_slot() << std::endl;
    // 输出：样式槽位: PrimaryButton
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 验证初始视觉状态
    // ════════════════════════════════════════════════════════════════════════
    std::cout << "初始视觉状态: " << static_cast<int>(button.visual_state()) << std::endl;
    // 输出：初始视觉状态: 0 (Normal)
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 模拟鼠标交互
    // ════════════════════════════════════════════════════════════════════════
    
    // 鼠标进入
    MouseEventArgs enter_args;
    button.on_mouse_enter(enter_args);
    // 输出：视觉状态变化: 0 → 1 (Normal → Hovered)
    
    std::cout << "鼠标进入后视觉状态: " << static_cast<int>(button.visual_state()) << std::endl;
    // 输出：鼠标进入后视觉状态: 1 (Hovered)
    
    // 鼠标按下
    MouseEventArgs down_args;
    button.on_mouse_down(down_args);
    // 输出：视觉状态变化: 1 → 2 (Hovered → Pressed)
    
    std::cout << "鼠标按下后视觉状态: " << static_cast<int>(button.visual_state()) << std::endl;
    // 输出：鼠标按下后视觉状态: 2 (Pressed)
    
    // 鼠标释放
    MouseEventArgs up_args;
    button.on_mouse_up(up_args);
    // 输出：视觉状态变化: 2 → 1 (Pressed → Hovered)
    
    // 鼠标离开
    MouseEventArgs leave_args;
    button.on_mouse_leave(leave_args);
    // 输出：视觉状态变化: 1 → 0 (Hovered → Normal)
    
    // ════════════════════════════════════════════════════════════════════════
    // 5. 验证 VSM
    // ════════════════════════════════════════════════════════════════════════
    VisualStateManager* vsm = button.vsm();
    if (vsm != nullptr) {
        std::cout << "VSM 已安装" << std::endl;
        
        // 手动切换状态
        vsm->go_to_state("Hovered", true);  // 带动画
        std::cout << "手动切换到 Hovered 状态" << std::endl;
    }
    
    return 0;
}
```

---

## 总结

`Control` 是 `mine.ui.visual` 模块的具有样式属性和视觉状态支持的控件基类，具备：

1. **样式槽位**：set_style_slot / style_slot（存储样式槽位名，如 "DefaultButton"）
2. **视觉状态**：visual_state（当前视觉状态枚举：Normal/Hovered/Pressed/Focused/Disabled）
3. **视觉状态管理器**：set_visual_state_manager / vsm（VisualStateManager 负责状态切换和动画）
4. **内部子元素**：set_inner_element / inner_element（管理内部实现元素，如 ContentPresenter）
5. **自定义外观**：继承控件并重写 on_render() 实现自定义绘制（类似 QWidget 风格）

**使用建议**：
- 覆盖 **compute_visual_state** 而非直接设置 visual_state_
- 状态变化时调用 **update_visual_state**
- 使用 **set_inner_element** 管理内部元素
- 覆盖 **on_visual_state_changed** 时调用基类实现
- 检查 **vsm() 是否为 nullptr** 后调用其方法
- 确保样式槽位名正确无误
