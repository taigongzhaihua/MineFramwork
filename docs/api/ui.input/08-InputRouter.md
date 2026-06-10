# InputRouter 详细接口文档

## 概述

`InputRouter` 是 `mine.ui.input` 模块的**输入事件路由器**。

**核心职责：**
- **订阅平台窗口事件**：实现 `IWindowEventSink` 接口，接收平台 `WindowEvent`
- **命中测试**：通过 `UIElement::hit_test()` 确定目标元素
- **事件转换**：将平台 `WindowEvent` 转换为路由事件（`KeyEventArgs`、`MouseEventArgs`、`TextInputEventArgs`）
- **事件派发**：调用 `EventManager::raise()` 将路由事件派发到视觉树

**核心特性：**
- **视觉树根节点管理**：维护视觉树根节点（命中测试起点）
- **键盘焦点管理**：维护当前键盘焦点元素（键盘事件目标）
- **鼠标悬停跟踪**：跟踪鼠标悬停元素，合成 `MouseEnter`/`MouseLeave` 事件
- **生命周期管理**：不持有元素所有权，调用者负责在元素销毁前调用 `set_xxx(nullptr)`

**继承关系：**
```
IWindowEventSink
    └── InputRouter
```

---

## 文件位置

```
src/mine/ui/input/api/include/mine/ui/input/InputRouter.h
```

---

## 类定义

```cpp
class MINE_UI_INPUT_API InputRouter : public platform::IWindowEventSink {
public:
    InputRouter() noexcept;
    ~InputRouter() override;

    // 禁止拷贝
    InputRouter(const InputRouter&)            = delete;
    InputRouter& operator=(const InputRouter&) = delete;

    // ── 视觉树根节点 ──────────────────────────────────────────────────────

    void set_root(UIElement* root) noexcept;
    [[nodiscard]] UIElement* root() const noexcept;

    // ── 键盘焦点 ──────────────────────────────────────────────────────────

    void set_keyboard_focus(UIElement* element) noexcept;
    [[nodiscard]] UIElement* keyboard_focus() const noexcept;

    // ── 鼠标悬停 ──────────────────────────────────────────────────────────

    [[nodiscard]] UIElement* mouse_over_element() const noexcept;

    // ── IWindowEventSink ─────────────────────────────────────────────────

    void on_window_event(platform::WindowEvent& event) override;

private:
    void dispatch_key_event(const platform::WindowEvent& we);
    void dispatch_mouse_event(const platform::WindowEvent& we);
    void dispatch_char_event(const platform::WindowEvent& we);
    void dispatch_ime_text_event(const platform::WindowEvent& we);

    UIElement* root_{nullptr};
    UIElement* keyboard_focus_{nullptr};
    UIElement* mouse_over_{nullptr};
    UIElement* prev_mouse_over_{nullptr};
};
```

---

## 构造函数与析构函数

### InputRouter

```cpp
InputRouter() noexcept;
```

**描述**：构造输入路由器。

**特征**：
- `noexcept`：保证不抛出异常
- 初始化所有成员指针为 `nullptr`

**示例**：
```cpp
#include <mine/ui/input/InputRouter.h>

using namespace mine::ui::input;

InputRouter router;
```

---

### ~InputRouter

```cpp
~InputRouter() override;
```

**描述**：析构输入路由器。

**说明**：
- 不持有元素所有权，不负责销毁元素
- 调用者应在元素销毁前调用 `set_root(nullptr)`、`set_keyboard_focus(nullptr)`

---

## 成员方法

### set_root

```cpp
void set_root(UIElement* root) noexcept;
```

**描述**：设置视觉树根节点（命中测试的起点）。

**参数**：
- `root`：视觉树根节点（可为 `nullptr` 清除根节点）

**说明**：
- `InputRouter` 不持有 `root` 的所有权
- 调用者负责在元素销毁前调用 `set_root(nullptr)`

**示例**：
```cpp
InputRouter router;
UIElement* root_element = /* 创建根元素 */;

// 设置根节点
router.set_root(root_element);

// 元素销毁前清除
router.set_root(nullptr);
```

---

### root

```cpp
[[nodiscard]] UIElement* root() const noexcept;
```

**描述**：返回当前视觉树根节点。

**返回值**：
- `UIElement*`：当前视觉树根节点（可能为 `nullptr`）

