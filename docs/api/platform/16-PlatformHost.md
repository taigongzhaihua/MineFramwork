# PlatformHost 详细接口文档

## 概述

`PlatformHost` 是 `mine.platform.abi` 模块的**平台应用宿主工厂函数声明**（平台无关接口），提供 `create_application_host()` 工厂函数。

**核心特性：**
- **单一函数**：`create_application_host()` 创建平台宿主
- **平台无关**：头文件不含任何平台特定代码
- **链接时绑定**：实际实现由链接的平台后端库提供
- **返回 OwnedPtr**：返回 `OwnedPtr<IApplicationHost>`

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/PlatformHost.h
```

---

## 类型定义

```cpp
namespace mine::platform {

/**
 * @brief 创建当前平台的应用程序宿主实例。
 *
 * 声明在此，定义由链接的平台后端库（mine.platform.win32 等）提供。
 * 须在进程主线程调用，每进程只应调用一次。
 *
 * @return 持有 IApplicationHost 的 OwnedPtr（不会为空）
 */
core::OwnedPtr<IApplicationHost> create_application_host();

}
```

---

## 函数

### create_application_host()

```cpp
core::OwnedPtr<IApplicationHost> create_application_host();
```

**描述**：创建当前平台的应用程序宿主实例。

**返回**：持有 `IApplicationHost` 的 `OwnedPtr`（不会为空）

**特征**：
- 声明在 `PlatformHost.h`，定义由链接的平台后端库提供
- 须在进程主线程调用
- 每进程只应调用一次
- 返回 `OwnedPtr<IApplicationHost>`（调用方拥有生命周期）

**示例**：
```cpp
#include <mine/platform/PlatformHost.h>

int main() {
    auto host = mine::platform::create_application_host();
    // host 是 OwnedPtr<IApplicationHost>
    
    WindowDesc desc;
    desc.title = "My Application";
    auto window = host->create_window(desc);
    window->show();
    
    return host->run();
}
```

---

## 平台后端绑定

### 链接时绑定

**特征**：
- 头文件 `PlatformHost.h` 不含任何平台特定代码
- 实际实现由链接的平台后端库提供
- 应用代码只需 `#include <mine/platform/PlatformHost.h>`

**平台后端库**：
- **Windows**：`mine.platform.win32` → 返回 Win32 宿主
- **macOS**：`mine.platform.macos` → 返回 Cocoa 宿主（未来）
- **Linux**：`mine.platform.linux` → 返回 Wayland/X11 宿主（未来）

**示例（构建系统配置）**：
```lua
-- xmake.lua
target("myapp")
    add_deps("mine.platform.win32")  -- 链接 Win32 后端
    add_files("src/main.cpp")
```

---

### 平台实现

#### Windows (mine.platform.win32)

```cpp
// src/mine/platform/win32/PlatformHost_Win32.cpp
namespace mine::platform {

core::OwnedPtr<IApplicationHost> create_application_host() {
    return core::make_owned<ApplicationHost_Win32>();
}

}
```

---

#### macOS (mine.platform.macos)

```cpp
// src/mine/platform/macos/PlatformHost_macOS.mm
namespace mine::platform {

core::OwnedPtr<IApplicationHost> create_application_host() {
    return core::make_owned<ApplicationHost_macOS>();
}

}
```

---

#### Linux (mine.platform.linux)

```cpp
// src/mine/platform/linux/PlatformHost_X11.cpp
namespace mine::platform {

core::OwnedPtr<IApplicationHost> create_application_host() {
    return core::make_owned<ApplicationHost_X11>();
}

}
```

---

## 使用场景

### 1. 基本应用程序

```cpp
#include <mine/platform/PlatformHost.h>

int main() {
    // 创建平台宿主
    auto host = mine::platform::create_application_host();
    
    // 创建窗口
    WindowDesc desc;
    desc.title = "My Application";
    auto window = host->create_window(desc);
    window->show();
    
    // 进入消息循环
    return host->run();
}
```

---

### 2. 跨平台应用

```cpp
#include <mine/platform/PlatformHost.h>

int main() {
    // 无需 #ifdef，自动选择平台后端
    auto host = mine::platform::create_application_host();
    
    WindowDesc desc;
    desc.title = "Cross-Platform App";
    auto window = host->create_window(desc);
    window->show();
    
    return host->run();
}
```

---

### 3. 构建系统配置

```lua
-- xmake.lua
target("myapp")
    set_kind("binary")
    add_files("src/main.cpp")
    
    -- 根据平台链接不同后端
    if is_plat("windows") then
        add_deps("mine.platform.win32")
    elseif is_plat("macosx") then
        add_deps("mine.platform.macos")
    elseif is_plat("linux") then
        add_deps("mine.platform.linux")
    end
```

---

## 设计理念

### 平台无关接口

**目标**：
- 应用代码不包含任何平台 `#ifdef`
- 应用代码只需 `#include <mine/platform/PlatformHost.h>`

