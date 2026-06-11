# WindowContext - 应用窗口上下文接口

## 1. 概述

`IWindowContext` 是 `mine.ui.window` 模块的核心接口，为 `Window` 提供应用级的资源访问能力。它采用**反向依赖注入**设计，使得 `mine.ui.window` 模块无需依赖 `mine.ui.app`，而是通过接口回调的方式与 `Application` 协作。

### 核心功能

- **平台窗口创建**：通过 `create_native_window()` 创建平台原生窗口实例
- **图形资源访问**：提供对 `IDevice`、`IQueue`、`IRenderer` 的引用
- **IME 服务访问**：提供对输入法服务的引用
- **懒初始化通知**：通过 `on_window_first_show()` 回调通知 Window 首次显示
- **生命周期管理**：通过全局函数注册/注销应用上下文

### 关键特性

- **模块解耦**：`mine.ui.window` 不依赖 `mine.ui.app`，避免循环依赖
- **单例模式**：进程级全局上下文，确保所有 Window 共享同一套资源
- **懒初始化支持**：Window 可以以 pending 状态构造，首次 show() 时才获取资源
- **资源共享**：所有 Window 共享 Application 管理的图形设备和渲染器
- **线程安全**：全局函数通过原子操作保证线程安全
- **可测试性**：可通过 Mock 实现测试 Window 而无需启动完整 Application

### 依赖关系

```
┌─────────────────────────────────────────────────────────────┐
│                      mine.ui.app                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Application (implements IWindowContext)              │  │
│  │  - device, queue, renderer, ime (owned)              │  │
│  │  - create_native_window() implementation             │  │
│  └───────────────┬──────────────────────────────────────┘  │
│                  │ registers via                            │
│                  │ set_application_window_context()         │
└──────────────────┼──────────────────────────────────────────┘
                   │
                   ▼
         ┌─────────────────────────┐
         │ Global Context Pointer  │
         │ (thread-safe atomic)    │
         └────────┬────────────────┘
                  │
                  │ accessed via
                  │ get_application_window_context()
                  │
┌─────────────────▼────────────────────────────────────────────┐
│                    mine.ui.window                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Window                                               │  │
│  │  - show() → get context → create native window      │  │
│  │  - render() → use device/queue/renderer             │  │
│  └──────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

### 与其他框架对比

| 框架 | 窗口上下文模式 | 依赖方向 |
|------|---------------|---------|
| **WPF** | Application 直接包含 Windows 集合 | Window → Application |
| **Avalonia** | IApplicationPlatform 接口 | Window ↔ Application (双向) |
| **Qt** | QApplication 全局单例 | QWidget → QApplication |
| **MineFramework** | IWindowContext 接口 + 全局函数 | Window ← Application (反向注入) |

MineFramework 的设计优势：
- **更清晰的依赖方向**：`mine.ui.window` 不依赖 `mine.ui.app`，可以独立编译
- **更灵活的扩展性**：可以实现自定义 Application 而无需修改 Window
- **更好的可测试性**：可以 Mock IWindowContext 进行单元测试

---

## 2. 文件位置

**头文件路径**：
```
src/mine/ui/window/api/include/mine/ui/window/WindowContext.h
```

**依赖模块**：
- `mine.core`：OwnedPtr、StringView 等基础类型
- `mine.platform.abi`：IWindow、WindowDesc、IMEService 等平台抽象
- `mine.gfx.rhi`：IDevice、IQueue 等图形设备抽象
- `mine.paint`：IRenderer 渲染器抽象

**相关文件**：
- `Window.h`：使用 IWindowContext 的主要客户端
- `Application.h`（mine.ui.app）：IWindowContext 的典型实现者

---

## 3. 类定义

```cpp
namespace mine::ui {

// 前置声明
class Window;

/// @brief 应用窗口上下文接口
/// @details 
/// 由 Application 实现,供 Window 在 show() 时获取应用级资源。
/// 采用反向依赖注入模式,使 mine.ui.window 无需依赖 mine.ui.app。
/// 
/// 生命周期:
/// - Application::run() 开始时调用 set_application_window_context(this)
/// - Application::~Application() 时调用 set_application_window_context(nullptr)
/// - Window::show() 时调用 get_application_window_context() 获取上下文
/// 
/// 线程安全性:
/// - 所有虚函数由 Application 在主线程实现
/// - get/set_application_window_context() 通过原子操作保证线程安全
class MINE_UI_WINDOW_API IWindowContext {
public:
    virtual ~IWindowContext() = default;

    /// @brief 创建平台原生窗口
    /// @param desc 窗口描述符(标题、尺寸、样式等)
    /// @return 原生窗口智能指针,失败返回 nullptr
    /// @note 
    /// - 调用者拥有返回窗口的所有权
    /// - 窗口创建失败时返回 nullptr(如:平台不支持、资源不足等)
    /// - 通常在 Window::show() 的懒初始化流程中被调用
    [[nodiscard]] virtual core::OwnedPtr<platform::IWindow>
        create_native_window(const platform::WindowDesc& desc) = 0;

    /// @brief 获取图形设备
    /// @return 图形设备引用(生命周期由 Application 管理)
    /// @note 
    /// - 用于创建 Swapchain、Buffer、Texture 等资源
    /// - 所有 Window 共享同一个 IDevice
    [[nodiscard]] virtual gfx::IDevice& device() noexcept = 0;

    /// @brief 获取图形队列
    /// @return 图形队列引用(生命周期由 Application 管理)
    /// @note 
    /// - 用于提交渲染命令、Present 操作
    /// - 所有 Window 共享同一个 IQueue
    [[nodiscard]] virtual gfx::IQueue& queue() noexcept = 0;

    /// @brief 获取渲染器
    /// @return 渲染器引用(生命周期由 Application 管理)
    /// @note 
    /// - 用于将视觉树渲染到 RenderTarget
    /// - 所有 Window 共享同一个 IRenderer
    [[nodiscard]] virtual paint::IRenderer& renderer() noexcept = 0;

    /// @brief 获取 IME 服务
    /// @return IME 服务引用(生命周期由 Application 管理)
    /// @note 
    /// - 用于处理输入法编辑、候选词窗口等
    /// - 所有 Window 共享同一个 IMEService
    [[nodiscard]] virtual platform::IMEService& ime() noexcept = 0;

