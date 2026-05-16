# 07 — 窗体与平台抽象

## 7.1 平台抽象层（PAL）

`mine.platform.abi` 提供如下抽象接口（节选）：

```cpp
namespace mine::platform {

class IApplicationHost {        // 进程级
    virtual void run() = 0;
    virtual void post_to_main(Action) = 0;
    virtual IClipboard&  clipboard() = 0;
    virtual IScreenManager& screens() = 0;
    virtual IMEService& ime() = 0;
};

class IWindow {                 // 单窗口
    virtual void show() = 0;
    virtual void close() = 0;
    virtual void set_title(StringView) = 0;
    virtual void set_size(Size) = 0;
    virtual void set_position(Point) = 0;
    virtual NativeHandle native_handle() const = 0;     // 不透明
    virtual Signal<WindowEvent>& events() = 0;
};

struct WindowDesc {
    StringView title;
    Size       size;
    Point      position;        // optional
    bool       resizable = true;
    bool       frameless = false;
    bool       transparent = false;
    bool       always_on_top = false;
    bool       startup_hidden = false;
    WindowKind kind = WindowKind::Normal;  // Normal/Tool/Dialog/Splash
};
}
```

`NativeHandle` 是 ABI 安全的 union（`{HWND, NSWindow*, ANativeWindow*, ...}`），仅平台后端解读。

## 7.2 Win32 实现要点

* 自管 `WindowClass`（一类一窗口风格），`DefWindowProc` 兜底。
* 处理 DPI per-monitor v2，`WM_DPICHANGED` 实时更新窗口与子树缩放。
* 处理 `WM_NCCALCSIZE` 实现自定义标题栏（无边框、阴影、Aero Snap、最大化裁剪）。
* IME：`ImmGetContext` 接管，转发到 `ui.input` 的 IME 事件流。
* 多显示器：`MonitorEnumProc` 提供 `IScreenManager`。

## 7.3 macOS / iOS

* `NSWindow` / `UIWindow`，`CAMetalLayer` 给 RHI。
* Retina：原生 backing scale。
* 输入：`NSEvent`/UITouch 路由到统一事件结构。

## 7.4 Linux

* 双后端：X11（XCB）、Wayland（xdg-shell）。运行时探测优先 Wayland。
* 输入：`libinput` 或后端原生事件。
* DPI：`Xft.dpi` / `wl_output.scale`。

## 7.5 移动端注意

* Android：`NativeActivity` 主循环 + JNI 调用 IME / 通知 / 系统对话框。
* iOS：`UIViewController` 持有 `UIView` 桥到 MineUI 的 `Visual` 根。

## 7.6 多窗口

* `Application` 拥有 `WindowManager`，支持多主窗口、子窗口、模态窗口。
* 每窗口独立**渲染线程**与**输入泵**，UI 逻辑线程默认共享主线程；可设置 per-window UI 线程（高级模式）。
* 窗口间共享：资源系统（字体/图集/纹理）、IoC 服务、设置。

## 7.7 生命周期

```
Application::Construct → Bootstrap (DI/资源/i18n)
                       → OnStartup(args)
                       → MainWindow::Show
                       → MessageLoop
                       → OnExit
```

* 单实例：`mine.ui.app::SingleInstance` 提供命名互斥 + IPC 唤醒。
* 进程崩溃/未处理异常 → `IUnhandledExceptionHandler`，可对接 minidump 上报。

## 7.8 自定义窗口形态

* **无边框 + 自绘标题栏**：通过 PAL 提供的"NCHitTest 预测函数"接入。
* **透明/异形窗**：DWM/Compositor 透明 + Visual 树透明导出。
* **嵌入到外部 HWND/NSView**：`Application::AttachToHost(NativeHandle)`。
