# IApplicationHost 详细接口文档

## 概述

`IApplicationHost` 是 `mine.platform.abi` 模块的**进程级应用程序宿主接口**，管理主消息/事件循环与核心平台服务。

**核心特性：**
- **9 个方法**：run、quit、create_window、clipboard、screens、ime、start_frame_timer、stop_frame_timer
- **消息循环**：`run()` 进入平台主消息循环，阻塞直到调用 `quit()`
- **窗口创建**：`create_window()` 创建窗口，返回 `OwnedPtr<IWindow>`
- **核心服务**：剪贴板、显示器枚举、IME 服务
- **帧定时器**：动画驱动定时器

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/IApplicationHost.h
```

---

## 类型定义

```cpp
namespace mine::platform {

class IApplicationHost {
public:
    virtual ~IApplicationHost() = default;
    
    /**
     * @brief 进入平台主消息循环，阻塞直到调用 quit()。
     * @return 退出码（通常为 0 表示正常退出）
     */
    virtual int run() = 0;
    
    /**
     * @brief 请求退出消息循环。
     * @param exit_code 传递给 run() 的返回值
     * @note  可在任意线程调用（线程安全）
     */
    virtual void quit(int exit_code = 0) = 0;
    
    /**
     * @brief 创建一个新窗口。
     * @param desc 窗口创建参数
     * @return 成功时返回持有 IWindow 的 OwnedPtr；失败时返回空指针
     */
    [[nodiscard]] virtual core::OwnedPtr<IWindow> create_window(
        const WindowDesc& desc) = 0;
    
    /// 获取系统剪贴板接口（生命周期与宿主相同）
    [[nodiscard]] virtual IClipboard& clipboard() = 0;
    
    /// 获取多显示器管理接口（生命周期与宿主相同）
    [[nodiscard]] virtual IScreenManager& screens() = 0;
    
    /// 获取 IME 服务接口（生命周期与宿主相同）
    [[nodiscard]] virtual IMEService& ime() = 0;
    
    /**
     * @brief 以指定间隔（毫秒）启动帧定时器，每次到期时回调 on_frame_tick()。
     *
     * 幂等：若定时器已启动则静默忽略（调用方无需判断是否已启动）。
     *
     * @param interval_ms  期望间隔（毫秒，推荐 8）
     * @param on_frame_tick  每帧回调（不可为 nullptr，生命周期须长于定时器）
     * @param user_data    透传给回调的用户数据指针
     */
    virtual void start_frame_timer(
        unsigned int       interval_ms,
        void             (*on_frame_tick)(void* user_data),
        void*              user_data) = 0;
    
    /**
     * @brief 停止帧定时器。
     *
     * 幂等：若定时器未启动则静默忽略。
     */
    virtual void stop_frame_timer() = 0;
};

}
```

---

## 成员方法

### run()

```cpp
virtual int run() = 0;
```

**描述**：进入平台主消息循环，阻塞直到调用 `quit()`。

**返回**：退出码（通常为 0 表示正常退出）

**特征**：
- 阻塞调用
- 进入平台主消息循环（处理窗口事件、输入事件等）
- 直到调用 `quit()` 时返回
- 返回值为 `quit()` 传递的退出码

**示例**：
```cpp
auto host = platform::create_application_host();

WindowDesc desc;
desc.title = "My Application";
auto window = host->create_window(desc);
window->show();

// 进入消息循环
int exit_code = host->run();  // 阻塞
log("程序退出，退出码: {}", exit_code);
return exit_code;
```

---

### quit()

```cpp
virtual void quit(int exit_code = 0) = 0;
```

**描述**：请求退出消息循环。

**参数**：
- `exit_code`：传递给 `run()` 的返回值（默认 0）

**特征**：
- 可在任意线程调用（线程安全）
- 请求退出消息循环
- `run()` 将返回指定的退出码

**示例**：
```cpp
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            host_->quit(0);  // 正常退出
        }
    }
};
```

---

### create_window()

```cpp
[[nodiscard]] virtual core::OwnedPtr<IWindow> create_window(
    const WindowDesc& desc) = 0;
```

**描述**：创建一个新窗口。

**参数**：
- `desc`：窗口创建参数

**返回**：
- 成功时返回持有 `IWindow` 的 `OwnedPtr`
- 失败时返回空指针

**特征**：
- 返回 `OwnedPtr<IWindow>`（调用方拥有生命周期）
- 窗口创建后默认隐藏，需调用 `show()` 显示

**示例**：
```cpp
WindowDesc desc;
desc.title = "My Application";
desc.size = {800, 600};
desc.position = {100, 100};
desc.auto_position = false;

