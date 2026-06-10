# ISwapchain 详细接口文档

## 概述

`ISwapchain` 是 `mine.gfx.rhi` 模块的**GPU 交换链抽象接口**。

**核心特性：**
- **后缓冲管理**：管理一组后缓冲纹理（双缓冲/三缓冲）
- **呈现**：present() 将帧提交给显示系统（DWM / 操作系统合成器）
- **尺寸调整**：resize() 调整交换链大小（窗口 SizeChanged 事件时调用）

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/ISwapchain.h
```

---

## 类定义

```cpp
class ISwapchain {
public:
    virtual ~ISwapchain() = default;

    /// 调整交换链大小（窗口 SizeChanged 事件时调用）
    virtual void resize(uint32_t width, uint32_t height) = 0;

    /// 将当前渲染帧呈现到屏幕
    virtual void present() = 0;

    /// 获取当前可写入的后缓冲渲染目标
    [[nodiscard]] virtual ITexture* current_render_target() noexcept = 0;

    /// 当前交换链宽度（像素）
    [[nodiscard]] virtual uint32_t width() const noexcept = 0;

    /// 当前交换链高度（像素）
    [[nodiscard]] virtual uint32_t height() const noexcept = 0;

    /// 后缓冲像素格式
    [[nodiscard]] virtual PixelFormat format() const noexcept = 0;

    /// 缓冲帧数（2=双缓冲，3=三缓冲）
    [[nodiscard]] virtual uint32_t image_count() const noexcept = 0;

    /// 垂直同步模式
    [[nodiscard]] virtual Vsync vsync() const noexcept = 0;
};
```

**描述**：GPU 交换链抽象接口。

**创建**：IDevice::create_swapchain(SwapchainDesc)，由 OwnedPtr<ISwapchain> 管理。

**特征**：
- D3D11：封装 IDXGISwapChain1（FLIP_DISCARD 交换模型）
- D3D12：封装 IDXGISwapChain3/4
- Vulkan：封装 VkSwapchainKHR

---

## SwapchainDesc 结构体

```cpp
struct SwapchainDesc {
    void*       native_window{nullptr}; ///< 平台原生窗口句柄（Win32: HWND）
    uint32_t    width{0};
    uint32_t    height{0};
    PixelFormat format{PixelFormat::BGRA8_UNorm_sRGB}; ///< 推荐使用 sRGB 格式
    uint32_t    image_count{3};  ///< 缓冲帧数（2=双缓冲，3=三缓冲）
    Vsync       vsync{Vsync::On};
};
```

**描述**：交换链创建描述符。

**字段说明**：
- **native_window**：平台原生窗口句柄（Win32: HWND）
- **width**：窗口客户区宽度（像素）
- **height**：窗口客户区高度（像素）
- **format**：后缓冲像素格式（推荐 BGRA8_UNorm_sRGB）
- **image_count**：缓冲帧数（2=双缓冲，3=三缓冲，默认 3）
- **vsync**：垂直同步模式（On/Off/Adaptive，默认 On）

---

## 成员方法

### resize()

```cpp
virtual void resize(uint32_t width, uint32_t height) = 0;
```

**描述**：调整交换链大小（窗口 SizeChanged 事件时调用）。

**参数**：
- `width`：新的窗口客户区宽度（像素）
- `height`：新的窗口客户区高度（像素）

**特征**：
- 调用此函数前应确保当前帧的命令已提交，且 GPU 不再写入后缓冲
- D3D11：调用 IDXGISwapChain::ResizeBuffers()
- 必须先释放所有后缓冲引用，否则会返回 DXGI_ERROR_INVALID_CALL

---

### present()

```cpp
virtual void present() = 0;
```

**描述**：将当前渲染帧呈现到屏幕。

**特征**：
- 根据创建时的 Vsync 设置决定是否等待垂直同步
- D3D11：调用 IDXGISwapChain::Present(sync_interval, flags)
- Vsync::Off + 硬件支持 Tearing 时，使用 DXGI_PRESENT_ALLOW_TEARING

---

### current_render_target()

```cpp
[[nodiscard]] virtual ITexture* current_render_target() noexcept = 0;
```

**描述**：获取当前可写入的后缓冲渲染目标。

**返回**：ITexture* 后缓冲纹理（裸指针，不转移所有权）

**特征**：
- 返回的指针由 Swapchain 内部管理
- 调用方不得删除此指针
- resize() / present() 后应重新获取

---

### width()

```cpp
[[nodiscard]] virtual uint32_t width() const noexcept = 0;
```

**描述**：当前交换链宽度（像素）。

---

### height()

```cpp
[[nodiscard]] virtual uint32_t height() const noexcept = 0;
```

**描述**：当前交换链高度（像素）。

---

### format()

```cpp
[[nodiscard]] virtual PixelFormat format() const noexcept = 0;
```

**描述**：后缓冲像素格式。

---

### image_count()

```cpp
[[nodiscard]] virtual uint32_t image_count() const noexcept = 0;
```

**描述**：缓冲帧数（2=双缓冲，3=三缓冲）。

---

### vsync()

```cpp
[[nodiscard]] virtual Vsync vsync() const noexcept = 0;
```

**描述**：垂直同步模式。

---

## 使用场景

### 1. 创建交换链

```cpp
// 创建设备
auto device = create_device(Backend::D3D11);

