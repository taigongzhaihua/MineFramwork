# PlatformAbi 详细接口文档

## 概述

`PlatformAbi` 是 `mine.platform.abi` 模块的**伞形头文件**，包含此头文件即可访问 `mine.platform.abi` 的全部公共接口。

**核心特性：**
- **单一头文件**：`#include <mine/platform/PlatformAbi.h>` 访问全部接口
- **包含 17 个头文件**：基础类型、枚举、事件、接口、描述符
- **便捷性**：无需手动包含多个头文件

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/PlatformAbi.h
```

---

## 类型定义

```cpp
#pragma once

#include <mine/platform/Api.h>
#include <mine/platform/ModuleTag.h>
#include <mine/platform/NativeHandle.h>
#include <mine/platform/WindowKind.h>
#include <mine/platform/WindowState.h>
#include <mine/platform/WindowEvent.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/ScreenInfo.h>
#include <mine/platform/IScreenManager.h>
#include <mine/platform/IClipboard.h>
#include <mine/platform/IMEService.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/PlatformHost.h>
```

---

## 包含的头文件

### 元数据

| 头文件 | 描述 |
|--------|------|
| `Api.h` | `MINE_PLATFORM_ABI_API` 导出宏（空占位） |
| `ModuleTag.h` | `kModuleName = "mine.platform.abi"` 模块标识 |

---

### 基础类型

| 头文件 | 描述 |
|--------|------|
| `NativeHandle.h` | 平台原生窗口句柄（HWND/NSWindow*/XID） |

---

### 枚举类型

| 头文件 | 描述 |
|--------|------|
| `WindowKind.h` | 窗口类型枚举（Normal/Tool/Dialog/Splash/Popup） |
| `WindowState.h` | 窗口显示状态枚举（Normal/Minimized/Maximized） |
| `WindowCornerPreference.h` | 窗口圆角偏好枚举（SystemDefault/DoNotRound/Round/RoundSmall） |

---

### 事件

| 头文件 | 描述 |
|--------|------|
| `WindowEvent.h` | 窗口事件数据（Flat 结构 + 21 种事件类型） |
| `IWindowEventSink.h` | 事件接收器接口（Observer） |
| `IWindowEventSource.h` | 事件分发器接口（Observable） |

---

### 显示器

| 头文件 | 描述 |
|--------|------|
| `ScreenInfo.h` | 显示器信息结构体（bounds/work_area/dpi/scale/is_primary） |
| `IScreenManager.h` | 多显示器管理接口（screen_count/screen_at/primary_screen/screen_for_point） |

---

### 剪贴板

| 头文件 | 描述 |
|--------|------|
| `IClipboard.h` | 系统剪贴板接口（has_text/get_text/set_text/clear） |

---

### IME

| 头文件 | 描述 |
|--------|------|
| `IMEService.h` | 输入法服务接口（enable/disable/is_enabled/set_composition_rect） |

---

### 窗口

| 头文件 | 描述 |
|--------|------|
| `WindowDesc.h` | 窗口创建参数（11 字段） |
| `WindowChromeDesc.h` | 自定义 Chrome 参数（5 字段） |
| `IWindow.h` | 窗口接口（18 方法） |

---

### 应用程序宿主

| 头文件 | 描述 |
|--------|------|
| `IApplicationHost.h` | 应用程序宿主接口（9 方法） |
| `PlatformHost.h` | 平台应用宿主工厂函数声明（create_application_host） |

---

## 使用场景

### 1. 包含全部接口

```cpp
#include <mine/platform/PlatformAbi.h>

int main() {
    // 访问全部 mine.platform.abi 接口
    auto host = mine::platform::create_application_host();
    
    WindowDesc desc;
    desc.title = "My Application";
    auto window = host->create_window(desc);
    window->show();
    
    return host->run();
}
```

---

### 2. 与单独包含对比

```cpp
// ✅ 推荐：包含伞形头文件
#include <mine/platform/PlatformAbi.h>

// ❌ 不推荐：手动包含多个头文件
#include <mine/platform/PlatformHost.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/WindowEvent.h>
#include <mine/platform/IWindowEventSink.h>
```

---

### 3. 跨平台应用

```cpp
#include <mine/platform/PlatformAbi.h>

class Application : public IWindowEventSink {
    IApplicationHost* host_;
    IWindow* window_;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        WindowDesc desc;
        desc.title = "Cross-Platform App";
        auto window_ptr = host_->create_window(desc);
        window_ = window_ptr.release();
        window_->events().subscribe(this);
        window_->show();
    }
    
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::Closed) {
            host_->quit(0);
        }
    }
};

int main() {
    auto host = mine::platform::create_application_host();
    Application app(host.get());
    return host->run();
}
```

---

## 设计理念

### 伞形头文件

**目标**：
- 提供单一入口访问全部接口
- 简化用户代码（无需手动包含多个头文件）

**实现**：
- `PlatformAbi.h` 包含全部公共头文件
- 用户只需 `#include <mine/platform/PlatformAbi.h>`

---

### 接口层

**特征**：
- `mine.platform.abi` 是纯接口层（头文件库）
- 无实际二进制输出
- 不含平台特定代码

---

## 最佳实践

### 1. 使用伞形头文件

```cpp
// ✅ 推荐：使用伞形头文件
#include <mine/platform/PlatformAbi.h>

// ❌ 不推荐：手动包含多个头文件
#include <mine/platform/PlatformHost.h>
#include <mine/platform/IApplicationHost.h>
// ...
```

---

### 2. 前向声明

```cpp
// ✅ 推荐：前向声明（减少编译依赖）
namespace mine::platform {
    class IWindow;
}

class MyClass {
    IWindow* window_;  // 只需前向声明
};

// 实现文件中包含完整头文件
#include <mine/platform/PlatformAbi.h>
```

---

## 完整示例

```cpp
#include <mine/platform/PlatformAbi.h>

class Application : public IWindowEventSink {
    IApplicationHost* host_;
    IWindow* window_;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        // 创建窗口
        WindowDesc desc;
        desc.title = "My Application";
        desc.size = {800, 600};
        auto window_ptr = host_->create_window(desc);
        window_ = window_ptr.release();
        window_->events().subscribe(this);
        window_->show();
    }
    
    ~Application() {
        if (window_) {
            window_->events().unsubscribe(this);
        }
    }
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Closed:
            cleanup();
            host_->quit(0);
            break;
            
        case WindowEventKind::KeyDown:
            if (event.key_vk_code == VK_ESCAPE) {
                window_->close();
            }
            break;
            
        default:
            break;
        }
    }
    
private:
    void cleanup() {
        log("清理资源");
        window_ = nullptr;
    }
};

int main() {
    // 访问全部 mine.platform.abi 接口
    auto host = mine::platform::create_application_host();
    Application app(host.get());
    return host->run();
}
```

---

## 总结

`PlatformAbi` 是 `mine.platform.abi` 模块的伞形头文件，具备：

1. **单一头文件**：`#include <mine/platform/PlatformAbi.h>` 访问全部接口
2. **包含 17 个头文件**：基础类型、枚举、事件、接口、描述符
3. **便捷性**：无需手动包含多个头文件
4. **接口层**：纯头文件库，无实际二进制输出

**使用建议**：
- 使用伞形头文件访问全部接口
- 前向声明减少编译依赖
- 接口层无需 DLL 导出
