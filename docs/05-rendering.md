# 05 — 渲染（RHI 与后端）

## 5.1 设计目标

* 一套抽象同时覆盖 D3D11 / D3D12 / Metal / Vulkan / GLES，避免 UI 层感知 API 细节。
* **薄抽象**：尽量贴近 D3D12/Metal/Vulkan 的现代命令缓冲模型，D3D11 / GLES 通过适配实现。
* 支持**多窗口**、**多线程录制命令**、**单线程提交**。
* 无全局状态：所有句柄经 `Device` / `CommandList` 等显式传递。

## 5.2 RHI 抽象（mine.gfx.rhi.api）

```cpp
namespace mine::gfx {

class IDevice;          // 物理/逻辑设备
class IQueue;           // 命令队列（Graphics/Compute/Copy）
class ICommandList;     // 命令录制
class ISwapchain;       // 与 Window 绑定
class IBuffer;          // 顶点/索引/常量
class ITexture;
class ISampler;
class IShader;
class IPipeline;        // 图形/计算
class IDescriptorSet;
class IFence;

struct DeviceDesc { Backend backend; uint32_t flags; };
struct SwapchainDesc {
    void*  native_window;        // PAL 提供（HWND/NSWindow/...）
    uint32_t width, height;
    PixelFormat format;
    uint32_t image_count;
    Vsync vsync;
};

class IDevice {
public:
    virtual mine::core::OwnedPtr<IQueue>       create_queue(QueueType) = 0;
    virtual mine::core::OwnedPtr<ISwapchain>   create_swapchain(SwapchainDesc const&) = 0;
    virtual mine::core::OwnedPtr<IBuffer>      create_buffer(BufferDesc const&) = 0;
    virtual mine::core::OwnedPtr<ITexture>     create_texture(TextureDesc const&) = 0;
    virtual mine::core::OwnedPtr<IPipeline>    create_pipeline(PipelineDesc const&) = 0;
    virtual mine::core::OwnedPtr<ICommandList> create_command_list(QueueType) = 0;
    /* ... */
};
}
```

后端注册：每个后端在静态初始化时调用 `mine::gfx::register_backend(...)`，由 `BackendChooser` 根据平台/能力/用户偏好选择。

## 5.3 Windows 默认链路

```
Window(win32) ─► HWND
                 │
                 ▼
   ISwapchain (D3D12, FlipDiscard, Tearing)
                 │
                 ▼
       Render Thread / 帧调度 / 反向 Z（UI 用 2D 投影）
```

* **D3D12 优先**：开启 Triple buffering、`DXGI_SWAP_EFFECT_FLIP_DISCARD`、`DXGI_FEATURE_PRESENT_ALLOW_TEARING`、可变刷新率。
* **D3D11 回退**：在驱动 < FL11.0、或用户配置时启用，保证老机器能跑。
* DX12 后端使用**自管线状态缓存** + **bindless**（SM 6.6 支持时）。

## 5.4 着色器策略

* 源代码使用 HLSL（SM6）。
* 构建期 `tool.shadercc`（L9 Tooling）通过 DXC 把 HLSL 转 DXIL/SPIR-V/MSL，并产出 C++ 头（嵌入字节码）。`tool.shadercc` 是纯构建期可执行文件，不参与运行时模块依赖图。
* 运行期不携带编译器（除非显式开启 `MINE_GFX_RUNTIME_HLSL`）。

## 5.5 2D 渲染（mine.paint）

不直接耦合到任何具体 API，使用 RHI：

* `Brush`：纯色 / 线性 / 径向 / 图像 / 视口图像。
* `Pen`：宽度、连接、端点、虚线。
* `Path`：可被光栅化为：
  * **网格化**（CPU 三角化，GPU 渲染）—— 默认；
  * **MSDF/SDF**（文本/图标）—— 文本走此路径；
  * **加性合成 + 模板裁剪** —— 复杂裁剪。
* 内置**批合成器**：合并相同 pipeline 的 draw call，按 z-order/Atlas 自动 batch。
* **纹理图集**：UI 元素图标、字符 glyph 全部进入 Atlas。

## 5.6 多线程

* 每窗口独立 Render Thread。
* UI 线程构建 `DisplayList`（不可变），原子地交给 Render Thread。
* 多个 `DisplayList` 由 `CommandRecorder` 并行录制（D3D12/Vulkan）。
* `IFence` + 信号量管控帧节流（CPU 不超前 2 帧）。

## 5.7 颜色管理

* 默认输出 sRGB；HDR Window 走 scRGB(F16)。
* `mine::Color` 为线性空间；输入/输出层做转换。
* 显示器 ICC 不在 v1 范围。

## 5.8 备选/扩展点

* `mine.gfx.skia`（可选）：把 mine.paint 的 `IRenderer` 接口替换为 Skia 实现，便于与现有生态对接（不在默认 SDK）。
* 用户可注册自定义 `IRenderer` / `IBackend`，UI 层无须修改。