**示例**：
```cpp
UIElement* root = router.root();
if (root) {
    std::cout << "根节点存在" << std::endl;
}
```

---

### set_keyboard_focus

```cpp
void set_keyboard_focus(UIElement* element) noexcept;
```

**描述**：设置当前键盘焦点元素（键盘事件的目标）。

**参数**：
- `element`：键盘焦点元素（可为 `nullptr` 清除焦点）

**说明**：
- 触发焦点事件：
  - 向旧焦点元素派发 `LostFocusEvent`（Direct 策略）
  - 向新焦点元素派发 `GotFocusEvent`（Direct 策略）
- `InputRouter` 不持有 `element` 的所有权
- 调用者负责在元素销毁前调用 `set_keyboard_focus(nullptr)`

**示例**：
```cpp
InputRouter router;
UIElement* element = /* 创建元素 */;

// 设置键盘焦点
router.set_keyboard_focus(element);

// 元素销毁前清除焦点
router.set_keyboard_focus(nullptr);
```

---

### keyboard_focus

```cpp
[[nodiscard]] UIElement* keyboard_focus() const noexcept;
```

**描述**：返回当前键盘焦点元素。

**返回值**：
- `UIElement*`：当前键盘焦点元素（可能为 `nullptr`）

**示例**：
```cpp
UIElement* focus = router.keyboard_focus();
if (focus) {
    std::cout << "键盘焦点存在" << std::endl;
}
```

---

### mouse_over_element

```cpp
[[nodiscard]] UIElement* mouse_over_element() const noexcept;
```

**描述**：返回当前鼠标悬停元素（最近一次命中测试结果）。

**返回值**：
- `UIElement*`：当前鼠标悬停元素（可能为 `nullptr`）

**说明**：
- 每次鼠标事件到达时，通过命中测试更新悬停元素
- 悬停元素切换时，触发 `MouseLeave`/`MouseEnter` 事件

**示例**：
```cpp
UIElement* hover = router.mouse_over_element();
if (hover) {
    std::cout << "鼠标悬停在元素上" << std::endl;
}
```

---

### on_window_event

```cpp
void on_window_event(platform::WindowEvent& event) override;
```

**描述**：接收平台 `WindowEvent` 并路由为 UI 输入事件。

**参数**：
- `event`：平台窗口事件

**说明**：
- 实现 `IWindowEventSink::on_window_event()` 接口
- 根据事件类型分发到不同的私有方法：
  - `WindowEventKind::KeyDown` / `KeyUp`：调用 `dispatch_key_event()`
  - `WindowEventKind::MouseDown` / `MouseUp` / `MouseMove` / `MouseWheel`：调用 `dispatch_mouse_event()`
  - `WindowEventKind::Char`：调用 `dispatch_char_event()`
  - `WindowEventKind::IMEText`：调用 `dispatch_ime_text_event()`

**示例**：
```cpp
#include <mine/platform/Window.h>

InputRouter router;
router.set_root(root_element);

// 订阅窗口事件
window->events().subscribe(&router);

// 平台窗口事件自动转发到 router.on_window_event()
```

---

## 私有方法

### dispatch_key_event

```cpp
void dispatch_key_event(const platform::WindowEvent& we);
```

**描述**：派发键盘事件（`KeyDown` / `KeyUp`）。

**参数**：
- `we`：平台窗口事件

**说明**：
- 将 `WindowEvent` 转换为 `KeyEventArgs`
- 调用 `EventManager::raise()` 向键盘焦点元素派发事件
- 事件策略：隧道（`PreviewKeyDown`）+ 冒泡（`KeyDown`）

---

### dispatch_mouse_event

```cpp
void dispatch_mouse_event(const platform::WindowEvent& we);
```

**描述**：派发鼠标事件（`MouseDown` / `MouseUp` / `MouseMove` / `MouseWheel`）。

**参数**：
- `we`：平台窗口事件

**说明**：
- 通过命中测试确定目标元素
- 将 `WindowEvent` 转换为 `MouseEventArgs`
- 调用 `EventManager::raise()` 向目标元素派发事件
- 事件策略：隧道（`PreviewMouseDown`）+ 冒泡（`MouseDown`）
- 悬停元素切换时，合成派发 `MouseLeave`/`MouseEnter` 事件

---

