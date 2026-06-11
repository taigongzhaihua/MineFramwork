# Application 类详细接口文档

## 1. 概述

`mine::ui::app::Application` 是 MineFramework UI 框架的应用程序基类，负责管理应用程序的完整生命周期、窗口管理、主题系统、资源管理以及底层基础设施的初始化与协调。它是所有 MineUI 应用程序的入口点和核心控制器。

### 1.1 核心功能

- **生命周期管理**：初始化平台宿主（`IApplicationHost`）、图形设备（`IDevice`）、渲染器（`IRenderer`），驱动主消息循环，管理退出流程。
- **窗口管理**：创建、注册、销毁 UI 窗口（`ui::Window`），支持主窗口语义（主窗口关闭时自动退出应用）。
- **主题系统**：注册命名主题资源字典、切换当前激活主题、广播资源变更通知，支持全局资源字典合并机制。
- **资源管理**：提供应用级全局资源字典（根字典），所有窗口/控件的资源字典向上连接到此字典。
- **字体管理**：提供默认系统字体访问接口（`default_font()`）。
- **基础设施访问**：向子类和外部模块暴露平台宿主、图形设备、渲染器等底层接口。
- **扩展点机制**：通过虚函数 `on_startup` / `on_exit` / `on_create_host` / `on_create_device` / `on_create_renderer` 支持子类扩展和测试注入。
- **动画驱动**：提供 `tick_and_render()` 辅助方法，推进窗口动画并触发重绘。

### 1.2 设计目标

1. **职责分离**：Application 负责"应用生命周期"，Window 负责"窗口内容"，IApplicationHost 负责"平台事件循环"。
2. **框架无关**：底层平台抽象（IApplicationHost）可替换，支持 Windows / macOS / Linux / Wasm 等平台。
3. **测试友好**：工厂扩展点（`on_create_*`）允许测试代码注入 mock 对象，避免依赖真实平台。
4. **扩展性**：子类只需重写 `on_startup` / `on_exit` 即可实现自定义应用逻辑，无需关心底层初始化细节。

### 1.3 在整体架构中的位置

```
mine::ui::app::Application (应用程序基类)
├─ 创建 platform::IApplicationHost (平台抽象层)
├─ 创建 gfx::IDevice (图形设备)
├─ 创建 paint::IRenderer (渲染器)
├─ 创建 ui::Window (UI 窗口)
│   ├─ 持有 IWindowContext (窗口渲染上下文)
│   ├─ 持有 Visual 树 (可视化树)
│   └─ 持有 ResourceDictionary (窗口资源字典，向上连接到 Application 全局资源)
└─ 驱动主消息循环 (委托 IApplicationHost::run())
```

### 1.4 继承关系

```
Application (抽象基类)
└─ MyApp (用户应用类，重写 on_startup/on_exit)
```

`Application` 本身可以直接实例化使用（用于最简单的场景），但通常建议继承并重写 `on_startup` 来实现自定义逻辑。

### 1.5 与其他框架的对比

| 框架 | 应用程序基类 | 主循环机制 | 主题切换 | 主窗口语义 |
|------|-------------|-----------|---------|-----------|
| WPF | `System.Windows.Application` | `Dispatcher.Run()` | `ThemeInfo` / `ResourceDictionary.MergedDictionaries` | `Application.MainWindow` |
| Avalonia | `Avalonia.Application` | `AppBuilder.StartWithClassicDesktopLifetime()` | `Application.Styles` | `ApplicationLifetime.MainWindow` |
| Qt | `QApplication` | `QApplication::exec()` | `QStyle` / `QStyleSheet` | 无内置主窗口语义（需手动连接 `lastWindowClosed`） |
| MineFramework | `mine::ui::app::Application` | `IApplicationHost::run()` | `register_theme()` / `set_theme()` | `set_main_window()` 自动退出 |

**MineFramework 的特色**：
- **资源字典合并机制**：与 WPF 类似，支持主题资源字典的动态合并和广播。
- **工厂扩展点**：`on_create_*` 方法支持测试注入，比 Qt 的单例设计更灵活。
- **平台抽象**：IApplicationHost 可替换，支持自定义事件循环（如嵌入到 SDL / GLFW）。

---

## 2. 文件位置

### 2.1 头文件

```
src/mine/ui/app/api/include/mine/ui/app/Application.h
```

### 2.2 依赖的模块

- `mine.core`：基础类型（OwnedPtr / StringView / Span）
- `mine.platform.abi`：平台抽象接口（IApplicationHost / WindowDesc）
- `mine.gfx.rhi`：图形设备接口（IDevice）
- `mine.paint`：渲染器接口（IRenderer）
- `mine.ui.window`：窗口类（ui::Window）
- `mine.ui.style`：资源字典（style::ResourceDictionary）
- `mine.text`：字体接口（text::FontFace）

### 2.3 包含方式

```cpp
// 方式1：直接包含
#include <mine/ui/app/Application.h>

// 方式2：使用伞形头文件（推荐）
#include <mine/ui/app/AppAll.h>
```

---

## 3. 类定义