    /// @brief Window 首次 show() 时的通知回调
    /// @param win 首次显示的窗口引用
    /// @note 
    /// - 在 Window::show() 完成懒初始化后被调用
    /// - Application 可以在此时将 Window 加入内部窗口列表
    /// - 不应抛出异常,否则会导致 Window 处于不一致状态
    virtual void on_window_first_show(Window& win) = 0;
};

/// @brief 注册进程级应用窗口上下文
/// @param ctx 上下文指针,nullptr 表示注销
/// @note 
/// - 线程安全:使用原子操作
/// - 通常在 Application::run() 开始时调用 set_application_window_context(this)
/// - 通常在 Application::~Application() 时调用 set_application_window_context(nullptr)
/// - 不检查 ctx 有效性,调用者需确保 ctx 生命周期足够长
MINE_UI_WINDOW_API void set_application_window_context(IWindowContext* ctx) noexcept;

/// @brief 获取进程级应用窗口上下文
/// @return 上下文指针,未注册时返回 nullptr
/// @note 
/// - 线程安全:使用原子加载
/// - 通常在 Window::show() 中调用以获取资源
/// - 返回 nullptr 表示 Application 未启动或已退出
[[nodiscard]] MINE_UI_WINDOW_API IWindowContext* get_application_window_context() noexcept;

} // namespace mine::ui
```

---

## 4. 成员方法

### 4.1 create_native_window()

**功能说明**：
创建平台原生窗口实例。根据 `WindowDesc` 描述符创建 Win32/X11/Wayland 等平台特定的窗口句柄,并封装为 `IWindow` 接口返回。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `const platform::WindowDesc&` | `desc` | 窗口描述符,包含标题、尺寸、样式、父窗口等信息 |

**返回值**：
- `core::OwnedPtr<platform::IWindow>`：成功时返回原生窗口智能指针,失败时返回 nullptr

**前置条件**：
- Application 必须已完成图形设备初始化
- `desc` 参数必须有效(尺寸 > 0、标题非空等)

**后置条件**：
- 成功时返回可用的原生窗口,调用者拥有所有权
- 失败时返回 nullptr,不抛出异常

**线程安全性**：
- 必须在主线程调用(平台窗口 API 通常不支持多线程)

**示例**：
```cpp
// Application 实现示例
core::OwnedPtr<platform::IWindow> 
MyApplication::create_native_window(const platform::WindowDesc& desc) {
    // 调用平台工厂创建窗口
    auto window = platform::create_window(desc);
    if (!window) {
        core::log_error("Failed to create native window: {}", desc.title);
        return nullptr;
    }
    return window;
}
```

---

### 4.2 device()

**功能说明**：
返回应用级共享的图形设备引用。所有 Window 通过此接口获取 `IDevice`,用于创建 Swapchain、Buffer、Texture 等资源。

**参数列表**：无

**返回值**：
- `gfx::IDevice&`：图形设备引用(非空,生命周期由 Application 管理)

**前置条件**：
- Application 必须已完成图形设备初始化

**后置条件**：
- 返回的引用在 Application 生命周期内始终有效

**线程安全性**：
- 返回的引用是线程安全的(IDevice 内部使用互斥锁)

**示例**：
```cpp
// Window::show() 中使用
IWindowContext* ctx = get_application_window_context();
gfx::IDevice& device = ctx->device();

// 创建交换链
platform::SwapchainDesc swapchain_desc{
    .window = &native_window(),
    .width  = static_cast<uint32_t>(size().width),
    .height = static_cast<uint32_t>(size().height),
    .format = gfx::Format::BGRA8_UNORM_SRGB,
};
m_swapchain = device.create_swapchain(swapchain_desc);
```

---

### 4.3 queue()

**功能说明**：
返回应用级共享的图形队列引用。所有 Window 通过此接口获取 `IQueue`,用于提交渲染命令和 Present 操作。

**参数列表**：无

**返回值**：
- `gfx::IQueue&`：图形队列引用(非空,生命周期由 Application 管理)

**前置条件**：
- Application 必须已完成图形队列初始化

**后置条件**：
- 返回的引用在 Application 生命周期内始终有效

**线程安全性**：
- 返回的引用是线程安全的(IQueue 内部使用互斥锁)

**示例**：
```cpp
// Window::render() 中使用
void Window::render() {
    // ... 布局和渲染 ...
    
    // 提交 Present
    IWindowContext* ctx = get_application_window_context();
    gfx::IQueue& queue = ctx->queue();
    queue.present(*m_swapchain);
}
```

---

### 4.4 renderer()

**功能说明**：
返回应用级共享的渲染器引用。所有 Window 通过此接口获取 `IRenderer`,用于将视觉树渲染到 RenderTarget。

**参数列表**：无

**返回值**：
- `paint::IRenderer&`：渲染器引用(非空,生命周期由 Application 管理)

**前置条件**：
- Application 必须已完成渲染器初始化

**后置条件**：
- 返回的引用在 Application 生命周期内始终有效

**线程安全性**：
- 返回的引用不保证线程安全,必须在主线程使用

**示例**：
```cpp
// Window::render() 中使用
void Window::render() {
    if (!m_content) return;
    
    IWindowContext* ctx = get_application_window_context();
    paint::IRenderer& renderer = ctx->renderer();
    
    // 渲染视觉树到后缓冲
    auto target = m_swapchain->current_render_target();
    renderer.begin_frame(*target);
    renderer.render_visual(*m_content->visual(), math::Matrix3x2::identity());
    renderer.end_frame();
}
```

---

### 4.5 ime()

**功能说明**：
返回应用级共享的 IME 服务引用。Window 通过此接口获取输入法服务,用于处理文本输入、候选词窗口等。

**参数列表**：无

**返回值**：
- `platform::IMEService&`：IME 服务引用(非空,生命周期由 Application 管理)

**前置条件**：
- Application 必须已完成 IME 服务初始化

**后置条件**：
- 返回的引用在 Application 生命周期内始终有效

**线程安全性**：
- 返回的引用不保证线程安全,必须在主线程使用

**示例**：
```cpp
// Window::ime() 转发实现
platform::IMEService& Window::ime() noexcept {
    IWindowContext* ctx = get_application_window_context();
    return ctx->ime();
}
```

---

### 4.6 on_window_first_show()

**功能说明**：
Window 首次 show() 完成懒初始化后的通知回调。Application 可以在此时将 Window 加入内部窗口列表,或执行其他初始化逻辑。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `Window&` | `win` | 首次显示的窗口引用 |

**返回值**：无

**前置条件**：
- `win` 必须已完成懒初始化(native_window 非空)

**后置条件**：
- Application 将 Window 加入内部管理列表

**线程安全性**：
- 必须在主线程调用

**异常保证**：
- 不应抛出异常,否则会导致 Window 处于不一致状态

**示例**：
```cpp
// Application 实现示例
void MyApplication::on_window_first_show(Window& win) {
    // 加入窗口列表
    m_windows.push_back(&win);
    
    // 记录日志
    core::log_info("Window shown: {}", win.native_window().title());
    
    // 更新窗口计数
    ++m_window_count;
}
```

---

### 4.7 set_application_window_context()

**功能说明**：
注册进程级应用窗口上下文。通常在 Application::run() 开始时调用,传入 `this` 指针;在 Application 析构时调用,传入 `nullptr`。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `IWindowContext*` | `ctx` | 上下文指针,nullptr 表示注销 |

**返回值**：无

**前置条件**：
- 调用者需确保 `ctx` 生命周期足够长(通常为 Application 的生命周期)

**后置条件**：
- `get_application_window_context()` 将返回 `ctx`

**线程安全性**：
- 使用原子操作,线程安全

**示例**：
```cpp
// Application::run() 中注册
void MyApplication::run() {
    // 注册上下文
    set_application_window_context(this);
    
    // 运行消息循环
    platform::run_message_loop();
    
    // 注销上下文
    set_application_window_context(nullptr);
}
```

---

### 4.8 get_application_window_context()

**功能说明**：
获取进程级应用窗口上下文。通常在 Window::show() 中调用以获取资源。返回 nullptr 表示 Application 未启动或已退出。

**参数列表**：无

**返回值**：
- `IWindowContext*`：上下文指针,未注册时返回 nullptr

**前置条件**：无

**后置条件**：
- 返回值可能为 nullptr,调用者需检查

**线程安全性**：
- 使用原子加载,线程安全

**示例**：
```cpp
// Window::show() 中使用
void Window::show() {
    if (!is_pending()) {
        m_native_window->show();
        return;
    }
    
    // 获取上下文
    IWindowContext* ctx = get_application_window_context();
    if (!ctx) {
        core::log_error("Cannot show window: Application not running");
        return;
    }
    
    // 懒初始化
    initialize_from_context(*ctx);
}
```

---

## 5. 使用场景

### 场景1：Application 注册窗口上下文

**业务需求**：Application 启动时注册自身为窗口上下文,退出时注销。

**代码示例**：
```cpp
#include <mine/ui/app/Application.h>
#include <mine/ui/window/WindowContext.h>

