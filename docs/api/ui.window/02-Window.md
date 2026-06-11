# Window - 应用窗口类

## 1. 概述

`Window` 是 `mine.ui.window` 模块的核心类,负责将 UI 视觉树与平台原生窗口及渲染管线桥接。它继承自 `DependencyObject`,支持依赖属性系统和数据绑定,提供了完整的窗口生命周期管理、自定义 Chrome、输入路由、布局渲染等功能。

### 核心功能

- **平台窗口桥接**：封装 `platform::IWindow`,提供跨平台的窗口操作接口
- **视觉树渲染**：持有内容根(`UIElement`),负责布局(Measure/Arrange)和渲染(Present)
- **依赖属性支持**：8 个依赖属性(DataContext/Padding/IsCustomChrome/CaptionHeight 等)
- **懒初始化模式**：支持 pending 状态构造,首次 `show()` 时通过 IWindowContext 获取资源
- **自定义 Chrome**：支持自定义标题栏、拖拽区域、DWM 玻璃帧效果
- **数据上下文传播**：DataContextProperty 支持 `inherits=true`,自动向子元素传播
- **输入路由**：持有 `InputRouter`,处理键盘、鼠标、触摸等输入事件
- **窗口状态管理**：支持 Normal/Minimized/Maximized 状态切换

### 关键特性

- **两种构造方式**：
  - **路径 A(推荐)**：无参构造 + pending 状态 + show() 懒初始化
  - **路径 B(框架内部)**：带参构造,立即绑定已创建的原生窗口和图形设备
- **所有权清晰**：
  - 路径 A：Window 持有 `platform::IWindow` 的所有权(`OwnedPtr`)
  - 路径 B：Application 持有 `platform::IWindow`,Window 仅持有指针
  - `content`(UIElement*) 由应用层管理,Window 不拥有其生命周期
  - `ISwapchain` 由 Window 自行创建并独占持有
- **自动响应平台事件**：
  - `Resized` 事件 → 驱动两遍布局(Measure + Arrange) + 重新渲染
  - `DpiChanged` 事件 → 更新布局参数 + 重新渲染
  - `Closed` 事件 → 标记 `is_closed()` 为 true
- **线程安全性**：所有方法必须在主线程调用(平台窗口 API 限制)

### 依赖关系

```
┌────────────────────────────────────────────────────────────────┐
│                       mine.ui.property                         │
│  DependencyObject (base class)                                 │
│  - 依赖属性系统                                                │
│  - GetValue / SetValue                                         │
└────────────────┬───────────────────────────────────────────────┘
                 │ inherits
                 ▼
┌────────────────────────────────────────────────────────────────┐
│                       mine.ui.window                           │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ Window                                                   │ │
│  │  - 8 个依赖属性                                         │ │
│  │  - content (UIElement*)                                 │ │
│  │  - native_window (IWindow*)                             │ │
│  │  - swapchain (OwnedPtr)                                 │ │
│  │  - input_router (OwnedPtr)                              │ │
│  └────┬─────────────────────────────────────────────────────┘ │
│       │ uses                                                   │
└───────┼────────────────────────────────────────────────────────┘
        │
        ├─→ platform::IWindow      (平台原生窗口)
        ├─→ gfx::IDevice           (通过 IWindowContext 获取)
        ├─→ gfx::IQueue            (通过 IWindowContext 获取)
        ├─→ gfx::ISwapchain        (Window 持有所有权)
        ├─→ paint::IRenderer       (通过 IWindowContext 获取)
        ├─→ input::InputRouter     (Window 持有所有权)
        └─→ ui::UIElement          (content,应用层管理)
```

### 与其他框架对比

| 框架 | 窗口类 | 内容属性 | 自定义 Chrome | 数据上下文 |
|------|-------|---------|--------------|-----------|
| **WPF** | `Window` | `Content` | 通过 `WindowChrome` 附加属性 | `DataContext` (inherits) |
| **Avalonia** | `Window` | `Content` | 通过 `ExtendClientAreaToDecorationsHint` | `DataContext` (inherits) |
| **Qt** | `QMainWindow` | `setCentralWidget()` | `setWindowFlags(Qt::FramelessWindowHint)` | 无内置支持 |
| **MineFramework** | `Window` | `set_content()` | `IsCustomChrome` + `CaptionHeight` + `drag()` | `DataContext` (inherits) |

MineFramework 的设计优势：
- **更灵活的 Chrome 控制**：提供细粒度的 `CaptionHeight`、`ResizeBorderThickness`、`GlassFrameThickness` 等属性
- **统一的依赖属性系统**：所有窗口属性都是依赖属性,支持绑定和动画
- **更清晰的生命周期**：pending 模式允许先配置窗口属性,再一次性初始化

---

## 2. 文件位置

**头文件路径**：
```
src/mine/ui/window/api/include/mine/ui/window/Window.h
```

**依赖模块**：
- `mine.core`：OwnedPtr、Variant、StringView 等基础类型
- `mine.ui.property`：DependencyObject、DependencyProperty 依赖属性系统
- `mine.ui.visual`：UIElement 视觉树基类
- `mine.ui.input`：InputRouter 输入路由
- `mine.platform.abi`：IWindow、WindowDesc、WindowState、WindowCornerPreference 等平台抽象
- `mine.gfx.rhi`：IDevice、IQueue、ISwapchain 等图形设备抽象
- `mine.paint`：IRenderer 渲染器抽象
- `mine.math`：Size、Thickness、Matrix3x2 等数学类型

**相关文件**：
- `WindowContext.h`：IWindowContext 接口(Window 通过此接口获取资源)
- `Application.h`(mine.ui.app)：Window 的典型使用者

---

## 3. 类定义