```cpp
namespace mine::ui::app {

/**
 * @brief UI 应用程序基类
 * 
 * 负责管理应用程序的完整生命周期：
 * 1. 初始化平台宿主（IApplicationHost）、图形设备（IDevice）、渲染器（IRenderer）
 * 2. 创建与管理 UI 窗口（ui::Window），持有其完整生命周期
 * 3. 驱动主消息循环（委托 IApplicationHost::run()），并管理退出码
 * 4. 提供 on_startup / on_exit 扩展点，供子类实现应用逻辑
 * 5. 支持"主窗口"语义：主窗口关闭时自动退出主循环
 * 
 * @note 该类不可拷贝、不可移动
 * @see MINE_APPLICATION_MAIN 宏（用于生成进程入口函数）
 */
class MINE_UI_APP_API Application {
public:
    /**
     * @brief 构造函数
     * @note 不应在构造函数中执行复杂初始化（如创建窗口），应在 on_startup 中进行
     */
    Application();
    
    /**
     * @brief 析构函数
     * @note 自动销毁所有已创建的窗口、释放平台宿主、图形设备、渲染器等资源
     */
    virtual ~Application();
    
    // ── 禁止拷贝和移动 ──
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // ═══════════════════════════════════════════════════════════
    //  生命周期入口
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 启动应用程序主循环
     * @param argc 命令行参数数量（可选，默认 0）
     * @param argv 命令行参数数组（可选，默认 nullptr）
     * @return 退出码（0 表示成功，非 0 表示错误）
     * 
     * @details 执行顺序：
     *   1. on_create_host()     → 创建平台宿主
     *   2. on_create_device()   → 创建图形设备
     *   3. on_create_renderer() → 创建渲染器
     *   4. on_startup(argc, argv) → 子类初始化逻辑（创建窗口等）
     *   5. IApplicationHost::run() → 进入主消息循环（阻塞）
     *   6. on_exit(exit_code)   → 子类清理逻辑
     *   7. 析构所有资源
     * 
     * @note 该方法只能调用一次，重复调用会抛出异常
     * @note 如果 on_startup 抛出异常，主循环不会启动，直接返回退出码 1
     */
    int run(int argc = 0, char** argv = nullptr);
    
    /**
     * @brief 请求退出应用程序
     * @param exit_code 退出码（默认 0）
     * 
     * @details 该方法会：
     *   1. 记录退出码
     *   2. 调用 IApplicationHost::quit() 退出主消息循环
     *   3. 主消息循环退出后，run() 方法会调用 on_exit(exit_code)
     * 
     * @note 该方法可以在任意线程调用（线程安全）
     * @note 如果主循环尚未启动（run() 未被调用），该方法无效
     */
    void quit(int exit_code = 0);

    // ═══════════════════════════════════════════════════════════
    //  窗口管理
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 创建新窗口
     * @param desc 窗口描述符（标题、尺寸、样式等）
     * @return 新创建的窗口指针（非空，所有权由 Application 持有）
     * 
     * @pre 必须在 run() 调用后（即 on_startup 内部或主循环中）调用
     * @post 返回的窗口已完全初始化，但尚未显示（需调用 show() 方法）
     * @note 窗口的生命周期由 Application 管理，不应手动 delete
     */
    [[nodiscard]] ui::Window* create_window(const platform::WindowDesc& desc);
    
    /**
     * @brief 设置主窗口
     * @param window 窗口指针（必须是由 create_window() 创建的窗口）
     * 
     * @details 主窗口的特殊语义：
     *   - 主窗口关闭时，自动调用 quit(0) 退出应用程序
     *   - 如果不设置主窗口，所有窗口关闭后应用程序不会自动退出（需手动调用 quit()）
     * 
     * @note 可以多次调用以更换主窗口
     * @note 传入 nullptr 可以清除主窗口语义
     */
    void set_main_window(ui::Window* window);

    // ═══════════════════════════════════════════════════════════
    //  主题 / 资源系统
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 注册命名主题资源字典
     * @param name 主题名称（如 "Light" / "Dark"）
     * @param theme_dict 主题资源字典（移动语义，所有权转移给 Application）
     * 
     * @details 注册后：
     *   - 主题字典会被存储在 Application 内部
     *   - 可以通过 set_theme(name) 切换激活该主题
     *   - 如果同名主题已存在，会被覆盖
     * 
     * @note 注册主题后，必须调用 set_theme(name) 才能激活（仅注册不会自动应用）
     */
    void register_theme(core::StringView name, style::ResourceDictionary&& theme_dict);
    
    /**
     * @brief 切换当前激活主题
     * @param name 主题名称（必须是已通过 register_theme() 注册的名称）
     * @return 成功返回 true，失败返回 false（主题不存在）
     * 
     * @details 切换流程：
     *   1. 清除全局资源字典的合并层（移除旧主题资源）
     *   2. 将新主题字典合并到全局资源字典
     *   3. 广播资源变更通知到所有窗口（触发重新布局和重绘）
     * 
     * @note 主题切换会立即生效，所有窗口会自动更新外观
     * @note 线程安全（可以在任意线程调用）
     */
    bool set_theme(core::StringView name) noexcept;
    
    /**
     * @brief 获取当前激活主题名称
     * @return 主题名称（如果未设置主题，返回空字符串）
     */
    [[nodiscard]] core::StringView current_theme() const noexcept;
    
    /**
     * @brief 获取全局资源字典（可变版本）
     * @return 全局资源字典引用（根字典，所有窗口/控件资源字典向上连接到此字典）
     * 
     * @details 全局资源字典的用途：
     *   - 存储应用级共享资源（如全局画刷、字体、样式等）
     *   - 作为主题资源的合并目标
     *   - 所有窗口的资源字典会自动继承此字典的内容
     */
    [[nodiscard]] style::ResourceDictionary& global_resources() noexcept;
    
    /**
     * @brief 获取全局资源字典（只读版本）
     */
    [[nodiscard]] const style::ResourceDictionary& global_resources() const noexcept;

    // ═══════════════════════════════════════════════════════════
    //  字体
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 获取默认系统字体
     * @return 默认字体对象指针（非空）
     * 
     * @details 默认字体由平台自动选择：
     *   - Windows: "Segoe UI" 或 "Microsoft YaHei UI"
     *   - macOS: "SF Pro Text"
     *   - Linux: "Noto Sans" 或 "DejaVu Sans"
     * 
     * @note 返回的字体对象生命周期与 Application 绑定，不应手动释放
     */
    [[nodiscard]] text::FontFace* default_font() noexcept;

    // ═══════════════════════════════════════════════════════════
    //  基础设施访问
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 获取平台宿主接口
     * @return 平台宿主引用
     * @pre 必须在 run() 调用后访问（否则会抛出异常）
     */
    [[nodiscard]] platform::IApplicationHost& host();
    
    /**
     * @brief 获取图形设备接口
     * @return 图形设备引用
     * @pre 必须在 run() 调用后访问（否则会抛出异常）
     */
    [[nodiscard]] gfx::IDevice& device();
    
    /**
     * @brief 获取渲染器接口
     * @return 渲染器引用
     * @pre 必须在 run() 调用后访问（否则会抛出异常）
     */
    [[nodiscard]] paint::IRenderer& renderer();

protected:
    // ═══════════════════════════════════════════════════════════
    //  生命周期扩展点（子类重写）
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 应用启动回调（在主循环开始前调用）
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     * 
     * @details 典型用途：
     *   - 创建主窗口（调用 create_window()）
     *   - 设置主窗口（调用 set_main_window()）
     *   - 加载应用配置
     *   - 初始化 DI 容器
     *   - 注册和激活主题
     * 
     * @note 如果该方法抛出异常，主循环不会启动，run() 会立即返回退出码 1
     */
    virtual void on_startup(int argc, char** argv);
    
    /**
     * @brief 应用退出回调（在主循环结束后调用）
     * @param exit_code 退出码（来自 quit() 的参数）
     * 
     * @details 典型用途：
     *   - 保存应用状态
     *   - 清理资源
     *   - 记录退出日志
     * 
     * @note 该方法在所有窗口销毁前调用，可以安全访问窗口对象
     * @note 如果该方法抛出异常，会被捕获并记录，但不会影响退出流程
     */
    virtual void on_exit(int exit_code);

    // ═══════════════════════════════════════════════════════════
    //  工厂扩展点（测试注入用）
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 创建平台宿主（工厂方法）
     * @return 平台宿主对象（所有权转移给 Application）
     * 
     * @details 默认实现：
     *   - Windows: 创建 Win32ApplicationHost
     *   - macOS: 创建 CocoaApplicationHost
     *   - Linux: 创建 X11ApplicationHost 或 WaylandApplicationHost
     * 
     * @note 测试代码可以重写该方法以注入 mock 对象
     */
    virtual core::OwnedPtr<platform::IApplicationHost> on_create_host();
    
    /**
     * @brief 创建图形设备（工厂方法）
     * @return 图形设备对象（所有权转移给 Application）
     * 
     * @details 默认实现：
     *   - Windows: 创建 D3D12Device 或 D3D11Device
     *   - macOS: 创建 MetalDevice
     *   - Linux: 创建 VulkanDevice
     * 
     * @note 测试代码可以重写该方法以注入 NullDevice（软件渲染器）
     */
    virtual core::OwnedPtr<gfx::IDevice> on_create_device();
    
    /**
     * @brief 创建渲染器（工厂方法）
     * @param device 图形设备指针（非空）
     * @return 渲染器对象（所有权转移给 Application）
     * 
     * @details 默认实现：
     *   - 创建 SkiaRenderer（基于 Skia 图形库）
     * 
     * @note 测试代码可以重写该方法以注入 NullRenderer（不渲染）
     */
    virtual core::OwnedPtr<paint::IRenderer> on_create_renderer(gfx::IDevice* device);

    // ═══════════════════════════════════════════════════════════
    //  动画驱动辅助
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief 推进窗口动画并触发重绘
     * @param win 目标窗口指针
     * 
     * @details 该方法会：
     *   1. 调用窗口动画系统的 tick() 方法（更新动画状态）
     *   2. 如果动画状态发生变化，触发窗口重绘
     * 
     * @note 通常不需要手动调用该方法（动画系统会自动驱动）
     * @note 该方法用于特殊场景（如手动帧循环、测试等）
     */
    void tick_and_render(ui::Window* win);
};

} // namespace mine::ui::app
```

---

## 4. 成员方法

### 4.1 生命周期入口

#### 4.1.1 `run()`

**功能**：启动应用程序主循环，初始化所有子系统并进入事件循环。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `int` | `argc` | 命令行参数数量（可选，默认 0） |
| `char**` | `argv` | 命令行参数数组（可选，默认 nullptr） |

**返回值**：`int` - 退出码（0 表示成功，非 0 表示错误）

**前置条件**：
- 该方法只能调用一次
- Application 对象必须处于有效状态（未被移动或销毁）

**后置条件**：
- 所有子系统已初始化完成（或因异常失败）
- 主循环已运行并退出
- 所有资源已清理（窗口、渲染器、设备、宿主）

**执行顺序**：

```
1. on_create_host()     → 创建平台宿主
2. on_create_device()   → 创建图形设备
3. on_create_renderer() → 创建渲染器
4. on_startup(argc, argv) → 子类初始化逻辑
5. IApplicationHost::run() → 进入主消息循环（阻塞）
6. on_exit(exit_code)   → 子类清理逻辑
7. 析构所有资源
```

**线程安全性**：非线程安全（必须在主线程调用）

**示例**：

```cpp
class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = "My Application";
        desc.size  = {1280.0f, 720.0f};
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};

int main(int argc, char** argv) {
    MyApp app;
    return app.run(argc, argv);  // 进入主循环
}
```

#### 4.1.2 `quit()`

**功能**：请求退出应用程序主循环。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `int` | `exit_code` | 退出码（默认 0） |

**返回值**：无

**前置条件**：
- 主循环已启动（`run()` 已被调用）

**后置条件**：
- 主循环将在下一帧退出
- `on_exit(exit_code)` 将被调用
- `run()` 方法将返回 `exit_code`

**线程安全性**：线程安全（可以在任意线程调用）

**示例**：

```cpp
void on_window_close_requested() {
    // 用户点击关闭按钮
    app->quit(0);  // 请求退出
}
```

### 4.2 窗口管理

#### 4.2.1 `create_window()`

**功能**：创建新的 UI 窗口。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `const platform::WindowDesc&` | `desc` | 窗口描述符（标题、尺寸、样式等） |

**返回值**：`ui::Window*` - 新创建的窗口指针（非空，所有权由 Application 持有）

**前置条件**：
- 必须在 `run()` 调用后（即 `on_startup` 内部或主循环中）调用
- `desc` 参数必须有效

**后置条件**：
- 返回的窗口已完全初始化（已创建平台窗口句柄、渲染上下文、根 Visual 等）
- 窗口尚未显示（需调用 `show()` 方法）
- 窗口的资源字典已连接到 Application 的全局资源字典

