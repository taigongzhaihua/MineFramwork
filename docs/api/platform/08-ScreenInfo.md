# ScreenInfo 详细接口文档

## 概述

`ScreenInfo` 是 `mine.platform.abi` 模块的**显示器信息描述结构体**，表示单个显示器的静态信息快照。

**核心特性：**
- **5 个字段**：bounds、work_area、dpi、scale、is_primary
- **逻辑像素**：所有坐标和尺寸使用逻辑像素（已按 DPI 缩放）
- **POD 类型**：简单聚合类型，ABI 稳定
- **快照数据**：静态信息，不自动更新

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/ScreenInfo.h
```

---

## 类型定义

```cpp
namespace mine::platform {

struct ScreenInfo {
    math::Rect bounds{};       // 显示器在虚拟桌面上的逻辑像素矩形（含任务栏）
    math::Rect work_area{};    // 去掉任务栏/Dock 后的可用工作区（逻辑像素）
    float      dpi{96.0f};     // 当前 DPI（Windows 默认 96 = 1x 缩放）
    float      scale{1.0f};    // 物理像素 / 逻辑像素缩放比（Retina = 2.0 等）
    bool       is_primary{};   // 是否为主显示器
};

}
```

---

## 成员字段

### bounds

```cpp
math::Rect bounds{};
```

**描述**：显示器在虚拟桌面上的逻辑像素矩形（含任务栏）。

**特征**：
- **逻辑像素**：96 DPI 基准，自动 DPI 缩放
- **虚拟桌面坐标**：多显示器环境下的全局坐标
- **含任务栏**：矩形包含任务栏/Dock 区域

**平台行为**：
- **Windows**：通过 `GetMonitorInfo()` 获取 `rcMonitor`
- **macOS**：`[NSScreen frame]`
- **Linux**：`_NET_WORKAREA` / `RandR` 输出矩形

**示例**：
```cpp
ScreenInfo screen = screen_manager->get_primary_screen();
math::Rect bounds = screen.bounds;
log("显示器矩形: x={}, y={}, w={}, h={}", 
    bounds.x, bounds.y, bounds.width, bounds.height);
```

---

### work_area

```cpp
math::Rect work_area{};
```

**描述**：去掉任务栏/Dock 后的可用工作区（逻辑像素）。

**特征**：
- **逻辑像素**：96 DPI 基准，自动 DPI 缩放
- **排除任务栏**：矩形不包含任务栏/Dock 区域
- **窗口最大化区域**：窗口最大化后的实际可用区域

**平台行为**：
- **Windows**：通过 `GetMonitorInfo()` 获取 `rcWork`
- **macOS**：`[NSScreen visibleFrame]`
- **Linux**：`_NET_WORKAREA` 属性

**示例**：
```cpp
ScreenInfo screen = screen_manager->get_primary_screen();
math::Rect work_area = screen.work_area;
log("工作区: x={}, y={}, w={}, h={}", 
    work_area.x, work_area.y, work_area.width, work_area.height);

// 窗口居中显示在工作区
math::Size window_size = {800, 600};
math::Point position = {
    work_area.x + (work_area.width - window_size.width) / 2,
    work_area.y + (work_area.height - window_size.height) / 2
};
window->set_position(position);
```

---

### dpi

```cpp
float dpi{96.0f};
```

**描述**：当前 DPI（每英寸点数）。

**默认值**：96.0f（Windows 标准 DPI，100% 缩放）

**特征**：
- **Windows**：96 = 100% 缩放，120 = 125% 缩放，144 = 150% 缩放
- **macOS**：始终 72（历史原因），使用 `scale` 表示缩放
- **Linux**：通常 96

**缩放比计算**：
```cpp
float scale_factor = dpi / 96.0f;
```

**示例**：
```cpp
ScreenInfo screen = screen_manager->get_primary_screen();
float dpi = screen.dpi;
log("DPI: {}, 缩放: {}%", dpi, (dpi / 96.0f) * 100);