auto window = host->create_window(desc);
if (window) {
    window->show();
} else {
    log("窗口创建失败");
}
```

---

### clipboard()

```cpp
[[nodiscard]] virtual IClipboard& clipboard() = 0;
```

**描述**：获取系统剪贴板接口（生命周期与宿主相同）。

**返回**：剪贴板接口引用

**示例**：
```cpp
IClipboard& clipboard = host->clipboard();
clipboard.set_text("Hello, World!");
```

---

### screens()

```cpp
[[nodiscard]] virtual IScreenManager& screens() = 0;
```

**描述**：获取多显示器管理接口（生命周期与宿主相同）。

**返回**：显示器管理接口引用

**示例**：
```cpp
IScreenManager& screens = host->screens();
ScreenInfo primary = screens.primary_screen();
log("主显示器: {}x{}", primary.bounds.width, primary.bounds.height);
```

---

### ime()

```cpp
[[nodiscard]] virtual IMEService& ime() = 0;
```

**描述**：获取 IME 服务接口（生命周期与宿主相同）。

**返回**：IME 服务接口引用

**示例**：
```cpp
IMEService& ime = host->ime();
math::Rect composition_rect = {10, 10, 200, 30};
ime.enable(composition_rect);
```

---

### start_frame_timer()

```cpp
virtual void start_frame_timer(
    unsigned int       interval_ms,
    void             (*on_frame_tick)(void* user_data),
    void*              user_data) = 0;
```

**描述**：以指定间隔（毫秒）启动帧定时器，每次到期时回调 `on_frame_tick()`。

**参数**：
- `interval_ms`：期望间隔（毫秒，推荐 8）
- `on_frame_tick`：每帧回调（不可为 `nullptr`，生命周期须长于定时器）
- `user_data`：透传给回调的用户数据指针

**特征**：
- 幂等：若定时器已启动则静默忽略
- 推荐间隔 8ms（确保 60Hz 每帧至少触发一次）
- 回调在主线程执行

**示例**：
```cpp
void on_frame_tick(void* user_data) {
    Application* app = static_cast<Application*>(user_data);
    app->update_animation();
}

Application app;
host->start_frame_timer(8, on_frame_tick, &app);
```

---

### stop_frame_timer()

```cpp
virtual void stop_frame_timer() = 0;
```

**描述**：停止帧定时器。

**特征**：
- 幂等：若定时器未启动则静默忽略

**示例**：
```cpp
host->stop_frame_timer();
```

---

## 使用场景

### 1. 基本应用程序

```cpp
int main() {
    // 创建宿主
    auto host = platform::create_application_host();
    
    // 创建窗口
    WindowDesc desc;
    desc.title = "My Application";
    desc.size = {800, 600};
    auto window = host->create_window(desc);
    window->show();
    
    // 进入消息循环
    int exit_code = host->run();
    return exit_code;
}
```

---

### 2. 窗口关闭时退出

```cpp
class Application : public IWindowEventSink {
    IApplicationHost* host_;
    IWindow* window_;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        WindowDesc desc;
        desc.title = "My Application";
        auto window_ptr = host_->create_window(desc);
        window_ = window_ptr.release();
        window_->events().subscribe(this);
        window_->show();
    }
    
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            host_->quit(0);  // 正常退出
        }
    }
};

int main() {
    auto host = platform::create_application_host();
    Application app(host.get());
    return host->run();
}
```

---

### 3. 多窗口应用

```cpp
class Application {
    IApplicationHost* host_;
    std::vector<core::OwnedPtr<IWindow>> windows_;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        // 创建多个窗口
        for (int i = 0; i < 3; ++i) {
            WindowDesc desc;
            desc.title = std::format("窗口 {}", i + 1);
            desc.position = {100 + i * 50, 100 + i * 50};
            auto window = host_->create_window(desc);
            window->show();
            windows_.push_back(std::move(window));
        }
    }
};

