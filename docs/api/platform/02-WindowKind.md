# WindowKind 详细接口文档

## 概述

`WindowKind` 是 `mine.platform.abi` 模块的**窗口类型枚举**，决定窗口的系统装饰（标题栏、边框）和行为（任务栏显示、模态性）。

**核心特性：**
- **五种类型**：Normal（普通窗口）、Tool（工具窗口）、Dialog（对话框）、Splash（闪屏）、Popup（弹出窗口）
- **平台无关**：抽象系统窗口类型，支持 Windows/macOS/Linux
- **创建时指定**：通过 `WindowDesc::kind` 传递给 `IApplicationHost::create_window()`

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/WindowKind.h
```

---

## 枚举定义

```cpp
namespace mine::platform {

enum class WindowKind : int {
    Normal,   // 普通应用窗口
    Tool,     // 工具窗口
    Dialog,   // 模态对话框
    Splash,   // 启动闪屏
    Popup,    // 弹出窗口
};

}
```

---

## 枚举值

### Normal

```cpp
WindowKind::Normal
```

**描述**：普通应用窗口，最常用的窗口类型。

**特征**：
- 完整的标题栏和边框
- 在任务栏/Dock 中显示图标
- 支持最小化、最大化、关闭按钮
- 可独立于其他窗口存在

**平台映射**：
- **Windows**：`WS_OVERLAPPEDWINDOW`（标题栏 + 边框 + 系统菜单）
- **macOS**：`NSWindow` with `NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable`
- **Linux (X11)**：`_NET_WM_WINDOW_TYPE_NORMAL`

**用途**：
- 应用程序主窗口
- 文档编辑器窗口
- 多文档界面（MDI）子窗口

**示例**：
```cpp
WindowDesc desc;
desc.kind = WindowKind::Normal;
desc.title = "主窗口";
auto window = host->create_window(desc);
```

---

### Tool

```cpp
WindowKind::Tool
```

**描述**：工具窗口，通常用于浮动面板和工具栏。

**特征**：
- **小标题栏**：高度约为普通窗口标题栏的 60%-80%
- **不在任务栏显示**：避免任务栏拥挤
- **始终在普通窗口之上**：保持可访问性
- **通常不可最小化**：关闭即销毁

**平台映射**：
- **Windows**：`WS_EX_TOOLWINDOW`（小标题栏，不在任务栏）
- **macOS**：`NSWindowStyleMaskUtilityWindow`（浮动面板）
- **Linux (X11)**：`_NET_WM_WINDOW_TYPE_UTILITY`

**用途**：
- Photoshop/Blender 的浮动工具面板
- 颜色选择器
- 字符映射表
- 调试器的 Watch 窗口

**示例**：
```cpp
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.title = "工具面板";
desc.size = {300, 400};
desc.resizable = false;
auto tool_window = host->create_window(desc);
```

---

### Dialog

```cpp
WindowKind::Dialog
```

**描述**：模态对话框，阻塞父窗口交互。

**特征**：
- **模态性**：显示时禁用父窗口输入
- **标准边框**：通常无最大化/最小化按钮
- **任务栏显示**：根据平台策略（Windows 显示，macOS 通常不显示）
- **居中显示**：相对于父窗口或屏幕中心

**平台映射**：
- **Windows**：`WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU`（无最大化/最小化按钮） + `SetParent()` 关联父窗口
- **macOS**：`[NSApp runModalForWindow:]` 模态循环
- **Linux (X11)**：`_NET_WM_WINDOW_TYPE_DIALOG` + `_NET_WM_STATE_MODAL`

**用途**：
- 文件打开/保存对话框
- 设置对话框
- 确认/警告对话框
- 登录对话框

**示例**：
```cpp
WindowDesc desc;
desc.kind = WindowKind::Dialog;
desc.title = "设置";
desc.size = {400, 300};
desc.resizable = false;
desc.frameless = false;
auto dialog = host->create_window(desc);
dialog->set_modal(parent_window);  // 设置父窗口
dialog->show();
```

---

### Splash

```cpp
WindowKind::Splash
```

**描述**：启动闪屏窗口，应用启动时显示品牌/加载进度。

**特征**：
- **无边框**：无标题栏和系统按钮
- **不在任务栏显示**：临时窗口
- **无阴影**：避免视觉杂乱
- **居中显示**：屏幕中心
- **短暂存在**：通常 2-5 秒后关闭

**平台映射**：
- **Windows**：`WS_POPUP | WS_EX_TOOLWINDOW`（无边框，不在任务栏）
- **macOS**：`NSWindowStyleMaskBorderless`（无边框）
- **Linux (X11)**：`_NET_WM_WINDOW_TYPE_SPLASH`

**用途**：
- 应用程序启动闪屏
- 品牌展示
- 加载进度显示

**示例**：
```cpp
WindowDesc desc;
desc.kind = WindowKind::Splash;
desc.size = {600, 400};
desc.frameless = true;
desc.resizable = false;
desc.auto_position = true;  // 自动居中
auto splash = host->create_window(desc);
splash->show();