**线程安全性**：非线程安全（必须在主线程调用）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    mine::platform::WindowDesc desc;
    desc.title = "Hello World";
    desc.size  = {800.0f, 600.0f};
    desc.resizable = true;
    desc.show_titlebar = true;
    
    auto* win = create_window(desc);
    win->show();
}
```

#### 4.2.2 `set_main_window()`

**功能**：设置主窗口（主窗口关闭时自动退出应用）。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `ui::Window*` | `window` | 窗口指针（必须是由 `create_window()` 创建的窗口，或 nullptr 清除主窗口语义） |

**返回值**：无

**前置条件**：
- 如果 `window` 非空，必须是由当前 Application 实例创建的窗口

**后置条件**：
- 旧的主窗口（如果有）不再具有主窗口语义
- 新的主窗口关闭时会自动调用 `quit(0)`

**线程安全性**：非线程安全（必须在主线程调用）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    auto* win1 = create_window(desc1);
    auto* win2 = create_window(desc2);
    
    set_main_window(win1);  // win1 关闭时退出应用
    
    win1->show();
    win2->show();
}
```

### 4.3 主题 / 资源系统

#### 4.3.1 `register_theme()`

**功能**：注册命名主题资源字典。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `core::StringView` | `name` | 主题名称（如 "Light" / "Dark"） |
| `style::ResourceDictionary&&` | `theme_dict` | 主题资源字典（移动语义，所有权转移给 Application） |

**返回值**：无

**前置条件**：
- `name` 不为空
- `theme_dict` 是有效的资源字典对象

**后置条件**：
- 主题字典已存储在 Application 内部
- 可以通过 `set_theme(name)` 激活该主题
- 如果同名主题已存在，会被覆盖

**线程安全性**：非线程安全（必须在主线程调用）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    // 创建 Light 主题
    mine::style::ResourceDictionary light_theme;
    light_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
    light_theme.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFF000000});
    register_theme("Light", std::move(light_theme));
    
    // 创建 Dark 主题
    mine::style::ResourceDictionary dark_theme;
    dark_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFF1E1E1E});
    dark_theme.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
    register_theme("Dark", std::move(dark_theme));
    
    // 激活 Light 主题
    set_theme("Light");
}
```

#### 4.3.2 `set_theme()`

**功能**：切换当前激活主题。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `core::StringView` | `name` | 主题名称（必须是已通过 `register_theme()` 注册的名称） |

**返回值**：`bool` - 成功返回 `true`，失败返回 `false`（主题不存在）

**前置条件**：
- 主题名称必须已通过 `register_theme()` 注册

**后置条件**：
- 全局资源字典的合并层已更新（移除旧主题资源，合并新主题资源）
- 所有窗口的资源字典已广播资源变更通知
- 所有控件会自动重新查找资源并更新外观

**线程安全性**：线程安全（可以在任意线程调用）

**示例**：

```cpp
void on_theme_button_click() {
    if (current_theme() == "Light") {
        set_theme("Dark");
    } else {
        set_theme("Light");
    }
}
```

#### 4.3.3 `current_theme()`

**功能**：获取当前激活主题名称。

**参数**：无

**返回值**：`core::StringView` - 主题名称（如果未设置主题，返回空字符串）

**线程安全性**：线程安全（可以在任意线程调用）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    register_theme("Light", std::move(light_theme));
    set_theme("Light");
    
    std::cout << "Current theme: " << current_theme() << std::endl;  // 输出: Light
}
```

#### 4.3.4 `global_resources()` / `global_resources() const`

**功能**：获取全局资源字典（应用级根字典）。

**参数**：无

**返回值**：`style::ResourceDictionary&` - 全局资源字典引用

**用途**：
- 存储应用级共享资源（如全局画刷、字体、样式等）
- 作为主题资源的合并目标
- 所有窗口的资源字典会自动继承此字典的内容

**线程安全性**：非线程安全（必须在主线程访问）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    // 向全局资源字典添加自定义资源
    auto& global = global_resources();
    global.set("AppTitle", core::String{u"My Application"});
    global.set("AppVersion", core::String{u"1.0.0"});
}
```

### 4.4 字体

#### 4.4.1 `default_font()`

**功能**：获取默认系统字体。

**参数**：无

**返回值**：`text::FontFace*` - 默认字体对象指针（非空）

**前置条件**：
- 必须在 `run()` 调用后访问

**线程安全性**：线程安全（可以在任意线程调用）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    auto* font = default_font();
    std::cout << "Default font: " << font->family_name() << std::endl;
    // Windows: Segoe UI
    // macOS: SF Pro Text
    // Linux: Noto Sans
}
```

### 4.5 基础设施访问

#### 4.5.1 `host()`

**功能**：获取平台宿主接口。

**参数**：无

**返回值**：`platform::IApplicationHost&` - 平台宿主引用

**前置条件**：
- 必须在 `run()` 调用后访问（否则会抛出异常）

**线程安全性**：非线程安全（必须在主线程访问）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    auto& host = this->host();
    std::cout << "Platform: " << host.platform_name() << std::endl;
}
```

#### 4.5.2 `device()`

**功能**：获取图形设备接口。

**参数**：无

**返回值**：`gfx::IDevice&` - 图形设备引用

**前置条件**：
- 必须在 `run()` 调用后访问（否则会抛出异常）

**线程安全性**：非线程安全（必须在主线程访问）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    auto& device = this->device();
    std::cout << "Graphics API: " << device.api_name() << std::endl;
    // Windows: Direct3D 12
    // macOS: Metal
    // Linux: Vulkan
}
```

#### 4.5.3 `renderer()`

**功能**：获取渲染器接口。

**参数**：无

**返回值**：`paint::IRenderer&` - 渲染器引用

**前置条件**：
- 必须在 `run()` 调用后访问（否则会抛出异常）

**线程安全性**：非线程安全（必须在主线程访问）

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    auto& renderer = this->renderer();
    std::cout << "Renderer: " << renderer.name() << std::endl;  // Skia
}
```

### 4.6 生命周期扩展点（子类重写）

#### 4.6.1 `on_startup()`

**功能**：应用启动回调（在主循环开始前调用）。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `int` | `argc` | 命令行参数数量 |
| `char**` | `argv` | 命令行参数数组 |

**返回值**：无

**典型用途**：
- 创建主窗口（调用 `create_window()`）
- 设置主窗口（调用 `set_main_window()`）
- 加载应用配置
- 初始化 DI 容器
- 注册和激活主题

**异常处理**：
- 如果该方法抛出异常，主循环不会启动，`run()` 会立即返回退出码 1

**示例**：

```cpp
void on_startup(int argc, char** argv) override {
    // 解析命令行参数
    bool fullscreen = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = true;
        }
    }
    
    // 创建主窗口
    mine::platform::WindowDesc desc;
    desc.title = "My Application";
    desc.size  = {1280.0f, 720.0f};
    desc.fullscreen = fullscreen;
    
    auto* win = create_window(desc);
    set_main_window(win);
    win->show();
}
```

#### 4.6.2 `on_exit()`

**功能**：应用退出回调（在主循环结束后调用）。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `int` | `exit_code` | 退出码（来自 `quit()` 的参数） |

**返回值**：无

**典型用途**：
- 保存应用状态
- 清理资源
- 记录退出日志

**异常处理**：
- 如果该方法抛出异常，会被捕获并记录，但不会影响退出流程

**示例**：

```cpp
void on_exit(int exit_code) override {
    // 保存应用状态
    save_application_state();
    
    // 记录退出日志
    std::cout << "Application exited with code: " << exit_code << std::endl;
}
```

### 4.7 工厂扩展点（测试注入用）

#### 4.7.1 `on_create_host()`

**功能**：创建平台宿主（工厂方法）。

**参数**：无

**返回值**：`core::OwnedPtr<platform::IApplicationHost>` - 平台宿主对象（所有权转移给 Application）

**默认实现**：
- Windows: 创建 `Win32ApplicationHost`
- macOS: 创建 `CocoaApplicationHost`
- Linux: 创建 `X11ApplicationHost` 或 `WaylandApplicationHost`

**用途**：测试代码可以重写该方法以注入 mock 对象

**示例**：

```cpp
// 测试类
class MockApplicationHost : public mine::platform::IApplicationHost {
    // ... mock 实现 ...
};

