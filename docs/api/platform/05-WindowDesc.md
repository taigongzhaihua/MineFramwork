# WindowDesc 详细接口文档

## 概述

`WindowDesc` 是 `mine.platform.abi` 模块的**窗口创建参数结构体**，传递给 `IApplicationHost::create_window()` 以配置新窗口的初始状态。

**核心特性：**
- **11 个字段**：title、size、position、auto_position、resizable、frameless、transparent、always_on_top、startup_hidden、kind、parent_native_handle
- **逻辑像素**：所有尺寸/坐标使用逻辑像素（DPI 缩放后的值）
- **默认值合理**：800x600 大小、自动居中、可调整大小、普通窗口类型
- **POD 类型**：聚合初始化，ABI 稳定

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/WindowDesc.h
```

---

## 类型定义

```cpp
namespace mine::platform {

struct WindowDesc {
    core::StringView title{};                    // 窗口标题（UTF-8）
    math::Size size{800.0f, 600.0f};            // 初始客户区尺寸（逻辑像素）
    math::Point position{};                      // 初始屏幕位置（逻辑像素，左上角）
    bool auto_position{true};                    // 是否由系统决定初始位置
    bool resizable{true};                        // 窗口是否可由用户调整大小
    bool frameless{false};                       // 是否隐藏系统标题栏与边框
    bool transparent{false};                     // 是否开启窗口背景透明
    bool always_on_top{false};                   // 是否始终显示在其他窗口之上
    bool startup_hidden{false};                  // 创建后是否保持隐藏
    WindowKind kind{WindowKind::Normal};         // 窗口类型
    void* parent_native_handle{nullptr};         // 父窗口句柄
};

}
```

---

## 成员字段

### title

```cpp
core::StringView title{};
```

**描述**：窗口标题，显示在标题栏中（UTF-8 编码）。

**默认值**：空字符串

**特征**：
- UTF-8 编码的字符串视图
- 创建后可通过 `IWindow::set_title()` 修改
- `frameless = true` 时标题不显示（但系统任务栏可能显示）

**平台行为**：
- **Windows**：显示在标题栏和任务栏
- **macOS**：显示在标题栏和 Dock
- **Linux**：显示在窗口管理器的标题栏

**示例**：
```cpp
WindowDesc desc;
desc.title = "我的应用";
auto window = host->create_window(desc);
```

---

### size

```cpp
math::Size size{800.0f, 600.0f};
```

**描述**：初始客户区尺寸（逻辑像素）。

**默认值**：800x600

**特征**：
- **逻辑像素**：96 DPI 基准，自动 DPI 缩放
- **客户区尺寸**：不包含标题栏和边框
- 创建后可通过 `IWindow::set_size()` 修改

**平台行为**：
- **Windows**：调用 `AdjustWindowRectEx()` 计算包含边框的窗口矩形
- **macOS**：设置 `contentSize`
- **Linux**：X11 `XResizeWindow()` 客户区尺寸

**示例**：
```cpp
WindowDesc desc;
desc.size = {1920, 1080};  // Full HD
auto window = host->create_window(desc);
```

---

### position

```cpp
math::Point position{};
```

**描述**：初始屏幕位置（逻辑像素，窗口左上角）。

**默认值**：`{0, 0}`（仅在 `auto_position = false` 时生效）

**特征**：
- **逻辑像素**：96 DPI 基准，自动 DPI 缩放
- **屏幕坐标**：相对于主显示器左上角
- 仅当 `auto_position = false` 时生效
- 创建后可通过 `IWindow::set_position()` 修改

**平台行为**：
- **Windows**：`CreateWindowEx()` 的 `x`、`y` 参数
- **macOS**：`setFrameTopLeftPoint:`
- **Linux**：`XMoveWindow()` 位置

**示例**：
```cpp
WindowDesc desc;
desc.auto_position = false;
desc.position = {100, 100};  // 屏幕左上角偏移 (100, 100)
auto window = host->create_window(desc);
```

---

### auto_position

```cpp
bool auto_position{true};
```

**描述**：是否由系统决定初始位置。

**默认值**：`true`（系统自动居中或级联）

**特征**：
- `true`：系统决定位置（通常居中或级联）
- `false`：使用 `position` 字段的值

**平台行为**：
- **Windows**：`true` 时使用 `CW_USEDEFAULT`，窗口级联显示
- **macOS**：`true` 时调用 `center()` 居中
- **Linux**：`true` 时由窗口管理器决定位置

**示例**：
```cpp
// 系统自动居中
WindowDesc desc1;
desc1.auto_position = true;  // 默认值

