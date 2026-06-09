# mine.ui.input —— 输入事件路由模块

## 模块概述

`mine.ui.input` 负责将平台原始窗口事件（`WindowEvent`）转换为 UI 路由事件（`MouseEnter`、`KeyDown` 等），通过 `InputRouter` 进行命中测试确定目标元素并派发。

| 核心类型 | 职责 |
|---------|------|
| `InputRouter` | 接收平台 `WindowEvent`，命中测试确定目标，派发路由事件 |
| `InputEvents` | 标准输入事件注册（`MouseEnterEvent()` 等全局函数） |
| `MouseEventArgs` | 鼠标事件参数（位置、按键、修饰键、滚轮增量） |
| `KeyEventArgs` | 键盘事件参数（键码、扫描码、修饰键、重复标志） |
| `TextInputEventArgs` | 文本输入事件参数（UTF-32 码点） |

**依赖**：`mine.core`、`mine.math`、`mine.ui.event`、`mine.ui.visual`、`mine.platform`。

---

## 1. 标准输入事件 —— `InputEvents`

**文件**：`<mine/ui/input/InputEvents.h>`

每类事件一个全局函数，返回 `const RoutedEvent&`（Meyer's 单例）。

### 鼠标事件

| 函数 | 策略 | 说明 |
|------|------|------|
| `MouseEnterEvent()` | `Direct` | 鼠标进入元素 |
| `MouseLeaveEvent()` | `Direct` | 鼠标离开元素 |
| `MouseMoveEvent()` | `Bubble` | 鼠标移动 |
| `MouseDownEvent()` | `Bubble` | 鼠标按下 |
| `MouseUpEvent()` | `Bubble` | 鼠标释放 |
| `MouseWheelEvent()` | `Bubble` | 鼠标滚轮 |
| `PreviewMouseDownEvent()` | `Tunnel` | 鼠标按下（隧道预览） |
| `PreviewMouseUpEvent()` | `Tunnel` | 鼠标释放（隧道预览） |

### 键盘事件

| 函数 | 策略 | 说明 |
|------|------|------|
| `KeyDownEvent()` | `Bubble` | 键按下 |
| `KeyUpEvent()` | `Bubble` | 键释放 |
| `PreviewKeyDownEvent()` | `Tunnel` | 键按下（隧道预览） |
| `PreviewKeyUpEvent()` | `Tunnel` | 键释放（隧道预览） |
| `TextInputEvent()` | `Bubble` | 字符输入（UTF-32） |

### 焦点事件

| 函数 | 策略 | 说明 |
|------|------|------|
| `GotFocusEvent()` | `Direct` | 获得键盘焦点 |
| `LostFocusEvent()` | `Direct` | 失去键盘焦点 |

---

## 2. InputRouter —— 输入路由器

**文件**：`<mine/ui/input/InputRouter.h>`

实现 `IWindowEventSink` 接口，订阅平台窗口事件。

### 类摘要

```cpp
namespace mine::ui::input {

class InputRouter : public platform::IWindowEventSink {
public:
    InputRouter() noexcept;
    ~InputRouter() override;

    // ── 视觉树 ──────────────────────────────────────────────────────────
    void set_root(UIElement* root) noexcept;
    UIElement* root() const noexcept;

    // ── 键盘焦点 ────────────────────────────────────────────────────────
    void set_keyboard_focus(UIElement* element) noexcept;
    UIElement* keyboard_focus() const noexcept;

    // ── 鼠标悬停 ────────────────────────────────────────────────────────
    UIElement* mouse_over_element() const noexcept;

    // ── IWindowEventSink ───────────────────────────────────────────────
    void on_window_event(platform::WindowEvent& event) override;
};

} // namespace mine::ui::input
```

### 事件派发流程