### dispatch_char_event

```cpp
void dispatch_char_event(const platform::WindowEvent& we);
```

**描述**：派发字符输入事件（`TextInputEvent`，向键盘焦点派发）。

**参数**：
- `we`：平台窗口事件（`WindowEventKind::Char`）

**说明**：
- 将 `WindowEvent` 转换为 `TextInputEventArgs`
- 调用 `EventManager::raise()` 向键盘焦点元素派发事件
- 仅包含可打印字符（≥ 32）

---

### dispatch_ime_text_event

```cpp
void dispatch_ime_text_event(const platform::WindowEvent& we);
```

**描述**：派发 IME 确认提交文字（逐 UTF-32 码点转发为 `TextInputEvent`）。

**参数**：
- `we`：平台窗口事件（`WindowEventKind::IMEText`）

**说明**：
- 将 IME 提交的文本逐个 UTF-32 码点转换为 `TextInputEventArgs`
- 调用 `EventManager::raise()` 向键盘焦点元素派发事件

---

## 使用场景

### 1. 基础使用（订阅窗口事件）

```cpp
#include <mine/ui/input/InputRouter.h>
#include <mine/platform/Window.h>

using namespace mine::ui::input;
using namespace mine::platform;

int main() {
    // 创建输入路由器
    InputRouter router;
    
    // 创建根元素
    UIElement* root_element = /* 创建根元素 */;
    router.set_root(root_element);
    
    // 设置键盘焦点
    router.set_keyboard_focus(root_element);
    
    // 创建窗口
    Window* window = /* 创建窗口 */;
    
    // 订阅窗口事件（InputRouter 自动接收平台事件）
    window->events().subscribe(&router);
    
    // 事件循环
    while (window->is_open()) {
        window->poll_events();  // 触发 router.on_window_event()
    }
    
    // 清理
    router.set_keyboard_focus(nullptr);
    router.set_root(nullptr);
    
    return 0;
}
```

---

### 2. 键盘焦点管理

```cpp
class MyApplication {
public:
    MyApplication() {
        router_.set_root(&root_);
        router_.set_keyboard_focus(&textbox_);
    }
    
    void on_button_click() {
        // 点击按钮时切换焦点
        router_.set_keyboard_focus(&button_);
    }

private:
    InputRouter router_;
    UIElement root_;
    TextBox textbox_;
    Button button_;
};
```

---

### 3. 查询当前状态

```cpp
void print_input_state(const InputRouter& router) {
    // 查询根节点
    if (UIElement* root = router.root()) {
        std::cout << "根节点存在" << std::endl;
    }
    
    // 查询键盘焦点
    if (UIElement* focus = router.keyboard_focus()) {
        std::cout << "键盘焦点存在" << std::endl;
    }
    
    // 查询鼠标悬停
    if (UIElement* hover = router.mouse_over_element()) {
        std::cout << "鼠标悬停在元素上" << std::endl;
    }
}
```

---

### 4. 生命周期管理

```cpp
class MyApplication {
public:
    MyApplication() {
        router_.set_root(&root_);
        router_.set_keyboard_focus(&textbox_);
    }
    
    ~MyApplication() {
        // 销毁前清除引用
        router_.set_keyboard_focus(nullptr);
        router_.set_root(nullptr);
    }

private:
    InputRouter router_;
    UIElement root_;
    TextBox textbox_;
};
```

---

### 5. 多窗口应用

```cpp
class MultiWindowApp {
public:
    MultiWindowApp() {
        // 每个窗口独立的输入路由器
        router1_.set_root(&root1_);
        router2_.set_root(&root2_);
        
        window1_->events().subscribe(&router1_);
        window2_->events().subscribe(&router2_);
    }

private:
    InputRouter router1_;
    InputRouter router2_;
    
    UIElement root1_;
    UIElement root2_;
    
    Window* window1_;
    Window* window2_;
};
```

---

## 最佳实践

### 1. 始终设置根节点

```cpp
// ✅ 推荐：创建 InputRouter 后立即设置根节点
InputRouter router;
router.set_root(root_element);
router.set_keyboard_focus(root_element);

// ❌ 不推荐：忘记设置根节点（命中测试失败）
InputRouter router;
// ❌ 缺少 set_root()
router.set_keyboard_focus(root_element);
```

---

### 2. 元素销毁前清除引用

