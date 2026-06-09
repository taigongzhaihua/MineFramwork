# IWindow 详细接口文档

## 概述

`IWindow` 是 `mine.platform.abi` 模块的**平台窗口抽象接口**，每个 `IWindow` 对象代表操作系统中的一个顶级窗口（或子窗口）。

**核心特性：**
- **18 个方法**：可见性控制、属性设置/查询、事件订阅、自定义 Chrome、窗口状态
- **主线程操作**：所有方法须在创建窗口的线程（通常为主线程）调用
- **OwnedPtr 返回**：通过 `IApplicationHost::create_window()` 创建，返回 `OwnedPtr<IWindow>`
- **生命周期管理**：`close()` → Closing 事件 → 销毁原生窗口 → Closed 事件

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/IWindow.h
```

---

## 类型定义

```cpp
namespace mine::platform {

class IWindow {
public:
    virtual ~IWindow() = default;
    
    // 可见性控制
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;
    
    // 属性设置
    virtual void set_title(core::StringView title) = 0;
    virtual void set_size(math::Size size) = 0;
    virtual void set_position(math::Point position) = 0;
    
    // 属性查询
    [[nodiscard]] virtual math::Size size() const = 0;
    [[nodiscard]] virtual math::Point position() const = 0;
    [[nodiscard]] virtual float dpi() const = 0;
    [[nodiscard]] virtual bool is_visible() const = 0;
    [[nodiscard]] virtual NativeHandle native_handle() const = 0;
    
    // 事件订阅
    [[nodiscard]] virtual IWindowEventSource& events() = 0;
    
    // 自定义 Chrome（可选功能）
    virtual void set_chrome(const WindowChromeDesc& chrome) {}
    virtual void begin_drag() {}
    
    // 窗口状态（可选功能）
    virtual void set_state(WindowState state) {}
    [[nodiscard]] virtual WindowState state() const { return WindowState::Normal; }
};

}
```

---

## 成员方法

### 可见性控制

#### show()

```cpp
virtual void show() = 0;
```

**描述**：显示窗口（若已可见则无操作）。

**特征**：
- 首次调用：触发原生窗口显示
- 重复调用：无操作（幂等）
- 不触发事件

**示例**：
```cpp
IWindow* window = host->create_window(desc).release();
window->show();  // 显示窗口
```

---

#### hide()

```cpp
virtual void hide() = 0;
```

**描述**：隐藏窗口（不销毁，可再次 `show()`）。

**特征**：
- 隐藏窗口但不销毁
- 可再次 `show()` 显示
- 不触发事件

**示例**：
```cpp
window->hide();  // 隐藏窗口
// ...
window->show();  // 再次显示
```

---

#### close()

```cpp
virtual void close() = 0;
```

**描述**：请求关闭窗口（触发 Closing 事件，可被取消）。

**特征**：
- 触发 `WindowEventKind::Closing` 事件
- 事件处理器可设置 `event.cancel = true` 阻止关闭
- 若未取消，销毁原生窗口并触发 `WindowEventKind::Closed` 事件
- `Closed` 事件后不得再调用任何 `IWindow` 方法

**示例**：
```cpp
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closing) {
            if (has_unsaved_data()) {
                event.cancel = true;  // 阻止关闭
                show_save_dialog();
            }
        } else if (event.kind == WindowEventKind::Closed) {
            cleanup();
            window_ = nullptr;  // 清除指针
        }
    }
};