```cpp
namespace mine::ui {

/// @brief 应用窗口类
/// @details 
/// 桥接视觉树与平台原生窗口,负责布局、渲染、输入路由等。
/// 支持两种构造方式:
/// - 路径 A(推荐): 无参构造 + pending 状态 + show() 懒初始化
/// - 路径 B(框架内部): 带参构造,立即绑定已创建的原生窗口
/// 
/// 线程安全性:
/// - 所有方法必须在主线程调用(平台窗口 API 限制)
/// - 不支持从多个线程访问同一个 Window 实例
class MINE_UI_WINDOW_API Window : public DependencyObject {
public:
    // ========== 依赖属性 ==========
    
    /// @brief DataContext 依赖属性
    /// @details 
    /// - 类型: core::Variant
    /// - 默认值: Variant::empty()
    /// - inherits: true (向子元素传播)
    /// - 用于 MVVM 模式,绑定到 ViewModel
    static const DependencyProperty& DataContextProperty;
    
    /// @brief Padding 依赖属性
    /// @details 
    /// - 类型: math::Thickness
    /// - 默认值: {0, 0, 0, 0}
    /// - 内容根相对于窗口客户区的内边距
    static const DependencyProperty& PaddingProperty;
    
    /// @brief IsCustomChrome 依赖属性
    /// @details 
    /// - 类型: bool
    /// - 默认值: false
    /// - 是否使用自定义标题栏(隐藏系统标题栏)
    static const DependencyProperty& IsCustomChromeProperty;
    
    /// @brief CaptionHeight 依赖属性
    /// @details 
    /// - 类型: float
    /// - 默认值: 32.0f
    /// - 自定义标题栏的可拖拽区域高度(像素)
    /// - 仅当 IsCustomChrome=true 时生效
    static const DependencyProperty& CaptionHeightProperty;
    
    /// @brief ResizeBorderThickness 依赖属性
    /// @details 
    /// - 类型: math::Thickness
    /// - 默认值: {8, 8, 8, 8}
    /// - 窗口边缘的可调整大小边框厚度(像素)
    /// - 仅当 IsCustomChrome=true 时生效
    static const DependencyProperty& ResizeBorderThicknessProperty;
    
    /// @brief CornerPreference 依赖属性
    /// @details 
    /// - 类型: platform::WindowCornerPreference
    /// - 默认值: WindowCornerPreference::Default
    /// - 窗口圆角首选项(Default/DoNotRound/Round/RoundSmall)
    /// - 仅 Windows 11+ 支持
    static const DependencyProperty& CornerPreferenceProperty;
    
    /// @brief GlassFrameThickness 依赖属性
    /// @details 
    /// - 类型: math::Thickness
    /// - 默认值: {0, 0, 0, 0}
    /// - DWM 玻璃帧厚度(像素)
    /// - 仅 Windows Vista+ 支持,用于 Aero 玻璃效果
    static const DependencyProperty& GlassFrameThicknessProperty;
    
    /// @brief WindowState 依赖属性
    /// @details 
    /// - 类型: platform::WindowState
    /// - 默认值: WindowState::Normal
    /// - 窗口状态(Normal/Minimized/Maximized)
    static const DependencyProperty& WindowStateProperty;
    
    // ========== 构造/析构 ==========
    
    /// @brief 无参构造(推荐)
    /// @details 
    /// - 创建 pending 状态的 Window
    /// - 通过 set_title/set_size 等配置窗口属性
    /// - 调用 show() 时通过 IWindowContext 懒初始化
    /// - 适合将 Window 作为应用类的值类型成员直接声明
    Window();
    
    /// @brief 带参构造(框架内部)
    /// @param native_window 原生窗口引用(所有权由调用者管理)
    /// @param device 图形设备引用(所有权由 Application 管理)
    /// @param queue 图形队列引用(所有权由 Application 管理)
    /// @param renderer 渲染器引用(所有权由 Application 管理)
    /// @details 
    /// - 立即初始化,无 pending 状态
    /// - 通常由 Application 在内部使用
    /// - Window 不拥有 native_window 的所有权,仅持有指针
    Window(platform::IWindow& native_window,
           gfx::IDevice& device,
           gfx::IQueue& queue,
           paint::IRenderer& renderer);
    
    /// @brief 析构函数
    /// @details 
    /// - 释放 swapchain、input_router 等资源
    /// - 如果 Window 持有 native_window 所有权(路径 A),会自动销毁
    virtual ~Window();
    
    // ========== 数据上下文 ==========
    
    /// @brief 设置数据上下文
    /// @param ctx 数据上下文(通常为 ViewModel 指针)
    /// @details 
    /// - DataContextProperty 支持 inherits=true,自动向子元素传播
    /// - 配合 Binding 实现 MVVM 模式
    void set_data_context(const core::Variant& ctx);
    
    /// @brief 获取数据上下文
    /// @return 数据上下文(可能为空)
    [[nodiscard]] const core::Variant& data_context() const noexcept;
    
    // ========== 内边距 ==========
    
    /// @brief 设置内边距
    /// @param padding 内边距(左、上、右、下)
    /// @details 
    /// - 内容根相对于窗口客户区的内边距
    /// - 影响布局计算,不影响窗口尺寸
    void set_padding(const math::Thickness& padding);
    
    /// @brief 获取内边距
    /// @return 内边距
    [[nodiscard]] math::Thickness padding() const noexcept;
    
    // ========== 自定义 Chrome ==========
    
    /// @brief 启用/禁用自定义标题栏
    /// @param enabled true 启用,false 禁用
    /// @details 
    /// - 启用后隐藏系统标题栏,由应用层自行绘制
    /// - 配合 CaptionHeight/ResizeBorderThickness 使用
    /// - 仅 pending 状态下设置有效,show() 后无法更改
    void set_custom_chrome(bool enabled);
    
    /// @brief 是否启用自定义标题栏
    /// @return true 已启用,false 未启用
    [[nodiscard]] bool is_custom_chrome() const noexcept;
    
    /// @brief 设置标题栏高度
    /// @param height 高度(像素)
    /// @details 
    /// - 仅当 IsCustomChrome=true 时生效
    /// - 顶部区域高度 <= height 时可拖拽窗口
    void set_caption_height(float height);
    
    /// @brief 获取标题栏高度
    /// @return 高度(像素)
    [[nodiscard]] float caption_height() const noexcept;
    
    /// @brief 设置调整大小边框厚度
    /// @param thickness 厚度(左、上、右、下)
    /// @details 
    /// - 仅当 IsCustomChrome=true 时生效
    /// - 窗口边缘区域厚度 <= thickness 时可拖拽边框调整窗口大小
    void set_resize_border_thickness(const math::Thickness& thickness);
    
    /// @brief 获取调整大小边框厚度
    /// @return 厚度
    [[nodiscard]] math::Thickness resize_border_thickness() const noexcept;
    
    /// @brief 设置窗口圆角首选项
    /// @param pref 圆角首选项(Default/DoNotRound/Round/RoundSmall)
    /// @details 
    /// - 仅 Windows 11+ 支持
    /// - Default: 遵循系统设置
    /// - DoNotRound: 强制直角
    /// - Round: 标准圆角
    /// - RoundSmall: 小圆角
    void set_corner_preference(platform::WindowCornerPreference pref);
    
    /// @brief 获取窗口圆角首选项
    /// @return 圆角首选项
    [[nodiscard]] platform::WindowCornerPreference corner_preference() const noexcept;
    
    /// @brief 设置 DWM 玻璃帧厚度
    /// @param thickness 厚度(左、上、右、下)
    /// @details 
    /// - 仅 Windows Vista+ 支持
    /// - 启用 Aero 玻璃效果(背景模糊)
    /// - 设置为 {-1, -1, -1, -1} 表示全窗口玻璃效果
    void set_glass_frame_thickness(const math::Thickness& thickness);
    
    /// @brief 获取 DWM 玻璃帧厚度
    /// @return 厚度
    [[nodiscard]] math::Thickness glass_frame_thickness() const noexcept;
    
    /// @brief 编程方式拖拽窗口
    /// @details 
    /// - 类似 WPF 的 Window.DragMove()
    /// - 通常在标题栏的 MouseDown 事件中调用
    /// - 必须在主线程调用
    void drag();
    
    // ========== 窗口状态 ==========
    
    /// @brief 设置窗口状态
    /// @param state 窗口状态(Normal/Minimized/Maximized)
    void set_window_state(platform::WindowState state);
    
    /// @brief 获取窗口状态
    /// @return 窗口状态
    [[nodiscard]] platform::WindowState window_state() const noexcept;
    
    // ========== 内容根 ==========
    
    /// @brief 设置内容根
    /// @param element 内容根元素(nullptr 表示清空)
    /// @details 
    /// - Window 不拥有 element 的生命周期,应用层需自行管理
    /// - 设置新内容根会触发重新布局和渲染
    void set_content(ui::UIElement* element);
    
    /// @brief 获取内容根
    /// @return 内容根元素(可能为 nullptr)
    [[nodiscard]] ui::UIElement* content() const noexcept;
    
    // ========== 平台窗口委托 ==========
    
    /// @brief 显示窗口
    /// @details 
    /// - pending 状态下首次调用触发懒初始化(通过 IWindowContext)
    /// - 已初始化的窗口直接显示
    /// - 必须在 Application::run() 之后调用
    void show();
    
    /// @brief 隐藏窗口
    /// @details 
    /// - 窗口不可见但未销毁,可再次 show()
    void hide();
    
    /// @brief 关闭窗口
    /// @details 
    /// - 触发 platform::IWindow::on_closed() 回调
    /// - 标记 is_closed() 为 true
    /// - Window 对象仍然存在,但不可再次 show()
    void close();
    
    /// @brief 设置窗口标题
    /// @param title 标题字符串
    /// @details 
    /// - pending 状态下设置缓存到内部,show() 时应用
    /// - 已初始化的窗口立即更新标题
    void set_title(core::StringView title);
    
    /// @brief 设置窗口尺寸
    /// @param size 尺寸(宽、高)
    /// @details 
    /// - pending 状态下设置缓存到内部,show() 时应用
    /// - 已初始化的窗口立即更新尺寸
    void set_size(math::Size size);
    
    /// @brief 设置窗口是否可调整大小
    /// @param resizable true 可调整,false 不可调整
    /// @details 
    /// - 仅 pending 状态下设置有效,show() 后无法更改
    void set_resizable(bool resizable);
    
    /// @brief 设置窗口是否自动定位
    /// @param auto_position true 自动定位(屏幕中央),false 手动定位
    /// @details 
    /// - 仅 pending 状态下设置有效,show() 后无法更改
    void set_auto_position(bool auto_position);
    
    /// @brief 设置窗口类型
    /// @param kind 窗口类型(Main/Popup/Tool)
    /// @details 
    /// - 仅 pending 状态下设置有效,show() 后无法更改
    void set_kind(platform::WindowKind kind);
    
    /// @brief 获取窗口尺寸
    /// @return 尺寸(宽、高)
    [[nodiscard]] math::Size size() const;
    
    /// @brief 获取窗口 DPI
    /// @return DPI 缩放比例(例如 1.0 表示 96 DPI, 1.5 表示 144 DPI)
    [[nodiscard]] float dpi() const;
    
    /// @brief 窗口是否已关闭
    /// @return true 已关闭,false 未关闭
    [[nodiscard]] bool is_closed() const noexcept;
    
    /// @brief 窗口是否处于 pending 状态
    /// @return true 处于 pending 状态,false 已初始化
    [[nodiscard]] bool is_pending() const noexcept;
    
    // ========== 平台窗口访问 ==========
    
    /// @brief 获取原生窗口
    /// @return 原生窗口引用
    /// @note 仅在已初始化状态下调用,pending 状态下会断言失败
    [[nodiscard]] platform::IWindow& native_window() noexcept;
    
    // ========== 输入路由 ==========
    
    /// @brief 获取输入路由器
    /// @return 输入路由器引用
    /// @note 仅在已初始化状态下调用,pending 状态下会断言失败
    [[nodiscard]] input::InputRouter& input_router() noexcept;
    
    /// @brief 获取 IME 服务
    /// @return IME 服务引用
    /// @note 仅在已初始化状态下调用,pending 状态下会断言失败
    [[nodiscard]] platform::IMEService& ime() noexcept;
    
    /// @brief 设置输入处理完成回调
    /// @param fn 回调函数(无参数,无返回值)
    /// @details 
    /// - 每次 InputRouter 处理完输入事件后调用
    /// - 通常用于触发渲染或更新 UI
    void set_on_input_processed(std::function<void()> fn);
    
    // ========== 渲染 ==========
    
    /// @brief 渲染窗口
    /// @details 
    /// - 执行 Measure + Arrange 两遍布局
    /// - 将视觉树渲染到交换链后缓冲
    /// - 调用 Present 显示到屏幕
    void render();
    
    // ========== 静态辅助 ==========
    
    /// @brief 从 UIElement 获取所属 Window
    /// @param element UI 元素
    /// @return 所属 Window(未找到返回 nullptr)
    /// @details 
    /// - 向上遍历视觉树,直到找到 Window 或到达根节点
    [[nodiscard]] static Window* from_element(ui::UIElement* element) noexcept;

private:
    // ========== 私有成员 ==========
    
    // pending 状态标志
    bool m_is_pending = true;
    
    // 平台窗口(路径 A: OwnedPtr, 路径 B: 裸指针)
    core::OwnedPtr<platform::IWindow> m_owned_native_window;
    platform::IWindow*                m_native_window = nullptr;
    
    // 图形资源(由 IWindowContext 提供,Window 仅持有指针)
    gfx::IDevice*      m_device   = nullptr;
    gfx::IQueue*       m_queue    = nullptr;
    paint::IRenderer*  m_renderer = nullptr;
    
    // 交换链(Window 持有所有权)
    core::OwnedPtr<gfx::ISwapchain> m_swapchain;
    
    // 输入路由(Window 持有所有权)
    core::OwnedPtr<input::InputRouter> m_input_router;
    
    // 内容根(应用层管理,Window 仅持有指针)
    ui::UIElement* m_content = nullptr;
    
    // pending 状态下的配置缓存
    struct PendingConfig {
        std::string             title         = "MineFramework Window";
        math::Size              size          = {800, 600};
        bool                    resizable     = true;
        bool                    auto_position = true;
        platform::WindowKind    kind          = platform::WindowKind::Main;
        bool                    custom_chrome = false;
    } m_pending_config;
    
    // 关闭标志
    bool m_is_closed = false;
    
    // 输入处理完成回调
    std::function<void()> m_on_input_processed;
    
    // ========== 私有方法 ==========
    
    // 从 IWindowContext 懒初始化
    void initialize_from_context(IWindowContext& ctx);
    
    // 注册平台窗口事件
    void register_native_events();
    
    // Resized 事件处理
    void on_native_resized(uint32_t width, uint32_t height);
    
    // DpiChanged 事件处理
    void on_native_dpi_changed(float dpi);
    
    // Closed 事件处理
    void on_native_closed();
};

} // namespace mine::ui
```