```cpp
// ✅ 推荐：元素销毁前清除引用
router.set_keyboard_focus(nullptr);
router.set_root(nullptr);
delete root_element;  // 安全销毁

// ❌ 不推荐：元素销毁后 InputRouter 仍持有悬挂指针
delete root_element;
// ❌ router 仍持有 root_element 的悬挂指针
```

---

### 3. 订阅窗口事件

```cpp
// ✅ 推荐：订阅窗口事件（自动接收平台事件）
window->events().subscribe(&router);

// ❌ 不推荐：手动转发事件（繁琐且易出错）
while (window->is_open()) {
    WindowEvent event = window->poll_event();
    router.on_window_event(event);  // ❌ 手动转发
}
```

---

### 4. 一个窗口一个 InputRouter

```cpp
// ✅ 推荐：每个窗口独立的 InputRouter
class Window {
public:
    Window() {
        router_.set_root(&root_);
        events_.subscribe(&router_);
    }

private:
    InputRouter router_;
    UIElement root_;
    EventBus events_;
};

// ❌ 不推荐：多个窗口共享一个 InputRouter（焦点混乱）
```

---

## 常见陷阱

### 1. 忘记设置根节点

```cpp
// ❌ 问题：忘记设置根节点
InputRouter router;
router.set_keyboard_focus(element);  // ❌ 缺少 set_root()
window->events().subscribe(&router);
// 结果：命中测试失败，鼠标事件无目标

// ✅ 解决：设置根节点
InputRouter router;
router.set_root(root_element);  // ✅ 设置根节点
router.set_keyboard_focus(element);
```

---

### 2. 元素销毁后仍持有悬挂指针

```cpp
// ❌ 问题：元素销毁后 InputRouter 仍持有悬挂指针
InputRouter router;
router.set_root(root_element);

delete root_element;  // ❌ 销毁元素
// router 仍持有 root_element 的悬挂指针

// 下次事件触发时访问悬挂指针（未定义行为）
window->poll_events();  // ❌ 崩溃或未定义行为

// ✅ 解决：元素销毁前清除引用
router.set_root(nullptr);  // ✅ 清除引用
delete root_element;
```

---

### 3. 忘记订阅窗口事件

```cpp
// ❌ 问题：忘记订阅窗口事件
InputRouter router;
router.set_root(root_element);
// ❌ 缺少 window->events().subscribe(&router)

// 结果：平台事件不会转发到 InputRouter

// ✅ 解决：订阅窗口事件
window->events().subscribe(&router);  // ✅ 订阅事件
```

---

### 4. 多个窗口共享一个 InputRouter

```cpp
// ❌ 问题：多个窗口共享一个 InputRouter
InputRouter router;
router.set_root(&root1);

window1->events().subscribe(&router);
window2->events().subscribe(&router);  // ❌ 共享 router

// 结果：焦点混乱（两个窗口共享一个焦点状态）

// ✅ 解决：每个窗口独立的 InputRouter
InputRouter router1;
InputRouter router2;

router1.set_root(&root1);
router2.set_root(&root2);

window1->events().subscribe(&router1);
window2->events().subscribe(&router2);
```

---

## 完整示例