namespace myapp {

class MyApplication : public ui::Application {
public:
    void run() override {
        // 1. 初始化图形设备
        initialize_graphics();
        
        // 2. 注册窗口上下文(this 实现了 IWindowContext)
        ui::set_application_window_context(this);
        
        // 3. 运行消息循环
        platform::run_message_loop();
        
        // 4. 注销窗口上下文
        ui::set_application_window_context(nullptr);
        
        // 5. 清理资源
        cleanup();
    }
    
    ~MyApplication() {
        // 确保上下文已注销
        ui::set_application_window_context(nullptr);
    }
};

} // namespace myapp
```

**预期效果**：
- Application 运行期间,所有 Window 可通过 `get_application_window_context()` 获取资源
- Application 退出后,`get_application_window_context()` 返回 nullptr

---

### 场景2：Window 懒初始化流程

**业务需求**：Window 以 pending 状态构造,首次 show() 时通过上下文获取资源完成初始化。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

namespace myapp {

void create_main_window() {
    // 1. 无参构造(pending 状态)
    auto window = core::make_owned<ui::Window>();
    
    // 2. 配置窗口属性
    window->set_title("My Application");
    window->set_size({800, 600});
    window->set_resizable(true);
    
    // 3. 设置内容
    auto content = core::make_owned<ui::TextBlock>();
    content->set_text("Hello, MineFramework!");
    window->set_content(content.get());
    
    // 4. 显示窗口(触发懒初始化)
    window->show();  // 内部调用 get_application_window_context()
    
    // 5. 此时 window 已完成初始化,可以访问 native_window()
    core::log_info("Window shown: {}", window->native_window().title());
}

} // namespace myapp
```

**预期效果**：
- `show()` 调用前,Window 处于 pending 状态,无原生窗口
- `show()` 调用后,Window 通过上下文创建原生窗口并完成初始化
- 后续可以正常访问 `native_window()` 等方法

---

### 场景3：自定义 Application 实现

**业务需求**：实现自定义 Application 类,提供特殊的图形设备配置或窗口创建逻辑。

**代码示例**：
```cpp
#include <mine/ui/window/WindowContext.h>
#include <mine/gfx/rhi/IDevice.h>

namespace myapp {

class CustomApplication : public ui::IWindowContext {
private:
    core::OwnedPtr<gfx::IDevice>      m_device;
    core::OwnedPtr<gfx::IQueue>       m_queue;
    core::OwnedPtr<paint::IRenderer>  m_renderer;
    core::OwnedPtr<platform::IMEService> m_ime;
    std::vector<ui::Window*>          m_windows;

public:
    CustomApplication() {
        // 初始化图形设备(使用 Vulkan 后端)
        gfx::DeviceDesc device_desc{
            .backend = gfx::Backend::Vulkan,
            .debug   = true,
        };
        m_device = gfx::create_device(device_desc);
        
        // 创建队列和渲染器
        m_queue    = m_device->create_queue();
        m_renderer = paint::create_renderer(*m_device);
        m_ime      = platform::create_ime_service();
    }
    
    // 实现 IWindowContext 接口
    core::OwnedPtr<platform::IWindow>
    create_native_window(const platform::WindowDesc& desc) override {
        // 自定义窗口创建逻辑(例如:强制使用圆角)
        auto modified_desc = desc;
        modified_desc.corner_preference = platform::WindowCornerPreference::Round;
        return platform::create_window(modified_desc);
    }
    
    gfx::IDevice&     device()   noexcept override { return *m_device; }
    gfx::IQueue&      queue()    noexcept override { return *m_queue; }
    paint::IRenderer& renderer() noexcept override { return *m_renderer; }
    platform::IMEService& ime()  noexcept override { return *m_ime; }
    
    void on_window_first_show(ui::Window& win) override {
        m_windows.push_back(&win);
        core::log_info("Window count: {}", m_windows.size());
    }
    
    void run() {
        ui::set_application_window_context(this);
        platform::run_message_loop();
        ui::set_application_window_context(nullptr);
    }
};

} // namespace myapp
```