// 主窗口加载完成后关闭闪屏
on_main_window_ready([splash]() {
    splash->close();
});
```

---

### Popup

```cpp
WindowKind::Popup
```

**描述**：弹出窗口，通常用于上下文菜单、工具提示、下拉列表。

**特征**：
- **无边框**：无标题栏和系统按钮
- **不在任务栏显示**：临时窗口
- **自动取消焦点关闭**：点击窗口外部自动关闭
- **始终在父窗口之上**：保持可见性
- **不可调整大小**：固定尺寸

**平台映射**：
- **Windows**：`WS_POPUP`（无边框） + 焦点丢失时关闭
- **macOS**：`NSWindowStyleMaskBorderless` + `[window setLevel:NSPopUpMenuWindowLevel]`
- **Linux (X11)**：`_NET_WM_WINDOW_TYPE_DROPDOWN_MENU` / `_NET_WM_WINDOW_TYPE_POPUP_MENU`

**用途**：
- 右键上下文菜单
- ComboBox 下拉列表
- 自动完成建议
- 工具提示（Tooltip）

**示例**：
```cpp
WindowDesc desc;
desc.kind = WindowKind::Popup;
desc.size = {200, 150};
desc.frameless = true;
desc.resizable = false;
desc.position = {mouse_x, mouse_y};
desc.auto_position = false;
auto popup = host->create_window(desc);
popup->show();

// 失去焦点时自动关闭
popup->events().on_focus_lost([popup]() {
    popup->close();
});
```

---

## 使用场景

### 1. 应用程序主窗口

```cpp
// 普通应用窗口
WindowDesc desc;
desc.kind = WindowKind::Normal;
desc.title = "我的应用";
desc.size = {1024, 768};
auto main_window = host->create_window(desc);
main_window->show();
```

---

### 2. 浮动工具面板

```cpp
// 工具窗口，始终在主窗口之上
WindowDesc tool_desc;
tool_desc.kind = WindowKind::Tool;
tool_desc.title = "颜色面板";
tool_desc.size = {250, 300};
tool_desc.resizable = false;
tool_desc.always_on_top = true;
auto color_panel = host->create_window(tool_desc);
color_panel->show();
```

---

### 3. 确认对话框

```cpp
// 模态对话框，阻塞父窗口
WindowDesc dialog_desc;
dialog_desc.kind = WindowKind::Dialog;
dialog_desc.title = "确认删除";
dialog_desc.size = {300, 150};
dialog_desc.resizable = false;
auto dialog = host->create_window(dialog_desc);
dialog->set_modal(parent_window);
dialog->show();
```

---

### 4. 启动闪屏

```cpp
// 启动闪屏，无边框居中显示
WindowDesc splash_desc;
splash_desc.kind = WindowKind::Splash;
splash_desc.size = {600, 400};
splash_desc.frameless = true;
auto splash = host->create_window(splash_desc);
splash->show();