```cpp
#include <mine/ui/input/InputRouter.h>
#include <mine/ui/input/KeyEventArgs.h>
#include <mine/ui/input/MouseEventArgs.h>
#include <mine/ui/visual/Visual.h>
#include <mine/platform/Window.h>
#include <iostream>

using namespace mine::ui::input;
using namespace mine::ui::visual;
using namespace mine::platform;

// ────────────────────────────────────────────────────────────────────────────
// 根元素（接收输入事件）
// ────────────────────────────────────────────────────────────────────────────

class RootElement : public Visual {
protected:
    void on_key_down(KeyEventArgs& args) override {
        std::cout << "[Root] KeyDown: " << static_cast<int>(args.key()) << std::endl;
        
        if (args.key() == Key::Escape) {
            std::cout << "  退出应用" << std::endl;
            args.set_handled(true);
        }
    }
    
    void on_mouse_down(MouseEventArgs& args) override {
        std::cout << "[Root] MouseDown: (" 
                  << args.position().x << ", " 
                  << args.position().y << ")" << std::endl;
    }
    
    void on_got_focus(RoutedEventArgs& args) override {
        std::cout << "[Root] GotFocus" << std::endl;
    }
    
    void on_lost_focus(RoutedEventArgs& args) override {
        std::cout << "[Root] LostFocus" << std::endl;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// 应用程序
// ────────────────────────────────────────────────────────────────────────────

class Application {
public:
    Application() {
        // 创建窗口
        window_ = Window::create(800, 600, "InputRouter 示例");
        
        // 创建根元素
        root_ = new RootElement();
        
        // 创建输入路由器
        router_.set_root(root_);
        router_.set_keyboard_focus(root_);
        
        // 订阅窗口事件
        window_->events().subscribe(&router_);
    }
    
    ~Application() {
        // 清理资源
        router_.set_keyboard_focus(nullptr);
        router_.set_root(nullptr);
        delete root_;
    }
    
    void run() {
        std::cout << "=== 应用启动 ===" << std::endl;
        std::cout << "按 Escape 退出" << std::endl;
        
        // 事件循环
        while (window_->is_open()) {
            // 拉取平台事件（触发 router_.on_window_event()）
            window_->poll_events();
            
            // 渲染
            window_->swap_buffers();
        }
        
        std::cout << "=== 应用退出 ===" << std::endl;
    }
    
    void print_state() const {
        std::cout << "=== InputRouter 状态 ===" << std::endl;
        
        if (UIElement* root = router_.root()) {
            std::cout << "根节点: 存在" << std::endl;
        } else {
            std::cout << "根节点: 不存在" << std::endl;
        }
        
        if (UIElement* focus = router_.keyboard_focus()) {
            std::cout << "键盘焦点: 存在" << std::endl;
        } else {
            std::cout << "键盘焦点: 不存在" << std::endl;
        }
        
        if (UIElement* hover = router_.mouse_over_element()) {
            std::cout << "鼠标悬停: 存在" << std::endl;
        } else {
            std::cout << "鼠标悬停: 不存在" << std::endl;
        }
    }

private:
    Window* window_{nullptr};
    InputRouter router_;
    RootElement* root_{nullptr};
};

// ────────────────────────────────────────────────────────────────────────────
// 主函数
// ────────────────────────────────────────────────────────────────────────────

int main() {
    Application app;
    
    // 打印初始状态
    app.print_state();
    // 输出：
    // === InputRouter 状态 ===
    // 根节点: 存在
    // 键盘焦点: 存在
    // 鼠标悬停: 不存在
    
    // 运行应用
    app.run();
    
    return 0;
}
```

**使用说明**：
1. 创建窗口（`Window::create()`）
2. 创建根元素（`RootElement`）
3. 创建输入路由器并设置根节点和焦点（`router_.set_root()` / `set_keyboard_focus()`）
4. 订阅窗口事件（`window_->events().subscribe(&router_)`）
5. 事件循环（`window_->poll_events()`）
6. 清理资源（`set_keyboard_focus(nullptr)` / `set_root(nullptr)`）

**运行效果**：
- 按键时触发 `[Root] KeyDown: <key>`
- 鼠标点击时触发 `[Root] MouseDown: (x, y)`
- 按 Escape 键退出应用

---

## 总结

`InputRouter` 是 `mine.ui.input` 模块的输入事件路由器，具备：

1. **订阅平台窗口事件**：实现 `IWindowEventSink` 接口，接收平台 `WindowEvent`
2. **命中测试**：通过 `UIElement::hit_test()` 确定目标元素
3. **事件转换**：将平台 `WindowEvent` 转换为路由事件
4. **事件派发**：调用 `EventManager::raise()` 将路由事件派发到视觉树

**核心方法**：
- `set_root()` / `root()`：管理视觉树根节点
- `set_keyboard_focus()` / `keyboard_focus()`：管理键盘焦点
- `mouse_over_element()`：查询鼠标悬停元素
- `on_window_event()`：接收平台窗口事件

**使用建议**：
- **始终设置根节点**（命中测试起点）
- **元素销毁前清除引用**（避免悬挂指针）
- **订阅窗口事件**（自动接收平台事件）
- **一个窗口一个 InputRouter**（避免焦点混乱）
- 避免忘记设置根节点
- 避免元素销毁后仍持有悬挂指针
- 避免忘记订阅窗口事件
- 避免多个窗口共享一个 InputRouter