// 手动指定位置
WindowDesc desc2;
desc2.auto_position = false;
desc2.position = {200, 200};
```

---

### resizable

```cpp
bool resizable{true};
```

**描述**：窗口是否可由用户调整大小。

**默认值**：`true`

**特征**：
- `true`：用户可拖动边框调整大小
- `false`：窗口固定尺寸，无法调整
- 窗口最大化后仍可恢复到原始可调整大小状态

**平台行为**：
- **Windows**：`true` 时添加 `WS_THICKFRAME` 样式
- **macOS**：`true` 时添加 `NSWindowStyleMaskResizable`
- **Linux**：`true` 时设置 `_NET_WM_STATE_RESIZABLE`

**示例**：
```cpp
// 固定大小的对话框
WindowDesc desc;
desc.kind = WindowKind::Dialog;
desc.size = {400, 300};
desc.resizable = false;
auto dialog = host->create_window(desc);
```

---

### frameless

```cpp
bool frameless{false};
```

**描述**：是否隐藏系统标题栏与边框（自定义标题栏模式）。

**默认值**：`false`

**特征**：
- `true`：无标题栏、无边框、无系统按钮（最小化/最大化/关闭）
- `false`：标准系统窗口装饰
- 自定义标题栏应用（如 VS Code、Chrome）通常使用此模式

**平台行为**：
- **Windows**：使用 `WS_POPUP` 样式，配合 DWM 阴影
- **macOS**：`NSWindowStyleMaskBorderless`
- **Linux**：移除窗口管理器装饰（`_MOTIF_WM_HINTS`）

**注意**：
- 应用需自行实现标题栏、拖动、最小化/最大化/关闭按钮
- 配合 `WindowChromeDesc` 使用可保留系统阴影和圆角

**示例**：
```cpp
// 自定义标题栏窗口
WindowDesc desc;
desc.frameless = true;
desc.size = {1024, 768};
auto window = host->create_window(desc);

// 设置自定义 Chrome
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;  // 自定义标题栏高度
window->set_chrome(chrome);
```

---

### transparent

```cpp
bool transparent{false};
```

**描述**：是否开启窗口背景透明（需合成器支持）。

**默认值**：`false`

**特征**：
- `true`：窗口背景透明，可渲染半透明效果
- `false`：窗口背景不透明
- 需要桌面合成器支持（Windows Vista+、macOS、现代 Linux 桌面）

**平台行为**：
- **Windows**：`WS_EX_LAYERED` + `UpdateLayeredWindow()` 或 DWM 透明
- **macOS**：`[window setOpaque:NO]` + `[window setBackgroundColor:[NSColor clearColor]]`
- **Linux**：ARGB 视觉 + 合成器

**性能影响**：
- 透明窗口禁用某些硬件加速优化
- 每帧需要重新合成整个窗口
- 刷新率可能降低

**示例**：
```cpp
// 透明窗口（如通知气泡）
WindowDesc desc;
desc.frameless = true;
desc.transparent = true;
desc.size = {300, 100};
auto notification = host->create_window(desc);
```

---

### always_on_top

```cpp
bool always_on_top{false};
```

**描述**：是否始终显示在其他窗口之上。

**默认值**：`false`

**特征**：
- `true`：窗口始终位于 Z 顺序顶层（除全屏窗口外）
- `false`：正常 Z 顺序行为
- 常用于工具窗口、画中画视频、通知

**平台行为**：
- **Windows**：`HWND_TOPMOST` 窗口级别
- **macOS**：`[window setLevel:NSFloatingWindowLevel]`
- **Linux**：`_NET_WM_STATE_ABOVE`

**示例**：
```cpp
// 画中画视频窗口
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.size = {320, 180};
desc.resizable = true;
desc.always_on_top = true;
auto pip = host->create_window(desc);
```

---

### startup_hidden

```cpp
bool startup_hidden{false};
```

**描述**：创建后是否保持隐藏（不立即调用 `show()`）。

**默认值**：`false`（创建后自动显示）

**特征**：
- `true`：窗口创建后隐藏，需手动调用 `IWindow::show()` 显示
- `false`：窗口创建后立即显示
- 用于预加载窗口或等待内容准备完成

**用途**：
- 启动闪屏：主窗口隐藏，等待资源加载完成后显示
- 后台窗口：预创建但不立即显示

**示例**：
```cpp
// 主窗口，等待资源加载后显示
WindowDesc desc;
desc.title = "主窗口";
desc.startup_hidden = true;
auto window = host->create_window(desc);

