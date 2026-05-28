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

## 7.9 Window 与 MML 集成（Window 组件模式）

### 两种层次的区分

| 层次 | 类型 | 角色 |
|------|------|------|
| 基础设施 | `mine::ui::Window` | 平台窗口 + 渲染管线的值类型封装（Pimpl，不可派生） |
| 视图层（MML） | `DemoWindow`（mmlc 生成） | 持有 `Window` 值成员的包装类（组合模式） |

`mine::ui::Window` 是纯粹的基础设施：它封装原生窗口句柄、交换链、渲染队列，是 Pimpl 值类型，无虚函数，**不可被派生**。

MML 视图层通过 `component X : Window` 声明一个**顶层窗口组件**，mmlc 生成**包装类**（不是子类）。
这与各框架的对比：

| 框架 | MML/XAML 声明 | C++ / 实现侧 | 是否继承 |
|------|-------------|------------|---------|
| WPF | `MainWindow : Window`（XAML） | 派生自 `System.Windows.Window` | ✅ 继承 |
| WinUI 3 | `MainWindow : Window`（XAML） | 实践上组合（WinUI Window 已接近 sealed） | ⚠ 实质组合 |
| QML | `ApplicationWindow { }` | 包含 `QQuickWindow` | ✅ 组合 |
| Slint | `component X inherits Window` | Rust 侧生成包装结构体 | ✅ 组合 |
| **MineUI** | `component X : Window` | mmlc 生成持有 `Window` 值成员的包装类 | ✅ **组合** |

### 生命周期与 IWindowContext

当用户在 `Application::on_startup` 中调用 `DemoWindow::show()` 时，流程如下：

```
DemoWindow::show()
  → win_.show()                        // mine::ui::Window::show()
    → Window::Impl::initialize()       // 首次调用时懒初始化
      → IWindowContext::create_native_window(desc)   // 创建原生窗口
      → IWindowContext::on_window_first_show(win_)   // Application 注册主窗口 + 渲染回调
      → 原生窗口显示
```

`IWindowContext` 由 `Application::run()` 创建并注册，负责：
1. 通过 PAL 创建原生窗口
2. 将新窗口登记到 `Application` 的窗口列表（首个自动成为主窗口）
3. 设置渲染回调（`set_on_input_processed`）

### 正确的使用模式

```cpp
// DemoApp.cpp（手写薄壳，10-15 行）
struct DemoApp : mine::ui::app::Application {
    // Window 组件包装类声明为值成员（非指针）
    app::DemoWindow main_win_;

    void on_startup(int, char**) override {
        // 订阅信号
        main_win_.closeRequested().connect([this] { quit(); });
        // show() 触发懒初始化，自动通过 IWindowContext 注册
        main_win_.show();
    }
};
```

**注意**：`DemoWindow main_win_` 应声明为 `DemoApp` 的**值成员**（不是指针或局部变量），使其生命周期与 Application 绑定，`Application` 析构时自动调用 `~DemoWindow()` → `win_` 最先析构（GPU 资源释放）→ UI 元素随后析构。

### 析构安全顺序保证

`DemoWindow` 内部的数据成员顺序（mmlc 严格生成）：

```cpp
class DemoWindow {
    // 1. UI 元素（先声明 = 后析构）
    ::mine::ui::Grid    root_grid_;
    ::mine::ui::TextBlock title_bar_;
    // ...

    // 2. win_ 最后声明 = 最先析构
    //    析构时：swapchain 释放 → 原生窗口销毁 → IWindowContext 解注册
    ::mine::ui::Window  win_;
};
```

这确保在 `win_` 的渲染循环停止（`~Window()` 内同步）之后，所有 `UIElement` 才开始析构——避免析构期间的绘制回调访问已销毁对象。