// 请求关闭
window->close();
```

---

### 属性设置

#### set_title()

```cpp
virtual void set_title(core::StringView title) = 0;
```

**描述**：设置窗口标题（UTF-8）。

**参数**：
- `title`：UTF-8 编码的标题字符串

**示例**：
```cpp
window->set_title("MineFramework - Document Editor");
```

---

#### set_size()

```cpp
virtual void set_size(math::Size size) = 0;
```

**描述**：设置客户区尺寸（逻辑像素）。

**参数**：
- `size`：客户区尺寸（逻辑像素）

**特征**：
- 逻辑像素（96 DPI 基准）
- 设置客户区尺寸（不含标题栏和边框）
- 触发 `WindowEventKind::Resized` 事件

**示例**：
```cpp
window->set_size({800, 600});  // 设置客户区尺寸为 800x600
```

---

#### set_position()

```cpp
virtual void set_position(math::Point position) = 0;
```

**描述**：设置窗口左上角屏幕位置（逻辑像素）。

**参数**：
- `position`：窗口左上角位置（逻辑像素）

**特征**：
- 逻辑像素（96 DPI 基准）
- 设置窗口左上角位置（相对于虚拟桌面原点）
- 触发 `WindowEventKind::Moved` 事件

**示例**：
```cpp
window->set_position({100, 100});  // 设置窗口位置为 (100, 100)
```

---

### 属性查询

#### size()

```cpp
[[nodiscard]] virtual math::Size size() const = 0;
```

**描述**：当前客户区尺寸（逻辑像素）。

**返回**：客户区尺寸（逻辑像素）

**示例**：
```cpp
math::Size current_size = window->size();
log("当前尺寸: {}x{}", current_size.width, current_size.height);
```

---

#### position()

```cpp
[[nodiscard]] virtual math::Point position() const = 0;
```

**描述**：当前窗口左上角屏幕位置（逻辑像素）。

**返回**：窗口左上角位置（逻辑像素）

**示例**：
```cpp
math::Point current_position = window->position();
log("当前位置: ({}, {})", current_position.x, current_position.y);
```

---

#### dpi()

```cpp
[[nodiscard]] virtual float dpi() const = 0;
```

**描述**：当前窗口 DPI（通常为 96.0 * 缩放比）。

**返回**：当前 DPI（默认 96.0）

**示例**：
```cpp
float current_dpi = window->dpi();
log("当前 DPI: {}", current_dpi);
float scale = current_dpi / 96.0f;
log("缩放比: {}%", scale * 100);
```

---

#### is_visible()

```cpp
[[nodiscard]] virtual bool is_visible() const = 0;
```

**描述**：窗口当前是否可见。

**返回**：`true` 表示可见，`false` 表示隐藏

**示例**：
```cpp
if (window->is_visible()) {
    log("窗口可见");
} else {
    log("窗口隐藏");
}
```

---

#### native_handle()

```cpp
[[nodiscard]] virtual NativeHandle native_handle() const = 0;
```

**描述**：获取平台原生句柄（不透明，供 RHI 后端使用）。

**返回**：平台原生句柄（Windows: HWND，macOS: NSWindow*，Linux: XID）

**示例**：
```cpp
NativeHandle handle = window->native_handle();
#ifdef _WIN32
    HWND hwnd = (HWND)handle.ptr;
#elif defined(__APPLE__)
    NSWindow* nswindow = (__bridge NSWindow*)handle.ptr;
#elif defined(__linux__)
    Window xwindow = (Window)handle.value;
#endif
```

---

### 事件订阅

#### events()

```cpp
[[nodiscard]] virtual IWindowEventSource& events() = 0;
```

**描述**：获取事件分发器，用于订阅/取消订阅窗口事件。

**返回**：事件分发器引用

**示例**：
```cpp
class Application : public IWindowEventSink {
public:
    void init(IWindow* window) {
        window->events().subscribe(this);
    }
    
    void cleanup(IWindow* window) {
        window->events().unsubscribe(this);
    }
    
    void on_window_event(WindowEvent& event) override {
        // 处理事件
    }
};
```

---

### 自定义 Chrome（可选功能）

#### set_chrome()

```cpp
virtual void set_chrome(const WindowChromeDesc& chrome) {}
```

**描述**：应用自定义窗口 Chrome 配置。

**参数**：
- `chrome`：自定义 Chrome 描述符

**特征**：
- 默认实现为空操作（no-op）
- 各平台后端可按需重写
- Windows：处理 `WM_NCCALCSIZE` / `WM_NCHITTEST`，调用 DWM API
- macOS：使用 `NSWindow` 标题栏/透明 API
- 窗口创建后可随时调用，立即生效

**示例**：
```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 32.0f;
chrome.resize_border_thickness = {8.0f, 8.0f, 8.0f, 8.0f};
chrome.corner_preference = WindowCornerPreference::Round;