class TestApp : public mine::ui::app::Application {
protected:
    core::OwnedPtr<platform::IApplicationHost> on_create_host() override {
        return core::make_owned<MockApplicationHost>();
    }
};
```

#### 4.7.2 `on_create_device()`

**功能**：创建图形设备（工厂方法）。

**参数**：无

**返回值**：`core::OwnedPtr<gfx::IDevice>` - 图形设备对象（所有权转移给 Application）

**默认实现**：
- Windows: 创建 `D3D12Device` 或 `D3D11Device`
- macOS: 创建 `MetalDevice`
- Linux: 创建 `VulkanDevice`

**用途**：测试代码可以重写该方法以注入 `NullDevice`（软件渲染器）

**示例**：

```cpp
class TestApp : public mine::ui::app::Application {
protected:
    core::OwnedPtr<gfx::IDevice> on_create_device() override {
        // 使用软件渲染器（不依赖 GPU）
        return core::make_owned<mine::gfx::NullDevice>();
    }
};
```

#### 4.7.3 `on_create_renderer()`

**功能**：创建渲染器（工厂方法）。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `gfx::IDevice*` | `device` | 图形设备指针（非空） |

**返回值**：`core::OwnedPtr<paint::IRenderer>` - 渲染器对象（所有权转移给 Application）

**默认实现**：
- 创建 `SkiaRenderer`（基于 Skia 图形库）

**用途**：测试代码可以重写该方法以注入 `NullRenderer`（不渲染）

**示例**：

```cpp
class TestApp : public mine::ui::app::Application {
protected:
    core::OwnedPtr<paint::IRenderer> on_create_renderer(gfx::IDevice* device) override {
        // 使用空渲染器（不实际渲染）
        return core::make_owned<mine::paint::NullRenderer>(device);
    }
};
```

### 4.8 动画驱动辅助

#### 4.8.1 `tick_and_render()`

**功能**：推进窗口动画并触发重绘。

**参数**：

| 类型 | 名称 | 描述 |
|------|------|------|
| `ui::Window*` | `win` | 目标窗口指针（非空） |

**返回值**：无

**详细说明**：
- 调用窗口动画系统的 `tick()` 方法（更新动画状态）
- 如果动画状态发生变化，触发窗口重绘

**用途**：
- 通常不需要手动调用该方法（动画系统会自动驱动）
- 该方法用于特殊场景（如手动帧循环、测试等）

**示例**：

```cpp
// 手动帧循环（测试用）
void manual_frame_loop() {
    auto* win = create_window(desc);
    win->show();
    
    for (int i = 0; i < 60; ++i) {
        tick_and_render(win);  // 手动推进动画并重绘
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // 60 FPS
    }
}
```

---

## 5. 使用场景

### 场景 1：基础单窗口应用

**业务需求**：创建一个最简单的 MineUI 应用程序，显示一个空白窗口。

```cpp
#include <mine/ui/app/AppAll.h>

class HelloWorldApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 创建窗口描述符
        mine::platform::WindowDesc desc;
        desc.title = u"Hello World";
        desc.size  = {800.0f, 600.0f};
        desc.resizable = true;
        desc.show_titlebar = true;
        
        // 创建并显示主窗口
        auto* win = create_window(desc);
        set_main_window(win);  // 主窗口关闭时自动退出
        win->show();
    }
};

MINE_APPLICATION_MAIN(HelloWorldApp)
```

**预期效果**：
- 程序启动后显示一个 800x600 的窗口，标题为 "Hello World"
- 用户点击关闭按钮后，应用程序自动退出

---

### 场景 2：MVVM 应用（集成 DI 容器）

**业务需求**：创建一个 MVVM 架构的应用程序，使用 DI 容器管理服务。

```cpp
#include <mine/ui/app/AppAll.h>
#include <mine/di/ServiceCollection.h>
#include <mine/mvvm/ViewModelLocator.h>

class MvvmApp : public mine::ui::app::Application {
private:
    mine::di::ServiceCollection services_;
    
protected:
    void on_startup(int argc, char** argv) override {
        // 1. 配置 DI 容器
        services_.add_singleton<IDataService, DataService>();
        services_.add_transient<MainViewModel>();
        
        // 2. 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"MVVM Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        
        // 3. 设置 ViewModel
        auto* vm = services_.resolve<MainViewModel>();
        win->set_data_context(vm);
        
        win->show();
    }
};

MINE_APPLICATION_MAIN(MvvmApp)
```

**预期效果**：
- 应用程序启动后，DI 容器自动创建 ViewModel 实例
- 窗口的 DataContext 绑定到 ViewModel
- 所有控件的数据绑定自动生效

---

### 场景 3：多窗口应用

**业务需求**：创建一个支持多窗口的应用程序，主窗口关闭时退出，其他窗口关闭时只销毁自身。

```cpp
#include <mine/ui/app/AppAll.h>

class MultiWindowApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 创建主窗口
        mine::platform::WindowDesc main_desc;
        main_desc.title = u"Main Window";
        main_desc.size  = {1280.0f, 720.0f};
        
        auto* main_win = create_window(main_desc);
        set_main_window(main_win);  // 主窗口关闭时退出应用
        main_win->show();
        
        // 创建辅助窗口 1（工具面板）
        mine::platform::WindowDesc tool_desc;
        tool_desc.title = u"Tool Panel";
        tool_desc.size  = {400.0f, 600.0f};
        tool_desc.position = {100.0f, 100.0f};
        
        auto* tool_win = create_window(tool_desc);
        tool_win->show();
        
        // 创建辅助窗口 2（属性面板）
        mine::platform::WindowDesc prop_desc;
        prop_desc.title = u"Properties";
        prop_desc.size  = {300.0f, 500.0f};
        prop_desc.position = {500.0f, 100.0f};
        
        auto* prop_win = create_window(prop_desc);
        prop_win->show();
    }
};

MINE_APPLICATION_MAIN(MultiWindowApp)
```

**预期效果**：
- 应用程序启动后显示 3 个窗口：主窗口、工具面板、属性面板
- 关闭工具面板或属性面板时，只销毁对应窗口，应用程序继续运行
- 关闭主窗口时，应用程序自动退出（其他窗口也会被自动销毁）

---

### 场景 4：主题切换（Light / Dark 模式）

**业务需求**：支持 Light 和 Dark 两种主题，用户可以通过按钮切换。

```cpp
#include <mine/ui/app/AppAll.h>
#include <mine/ui/controls/Button.h>

class ThemeApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 1. 注册 Light 主题
        mine::style::ResourceDictionary light_theme;
        light_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
        light_theme.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFF000000});
        light_theme.set("ButtonBackgroundBrush", mine::paint::SolidColorBrush{0xFFE0E0E0});
        register_theme("Light", std::move(light_theme));
        
        // 2. 注册 Dark 主题
        mine::style::ResourceDictionary dark_theme;
        dark_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFF1E1E1E});
        dark_theme.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
        dark_theme.set("ButtonBackgroundBrush", mine::paint::SolidColorBrush{0xFF3C3C3C});
        register_theme("Dark", std::move(dark_theme));
        
        // 3. 激活 Light 主题
        set_theme("Light");
        
        // 4. 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"Theme Demo";
        desc.size  = {800.0f, 600.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        
        // 5. 创建切换主题按钮
        auto* button = new mine::ui::controls::Button();
        button->set_content(u"Toggle Theme");
        button->clicked().connect([this]() {
            if (current_theme() == "Light") {
                set_theme("Dark");
            } else {
                set_theme("Light");
            }
        });
        
        win->set_content(button);
        win->show();
    }
};

MINE_APPLICATION_MAIN(ThemeApp)
```

**预期效果**：
- 应用程序启动后使用 Light 主题（白色背景、黑色文字）
- 点击 "Toggle Theme" 按钮后，窗口立即切换到 Dark 主题（黑色背景、白色文字）
- 再次点击按钮后，切换回 Light 主题

---

### 场景 5：自定义全局资源

**业务需求**：在全局资源字典中定义应用级共享资源（如公司 Logo、品牌颜色等）。

```cpp
#include <mine/ui/app/AppAll.h>

class BrandedApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 1. 向全局资源字典添加自定义资源
        auto& global = global_resources();
        
        // 品牌颜色
        global.set("BrandPrimaryColor", mine::paint::Color{0xFF007ACC});
        global.set("BrandSecondaryColor", mine::paint::Color{0xFF68217A});
        
        // 品牌画刷
        global.set("BrandPrimaryBrush", 
            mine::paint::SolidColorBrush{mine::paint::Color{0xFF007ACC}});
        global.set("BrandSecondaryBrush", 
            mine::paint::SolidColorBrush{mine::paint::Color{0xFF68217A}});
        
        // 应用信息
        global.set("AppTitle", mine::core::String{u"MyCompany Product"});
        global.set("AppVersion", mine::core::String{u"1.0.0"});
        
        // 2. 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"Branded Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};

MINE_APPLICATION_MAIN(BrandedApp)
```

**预期效果**：
- 所有窗口和控件都可以通过资源字典访问 "BrandPrimaryBrush" 等全局资源
- 在 MML 中可以使用 `{StaticResource BrandPrimaryBrush}` 引用品牌颜色

---

### 场景 6：测试注入（Mock 平台宿主）

**业务需求**：在单元测试中运行 UI 代码，但不依赖真实平台窗口系统（使用 headless 模式）。

```cpp
#include <mine/ui/app/AppAll.h>
#include <gtest/gtest.h>

// Mock 平台宿主（不创建真实窗口）
class MockApplicationHost : public mine::platform::IApplicationHost {
public:
    int run() override {
        // 不进入真实消息循环，直接返回
        return 0;
    }
    