**预期效果**：
- 可以使用自定义的图形后端(如 Vulkan 而非默认 D3D12)
- 可以修改窗口创建行为(如强制圆角)
- 保持与 Window 的兼容性

---

### 场景4：测试场景 - Mock 窗口上下文

**业务需求**：在单元测试中 Mock IWindowContext,无需启动完整 Application。

**代码示例**：
```cpp
#include <mine/ui/window/WindowContext.h>
#include <gtest/gtest.h>

namespace myapp::test {

// Mock 实现
class MockWindowContext : public ui::IWindowContext {
private:
    MockDevice      m_device;
    MockQueue       m_queue;
    MockRenderer    m_renderer;
    MockIMEService  m_ime;

public:
    core::OwnedPtr<platform::IWindow>
    create_native_window(const platform::WindowDesc& desc) override {
        // 返回 Mock 窗口
        return core::make_owned<MockWindow>(desc);
    }
    
    gfx::IDevice&     device()   noexcept override { return m_device; }
    gfx::IQueue&      queue()    noexcept override { return m_queue; }
    paint::IRenderer& renderer() noexcept override { return m_renderer; }
    platform::IMEService& ime()  noexcept override { return m_ime; }
    
    void on_window_first_show(ui::Window& win) override {
        // 记录调用次数
        ++first_show_count;
    }
    
    int first_show_count = 0;
};

// 测试用例
TEST(WindowTest, LazyInitialization) {
    // 1. 设置 Mock 上下文
    MockWindowContext mock_ctx;
    ui::set_application_window_context(&mock_ctx);
    
    // 2. 创建 Window
    ui::Window window;
    window.set_title("Test Window");
    
    // 3. 验证 pending 状态
    EXPECT_TRUE(window.is_pending());
    
    // 4. 调用 show()
    window.show();
    
    // 5. 验证初始化完成
    EXPECT_FALSE(window.is_pending());
    EXPECT_EQ(mock_ctx.first_show_count, 1);
    
    // 6. 清理
    ui::set_application_window_context(nullptr);
}

} // namespace myapp::test
```

**预期效果**：
- 可以在单元测试中测试 Window 的懒初始化流程
- 无需启动完整的图形设备和消息循环

---

### 场景5：多窗口管理

**业务需求**：Application 跟踪所有已创建的窗口,实现"关闭所有窗口时退出"逻辑。

**代码示例**：
```cpp
#include <mine/ui/window/WindowContext.h>

namespace myapp {

class MultiWindowApplication : public ui::IWindowContext {
private:
    std::vector<ui::Window*> m_windows;
    
public:
    void on_window_first_show(ui::Window& win) override {
        // 加入窗口列表
        m_windows.push_back(&win);
        
        // 监听 Closed 事件
        win.native_window().on_closed([this, &win]() {
            on_window_closed(win);
        });
    }
    
    void on_window_closed(ui::Window& win) {
        // 从列表移除
        auto it = std::find(m_windows.begin(), m_windows.end(), &win);
        if (it != m_windows.end()) {
            m_windows.erase(it);
        }
        
        // 所有窗口关闭时退出
        if (m_windows.empty()) {
            core::log_info("All windows closed, exiting...");
            platform::quit_message_loop();
        }
    }
    
    // 实现其他 IWindowContext 方法...
};

} // namespace myapp
```

**预期效果**：
- Application 自动跟踪所有窗口
- 最后一个窗口关闭时自动退出消息循环

---

### 场景6：跨模块资源共享

**业务需求**：多个 Window 共享同一个图形设备和渲染器,节省资源。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

namespace myapp {

void create_multiple_windows() {
    // 所有窗口共享同一个 IWindowContext
    IWindowContext* ctx = ui::get_application_window_context();
    
    // 创建主窗口
    auto main_window = core::make_owned<ui::Window>();
    main_window->set_title("Main Window");
    main_window->show();
    
    // 创建工具窗口
    auto tool_window = core::make_owned<ui::Window>();
    tool_window->set_title("Tool Window");
    tool_window->show();
    
    // 验证共享资源
    gfx::IDevice& main_device = ctx->device();
    gfx::IDevice& tool_device = ctx->device();
    
    // 同一个 device 实例
    assert(&main_device == &tool_device);
    
    core::log_info("All windows share the same device: {}",
                   static_cast<void*>(&main_device));
}

} // namespace myapp
```

**预期效果**：
- 多个 Window 共享同一个 IDevice/IQueue/IRenderer
- 节省显存和系统资源

---

### 场景7：延迟启动窗口

**业务需求**：Application 启动后延迟创建窗口,例如在后台初始化完成后再显示 UI。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>

namespace myapp {

class DelayedStartApplication {
private:
    core::OwnedPtr<ui::Window> m_main_window;
    
public:
    void run() {
        // 1. 注册上下文
        ui::set_application_window_context(this);
        
        // 2. 创建 pending 窗口(不显示)
        m_main_window = core::make_owned<ui::Window>();
        m_main_window->set_title("Loading...");
        
        // 3. 启动后台初始化线程
        async::run_async([this]() {
            // 模拟耗时初始化
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // 4. 初始化完成后在主线程显示窗口
            async::dispatch_main([this]() {
                m_main_window->set_title("My Application");
                m_main_window->show();  // 此时才触发懒初始化
            });
        });
        
        // 5. 运行消息循环
        platform::run_message_loop();
        
        // 6. 清理
        ui::set_application_window_context(nullptr);
    }
    
    // 实现 IWindowContext 接口...
};

} // namespace myapp
```

**预期效果**：
- Application 启动后不立即显示窗口
- 后台初始化完成后再显示 UI,提升用户体验

---

### 场景8：上下文生命周期管理