---

## 4. 成员方法

### 4.1 构造/析构

#### Window()（无参构造）

**功能说明**：
创建 pending 状态的 Window,不立即创建原生窗口。通过 `set_title`/`set_size` 等配置窗口属性,首次调用 `show()` 时通过 IWindowContext 懒初始化。

**参数列表**：无

**返回值**：无

**后置条件**：
- `is_pending()` 返回 true
- 原生窗口尚未创建
- 可以通过 `set_title`/`set_size` 等配置窗口属性

**线程安全性**：必须在主线程调用

**示例**：
```cpp
// 推荐方式:无参构造 + pending 配置 + show
ui::Window window;
window.set_title("My Application");
window.set_size({1024, 768});
window.set_custom_chrome(true);
window.show();  // 触发懒初始化
```

---

#### Window(IWindow&, IDevice&, IQueue&, IRenderer&)（带参构造）

**功能说明**：
立即初始化 Window,绑定已创建的原生窗口和图形设备。通常由 Application 在内部使用。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `platform::IWindow&` | `native_window` | 原生窗口引用(所有权由调用者管理) |
| `gfx::IDevice&` | `device` | 图形设备引用(所有权由 Application 管理) |
| `gfx::IQueue&` | `queue` | 图形队列引用(所有权由 Application 管理) |
| `paint::IRenderer&` | `renderer` | 渲染器引用(所有权由 Application 管理) |

**返回值**：无

**前置条件**：
- `native_window` 必须已创建且有效
- `device`/`queue`/`renderer` 必须已初始化且生命周期长于 Window

**后置条件**：
- `is_pending()` 返回 false
- 原生窗口已绑定,可以调用 `native_window()`
- 交换链已创建,可以调用 `render()`

**线程安全性**：必须在主线程调用

**示例**：
```cpp
// 框架内部使用方式
platform::IWindow& native = /* ... */;
gfx::IDevice&      device = /* ... */;
gfx::IQueue&       queue  = /* ... */;
paint::IRenderer&  renderer = /* ... */;

ui::Window window(native, device, queue, renderer);
window.set_content(my_content);
window.render();
```

---

#### ~Window()

**功能说明**：
销毁 Window,释放交换链、输入路由等资源。如果 Window 持有原生窗口所有权(路径 A),会自动销毁原生窗口。

**参数列表**：无

**返回值**：无

**后置条件**：
- 交换链已销毁
- 输入路由已销毁
- 如果持有原生窗口所有权,原生窗口已销毁

**线程安全性**：必须在主线程调用

---

### 4.2 数据上下文

#### set_data_context()

**功能说明**：
设置数据上下文,通常绑定到 ViewModel 实例。DataContextProperty 支持 `inherits=true`,会自动向子元素传播。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `const core::Variant&` | `ctx` | 数据上下文(通常为 ViewModel 指针) |

**返回值**：无

**后置条件**：
- `data_context()` 返回 `ctx`
- 所有子元素的 DataContext 自动继承(如果未显式设置)

**线程安全性**：必须在主线程调用

**示例**：
```cpp
class MyViewModel {
public:
    std::string title = "Hello, MineFramework!";
};

MyViewModel vm;
ui::Window window;
window.set_data_context(&vm);  // 绑定 ViewModel

// 子元素自动继承 DataContext
auto text = core::make_owned<ui::TextBlock>();
text->set_binding(ui::TextBlock::TextProperty, "title");  // 绑定到 vm.title
window.set_content(text.get());
```

---

#### data_context()

**功能说明**：
获取当前数据上下文。

**参数列表**：无

**返回值**：
- `const core::Variant&`：数据上下文(可能为空)

**线程安全性**：必须在主线程调用

---

### 4.3 内边距

#### set_padding()

**功能说明**：
设置内容根相对于窗口客户区的内边距。影响布局计算,不影响窗口尺寸。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `const math::Thickness&` | `padding` | 内边距(左、上、右、下) |

**返回值**：无

**后置条件**：
- `padding()` 返回 `padding`
- 内容根的可用空间减少,触发重新布局

**线程安全性**：必须在主线程调用

**示例**：
```cpp
ui::Window window;
window.set_padding({20, 10, 20, 10});  // 左右各 20 像素,上下各 10 像素
```

---

#### padding()

**功能说明**：
获取当前内边距。

**参数列表**：无

**返回值**：
- `math::Thickness`：内边距

**线程安全性**：必须在主线程调用

---

### 4.4 自定义 Chrome

#### set_custom_chrome()

**功能说明**：
启用/禁用自定义标题栏。启用后隐藏系统标题栏,由应用层自行绘制标题栏和窗口控制按钮。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `bool` | `enabled` | true 启用,false 禁用 |

**返回值**：无

**前置条件**：
- 仅 pending 状态下设置有效,show() 后无法更改

**后置条件**：
- `is_custom_chrome()` 返回 `enabled`
- 启用后系统标题栏隐藏,需要应用层绘制

**线程安全性**：必须在主线程调用

**示例**：
```cpp
ui::Window window;
window.set_custom_chrome(true);  // 启用自定义标题栏
window.set_caption_height(40);   // 设置标题栏高度
window.show();
```

---

#### is_custom_chrome()

**功能说明**：
查询是否启用自定义标题栏。

**参数列表**：无

**返回值**：
- `bool`：true 已启用,false 未启用

**线程安全性**：必须在主线程调用

---

#### set_caption_height()

**功能说明**：
设置自定义标题栏的高度。顶部区域高度 <= height 时可拖拽窗口。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `float` | `height` | 高度(像素) |

**返回值**：无

**后置条件**：
- `caption_height()` 返回 `height`
- 顶部区域可拖拽

**线程安全性**：必须在主线程调用

---

#### caption_height()

**功能说明**：
获取标题栏高度。

**参数列表**：无

**返回值**：
- `float`：高度(像素)

**线程安全性**：必须在主线程调用

---

#### set_resize_border_thickness()

**功能说明**：
设置窗口边缘的可调整大小边框厚度。窗口边缘区域厚度 <= thickness 时可拖拽边框调整窗口大小。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `const math::Thickness&` | `thickness` | 厚度(左、上、右、下) |

**返回值**：无

**后置条件**：
- `resize_border_thickness()` 返回 `thickness`
- 窗口边缘可拖拽调整大小

**线程安全性**：必须在主线程调用

---

#### resize_border_thickness()

**功能说明**：
获取调整大小边框厚度。

**参数列表**：无

**返回值**：
- `math::Thickness`：厚度

**线程安全性**：必须在主线程调用

---

#### set_corner_preference()

**功能说明**：
设置窗口圆角首选项。仅 Windows 11+ 支持。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `platform::WindowCornerPreference` | `pref` | 圆角首选项 |

**返回值**：无

**后置条件**：
- `corner_preference()` 返回 `pref`
- Windows 11+ 上窗口显示对应圆角样式

**线程安全性**：必须在主线程调用

---

#### corner_preference()

**功能说明**：
获取窗口圆角首选项。

**参数列表**：无

**返回值**：
- `platform::WindowCornerPreference`：圆角首选项

**线程安全性**：必须在主线程调用

---

#### set_glass_frame_thickness()

**功能说明**：
设置 DWM 玻璃帧厚度。仅 Windows Vista+ 支持,用于 Aero 玻璃效果(背景模糊)。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `const math::Thickness&` | `thickness` | 厚度(左、上、右、下) |

**返回值**：无

**后置条件**：
- `glass_frame_thickness()` 返回 `thickness`
- Windows Vista+ 上显示玻璃效果

**线程安全性**：必须在主线程调用

**示例**：
```cpp
window.set_glass_frame_thickness({-1, -1, -1, -1});  // 全窗口玻璃效果
```

---

#### glass_frame_thickness()

**功能说明**：
获取 DWM 玻璃帧厚度。

**参数列表**：无

**返回值**：
- `math::Thickness`：厚度

**线程安全性**：必须在主线程调用

---

#### drag()

**功能说明**：
编程方式拖拽窗口,类似 WPF 的 `Window.DragMove()`。通常在标题栏的 MouseDown 事件中调用。

