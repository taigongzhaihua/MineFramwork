# Gfx/Api 详细接口文档

## 概述

`Gfx.h` 和 `Api.h` 是 `mine.gfx.rhi` 模块的**元文件**。

**核心特性：**
- **Gfx.h**：伞形头文件（umbrella header），包含完整的 RHI 抽象接口
- **Api.h**：导出宏定义（DLL export/import macros），定义 MINE_GFX_RHI_API 宏

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/Gfx.h
src/mine/gfx/rhi/api/include/mine/gfx/Api.h
```

---

## Gfx.h（伞形头文件）

### 文件内容

```cpp
/**
 * @file Gfx.h
 * @brief mine.gfx.rhi 模块伞形头文件。
 *
 * 包含此头文件即可引入完整的 RHI 抽象接口。
 * 具体实现由链接的后端库（mine.gfx.d3d11 / mine.gfx.d3d12 / ...）提供。
 */

#pragma once

#include "Api.h"
#include "GfxTypes.h"
#include "IBuffer.h"
#include "ICommandList.h"
#include "IDevice.h"
#include "IFence.h"
#include "IPipeline.h"
#include "IQueue.h"
#include "ISwapchain.h"
#include "ITexture.h"
#include "GfxHost.h"
```

**描述**：伞形头文件（umbrella header），包含完整的 RHI 抽象接口。

**特征**：
- 一次性引入所有 RHI 接口头文件
- 简化用户代码（无需逐个 include）
- 具体实现由链接的后端库提供（mine.gfx.d3d11 / mine.gfx.d3d12 / mine.gfx.vulkan / mine.gfx.metal / mine.gfx.gles）

---

## Api.h（导出宏定义）

### 文件内容

```cpp
/**
 * @file Api.h
 * @brief mine.gfx.rhi 模块导出宏定义。
 */

#pragma once

// mine.gfx.rhi 是纯接口层（无实现），不需要 DLL 导出标记；
// 保留此宏供将来将接口层单独编译为共享库时使用。
#define MINE_GFX_RHI_API
```

**描述**：导出宏定义（DLL export/import macros），定义 MINE_GFX_RHI_API 宏。

**特征**：
- MINE_GFX_RHI_API 当前为空（纯接口层无实现）
- 保留宏供将来将接口层单独编译为共享库时使用

---

## 使用场景

### 1. 使用 Gfx.h 伞形头文件

```cpp
// 推荐：使用伞形头文件（一次性引入所有 RHI 接口）
#include <mine/gfx/Gfx.h>

using namespace mine::gfx;

int main() {
    // 创建设备
    auto device = create_device(Backend::Auto);
    
    // 使用所有 RHI 接口
    auto queue = device->create_queue(QueueType::Graphics);
    auto cmd = device->create_command_list();
    auto fence = device->create_fence(0);
    // ...
}
```

---

### 2. 使用单个头文件

```cpp
// 也可以：只引入需要的头文件
#include <mine/gfx/GfxHost.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>

using namespace mine::gfx;

int main() {
    auto device = create_device(Backend::Auto);
    auto queue = device->create_queue(QueueType::Graphics);
    // ...
}
```

---

### 3. Gfx.h 包含的头文件

```cpp
// Gfx.h 包含以下头文件：
// - Api.h            导出宏定义
// - GfxTypes.h       基础类型、枚举、描述符结构体
// - IBuffer.h        GPU 缓冲资源接口
// - ITexture.h       GPU 纹理资源接口
// - ICommandList.h   命令录制接口
// - IQueue.h         命令提交接口
// - IFence.h         GPU-CPU 同步接口
// - IPipeline.h      管线状态对象接口
// - ISwapchain.h     交换链接口
// - IDevice.h        图形设备接口
// - GfxHost.h        工厂函数声明
```

---

## 完整示例

### 示例1：使用 Gfx.h 伞形头文件

```cpp
#include <mine/gfx/Gfx.h>
#include <mine/platform/PlatformAbi.h>

