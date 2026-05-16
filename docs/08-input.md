# 08 — 输入与事件

## 8.1 事件统一模型

`mine.ui.input` 定义统一事件结构：

```cpp
struct InputEvent {
    enum class Kind { KeyDown, KeyUp, Char, MouseMove, MouseDown, MouseUp,
                      MouseWheel, TouchBegin, TouchMove, TouchEnd, PenBegin,
                      PenMove, PenEnd, PenHover, GestureTap, GesturePinch,
                      GesturePan, IMEComposition, IMEEnd, FocusIn, FocusOut, ... };
    Kind kind;
    uint64_t timestamp;     // 单调毫秒
    InputDeviceId device;
    Modifiers modifiers;
    Variant<KeyData, MouseData, TouchData, PenData, GestureData, IMEData> data;
};
```

PAL 把平台事件转换为 `InputEvent`，再交给 `InputRouter`。

## 8.2 路由事件（Routed Events）

参考 WPF：

* **隧道（Preview）**：从根向命中元素逐级派发，`PreviewKeyDown`、`PreviewMouseDown` 等。
* **冒泡（Bubble）**：从命中元素向上派发。
* 任意阶段可调用 `Handled = true` 终止后续传播。
* 路由事件在 MML 中以 `attached event` 语法订阅：

```mml
Button { @event:Click => vm.OnClick(); }
```

## 8.3 命令系统

* `ICommand`：`bool can_execute(const Variant&)` / `void execute(const Variant&)` / `Signal<> can_execute_changed`。
* `RelayCommand`：std::function 包装。
* `AsyncCommand`：返回 `Task<void>`，自动维护 `IsExecuting` 属性。
* MML 中：

```mml
Button { command: vm.SaveCommand; commandParameter: vm.CurrentItem; }
```

## 8.4 焦点

* `FocusManager`：管理逻辑/键盘/IME 焦点。
* `Focusable`、`TabIndex`、`TabStop`、`FocusVisualStyle`。
* 屏幕键盘焦点：移动端通过 PAL IME 服务上报。

## 8.5 快捷键 / 输入手势

* `KeyBinding { gesture: "Ctrl+S"; command: vm.SaveCommand; }`
* 由 `InputManager` 全窗口订阅；可作用域到 `Window` / `UserControl`。

## 8.6 命中测试

* 按合成树（包含变换、裁剪）反向遍历。
* 每节点提供 `bool hit_test(Point local)`，控件可重写自定义形状。
* 命中结果含路径，方便事件路由。

## 8.7 手势识别

* 内置识别器：Tap/DoubleTap/LongPress/Pan/Pinch/Rotate/Swipe。
* 可注册自定义 `IGestureRecognizer`，参与命中元素的事件流。

## 8.8 输入与 MVVM

事件 → 命令 → ViewModel：

```mml
TextBox { keyDown(e) => if (e.key == Enter) vm.SearchCommand.execute(text); }
```

事件体支持参数（`e` 隐式），表达式语言子集（不允许循环/任意函数）。