**业务需求**：确保 IWindowContext 生命周期长于所有 Window,避免悬空指针。

**代码示例**：
```cpp
#include <mine/ui/window/WindowContext.h>

namespace myapp {

class SafeApplication : public ui::IWindowContext {
private:
    std::vector<core::OwnedPtr<ui::Window>> m_owned_windows;
    
public:
    ~SafeApplication() {
        // 1. 先清理所有窗口(Window 析构会访问 IWindowContext)
        m_owned_windows.clear();
        
        // 2. 注销上下文
        ui::set_application_window_context(nullptr);
        
        // 3. 清理图形资源
        m_renderer.reset();
        m_queue.reset();
        m_device.reset();
    }
    
    void add_window(core::OwnedPtr<ui::Window> window) {
        m_owned_windows.push_back(std::move(window));
    }
    
    // 实现 IWindowContext 接口...
};

} // namespace myapp
```

**预期效果**：
- Window 析构时 IWindowContext 仍然有效
- 避免悬空指针导致的崩溃

---

## 6. 最佳实践

### ✅ 实践1：优先使用全局函数而非直接持有指针

**说明**：
不要在 Window 中缓存 `IWindowContext*` 指针,而是每次通过 `get_application_window_context()` 获取。这样可以避免悬空指针问题,且开销极小(原子加载)。

**✅ 推荐写法**：
```cpp
class Window {
public:
    void render() {
        // 每次获取上下文(开销可忽略)
        IWindowContext* ctx = get_application_window_context();
        if (!ctx) return;  // 防御性检查
        
        paint::IRenderer& renderer = ctx->renderer();
        renderer.render_visual(*m_content->visual());
    }
};
```

**❌ 不推荐写法**：
```cpp
class Window {
private:
    IWindowContext* m_cached_context;  // ❌ 缓存指针

public:
    Window() {
        m_cached_context = get_application_window_context();  // ❌ 可能为 nullptr
    }
    
    void render() {
        // ❌ 可能是悬空指针(Application 已退出)
        paint::IRenderer& renderer = m_cached_context->renderer();
        renderer.render_visual(*m_content->visual());
    }
};
```

**原因**：
- `get_application_window_context()` 使用原子加载,性能几乎无损
- 缓存指针可能在 Application 退出后变为悬空指针
- 防御性检查 nullptr 更安全

---

### ✅ 实践2：在 Application 构造函数中初始化资源

**说明**：
在 Application 构造函数中初始化图形设备等资源,而非在 `run()` 中初始化。这样可以确保资源在注册上下文之前就绪。

**✅ 推荐写法**：
```cpp
class MyApplication : public IWindowContext {
private:
    core::OwnedPtr<gfx::IDevice>     m_device;
    core::OwnedPtr<paint::IRenderer> m_renderer;

public:
    MyApplication() {
        // ✅ 在构造函数中初始化资源
        m_device = gfx::create_device({
            .backend = gfx::Backend::D3D12,
        });
        m_renderer = paint::create_renderer(*m_device);
    }
    
    void run() {
        // ✅ 资源已就绪,直接注册上下文
        set_application_window_context(this);
        platform::run_message_loop();
        set_application_window_context(nullptr);
    }
};
```

**❌ 不推荐写法**：
```cpp
class MyApplication : public IWindowContext {
private:
    core::OwnedPtr<gfx::IDevice>     m_device;
    core::OwnedPtr<paint::IRenderer> m_renderer;

public:
    void run() {
        // ❌ 先注册上下文,资源尚未初始化
        set_application_window_context(this);
        
        // ❌ 如果此时有 Window::show() 被调用,会访问未初始化的资源
        m_device = gfx::create_device({...});
        m_renderer = paint::create_renderer(*m_device);
        
        platform::run_message_loop();
        set_application_window_context(nullptr);
    }
};
```

**原因**：
- 注册上下文后,Window 可能立即调用 `device()`/`renderer()`
- 在构造函数中初始化可确保资源就绪

---

### ✅ 实践3：使用 RAII 管理上下文注册

**说明**：
使用 RAII 模式自动管理上下文注册/注销,避免忘记调用 `set_application_window_context(nullptr)`。

**✅ 推荐写法**：
```cpp
class ApplicationContextGuard {
private:
    IWindowContext* m_context;

public:
    explicit ApplicationContextGuard(IWindowContext* ctx)
        : m_context(ctx) {
        set_application_window_context(m_context);
    }
    
    ~ApplicationContextGuard() {
        set_application_window_context(nullptr);
    }
    
    // 禁止拷贝和移动
    ApplicationContextGuard(const ApplicationContextGuard&) = delete;
    ApplicationContextGuard& operator=(const ApplicationContextGuard&) = delete;
};

class MyApplication : public IWindowContext {
public:
    void run() {
        // ✅ 使用 RAII 自动管理上下文
        ApplicationContextGuard guard(this);
        
        // 运行消息循环(可能抛出异常)
        platform::run_message_loop();
        
        // ✅ guard 析构自动注销上下文,即使发生异常也安全
    }
};
```

**❌ 不推荐写法**：
```cpp
class MyApplication : public IWindowContext {
public:
    void run() {
        set_application_window_context(this);
        
        try {
            platform::run_message_loop();
        } catch (...) {
            // ❌ 异常路径容易忘记注销上下文
            set_application_window_context(nullptr);
            throw;
        }
        
        set_application_window_context(nullptr);
    }
};
```

**原因**：
- RAII 保证即使发生异常也会正确注销上下文
- 减少重复代码,提高可维护性

---

### ✅ 实践4：在 on_window_first_show() 中注册窗口事件

**说明**：
在 `on_window_first_show()` 回调中注册窗口的 Closed/Resized 等事件,确保不会遗漏。

**✅ 推荐写法**：
```cpp
class MyApplication : public IWindowContext {
private:
    std::vector<ui::Window*> m_windows;

public:
    void on_window_first_show(ui::Window& win) override {
        // ✅ 加入窗口列表
        m_windows.push_back(&win);
        
        // ✅ 注册 Closed 事件
        win.native_window().on_closed([this, &win]() {
            auto it = std::find(m_windows.begin(), m_windows.end(), &win);
            if (it != m_windows.end()) {
                m_windows.erase(it);
            }
            
            if (m_windows.empty()) {
                platform::quit_message_loop();
            }
        });
    }
};
```