**参数列表**：无

**返回值**：无

**前置条件**：
- 必须在鼠标左键按下状态调用

**后置条件**：
- 窗口进入拖拽模式,跟随鼠标移动

**线程安全性**：必须在主线程调用

**示例**：
```cpp
// 标题栏拖拽实现
title_bar->on_mouse_down([&](const input::MouseButtonEventArgs& e) {
    if (e.button == input::MouseButton::Left) {
        window.drag();  // 开始拖拽窗口
    }
});
```

---

### 4.5 窗口状态

#### set_window_state()

**功能说明**：
设置窗口状态(Normal/Minimized/Maximized)。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `platform::WindowState` | `state` | 窗口状态 |

**返回值**：无

**后置条件**：
- `window_state()` 返回 `state`
- 窗口显示对应状态

**线程安全性**：必须在主线程调用

**示例**：
```cpp
window.set_window_state(platform::WindowState::Maximized);  // 最大化
```

---

#### window_state()

**功能说明**：
获取当前窗口状态。

**参数列表**：无

**返回值**：
- `platform::WindowState`：窗口状态

**线程安全性**：必须在主线程调用

---

### 4.6 内容根

#### set_content()

**功能说明**：
设置内容根元素。Window 不拥有 element 的生命周期,应用层需自行管理。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `ui::UIElement*` | `element` | 内容根元素(nullptr 表示清空) |

**返回值**：无

**后置条件**：
- `content()` 返回 `element`
- 触发重新布局和渲染

**线程安全性**：必须在主线程调用

**示例**：
```cpp
auto panel = core::make_owned<ui::StackPanel>();
panel->add_child(/* ... */);
window.set_content(panel.get());  // Window 不持有 panel 所有权
```

---

#### content()

**功能说明**：
获取当前内容根元素。

**参数列表**：无

**返回值**：
- `ui::UIElement*`：内容根元素(可能为 nullptr)

**线程安全性**：必须在主线程调用

---

### 4.7 平台窗口委托

#### show()

**功能说明**：
显示窗口。pending 状态下首次调用触发懒初始化(通过 IWindowContext)。

**参数列表**：无

**返回值**：无

**前置条件**：
- 必须在 `Application::run()` 之后调用
- `get_application_window_context()` 必须返回非空

**后置条件**：
- pending 状态下,完成懒初始化,`is_pending()` 返回 false
- 窗口可见

**线程安全性**：必须在主线程调用

**示例**：
```cpp
ui::Window window;
window.set_title("My Window");
window.show();  // 触发懒初始化
```

---

#### hide()

**功能说明**：
隐藏窗口。窗口不可见但未销毁,可再次 `show()`。

**参数列表**：无

**返回值**：无

**后置条件**：
- 窗口不可见

**线程安全性**：必须在主线程调用

---

#### close()

**功能说明**：
关闭窗口。触发 `platform::IWindow::on_closed()` 回调,标记 `is_closed()` 为 true。

**参数列表**：无

**返回值**：无

**后置条件**：
- `is_closed()` 返回 true
- 窗口不可再次 `show()`

**线程安全性**：必须在主线程调用

---

#### set_title()

**功能说明**：
设置窗口标题。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `core::StringView` | `title` | 标题字符串 |

**返回值**：无

**后置条件**：
- pending 状态下,缓存到内部,`show()` 时应用
- 已初始化的窗口,立即更新标题

**线程安全性**：必须在主线程调用

---

#### set_size()

**功能说明**：
设置窗口尺寸。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `math::Size` | `size` | 尺寸(宽、高) |

**返回值**：无

**后置条件**：
- pending 状态下,缓存到内部,`show()` 时应用
- 已初始化的窗口,立即更新尺寸

**线程安全性**：必须在主线程调用

---

#### set_resizable()

**功能说明**：
设置窗口是否可调整大小。仅 pending 状态下设置有效。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `bool` | `resizable` | true 可调整,false 不可调整 |

**返回值**：无

**前置条件**：
- 仅 pending 状态下设置有效

**线程安全性**：必须在主线程调用

---

#### set_auto_position()

**功能说明**：
设置窗口是否自动定位。仅 pending 状态下设置有效。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `bool` | `auto_position` | true 自动定位(屏幕中央),false 手动定位 |

**返回值**：无

**前置条件**：
- 仅 pending 状态下设置有效

**线程安全性**：必须在主线程调用

---

#### set_kind()

**功能说明**：
设置窗口类型。仅 pending 状态下设置有效。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `platform::WindowKind` | `kind` | 窗口类型(Main/Popup/Tool) |

**返回值**：无

**前置条件**：
- 仅 pending 状态下设置有效

**线程安全性**：必须在主线程调用

---

#### size()

**功能说明**：
获取窗口尺寸。

**参数列表**：无

**返回值**：
- `math::Size`：尺寸(宽、高)

**线程安全性**：必须在主线程调用

---

#### dpi()

**功能说明**：
获取窗口 DPI 缩放比例。

**参数列表**：无

**返回值**：
- `float`：DPI 缩放比例(例如 1.0 表示 96 DPI, 1.5 表示 144 DPI)

**线程安全性**：必须在主线程调用

---

#### is_closed()

**功能说明**：
查询窗口是否已关闭。

**参数列表**：无

**返回值**：
- `bool`：true 已关闭,false 未关闭

**线程安全性**：必须在主线程调用

---

#### is_pending()

**功能说明**：
查询窗口是否处于 pending 状态。

**参数列表**：无

**返回值**：
- `bool`：true 处于 pending 状态,false 已初始化

**线程安全性**：必须在主线程调用

---

### 4.8 平台窗口访问

#### native_window()

**功能说明**：
获取原生窗口引用。仅在已初始化状态下调用。

**参数列表**：无

**返回值**：
- `platform::IWindow&`：原生窗口引用

**前置条件**：
- `is_pending()` 必须返回 false

**线程安全性**：必须在主线程调用

---

### 4.9 输入路由

#### input_router()

**功能说明**：
获取输入路由器引用。仅在已初始化状态下调用。

**参数列表**：无

**返回值**：
- `input::InputRouter&`：输入路由器引用

**前置条件**：
- `is_pending()` 必须返回 false

**线程安全性**：必须在主线程调用

---

#### ime()

**功能说明**：
获取 IME 服务引用。仅在已初始化状态下调用。

**参数列表**：无

**返回值**：
- `platform::IMEService&`：IME 服务引用

**前置条件**：
- `is_pending()` 必须返回 false

**线程安全性**：必须在主线程调用

---

#### set_on_input_processed()

**功能说明**：
设置输入处理完成回调。每次 InputRouter 处理完输入事件后调用。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `std::function<void()>` | `fn` | 回调函数 |

**返回值**：无

**线程安全性**：必须在主线程调用

**示例**：
```cpp
window.set_on_input_processed([&]() {
    window.render();  // 输入处理完成后重新渲染
});
```

---

### 4.10 渲染

#### render()

**功能说明**：
渲染窗口。执行 Measure + Arrange 两遍布局,将视觉树渲染到交换链后缓冲,调用 Present 显示到屏幕。

**参数列表**：无

**返回值**：无

**前置条件**：
- `is_pending()` 必须返回 false
- `content()` 不为 nullptr

**后置条件**：
- 视觉树已渲染到屏幕

**线程安全性**：必须在主线程调用

**示例**：
```cpp
window.render();  // 立即渲染
```

---

### 4.11 静态辅助

#### from_element()

**功能说明**：
从 UIElement 获取所属 Window。向上遍历视觉树,直到找到 Window 或到达根节点。

**参数列表**：

| 参数类型 | 参数名 | 描述 |
|---------|--------|------|
| `ui::UIElement*` | `element` | UI 元素 |

**返回值**：
- `Window*`：所属 Window(未找到返回 nullptr)

**线程安全性**：必须在主线程调用

**示例**：
```cpp
auto window = ui::Window::from_element(my_button);
if (window) {
    window->set_title("Button Clicked!");
}
```

---

## 5. 使用场景

### 场景1：MVVM 窗口 + DataContext 绑定

**业务需求**：创建一个 MVVM 模式的窗口,绑定 ViewModel,实现数据驱动 UI。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/binding/Binding.h>

// ViewModel
class MainViewModel {
public:
    std::string title = "Hello, MineFramework!";
    int         count = 0;
    
    void increment() {
        ++count;
        // 通知 UI 更新(简化示例,实际应使用 INotifyPropertyChanged)
    }
};

