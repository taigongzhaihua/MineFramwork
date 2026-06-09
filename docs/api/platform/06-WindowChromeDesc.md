# WindowChromeDesc 详细接口文档

## 概述

`WindowChromeDesc` 是 `mine.platform.abi` 模块的**自定义窗口标题栏（Chrome）配置描述符**，通过 `IWindow::set_chrome()` 传递给平台实现层，实现自定义标题栏行为。

**核心特性：**
- **5 个字段**：enabled、caption_height、resize_border_thickness、corner_preference、glass_frame_thickness
- **WPF WindowChrome 设计**：优化为单次提交模式，避免零散更新
- **逻辑像素**：所有尺寸使用逻辑像素（96 DPI 基准）
- **平台集成**：调用 DWM（Windows）/ Compositor（macOS/Linux）API

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/WindowChromeDesc.h
```

---

## 类型定义

```cpp
namespace mine::platform {

struct WindowChromeDesc {
    bool enabled{false};                                                           // 是否启用自定义 Chrome
    float caption_height{32.0f};                                                   // 可拖拽标题栏区域高度（逻辑像素）
    math::Thickness resize_border_thickness{math::Thickness::uniform(4.0f)};      // 可调整大小的边框厚度（逻辑像素）
    WindowCornerPreference corner_preference{WindowCornerPreference::SystemDefault}; // 窗口圆角首选项
    math::Thickness glass_frame_thickness{};                                       // DWM 玻璃帧延伸厚度（逻辑像素）
};

}
```

---

## 成员字段

### enabled

```cpp
bool enabled{false};
```

**描述**：是否启用自定义 Chrome。

**默认值**：`false`

**特征**：
- `false`：恢复系统默认标题栏行为，忽略所有其他字段
- `true`：启用自定义 Chrome，应用其他字段配置

**用途**：
- 统一开关，无需为"禁用状态"维护特殊值
- 禁用自定义 Chrome 时恢复系统默认行为

**示例**：
```cpp
// 禁用自定义 Chrome，恢复系统默认
WindowChromeDesc chrome;
chrome.enabled = false;
window->set_chrome(chrome);

// 启用自定义 Chrome
chrome.enabled = true;
chrome.caption_height = 40.0f;
window->set_chrome(chrome);
```

---

### caption_height

```cpp
float caption_height{32.0f};
```

**描述**：可拖拽标题栏区域高度（逻辑像素）。

**默认值**：32.0f

**特征**：
- **逻辑像素**：96 DPI 基准，自动 DPI 缩放
- **拖拽区域**：窗口顶部该高度范围内的区域识别为标题栏（HTCAPTION）
- **用户交互**：可拖拽移动窗口、双击最大化/还原
- **0 值**：不提供任何可拖拽区域，应用完全接管命中测试

**平台行为**：
- **Windows**：在 `WM_NCHITTEST` 中返回 `HTCAPTION`
- **macOS**：设置 `-[NSView mouseDownCanMoveWindow]`
- **Linux**：处理 `_NET_WM_MOVERESIZE` 消息

**示例**：
```cpp
// 40px 高度的标题栏
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
window->set_chrome(chrome);

// 无拖拽区域，应用自定义命中测试
chrome.caption_height = 0.0f;
```

---

### resize_border_thickness

```cpp
math::Thickness resize_border_thickness{math::Thickness::uniform(4.0f)};
```

**描述**：可调整大小的边框厚度（逻辑像素，四边各自独立）。

**默认值**：`math::Thickness::uniform(4.0f)`（四边均 4px）

**特征**：
- **逻辑像素**：96 DPI 基准，自动 DPI 缩放
- **四边独立**：`{left, top, right, bottom}` 各自指定厚度
- **调整区域**：窗口四边各自 `resize_border_thickness` 宽度内的区域识别为 resize 边缘
- **自动失效**：窗口最大化或 `resizable = false` 时失效

**平台行为**：
- **Windows**：在 `WM_NCHITTEST` 中返回 `HTLEFT`、`HTTOP`、`HTRIGHT`、`HTBOTTOM` 及四角
- **macOS**：通过 `-[NSView canBecomeFirstResponder]` 和鼠标事件处理
- **Linux**：处理 `_NET_WM_MOVERESIZE` 消息

**示例**：
```cpp
// 四边均 4px 边框
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);

