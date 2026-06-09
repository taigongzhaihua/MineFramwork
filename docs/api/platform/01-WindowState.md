# WindowState 详细接口文档

## 概述

`WindowState` 是 `mine.platform.abi` 模块的**窗口显示状态枚举**，表示窗口的三种典型显示状态：正常、最小化、最大化。

**核心特性：**
- **三态模型**：Normal（正常）、Minimized（最小化）、Maximized（最大化）
- **平台无关**：抽象系统窗口状态，支持 Windows/macOS/Linux
- **轻量级**：`sizeof(WindowState) == sizeof(int)`

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/WindowState.h
```

---

## 枚举定义

```cpp
namespace mine::platform {

enum class WindowState : int {
    Normal = 0,     // 正常大小的窗口
    Minimized = 1,  // 最小化到任务栏
    Maximized = 2,  // 最大化占满工作区
};

}
```

---

## 枚举值

### Normal

```cpp
WindowState::Normal = 0
```

**描述**：正常大小的窗口。

**特征**：
- 窗口显示在屏幕上，可自由拖动和调整大小
- 窗口尺寸由用户或应用程序设定
- 窗口可能居中或位于上次关闭前的位置

**平台映射**：
- **Windows**：`SW_RESTORE` / `SW_NORMAL`
- **macOS**：窗口既未 miniaturize 也未 zoom
- **Linux (X11/Wayland)**：窗口既未最小化也未最大化

---

### Minimized

```cpp
WindowState::Minimized = 1
```

**描述**：最小化到任务栏。

**特征**：
- 窗口内容不可见，仅在任务栏保留图标
- 窗口仍然存在，未被销毁
- 点击任务栏图标或调用 `set_state(Normal)` 可恢复

**平台映射**：
- **Windows**：`SW_MINIMIZE`，窗口折叠到任务栏
- **macOS**：`miniaturize`，窗口缩小到 Dock
- **Linux (X11)**：`_NET_WM_STATE_HIDDEN`，窗口图标化

**用途**：
- 节省屏幕空间
- 保持应用程序运行但暂时隐藏

---

### Maximized

```cpp
WindowState::Maximized = 2
```

**描述**：最大化，占满当前工作区。

**特征**：
- 窗口填充整个屏幕（不包含任务栏/Dock 区域）
- 窗口边框可能被系统修改（如 Windows 去除边框阴影）
- 用户仍可通过双击标题栏或调用 `set_state(Normal)` 恢复

**平台映射**：
- **Windows**：`SW_MAXIMIZE`，填充工作区
- **macOS**：`zoom`，全屏（非 Full Screen Mode）
- **Linux (X11)**：`_NET_WM_STATE_MAXIMIZED_HORZ` + `_NET_WM_STATE_MAXIMIZED_VERT`

**与全屏模式的区别**：
- 最大化保留任务栏和窗口边框
- 全屏模式（Full Screen）完全隐藏任务栏和标题栏，占据整个屏幕

---

## 使用场景

### 1. 程序化控制窗口状态

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/WindowState.h>

void minimize_window(IWindow* window) {
    window->set_state(WindowState::Minimized);
}

void maximize_window(IWindow* window) {
    window->set_state(WindowState::Maximized);
}

void restore_window(IWindow* window) {
    window->set_state(WindowState::Normal);
}
```

---

### 2. 响应系统窗口状态变化

```cpp
class MyWindowEventSink : public IWindowEventSink {
public:
    void on_state_changed(WindowState new_state) override {
        switch (new_state) {
        case WindowState::Normal:
            log("窗口恢复正常");
            resume_rendering();
            break;
        case WindowState::Minimized:
            log("窗口最小化");
            pause_rendering();  // 节省资源
            break;
        case WindowState::Maximized:
            log("窗口最大化");
            adjust_layout_for_fullscreen();
            break;
        }
    }
};
```

---

### 3. 保存和恢复窗口状态

```cpp
struct WindowSettings {
    math::Point position;
    math::Size size;
    WindowState state;
};

// 保存
WindowSettings save_window_settings(IWindow* window) {
    return {
        .position = window->position(),
        .size = window->size(),
        .state = window->state(),
    };
}

// 恢复
void restore_window_settings(IWindow* window, const WindowSettings& settings) {
    window->set_position(settings.position);
    window->set_size(settings.size);
    window->set_state(settings.state);
}
```

---

## 状态转换

### 状态机

```
          ┌──────────┐
          │  Normal  │
          └─────┬────┘
             ↕  ↕
      ┌──────┘  └──────┐
      ↓                ↓
┌──────────┐      ┌──────────┐
│Minimized │      │Maximized │
└──────────┘      └──────────┘
```

**允许的转换**：
- `Normal` ↔ `Minimized`
- `Normal` ↔ `Maximized`
- `Minimized` → `Normal` → `Maximized`（通常需要两步）
- `Maximized` → `Normal` → `Minimized`（通常需要两步）

**注意**：`Minimized` ↔ `Maximized` 通常不直接支持，需要先恢复到 `Normal`。

---

## 平台差异

### Windows