// 加载合适分辨率的图标
if (dpi >= 144) {
    load_icon("icon@2x.png");  // 150% 缩放
} else if (dpi >= 120) {
    load_icon("icon@1.5x.png");  // 125% 缩放
} else {
    load_icon("icon.png");  // 100% 缩放
}
```

---

### scale

```cpp
float scale{1.0f};
```

**描述**：物理像素 / 逻辑像素缩放比。

**默认值**：1.0f（无缩放）

**特征**：
- **macOS Retina**：2.0（2x 缩放）
- **Windows 高 DPI**：`dpi / 96.0f`（如 150% = 1.5）
- **物理像素计算**：`physical_pixels = logical_pixels * scale`

**示例**：
```cpp
ScreenInfo screen = screen_manager->get_primary_screen();
float scale = screen.scale;
log("缩放比: {}x", scale);

// 计算物理像素尺寸
math::Size logical_size = {800, 600};
math::Size physical_size = {
    logical_size.width * scale,
    logical_size.height * scale
};
log("逻辑像素: {}x{}, 物理像素: {}x{}", 
    logical_size.width, logical_size.height,
    physical_size.width, physical_size.height);
```

---

### is_primary

```cpp
bool is_primary{};
```

**描述**：是否为主显示器。

**默认值**：`false`

**特征**：
- **主显示器**：系统启动时的默认显示器，通常任务栏在此显示器上
- **虚拟桌面原点**：主显示器的左上角通常为 (0, 0)

**平台行为**：
- **Windows**：通过 `MONITORINFOEX::dwFlags & MONITORINFOF_PRIMARY` 判断
- **macOS**：`[NSScreen mainScreen]`
- **Linux**：通过 `_NET_PRIMARY_MONITOR` / `RandR` 主输出判断

**示例**：
```cpp
ScreenInfo screen = screen_manager->get_primary_screen();
if (screen.is_primary) {
    log("主显示器");
} else {
    log("副显示器");
}

// 在主显示器居中显示窗口
auto screens = screen_manager->get_all_screens();
for (const auto& screen : screens) {
    if (screen.is_primary) {
        center_window_on_screen(window, screen);
        break;
    }
}
```

---

## 使用场景

### 1. 获取主显示器信息

```cpp
IScreenManager* screen_manager = host->screen_manager();
ScreenInfo primary = screen_manager->get_primary_screen();