// 加载资源...
load_resources();

// 资源加载完成，显示窗口
window->show();
```

---

### kind

```cpp
WindowKind kind{WindowKind::Normal};
```

**描述**：窗口类型。

**默认值**：`WindowKind::Normal`

**可选值**：
- `WindowKind::Normal`：普通应用窗口
- `WindowKind::Tool`：工具窗口（浮动面板）
- `WindowKind::Dialog`：模态对话框
- `WindowKind::Splash`：启动闪屏
- `WindowKind::Popup`：弹出窗口（上下文菜单）

**详细说明**：参见 [02-WindowKind.md](02-WindowKind.md)

**示例**：
```cpp
// 工具窗口
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.title = "工具面板";
desc.resizable = false;
desc.always_on_top = true;
auto tool = host->create_window(desc);
```

---

### parent_native_handle

```cpp
void* parent_native_handle{nullptr};
```

**描述**：父窗口句柄（用于模态对话框）。

**默认值**：`nullptr`（无父窗口）

**特征**：
- 非 `nullptr` 时，窗口关联到父窗口
- 父窗口关闭时，子窗口自动关闭
- 模态对话框（`Dialog` 类型）必须设置父窗口

**平台行为**：
- **Windows**：`SetParent(hwnd, parent_hwnd)`
- **macOS**：`[window setParentWindow:parentWindow]`
- **Linux**：`XSetTransientForHint()`

**示例**：
```cpp
// 模态对话框
IWindow* parent_window = /* 主窗口 */;
NativeHandle parent_handle = parent_window->native_handle();

WindowDesc desc;
desc.kind = WindowKind::Dialog;
desc.title = "设置";
desc.size = {400, 300};
desc.resizable = false;
desc.parent_native_handle = parent_handle.ptr;
auto dialog = host->create_window(desc);
dialog->show();
```

---

## 使用场景

### 1. 标准应用主窗口

```cpp
WindowDesc desc;
desc.title = "我的应用";
desc.size = {1024, 768};
desc.resizable = true;
desc.kind = WindowKind::Normal;
auto main_window = host->create_window(desc);
main_window->show();
```

---

### 2. 固定大小对话框

```cpp
WindowDesc desc;
desc.title = "设置";
desc.size = {400, 300};
desc.resizable = false;
desc.kind = WindowKind::Dialog;
desc.parent_native_handle = parent_window->native_handle().ptr;
auto dialog = host->create_window(desc);
dialog->show();
```

---

### 3. 自定义标题栏窗口

```cpp
// 无边框窗口 + 自定义 Chrome
WindowDesc desc;
desc.title = "现代应用";
desc.size = {1280, 720};
desc.frameless = true;
auto window = host->create_window(desc);