// 仅左右边 8px，上下边 4px
chrome.resize_border_thickness = {8.0f, 4.0f, 8.0f, 4.0f};

// 禁用调整大小
chrome.resize_border_thickness = math::Thickness{};  // 全零
```

---

### corner_preference

```cpp
WindowCornerPreference corner_preference{WindowCornerPreference::SystemDefault};
```

**描述**：窗口圆角首选项。

**默认值**：`WindowCornerPreference::SystemDefault`

**特征**：
- 映射到系统 API（Windows 11: `DWMWA_WINDOW_CORNER_PREFERENCE`）
- 不支持圆角的系统（Windows 10-）忽略此字段

**可选值**：
- `SystemDefault`：系统默认圆角（跟随主题）
- `DoNotRound`：强制直角
- `Round`：大圆角（8-10px）
- `RoundSmall`：小圆角（4-6px）

**详细说明**：参见 [03-WindowCornerPreference.md](03-WindowCornerPreference.md)

**示例**：
```cpp
// 大圆角
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.corner_preference = WindowCornerPreference::Round;

// 直角
chrome.corner_preference = WindowCornerPreference::DoNotRound;
```

---

### glass_frame_thickness

```cpp
math::Thickness glass_frame_thickness{};
```

**描述**：DWM 玻璃帧延伸厚度（逻辑像素）。

**默认值**：`{}`（全零，不延伸）

**特征**：
- **逻辑像素**：96 DPI 基准，自动 DPI 缩放
- **0 值**：不延伸（默认），客户区无玻璃效果
- **正值**：向客户区延伸对应宽度的玻璃帧（毛玻璃半透明效果）
- **-1 值**：`Thickness::uniform(-1.0f)`，延伸覆盖整个客户区（全窗口玻璃效果）
- **DWM 依赖**：仅在 DWM 组合（桌面合成）开启时生效（Windows 8+ 默认开启）

**平台行为**：
- **Windows**：`DwmExtendFrameIntoClientArea()`
- **macOS**：通过 `NSVisualEffectView` 实现毛玻璃效果
- **Linux**：依赖合成器支持（如 GNOME、KDE）

**示例**：
```cpp
// 不延伸玻璃帧（默认）
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.glass_frame_thickness = {};  // 全零

// 顶部延伸 40px 玻璃帧（标题栏区域半透明）
chrome.glass_frame_thickness = {0, 40, 0, 0};

// 全窗口玻璃效果
chrome.glass_frame_thickness = math::Thickness::uniform(-1.0f);
```

---

## 使用场景

### 1. 基本自定义标题栏

```cpp
WindowDesc desc;
desc.frameless = true;
auto window = host->create_window(desc);

WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
window->set_chrome(chrome);
```

---

### 2. 自定义标题栏 + 大圆角

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
chrome.corner_preference = WindowCornerPreference::Round;
window->set_chrome(chrome);
```

---

### 3. 自定义标题栏 + 玻璃帧（毛玻璃标题栏）

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
chrome.glass_frame_thickness = {0, 40, 0, 0};  // 顶部 40px 玻璃帧
window->set_chrome(chrome);
```

---

### 4. 全窗口玻璃效果

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 32.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
chrome.glass_frame_thickness = math::Thickness::uniform(-1.0f);  // 全窗口玻璃
window->set_chrome(chrome);
```

---

### 5. 禁用调整大小（固定窗口）

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 32.0f;
chrome.resize_border_thickness = {};  // 全零，禁用调整大小
window->set_chrome(chrome);
```

---

### 6. 无标题栏拖拽区域（应用自定义命中测试）

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 0.0f;  // 无拖拽区域
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
window->set_chrome(chrome);

// 应用需实现自定义命中测试
window->events().on_hit_test([](math::Point point) -> HitTestResult {
    // 自定义命中测试逻辑
    return HitTestResult::Caption;
});
```

---

### 7. 恢复系统默认标题栏

```cpp
// 禁用自定义 Chrome
WindowChromeDesc chrome;
chrome.enabled = false;
window->set_chrome(chrome);
```

---

## 字段组合建议