**❌ 不推荐写法**：
```cpp
class MyApplication : public IWindowContext {
public:
    void create_window() {
        auto window = core::make_owned<ui::Window>();
        
        // ❌ 在窗口创建时注册事件(此时 native_window 可能未初始化)
        window->native_window().on_closed([this]() {
            // ...
        });
        
        window->show();
    }
};
```

**原因**：
- `on_window_first_show()` 确保窗口已完成初始化
- 集中管理窗口事件,避免遗漏

---

## 7. 常见陷阱

### ❌ 陷阱1：在 Application::run() 之前调用 Window::show()

**问题描述**：
如果在 Application 注册上下文之前调用 `Window::show()`,会导致 `get_application_window_context()` 返回 nullptr,窗口无法初始化。

**❌ 错误代码**：
```cpp
int main() {
    MyApplication app;
    
    // ❌ Application 尚未启动,上下文未注册
    ui::Window window;
    window.set_title("Main Window");
    window.show();  // ❌ 内部获取上下文失败,窗口无法显示
    
    // 启动 Application
    app.run();
    
    return 0;
}
```

**错误现象**：
- `Window::show()` 内部检测到上下文为 nullptr,打印错误日志
- 窗口不显示,程序继续运行但无 UI

**✅ 正确代码**：
```cpp
int main() {
    MyApplication app;
    
    // ✅ 创建 pending 窗口,但不 show
    ui::Window window;
    window.set_title("Main Window");
    
    // ✅ 注册启动回调
    app.on_started([&]() {
        // ✅ Application 已启动,上下文已注册
        window.show();
    });
    
    // 启动 Application
    app.run();
    
    return 0;
}
```

**原理解释**：
`Window::show()` 需要通过 `get_application_window_context()` 获取图形设备等资源,必须在 Application 注册上下文之后调用。

---

### ❌ 陷阱2：IWindowContext 生命周期短于 Window

**问题描述**：
如果 IWindowContext 实现对象(通常是 Application)在 Window 之前析构,会导致 Window 析构时访问悬空指针。

**❌ 错误代码**：
```cpp
void bad_example() {
    ui::Window window;
    
    {
        MyApplication app;  // ❌ app 的生命周期短于 window
        set_application_window_context(&app);
        
        window.set_title("Test");
        window.show();
        
    }  // ❌ app 析构,但 window 仍然存活
    
    // ❌ window 析构时可能访问已析构的 app
}
```

**错误现象**：
- 程序崩溃(访问悬空指针)
- 未定义行为

**✅ 正确代码**：
```cpp
void correct_example() {
    MyApplication app;
    set_application_window_context(&app);
    
    {
        ui::Window window;  // ✅ window 的生命周期短于 app
        window.set_title("Test");
        window.show();
        
    }  // ✅ window 先析构
    
    set_application_window_context(nullptr);
    // ✅ app 后析构
}
```

**原理解释**：
Window 析构时可能需要访问 IWindowContext(例如调用 `device()` 释放资源),必须确保 IWindowContext 生命周期长于所有 Window。

---

### ❌ 陷阱3：忘记注销上下文导致内存泄漏

**问题描述**：
如果 Application 退出时忘记调用 `set_application_window_context(nullptr)`,可能导致后续创建的 Window 访问到已析构的 Application。

**❌ 错误代码**：
```cpp
class MyApplication : public IWindowContext {
public:
    void run() {
        set_application_window_context(this);
        platform::run_message_loop();
        // ❌ 忘记注销上下文
    }
    
    ~MyApplication() {
        // ❌ 此时上下文仍指向 this,但对象即将析构
    }
};

int main() {
    {
        MyApplication app1;
        app1.run();
    }  // app1 析构,但上下文未清除
    
    {
        ui::Window window;
        window.show();  // ❌ 访问已析构的 app1
    }
    
    return 0;
}
```

**错误现象**：
- 程序崩溃(访问悬空指针)
- 内存泄漏(Window 持有已析构对象的引用)

**✅ 正确代码**：
```cpp
class MyApplication : public IWindowContext {
public:
    void run() {
        set_application_window_context(this);
        platform::run_message_loop();
        set_application_window_context(nullptr);  // ✅ 注销上下文
    }
    
    ~MyApplication() {
        // ✅ 双重保险:确保上下文已清除
        set_application_window_context(nullptr);
    }
};
```

**原理解释**：
全局上下文指针是静态变量,不会自动清除。必须手动调用 `set_application_window_context(nullptr)` 注销。

---

### ❌ 陷阱4：在非主线程调用 create_native_window()

**问题描述**：
平台窗口 API(Win32/X11/Wayland)通常要求在主线程调用,如果在子线程调用 `create_native_window()` 会导致未定义行为。

**❌ 错误代码**：
```cpp
class MyApplication : public IWindowContext {
public:
    core::OwnedPtr<platform::IWindow>
    create_native_window(const platform::WindowDesc& desc) override {
        // ❌ 在非主线程调用(例如:Window::show() 在子线程被调用)
        return platform::create_window(desc);
    }
};

void bad_thread_example() {
    std::thread worker([&]() {
        ui::Window window;
        window.show();  // ❌ 间接在子线程调用 create_native_window()
    });
    worker.join();
}
```

**错误现象**：
- Windows: 窗口创建失败或消息循环异常
- Linux: X11 协议错误或 Wayland 断言失败
- 程序崩溃或死锁

**✅ 正确代码**：
```cpp
void correct_thread_example() {
    ui::Window window;
    
    std::thread worker([&]() {
        // ✅ 在子线程准备数据
        auto data = prepare_data();
        
        // ✅ 在主线程显示窗口
        async::dispatch_main([&, data]() {
            window.set_content(create_content(data));
            window.show();  // ✅ 在主线程调用
        });
    });
    
    worker.join();
}
```

**原理解释**：
平台窗口 API 通常使用线程本地存储(TLS)和消息队列,必须在创建消息循环的线程(通常是主线程)调用。

---

### ❌ 陷阱5：在 IWindowContext 方法中抛出异常

**问题描述**：
如果 IWindowContext 方法(尤其是 `on_window_first_show()`)抛出异常,会导致 Window 处于不一致状态。