void create_mvvm_window() {
    // 1. 创建 ViewModel
    auto vm = std::make_shared<MainViewModel>();
    
    // 2. 创建 Window 并设置 DataContext
    ui::Window window;
    window.set_title("MVVM 示例");
    window.set_data_context(vm.get());  // 绑定 ViewModel
    
    // 3. 创建 UI
    auto panel = core::make_owned<ui::StackPanel>();
    panel->set_spacing(10);
    panel->set_padding({20, 20, 20, 20});
    
    // 标题(绑定到 vm.title)
    auto title_text = core::make_owned<ui::TextBlock>();
    title_text->set_binding(ui::TextBlock::TextProperty, "title");
    panel->add_child(title_text.get());
    
    // 计数器(绑定到 vm.count)
    auto count_text = core::make_owned<ui::TextBlock>();
    count_text->set_binding(ui::TextBlock::TextProperty, 
                            "count",
                            binding::BindingMode::OneWay,
                            [](const core::Variant& v) {
                                return core::Variant(
                                    std::format("Count: {}", v.as<int>())
                                );
                            });
    panel->add_child(count_text.get());
    
    // 按钮
    auto button = core::make_owned<ui::Button>();
    button->set_content("Increment");
    button->on_click([vm]() {
        vm->increment();
    });
    panel->add_child(button.get());
    
    // 4. 设置内容并显示
    window.set_content(panel.get());
    window.show();
}
```

**预期效果**：
- 窗口标题显示 "Hello, MineFramework!"
- 计数器显示 "Count: 0"
- 点击按钮后计数器自动更新

---

### 场景2：自定义标题栏窗口

**业务需求**：创建一个自定义标题栏的窗口,隐藏系统标题栏,由应用层绘制。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/StackPanel.h>

void create_custom_chrome_window() {
    // 1. 创建 Window 并启用自定义 Chrome
    ui::Window window;
    window.set_title("自定义标题栏示例");
    window.set_size({800, 600});
    window.set_custom_chrome(true);        // 启用自定义标题栏
    window.set_caption_height(40);         // 标题栏高度 40 像素
    window.set_resize_border_thickness({8, 8, 8, 8});  // 边框厚度
    window.set_corner_preference(platform::WindowCornerPreference::Round);  // 圆角
    
    // 2. 创建根布局(Grid: 标题栏行 + 内容行)
    auto grid = core::make_owned<ui::Grid>();
    grid->add_row_definition({ui::GridLength::absolute(40)});  // 标题栏
    grid->add_row_definition({ui::GridLength::star(1)});       // 内容
    
    // 3. 创建标题栏
    auto title_bar = core::make_owned<ui::Border>();
    title_bar->set_background(ui::Brush::solid_color({0.2f, 0.2f, 0.2f, 1.0f}));
    ui::Grid::set_row(title_bar.get(), 0);
    
    // 标题栏布局(Grid: 标题文本 + 窗口控制按钮)
    auto title_bar_grid = core::make_owned<ui::Grid>();
    title_bar_grid->add_column_definition({ui::GridLength::star(1)});   // 标题
    title_bar_grid->add_column_definition({ui::GridLength::auto_size});  // 最小化
    title_bar_grid->add_column_definition({ui::GridLength::auto_size});  // 最大化
    title_bar_grid->add_column_definition({ui::GridLength::auto_size});  // 关闭
    
    // 标题文本
    auto title_text = core::make_owned<ui::TextBlock>();
    title_text->set_text("自定义标题栏示例");
    title_text->set_foreground(ui::Brush::solid_color({1.0f, 1.0f, 1.0f, 1.0f}));
    title_text->set_vertical_alignment(ui::VerticalAlignment::Center);
    title_text->set_padding({10, 0, 0, 0});
    ui::Grid::set_column(title_text.get(), 0);
    title_bar_grid->add_child(title_text.get());
    
    // 标题栏拖拽(鼠标按下时拖拽窗口)
    title_bar->on_mouse_down([&window](const input::MouseButtonEventArgs& e) {
        if (e.button == input::MouseButton::Left) {
            window.drag();  // 开始拖拽
        }
    });
    
    // 最小化按钮
    auto minimize_btn = core::make_owned<ui::Button>();
    minimize_btn->set_content("_");
    minimize_btn->set_width(46);
    minimize_btn->set_height(40);
    minimize_btn->on_click([&window]() {
        window.set_window_state(platform::WindowState::Minimized);
    });
    ui::Grid::set_column(minimize_btn.get(), 1);
    title_bar_grid->add_child(minimize_btn.get());
    
    // 最大化/还原按钮
    auto maximize_btn = core::make_owned<ui::Button>();
    maximize_btn->set_content("□");
    maximize_btn->set_width(46);
    maximize_btn->set_height(40);
    maximize_btn->on_click([&window, &maximize_btn]() {
        if (window.window_state() == platform::WindowState::Maximized) {
            window.set_window_state(platform::WindowState::Normal);
            maximize_btn->set_content("□");
        } else {
            window.set_window_state(platform::WindowState::Maximized);
            maximize_btn->set_content("❐");
        }
    });
    ui::Grid::set_column(maximize_btn.get(), 2);
    title_bar_grid->add_child(maximize_btn.get());
    
    // 关闭按钮
    auto close_btn = core::make_owned<ui::Button>();
    close_btn->set_content("✕");
    close_btn->set_width(46);
    close_btn->set_height(40);
    close_btn->set_background(ui::Brush::solid_color({0.8f, 0.0f, 0.0f, 1.0f}));
    close_btn->on_click([&window]() {
        window.close();
    });
    ui::Grid::set_column(close_btn.get(), 3);
    title_bar_grid->add_child(close_btn.get());
    
    title_bar->set_child(title_bar_grid.get());
    grid->add_child(title_bar.get());
    
    // 4. 创建内容区域
    auto content = core::make_owned<ui::TextBlock>();
    content->set_text("这是自定义标题栏窗口的内容区域。\n"
                      "可以拖拽标题栏移动窗口,点击按钮控制窗口状态。");
    content->set_padding({20, 20, 20, 20});
    ui::Grid::set_row(content.get(), 1);
    grid->add_child(content.get());
    
    // 5. 设置内容并显示
    window.set_content(grid.get());
    window.show();
}
```

**预期效果**：
- 窗口无系统标题栏,应用层自行绘制
- 可以拖拽标题栏移动窗口
- 可以点击最小化/最大化/关闭按钮控制窗口状态

---

### 场景3：DWM 玻璃帧效果(Windows)

**业务需求**：在 Windows 上启用 Aero 玻璃效果,背景模糊。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/TextBlock.h>

void create_glass_window() {
    // 1. 创建 Window
    ui::Window window;
    window.set_title("玻璃帧效果示例");
    window.set_size({600, 400});
    
    // 2. 启用全窗口玻璃效果
    window.set_glass_frame_thickness({-1, -1, -1, -1});  // -1 表示全窗口
    
    // 3. 创建半透明内容
    auto content = core::make_owned<ui::TextBlock>();
    content->set_text("Windows Aero 玻璃效果\n"
                      "背景模糊,透明度混合");
    content->set_foreground(ui::Brush::solid_color({1.0f, 1.0f, 1.0f, 0.9f}));
    content->set_padding({20, 20, 20, 20});
    
    // 4. 设置内容并显示
    window.set_content(content.get());
    window.show();
}
```

**预期效果**：
- Windows Vista+ 上显示玻璃效果(背景模糊)
- 窗口边缘半透明

---

### 场景4：窗口拖拽辅助函数

**业务需求**：在任意 UI 元素上实现拖拽窗口功能。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/Border.h>

void create_draggable_window() {
    // 1. 创建 Window
    ui::Window window;
    window.set_title("拖拽示例");
    
    // 2. 创建一个大的拖拽区域
    auto drag_area = core::make_owned<ui::Border>();
    drag_area->set_width(400);
    drag_area->set_height(300);
    drag_area->set_background(ui::Brush::solid_color({0.3f, 0.3f, 0.8f, 1.0f}));
    
    // 3. 监听鼠标按下事件,触发拖拽
    drag_area->on_mouse_down([&window](const input::MouseButtonEventArgs& e) {
        if (e.button == input::MouseButton::Left) {
            window.drag();  // 开始拖拽窗口
        }
    });
    
    // 4. 设置内容并显示
    window.set_content(drag_area.get());
    window.show();
}
```

**预期效果**：
- 在蓝色区域点击并拖动,窗口跟随鼠标移动

---

### 场景5：多窗口管理

**业务需求**：动态创建多个窗口,每个窗口独立管理。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/layout/StackPanel.h>
#include <vector>

class MultiWindowManager {
private:
    std::vector<core::OwnedPtr<ui::Window>> m_windows;

public:
    void create_main_window() {
        auto window = core::make_owned<ui::Window>();
        window->set_title("主窗口");
        window->set_size({400, 300});
        
        // 创建 UI
        auto panel = core::make_owned<ui::StackPanel>();
        panel->set_spacing(10);
        panel->set_padding({20, 20, 20, 20});
        
        // 新建窗口按钮
        auto new_window_btn = core::make_owned<ui::Button>();
        new_window_btn->set_content("新建窗口");
        new_window_btn->on_click([this]() {
            create_child_window();
        });
        panel->add_child(new_window_btn.get());
        
        // 窗口计数
        auto count_text = core::make_owned<ui::TextBlock>();
        count_text->set_text(std::format("当前窗口数: {}", m_windows.size()));
        panel->add_child(count_text.get());
        
        window->set_content(panel.get());
        window->show();
        
        m_windows.push_back(std::move(window));
    }
    
    void create_child_window() {
        auto window = core::make_owned<ui::Window>();
        window->set_title(std::format("窗口 {}", m_windows.size() + 1));
        window->set_size({300, 200});
        
        auto text = core::make_owned<ui::TextBlock>();
        text->set_text("这是一个子窗口");
        text->set_padding({20, 20, 20, 20});
        
        window->set_content(text.get());
        window->show();
        
        m_windows.push_back(std::move(window));
    }
};
```

**预期效果**：
- 主窗口显示窗口计数和"新建窗口"按钮
- 点击按钮后创建新的独立窗口
- 所有窗口独立管理,不互相影响

---

### 场景6：Padding 内边距应用

**业务需求**：设置窗口内边距,使内容根不紧贴窗口边缘。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/TextBlock.h>

void create_padded_window() {
    // 1. 创建 Window
    ui::Window window;
    window.set_title("内边距示例");
    window.set_size({400, 300});
    
    // 2. 设置内边距(左右各 30 像素,上下各 20 像素)
    window.set_padding({30, 20, 30, 20});
    
    // 3. 创建填充整个可用空间的内容
    auto content = core::make_owned<ui::Border>();
    content->set_background(ui::Brush::solid_color({0.8f, 0.8f, 0.8f, 1.0f}));
    
    auto text = core::make_owned<ui::TextBlock>();
    text->set_text("注意内容根与窗口边缘之间的间距。\n"
                   "这是通过 Window::set_padding() 实现的。");
    content->set_child(text.get());
    
    // 4. 设置内容并显示
    window.set_content(content.get());
    window.show();
}
```

