# IDevice 与 GfxHost 详细接口文档

## 概述

`IDevice` 和 `GfxHost` 是 `mine.gfx.rhi` 模块的**设备与工厂接口**。

**核心特性：**
- **IDevice**：图形设备抽象接口，RHI 的入口点，负责创建所有 GPU 资源
- **GfxHost**：工厂函数 create_device(Backend)，创建设备实例（链接时替换模式）

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/IDevice.h
src/mine/gfx/rhi/api/include/mine/gfx/GfxHost.h
```

---

## GfxHost（工厂函数）

### create_device()

```cpp
[[nodiscard]] core::OwnedPtr<IDevice> create_device(Backend backend = Backend::Auto);
```

**描述**：创建图形设备。

**参数**：
- `backend`：后端选择（Auto = 运行时自动选择最优后端）

**返回**：
- `OwnedPtr<IDevice>`：已初始化的设备；若指定后端不可用或初始化失败则返回 nullptr

**特征**：
- 函数定义由链接的后端库提供（链接时替换模式）
- 链接多个后端时，Auto 将按 D3D12 > D3D11 > Vulkan > Metal > GLES 顺序尝试

---

## IDevice 接口

### 类定义

```cpp
class IDevice {
public:
    virtual ~IDevice() = default;

    // ── 队列创建 ──────────────────────────────────────────────────────────
    [[nodiscard]] virtual core::OwnedPtr<IQueue> create_queue(QueueType type) = 0;

    // ── 交换链创建 ────────────────────────────────────────────────────────
    [[nodiscard]] virtual core::OwnedPtr<ISwapchain> create_swapchain(const SwapchainDesc& desc) = 0;

    // ── 资源创建 ──────────────────────────────────────────────────────────
    [[nodiscard]] virtual core::OwnedPtr<IBuffer> create_buffer(const BufferDesc& desc, const void* initial_data = nullptr) = 0;
    [[nodiscard]] virtual core::OwnedPtr<ITexture> create_texture(const TextureDesc& desc) = 0;

    // ── 管线创建 ──────────────────────────────────────────────────────────
    [[nodiscard]] virtual core::OwnedPtr<IPipeline> create_pipeline(const PipelineDesc& desc) = 0;

    // ── 命令列表创建 ──────────────────────────────────────────────────────
    [[nodiscard]] virtual core::OwnedPtr<ICommandList> create_command_list(QueueType type = QueueType::Graphics) = 0;

    // ── 同步对象创建 ──────────────────────────────────────────────────────
    [[nodiscard]] virtual core::OwnedPtr<IFence> create_fence(uint64_t initial_value = 0) = 0;

    // ── 设备信息 ──────────────────────────────────────────────────────────
    [[nodiscard]] virtual Backend backend() const noexcept = 0;
    [[nodiscard]] virtual const char* adapter_name() const noexcept = 0;