window->set_chrome(chrome);
```

---

#### begin_drag()

```cpp
virtual void begin_drag() {}
```

**描述**：以编程方式发起窗口拖拽，类似 WPF 的 `Window.DragMove()`。

**特征**：
- 释放框架内部鼠标捕获
- 通知系统"用户在标题栏（HTCAPTION）区域按下鼠标"
- 系统接管后续拖拽逻辑（移动窗口 + 磁吸/Snap 布局）
- 典型用法：在自定义标题栏区域的 `MouseDownEvent` 处理函数中调用
- 调用后当前帧内不应再处理鼠标事件（建议同时调用 `args.set_handled(true)`）
- 默认实现为空操作（no-op）

**示例**：
```cpp
class TitleBarControl : public Control {
public:
    void on_mouse_down(const MouseButtonEventArgs& args) override {
        if (args.button() == MouseButton::Left) {
            window()->begin_drag();  // 开始拖拽窗口
            args.set_handled(true);
        }
    }
};
```

---

### 窗口状态（可选功能）

#### set_state()

```cpp
virtual void set_state(WindowState state) {}
```

**描述**：设置窗口显示状态（Normal / Minimized / Maximized）。

**参数**：
- `state`：目标状态

**特征**：
- 默认实现为空操作（no-op）
- 各平台后端按需重写

**示例**：
```cpp
window->set_state(WindowState::Maximized);  // 最大化窗口
window->set_state(WindowState::Minimized);  // 最小化窗口
window->set_state(WindowState::Normal);     // 恢复正常状态
```

---

#### state()

```cpp
[[nodiscard]] virtual WindowState state() const { return WindowState::Normal; }
```

**描述**：查询窗口当前显示状态。

**返回**：当前 `WindowState` 枚举值

**特征**：
- 默认实现始终返回 `Normal`
- 各平台后端按需重写

**示例**：
```cpp
WindowState current_state = window->state();
if (current_state == WindowState::Maximized) {
    log("窗口已最大化");
} else if (current_state == WindowState::Minimized) {
    log("窗口已最小化");
} else {
    log("窗口正常显示");
}
```

---

## 使用场景

### 1. 创建和显示窗口

```cpp
WindowDesc desc;
desc.title = "My Application";
desc.size = {800, 600};
desc.position = {100, 100};
desc.auto_position = false;

auto window = host->create_window(desc);
window->show();
```

---

### 2. 订阅窗口事件

```cpp
class Application : public IWindowEventSink {
    IWindow* window_;
    
public:
    void init(IWindow* window) {
        window_ = window;
        window_->events().subscribe(this);
    }
    
    void cleanup() {
        window_->events().unsubscribe(this);
        window_ = nullptr;
    }
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Resized:
            handle_resize(event.new_size);
            break;
        case WindowEventKind::Closed:
            cleanup();
            break;
        default:
            break;
        }
    }
};
```

---

### 3. 窗口居中显示

```cpp
void center_window(IWindow* window, IScreenManager* screen_manager) {
    ScreenInfo screen = screen_manager->get_primary_screen();
    math::Rect work_area = screen.work_area;
    math::Size window_size = window->size();
    
    math::Point center = {
        work_area.x + (work_area.width - window_size.width) / 2,
        work_area.y + (work_area.height - window_size.height) / 2
    };
    
    window->set_position(center);
}
```

---

### 4. DPI 变化处理

```cpp
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::DpiChanged) {
            float new_dpi = event.new_dpi;
            math::Rect suggested_rect = event.suggested_rect;
            
            // 应用建议的尺寸和位置
            window_->set_position(suggested_rect.position());
            window_->set_size(suggested_rect.size());
            
            // 重新加载资源
            reload_resources(new_dpi);
        }
    }
};
```

---

### 5. 自定义标题栏拖拽

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 32.0f;
window->set_chrome(chrome);

class TitleBarControl : public Control {
public:
    void on_mouse_down(const MouseButtonEventArgs& args) override {
        if (args.button() == MouseButton::Left) {
            window()->begin_drag();
            args.set_handled(true);
        }
    }
};
```

---

### 6. 窗口状态管理

```cpp
void toggle_maximize(IWindow* window) {
    if (window->state() == WindowState::Maximized) {
        window->set_state(WindowState::Normal);
    } else {
        window->set_state(WindowState::Maximized);
    }
}
```

---

### 7. 阻止窗口关闭

```cpp
class Application : public IWindowEventSink {
    bool has_unsaved_data_ = false;
    
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closing) {
            if (has_unsaved_data_) {
                event.cancel = true;  // 阻止关闭
                show_save_confirmation_dialog();
            }
        }
    }
    
private:
    void show_save_confirmation_dialog() {
        // 显示保存确认对话框
        // 用户确认后：
        has_unsaved_data_ = false;
        window_->close();
    }
};
```

---