// 创建交换链
SwapchainDesc desc{};
desc.native_window = window->native_handle().ptr;  // HWND
desc.width = 1920;
desc.height = 1080;
desc.image_count = 2;  // 双缓冲
desc.format = PixelFormat::BGRA8_UNorm_sRGB;
desc.vsync = Vsync::On;

auto swapchain = device->create_swapchain(desc);
```

---

### 2. 渲染到交换链后缓冲

```cpp
// 获取后缓冲
ITexture* back_buffer = swapchain->current_render_target();

// 录制命令
cmd->begin();
cmd->set_render_target(back_buffer);
cmd->clear_render_target(back_buffer, {0.1f, 0.1f, 0.1f, 1.0f});
cmd->set_viewport(viewport);
cmd->set_pipeline(pipeline.get());
cmd->draw(3, 1, 0, 0);
cmd->end();

// 提交到 GPU
queue->submit(cmd.get());

// 呈现
swapchain->present();
```

---

### 3. 窗口 resize 处理

```cpp
// 监听窗口 SizeChanged 事件
window->resized_event += [&](IWindow* /*win*/, const core::Size& new_size) {
    // 等待 GPU 完成
    queue->wait_idle();

    // 调整交换链大小
    swapchain->resize(
        static_cast<uint32_t>(new_size.width),
        static_cast<uint32_t>(new_size.height));
};
```

---

### 4. 查询交换链属性

```cpp
uint32_t width = swapchain->width();
uint32_t height = swapchain->height();
PixelFormat format = swapchain->format();
uint32_t image_count = swapchain->image_count();
Vsync vsync = swapchain->vsync();
```

---

### 5. 三缓冲

```cpp
SwapchainDesc desc{};
desc.native_window = window->native_handle().ptr;
desc.width = 1920;
desc.height = 1080;
desc.image_count = 3;  // 三缓冲
desc.format = PixelFormat::BGRA8_UNorm_sRGB;
desc.vsync = Vsync::On;

auto swapchain = device->create_swapchain(desc);
```

---

### 6. 无 Vsync（高性能）

```cpp
SwapchainDesc desc{};
desc.native_window = window->native_handle().ptr;
desc.width = 1920;
desc.height = 1080;
desc.image_count = 2;
desc.format = PixelFormat::BGRA8_UNorm_sRGB;
desc.vsync = Vsync::Off;  // 关闭垂直同步

auto swapchain = device->create_swapchain(desc);
```

---

## 性能分析

### present() 开销

**特征**：
- Vsync::On：等待垂直同步（~16.7 ms @ 60 Hz）
- Vsync::Off：无等待（~0.1-1 ms）
- Vsync::Off + Tearing：无撕裂的不等待（G-Sync/FreeSync）

---

### resize() 开销

**特征**：
- D3D11：ResizeBuffers() 开销中等（~1-5 ms）
- 必须先等待 GPU 完成（wait_idle）
- 应尽量减少 resize 调用频率

---

## 最佳实践

### 1. resize 前等待 GPU 完成

```cpp
// ✅ 推荐：resize 前等待 GPU 完成
queue->wait_idle();
swapchain->resize(new_width, new_height);

// ❌ 不推荐：未等待 GPU 完成
swapchain->resize(new_width, new_height);  // 可能导致设备丢失
```

---

### 2. present 后重新获取后缓冲

```cpp
// ✅ 推荐：present 后重新获取后缓冲
swapchain->present();
ITexture* back_buffer = swapchain->current_render_target();

// ❌ 不推荐：使用旧的后缓冲指针
ITexture* back_buffer = swapchain->current_render_target();
swapchain->present();
// back_buffer 可能已失效
```

---

### 3. 使用 sRGB 格式

```cpp
// ✅ 推荐：使用 sRGB 格式（正确的颜色空间）
desc.format = PixelFormat::BGRA8_UNorm_sRGB;