    // ── 资源更新 ──────────────────────────────────────────────────────────
    virtual void update_texture_region(ITexture* texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data, uint32_t row_pitch) = 0;
    virtual void copy_texture(ITexture* dst, ITexture* src) = 0;
};
```

**描述**：图形设备抽象接口，RHI 的入口点，负责创建所有 GPU 资源。

**创建**：mine::gfx::create_device(Backend)

**特征**：
- 同一进程可以同时存在多个 IDevice（多 GPU 场景），但通常只需一个
- D3D11：封装 ID3D11Device、ID3D11DeviceContext、IDXGIFactory2
- D3D12：封装 ID3D12Device
- Vulkan：封装 VkDevice

---

## 成员方法

### create_queue()

```cpp
[[nodiscard]] virtual core::OwnedPtr<IQueue> create_queue(QueueType type) = 0;
```

**描述**：创建指定类型的命令队列。

**参数**：
- `type`：Graphics / Compute / Copy

**返回**：队列对象；若该类型不支持则返回 nullptr

---

### create_swapchain()

```cpp
[[nodiscard]] virtual core::OwnedPtr<ISwapchain> create_swapchain(const SwapchainDesc& desc) = 0;
```

**描述**：为平台窗口创建交换链。

**参数**：
- `desc`：描述符，native_window 必须为有效窗口句柄

**返回**：交换链对象；失败返回 nullptr

---

### create_buffer()

```cpp
[[nodiscard]] virtual core::OwnedPtr<IBuffer> create_buffer(const BufferDesc& desc, const void* initial_data = nullptr) = 0;
```

**描述**：创建 GPU 缓冲。

**参数**：
- `desc`：缓冲描述符（大小、绑定标志等）
- `initial_data`：可选初始数据指针（传 nullptr 则创建空缓冲）

**返回**：缓冲对象；失败返回 nullptr

---

### create_texture()

```cpp
[[nodiscard]] virtual core::OwnedPtr<ITexture> create_texture(const TextureDesc& desc) = 0;
```

**描述**：创建 GPU 纹理。

**参数**：
- `desc`：纹理描述符（尺寸、格式、绑定标志等）

**返回**：纹理对象；失败返回 nullptr

---

### create_pipeline()

```cpp
[[nodiscard]] virtual core::OwnedPtr<IPipeline> create_pipeline(const PipelineDesc& desc) = 0;
```

**描述**：编译并创建图形管线状态对象（含着色器、顶点布局、混合/光栅化状态）。

**参数**：
- `desc`：管线描述符（着色器源码/字节码、顶点布局、混合开关等）

**返回**：管线对象；着色器编译失败或参数非法时返回 nullptr

**特征**：
- D3D11 后端在此调用 D3DCompile 完成 HLSL 编译
- 后续工具链（tool.shadercc）将改为传入预编译字节码

---

### create_command_list()

```cpp
[[nodiscard]] virtual core::OwnedPtr<ICommandList> create_command_list(QueueType type = QueueType::Graphics) = 0;
```

**描述**：为指定队列类型创建命令录制列表。

**参数**：
- `type`：命令列表对应的队列类型（需与提交目标队列一致）

**返回**：命令列表；失败返回 nullptr

---

### create_fence()

```cpp
[[nodiscard]] virtual core::OwnedPtr<IFence> create_fence(uint64_t initial_value = 0) = 0;
```

**描述**：创建 GPU-CPU 同步围栏。

**参数**：
- `initial_value`：初始完成值（默认 0）

**返回**：围栏对象；失败返回 nullptr

---

### backend()

```cpp
[[nodiscard]] virtual Backend backend() const noexcept = 0;
```

**描述**：返回此设备使用的图形后端。

**返回**：Backend（D3D11 / D3D12 / Vulkan / Metal / GLES）

---

### adapter_name()

```cpp
[[nodiscard]] virtual const char* adapter_name() const noexcept = 0;
```

**描述**：返回 GPU 名称（用于日志/调试）。

**返回**：GPU 名称（UTF-8 字符串）

---

### update_texture_region()

```cpp
virtual void update_texture_region(ITexture* texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data, uint32_t row_pitch) = 0;
```

**描述**：将 CPU 数据上传到 GPU 纹理的指定矩形区域。

**参数**：
- `texture`：目标纹理
- `x`：起始 X 坐标（像素）
- `y`：起始 Y 坐标（像素）
- `width`：宽度（像素）
- `height`：高度（像素）
- `data`：CPU 数据指针
- `row_pitch`：每行字节数（宽度 * 像素大小）

**特征**：
- 纹理必须以 D3D11_USAGE_DEFAULT / D3D11_USAGE_DYNAMIC 创建

---

### copy_texture()

```cpp
virtual void copy_texture(ITexture* dst, ITexture* src) = 0;
```

**描述**：在 GPU 上复制纹理（全纹理拷贝）。

**参数**：
- `dst`：目标纹理
- `src`：源纹理

---

## 使用场景

### 1. 创建设备

```cpp
#include <mine/gfx/GfxHost.h>

using namespace mine::gfx;

// 自动选择后端
auto device = create_device(Backend::Auto);
if (!device) {
    // 设备创建失败（硬件不支持或驱动问题）
    return -1;
}

// 明确指定后端
auto d3d11_device = create_device(Backend::D3D11);
```

---

### 2. 查询设备信息

```cpp
Backend backend = device->backend();
const char* adapter_name = device->adapter_name();

printf("后端: %d\n", static_cast<int>(backend));
printf("GPU: %s\n", adapter_name);
```

---

### 3. 创建资源

```cpp
// 创建队列
auto queue = device->create_queue(QueueType::Graphics);

// 创建交换链
SwapchainDesc sc_desc{};
sc_desc.native_window = window->native_handle().ptr;
sc_desc.width = 1920;
sc_desc.height = 1080;
sc_desc.format = PixelFormat::BGRA8_UNorm_sRGB;
auto swapchain = device->create_swapchain(sc_desc);

// 创建顶点缓冲
BufferDesc vb_desc{};
vb_desc.size = sizeof(vertices);
vb_desc.stride = sizeof(Vertex);
vb_desc.bind_flags = BufferBindFlags::Vertex;
auto vertex_buffer = device->create_buffer(vb_desc, vertices);

// 创建纹理
TextureDesc tex_desc{};
tex_desc.width = 512;
tex_desc.height = 512;
tex_desc.format = PixelFormat::RGBA8_UNorm;
tex_desc.bind_flags = TextureBindFlags::ShaderResource | TextureBindFlags::RenderTarget;
auto texture = device->create_texture(tex_desc);

// 创建管线
PipelineDesc pl_desc{};
// ... (设置管线描述符)
auto pipeline = device->create_pipeline(pl_desc);

// 创建命令列表
auto cmd = device->create_command_list(QueueType::Graphics);