## 生命周期

### 创建

```cpp
WindowDesc desc;
auto window = host->create_window(desc);  // 返回 OwnedPtr<IWindow>
window->show();
```

---

### 关闭

```cpp
window->close();  // 触发 Closing 事件
```

**事件序列**：
1. 调用 `window->close()`
2. 触发 `WindowEventKind::Closing` 事件
3. 事件处理器可设置 `event.cancel = true` 阻止关闭
4. 若未取消，销毁原生窗口
5. 触发 `WindowEventKind::Closed` 事件
6. `Closed` 事件后不得再调用任何 `IWindow` 方法

---

### 销毁

```cpp
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            cleanup();
            window_ = nullptr;  // 清除指针
        }
    }
};
```

---

## 线程模型

### 主线程操作

**特征**：
- 所有 `IWindow` 方法须在创建窗口的线程（通常为主线程）调用
- 跨线程调用未定义行为（可能崩溃）

**示例**：
```cpp
// ✅ 主线程调用
void on_main_thread() {
    window->set_size({800, 600});
}

// ❌ 后台线程调用（未定义行为）
std::thread([window]() {
    window->set_size({800, 600});  // 崩溃
}).detach();
```

---

### 跨线程通信

```cpp
// ✅ 使用事件循环跨线程调用
async::EventLoop* main_loop = /* ... */;

std::thread([main_loop, window]() {
    // 后台线程执行耗时操作
    auto result = do_heavy_work();
    
    // 切换到主线程更新窗口
    main_loop->post([window, result]() {
        window->set_title(result);  // 主线程安全
    });
}).detach();
```

---

## 最佳实践

### 1. 使用 RAII 管理窗口生命周期

```cpp
// ✅ 推荐：使用 OwnedPtr/UniquePtr
auto window = host->create_window(desc);
// 自动管理生命周期

// ❌ 不推荐：裸指针
IWindow* window = host->create_window(desc).release();
// 需要手动管理生命周期
```

---

### 2. Closed 事件后清除指针

```cpp
// ✅ 推荐：Closed 事件后清除指针
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        window_ = nullptr;  // 清除指针
    }
}

// ❌ 不推荐：Closed 事件后继续使用
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        window_->set_title("Closed");  // 崩溃
    }
}
```

---

### 3. 主线程调用所有方法

```cpp
// ✅ 推荐：主线程调用
void on_main_thread() {
    window->set_size({800, 600});
}

// ❌ 不推荐：后台线程调用
std::thread([window]() {
    window->set_size({800, 600});  // 未定义行为
}).detach();
```

---

### 4. 使用 work_area 而非 bounds 居中窗口

```cpp
// ✅ 推荐：使用 work_area
ScreenInfo screen = screen_manager->get_primary_screen();
math::Point center = {
    screen.work_area.x + (screen.work_area.width - window->size().width) / 2,
    screen.work_area.y + (screen.work_area.height - window->size().height) / 2
};
window->set_position(center);

// ❌ 不推荐：使用 bounds（可能被任务栏遮挡）
math::Point center = {
    screen.bounds.x + (screen.bounds.width - window->size().width) / 2,
    screen.bounds.y + (screen.bounds.height - window->size().height) / 2
};
```

---

### 5. DPI 变化时应用建议尺寸

```cpp
// ✅ 推荐：应用建议尺寸
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::DpiChanged) {
        window_->set_position(event.suggested_rect.position());
        window_->set_size(event.suggested_rect.size());
        reload_resources(event.new_dpi);
    }
}

// ❌ 不推荐：忽略建议尺寸
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::DpiChanged) {
        reload_resources(event.new_dpi);  // 窗口尺寸不变，内容模糊
    }
}
```

---

## 常见陷阱

### 1. Closed 事件后继续调用方法

```cpp
// ❌ 问题：Closed 事件后继续调用
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        window_->set_title("Closed");  // 崩溃
    }
}

// ✅ 解决：Closed 事件后清除指针
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::Closed) {
        window_ = nullptr;
    }
}
```

---

### 2. 后台线程调用窗口方法

```cpp
// ❌ 问题：后台线程调用
std::thread([window]() {
    window->set_size({800, 600});  // 未定义行为
}).detach();

// ✅ 解决：切换到主线程
std::thread([main_loop, window]() {
    main_loop->post([window]() {
        window->set_size({800, 600});
    });
}).detach();
```

---