// 3 秒后自动关闭
schedule_after(3000ms, [splash]() {
    splash->close();
});
```

---

### 5. 右键菜单

```cpp
// 弹出菜单，点击外部自动关闭
WindowDesc menu_desc;
menu_desc.kind = WindowKind::Popup;
menu_desc.size = {180, 120};
menu_desc.frameless = true;
menu_desc.position = {mouse_x, mouse_y};
menu_desc.auto_position = false;
auto menu = host->create_window(menu_desc);
menu->show();

// 失去焦点时关闭
menu->events().on_focus_lost([menu]() {
    menu->close();
});
```

---

## 平台差异

### Windows

| WindowKind | Win32 样式 | 说明 |
|------------|-----------|------|
| `Normal` | `WS_OVERLAPPEDWINDOW` | 完整标题栏 + 边框 |
| `Tool` | `WS_EX_TOOLWINDOW` | 小标题栏，不在任务栏 |
| `Dialog` | `WS_OVERLAPPED \| WS_CAPTION` | 无最大化/最小化按钮 |
| `Splash` | `WS_POPUP \| WS_EX_TOOLWINDOW` | 无边框，不在任务栏 |
| `Popup` | `WS_POPUP` | 无边框，临时窗口 |

---

### macOS

| WindowKind | NSWindow 样式 | 说明 |
|------------|--------------|------|
| `Normal` | `NSWindowStyleMaskTitled` 等 | 完整装饰 |
| `Tool` | `NSWindowStyleMaskUtilityWindow` | 浮动面板 |
| `Dialog` | `runModalForWindow:` | 模态循环 |
| `Splash` | `NSWindowStyleMaskBorderless` | 无边框 |
| `Popup` | `NSWindowStyleMaskBorderless` + `NSPopUpMenuWindowLevel` | 菜单级别 |

---

### Linux (X11)

| WindowKind | _NET_WM_WINDOW_TYPE | 说明 |
|------------|---------------------|------|
| `Normal` | `_NET_WM_WINDOW_TYPE_NORMAL` | 普通窗口 |
| `Tool` | `_NET_WM_WINDOW_TYPE_UTILITY` | 工具窗口 |
| `Dialog` | `_NET_WM_WINDOW_TYPE_DIALOG` + `_NET_WM_STATE_MODAL` | 模态对话框 |
| `Splash` | `_NET_WM_WINDOW_TYPE_SPLASH` | 闪屏 |
| `Popup` | `_NET_WM_WINDOW_TYPE_POPUP_MENU` | 弹出菜单 |

---

## 最佳实践

### 1. 选择合适的窗口类型

```cpp
// ✅ 推荐：主窗口使用 Normal
WindowDesc main_desc;
main_desc.kind = WindowKind::Normal;

// ✅ 推荐：浮动工具面板使用 Tool
WindowDesc tool_desc;
tool_desc.kind = WindowKind::Tool;

// ❌ 不推荐：工具面板使用 Normal（会在任务栏显示）
WindowDesc wrong_desc;
wrong_desc.kind = WindowKind::Normal;  // 任务栏拥挤
```

---

### 2. Tool 窗口始终在主窗口之上

```cpp
// ✅ 推荐：Tool 窗口设置 always_on_top
WindowDesc tool_desc;
tool_desc.kind = WindowKind::Tool;
tool_desc.always_on_top = true;  // 保持可见性
```

---

### 3. Popup 窗口失去焦点自动关闭

```cpp
// ✅ 推荐：Popup 失去焦点时关闭
auto popup = host->create_window(popup_desc);
popup->events().on_focus_lost([popup]() {
    popup->close();
});