### VS Code 风格（自定义标题栏 + 圆角）

```cpp
WindowDesc desc;
desc.frameless = true;
auto window = host->create_window(desc);

WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
chrome.corner_preference = WindowCornerPreference::Round;
window->set_chrome(chrome);
```

---

### Chrome 风格（自定义标题栏 + 玻璃帧）

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 48.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
chrome.glass_frame_thickness = {0, 48, 0, 0};  // 标题栏玻璃帧
chrome.corner_preference = WindowCornerPreference::Round;
window->set_chrome(chrome);
```

---

### macOS 风格（全窗口玻璃效果）

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 32.0f;
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
chrome.glass_frame_thickness = math::Thickness::uniform(-1.0f);  // 全窗口玻璃
chrome.corner_preference = WindowCornerPreference::Round;
window->set_chrome(chrome);
```

---

### 固定窗口（无调整大小）

```cpp
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 32.0f;
chrome.resize_border_thickness = {};  // 禁用调整大小
chrome.corner_preference = WindowCornerPreference::SystemDefault;
window->set_chrome(chrome);
```

---

## 平台差异

### Windows

| 字段 | 实现方式 |
|------|---------|
| `enabled` | 控制是否处理 `WM_NCHITTEST` |
| `caption_height` | `WM_NCHITTEST` 返回 `HTCAPTION` |
| `resize_border_thickness` | `WM_NCHITTEST` 返回 `HTLEFT/HTTOP/HTRIGHT/HTBOTTOM` |
| `corner_preference` | `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE)` |
| `glass_frame_thickness` | `DwmExtendFrameIntoClientArea()` |

**示例**：
```cpp
// Windows 实现
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCHITTEST) {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &pt);
        
        // 标题栏区域
        if (pt.y < caption_height_) {
            return HTCAPTION;
        }
        
        // 边框区域
        if (pt.x < resize_border_thickness_.left) return HTLEFT;
        if (pt.y < resize_border_thickness_.top) return HTTOP;
        // ...
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
```

---

### macOS

| 字段 | 实现方式 |
|------|---------|
| `enabled` | 控制是否使用 `NSWindowStyleMaskFullSizeContentView` |
| `caption_height` | `-[NSView mouseDownCanMoveWindow]` |
| `resize_border_thickness` | 自定义鼠标事件处理 |
| `corner_preference` | 忽略（macOS 窗口始终有圆角） |
| `glass_frame_thickness` | `NSVisualEffectView` 毛玻璃效果 |

**示例**：
```objc
// macOS 实现
@implementation CustomView

- (BOOL)mouseDownCanMoveWindow {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    return point.y >= self.bounds.size.height - captionHeight;
}

@end
```

---

### Linux

| 字段 | 实现方式 |
|------|---------|
| `enabled` | 控制是否移除窗口管理器装饰 |
| `caption_height` | 处理 `_NET_WM_MOVERESIZE` 消息 |
| `resize_border_thickness` | 处理 `_NET_WM_MOVERESIZE` 消息 |
| `corner_preference` | 依赖窗口管理器（GNOME/KDE） |
| `glass_frame_thickness` | 依赖合成器支持 |

**示例**：
```cpp
// Linux X11 实现
void handle_button_press(XButtonEvent* event) {
    if (event->y < caption_height_) {
        // 启动窗口移动
        XUngrabPointer(display, CurrentTime);
        XEvent xev;
        xev.xclient.type = ClientMessage;
        xev.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
        xev.xclient.data.l[2] = 8;  // _NET_WM_MOVERESIZE_MOVE
        XSendEvent(display, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &xev);
    }
}
```

---

## 最佳实践

### 1. 启用自定义 Chrome 前设置 frameless

```cpp
// ✅ 推荐：先创建无边框窗口
WindowDesc desc;
desc.frameless = true;
auto window = host->create_window(desc);

// 然后设置自定义 Chrome
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 40.0f;
window->set_chrome(chrome);

// ❌ 不推荐：有边框窗口设置自定义 Chrome（冲突）
WindowDesc desc;
desc.frameless = false;  // 系统标题栏
auto window = host->create_window(desc);
window->set_chrome(chrome);  // 冲突
```

---

### 2. 标题栏高度与字体大小匹配