// 配置自定义标题栏
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
chrome.corner_preference = WindowCornerPreference::Round;
window->set_chrome(chrome);
window->show();
```

---

### 4. 透明通知窗口

```cpp
WindowDesc desc;
desc.kind = WindowKind::Popup;
desc.size = {300, 100};
desc.frameless = true;
desc.transparent = true;
desc.always_on_top = true;
desc.auto_position = false;
desc.position = {screen_width - 320, screen_height - 120};
auto notification = host->create_window(desc);
notification->show();
```

---

### 5. 工具面板

```cpp
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.title = "工具";
desc.size = {250, 400};
desc.resizable = false;
desc.always_on_top = true;
auto tool = host->create_window(desc);
tool->show();
```

---

### 6. 启动闪屏

```cpp
// 主窗口隐藏启动
WindowDesc main_desc;
main_desc.title = "主窗口";
main_desc.startup_hidden = true;
auto main_window = host->create_window(main_desc);

// 闪屏窗口
WindowDesc splash_desc;
splash_desc.kind = WindowKind::Splash;
splash_desc.size = {600, 400};
splash_desc.frameless = true;
auto splash = host->create_window(splash_desc);
splash->show();

// 加载资源...
load_resources();

// 关闭闪屏，显示主窗口
splash->close();
main_window->show();
```

---

## 字段组合建议

### 普通窗口

```cpp
WindowDesc desc;
desc.kind = WindowKind::Normal;
desc.resizable = true;
desc.frameless = false;
```

---

### 固定大小对话框

```cpp
WindowDesc desc;
desc.kind = WindowKind::Dialog;
desc.resizable = false;
desc.parent_native_handle = parent_handle.ptr;
```

---

### 自定义标题栏

```cpp
WindowDesc desc;
desc.frameless = true;
// 配合 WindowChromeDesc 使用
```

---

### 工具窗口

```cpp
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.always_on_top = true;
desc.resizable = false;
```

---

### 透明窗口

```cpp
WindowDesc desc;
desc.frameless = true;
desc.transparent = true;
// 需要自行渲染背景
```

---

## 最佳实践

### 1. 使用合理的默认尺寸

```cpp
// ✅ 推荐：使用常见分辨率
WindowDesc desc;
desc.size = {1024, 768};  // 4:3
desc.size = {1280, 720};  // 16:9 HD
desc.size = {1920, 1080}; // 16:9 Full HD

// ❌ 不推荐：过小或过大的尺寸
desc.size = {100, 100};   // 太小
desc.size = {5000, 3000}; // 可能超出屏幕
```

---

### 2. 对话框禁用调整大小

```cpp
// ✅ 推荐：对话框固定大小
WindowDesc desc;
desc.kind = WindowKind::Dialog;
desc.resizable = false;

// ❌ 不推荐：对话框可调整大小（布局复杂）
desc.resizable = true;
```

---

### 3. 模态对话框设置父窗口

```cpp
// ✅ 推荐：设置父窗口
WindowDesc desc;
desc.kind = WindowKind::Dialog;
desc.parent_native_handle = parent_window->native_handle().ptr;

// ❌ 不推荐：模态对话框无父窗口
desc.parent_native_handle = nullptr;  // 无法阻塞父窗口
```

---

### 4. 透明窗口配合无边框

```cpp
// ✅ 推荐：透明窗口 + 无边框
WindowDesc desc;
desc.transparent = true;
desc.frameless = true;

// ❌ 不推荐：透明窗口 + 系统边框（视觉冲突）
desc.transparent = true;
desc.frameless = false;
```

---

## 常见陷阱

### 1. 忘记设置 auto_position = false

```cpp
// ❌ 问题：设置 position 但未禁用 auto_position
WindowDesc desc;
desc.position = {100, 100};  // 被忽略
desc.auto_position = true;   // 默认值