// 创建围栏
auto fence = device->create_fence(0);
```

---

### 4. 更新纹理内容

```cpp
// 更新纹理区域
uint32_t x = 0, y = 0;
uint32_t width = 256, height = 256;
uint32_t row_pitch = width * 4;  // RGBA8 = 4 字节/像素
device->update_texture_region(texture.get(), x, y, width, height, data, row_pitch);

// 复制纹理
device->copy_texture(dst_texture.get(), src_texture.get());
```

---

### 5. 完整初始化流程

```cpp
#include <mine/gfx/GfxHost.h>
#include <mine/platform/PlatformAbi.h>

using namespace mine::gfx;
using namespace mine::platform;

int main() {
    // 1. 创建窗口
    auto app_host = create_application_host();
    WindowDesc win_desc{};
    win_desc.title = u"设备示例";
    win_desc.size = {1920, 1080};
    auto window = app_host->create_window(win_desc);
    
    // 2. 创建设备
    auto device = create_device(Backend::Auto);
    if (!device) {
        fprintf(stderr, "设备创建失败\n");
        return -1;
    }
    
    // 3. 查询设备信息
    printf("后端: %d\n", static_cast<int>(device->backend()));
    printf("GPU: %s\n", device->adapter_name());
    
    // 4. 创建交换链
    SwapchainDesc sc_desc{};
    sc_desc.native_window = window->native_handle().ptr;
    sc_desc.width = static_cast<uint32_t>(win_desc.size.width);
    sc_desc.height = static_cast<uint32_t>(win_desc.size.height);
    sc_desc.format = PixelFormat::BGRA8_UNorm_sRGB;
    auto swapchain = device->create_swapchain(sc_desc);
    
    // 5. 创建其他资源
    auto queue = device->create_queue(QueueType::Graphics);
    auto cmd = device->create_command_list();
    
    // 6. 渲染循环
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
    
    // 7. 清理（正确顺序）
    queue->wait_idle();
    swapchain.reset();
    window.reset();
    
    return 0;
}
```

---

## 最佳实践

### 1. 检查设备创建成功

```cpp
// ✅ 推荐：检查设备创建成功
auto device = create_device(Backend::Auto);
if (!device) {
    // 设备创建失败（硬件不支持或驱动问题）
    return -1;
}

// ❌ 不推荐：未检查设备创建成功
auto device = create_device(Backend::Auto);
auto queue = device->create_queue(QueueType::Graphics);  // 崩溃
```

---

### 2. 使用 Auto 后端

```cpp
// ✅ 推荐：使用 Auto 后端（自动选择最优）
auto device = create_device(Backend::Auto);

// ✅ 也可以：明确指定后端（测试特定后端）
auto device = create_device(Backend::D3D11);
```

---

### 3. 检查资源创建成功

```cpp
// ✅ 推荐：检查资源创建成功
auto buffer = device->create_buffer(desc, data);
if (!buffer) {
    // 缓冲创建失败
    return false;
}

// ❌ 不推荐：未检查资源创建成功
auto buffer = device->create_buffer(desc, data);
cmd->set_vertex_buffer(0, buffer.get(), 0);  // 崩溃
```

---

## 常见陷阱

### 1. 设备创建失败未检查

```cpp
// ❌ 问题：设备创建失败未检查
auto device = create_device(Backend::Auto);
auto queue = device->create_queue(QueueType::Graphics);  // 崩溃

// ✅ 解决：检查设备创建成功
auto device = create_device(Backend::Auto);
if (!device) {
    fprintf(stderr, "设备创建失败\n");
    return -1;
}
```

---

### 2. 资源创建失败未检查

```cpp
// ❌ 问题：资源创建失败未检查
auto buffer = device->create_buffer(desc, data);
cmd->set_vertex_buffer(0, buffer.get(), 0);  // 崩溃

// ✅ 解决：检查资源创建成功
auto buffer = device->create_buffer(desc, data);
if (!buffer) {
    fprintf(stderr, "缓冲创建失败\n");
    return false;
}
```

---

### 3. 管线编译失败未检查

```cpp
// ❌ 问题：管线编译失败未检查
auto pipeline = device->create_pipeline(desc);
cmd->set_pipeline(pipeline.get());  // 崩溃

// ✅ 解决：检查管线创建成功
auto pipeline = device->create_pipeline(desc);
if (!pipeline) {
    fprintf(stderr, "管线编译失败（HLSL 编译错误）\n");
    return false;
}
```

---

## 总结

`IDevice` 和 `GfxHost` 是 `mine.gfx.rhi` 模块的设备与工厂接口，具备：

1. **IDevice**：图形设备抽象接口，RHI 的入口点，负责创建所有 GPU 资源
2. **GfxHost**：工厂函数 create_device(Backend)，创建设备实例

**使用建议**：
- 检查设备创建成功
- 使用 Auto 后端（自动选择最优）
- 检查资源创建成功
- 查询设备信息（backend / adapter_name）
- 资源清理顺序：swapchain → window → device