**预期效果**：
- 内容根与窗口边缘之间有明显的间距
- 间距大小为左右 30 像素,上下 20 像素

---

### 场景7：窗口状态绑定

**业务需求**：将窗口状态绑定到 ViewModel,实现双向同步。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/ComboBox.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/binding/Binding.h>

class WindowStateViewModel {
public:
    platform::WindowState state = platform::WindowState::Normal;
};

void create_window_state_binding() {
    // 1. 创建 ViewModel
    auto vm = std::make_shared<WindowStateViewModel>();
    
    // 2. 创建 Window
    ui::Window window;
    window.set_title("窗口状态绑定示例");
    window.set_data_context(vm.get());
    
    // 3. 绑定窗口状态到 ViewModel
    window.set_binding(ui::Window::WindowStateProperty,
                       "state",
                       binding::BindingMode::TwoWay);
    
    // 4. 创建 UI
    auto panel = core::make_owned<ui::StackPanel>();
    panel->set_spacing(10);
    panel->set_padding({20, 20, 20, 20});
    
    // 状态下拉框
    auto combo = core::make_owned<ui::ComboBox>();
    combo->add_item("Normal");
    combo->add_item("Minimized");
    combo->add_item("Maximized");
    combo->set_binding(ui::ComboBox::SelectedIndexProperty,
                       "state",
                       binding::BindingMode::TwoWay,
                       [](const core::Variant& v) {
                           return static_cast<int>(v.as<platform::WindowState>());
                       });
    panel->add_child(combo.get());
    
    // 说明文本
    auto text = core::make_owned<ui::TextBlock>();
    text->set_text("窗口状态与下拉框双向绑定。\n"
                   "改变下拉框或窗口状态,另一方自动同步。");
    panel->add_child(text.get());
    
    window.set_content(panel.get());
    window.show();
}
```

**预期效果**：
- 下拉框显示当前窗口状态
- 改变下拉框,窗口状态自动更新
- 手动最大化/最小化窗口,下拉框自动同步

---

### 场景8：pending 模式值类型成员

**业务需求**：将 Window 作为应用类的值类型成员直接声明,无需指针。

**代码示例**：
```cpp
#include <mine/ui/window/Window.h>
#include <mine/ui/controls/TextBlock.h>

class MyApplication {
private:
    ui::Window m_main_window;  // ✅ 值类型成员,无需指针

public:
    MyApplication() {
        // ✅ 在构造函数中配置 pending 窗口
        m_main_window.set_title("My Application");
        m_main_window.set_size({800, 600});
        m_main_window.set_padding({20, 20, 20, 20});
        
        // 创建内容
        auto content = core::make_owned<ui::TextBlock>();
        content->set_text("Window 作为值类型成员直接声明");
        m_main_window.set_content(content.get());
    }
    
    void run() {
        // ✅ 在 Application 启动后显示窗口
        m_main_window.show();  // 触发懒初始化
        
        platform::run_message_loop();
    }
};

int main() {
    MyApplication app;
    app.run();
    return 0;
}
```

**预期效果**：
- Window 作为值类型成员,无需手动管理生命周期
- 构造时配置 pending 窗口,运行时触发懒初始化

---

## 6. 最佳实践

### ✅ 实践1：优先使用 pending 路径构造窗口

**说明**：
推荐使用无参构造 + pending 状态 + show() 懒初始化的方式创建窗口。这样可以先配置窗口属性,再一次性初始化,代码更清晰。

**✅ 推荐写法**：
```cpp
ui::Window window;  // ✅ 无参构造,pending 状态
window.set_title("My Window");
window.set_size({800, 600});
window.set_custom_chrome(true);
window.set_padding({20, 20, 20, 20});
window.set_content(my_content);
window.show();  // ✅ 一次性初始化
```

**❌ 不推荐写法**：
```cpp
// ❌ 手动创建原生窗口,再构造 Window
platform::WindowDesc desc{.title = "My Window", .width = 800, .height = 600};
auto native_window = platform::create_window(desc);
ui::Window window(*native_window, device, queue, renderer);
window.set_content(my_content);
```

**原因**：
- pending 模式代码更简洁,无需手动创建原生窗口
- 可以将 Window 作为值类型成员直接声明
- 配置和初始化分离,更易维护

---

### ✅ 实践2：自定义 Chrome 配合 drag() 实现拖拽

**说明**：
启用自定义标题栏后,需要在标题栏 UI 元素上监听 MouseDown 事件,调用 `window.drag()` 实现窗口拖拽。

**✅ 推荐写法**：
```cpp
ui::Window window;
window.set_custom_chrome(true);
window.set_caption_height(40);

// ✅ 创建标题栏 UI
auto title_bar = core::make_owned<ui::Border>();
title_bar->set_height(40);
title_bar->set_background(ui::Brush::solid_color({0.2f, 0.2f, 0.2f, 1.0f}));

// ✅ 监听鼠标按下事件,调用 drag()
title_bar->on_mouse_down([&window](const input::MouseButtonEventArgs& e) {
    if (e.button == input::MouseButton::Left) {
        window.drag();  // ✅ 编程方式拖拽窗口
    }
});
```

**❌ 不推荐写法**：
```cpp
ui::Window window;
window.set_custom_chrome(true);
window.set_caption_height(40);

// ❌ 未实现拖拽逻辑,标题栏无法拖动窗口
auto title_bar = core::make_owned<ui::Border>();
title_bar->set_height(40);
title_bar->set_background(ui::Brush::solid_color({0.2f, 0.2f, 0.2f, 1.0f}));
// ❌ 忘记监听鼠标事件
```

**原因**：
- 自定义标题栏后系统不再处理拖拽,需要应用层实现
- `window.drag()` 提供了类似 WPF 的便捷 API

---

### ✅ 实践3：合理设置 Padding 避免内容紧贴边缘

**说明**：
窗口内容根默认无内边距,可能紧贴窗口边缘。建议根据设计规范设置合理的 Padding。

**✅ 推荐写法**：
```cpp
ui::Window window;
window.set_padding({20, 20, 20, 20});  // ✅ 设置 20 像素内边距

auto content = core::make_owned<ui::StackPanel>();
// 内容根自动留出内边距
window.set_content(content.get());
```

**❌ 不推荐写法**：
```cpp
ui::Window window;
// ❌ 未设置 Padding,内容紧贴边缘

auto content = core::make_owned<ui::StackPanel>();
content->set_padding({20, 20, 20, 20});  // ❌ 在内容根上设置 Padding(不推荐)
window.set_content(content.get());
```

**原因**：
- Window.Padding 是统一的内边距设置点,避免在多个内容根上重复设置
- 符合 WPF/Avalonia 的设计惯例

---

### ✅ 实践4：避免频繁调用 render()

**说明**：
`render()` 会执行完整的布局和渲染流程,开销较大。应通过事件驱动(如 `set_on_input_processed()`)或定时器按需渲染,避免频繁调用。

**✅ 推荐写法**：
```cpp
ui::Window window;

// ✅ 输入处理完成后自动渲染
window.set_on_input_processed([&]() {
    window.render();
});

// ✅ 或使用定时器按固定帧率渲染(例如 60 FPS)
async::run_timer(std::chrono::milliseconds(16), [&]() {
    window.render();
});
```

**❌ 不推荐写法**：
```cpp
ui::Window window;

// ❌ 死循环频繁渲染(CPU 占用 100%)
while (!window.is_closed()) {
    window.render();  // ❌ 频繁调用,浪费资源
}
```

**原因**：
- 频繁渲染浪费 CPU 和 GPU 资源
- 事件驱动或固定帧率渲染更高效

---

## 7. 常见陷阱

### ❌ 陷阱1：在 Application::run() 之前调用 Window::show()

**问题描述**：
如果在 Application 注册 IWindowContext 之前调用 `Window::show()`,会导致懒初始化失败。

**❌ 错误代码**：
```cpp
int main() {
    MyApplication app;
    
    ui::Window window;
    window.set_title("Main Window");
    window.show();  // ❌ Application 尚未启动,上下文未注册
    
    app.run();  // ❌ 窗口已显示但未正确初始化
    
    return 0;
}
```

**错误现象**：
- `Window::show()` 内部获取上下文失败,打印错误日志
- 窗口不显示或显示异常

**✅ 正确代码**：
```cpp
int main() {
    MyApplication app;
    
    ui::Window window;
    window.set_title("Main Window");
    
    // ✅ 注册启动回调,在 Application 启动后显示窗口
    app.on_started([&]() {
        window.show();  // ✅ 此时上下文已注册
    });
    
    app.run();
    
    return 0;
}
```

**原理解释**：
`Window::show()` 需要通过 `get_application_window_context()` 获取图形设备等资源,必须在 Application 注册上下文之后调用。

---

### ❌ 陷阱2：show() 后访问 native_window() 但未检查 pending 状态

**问题描述**：
如果 `show()` 失败(例如上下文未注册),Window 仍处于 pending 状态,访问 `native_window()` 会断言失败。

**❌ 错误代码**：
```cpp
ui::Window window;
window.set_title("Test");
window.show();  // ❌ 可能失败(上下文未注册)