log("主显示器:");
log("  尺寸: {}x{}", primary.bounds.width, primary.bounds.height);
log("  工作区: {}x{}", primary.work_area.width, primary.work_area.height);
log("  DPI: {}", primary.dpi);
log("  缩放: {}x", primary.scale);
```

---

### 2. 窗口居中显示在工作区

```cpp
void center_window_on_primary_screen(IWindow* window, IScreenManager* screen_manager) {
    ScreenInfo screen = screen_manager->get_primary_screen();
    math::Rect work_area = screen.work_area;
    math::Size window_size = window->size();
    
    math::Point center_position = {
        work_area.x + (work_area.width - window_size.width) / 2,
        work_area.y + (work_area.height - window_size.height) / 2
    };
    
    window->set_position(center_position);
}
```

---

### 3. 多显示器布局

```cpp
void show_all_screens(IScreenManager* screen_manager) {
    auto screens = screen_manager->get_all_screens();
    
    for (size_t i = 0; i < screens.size(); ++i) {
        const auto& screen = screens[i];
        log("显示器 {}: {}x{}, DPI={}, 缩放={}x, 主显示器={}", 
            i,
            screen.bounds.width, screen.bounds.height,
            screen.dpi, screen.scale,
            screen.is_primary ? "是" : "否");
    }
}
```

---

### 4. DPI 感知资源加载

```cpp
void load_high_dpi_assets(IScreenManager* screen_manager) {
    ScreenInfo screen = screen_manager->get_primary_screen();
    float scale = screen.scale;
    
    if (scale >= 2.0f) {
        load_icon("icon@3x.png");  // Retina + 150% 缩放
    } else if (scale >= 1.5f) {
        load_icon("icon@2x.png");  // Retina 或 150% 缩放
    } else {
        load_icon("icon.png");     // 100% 缩放
    }
}
```

---

### 5. 跨显示器拖动窗口

```cpp
void handle_window_moved(IWindow* window, IScreenManager* screen_manager) {
    math::Point position = window->position();
    auto screens = screen_manager->get_all_screens();
    
    // 查找窗口所在的显示器
    ScreenInfo* current_screen = nullptr;
    for (auto& screen : screens) {
        if (screen.bounds.contains(position)) {
            current_screen = &screen;
            break;
        }
    }
    
    if (current_screen) {
        log("窗口位于显示器: DPI={}, 缩放={}x", 
            current_screen->dpi, current_screen->scale);
        
        // 根据显示器 DPI 调整窗口内容
        adjust_for_dpi(current_screen->dpi);
    }
}
```

---

### 6. 全屏窗口

```cpp
void enter_fullscreen(IWindow* window, IScreenManager* screen_manager) {
    ScreenInfo screen = screen_manager->get_primary_screen();
    
    // 设置窗口尺寸为显示器完整尺寸
    window->set_position({screen.bounds.x, screen.bounds.y});
    window->set_size({screen.bounds.width, screen.bounds.height});
    
    // 无边框全屏
    window->set_state(WindowState::Maximized);
}
```

---

## 平台差异

### Windows

| 字段 | API | 说明 |
|------|-----|------|
| `bounds` | `GetMonitorInfo()` `rcMonitor` | 完整显示器矩形 |
| `work_area` | `GetMonitorInfo()` `rcWork` | 排除任务栏区域 |
| `dpi` | `GetDpiForMonitor()` | Per-monitor DPI |
| `scale` | `dpi / 96.0f` | 缩放比 |
| `is_primary` | `MONITORINFOF_PRIMARY` | 主显示器标志 |

**示例**：
```cpp
// Windows 实现
HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
MONITORINFOEX info{};
info.cbSize = sizeof(info);
GetMonitorInfo(hMonitor, &info);

ScreenInfo screen;
screen.bounds = to_rect(info.rcMonitor);
screen.work_area = to_rect(info.rcWork);
screen.is_primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

UINT dpi_x, dpi_y;
GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
screen.dpi = static_cast<float>(dpi_x);
screen.scale = screen.dpi / 96.0f;
```

---

### macOS

| 字段 | API | 说明 |
|------|-----|------|
| `bounds` | `[NSScreen frame]` | 完整显示器矩形 |
| `work_area` | `[NSScreen visibleFrame]` | 排除 Dock/菜单栏区域 |
| `dpi` | 72.0f | 历史固定值 |
| `scale` | `[NSScreen backingScaleFactor]` | Retina 缩放比 |
| `is_primary` | `[NSScreen mainScreen]` | 主显示器 |

**示例**：
```objc
// macOS 实现
NSScreen* nsscreen = [NSScreen mainScreen];

ScreenInfo screen;
screen.bounds = to_rect(nsscreen.frame);
screen.work_area = to_rect(nsscreen.visibleFrame);
screen.dpi = 72.0f;  // macOS 固定值
screen.scale = nsscreen.backingScaleFactor;  // Retina = 2.0
screen.is_primary = (nsscreen == [NSScreen mainScreen]);
```

---

### Linux (X11/RandR)

| 字段 | API | 说明 |
|------|-----|------|
| `bounds` | `XRRGetScreenResources()` | 输出矩形 |
| `work_area` | `_NET_WORKAREA` | EWMH 工作区 |
| `dpi` | `XGetDefault()` / 96.0f | 通常 96 |
| `scale` | `GDK_SCALE` / 1.0f | HiDPI 缩放比 |
| `is_primary` | `_NET_PRIMARY_MONITOR` | 主输出 |

**示例**：
```cpp
// Linux X11 实现
Display* display = XOpenDisplay(nullptr);
int screen_num = DefaultScreen(display);
Screen* screen = ScreenOfDisplay(display, screen_num);

