# WindowCornerPreference 详细接口文档

## 概述

`WindowCornerPreference` 是 `mine.platform.abi` 模块的**窗口圆角偏好枚举**，控制窗口边框圆角样式，主要用于 Windows 11+ 系统。

**核心特性：**
- **四种偏好**：SystemDefault（系统默认）、DoNotRound（直角）、Round（大圆角）、RoundSmall（小圆角）
- **平台相关**：仅 Windows 11+ 生效，其他平台忽略
- **DWM 集成**：调用 `DwmSetWindowAttribute()` 设置 `DWMWA_WINDOW_CORNER_PREFERENCE`

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/WindowCornerPreference.h
```

---

## 枚举定义

```cpp
namespace mine::platform {

enum class WindowCornerPreference : int {
    SystemDefault = 0,  // 系统默认圆角（跟随系统主题）
    DoNotRound = 1,     // 直角，不圆角化
    Round = 2,          // 大圆角（8-10px）
    RoundSmall = 3,     // 小圆角（4-6px）
};

}
```

---

## 枚举值

### SystemDefault

```cpp
WindowCornerPreference::SystemDefault = 0
```

**描述**：系统默认圆角样式，跟随 Windows 11 主题设置。

**特征**：
- **系统控制**：由 Windows 11 主题决定圆角大小
- **默认行为**：大多数应用程序的默认选择
- **主题响应**：用户切换系统主题时自动更新

**Windows 11 行为**：
- **默认主题**：通常为 8px 圆角
- **高对比度主题**：可能为直角

**其他平台**：
- **Windows 10-**：无效果（不支持圆角）
- **macOS**：窗口自带圆角，无法控制
- **Linux**：由窗口管理器决定

**用途**：
- 大多数应用程序的默认选择
- 遵循系统 UI 规范

---

### DoNotRound

```cpp
WindowCornerPreference::DoNotRound = 1
```

**描述**：直角边框，不应用圆角化。

**特征**：
- **强制直角**：即使系统主题为圆角也不应用
- **精确像素**：适合需要精确像素对齐的 UI
- **传统外观**：Windows 10 风格的直角窗口

**Windows 11 映射**：
- **DWM 属性**：`DWMWCP_DONOTROUND`
- **视觉效果**：窗口边框完全直角

**用途**：
- 工程/设计工具（需要精确像素对齐）
- 终端/控制台窗口（传统外观）
- 游戏窗口（避免圆角裁剪）

**示例**：
```cpp
WindowDesc desc;
desc.title = "终端";
desc.corner_preference = WindowCornerPreference::DoNotRound;
auto terminal = host->create_window(desc);
```

---

### Round

```cpp
WindowCornerPreference::Round = 2
```

**描述**：大圆角，提供柔和的视觉效果。

**特征**：
- **8-10px 圆角**：明显的圆角效果
- **现代风格**：符合 Windows 11 设计语言
- **柔和过渡**：视觉上更柔和

**Windows 11 映射**：
- **DWM 属性**：`DWMWCP_ROUND`
- **圆角半径**：约 8-10px（根据 DPI 缩放）

**用途**：
- 现代 UI 应用程序
- 内容创作工具
- 媒体播放器

**示例**：
```cpp
WindowDesc desc;
desc.title = "媒体播放器";
desc.corner_preference = WindowCornerPreference::Round;
auto player = host->create_window(desc);
```

---

### RoundSmall

```cpp
WindowCornerPreference::RoundSmall = 3
```

**描述**：小圆角，微妙的视觉优化。

**特征**：
- **4-6px 圆角**：较小的圆角半径
- **微妙效果**：既有圆角又不过分柔和
- **适中风格**：介于直角和大圆角之间

**Windows 11 映射**：
- **DWM 属性**：`DWMWCP_ROUNDSMALL`
- **圆角半径**：约 4-6px（根据 DPI 缩放）

**用途**：
- 工具窗口
- 对话框
- 需要微妙圆角的应用

**示例**：
```cpp
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.title = "工具面板";
desc.corner_preference = WindowCornerPreference::RoundSmall;
auto tool = host->create_window(desc);
```

---

## 使用场景

### 1. 默认圆角（大多数应用）

```cpp
// 使用系统默认圆角
WindowDesc desc;
desc.title = "我的应用";
desc.corner_preference = WindowCornerPreference::SystemDefault;  // 或省略（默认值）
auto window = host->create_window(desc);
```

---

### 2. 强制直角（终端/IDE）

```cpp
// 终端窗口，需要精确像素对齐
WindowDesc desc;
desc.title = "终端";
desc.corner_preference = WindowCornerPreference::DoNotRound;
auto terminal = host->create_window(desc);
```

---

### 3. 大圆角（媒体应用）

```cpp
// 媒体播放器，现代风格
WindowDesc desc;
desc.title = "视频播放器";
desc.corner_preference = WindowCornerPreference::Round;
auto player = host->create_window(desc);
```

---

### 4. 小圆角（工具窗口）

```cpp
// 工具窗口，微妙圆角
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.title = "颜色面板";
desc.corner_preference = WindowCornerPreference::RoundSmall;
auto tool = host->create_window(desc);
```

---

## 平台差异

### Windows 11+

| WindowCornerPreference | DWM 属性 | 圆角半径 | 说明 |
|------------------------|---------|---------|------|
| `SystemDefault` | `DWMWCP_DEFAULT` | ~8px | 跟随系统主题 |
| `DoNotRound` | `DWMWCP_DONOTROUND` | 0px | 强制直角 |
| `Round` | `DWMWCP_ROUND` | ~8-10px | 大圆角 |
| `RoundSmall` | `DWMWCP_ROUNDSMALL` | ~4-6px | 小圆角 |

**实现**：
```cpp
// 内部调用 DwmSetWindowAttribute()
DWORD preference = DWMWCP_ROUND;
DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, 
                      &preference, sizeof(preference));