// ❌ 未检查 pending 状态,直接访问 native_window()
auto& native = window.native_window();  // ❌ 断言失败
```

**错误现象**：
- 程序崩溃(断言失败)

**✅ 正确代码**：
```cpp
ui::Window window;
window.set_title("Test");
window.show();

// ✅ 检查 pending 状态
if (!window.is_pending()) {
    auto& native = window.native_window();  // ✅ 安全访问
} else {
    core::log_error("Window initialization failed");
}
```

**原理解释**：
`native_window()` 仅在已初始化状态下有效,pending 状态下会触发断言。必须检查 `is_pending()` 或确保 `show()` 成功。

---

### ❌ 陷阱3：频繁调用 render() 导致性能问题

**问题描述**：
在死循环中频繁调用 `render()` 会导致 CPU 和 GPU 占用过高,浪费资源。

**❌ 错误代码**：
```cpp
ui::Window window;
window.set_content(my_content);
window.show();

// ❌ 死循环频繁渲染
while (!window.is_closed()) {
    window.render();  // ❌ 每帧都渲染,CPU 占用 100%
}
```

**错误现象**：
- CPU 占用 100%
- 风扇全速运转,发热严重
- 笔记本电池快速耗尽

**✅ 正确代码**：
```cpp
ui::Window window;
window.set_content(my_content);
window.show();

// ✅ 输入处理完成后自动渲染
window.set_on_input_processed([&]() {
    window.render();
});

// ✅ 或使用定时器按固定帧率渲染
async::run_timer(std::chrono::milliseconds(16), [&]() {
    window.render();  // ✅ 60 FPS
});

// ✅ 运行消息循环
platform::run_message_loop();
```

**原理解释**：
应使用事件驱动或定时器按需渲染,避免死循环。

---

### ❌ 陷阱4：忘记处理 Closed 事件导致资源泄漏

**问题描述**：
窗口关闭后,如果应用层持有 Window 的智能指针但未释放,会导致资源泄漏。

**❌ 错误代码**：
```cpp
class MyApplication {
private:
    std::vector<core::OwnedPtr<ui::Window>> m_windows;

public:
    void create_window() {
        auto window = core::make_owned<ui::Window>();
        window->set_title("Test");
        window->show();
        
        m_windows.push_back(std::move(window));
        
        // ❌ 未监听 Closed 事件,窗口关闭后仍在列表中
    }
};
```

**错误现象**：
- 窗口关闭后 Window 对象未释放
- 内存泄漏

**✅ 正确代码**：
```cpp
class MyApplication {
private:
    std::vector<core::OwnedPtr<ui::Window>> m_windows;

public:
    void create_window() {
        auto window = core::make_owned<ui::Window>();
        window->set_title("Test");
        
        // ✅ 监听 Closed 事件,从列表移除
        window->native_window().on_closed([this, ptr = window.get()]() {
            auto it = std::find_if(m_windows.begin(), m_windows.end(),
                [ptr](const auto& w) { return w.get() == ptr; });
            if (it != m_windows.end()) {
                m_windows.erase(it);  // ✅ 释放 Window
            }
        });
        
        window->show();
        m_windows.push_back(std::move(window));
    }
};
```

**原理解释**：
窗口关闭后应从管理列表移除,释放资源。

---

### ❌ 陷阱5：pending 状态下访问 input_router()

**问题描述**：
`input_router()` 仅在已初始化状态下有效,pending 状态下会断言失败。

**❌ 错误代码**：
```cpp
ui::Window window;
window.set_title("Test");

// ❌ pending 状态下访问 input_router()
auto& router = window.input_router();  // ❌ 断言失败
```

**错误现象**：
- 程序崩溃(断言失败)

**✅ 正确代码**：
```cpp
ui::Window window;
window.set_title("Test");
window.show();  // ✅ 先初始化

// ✅ 初始化后访问 input_router()
if (!window.is_pending()) {
    auto& router = window.input_router();  // ✅ 安全访问
}
```

**原理解释**：
`input_router()` 在懒初始化时创建,pending 状态下尚未创建。

---

## 8. 完整示例

以下是一个完整的示例程序,展示自定义标题栏窗口 + MVVM 绑定 + 拖拽实现 + DataContext 传播的完整流程。

```cpp
// ========== MainViewModel.h ==========

#pragma once

#include <mine/mvvm/ViewModelBase.h>
#include <string>

namespace myapp {

/// @brief 主窗口 ViewModel
class MainViewModel : public mine::mvvm::ViewModelBase {
private:
    std::string m_title = "MineFramework 自定义标题栏示例";
    int         m_counter = 0;

public:
    // 标题属性
    void set_title(const std::string& title) {
        if (m_title != title) {
            m_title = title;
            notify_property_changed("Title");
        }
    }
    
    [[nodiscard]] const std::string& title() const noexcept {
        return m_title;
    }
    
    // 计数器属性
    void set_counter(int counter) {
        if (m_counter != counter) {
            m_counter = counter;
            notify_property_changed("Counter");
        }
    }
    
    [[nodiscard]] int counter() const noexcept {
        return m_counter;
    }
    
    // 递增命令
    void increment() {
        set_counter(m_counter + 1);
    }
    
    // 重置命令
    void reset() {
        set_counter(0);
    }
};

} // namespace myapp

// ========== main.cpp ==========

#include "MainViewModel.h"
#include <mine/ui/window/Window.h>
#include <mine/ui/window/WindowContext.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/layout/Grid.h>
#include <mine/ui/layout/StackPanel.h>
#include <mine/ui/binding/Binding.h>
#include <mine/ui/app/Application.h>

using namespace mine;