```cpp
// ✅ 推荐：标题栏高度容纳字体 + 内边距
WindowChromeDesc chrome;
chrome.caption_height = 40.0f;  // 16px 字体 + 12px 上下内边距 + 控件

// ❌ 不推荐：标题栏过低，字体显示不全
chrome.caption_height = 16.0f;  // 无法容纳控件
```

---

### 3. 玻璃帧厚度与标题栏高度一致

```cpp
// ✅ 推荐：玻璃帧与标题栏高度一致
WindowChromeDesc chrome;
chrome.caption_height = 40.0f;
chrome.glass_frame_thickness = {0, 40, 0, 0};  // 顶部 40px

// ❌ 不推荐：玻璃帧超出标题栏（视觉不协调）
chrome.caption_height = 40.0f;
chrome.glass_frame_thickness = {0, 60, 0, 0};  // 顶部 60px
```

---

### 4. 禁用调整大小时清零边框厚度

```cpp
// ✅ 推荐：禁用调整大小
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.resize_border_thickness = {};  // 全零

// ❌ 不推荐：禁用调整大小但保留边框厚度（用户困惑）
chrome.resize_border_thickness = math::Thickness::uniform(4.0f);  // 仍有边框
```

---

### 5. 全窗口玻璃效果使用 uniform(-1.0f)

```cpp
// ✅ 推荐：全窗口玻璃效果
WindowChromeDesc chrome;
chrome.glass_frame_thickness = math::Thickness::uniform(-1.0f);

// ❌ 不推荐：使用极大值模拟全窗口（不可靠）
chrome.glass_frame_thickness = {10000, 10000, 10000, 10000};
```

---

## 常见陷阱

### 1. 有边框窗口设置自定义 Chrome

```cpp
// ❌ 问题：有边框窗口 + 自定义 Chrome
WindowDesc desc;
desc.frameless = false;  // 系统标题栏
auto window = host->create_window(desc);

WindowChromeDesc chrome;
chrome.enabled = true;
window->set_chrome(chrome);  // 与系统标题栏冲突

// ✅ 解决：使用无边框窗口
desc.frameless = true;
```

---

### 2. 忘记设置 enabled = true

```cpp
// ❌ 问题：配置 Chrome 但未启用
WindowChromeDesc chrome;
chrome.enabled = false;  // 默认值
chrome.caption_height = 40.0f;
window->set_chrome(chrome);  // 配置被忽略

// ✅ 解决：启用 Chrome
chrome.enabled = true;
```

---

### 3. 标题栏过高或过低

```cpp
// ❌ 问题：标题栏过低
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 10.0f;  // 无法容纳控件

// ❌ 问题：标题栏过高
chrome.caption_height = 100.0f;  // 浪费空间

// ✅ 解决：合理的标题栏高度
chrome.caption_height = 32.0f;  // 标准高度
chrome.caption_height = 40.0f;  // 较大高度
```

---

### 4. 玻璃帧在 Windows 7 上无效

```cpp
// ❌ 问题：Windows 7 DWM 关闭时玻璃帧无效
WindowChromeDesc chrome;
chrome.glass_frame_thickness = {0, 40, 0, 0};
// Windows 7 用户关闭 DWM 时无效果

// ✅ 解决：检测 DWM 状态
if (DwmIsCompositionEnabled()) {
    chrome.glass_frame_thickness = {0, 40, 0, 0};
} else {
    chrome.glass_frame_thickness = {};  // 禁用玻璃帧
}
```

---

### 5. 圆角在 Windows 10 上无效

```cpp
// ❌ 问题：Windows 10 不支持圆角偏好
WindowChromeDesc chrome;
chrome.corner_preference = WindowCornerPreference::Round;
// Windows 10 上无效果

// ✅ 解决：检测系统版本
if (is_windows_11_or_later()) {
    chrome.corner_preference = WindowCornerPreference::Round;
} else {
    chrome.corner_preference = WindowCornerPreference::SystemDefault;  // Windows 10 忽略
}
```

---

## 完整示例

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/WindowChromeDesc.h>