// ❌ 不推荐：Popup 不自动关闭（用户需要手动关闭）
```

---

### 4. Splash 窗口定时关闭

```cpp
// ✅ 推荐：闪屏窗口定时或条件关闭
auto splash = host->create_window(splash_desc);
splash->show();

on_resources_loaded([splash]() {
    splash->close();  // 资源加载完成后关闭
});

// ❌ 不推荐：闪屏窗口永久显示
```

---

## 常见陷阱

### 1. Tool 窗口在任务栏显示

```cpp
// ❌ 问题：使用 Normal 类型作为工具窗口
WindowDesc desc;
desc.kind = WindowKind::Normal;  // 会在任务栏显示

// ✅ 解决：使用 Tool 类型
desc.kind = WindowKind::Tool;  // 不在任务栏显示
```

---

### 2. Dialog 窗口未设置模态

```cpp
// ❌ 问题：Dialog 类型但未设置模态性
WindowDesc desc;
desc.kind = WindowKind::Dialog;
auto dialog = host->create_window(desc);
dialog->show();  // 父窗口仍可交互

// ✅ 解决：调用 set_modal()
dialog->set_modal(parent_window);
```

---

### 3. Popup 窗口未正确定位

```cpp
// ❌ 问题：Popup 使用自动定位
WindowDesc desc;
desc.kind = WindowKind::Popup;
desc.auto_position = true;  // 可能定位错误

// ✅ 解决：手动指定位置
desc.auto_position = false;
desc.position = {mouse_x, mouse_y};
```

---

## 完整示例

```cpp
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/WindowKind.h>

class Application {
    IApplicationHost* host_;
    IWindow* main_window_;
    IWindow* tool_window_;
    
public:
    void create_windows() {
        // 主窗口
        WindowDesc main_desc;
        main_desc.kind = WindowKind::Normal;
        main_desc.title = "主窗口";
        main_desc.size = {1024, 768};
        main_window_ = host_->create_window(main_desc).release();
        main_window_->show();
        
        // 工具面板
        WindowDesc tool_desc;
        tool_desc.kind = WindowKind::Tool;
        tool_desc.title = "工具";
        tool_desc.size = {250, 400};
        tool_desc.resizable = false;
        tool_desc.always_on_top = true;
        tool_window_ = host_->create_window(tool_desc).release();
        tool_window_->show();
    }
    
    void show_settings_dialog() {
        WindowDesc dialog_desc;
        dialog_desc.kind = WindowKind::Dialog;
        dialog_desc.title = "设置";
        dialog_desc.size = {400, 300};
        dialog_desc.resizable = false;
        auto dialog = host_->create_window(dialog_desc);
        dialog->set_modal(main_window_);
        dialog->show();
    }
    
    void show_context_menu(int x, int y) {
        WindowDesc menu_desc;
        menu_desc.kind = WindowKind::Popup;
        menu_desc.size = {180, 120};
        menu_desc.frameless = true;
        menu_desc.position = {static_cast<float>(x), static_cast<float>(y)};
        menu_desc.auto_position = false;
        auto menu = host_->create_window(menu_desc);
        
        // 失去焦点时关闭
        menu->events().on_focus_lost([menu = menu.get()]() {
            menu->close();
        });
        
        menu->show();
    }
};
```

---

## 总结

`WindowKind` 是 `mine.platform.abi` 模块的窗口类型枚举，具备：

1. **五种类型**：Normal / Tool / Dialog / Splash / Popup
2. **平台无关**：抽象 Windows、macOS、Linux 窗口类型
3. **创建时指定**：通过 `WindowDesc::kind` 设置
4. **行为差异**：决定标题栏、边框、任务栏显示、模态性

**使用建议**：
- 主窗口使用 `Normal`
- 浮动面板使用 `Tool` + `always_on_top`
- 模态对话框使用 `Dialog` + `set_modal()`
- 启动闪屏使用 `Splash` + 定时关闭
- 临时菜单使用 `Popup` + 失去焦点关闭