int main() {
    // 1. 创建 ViewModel
    auto view_model = std::make_shared<myapp::MainViewModel>();
    
    // 2. 创建 Window 并启用自定义标题栏
    ui::Window window;
    window.set_title("MineFramework 示例");
    window.set_size({800, 600});
    window.set_custom_chrome(true);        // 启用自定义标题栏
    window.set_caption_height(40);         // 标题栏高度
    window.set_resize_border_thickness({8, 8, 8, 8});
    window.set_corner_preference(platform::WindowCornerPreference::Round);
    window.set_padding({0, 0, 0, 0});      // 无内边距(标题栏占满)
    
    // 3. 设置 DataContext(向子元素传播)
    window.set_data_context(view_model.get());
    
    // 4. 创建根布局(Grid: 标题栏 + 内容)
    auto root_grid = core::make_owned<ui::Grid>();
    root_grid->add_row_definition({ui::GridLength::absolute(40)});  // 标题栏
    root_grid->add_row_definition({ui::GridLength::star(1)});       // 内容
    
    // ===== 标题栏 =====
    auto title_bar = core::make_owned<ui::Border>();
    title_bar->set_background(ui::Brush::solid_color({0.15f, 0.15f, 0.15f, 1.0f}));
    ui::Grid::set_row(title_bar.get(), 0);
    
    // 标题栏布局(Grid: 图标 + 标题 + 窗口控制按钮)
    auto title_bar_grid = core::make_owned<ui::Grid>();
    title_bar_grid->add_column_definition({ui::GridLength::auto_size});      // 图标
    title_bar_grid->add_column_definition({ui::GridLength::star(1)});        // 标题
    title_bar_grid->add_column_definition({ui::GridLength::auto_size});      // 最小化
    title_bar_grid->add_column_definition({ui::GridLength::auto_size});      // 最大化
    title_bar_grid->add_column_definition({ui::GridLength::auto_size});      // 关闭
    
    // 应用图标
    auto icon = core::make_owned<ui::TextBlock>();
    icon->set_text("🚀");
    icon->set_font_size(20);
    icon->set_vertical_alignment(ui::VerticalAlignment::Center);
    icon->set_padding({10, 0, 10, 0});
    ui::Grid::set_column(icon.get(), 0);
    title_bar_grid->add_child(icon.get());
    
    // 标题文本(绑定到 ViewModel.Title)
    auto title_text = core::make_owned<ui::TextBlock>();
    title_text->set_binding(ui::TextBlock::TextProperty,
                            "Title",
                            binding::BindingMode::OneWay);
    title_text->set_foreground(ui::Brush::solid_color({1.0f, 1.0f, 1.0f, 1.0f}));
    title_text->set_font_size(14);
    title_text->set_vertical_alignment(ui::VerticalAlignment::Center);
    ui::Grid::set_column(title_text.get(), 1);
    title_bar_grid->add_child(title_text.get());
    
    // 标题栏拖拽(鼠标按下时拖拽窗口)
    title_bar->on_mouse_down([&window](const input::MouseButtonEventArgs& e) {
        if (e.button == input::MouseButton::Left) {
            window.drag();  // 开始拖拽
        }
    });
    
    // 双击最大化/还原
    title_bar->on_mouse_double_click([&window](const input::MouseButtonEventArgs& e) {
        if (e.button == input::MouseButton::Left) {
            if (window.window_state() == platform::WindowState::Maximized) {
                window.set_window_state(platform::WindowState::Normal);
            } else {
                window.set_window_state(platform::WindowState::Maximized);
            }
        }
    });
    
    // 最小化按钮
    auto minimize_btn = core::make_owned<ui::Button>();
    minimize_btn->set_content("─");
    minimize_btn->set_width(46);
    minimize_btn->set_height(40);
    minimize_btn->set_background(ui::Brush::transparent());
    minimize_btn->set_foreground(ui::Brush::solid_color({1.0f, 1.0f, 1.0f, 1.0f}));
    minimize_btn->on_click([&window]() {
        window.set_window_state(platform::WindowState::Minimized);
    });
    ui::Grid::set_column(minimize_btn.get(), 2);
    title_bar_grid->add_child(minimize_btn.get());
    
    // 最大化/还原按钮
    auto maximize_btn = core::make_owned<ui::Button>();
    maximize_btn->set_content("□");
    maximize_btn->set_width(46);
    maximize_btn->set_height(40);
    maximize_btn->set_background(ui::Brush::transparent());
    maximize_btn->set_foreground(ui::Brush::solid_color({1.0f, 1.0f, 1.0f, 1.0f}));
    maximize_btn->on_click([&window, &maximize_btn]() {
        if (window.window_state() == platform::WindowState::Maximized) {
            window.set_window_state(platform::WindowState::Normal);
            maximize_btn->set_content("□");
        } else {
            window.set_window_state(platform::WindowState::Maximized);
            maximize_btn->set_content("❐");
        }
    });
    ui::Grid::set_column(maximize_btn.get(), 3);
    title_bar_grid->add_child(maximize_btn.get());
    
    // 关闭按钮
    auto close_btn = core::make_owned<ui::Button>();
    close_btn->set_content("✕");
    close_btn->set_width(46);
    close_btn->set_height(40);
    close_btn->set_background(ui::Brush::transparent());
    close_btn->set_foreground(ui::Brush::solid_color({1.0f, 1.0f, 1.0f, 1.0f}));
    close_btn->on_mouse_enter([&close_btn](const input::MouseEventArgs&) {
        close_btn->set_background(ui::Brush::solid_color({0.8f, 0.0f, 0.0f, 1.0f}));
    });
    close_btn->on_mouse_leave([&close_btn](const input::MouseEventArgs&) {
        close_btn->set_background(ui::Brush::transparent());
    });
    close_btn->on_click([&window]() {
        window.close();
    });
    ui::Grid::set_column(close_btn.get(), 4);
    title_bar_grid->add_child(close_btn.get());
    
    title_bar->set_child(title_bar_grid.get());
    root_grid->add_child(title_bar.get());
    
    // ===== 内容区域 =====
    auto content_panel = core::make_owned<ui::StackPanel>();
    content_panel->set_orientation(ui::Orientation::Vertical);
    content_panel->set_spacing(15);
    content_panel->set_padding({30, 30, 30, 30});
    content_panel->set_background(ui::Brush::solid_color({0.95f, 0.95f, 0.95f, 1.0f}));
    ui::Grid::set_row(content_panel.get(), 1);
    
    // 欢迎标题
    auto welcome_title = core::make_owned<ui::TextBlock>();
    welcome_title->set_text("欢迎使用 MineFramework!");
    welcome_title->set_font_size(28);
    welcome_title->set_font_weight(ui::FontWeight::Bold);
    content_panel->add_child(welcome_title.get());
    
    // 说明文本
    auto description = core::make_owned<ui::TextBlock>();
    description->set_text(
        "这是一个完整的自定义标题栏窗口示例,展示了:\n"
        "• 自定义标题栏(隐藏系统标题栏)\n"
        "• MVVM 数据绑定(DataContext 传播)\n"
        "• 窗口拖拽(drag() 方法)\n"
        "• 窗口控制按钮(最小化/最大化/关闭)\n"
        "• 双向绑定(计数器)"
    );
    description->set_font_size(14);
    description->set_foreground(ui::Brush::solid_color({0.3f, 0.3f, 0.3f, 1.0f}));
    content_panel->add_child(description.get());
    
    // 分隔线
    auto separator = core::make_owned<ui::Border>();
    separator->set_height(1);
    separator->set_background(ui::Brush::solid_color({0.7f, 0.7f, 0.7f, 1.0f}));
    content_panel->add_child(separator.get());
    
    // 计数器标题
    auto counter_title = core::make_owned<ui::TextBlock>();
    counter_title->set_text("计数器示例:");
    counter_title->set_font_size(18);
    counter_title->set_font_weight(ui::FontWeight::Bold);
    content_panel->add_child(counter_title.get());
    
    // 计数器显示(绑定到 ViewModel.Counter)
    auto counter_text = core::make_owned<ui::TextBlock>();
    counter_text->set_binding(ui::TextBlock::TextProperty,
                              "Counter",
                              binding::BindingMode::OneWay,
                              [](const core::Variant& v) {
                                  return core::Variant(
                                      std::format("当前计数: {}", v.as<int>())
                                  );
                              });
    counter_text->set_font_size(24);
    counter_text->set_foreground(ui::Brush::solid_color({0.0f, 0.5f, 0.8f, 1.0f}));
    content_panel->add_child(counter_text.get());
    
    // 按钮面板
    auto button_panel = core::make_owned<ui::StackPanel>();
    button_panel->set_orientation(ui::Orientation::Horizontal);
    button_panel->set_spacing(10);
    
    // 递增按钮
    auto increment_btn = core::make_owned<ui::Button>();
    increment_btn->set_content("递增 (+1)");
    increment_btn->set_width(120);
    increment_btn->set_height(40);
    increment_btn->on_click([view_model]() {
        view_model->increment();
    });
    button_panel->add_child(increment_btn.get());
    
    // 重置按钮
    auto reset_btn = core::make_owned<ui::Button>();
    reset_btn->set_content("重置 (0)");
    reset_btn->set_width(120);
    reset_btn->set_height(40);
    reset_btn->on_click([view_model]() {
        view_model->reset();
    });
    button_panel->add_child(reset_btn.get());
    
    content_panel->add_child(button_panel.get());
    root_grid->add_child(content_panel.get());
    
    // 5. 设置内容并显示
    window.set_content(root_grid.get());
    
    // 6. 创建 Application 并运行
    ui::Application app;
    app.on_started([&]() {
        window.show();  // 触发懒初始化
    });
    
    app.run();
    
    return 0;
}
```

### 预期运行效果

**窗口显示**：
- 顶部自定义标题栏(深灰色背景)
  - 左侧：应用图标(火箭 emoji)
  - 中间：标题文本(绑定到 ViewModel.Title)
  - 右侧：最小化/最大化/关闭按钮
- 内容区域(浅灰色背景)
  - 欢迎标题
  - 说明文本
  - 分隔线
  - 计数器标题
  - 计数器显示(绑定到 ViewModel.Counter)
  - 递增/重置按钮

**交互行为**：
- 拖拽标题栏移动窗口
- 双击标题栏最大化/还原窗口
- 点击最小化按钮最小化窗口
- 点击最大化按钮最大化/还原窗口
- 点击关闭按钮关闭窗口
- 点击递增按钮,计数器 +1
- 点击重置按钮,计数器归零

**控制台输出**：
```
[INFO] Initializing graphics device...
[INFO] Graphics device initialized successfully
[INFO] Starting application...
[INFO] Application started, showing main window...
[INFO] Creating native window: MineFramework 示例
[INFO] Native window created successfully: MineFramework 示例
[INFO] Window shown: MineFramework 示例, total windows: 1
[INFO] Entering message loop...
[INFO] Window closed: MineFramework 示例, remaining windows: 0
[INFO] All windows closed, quitting application...
[INFO] Message loop exited
[INFO] Shutting down application...
[INFO] Application shut down successfully
```

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

- **Window 桥接视觉树与平台窗口**：负责布局(Measure/Arrange)、渲染(Present)、输入路由等
- **两种构造方式**：推荐使用无参构造 + pending 状态 + show() 懒初始化
- **8 个依赖属性**：DataContext、Padding、IsCustomChrome、CaptionHeight、ResizeBorderThickness、CornerPreference、GlassFrameThickness、WindowState
- **DataContext 传播**：DataContextProperty 支持 `inherits=true`,自动向子元素传播
- **自定义 Chrome**：配合 drag() 实现窗口拖拽,ResizeBorderThickness 控制边框
- **事件驱动渲染**：通过 `set_on_input_processed()` 或定时器按需渲染,避免频繁调用 render()
- **所有权清晰**：Window 持有 swapchain/input_router,content 由应用层管理

### 与其他模块的协作关系

```
mine.ui.property::DependencyObject
  ↓ inherits
mine.ui.window::Window
  ↓ uses
├─ IWindowContext (获取图形资源)
├─ platform::IWindow (平台原生窗口)
├─ gfx::ISwapchain (交换链,Window 持有所有权)
├─ input::InputRouter (输入路由,Window 持有所有权)
├─ ui::UIElement (内容根,应用层管理)
└─ paint::IRenderer (渲染器,通过 IWindowContext 获取)
```

### 适用场景总结

- **MVVM 应用**：通过 DataContext 绑定 ViewModel,实现数据驱动 UI
- **自定义标题栏应用**：隐藏系统标题栏,由应用层绘制,提升品牌一致性
- **多窗口应用**：动态创建多个独立窗口,每个窗口独立管理
- **工具类应用**：使用 Padding 设置内边距,避免内容紧贴边缘
- **现代 UI 应用**：使用 CornerPreference 设置圆角,DWM 玻璃帧实现半透明效果

### 下一步学习建议

- **模块元数据**：阅读 [03-ModuleMetadata.md](03-ModuleMetadata.md),了解 Api.h 和 WindowAll.h 的使用
- **WindowContext 详解**：阅读 [01-WindowContext.md](01-WindowContext.md),了解懒初始化的底层机制
- **Application 实现**：阅读 mine.ui.app 模块文档,了解 Application 的完整实现
- **依赖属性系统**：阅读 mine.ui.property 模块文档,了解依赖属性的实现原理
- **数据绑定**：阅读 mine.ui.binding 模块文档,了解 Binding 的高级用法

---

**文档版本**：1.0.0  
**最后更新**：2026-06-11  
**作者**：MineFramework 开发团队