ScreenInfo info;
info.bounds.width = screen->width;
info.bounds.height = screen->height;
info.dpi = 96.0f;  // 默认 DPI
info.scale = 1.0f;  // 默认无缩放
info.is_primary = true;  // 单显示器默认主显示器

// 获取工作区（排除面板）
Atom workarea_atom = XInternAtom(display, "_NET_WORKAREA", False);
unsigned long* workarea = nullptr;
if (get_property(display, root, workarea_atom, &workarea)) {
    info.work_area = {workarea[0], workarea[1], workarea[2], workarea[3]};
}
```

---

## 最佳实践

### 1. 使用 work_area 而非 bounds 居中窗口

```cpp
// ✅ 推荐：使用 work_area 居中，避免被任务栏遮挡
ScreenInfo screen = screen_manager->get_primary_screen();
math::Rect work_area = screen.work_area;
math::Point center = {
    work_area.x + (work_area.width - window_size.width) / 2,
    work_area.y + (work_area.height - window_size.height) / 2
};

// ❌ 不推荐：使用 bounds 居中，可能被任务栏遮挡
math::Rect bounds = screen.bounds;
math::Point center = {
    bounds.x + (bounds.width - window_size.width) / 2,
    bounds.y + (bounds.height - window_size.height) / 2
};
```

---

### 2. 根据 scale 加载合适分辨率资源

```cpp
// ✅ 推荐：根据 scale 加载资源
ScreenInfo screen = screen_manager->get_primary_screen();
if (screen.scale >= 2.0f) {
    load_icon("icon@2x.png");
} else {
    load_icon("icon.png");
}

// ❌ 不推荐：忽略 scale，图标模糊
load_icon("icon.png");  // Retina 显示器上模糊
```

---

### 3. 多显示器环境查找窗口所在显示器

```cpp
// ✅ 推荐：根据窗口位置查找显示器
math::Point position = window->position();
auto screens = screen_manager->get_all_screens();
for (const auto& screen : screens) {
    if (screen.bounds.contains(position)) {
        // 找到窗口所在显示器
        adjust_for_dpi(screen.dpi);
        break;
    }
}

// ❌ 不推荐：假设窗口在主显示器
ScreenInfo screen = screen_manager->get_primary_screen();
adjust_for_dpi(screen.dpi);  // 窗口可能在副显示器
```

---

### 4. DPI 变化时重新查询 ScreenInfo

```cpp
// ✅ 推荐：DPI 变化时重新查询
void on_event(const WindowEvent& event) {
    if (event.kind == WindowEventKind::DpiChanged) {
        ScreenInfo screen = screen_manager->get_screen_at(window->position());
        reload_resources(screen.dpi);
    }
}

// ❌ 不推荐：缓存 ScreenInfo 不更新
ScreenInfo cached_screen = screen_manager->get_primary_screen();
// DPI 变化后 cached_screen 过时
```

---

## 常见陷阱

### 1. 使用 bounds 而非 work_area 定位窗口

```cpp
// ❌ 问题：使用 bounds 居中，窗口被任务栏遮挡
ScreenInfo screen = screen_manager->get_primary_screen();
math::Point center = {
    screen.bounds.x + (screen.bounds.width - window_size.width) / 2,
    screen.bounds.y + (screen.bounds.height - window_size.height) / 2
};
window->set_position(center);  // 可能被任务栏遮挡

// ✅ 解决：使用 work_area
math::Point center = {
    screen.work_area.x + (screen.work_area.width - window_size.width) / 2,
    screen.work_area.y + (screen.work_area.height - window_size.height) / 2
};
```

---

### 2. 假设所有显示器 DPI 相同

```cpp
// ❌ 问题：假设所有显示器 DPI 相同
ScreenInfo primary = screen_manager->get_primary_screen();
float dpi = primary.dpi;  // 假设所有显示器 DPI 相同
load_resources(dpi);