**❌ 错误代码**：
```cpp
class MyApplication : public IWindowContext {
public:
    void on_window_first_show(ui::Window& win) override {
        m_windows.push_back(&win);
        
        // ❌ 可能抛出异常(例如:网络请求失败)
        auto config = fetch_remote_config();
        apply_config(win, config);
        
        // ❌ 如果抛出异常,Window 已部分初始化但未完全就绪
    }
};
```

**错误现象**：
- Window 显示但配置未应用
- Window 处于半初始化状态,后续操作可能失败

**✅ 正确代码**：
```cpp
class MyApplication : public IWindowContext {
public:
    void on_window_first_show(ui::Window& win) override {
        m_windows.push_back(&win);
        
        try {
            // ✅ 捕获异常,不向外传播
            auto config = fetch_remote_config();
            apply_config(win, config);
        } catch (const std::exception& e) {
            // ✅ 使用默认配置,记录错误日志
            core::log_error("Failed to fetch remote config: {}", e.what());
            apply_default_config(win);
        }
    }
};
```

**原理解释**：
`on_window_first_show()` 在 `Window::show()` 的关键路径上,抛出异常会破坏 Window 的状态。应捕获所有异常并提供降级方案。

---

## 8. 完整示例

以下是一个完整的示例程序,展示 Application 实现 IWindowContext、Window 懒初始化、多窗口管理的完整流程。

```cpp
// ========== MyApplication.h ==========

#pragma once

#include <mine/ui/window/WindowContext.h>
#include <mine/gfx/rhi/IDevice.h>
#include <mine/gfx/rhi/IQueue.h>
#include <mine/paint/IRenderer.h>
#include <mine/platform/abi/IWindow.h>
#include <mine/platform/abi/IMEService.h>
#include <vector>
#include <functional>

namespace myapp {

/// @brief 自定义 Application,实现 IWindowContext 接口
class MyApplication : public mine::ui::IWindowContext {
private:
    // 图形资源(Application 拥有所有权)
    mine::core::OwnedPtr<mine::gfx::IDevice>     m_device;
    mine::core::OwnedPtr<mine::gfx::IQueue>      m_queue;
    mine::core::OwnedPtr<mine::paint::IRenderer> m_renderer;
    mine::core::OwnedPtr<mine::platform::IMEService> m_ime;
    
    // 窗口列表
    std::vector<mine::ui::Window*> m_windows;
    
    // 启动回调
    std::function<void()> m_on_started;

public:
    /// @brief 构造函数:初始化图形资源
    MyApplication() {
        using namespace mine;
        
        core::log_info("Initializing graphics device...");
        
        // 1. 创建图形设备(D3D12 后端)
        gfx::DeviceDesc device_desc{
            .backend = gfx::Backend::D3D12,
            .debug   = true,
        };
        m_device = gfx::create_device(device_desc);
        
        // 2. 创建图形队列
        m_queue = m_device->create_queue();
        
        // 3. 创建渲染器
        m_renderer = paint::create_renderer(*m_device);
        
        // 4. 创建 IME 服务
        m_ime = platform::create_ime_service();
        
        core::log_info("Graphics device initialized successfully");
    }
    
    /// @brief 析构函数:清理资源
    ~MyApplication() {
        using namespace mine;
        
        core::log_info("Shutting down application...");
        
        // 1. 清理窗口列表(实际所有权不在 Application)
        m_windows.clear();
        
        // 2. 注销上下文(双重保险)
        ui::set_application_window_context(nullptr);
        
        // 3. 清理图形资源(按创建的逆序)
        m_ime.reset();
        m_renderer.reset();
        m_queue.reset();
        m_device.reset();
        
        core::log_info("Application shut down successfully");
    }
    
    /// @brief 设置启动回调
    void on_started(std::function<void()> callback) {
        m_on_started = std::move(callback);
    }
    
    /// @brief 运行应用
    void run() {
        using namespace mine;
        
        core::log_info("Starting application...");
        
        // 1. 注册窗口上下文(使 Window::show() 可以获取资源)
        ui::set_application_window_context(this);
        
        // 2. 调用启动回调(通常在此创建并显示窗口)
        if (m_on_started) {
            m_on_started();
        }
        
        core::log_info("Entering message loop...");
        
        // 3. 运行消息循环(阻塞直到所有窗口关闭)
        platform::run_message_loop();
        
        core::log_info("Message loop exited");
        
        // 4. 注销窗口上下文
        ui::set_application_window_context(nullptr);
    }
    
    // ========== IWindowContext 接口实现 ==========
    
    /// @brief 创建平台原生窗口
    mine::core::OwnedPtr<mine::platform::IWindow>
    create_native_window(const mine::platform::WindowDesc& desc) override {
        using namespace mine;
        
        core::log_info("Creating native window: {}", desc.title);
        
        // 调用平台工厂创建窗口
        auto window = platform::create_window(desc);
        
        if (!window) {
            core::log_error("Failed to create native window: {}", desc.title);
            return nullptr;
        }
        
        core::log_info("Native window created successfully: {}",
                       window->title());
        return window;
    }
    
    /// @brief 获取图形设备
    mine::gfx::IDevice& device() noexcept override {
        return *m_device;
    }
    
    /// @brief 获取图形队列
    mine::gfx::IQueue& queue() noexcept override {
        return *m_queue;
    }
    
    /// @brief 获取渲染器
    mine::paint::IRenderer& renderer() noexcept override {
        return *m_renderer;
    }
    
    /// @brief 获取 IME 服务
    mine::platform::IMEService& ime() noexcept override {
        return *m_ime;
    }
    
    /// @brief Window 首次显示时的通知回调
    void on_window_first_show(mine::ui::Window& win) override {
        using namespace mine;
        
        // 1. 加入窗口列表
        m_windows.push_back(&win);
        core::log_info("Window shown: {}, total windows: {}",
                       win.native_window().title(),
                       m_windows.size());
        
        // 2. 注册 Closed 事件
        win.native_window().on_closed([this, &win]() {
            on_window_closed(win);
        });
    }

private:
    /// @brief 窗口关闭时的处理
    void on_window_closed(mine::ui::Window& win) {
        using namespace mine;
        
        // 1. 从列表移除
        auto it = std::find(m_windows.begin(), m_windows.end(), &win);
        if (it != m_windows.end()) {
            m_windows.erase(it);
        }
        
        core::log_info("Window closed: {}, remaining windows: {}",
                       win.native_window().title(),
                       m_windows.size());
        
        // 2. 所有窗口关闭时退出消息循环
        if (m_windows.empty()) {
            core::log_info("All windows closed, quitting application...");
            platform::quit_message_loop();
        }
    }
};

} // namespace myapp

// ========== main.cpp ==========

#include "MyApplication.h"
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/layout/StackPanel.h>

using namespace mine;

int main() {
    // 1. 创建 Application
    myapp::MyApplication app;
    
    // 2. 创建主窗口(pending 状态,不立即显示)
    auto main_window = core::make_owned<ui::Window>();
    main_window->set_title("MineFramework - WindowContext 示例");
    main_window->set_size({800, 600});
    main_window->set_padding({20, 20, 20, 20});
    
    // 3. 创建内容 UI
    auto panel = core::make_owned<ui::StackPanel>();
    panel->set_orientation(ui::Orientation::Vertical);
    panel->set_spacing(10);
    
    // 标题文本
    auto title = core::make_owned<ui::TextBlock>();
    title->set_text("WindowContext 懒初始化示例");
    title->set_font_size(24);
    panel->add_child(title.get());
    
    // 说明文本
    auto description = core::make_owned<ui::TextBlock>();
    description->set_text(
        "此窗口通过 IWindowContext 懒初始化创建。\n"
        "Application 实现了 IWindowContext 接口,\n"
        "Window 在 show() 时自动获取图形设备和渲染器。"
    );
    panel->add_child(description.get());
    
    // 创建新窗口按钮
    auto new_window_btn = core::make_owned<ui::Button>();
    new_window_btn->set_content("创建新窗口");
    new_window_btn->on_click([&]() {
        // 动态创建新窗口
        auto new_window = core::make_owned<ui::Window>();
        new_window->set_title("新窗口");
        new_window->set_size({400, 300});
        
        auto text = core::make_owned<ui::TextBlock>();
        text->set_text("这是一个新窗口!");
        new_window->set_content(text.get());
        
        new_window->show();  // 通过同一个 IWindowContext 初始化
        
        core::log_info("New window created");
    });
    panel->add_child(new_window_btn.get());
    
    // 关闭按钮
    auto close_btn = core::make_owned<ui::Button>();
    close_btn->set_content("关闭窗口");
    close_btn->on_click([&main_window]() {
        main_window->close();
    });
    panel->add_child(close_btn.get());
    
    // 设置内容
    main_window->set_content(panel.get());
    
    // 4. 注册启动回调(在 Application 启动后显示窗口)
    app.on_started([&main_window]() {
        core::log_info("Application started, showing main window...");
        main_window->show();  // 触发懒初始化
    });
    
    // 5. 运行 Application
    app.run();
    
    return 0;
}
```