int main() {
    auto host = platform::create_application_host();
    Application app(host.get());
    return host->run();
}
```

---

### 4. 剪贴板操作

```cpp
int main() {
    auto host = platform::create_application_host();
    
    IClipboard& clipboard = host->clipboard();
    clipboard.set_text("Hello, World!");
    log("已写入剪贴板");
    
    if (clipboard.has_text()) {
        // 读取剪贴板文本
        size_t required_size = 0;
        clipboard.get_text(nullptr, 0, &required_size);
        std::vector<char> buffer(required_size + 1);
        size_t actual_size = 0;
        clipboard.get_text(buffer.data(), buffer.size(), &actual_size);
        buffer[actual_size] = '\0';
        log("剪贴板文本: {}", buffer.data());
    }
    
    return 0;
}
```

---

### 5. 多显示器布局

```cpp
int main() {
    auto host = platform::create_application_host();
    
    IScreenManager& screens = host->screens();
    int count = screens.screen_count();
    log("检测到 {} 个显示器", count);
    
    for (int i = 0; i < count; ++i) {
        ScreenInfo screen = screens.screen_at(i);
        log("显示器 {}: {}x{}, DPI={}", 
            i, screen.bounds.width, screen.bounds.height, screen.dpi);
    }
    
    return 0;
}
```

---

### 6. 帧定时器动画

```cpp
class Application {
    IApplicationHost* host_;
    float rotation_ = 0.0f;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        // 启动帧定时器
        host_->start_frame_timer(8, on_frame_tick, this);
    }
    
    ~Application() {
        // 停止帧定时器
        host_->stop_frame_timer();
    }
    
    void update_animation() {
        rotation_ += 1.0f;
        if (rotation_ >= 360.0f) {
            rotation_ -= 360.0f;
        }
        // 重绘窗口
    }
    
private:
    static void on_frame_tick(void* user_data) {
        Application* app = static_cast<Application*>(user_data);
        app->update_animation();
    }
};
```

---

## 线程安全

### run() 主线程调用

**特征**：
- `run()` 须在主线程调用
- 进入消息循环后阻塞

**示例**：
```cpp
int main() {
    auto host = platform::create_application_host();
    // ... 创建窗口
    return host->run();  // 主线程阻塞
}
```

---

### quit() 线程安全

**特征**：
- `quit()` 可在任意线程调用（线程安全）
- 请求退出消息循环

**示例**：
```cpp
std::thread worker([host]() {
    // 后台工作完成后退出
    do_work();
    host->quit(0);  // 线程安全
}).detach();
```

---

### create_window() 主线程调用

**特征**：
- `create_window()` 须在主线程调用
- 窗口创建在主线程

**示例**：
```cpp
// ✅ 主线程调用
auto window = host->create_window(desc);

// ❌ 后台线程调用（未定义行为）
std::thread([host, desc]() {
    auto window = host->create_window(desc);  // 崩溃
}).detach();
```

---

## 平台实现

### Windows

```cpp
class ApplicationHost_Win32 : public IApplicationHost {
public:
    int run() override {
        MSG msg{};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return static_cast<int>(msg.wParam);
    }
    
    void quit(int exit_code) override {
        PostQuitMessage(exit_code);
    }
    
    core::OwnedPtr<IWindow> create_window(const WindowDesc& desc) override {
        // 创建 Win32 窗口
        HWND hwnd = CreateWindowEx(...);
        return make_owned<Window_Win32>(hwnd);
    }
};
```

---

### macOS

```objc
class ApplicationHost_macOS : public IApplicationHost {
public:
    int run() override {
        [NSApp run];
        return exit_code_;
    }
    
    void quit(int exit_code) override {
        exit_code_ = exit_code;
        [NSApp stop:nil];
    }
    
    core::OwnedPtr<IWindow> create_window(const WindowDesc& desc) override {
        // 创建 NSWindow
        NSWindow* window = [[NSWindow alloc] init...];
        return make_owned<Window_macOS>(window);
    }
};
```

---

### Linux (X11)

```cpp
class ApplicationHost_X11 : public IApplicationHost {
public:
    int run() override {
        XEvent event;
        while (!quit_requested_) {
            XNextEvent(display_, &event);
            dispatch_event(event);
        }
        return exit_code_;
    }
    
    void quit(int exit_code) override {
        exit_code_ = exit_code;
        quit_requested_ = true;
    }
    
    core::OwnedPtr<IWindow> create_window(const WindowDesc& desc) override {
        // 创建 X11 窗口
        Window window = XCreateWindow(...);
        return make_owned<Window_X11>(window);
    }
};
```

---

## 最佳实践

### 1. 使用 OwnedPtr 管理窗口生命周期

```cpp
// ✅ 推荐：使用 OwnedPtr
auto window = host->create_window(desc);
// 自动管理生命周期