    void quit(int exit_code) override {
        // 空实现
    }
    
    core::StringView platform_name() const override {
        return "Mock";
    }
};

// 测试应用类
class TestApp : public mine::ui::app::Application {
protected:
    core::OwnedPtr<mine::platform::IApplicationHost> on_create_host() override {
        return core::make_owned<MockApplicationHost>();
    }
    
    core::OwnedPtr<mine::gfx::IDevice> on_create_device() override {
        return core::make_owned<mine::gfx::NullDevice>();  // 软件渲染器
    }
    
    core::OwnedPtr<mine::paint::IRenderer> on_create_renderer(gfx::IDevice* device) override {
        return core::make_owned<mine::paint::NullRenderer>(device);  // 空渲染器
    }
};

// 单元测试
TEST(ApplicationTest, CreateWindow) {
    TestApp app;
    int exit_code = app.run();
    EXPECT_EQ(exit_code, 0);
}
```

**预期效果**：
- 测试代码可以创建窗口、设置属性、触发事件，但不会显示真实窗口
- 测试运行速度快，不依赖图形硬件
- 适合 CI/CD 环境（无 GPU、无窗口系统）

---

### 场景 7：命令行参数处理

**业务需求**：根据命令行参数决定应用程序行为（如全屏模式、日志级别等）。

```cpp
#include <mine/ui/app/AppAll.h>
#include <cstring>
#include <iostream>

class CmdLineApp : public mine::ui::app::Application {
private:
    bool fullscreen_ = false;
    std::string log_level_ = "info";
    
protected:
    void on_startup(int argc, char** argv) override {
        // 1. 解析命令行参数
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--fullscreen") == 0) {
                fullscreen_ = true;
            } else if (std::strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
                log_level_ = argv[++i];
            }
        }
        
        // 2. 输出配置信息
        std::cout << "Fullscreen: " << (fullscreen_ ? "Yes" : "No") << std::endl;
        std::cout << "Log Level: " << log_level_ << std::endl;
        
        // 3. 创建主窗口
        mine::platform::WindowDesc desc;
        desc.title = u"Command Line App";
        desc.size  = {1280.0f, 720.0f};
        desc.fullscreen = fullscreen_;
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};

MINE_APPLICATION_MAIN(CmdLineApp)
```

**使用方式**：

```bash
# 普通窗口模式
./cmdlineapp

# 全屏模式
./cmdlineapp --fullscreen

# 设置日志级别
./cmdlineapp --log-level debug

# 组合使用
./cmdlineapp --fullscreen --log-level trace
```

**预期效果**：
- 不带参数启动时，显示普通窗口
- 带 `--fullscreen` 参数时，显示全屏窗口
- 带 `--log-level <level>` 参数时，设置日志级别

---

### 场景 8：异步加载资源（启动画面）

**业务需求**：应用程序启动时显示启动画面，异步加载资源，加载完成后显示主窗口。

```cpp
#include <mine/ui/app/AppAll.h>
#include <mine/async/TaskScheduler.h>

class SplashApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 1. 创建启动画面窗口
        mine::platform::WindowDesc splash_desc;
        splash_desc.title = u"Loading...";
        splash_desc.size  = {400.0f, 300.0f};
        splash_desc.show_titlebar = false;
        splash_desc.borderless = true;
        
        auto* splash_win = create_window(splash_desc);
        splash_win->show();
        
        // 2. 异步加载资源
        mine::async::TaskScheduler::run_async([this, splash_win]() {
            // 模拟加载过程
            load_fonts();
            load_textures();
            load_data();
            
            // 加载完成后，在主线程创建主窗口
            mine::async::TaskScheduler::run_on_main_thread([this, splash_win]() {
                // 关闭启动画面
                splash_win->close();
                
                // 创建主窗口
                mine::platform::WindowDesc main_desc;
                main_desc.title = u"Main Application";
                main_desc.size  = {1280.0f, 720.0f};
                
                auto* main_win = create_window(main_desc);
                set_main_window(main_win);
                main_win->show();
            });
        });
    }
    
private:
    void load_fonts() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void load_textures() {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
    
    void load_data() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
};

MINE_APPLICATION_MAIN(SplashApp)
```

**预期效果**：
- 应用程序启动后立即显示启动画面（无边框窗口）
- 异步加载字体、纹理、数据（总计约 1.6 秒）
- 加载完成后，启动画面消失，主窗口显示

---

## 6. 最佳实践

### ✅ 实践 1：在 `on_startup` 中创建窗口

**说明**：窗口创建必须在 `run()` 调用后（即 `on_startup` 内部）进行，不应在构造函数中创建窗口，因为此时平台宿主、图形设备、渲染器尚未初始化。

**✅ 推荐写法**：

```cpp
class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // 此时所有子系统已初始化完成，可以安全创建窗口
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};
```

**❌ 不推荐写法**：

```cpp
class MyApp : public mine::ui::app::Application {
private:
    ui::Window* main_window_ = nullptr;
    
public:
    MyApp() {
        // ❌ 错误：构造函数中创建窗口，此时 run() 尚未调用
        //    平台宿主、图形设备、渲染器都是 nullptr
        //    会导致崩溃或断言失败
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        
        main_window_ = create_window(desc);  // ❌ 崩溃！
    }
};
```

**问题说明**：
- `create_window()` 内部会调用 `device()` 和 `renderer()` 获取图形设备和渲染器
- 如果在构造函数中调用，这些对象尚未创建，会抛出异常或返回 nullptr
- 正确做法是在 `on_startup()` 回调中创建窗口

---

### ✅ 实践 2：设置主窗口语义

**说明**：如果应用程序有明确的主窗口（关闭时应退出应用），应调用 `set_main_window()` 设置主窗口语义，避免手动管理退出逻辑。

**✅ 推荐写法**：

```cpp
void on_startup(int argc, char** argv) override {
    // 创建主窗口
    auto* main_win = create_window(main_desc);
    set_main_window(main_win);  // ✅ 主窗口关闭时自动退出应用
    main_win->show();
    
    // 创建辅助窗口（不影响退出逻辑）
    auto* tool_win = create_window(tool_desc);
    tool_win->show();
}
```

**❌ 不推荐写法**：

```cpp
void on_startup(int argc, char** argv) override {
    // 创建主窗口
    auto* main_win = create_window(main_desc);
    // ❌ 忘记设置主窗口语义
    main_win->show();
    
    // ❌ 手动监听关闭事件
    main_win->closing().connect([this]() {
        quit(0);  // 手动退出（冗余且容易遗漏）
    });
}
```

**问题说明**：
- 忘记调用 `set_main_window()` 会导致主窗口关闭后应用程序不退出（需要手动调用 `quit()`）
- 手动监听关闭事件不仅冗余，而且容易遗漏（如果有多个地方创建主窗口）
- 使用 `set_main_window()` 可以统一管理退出逻辑，减少错误

---

### ✅ 实践 3：合理管理资源字典

**说明**：全局资源字典应存储应用级共享资源，主题资源应通过 `register_theme()` + `set_theme()` 管理，避免手动操作合并字典。

**✅ 推荐写法**：

```cpp
void on_startup(int argc, char** argv) override {
    // 1. 向全局资源字典添加应用级共享资源
    auto& global = global_resources();
    global.set("AppTitle", mine::core::String{u"My Application"});
    global.set("AppVersion", mine::core::String{u"1.0.0"});
    
    // 2. 注册主题（主题资源单独管理）
    mine::style::ResourceDictionary light_theme;
    light_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
    register_theme("Light", std::move(light_theme));
    
    mine::style::ResourceDictionary dark_theme;
    dark_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFF1E1E1E});
    register_theme("Dark", std::move(dark_theme));
    
    // 3. 激活主题
    set_theme("Light");  // ✅ 自动合并到全局资源字典并广播通知
}
```

**❌ 不推荐写法**：

```cpp
void on_startup(int argc, char** argv) override {
    // ❌ 手动将主题资源添加到全局资源字典
    auto& global = global_resources();
    global.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
    global.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFF000000});
    
    // ❌ 切换主题时需要手动清除旧资源、添加新资源、广播通知
    // 代码复杂、容易出错
}