// ❌ 不推荐：使用 UNorm 格式（颜色过亮）
desc.format = PixelFormat::BGRA8_UNorm;
```

---

### 4. 使用三缓冲降低延迟

```cpp
// ✅ 推荐：三缓冲（更流畅）
desc.image_count = 3;

// ✅ 也可以：双缓冲（更低延迟）
desc.image_count = 2;
```

---

## 常见陷阱

### 1. resize 前未等待 GPU 完成

```cpp
// ❌ 问题：resize 前未等待 GPU 完成
swapchain->resize(new_width, new_height);  // 可能导致 DXGI_ERROR_INVALID_CALL

// ✅ 解决：resize 前等待 GPU 完成
queue->wait_idle();
swapchain->resize(new_width, new_height);
```

---

### 2. 使用已失效的后缓冲指针

```cpp
// ❌ 问题：使用已失效的后缓冲指针
ITexture* back_buffer = swapchain->current_render_target();
swapchain->resize(new_width, new_height);
cmd->set_render_target(back_buffer);  // 错误：back_buffer 已失效

// ✅ 解决：resize 后重新获取后缓冲
ITexture* back_buffer = swapchain->current_render_target();
swapchain->resize(new_width, new_height);
back_buffer = swapchain->current_render_target();  // 重新获取
cmd->set_render_target(back_buffer);
```

---

### 3. 忘记调用 present

```cpp
// ❌ 问题：忘记调用 present
cmd->begin();
cmd->set_render_target(swapchain->current_render_target());
cmd->draw(...);
cmd->end();
queue->submit(cmd.get());
// 缺少 swapchain->present();

// ✅ 解决：提交后调用 present
cmd->begin();
cmd->set_render_target(swapchain->current_render_target());
cmd->draw(...);
cmd->end();
queue->submit(cmd.get());
swapchain->present();
```

---

### 4. 窗口销毁前未销毁交换链

```cpp
// ❌ 问题：窗口销毁前未销毁交换链
window.reset();  // 窗口销毁
swapchain.reset();  // 错误：交换链绑定的 HWND 已失效

// ✅ 解决：先销毁交换链再销毁窗口
swapchain.reset();
window.reset();
```

---

## 完整示例

```cpp
#include <mine/gfx/Gfx.h>
#include <mine/platform/PlatformAbi.h>

using namespace mine::gfx;
using namespace mine::platform;

int main() {
    // 创建窗口
    auto app_host = create_application_host();
    WindowDesc win_desc{};
    win_desc.title = u"Swapchain 示例";
    win_desc.size = {1920, 1080};
    auto window = app_host->create_window(win_desc);
    
    // 创建设备
    auto device = create_device(Backend::D3D11);
    
    // 创建交换链
    SwapchainDesc sc_desc{};
    sc_desc.native_window = window->native_handle().ptr;
    sc_desc.width = static_cast<uint32_t>(win_desc.size.width);
    sc_desc.height = static_cast<uint32_t>(win_desc.size.height);
    sc_desc.image_count = 2;
    sc_desc.format = PixelFormat::BGRA8_UNorm_sRGB;
    sc_desc.vsync = Vsync::On;
    auto swapchain = device->create_swapchain(sc_desc);
    
    // 创建命令列表和队列
    auto cmd = device->create_command_list();
    auto queue = device->create_queue(QueueType::Graphics);
    
    // 监听窗口 resize
    window->resized_event += [&](IWindow* /*win*/, const core::Size& new_size) {
        queue->wait_idle();
        swapchain->resize(
            static_cast<uint32_t>(new_size.width),
            static_cast<uint32_t>(new_size.height));
    };
    
    // 渲染循环
    bool running = true;
    while (running) {
        app_host->poll_events();
        
        ITexture* back_buffer = swapchain->current_render_target();
        
        cmd->begin();
        cmd->set_render_target(back_buffer);
        cmd->clear_render_target(back_buffer, {0.1f, 0.1f, 0.1f, 1.0f});
        // ... 更多绘制命令
        cmd->end();
        
        queue->submit(cmd.get());
        swapchain->present();
    }
    
    // 清理（正确顺序）
    queue->wait_idle();
    swapchain.reset();
    window.reset();
    
    return 0;
}
```

---

## 总结

`ISwapchain` 是 `mine.gfx.rhi` 模块的 GPU 交换链抽象接口，具备：

1. **后缓冲管理**：管理一组后缓冲纹理（双缓冲/三缓冲）
2. **呈现**：present() 将帧提交给显示系统
3. **尺寸调整**：resize() 调整交换链大小

**使用建议**：
- resize 前等待 GPU 完成
- present 后重新获取后缓冲
- 使用 sRGB 格式
- 使用三缓冲降低延迟
- 先销毁交换链再销毁窗口
- 提交后调用 present