using namespace mine::gfx;
using namespace mine::platform;

int main() {
    // 创建窗口
    auto app_host = create_application_host();
    WindowDesc win_desc{};
    win_desc.title = u"Gfx 示例";
    win_desc.size = {1920, 1080};
    auto window = app_host->create_window(win_desc);
    
    // 创建设备
    auto device = create_device(Backend::Auto);
    
    // 创建交换链
    SwapchainDesc sc_desc{};
    sc_desc.native_window = window->native_handle().ptr;
    sc_desc.width = static_cast<uint32_t>(win_desc.size.width);
    sc_desc.height = static_cast<uint32_t>(win_desc.size.height);
    sc_desc.format = PixelFormat::BGRA8_UNorm_sRGB;
    auto swapchain = device->create_swapchain(sc_desc);
    
    // 创建其他资源
    auto queue = device->create_queue(QueueType::Graphics);
    auto cmd = device->create_command_list();
    auto fence = device->create_fence(0);
    
    // 渲染循环
    bool running = true;
    while (running) {
        app_host->poll_events();
        
        cmd->begin();
        cmd->set_render_target(swapchain->current_render_target());
        cmd->clear_render_target(swapchain->current_render_target(), {0.1f, 0.1f, 0.1f, 1.0f});
        cmd->end();
        
        queue->submit(cmd.get());
        swapchain->present();
    }
    
    // 清理
    queue->wait_idle();
    swapchain.reset();
    window.reset();
    
    return 0;
}
```

---

### 示例2：只引入需要的头文件

```cpp
#include <mine/gfx/GfxHost.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ICommandList.h>

using namespace mine::gfx;

int main() {
    // 创建设备
    auto device = create_device(Backend::D3D11);
    if (!device) {
        return -1;
    }
    
    // 创建队列和命令列表
    auto queue = device->create_queue(QueueType::Graphics);
    auto cmd = device->create_command_list();
    
    // 使用队列和命令列表
    cmd->begin();
    // ... 录制命令
    cmd->end();
    queue->submit(cmd.get());
    queue->wait_idle();
    
    return 0;
}
```

---

## 最佳实践

### 1. 使用 Gfx.h 伞形头文件

```cpp
// ✅ 推荐：使用伞形头文件（一次性引入所有 RHI 接口）
#include <mine/gfx/Gfx.h>

// ❌ 不推荐：逐个引入头文件（繁琐）
#include <mine/gfx/GfxHost.h>
#include <mine/gfx/IDevice.h>
#include <mine/gfx/IQueue.h>
#include <mine/gfx/ICommandList.h>
#include <mine/gfx/IFence.h>
#include <mine/gfx/ISwapchain.h>
#include <mine/gfx/IBuffer.h>
#include <mine/gfx/ITexture.h>
#include <mine/gfx/IPipeline.h>
```

---

### 2. 使用命名空间

```cpp
// ✅ 推荐：使用 using 命名空间（简化代码）
#include <mine/gfx/Gfx.h>
using namespace mine::gfx;

auto device = create_device(Backend::Auto);

// ❌ 不推荐：每次都写完整命名空间（繁琐）
#include <mine/gfx/Gfx.h>

auto device = mine::gfx::create_device(mine::gfx::Backend::Auto);
```

---

## 总结

`Gfx.h` 和 `Api.h` 是 `mine.gfx.rhi` 模块的元文件，具备：

1. **Gfx.h**：伞形头文件（umbrella header），包含完整的 RHI 抽象接口
2. **Api.h**：导出宏定义（DLL export/import macros），定义 MINE_GFX_RHI_API 宏

**使用建议**：
- 使用 Gfx.h 伞形头文件（一次性引入所有 RHI 接口）
- 使用命名空间（using namespace mine::gfx）
- 具体实现由链接的后端库提供（mine.gfx.d3d11 / mine.gfx.d3d12 / ...）