void switch_to_dark_theme() {
    auto& global = global_resources();
    global.remove("BackgroundBrush");  // ❌ 手动清除
    global.remove("ForegroundBrush");
    global.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFF1E1E1E});
    global.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
    // ❌ 忘记广播资源变更通知，导致窗口不更新
}
```

**问题说明**：
- 手动操作全局资源字典切换主题容易遗漏资源清理或广播通知
- 使用 `register_theme()` + `set_theme()` 可以自动处理合并字典、广播通知等逻辑
- 主题资源应与应用级共享资源分离，避免混乱

---

### ✅ 实践 4：避免在构造函数中初始化窗口

**说明**：Application 的构造函数只应执行轻量级初始化（如成员变量初始化），不应创建窗口、访问设备/渲染器等，这些操作应在 `on_startup()` 中进行。

**✅ 推荐写法**：

```cpp
class MyApp : public mine::ui::app::Application {
private:
    std::string config_path_;
    
public:
    MyApp() 
        : config_path_("config.json")  // ✅ 轻量级初始化
    {
        // 不执行复杂操作
    }
    
protected:
    void on_startup(int argc, char** argv) override {
        // ✅ 在 on_startup 中执行复杂初始化
        load_config(config_path_);
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};
```

**❌ 不推荐写法**：

```cpp
class MyApp : public mine::ui::app::Application {
private:
    ui::Window* main_window_;
    
public:
    MyApp() {
        // ❌ 构造函数中执行复杂操作
        load_config("config.json");
        
        // ❌ 尝试创建窗口（此时 run() 尚未调用，会崩溃）
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        main_window_ = create_window(desc);  // ❌ 崩溃！
    }
};
```

**问题说明**：
- 构造函数中无法访问 `device()` / `renderer()` / `host()`，因为它们尚未创建
- 如果在构造函数中抛出异常，`run()` 方法可能无法正常清理资源
- 将复杂初始化移到 `on_startup()` 可以确保所有子系统已就绪

---

## 7. 常见陷阱

### ❌ 陷阱 1：在 `run()` 之前调用 `create_window()`

**问题描述**：在 `run()` 方法调用之前（如构造函数中）调用 `create_window()`，会导致崩溃或断言失败，因为平台宿主、图形设备、渲染器尚未初始化。

**❌ 错误代码**：

```cpp
class MyApp : public mine::ui::app::Application {
private:
    ui::Window* main_window_ = nullptr;
    
public:
    MyApp() {
        // ❌ 错误：此时 run() 尚未调用，device() 和 renderer() 返回 nullptr
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        
        main_window_ = create_window(desc);  // ❌ 崩溃！内部调用 device() 失败
    }
};

int main() {
    MyApp app;  // 构造函数中崩溃
    return app.run();
}
```

**错误现象**：
- 程序崩溃，提示 "device() returned nullptr"
- 或触发断言失败："MINE_ASSERT(device_ != nullptr)"

**✅ 正确代码**：

```cpp
class MyApp : public mine::ui::app::Application {
protected:
    void on_startup(int argc, char** argv) override {
        // ✅ 正确：在 on_startup 中创建窗口，此时所有子系统已初始化
        mine::platform::WindowDesc desc;
        desc.title = u"My Application";
        desc.size  = {1280.0f, 720.0f};
        
        auto* win = create_window(desc);
        set_main_window(win);
        win->show();
    }
};

int main() {
    MyApp app;
    return app.run();  // on_startup 会在 run() 内部调用
}
```

**原理解释**：
- `run()` 方法的执行顺序是：`on_create_host()` → `on_create_device()` → `on_create_renderer()` → `on_startup()`
- `create_window()` 内部需要访问 `device()` 和 `renderer()`，因此必须在 `on_startup()` 之后调用
- 在构造函数中调用 `create_window()` 会跳过初始化流程，导致访问未初始化的对象

---

### ❌ 陷阱 2：未设置主窗口导致无法退出

**问题描述**：创建主窗口后忘记调用 `set_main_window()`，导致主窗口关闭后应用程序不会自动退出，用户必须手动杀掉进程。

**❌ 错误代码**：

```cpp
void on_startup(int argc, char** argv) override {
    // 创建主窗口
    mine::platform::WindowDesc desc;
    desc.title = u"My Application";
    desc.size  = {1280.0f, 720.0f};
    
    auto* win = create_window(desc);
    // ❌ 忘记设置主窗口语义
    // set_main_window(win);  
    win->show();
}
```

**错误现象**：
- 用户点击主窗口的关闭按钮后，窗口消失，但应用程序仍在后台运行
- 任务管理器中可以看到进程仍然存在
- 用户必须手动杀掉进程（或按 Ctrl+C）

**✅ 正确代码**：

```cpp
void on_startup(int argc, char** argv) override {
    // 创建主窗口
    mine::platform::WindowDesc desc;
    desc.title = u"My Application";
    desc.size  = {1280.0f, 720.0f};
    
    auto* win = create_window(desc);
    set_main_window(win);  // ✅ 设置主窗口语义，关闭时自动退出
    win->show();
}
```

**原理解释**：
- 默认情况下，窗口关闭只会销毁窗口对象，不会退出主循环
- `set_main_window()` 会监听主窗口的关闭事件，自动调用 `quit(0)`
- 如果不设置主窗口，应用程序会一直运行，直到手动调用 `quit()` 或所有窗口都被销毁

---

### ❌ 陷阱 3：主题注册后未调用 `set_theme()`

**问题描述**：调用 `register_theme()` 注册主题后，忘记调用 `set_theme()` 激活主题，导致主题资源不生效。

**❌ 错误代码**：

```cpp
void on_startup(int argc, char** argv) override {
    // 注册 Light 主题
    mine::style::ResourceDictionary light_theme;
    light_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
    light_theme.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFF000000});
    register_theme("Light", std::move(light_theme));
    
    // ❌ 忘记激活主题
    // set_theme("Light");
    
    // 创建主窗口
    auto* win = create_window(desc);
    set_main_window(win);
    win->show();
}
```

**错误现象**：
- 窗口显示后，控件无法找到 "BackgroundBrush" / "ForegroundBrush" 等资源
- 控件使用默认外观（可能是空白或错误的颜色）
- 控制台输出警告："Resource not found: BackgroundBrush"

**✅ 正确代码**：

```cpp
void on_startup(int argc, char** argv) override {
    // 注册 Light 主题
    mine::style::ResourceDictionary light_theme;
    light_theme.set("BackgroundBrush", mine::paint::SolidColorBrush{0xFFFFFFFF});
    light_theme.set("ForegroundBrush", mine::paint::SolidColorBrush{0xFF000000});
    register_theme("Light", std::move(light_theme));
    
    // ✅ 激活主题
    set_theme("Light");  // 将主题资源合并到全局资源字典
    
    // 创建主窗口
    auto* win = create_window(desc);
    set_main_window(win);
    win->show();
}
```

**原理解释**：
- `register_theme()` 只是将主题字典存储在 Application 内部，不会自动应用
- `set_theme()` 会将主题字典合并到全局资源字典，并广播资源变更通知
- 如果不调用 `set_theme()`，主题资源不会被合并到全局资源字典，控件无法访问

---

### ❌ 陷阱 4：资源字典循环引用

**问题描述**：在资源字典中引用自身或形成循环引用链，导致资源查找死循环或栈溢出。

**❌ 错误代码**：

```cpp
void on_startup(int argc, char** argv) override {
    auto& global = global_resources();
    
    // ❌ 错误：资源 A 引用资源 B，资源 B 引用资源 A
    mine::style::ResourceReference ref_a{"ResourceB"};
    mine::style::ResourceReference ref_b{"ResourceA"};
    
    global.set("ResourceA", ref_a);
    global.set("ResourceB", ref_b);
    
    // 尝试查找资源 A
    auto* res = global.find("ResourceA");  // ❌ 死循环或栈溢出！
}
```

**错误现象**：
- 程序卡死或崩溃（栈溢出）
- 控制台输出警告："Circular resource reference detected: ResourceA → ResourceB → ResourceA"

**✅ 正确代码**：

```cpp
void on_startup(int argc, char** argv) override {
    auto& global = global_resources();
    
    // ✅ 正确：资源只引用叶子节点（实际值），不形成循环
    mine::paint::SolidColorBrush base_brush{0xFFFFFFFF};
    mine::style::ResourceReference ref_a{"BaseBrush"};
    
    global.set("BaseBrush", base_brush);  // 叶子节点（实际值）
    global.set("ResourceA", ref_a);       // 引用叶子节点
}
```

**原理解释**：
- 资源字典支持嵌套引用（`ResourceReference`），但不允许循环引用
- 资源查找算法会递归解析引用链，如果形成循环，会导致无限递归
- 正确做法是确保引用链最终指向一个具体值（叶子节点），不形成环

---

## 8. 完整示例

以下是一个完整的 MineUI 应用程序示例，展示了 Application 类的主要功能：主题切换、多窗口管理、MVVM 集成、命令行参数处理等。

```cpp
// ════════════════════════════════════════════════════════════════
//  MyApplication.h - 应用程序主类
// ════════════════════════════════════════════════════════════════

#pragma once
#include <mine/ui/app/AppAll.h>
#include <mine/ui/controls/ControlsAll.h>
#include <mine/di/ServiceCollection.h>
#include <mine/mvvm/ViewModelLocator.h>

namespace myapp {

/**
 * @brief 示例应用程序类
 * 
 * 功能：
 * 1. 支持 Light / Dark 主题切换
 * 2. 支持多窗口（主窗口 + 设置窗口）
 * 3. 使用 MVVM 架构（ViewModel + DataBinding）
 * 4. 支持命令行参数（--theme light/dark）
 */
class MyApplication : public mine::ui::app::Application {
public:
    MyApplication() = default;
    ~MyApplication() override = default;

protected:
    // ── 生命周期回调 ──
    