| 状态 | Win32 API | 说明 |
|------|-----------|------|
| `Normal` | `ShowWindow(hwnd, SW_RESTORE)` | 恢复正常大小 |
| `Minimized` | `ShowWindow(hwnd, SW_MINIMIZE)` | 最小化到任务栏 |
| `Maximized` | `ShowWindow(hwnd, SW_MAXIMIZE)` | 最大化填充工作区 |

---

### macOS

| 状态 | Cocoa API | 说明 |
|------|-----------|------|
| `Normal` | `[window deminiaturize:nil]` + `[window zoom:nil]` 取消 | 恢复正常大小 |
| `Minimized` | `[window miniaturize:nil]` | 缩小到 Dock |
| `Maximized` | `[window zoom:nil]` | 放大填充屏幕（非 Full Screen） |

---

### Linux (X11)

| 状态 | EWMH 属性 | 说明 |
|------|-----------|------|
| `Normal` | 移除 `_NET_WM_STATE_HIDDEN` / `_NET_WM_STATE_MAXIMIZED_*` | 恢复正常大小 |
| `Minimized` | `_NET_WM_STATE_HIDDEN` | 图标化到任务栏 |
| `Maximized` | `_NET_WM_STATE_MAXIMIZED_HORZ` + `_NET_WM_STATE_MAXIMIZED_VERT` | 水平和垂直最大化 |

---

## 最佳实践

### 1. 最小化时暂停渲染

```cpp
// ✅ 推荐：窗口最小化时节省 CPU/GPU
void on_state_changed(WindowState new_state) {
    if (new_state == WindowState::Minimized) {
        renderer_->pause();
    } else {
        renderer_->resume();
    }
}

// ❌ 不推荐：窗口最小化仍以 60 FPS 渲染
void on_render() {
    // 浪费资源，用户看不到
}
```

---

### 2. 最大化时调整布局

```cpp
// ✅ 推荐：最大化时优化 UI 布局
void on_state_changed(WindowState new_state) {
    if (new_state == WindowState::Maximized) {
        sidebar_->set_width(300);  // 更宽的侧边栏
    } else {
        sidebar_->set_width(200);  // 正常宽度
    }
}
```

---

### 3. 避免频繁切换状态

```cpp
// ❌ 不推荐：短时间内频繁切换
window->set_state(WindowState::Maximized);
window->set_state(WindowState::Normal);
window->set_state(WindowState::Maximized);  // 闪烁

// ✅ 推荐：一次性设置到目标状态
if (should_maximize) {
    window->set_state(WindowState::Maximized);
}
```

---

## 常见陷阱

### 1. 最小化不等于隐藏

```cpp
// ❌ 错误：最小化后窗口仍然"可见"（is_visible() == true）
window->set_state(WindowState::Minimized);
assert(!window->is_visible());  // 可能失败！

// ✅ 正确：最小化是显示状态，不改变可见性
window->set_state(WindowState::Minimized);
assert(window->state() == WindowState::Minimized);
```

---

### 2. 跨平台状态同步时序

```cpp
// ❌ 问题：set_state() 可能异步生效
window->set_state(WindowState::Maximized);
assert(window->state() == WindowState::Maximized);  // 可能失败！

// ✅ 解决：等待 on_state_changed 事件
window->set_state(WindowState::Maximized);
// 在事件回调中验证状态
```

---

## 完整示例

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/WindowState.h>

class MainWindow : public IWindowEventSink {
    IWindow* window_;
    bool is_paused_ = false;
    
public:
    void on_state_changed(WindowState new_state) override {
        switch (new_state) {
        case WindowState::Normal:
            log("窗口恢复正常");
            if (is_paused_) {
                resume_application();
                is_paused_ = false;
            }
            break;
            
        case WindowState::Minimized:
            log("窗口最小化，暂停渲染");
            pause_application();
            is_paused_ = true;
            break;
            
        case WindowState::Maximized:
            log("窗口最大化，调整布局");
            adjust_layout_for_large_screen();
            break;
        }
    }
    
    void toggle_fullscreen() {
        if (window_->state() == WindowState::Maximized) {
            window_->set_state(WindowState::Normal);
        } else {
            window_->set_state(WindowState::Maximized);
        }
    }
    
private:
    void pause_application() {
        // 停止动画、暂停音频、降低更新频率
    }
    
    void resume_application() {
        // 恢复渲染、重新开始动画
    }
    
    void adjust_layout_for_large_screen() {
        // 调整 UI 元素布局以适应大屏幕
    }
};
```

---

## 总结

`WindowState` 是 `mine.platform.abi` 模块的窗口显示状态枚举，具备：

1. **三态模型**：Normal / Minimized / Maximized
2. **平台无关**：抽象 Windows、macOS、Linux 窗口状态
3. **资源优化**：最小化时可暂停渲染节省资源
4. **布局适配**：最大化时可调整 UI 布局

**使用建议**：
- 最小化时暂停渲染和动画
- 最大化时优化布局以适应大屏幕
- 通过 `on_state_changed` 事件响应状态变化
- 避免频繁切换状态导致闪烁
- 注意跨平台状态同步的异步性