```

---

### Windows 10 及更早版本

**行为**：圆角偏好无效果，所有窗口均为直角。

**原因**：Windows 10 不支持 `DWMWA_WINDOW_CORNER_PREFERENCE` 属性。

---

### macOS

**行为**：忽略圆角偏好，窗口自带圆角。

**特征**：
- macOS 窗口始终有 5-10px 圆角（系统强制）
- 无法通过 API 禁用或调整圆角

---

### Linux

**行为**：圆角由窗口管理器（如 GNOME、KDE）决定。

**特征**：
- 某些窗口管理器支持圆角（如 Mutter、KWin）
- 无统一标准 API 控制圆角

---

## 最佳实践

### 1. 大多数应用使用默认值

```cpp
// ✅ 推荐：使用系统默认，遵循系统 UI 规范
WindowDesc desc;
desc.corner_preference = WindowCornerPreference::SystemDefault;

// 或省略（默认即为 SystemDefault）
WindowDesc desc;  // corner_preference 自动为 SystemDefault
```

---

### 2. 终端/IDE 使用直角

```cpp
// ✅ 推荐：终端、代码编辑器使用直角，精确像素对齐
WindowDesc desc;
desc.title = "VS Code";
desc.corner_preference = WindowCornerPreference::DoNotRound;
```

---

### 3. 媒体应用使用大圆角

```cpp
// ✅ 推荐：媒体播放器、相册使用大圆角，现代风格
WindowDesc desc;
desc.title = "照片";
desc.corner_preference = WindowCornerPreference::Round;
```

---

### 4. 工具窗口使用小圆角

```cpp
// ✅ 推荐：工具面板使用小圆角，微妙优化
WindowDesc desc;
desc.kind = WindowKind::Tool;
desc.corner_preference = WindowCornerPreference::RoundSmall;
```

---

## 常见陷阱

### 1. Windows 10 上期望圆角生效

```cpp
// ❌ 问题：Windows 10 上设置圆角无效果
WindowDesc desc;
desc.corner_preference = WindowCornerPreference::Round;
auto window = host->create_window(desc);  // Windows 10 仍为直角