    /**
     * @brief 应用启动回调
     * @details 执行顺序：
     *   1. 解析命令行参数
     *   2. 配置 DI 容器
     *   3. 注册和激活主题
     *   4. 创建主窗口
     */
    void on_startup(int argc, char** argv) override;
    
    /**
     * @brief 应用退出回调
     * @details 保存应用状态、记录退出日志
     */
    void on_exit(int exit_code) override;

private:
    // ── 初始化方法 ──
    
    /**
     * @brief 解析命令行参数
     * @param argc 参数数量
     * @param argv 参数数组
     */
    void parse_command_line(int argc, char** argv);
    
    /**
     * @brief 配置 DI 容器
     * @details 注册服务、ViewModels 等
     */
    void configure_services();
    
    /**
     * @brief 注册应用主题
     * @details 注册 Light 和 Dark 两种主题
     */
    void register_themes();
    
    /**
     * @brief 创建主窗口
     * @details 创建主窗口、设置 ViewModel、绑定事件
     */
    void create_main_window();
    
    /**
     * @brief 创建设置窗口
     * @details 创建设置窗口（主题切换、关于信息等）
     */
    void create_settings_window();

    // ── 事件处理 ──
    
    /**
     * @brief 主题切换按钮点击事件
     */
    void on_theme_toggle_click();
    
    /**
     * @brief 设置按钮点击事件
     */
    void on_settings_click();

private:
    // ── 成员变量 ──
    mine::di::ServiceCollection services_;       ///< DI 容器
    mine::ui::Window* main_window_ = nullptr;    ///< 主窗口
    mine::ui::Window* settings_window_ = nullptr; ///< 设置窗口
    std::string initial_theme_ = "Light";         ///< 初始主题
};

} // namespace myapp


// ════════════════════════════════════════════════════════════════
//  MyApplication.cpp - 实现文件
// ════════════════════════════════════════════════════════════════

#include "MyApplication.h"
#include <iostream>
#include <cstring>

namespace myapp {

// ────────────────────────────────────────────────────────────────
//  生命周期回调
// ────────────────────────────────────────────────────────────────

void MyApplication::on_startup(int argc, char** argv) {
    std::cout << "Application starting..." << std::endl;
    
    // 1. 解析命令行参数
    parse_command_line(argc, argv);
    
    // 2. 配置 DI 容器
    configure_services();
    
    // 3. 注册和激活主题
    register_themes();
    set_theme(initial_theme_);  // 激活初始主题
    
    // 4. 创建主窗口
    create_main_window();
    
    std::cout << "Application started successfully!" << std::endl;
}

void MyApplication::on_exit(int exit_code) {
    std::cout << "Application exiting with code: " << exit_code << std::endl;
    
    // 保存应用状态
    // ... 保存窗口位置、大小、当前主题等 ...
    
    std::cout << "Application state saved." << std::endl;
}

// ────────────────────────────────────────────────────────────────
//  初始化方法
// ────────────────────────────────────────────────────────────────

void MyApplication::parse_command_line(int argc, char** argv) {
    // 解析命令行参数：--theme light/dark
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--theme") == 0 && i + 1 < argc) {
            std::string theme = argv[++i];
            if (theme == "light") {
                initial_theme_ = "Light";
            } else if (theme == "dark") {
                initial_theme_ = "Dark";
            } else {
                std::cerr << "Unknown theme: " << theme << std::endl;
            }
        }
    }
    
    std::cout << "Initial theme: " << initial_theme_ << std::endl;
}

void MyApplication::configure_services() {
    // 注册服务到 DI 容器
    // services_.add_singleton<IDataService, DataService>();
    // services_.add_transient<MainViewModel>();
    
    std::cout << "DI container configured." << std::endl;
}