// ✅ 解决：禁用 auto_position
desc.auto_position = false;
desc.position = {100, 100};
```

---

### 2. 自定义标题栏忘记设置 Chrome

```cpp
// ❌ 问题：无边框但未配置 Chrome
WindowDesc desc;
desc.frameless = true;
auto window = host->create_window(desc);
// 窗口无法拖动、无法调整大小

// ✅ 解决：配置 Chrome
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
window->set_chrome(chrome);
```

---

### 3. 模态对话框未设置父窗口

```cpp
// ❌ 问题：Dialog 类型但无父窗口
WindowDesc desc;
desc.kind = WindowKind::Dialog;
desc.parent_native_handle = nullptr;  // 无父窗口
auto dialog = host->create_window(desc);
// 无法实现模态行为

// ✅ 解决：设置父窗口
desc.parent_native_handle = parent_window->native_handle().ptr;
```

---

### 4. 透明窗口性能问题

```cpp
// ❌ 问题：大尺寸透明窗口
WindowDesc desc;
desc.transparent = true;
desc.size = {1920, 1080};  // 全屏透明，性能差

// ✅ 解决：透明窗口尽量小
desc.size = {300, 100};  // 仅通知区域透明
```

---

## 完整示例

```cpp
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/WindowKind.h>

class Application {
    IApplicationHost* host_;
    
public:
    void create_main_window() {
        WindowDesc desc;
        desc.title = "主窗口";
        desc.size = {1024, 768};
        desc.resizable = true;
        desc.kind = WindowKind::Normal;
        auto main_window = host_->create_window(desc);
        main_window->show();
    }
    
    void create_settings_dialog(IWindow* parent) {
        WindowDesc desc;
        desc.title = "设置";
        desc.size = {400, 300};
        desc.resizable = false;
        desc.kind = WindowKind::Dialog;
        desc.parent_native_handle = parent->native_handle().ptr;
        auto dialog = host_->create_window(desc);
        dialog->show();
    }
    
    void create_custom_chrome_window() {
        WindowDesc desc;
        desc.title = "现代应用";
        desc.size = {1280, 720};
        desc.frameless = true;
        auto window = host_->create_window(desc);
        
        WindowChromeDesc chrome;
        chrome.enabled = true;
        chrome.caption_height = 40.0f;
        chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
        chrome.corner_preference = WindowCornerPreference::Round;
        window->set_chrome(chrome);
        window->show();
    }
    
    void create_notification() {
        WindowDesc desc;
        desc.kind = WindowKind::Popup;
        desc.size = {300, 100};
        desc.frameless = true;
        desc.transparent = true;
        desc.always_on_top = true;
        desc.auto_position = false;
        
        // 定位到屏幕右下角
        auto screen_size = get_primary_screen_size();
        desc.position = {screen_size.width - 320, screen_size.height - 120};
        
        auto notification = host_->create_window(desc);
        notification->show();
        
        // 3 秒后自动关闭
        schedule_after(3000ms, [notification = notification.get()]() {
            notification->close();
        });
    }
};
```

---

## 总结

`WindowDesc` 是 `mine.platform.abi` 模块的窗口创建参数结构体，具备：

1. **11 个字段**：title、size、position、auto_position、resizable、frameless、transparent、always_on_top、startup_hidden、kind、parent_native_handle
2. **逻辑像素**：所有尺寸/坐标使用逻辑像素，自动 DPI 缩放
3. **默认值合理**：800x600 大小、自动居中、可调整大小、普通窗口类型
4. **POD 类型**：聚合初始化，ABI 稳定

**使用建议**：
- 使用合理的默认尺寸（1024x768、1280x720 等）
- 对话框禁用调整大小（`resizable = false`）
- 模态对话框设置父窗口（`parent_native_handle`）
- 透明窗口配合无边框（`transparent + frameless`）
- 自定义标题栏配合 `WindowChromeDesc` 使用
- 手动指定位置需禁用 `auto_position`