### 3. 忘记订阅事件

```cpp
// ❌ 问题：忘记订阅事件
class Application : public IWindowEventSink {
public:
    void init(IWindow* window) {
        window_ = window;
        // 忘记订阅
    }
    
    void on_window_event(WindowEvent& event) override {
        // 永远不会被调用
    }
};

// ✅ 解决：订阅事件
void init(IWindow* window) {
    window_ = window;
    window_->events().subscribe(this);
}
```

---

### 4. 使用 bounds 而非 work_area 居中窗口

```cpp
// ❌ 问题：使用 bounds，窗口被任务栏遮挡
ScreenInfo screen = screen_manager->get_primary_screen();
math::Point center = {
    screen.bounds.x + (screen.bounds.width - window->size().width) / 2,
    screen.bounds.y + (screen.bounds.height - window->size().height) / 2
};

// ✅ 解决：使用 work_area
math::Point center = {
    screen.work_area.x + (screen.work_area.width - window->size().width) / 2,
    screen.work_area.y + (screen.work_area.height - window->size().height) / 2
};
```

---

## 完整示例

```cpp
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/IScreenManager.h>

class Application : public IWindowEventSink {
    IApplicationHost* host_;
    IWindow* window_;
    bool has_unsaved_data_ = false;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        // 创建窗口
        WindowDesc desc;
        desc.title = "My Application";
        desc.size = {800, 600};
        auto window_ptr = host_->create_window(desc);
        window_ = window_ptr.release();
        
        // 订阅事件
        window_->events().subscribe(this);
        
        // 居中显示
        center_window();
        
        // 显示窗口
        window_->show();
    }
    
    ~Application() {
        if (window_) {
            window_->events().unsubscribe(this);
        }
    }
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Resized:
            handle_resize(event.new_size);
            break;
            
        case WindowEventKind::Closing:
            if (has_unsaved_data_) {
                event.cancel = true;
                show_save_confirmation_dialog();
            }
            break;
            
        case WindowEventKind::Closed:
            cleanup();
            break;
            
        case WindowEventKind::DpiChanged:
            window_->set_position(event.suggested_rect.position());
            window_->set_size(event.suggested_rect.size());
            reload_resources(event.new_dpi);
            break;
            
        default:
            break;
        }
    }
    
private:
    void center_window() {
        IScreenManager* screen_manager = host_->screen_manager();
        ScreenInfo screen = screen_manager->get_primary_screen();
        math::Rect work_area = screen.work_area;
        math::Size window_size = window_->size();
        
        math::Point center = {
            work_area.x + (work_area.width - window_size.width) / 2,
            work_area.y + (work_area.height - window_size.height) / 2
        };
        
        window_->set_position(center);
    }
    
    void handle_resize(math::Size new_size) {
        log("窗口尺寸变化: {}x{}", new_size.width, new_size.height);
        renderer_->resize(new_size);
    }
    
    void show_save_confirmation_dialog() {
        // 显示保存确认对话框
        // 用户确认后：
        has_unsaved_data_ = false;
        window_->close();
    }
    
    void reload_resources(float new_dpi) {
        log("DPI 变化: {}", new_dpi);
        resource_manager_->reload(new_dpi);
    }
    
    void cleanup() {
        log("清理资源");
        window_->events().unsubscribe(this);
        window_ = nullptr;
    }
};

int main() {
    auto host = platform::initialize();
    Application app(host.get());
    host->run();
    return 0;
}
```

---

## 总结

`IWindow` 是 `mine.platform.abi` 模块的平台窗口抽象接口，具备：

1. **18 个方法**：可见性控制、属性设置/查询、事件订阅、自定义 Chrome、窗口状态
2. **主线程操作**：所有方法须在创建窗口的线程（通常为主线程）调用
3. **OwnedPtr 返回**：通过 `IApplicationHost::create_window()` 创建，返回 `OwnedPtr<IWindow>`
4. **生命周期管理**：`close()` → Closing 事件 → 销毁原生窗口 → Closed 事件

**使用建议**：
- 使用 RAII 管理窗口生命周期（`OwnedPtr`/`UniquePtr`）
- `Closed` 事件后清除指针
- 主线程调用所有方法
- 使用 `work_area` 而非 `bounds` 居中窗口
- DPI 变化时应用建议尺寸
- 订阅事件处理窗口生命周期
- 不在 `Closed` 事件后调用任何方法