// ✅ 解决：检测系统版本
if (is_windows_11_or_later()) {
    desc.corner_preference = WindowCornerPreference::Round;
} else {
    // Windows 10 不支持，使用默认
}
```

---

### 2. macOS 上期望禁用圆角

```cpp
// ❌ 问题：macOS 上设置 DoNotRound 无效
WindowDesc desc;
desc.corner_preference = WindowCornerPreference::DoNotRound;
auto window = host->create_window(desc);  // macOS 仍有圆角

// ✅ 解决：macOS 圆角无法禁用，接受系统默认
#ifdef __APPLE__
    // macOS 始终有圆角
#endif
```

---

### 3. 自定义窗口边框与圆角冲突

```cpp
// ❌ 问题：自定义边框渲染与系统圆角不匹配
WindowDesc desc;
desc.frameless = true;  // 自定义边框
desc.corner_preference = WindowCornerPreference::Round;  // 系统圆角
// 自定义内容可能被圆角裁剪

// ✅ 解决：frameless 窗口自行处理圆角渲染
WindowDesc desc;
desc.frameless = true;
// 在自定义渲染中绘制圆角边框
```

---

## 完整示例

```cpp
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/WindowCornerPreference.h>

class Application {
    IApplicationHost* host_;
    
public:
    void create_main_window() {
        // 主窗口：使用系统默认圆角
        WindowDesc main_desc;
        main_desc.title = "主窗口";
        main_desc.size = {1024, 768};
        main_desc.corner_preference = WindowCornerPreference::SystemDefault;
        auto main_window = host_->create_window(main_desc);
        main_window->show();
    }
    
    void create_terminal_window() {
        // 终端窗口：强制直角
        WindowDesc terminal_desc;
        terminal_desc.title = "终端";
        terminal_desc.size = {800, 600};
        terminal_desc.corner_preference = WindowCornerPreference::DoNotRound;
        auto terminal = host_->create_window(terminal_desc);
        terminal->show();
    }
    
    void create_media_player() {
        // 媒体播放器：大圆角
        WindowDesc player_desc;
        player_desc.title = "播放器";
        player_desc.size = {1280, 720};
        player_desc.corner_preference = WindowCornerPreference::Round;
        auto player = host_->create_window(player_desc);
        player->show();
    }
    
    void create_tool_panel() {
        // 工具面板：小圆角
        WindowDesc tool_desc;
        tool_desc.kind = WindowKind::Tool;
        tool_desc.title = "工具";
        tool_desc.size = {250, 400};
        tool_desc.corner_preference = WindowCornerPreference::RoundSmall;
        auto tool = host_->create_window(tool_desc);
        tool->show();
    }
};
```

---

## 性能考虑

### 1. 圆角对性能影响微乎其微

```cpp
// ✅ 圆角由 DWM 硬件加速处理，无性能损失
desc.corner_preference = WindowCornerPreference::Round;
```

---

### 2. 自定义边框需要手动裁剪

```cpp
// ⚠️ 自定义边框（frameless）需要在渲染时裁剪圆角
WindowDesc desc;
desc.frameless = true;
// 在渲染管线中应用圆角裁剪（CPU 开销）
```

---

## 总结

`WindowCornerPreference` 是 `mine.platform.abi` 模块的窗口圆角偏好枚举，具备：

1. **四种偏好**：SystemDefault / DoNotRound / Round / RoundSmall
2. **平台相关**：仅 Windows 11+ 生效，其他平台忽略
3. **创建时指定**：通过 `WindowDesc::corner_preference` 设置
4. **DWM 集成**：调用 `DwmSetWindowAttribute()` 设置圆角样式

**使用建议**：
- 大多数应用使用 `SystemDefault`（默认值）
- 终端/IDE 使用 `DoNotRound`（精确像素对齐）
- 媒体应用使用 `Round`（现代风格）
- 工具窗口使用 `RoundSmall`（微妙优化）
- Windows 11+ 外的平台会忽略此设置
- 自定义边框（frameless）需自行处理圆角渲染