**实现**：
- 头文件 `PlatformHost.h` 只声明 `create_application_host()`
- 实际实现由链接的平台后端库提供

**示例**：
```cpp
// 应用代码（无 #ifdef）
#include <mine/platform/PlatformHost.h>

int main() {
    auto host = mine::platform::create_application_host();
    // ...
}
```

---

### 链接时绑定

**特征**：
- 编译时：应用代码调用 `create_application_host()`
- 链接时：链接器绑定到平台后端实现
- 运行时：调用实际平台实现

**优点**：
- 应用代码无平台特定代码
- 构建系统控制平台选择
- 易于跨平台移植

---

## 最佳实践

### 1. 每进程只调用一次

```cpp
// ✅ 推荐：每进程只调用一次
int main() {
    auto host = mine::platform::create_application_host();
    // ...
}

// ❌ 不推荐：多次调用（未定义行为）
int main() {
    auto host1 = mine::platform::create_application_host();
    auto host2 = mine::platform::create_application_host();  // 未定义行为
}
```

---

### 2. 主线程调用

```cpp
// ✅ 推荐：主线程调用
int main() {
    auto host = mine::platform::create_application_host();
    // ...
}

// ❌ 不推荐：后台线程调用（未定义行为）
std::thread([]() {
    auto host = mine::platform::create_application_host();  // 崩溃
}).join();
```

---

### 3. 使用 OwnedPtr 管理生命周期

```cpp
// ✅ 推荐：使用 OwnedPtr
auto host = mine::platform::create_application_host();
// 自动管理生命周期

// ❌ 不推荐：裸指针
IApplicationHost* host = mine::platform::create_application_host().release();
// 需要手动管理生命周期
```

---

## 常见陷阱

### 1. 多次调用 create_application_host()

```cpp
// ❌ 问题：多次调用
auto host1 = mine::platform::create_application_host();
auto host2 = mine::platform::create_application_host();  // 未定义行为

// ✅ 解决：每进程只调用一次
auto host = mine::platform::create_application_host();
```

---

### 2. 后台线程调用

```cpp
// ❌ 问题：后台线程调用
std::thread([]() {
    auto host = mine::platform::create_application_host();  // 崩溃
}).join();

// ✅ 解决：主线程调用
int main() {
    auto host = mine::platform::create_application_host();
    // ...
}
```

---

### 3. 忘记链接平台后端库

```lua
-- ❌ 问题：忘记链接平台后端库
target("myapp")
    set_kind("binary")
    add_files("src/main.cpp")
    -- 忘记链接平台后端库

-- ✅ 解决：链接平台后端库
target("myapp")
    set_kind("binary")
    add_files("src/main.cpp")
    add_deps("mine.platform.win32")  -- 链接 Win32 后端
```

---

## 完整示例

```cpp
#include <mine/platform/PlatformHost.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IWindow.h>

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
        if (!window_ptr) {
            log("窗口创建失败");
            return;
        }
        
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
        if (event.kind == WindowEventKind::Closed) {
            cleanup();
            host_->quit(0);
        }
    }
    
private:
    void cleanup() {
        log("清理资源");
        window_ = nullptr;
    }
};

int main() {
    // 创建平台宿主（无 #ifdef）
    auto host = mine::platform::create_application_host();
    
    // 创建应用
    Application app(host.get());
    
    // 进入消息循环
    return host->run();
}
```

---

## 构建系统集成

### xmake.lua 示例

```lua
-- xmake.lua
target("myapp")
    set_kind("binary")
    add_files("src/main.cpp")
    
    -- 根据平台链接不同后端
    if is_plat("windows") then
        add_deps("mine.platform.win32")
    elseif is_plat("macosx") then
        add_deps("mine.platform.macos")
    elseif is_plat("linux") then
        add_deps("mine.platform.linux")
    end
```

---

### CMakeLists.txt 示例

```cmake
# CMakeLists.txt
add_executable(myapp src/main.cpp)

# 根据平台链接不同后端
if(WIN32)
    target_link_libraries(myapp mine.platform.win32)
elseif(APPLE)
    target_link_libraries(myapp mine.platform.macos)
elseif(UNIX)
    target_link_libraries(myapp mine.platform.linux)
endif()
```

---

## 总结

`PlatformHost` 是 `mine.platform.abi` 模块的平台应用宿主工厂函数声明，具备：

1. **单一函数**：`create_application_host()` 创建平台宿主
2. **平台无关**：头文件不含任何平台特定代码
3. **链接时绑定**：实际实现由链接的平台后端库提供
4. **返回 OwnedPtr**：返回 `OwnedPtr<IApplicationHost>`

**使用建议**：
- 每进程只调用一次 `create_application_host()`
- 主线程调用
- 使用 `OwnedPtr` 管理生命周期
- 构建系统链接正确的平台后端库
- 应用代码无需 `#ifdef` 或平台特定 `#include`