```
平台 WindowEvent
  → on_window_event()
    ├─ KeyDown/KeyUp      → dispatch_key_event()
    │   └─ 目标 = keyboard_focus（或 root）
    │       → EventManager::raise(target, PreviewKeyDown/KeyDown)
    │
    ├─ MouseMove/Down/Up  → dispatch_mouse_event()
    │   ├─ hit_test(pos) → 确定 target
    │   ├─ target != prev → 合成 MouseLeave(prev) + MouseEnter(target)
    │   ├─ MouseDown → PreviewMouseDown(Tunnel) + MouseDown(Bubble)
    │   │              若 target 可聚焦 → set_keyboard_focus(target)
    │   ├─ MouseUp   → PreviewMouseUp(Tunnel) + MouseUp(Bubble)
    │   └─ MouseMove → MouseMove(Bubble)
    │
    └─ Char / IME         → dispatch_char_event / dispatch_ime_text_event
        └─ 目标 = keyboard_focus（或 root）
            → EventManager::raise(target, TextInputEvent)
```

---

## 3. MouseEventArgs —— 鼠标事件参数

**文件**：`<mine/ui/input/MouseEventArgs.h>`

```cpp
namespace mine::ui::input {

enum class MouseButton : uint8_t {
    None   = 0,
    Left   = 1,
    Right  = 2,
    Middle = 3,
    X1     = 4,
    X2     = 5,
};

class MouseEventArgs : public RoutedEventArgs {
public:
    MouseEventArgs(const RoutedEvent& ev, MouseButton btn,
                   Point pos, ModifierKeys mods,
                   int wheel_delta = 0) noexcept;

    MouseButton  button()       const noexcept;
    Point        position()     const noexcept;
    ModifierKeys modifiers()    const noexcept;
    int          wheel_delta()  const noexcept;
};

} // namespace mine::ui::input
```

---

## 4. KeyEventArgs —— 键盘事件参数

**文件**：`<mine/ui/input/KeyEventArgs.h>`

```cpp
namespace mine::ui::input {

class KeyEventArgs : public RoutedEventArgs {
public:
    KeyEventArgs(const RoutedEvent& ev, Key key,
                 uint32_t scan_code, ModifierKeys mods,
                 bool is_repeat) noexcept;

    Key          key()         const noexcept;
    uint32_t     scan_code()   const noexcept;
    ModifierKeys modifiers()   const noexcept;
    bool         is_repeat()   const noexcept;
};

} // namespace mine::ui::input
```

---

## 5. 典型输入事件流

```
用户移动鼠标到 CheckBox
  → WM_MOUSEMOVE → WindowEvent(kind=MouseMove, x=..., y=...)
  → InputRouter::dispatch_mouse_event()
    → hit_test((150, 340)) → CheckBox*  ← StackPanel 已 set_hit_transparent
    → prev_mouse_over == nullptr → 合成 MouseEnter
      → EventManager::raise(*checkBox, MouseEnter)
        → Direct 策略
        → checkBox.invoke_handlers(MouseEnter, args)
          → CheckBox::on_mouse_enter_router()
            → is_hovered_ = true
            → update_visual_state()
              → vsm()->go_to_state("Hovered")
                → 启动 StateLayer + IconBorder 动画

用户左键点击
  → WM_LBUTTONDOWN → dispatch_mouse_event()
    → PreviewMouseDown(Tunnel) + MouseDown(Bubble)
    → CheckBox::on_mouse_down_router()
      → is_pressed_ = true, update_visual_state() → "Pressed"

  → WM_LBUTTONUP → dispatch_mouse_event()
    → PreviewMouseUp(Tunnel) + MouseUp(Bubble)
    → CheckBox::on_mouse_up_router()
      → set_checked(!is_checked())  → on_is_checked_changed
        → ivsm->stop_all_storyboards()
        → chk_vsm->go_to_state("Checked")
      → is_pressed_ = false
      → update_visual_state() → "Hovered"（鼠标仍在控件上）
        → on_visual_state_changed → chk_vsm->stop_all_storyboards()
```

---

## 相关模块

| 模块 | 关系 |
|------|------|
| `mine.platform` | 提供 `WindowEvent` 和 `IWindowEventSink` 接口 |
| `mine.ui.event` | `InputRouter` 通过 `EventManager::raise` 派发路由事件 |
| `mine.ui.visual` | `hit_test` 确定命中目标 |