// ❌ 不推荐：裸指针
IWindow* window = host->create_window(desc).release();
// 需要手动管理生命周期
```

---

### 2. 窗口关闭时退出

```cpp
// ✅ 推荐：窗口关闭时退出
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        host->quit(0);
    }
}

// ❌ 不推荐：忘记退出
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        // 忘记退出，消息循环继续运行
    }
}
```

---

### 3. 检查窗口创建是否成功

```cpp
// ✅ 推荐：检查窗口创建是否成功
auto window = host->create_window(desc);
if (!window) {
    log("窗口创建失败");
    return -1;
}

// ❌ 不推荐：不检查（可能空指针）
auto window = host->create_window(desc);
window->show();  // 可能崩溃
```

---

### 4. 停止帧定时器

```cpp
// ✅ 推荐：停止帧定时器
~Application() {
    host->stop_frame_timer();
}

// ❌ 不推荐：忘记停止
~Application() {
    // 忘记停止帧定时器，可能内存泄漏
}
```

---

## 常见陷阱

### 1. 后台线程调用 create_window()

```cpp
// ❌ 问题：后台线程调用
std::thread([host, desc]() {
    auto window = host->create_window(desc);  // 崩溃
}).detach();

// ✅ 解决：主线程调用
auto window = host->create_window(desc);
```

---

### 2. 窗口关闭后未退出

```cpp
// ❌ 问题：窗口关闭后未退出
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        // 忘记退出
    }
}
// 消息循环继续运行

// ✅ 解决：窗口关闭时退出
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        host->quit(0);
    }
}
```

---

### 3. 忘记停止帧定时器

```cpp
// ❌ 问题：忘记停止帧定时器
~Application() {
    // 忘记停止
}
// 可能内存泄漏

// ✅ 解决：停止帧定时器
~Application() {
    host->stop_frame_timer();
}
```

---

### 4. 窗口创建失败未检查

```cpp
// ❌ 问题：窗口创建失败未检查
auto window = host->create_window(desc);
window->show();  // 可能崩溃

// ✅ 解决：检查窗口创建是否成功
auto window = host->create_window(desc);
if (window) {
    window->show();
}
```

---

## 完整示例

```cpp
#include <mine/platform/PlatformHost.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IWindow.h>

class Application : public IWindowEventSink {
    IApplicationHost* host_;
    IWindow* window_;
    float rotation_ = 0.0f;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        // 创建窗口
        WindowDesc desc;
        desc.title = "My Application";
        desc.size = {800, 600};
        auto window_ptr = host_->create_window(desc);
        if (!window_ptr) {
            log("窗口创建失败");
            return;
        }
        
        window_ = window_ptr.release();
        window_->events().subscribe(this);
        window_->show();
        
        // 启动帧定时器
        host_->start_frame_timer(8, on_frame_tick, this);
    }
    
    ~Application() {
        host_->stop_frame_timer();
        if (window_) {
            window_->events().unsubscribe(this);
        }
    }
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Closed:
            cleanup();
            host_->quit(0);
            break;
            
        case WindowEventKind::KeyDown:
            if (event.key_vk_code == VK_ESCAPE) {
                window_->close();
            }
            break;
            
        default:
            break;
        }
    }
    
    void update_animation() {
        rotation_ += 1.0f;
        if (rotation_ >= 360.0f) {
            rotation_ -= 360.0f;
        }
        // 重绘窗口
    }
    
private:
    static void on_frame_tick(void* user_data) {
        Application* app = static_cast<Application*>(user_data);
        app->update_animation();
    }
    
    void cleanup() {
        log("清理资源");
        window_ = nullptr;
    }
};

int main() {
    auto host = platform::create_application_host();
    Application app(host.get());
    return host->run();
}
```

---

## 总结

`IApplicationHost` 是 `mine.platform.abi` 模块的进程级应用程序宿主接口，具备：

1. **9 个方法**：run、quit、create_window、clipboard、screens、ime、start_frame_timer、stop_frame_timer
2. **消息循环**：`run()` 进入平台主消息循环，阻塞直到调用 `quit()`
3. **窗口创建**：`create_window()` 创建窗口，返回 `OwnedPtr<IWindow>`
4. **核心服务**：剪贴板、显示器枚举、IME 服务
5. **帧定时器**：动画驱动定时器

**使用建议**：
- 使用 `OwnedPtr` 管理窗口生命周期
- 窗口关闭时退出消息循环（`quit()`）
- 检查窗口创建是否成功
- 停止帧定时器
- `run()` 和 `create_window()` 在主线程调用
- `quit()` 可在任意线程调用（线程安全）