// ✅ 解决：根据窗口所在显示器查询 DPI
ScreenInfo screen = screen_manager->get_screen_at(window->position());
load_resources(screen.dpi);
```

---

### 3. 混淆 dpi 和 scale

```cpp
// ❌ 问题：Windows 使用 dpi，macOS 使用 scale
#ifdef _WIN32
    float factor = screen.dpi / 96.0f;
#elif defined(__APPLE__)
    float factor = screen.dpi / 96.0f;  // 错误：macOS dpi 始终 72
#endif

// ✅ 解决：统一使用 scale
float factor = screen.scale;  // 跨平台统一
```

---

### 4. 缓存 ScreenInfo 不更新

```cpp
// ❌ 问题：缓存 ScreenInfo，显示器配置变化后过时
class Application {
    ScreenInfo cached_screen_ = screen_manager->get_primary_screen();
    
    void render() {
        use_dpi(cached_screen_.dpi);  // 可能过时
    }
};

// ✅ 解决：每次使用时查询
void render() {
    ScreenInfo screen = screen_manager->get_primary_screen();
    use_dpi(screen.dpi);
}
```

---

## 完整示例

```cpp
#include <mine/platform/IScreenManager.h>
#include <mine/platform/ScreenInfo.h>
#include <mine/platform/IWindow.h>

class Application {
    IScreenManager* screen_manager_;
    IWindow* window_;
    
public:
    void show_all_screens() {
        auto screens = screen_manager_->get_all_screens();
        
        log("检测到 {} 个显示器:", screens.size());
        for (size_t i = 0; i < screens.size(); ++i) {
            const auto& screen = screens[i];
            log("显示器 {}:", i);
            log("  位置: ({}, {})", screen.bounds.x, screen.bounds.y);
            log("  尺寸: {}x{}", screen.bounds.width, screen.bounds.height);
            log("  工作区: {}x{}", screen.work_area.width, screen.work_area.height);
            log("  DPI: {}", screen.dpi);
            log("  缩放: {}x", screen.scale);
            log("  主显示器: {}", screen.is_primary ? "是" : "否");
        }
    }
    
    void center_window_on_primary_screen() {
        ScreenInfo screen = screen_manager_->get_primary_screen();
        math::Rect work_area = screen.work_area;
        math::Size window_size = window_->size();
        
        math::Point center = {
            work_area.x + (work_area.width - window_size.width) / 2,
            work_area.y + (work_area.height - window_size.height) / 2
        };
        
        window_->set_position(center);
    }
    
    void load_high_dpi_assets() {
        ScreenInfo screen = screen_manager_->get_primary_screen();
        float scale = screen.scale;
        
        if (scale >= 2.0f) {
            resource_manager_->load("assets@2x");
        } else if (scale >= 1.5f) {
            resource_manager_->load("assets@1.5x");
        } else {
            resource_manager_->load("assets");
        }
    }
    
    void handle_window_moved() {
        math::Point position = window_->position();
        auto screens = screen_manager_->get_all_screens();
        
        for (const auto& screen : screens) {
            if (screen.bounds.contains(position)) {
                log("窗口位于显示器: DPI={}, 缩放={}x", 
                    screen.dpi, screen.scale);
                adjust_for_dpi(screen.dpi);
                break;
            }
        }
    }
};
```

---

## 总结

`ScreenInfo` 是 `mine.platform.abi` 模块的显示器信息描述结构体，具备：

1. **5 个字段**：bounds、work_area、dpi、scale、is_primary
2. **逻辑像素**：所有坐标和尺寸使用逻辑像素（已按 DPI 缩放）
3. **POD 类型**：简单聚合类型，ABI 稳定
4. **快照数据**：静态信息，不自动更新

**使用建议**：
- 使用 `work_area` 而非 `bounds` 居中窗口（避免被任务栏遮挡）
- 根据 `scale` 加载合适分辨率资源
- 多显示器环境根据窗口位置查找所在显示器
- DPI 变化时重新查询 `ScreenInfo`
- Windows 使用 `dpi / 96.0f` 计算缩放，macOS 使用 `scale`
- 不要缓存 `ScreenInfo`，显示器配置变化后过时