class CustomChromeWindow {
    IWindow* window_;
    
public:
    void create_vs_code_style_window(IApplicationHost* host) {
        // 创建无边框窗口
        WindowDesc desc;
        desc.title = "VS Code 风格";
        desc.size = {1280, 720};
        desc.frameless = true;
        window_ = host->create_window(desc).release();
        
        // 配置自定义 Chrome
        WindowChromeDesc chrome;
        chrome.enabled = true;
        chrome.caption_height = 40.0f;
        chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
        chrome.corner_preference = WindowCornerPreference::Round;
        window_->set_chrome(chrome);
        
        window_->show();
    }
    
    void create_chrome_style_window(IApplicationHost* host) {
        // 创建无边框窗口
        WindowDesc desc;
        desc.title = "Chrome 风格";
        desc.size = {1280, 720};
        desc.frameless = true;
        window_ = host->create_window(desc).release();
        
        // 配置自定义 Chrome + 玻璃帧
        WindowChromeDesc chrome;
        chrome.enabled = true;
        chrome.caption_height = 48.0f;
        chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
        chrome.glass_frame_thickness = {0, 48, 0, 0};  // 顶部玻璃帧
        chrome.corner_preference = WindowCornerPreference::Round;
        window_->set_chrome(chrome);
        
        window_->show();
    }
    
    void create_macos_style_window(IApplicationHost* host) {
        // 创建无边框窗口
        WindowDesc desc;
        desc.title = "macOS 风格";
        desc.size = {1280, 720};
        desc.frameless = true;
        window_ = host->create_window(desc).release();
        
        // 配置全窗口玻璃效果
        WindowChromeDesc chrome;
        chrome.enabled = true;
        chrome.caption_height = 32.0f;
        chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
        chrome.glass_frame_thickness = math::Thickness::uniform(-1.0f);  // 全窗口玻璃
        chrome.corner_preference = WindowCornerPreference::Round;
        window_->set_chrome(chrome);
        
        window_->show();
    }
    
    void toggle_chrome() {
        // 切换自定义 Chrome 开关
        WindowChromeDesc chrome;
        chrome.enabled = !chrome_enabled_;
        chrome.caption_height = 40.0f;
        chrome.resize_border_thickness = math::Thickness::uniform(4.0f);
        chrome.corner_preference = WindowCornerPreference::Round;
        window_->set_chrome(chrome);
        
        chrome_enabled_ = !chrome_enabled_;
    }
    
private:
    bool chrome_enabled_ = true;
};
```

---

## 性能考虑

### 1. 玻璃帧性能影响

```cpp
// ⚠️ 玻璃帧需要额外的合成开销
WindowChromeDesc chrome;
chrome.glass_frame_thickness = math::Thickness::uniform(-1.0f);  // 全窗口玻璃
// 每帧需要重新合成，可能降低帧率
```

---

### 2. 频繁更新 Chrome 配置

```cpp
// ❌ 不推荐：频繁更新 Chrome
for (int i = 0; i < 100; ++i) {
    WindowChromeDesc chrome;
    chrome.enabled = true;
    chrome.caption_height = 32.0f + i;
    window->set_chrome(chrome);  // 每次触发 DWM 更新
}

// ✅ 推荐：一次性更新
WindowChromeDesc chrome;
chrome.enabled = true;
chrome.caption_height = 132.0f;
window->set_chrome(chrome);
```

---

## 总结

`WindowChromeDesc` 是 `mine.platform.abi` 模块的自定义窗口标题栏配置描述符，具备：

1. **5 个字段**：enabled、caption_height、resize_border_thickness、corner_preference、glass_frame_thickness
2. **WPF WindowChrome 设计**：优化为单次提交模式，避免零散更新
3. **逻辑像素**：所有尺寸使用逻辑像素（96 DPI 基准）
4. **平台集成**：调用 DWM（Windows）/ Compositor（macOS/Linux）API

**使用建议**：
- 启用自定义 Chrome 前设置 `frameless = true`
- 标题栏高度与字体大小匹配（32-48px）
- 玻璃帧厚度与标题栏高度一致
- 禁用调整大小时清零 `resize_border_thickness`
- 全窗口玻璃效果使用 `uniform(-1.0f)`
- Windows 11+ 外的系统忽略 `corner_preference`
- 玻璃帧需要 DWM 合成（Windows 8+）