### 预期运行效果

**控制台输出**：
```
[INFO] Initializing graphics device...
[INFO] Graphics device initialized successfully
[INFO] Starting application...
[INFO] Application started, showing main window...
[INFO] Creating native window: MineFramework - WindowContext 示例
[INFO] Native window created successfully: MineFramework - WindowContext 示例
[INFO] Window shown: MineFramework - WindowContext 示例, total windows: 1
[INFO] Entering message loop...
[INFO] Creating native window: 新窗口
[INFO] Native window created successfully: 新窗口
[INFO] Window shown: 新窗口, total windows: 2
[INFO] New window created
[INFO] Window closed: 新窗口, remaining windows: 1
[INFO] Window closed: MineFramework - WindowContext 示例, remaining windows: 0
[INFO] All windows closed, quitting application...
[INFO] Message loop exited
[INFO] Shutting down application...
[INFO] Application shut down successfully
```

**窗口显示**：
- 主窗口显示标题、说明文本、两个按钮
- 点击"创建新窗口"按钮后,弹出一个新的独立窗口
- 两个窗口共享同一个图形设备和渲染器
- 关闭所有窗口后,程序自动退出

**编译命令**：
```bash
# 使用 xmake 构建
xmake build myapp

# 或使用 CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

---

## 9. 总结

### 核心要点回顾

- **IWindowContext 采用反向依赖注入**：`mine.ui.window` 不依赖 `mine.ui.app`,通过接口回调与 Application 协作
- **全局上下文管理**：通过 `set_application_window_context()` / `get_application_window_context()` 管理进程级单例
- **懒初始化支持**：Window 可以以 pending 状态构造,首次 show() 时通过上下文获取资源
- **资源共享模式**：所有 Window 共享 Application 的 IDevice、IQueue、IRenderer、IMEService
- **生命周期管理**：IWindowContext 必须在 Application::run() 开始时注册,退出时注销
- **线程安全保证**：全局函数使用原子操作,但 IWindowContext 方法必须在主线程调用
- **可测试性设计**：可以通过 Mock IWindowContext 进行单元测试,无需启动完整 Application

### 与其他模块的协作关系

```
mine.ui.app::Application
  ↓ implements
IWindowContext
  ↓ registered via set_application_window_context()
Global Context Pointer (atomic)
  ↓ accessed via get_application_window_context()
mine.ui.window::Window
  ↓ uses
IDevice / IQueue / IRenderer / IMEService
```

### 适用场景总结

- **标准应用开发**：使用默认 Application 实现,无需关心 IWindowContext 细节
- **自定义 Application**：实现 IWindowContext 接口,提供特殊的图形设备配置或窗口创建逻辑
- **多窗口管理**：通过 `on_window_first_show()` 跟踪所有窗口,实现"关闭所有窗口时退出"等逻辑
- **单元测试**：Mock IWindowContext,测试 Window 的懒初始化流程和渲染逻辑

### 下一步学习建议

- **Window 类详解**：阅读 [02-Window.md](02-Window.md),了解 Window 的完整 API 和使用方式
- **Application 实现**：阅读 mine.ui.app 模块文档,了解默认 Application 的实现细节
- **平台窗口抽象**：阅读 mine.platform.abi 模块文档,了解 IWindow 接口和平台特定实现
- **图形设备管理**：阅读 mine.gfx.rhi 模块文档,了解 IDevice、IQueue、ISwapchain 的使用

---

**文档版本**：1.0.0  
**最后更新**：2026-06-11  
**作者**：MineFramework 开发团队