void MyApplication::register_themes() {
    // ── 注册 Light 主题 ──
    mine::style::ResourceDictionary light_theme;
    
    // 颜色定义
    light_theme.set("BackgroundColor", mine::paint::Color{0xFFFFFFFF});
    light_theme.set("ForegroundColor", mine::paint::Color{0xFF000000});
    light_theme.set("AccentColor", mine::paint::Color{0xFF007ACC});
    
    // 画刷定义
    light_theme.set("BackgroundBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFFFFFFFF}});
    light_theme.set("ForegroundBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFF000000}});
    light_theme.set("AccentBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFF007ACC}});
    
    // 按钮样式
    light_theme.set("ButtonBackgroundBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFFE0E0E0}});
    light_theme.set("ButtonHoverBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFFD0D0D0}});
    
    register_theme("Light", std::move(light_theme));
    
    // ── 注册 Dark 主题 ──
    mine::style::ResourceDictionary dark_theme;
    
    // 颜色定义
    dark_theme.set("BackgroundColor", mine::paint::Color{0xFF1E1E1E});
    dark_theme.set("ForegroundColor", mine::paint::Color{0xFFFFFFFF});
    dark_theme.set("AccentColor", mine::paint::Color{0xFF0078D7});
    
    // 画刷定义
    dark_theme.set("BackgroundBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFF1E1E1E}});
    dark_theme.set("ForegroundBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFFFFFFFF}});
    dark_theme.set("AccentBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFF0078D7}});
    
    // 按钮样式
    dark_theme.set("ButtonBackgroundBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFF3C3C3C}});
    dark_theme.set("ButtonHoverBrush", 
        mine::paint::SolidColorBrush{mine::paint::Color{0xFF4C4C4C}});
    
    register_theme("Dark", std::move(dark_theme));
    
    std::cout << "Themes registered: Light, Dark" << std::endl;
}

void MyApplication::create_main_window() {
    // ── 创建窗口 ──
    mine::platform::WindowDesc desc;
    desc.title = u"MineUI Application Demo";
    desc.size  = {1280.0f, 720.0f};
    desc.resizable = true;
    desc.show_titlebar = true;
    
    main_window_ = create_window(desc);
    set_main_window(main_window_);  // 主窗口关闭时自动退出
    
    // ── 创建 UI 内容 ──
    using namespace mine::ui::controls;
    
    // 根容器（垂直布局）
    auto* root = new StackPanel();
    root->set_orientation(Orientation::Vertical);
    root->set_spacing(20.0f);
    root->set_padding(mine::ui::Thickness{20.0f});
    
    // 标题文本
    auto* title = new TextBlock();
    title->set_text(u"Welcome to MineUI!");
    title->set_font_size(32.0f);
    title->set_font_weight(mine::text::FontWeight::Bold);
    // 使用主题资源
    title->bind_property("Foreground", "{ThemeResource ForegroundBrush}");
    
    // 描述文本
    auto* desc_text = new TextBlock();
    desc_text->set_text(u"This is a demo application showcasing MineUI features.");
    desc_text->set_font_size(16.0f);
    desc_text->bind_property("Foreground", "{ThemeResource ForegroundBrush}");
    
    // 主题切换按钮
    auto* theme_button = new Button();
    theme_button->set_content(u"Toggle Theme");
    theme_button->set_width(200.0f);
    theme_button->set_height(40.0f);
    theme_button->clicked().connect([this]() { on_theme_toggle_click(); });
    
    // 设置按钮
    auto* settings_button = new Button();
    settings_button->set_content(u"Settings");
    settings_button->set_width(200.0f);
    settings_button->set_height(40.0f);
    settings_button->clicked().connect([this]() { on_settings_click(); });
    
    // 组装 UI
    root->add_child(title);
    root->add_child(desc_text);
    root->add_child(theme_button);
    root->add_child(settings_button);
    
    main_window_->set_content(root);
    
    // 绑定窗口背景到主题资源
    main_window_->bind_property("Background", "{ThemeResource BackgroundBrush}");
    
    // 显示窗口
    main_window_->show();
    
    std::cout << "Main window created." << std::endl;
}

void MyApplication::create_settings_window() {
    // 如果设置窗口已存在，直接激活
    if (settings_window_ != nullptr) {
        settings_window_->activate();
        return;
    }
    
    // ── 创建窗口 ──
    mine::platform::WindowDesc desc;
    desc.title = u"Settings";
    desc.size  = {600.0f, 400.0f};
    desc.resizable = false;
    desc.show_titlebar = true;
    
    settings_window_ = create_window(desc);
    
    // ── 创建 UI 内容 ──
    using namespace mine::ui::controls;
    
    auto* root = new StackPanel();
    root->set_orientation(Orientation::Vertical);
    root->set_spacing(15.0f);
    root->set_padding(mine::ui::Thickness{20.0f});
    
    // 标题
    auto* title = new TextBlock();
    title->set_text(u"Application Settings");
    title->set_font_size(24.0f);
    title->set_font_weight(mine::text::FontWeight::Bold);
    title->bind_property("Foreground", "{ThemeResource ForegroundBrush}");
    
    // 主题选择
    auto* theme_label = new TextBlock();
    theme_label->set_text(u"Theme:");
    theme_label->set_font_size(16.0f);
    theme_label->bind_property("Foreground", "{ThemeResource ForegroundBrush}");
    
    auto* light_button = new Button();
    light_button->set_content(u"Light");
    light_button->set_width(100.0f);
    light_button->clicked().connect([this]() { set_theme("Light"); });
    
    auto* dark_button = new Button();
    dark_button->set_content(u"Dark");
    dark_button->set_width(100.0f);
    dark_button->clicked().connect([this]() { set_theme("Dark"); });
    
    // 关于信息
    auto* about_text = new TextBlock();
    about_text->set_text(u"MineUI Application Demo v1.0.0\n© 2024 MineFramework");
    about_text->set_font_size(14.0f);
    about_text->bind_property("Foreground", "{ThemeResource ForegroundBrush}");
    
    // 组装 UI
    root->add_child(title);
    root->add_child(theme_label);
    
    auto* theme_panel = new StackPanel();
    theme_panel->set_orientation(Orientation::Horizontal);
    theme_panel->set_spacing(10.0f);
    theme_panel->add_child(light_button);
    theme_panel->add_child(dark_button);
    root->add_child(theme_panel);
    
    root->add_child(about_text);
    
    settings_window_->set_content(root);
    settings_window_->bind_property("Background", "{ThemeResource BackgroundBrush}");
    
    // 窗口关闭时清空指针
    settings_window_->closed().connect([this]() {
        settings_window_ = nullptr;
    });
    
    // 显示窗口
    settings_window_->show();
    
    std::cout << "Settings window created." << std::endl;
}

// ────────────────────────────────────────────────────────────────
//  事件处理
// ────────────────────────────────────────────────────────────────

void MyApplication::on_theme_toggle_click() {
    // 切换主题：Light ↔ Dark
    if (current_theme() == "Light") {
        set_theme("Dark");
        std::cout << "Theme switched to Dark." << std::endl;
    } else {
        set_theme("Light");
        std::cout << "Theme switched to Light." << std::endl;
    }
}

void MyApplication::on_settings_click() {
    // 打开设置窗口
    create_settings_window();
}

} // namespace myapp


// ════════════════════════════════════════════════════════════════
//  main.cpp - 进程入口
// ════════════════════════════════════════════════════════════════

#include "MyApplication.h"
#include <mine/ui/app/ApplicationMain.h>

// 使用 MINE_APPLICATION_MAIN 宏生成标准 main 函数
MINE_APPLICATION_MAIN(myapp::MyApplication)

// 等价于：
//
// int main(int argc, char** argv) {
//     myapp::MyApplication app;
//     return app.run(argc, argv);
// }


// ════════════════════════════════════════════════════════════════
//  CMakeLists.txt - 构建配置
// ════════════════════════════════════════════════════════════════

/*
cmake_minimum_required(VERSION 3.20)
project(MyApplication)

# 查找 MineFramework
find_package(MineFramework REQUIRED)

# 创建可执行文件
add_executable(MyApplication
    MyApplication.h
    MyApplication.cpp
    main.cpp
)

# 链接 MineFramework 库
target_link_libraries(MyApplication PRIVATE
    MineFramework::mine.ui.app
    MineFramework::mine.ui.controls
    MineFramework::mine.di
    MineFramework::mine.mvvm
)

# 设置 C++ 标准
set_property(TARGET MyApplication PROPERTY CXX_STANDARD 20)
*/

// ════════════════════════════════════════════════════════════════
//  运行效果
// ════════════════════════════════════════════════════════════════

/*
1. 编译并运行：

   $ cmake -B build
   $ cmake --build build
   $ ./build/MyApplication

2. 使用命令行参数启动：

   $ ./build/MyApplication --theme dark

3. 预期效果：
   - 程序启动后显示主窗口（使用 Light 或 Dark 主题）
   - 点击 "Toggle Theme" 按钮可以在 Light 和 Dark 之间切换
   - 点击 "Settings" 按钮打开设置窗口
   - 在设置窗口中点击 "Light" 或 "Dark" 按钮切换主题
   - 关闭主窗口后，应用程序自动退出
   - 关闭设置窗口后，主窗口继续运行

4. 控制台输出：

   Application starting...
   Initial theme: Light
   DI container configured.
   Themes registered: Light, Dark
   Main window created.
   Application started successfully!
   Theme switched to Dark.
   Settings window created.
   Theme switched to Light.
   Application exiting with code: 0
   Application state saved.
*/
```

---

## 9. 总结

### 9.1 核心要点回顾

- **Application 是应用程序的入口点**：负责初始化所有子系统（平台宿主、图形设备、渲染器），驱动主消息循环，管理窗口生命周期。
- **生命周期顺序**：`on_create_host()` → `on_create_device()` → `on_create_renderer()` → `on_startup()` → 主循环 → `on_exit()` → 析构。
- **窗口管理**：通过 `create_window()` 创建窗口，通过 `set_main_window()` 设置主窗口语义（主窗口关闭时自动退出）。
- **主题系统**：通过 `register_theme()` 注册主题，通过 `set_theme()` 激活主题，支持动态切换并自动广播资源变更通知。
- **资源管理**：全局资源字典是应用级根字典，所有窗口/控件的资源字典向上连接到此字典，支持主题资源合并。
- **扩展点机制**：子类重写 `on_startup()` / `on_exit()` 实现自定义逻辑，重写 `on_create_*()` 支持测试注入。
- **线程安全性**：`quit()` 和 `set_theme()` 是线程安全的，其他方法必须在主线程调用。
- **最佳实践**：在 `on_startup()` 中创建窗口、设置主窗口语义、合理管理资源字典、避免在构造函数中初始化窗口。
- **常见陷阱**：在 `run()` 之前调用 `create_window()`、未设置主窗口导致无法退出、主题注册后未调用 `set_theme()`、资源字典循环引用。

### 9.2 与其他模块的协作关系

```
Application ↔ IApplicationHost (平台宿主)
    ├─ 创建平台窗口句柄
    ├─ 驱动主消息循环
    └─ 管理平台事件

Application ↔ IDevice (图形设备)
    ├─ 创建渲染上下文
    ├─ 分配 GPU 资源
    └─ 提交渲染命令

Application ↔ IRenderer (渲染器)
    ├─ 绘制 Visual 树
    ├─ 执行合成
    └─ 提交帧缓冲

Application ↔ Window (窗口)
    ├─ 创建和销毁窗口
    ├─ 管理窗口生命周期
    └─ 广播资源变更通知

Application ↔ ResourceDictionary (资源字典)
    ├─ 提供全局资源字典
    ├─ 合并主题资源
    └─ 广播资源变更通知
```

### 9.3 适用场景总结

- **单窗口应用**：最简单的场景，只需在 `on_startup()` 中创建主窗口并调用 `show()`。
- **多窗口应用**：创建多个窗口，设置主窗口语义，其他窗口关闭时不影响应用退出。
- **MVVM 应用**：集成 DI 容器，在 `on_startup()` 中注册服务、创建 ViewModel、绑定 DataContext。
- **主题切换**：注册多个主题，通过按钮或菜单动态切换，支持 Light / Dark 模式。
- **自定义资源**：在全局资源字典中定义应用级共享资源（如品牌颜色、字体等）。
- **测试注入**：重写 `on_create_*()` 方法注入 mock 对象，支持无头测试（不依赖真实平台）。
- **命令行参数**：在 `on_startup()` 中解析命令行参数，根据参数决定应用行为。
- **异步加载**：显示启动画面，异步加载资源，加载完成后显示主窗口。

### 9.4 下一步学习建议

- **ui::Window**：学习窗口类的详细接口，了解窗口内容管理、事件处理、资源字典等。
- **style::ResourceDictionary**：深入理解资源字典的合并机制、资源查找算法、资源变更通知等。
- **platform::IApplicationHost**：了解平台抽象层的设计，学习如何扩展到新平台。
- **gfx::IDevice**：学习图形设备抽象层，了解 Direct3D / Metal / Vulkan 的统一封装。
- **paint::IRenderer**：学习渲染器接口，了解 Skia 集成、合成管线、脏区管理等。
- **MINE_APPLICATION_MAIN 宏**：学习宏的详细实现，了解如何生成跨平台的进程入口。
- **DI 容器集成**：学习如何在 Application 中集成 DI 容器，管理服务生命周期。
- **MVVM 架构**：学习如何在 Application 中集成 MVVM，实现数据绑定、命令绑定等。

---

**相关文档**：
- [02-ApplicationMain.md](02-ApplicationMain.md) - MINE_APPLICATION_MAIN 宏详解
- [03-ModuleMetadata.md](03-ModuleMetadata.md) - Api.h 和 AppAll.h 详解
- [ui.window/01-Window.md](../ui.window/01-Window.md) - Window 类详细接口
- [ui.style/01-ResourceDictionary.md](../ui.style/01-ResourceDictionary.md) - 资源字典详细接口
- [platform/01-IApplicationHost.md](../platform/01-IApplicationHost.md) - 平台宿主接口
- [gfx/01-IDevice.md](../gfx/01-IDevice.md) - 图形设备接口
- [paint/01-IRenderer.md](../paint/01-IRenderer.md) - 渲染器接口
