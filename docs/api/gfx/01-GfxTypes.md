# GfxTypes 详细接口文档

## 概述

`GfxTypes` 是 `mine.gfx.rhi` 模块的**基础类型、枚举与描述符结构体定义**。

**核心特性：**
- **后端选择**：Backend、QueueType
- **像素格式**：PixelFormat（8-bit、16-bit、32-bit、深度/模板）
- **绑定标志**：TextureBindFlags、BufferBindFlags（位标志组合）
- **基础数据结构**：Color4f、Viewport、ScissorRect
- **资源描述符**：TextureDesc、BufferDesc、SwapchainDesc
- **着色器与管线**：ShaderLanguage、ShaderDesc、VertexSemantic、VertexElementFormat、VertexElement、PipelineDesc、StencilMode

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/GfxTypes.h
```

---

## 枚举类型

### Backend（后端选择）

```cpp
enum class Backend : int {
    D3D11,   ///< Direct3D 11（兼容后端，FL11.0+）
    D3D12,   ///< Direct3D 12（Windows 推荐，FL12.0+）
    Metal,   ///< Metal（macOS / iOS）
    Vulkan,  ///< Vulkan（跨平台）
    GLES,    ///< OpenGL ES（移动端回退）
    Auto,    ///< 运行时自动选择最优后端
};
```

**描述**：图形后端选择。

**取值**：
- `D3D11`：Direct3D 11（兼容后端，FL11.0+）
- `D3D12`：Direct3D 12（Windows 推荐，FL12.0+）
- `Metal`：Metal（macOS / iOS）
- `Vulkan`：Vulkan（跨平台）
- `GLES`：OpenGL ES（移动端回退）
- `Auto`：运行时自动选择最优后端

---

### QueueType（队列类型）

```cpp
enum class QueueType : int {
    Graphics,  ///< 图形渲染队列（含光栅化）
    Compute,   ///< 纯计算队列
    Copy,      ///< 数据传输队列
};
```

**描述**：命令队列类型。

**取值**：
- `Graphics`：图形渲染队列（含光栅化）
- `Compute`：纯计算队列
- `Copy`：数据传输队列

---

### PixelFormat（像素格式）

```cpp
enum class PixelFormat : int {
    Unknown,

    // ── 8-bit 通道格式 ──────────────────────────────────
    R8_UNorm,           ///< 8-bit 单通道，线性空间（字形图集专用）
    RGBA8_UNorm,        ///< 8-bit RGBA，线性空间
    RGBA8_UNorm_sRGB,   ///< 8-bit RGBA，sRGB 空间
    BGRA8_UNorm,        ///< 8-bit BGRA，线性空间（DXGI/Win32 首选）
    BGRA8_UNorm_sRGB,   ///< 8-bit BGRA，sRGB 空间

    // ── 浮点格式 ─────────────────────────────────────────
    R16G16B16A16_Float, ///< 16-bit RGBA 半精度浮点（HDR）
    R32G32B32A32_Float, ///< 32-bit RGBA 单精度浮点

    // ── 深度/模板格式 ────────────────────────────────────
    D24_UNorm_S8_UInt,  ///< 24-bit 深度 + 8-bit 模板
    D32_Float,          ///< 32-bit 单精度浮点深度
    D32_Float_S8_UInt,  ///< 32-bit 深度 + 8-bit 模板
};
```

**描述**：纹理/交换链像素格式。

**8-bit 通道格式**：
- `R8_UNorm`：8-bit 单通道，线性空间（字形图集专用）
- `RGBA8_UNorm`：8-bit RGBA，线性空间
- `RGBA8_UNorm_sRGB`：8-bit RGBA，sRGB 空间
- `BGRA8_UNorm`：8-bit BGRA，线性空间（DXGI/Win32 首选）
- `BGRA8_UNorm_sRGB`：8-bit BGRA，sRGB 空间

**浮点格式**：
- `R16G16B16A16_Float`：16-bit RGBA 半精度浮点（HDR）
- `R32G32B32A32_Float`：32-bit RGBA 单精度浮点

**深度/模板格式**：
- `D24_UNorm_S8_UInt`：24-bit 深度 + 8-bit 模板
- `D32_Float`：32-bit 单精度浮点深度
- `D32_Float_S8_UInt`：32-bit 深度 + 8-bit 模板

---

### Vsync（垂直同步）

```cpp
enum class Vsync : int {
    Off,      ///< 关闭（允许撕裂，最低延迟）
    On,       ///< 开启垂直同步
    Adaptive, ///< 自适应（DXGI / G-Sync / FreeSync）
};
```

**描述**：垂直同步模式。

**取值**：
- `Off`：关闭（允许撕裂，最低延迟）
- `On`：开启垂直同步
- `Adaptive`：自适应（DXGI / G-Sync / FreeSync）

---

### TextureBindFlags（纹理绑定标志）

```cpp
enum class TextureBindFlags : uint32_t {
    None            = 0,
    ShaderResource  = 1u << 0,  ///< 可作为着色器采样纹理（SRV）
    RenderTarget    = 1u << 1,  ///< 可作为渲染目标（RTV）
    DepthStencil    = 1u << 2,  ///< 可作为深度/模板缓冲（DSV）
    UnorderedAccess = 1u << 3,  ///< 可作为无序访问视图（UAV）
};
```

**描述**：纹理绑定标志（可位或组合）。

**标志**：
- `None`：无绑定
- `ShaderResource`：可作为着色器采样纹理（SRV）
- `RenderTarget`：可作为渲染目标（RTV）
- `DepthStencil`：可作为深度/模板缓冲（DSV）
- `UnorderedAccess`：可作为无序访问视图（UAV）

**辅助函数**：
```cpp
inline TextureBindFlags operator|(TextureBindFlags a, TextureBindFlags b) noexcept;
inline bool has_flag(TextureBindFlags flags, TextureBindFlags flag) noexcept;
```

---

### BufferBindFlags（缓冲绑定标志）

```cpp
enum class BufferBindFlags : uint32_t {
    None            = 0,
    Vertex          = 1u << 0, ///< 顶点缓冲
    Index           = 1u << 1, ///< 索引缓冲
    Constant        = 1u << 2, ///< 常量缓冲（Uniform Buffer）
    ShaderResource  = 1u << 3, ///< 结构化缓冲 SRV
    UnorderedAccess = 1u << 4, ///< UAV
};
```

**描述**：缓冲绑定标志（可位或组合）。

**标志**：
- `None`：无绑定
- `Vertex`：顶点缓冲
- `Index`：索引缓冲
- `Constant`：常量缓冲（Uniform Buffer）
- `ShaderResource`：结构化缓冲 SRV
- `UnorderedAccess`：UAV

**辅助函数**：
```cpp
inline BufferBindFlags operator|(BufferBindFlags a, BufferBindFlags b) noexcept;
inline bool has_flag(BufferBindFlags flags, BufferBindFlags flag) noexcept;
```

---

### ShaderLanguage（着色器语言）

```cpp
enum class ShaderLanguage : int {
    HLSL,   ///< 高级着色器语言（Direct3D 11/12）
    SPIRV,  ///< SPIR-V 中间语言（Vulkan）
    MSL,    ///< Metal 着色器语言（macOS/iOS）
};
```

**描述**：着色器源码/字节码语言标识。

**取值**：
- `HLSL`：高级着色器语言（Direct3D 11/12）
- `SPIRV`：SPIR-V 中间语言（Vulkan）
- `MSL`：Metal 着色器语言（macOS/iOS）

---

### VertexSemantic（顶点输入元素语义）

```cpp
enum class VertexSemantic : int {
    Position, ///< 位置（POSITION）
    Color,    ///< 颜色（COLOR）
    TexCoord, ///< 纹理坐标（TEXCOORD）
    Normal,   ///< 法线（NORMAL）
};
```

**描述**：顶点输入元素语义。

**取值**：
- `Position`：位置（POSITION）
- `Color`：颜色（COLOR）
- `TexCoord`：纹理坐标（TEXCOORD）
- `Normal`：法线（NORMAL）

---

### VertexElementFormat（顶点元素数据格式）

```cpp
enum class VertexElementFormat : int {
    Float2,       ///< 两个 float（8 字节）
    Float3,       ///< 三个 float（12 字节）
    Float4,       ///< 四个 float（16 字节）
    UByte4_UNorm, ///< 四个无符号字节归一化（4 字节，等价 RGBA8）
};
```

**描述**：顶点元素数据格式。

**取值**：
- `Float2`：两个 float（8 字节）
- `Float3`：三个 float（12 字节）
- `Float4`：四个 float（16 字节）
- `UByte4_UNorm`：四个无符号字节归一化（4 字节，等价 RGBA8）

---

### StencilMode（模板操作模式）

```cpp
enum class StencilMode : uint8_t {
    Off,          ///< 禁用模板测试
    ClipPush,     ///< 裁剪开始：模板值 +1
    ClipPop,      ///< 裁剪结束：模板值 -1
    ClipTest,     ///< 裁剪测试：仅当模板值 > 0 时绘制
};
```

**描述**：模板缓冲操作模式，用于形状裁剪。

**取值**：
- `Off`：禁用模板测试
- `ClipPush`：裁剪开始（模板值 +1）
- `ClipPop`：裁剪结束（模板值 -1）
- `ClipTest`：裁剪测试（仅当模板值 > 0 时绘制）

**裁剪工作原理（嵌套裁剪栈）**：
1. 初始状态：模板缓冲全部清零
2. ClipPush：绘制裁剪形状，模板值 +1
3. ClipTest：绘制内容，仅当模板值 > 0 时通过
4. ClipPop：绘制裁剪形状，模板值 -1

---

## 基础数据结构

### Color4f（清屏颜色）

```cpp
struct Color4f {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
};
```

**描述**：清屏颜色（线性 RGBA，各分量范围 [0, 1]）。

**字段**：
- `r`：红色分量（默认 0.0）
- `g`：绿色分量（默认 0.0）
- `b`：蓝色分量（默认 0.0）
- `a`：透明度分量（默认 1.0）

---

### Viewport（视口描述）

```cpp
struct Viewport {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
    float min_depth{0.0f};
    float max_depth{1.0f};
};
```

**描述**：视口描述（归一化设备坐标 → 屏幕空间映射）。

**字段**：
- `x`：左上角 X 坐标（像素）
- `y`：左上角 Y 坐标（像素）
- `width`：视口宽度（像素）
- `height`：视口高度（像素）
- `min_depth`：最小深度值（默认 0.0）
- `max_depth`：最大深度值（默认 1.0）

---

### ScissorRect（裁剪矩形）

```cpp
struct ScissorRect {
    int32_t left{0};
    int32_t top{0};
    int32_t right{0};
    int32_t bottom{0};
};
```

**描述**：裁剪矩形（整数像素坐标，左闭右开）。

**字段**：
- `left`：左边界（包含）
- `top`：上边界（包含）
- `right`：右边界（不包含）
- `bottom`：下边界（不包含）

---

## 资源描述符

### TextureDesc（纹理创建描述符）

```cpp
struct TextureDesc {
    uint32_t         width{1};
    uint32_t         height{1};
    uint32_t         depth{1};          ///< 3D 纹理层数（2D 设为 1）
    uint32_t         mip_levels{1};     ///< Mipmap 级数（0 = 自动生成完整链）
    uint32_t         array_size{1};     ///< 纹理数组大小
    PixelFormat      format{PixelFormat::RGBA8_UNorm};
    TextureBindFlags bind_flags{TextureBindFlags::ShaderResource};
};
```

**描述**：纹理创建描述符。

**字段**：
- `width`：纹理宽度（默认 1）
- `height`：纹理高度（默认 1）
- `depth`：3D 纹理层数（2D 设为 1，默认 1）
- `mip_levels`：Mipmap 级数（0 = 自动生成完整链，默认 1）
- `array_size`：纹理数组大小（默认 1）
- `format`：像素格式（默认 RGBA8_UNorm）
- `bind_flags`：绑定标志（默认 ShaderResource）

---

### BufferDesc（缓冲创建描述符）

```cpp
struct BufferDesc {
    uint64_t        size{0};    ///< 缓冲字节大小
    uint32_t        stride{0};  ///< 结构化缓冲元素步长（字节）
    BufferBindFlags bind_flags{BufferBindFlags::Vertex};
};
```

**描述**：缓冲创建描述符。

**字段**：
- `size`：缓冲字节大小（默认 0）
- `stride`：结构化缓冲元素步长（字节，默认 0）
- `bind_flags`：绑定标志（默认 Vertex）

---

### SwapchainDesc（交换链创建描述符）

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

**字段**：
- `native_window`：平台原生窗口句柄（Win32: HWND，默认 nullptr）
- `width`：交换链宽度（默认 0）
- `height`：交换链高度（默认 0）
- `format`：像素格式（推荐使用 sRGB 格式，默认 BGRA8_UNorm_sRGB）
- `image_count`：缓冲帧数（2=双缓冲，3=三缓冲，默认 3）
- `vsync`：垂直同步模式（默认 On）

---

## 着色器与管线

### ShaderDesc（着色器描述符）

```cpp
struct ShaderDesc {
    ShaderLanguage language{ShaderLanguage::HLSL};
    const char*    source{nullptr};      ///< 源码字符串（零结尾）
    size_t         source_length{0};     ///< 源码长度（字节）
    const void*    bytecode{nullptr};    ///< 预编译字节码（可选）
    size_t         bytecode_length{0};   ///< 字节码长度（字节）
    const char*    entry_point{nullptr}; ///< 入口点函数名（默认 "main"）
};
```

**描述**：单个着色器描述符（含源码或预编译字节码）。

**字段**：
- `language`：着色器语言（默认 HLSL）
- `source`：源码字符串（零结尾，默认 nullptr）
- `source_length`：源码长度（字节，默认 0）
- `bytecode`：预编译字节码（可选，默认 nullptr）
- `bytecode_length`：字节码长度（字节，默认 0）
- `entry_point`：入口点函数名（默认 nullptr，使用 "main"）

**特征**：
- M0 阶段：D3D11 后端接受 HLSL 源码，在 create_pipeline() 内部调用 D3DCompile 完成编译
- 后续工具链（tool.shadercc）可提供预编译字节码

---

### VertexElement（顶点输入布局元素）

```cpp
struct VertexElement {
    VertexSemantic      semantic{VertexSemantic::Position}; ///< 语义类型
    uint32_t            semantic_index{0};                  ///< 语义索引（多纹理坐标时使用）
    VertexElementFormat format{VertexElementFormat::Float2};///< 数据格式
    uint32_t            slot{0};                            ///< 顶点缓冲绑定槽位
    uint32_t            offset{0};                          ///< 元素在顶点结构内的字节偏移
};
```

**描述**：顶点输入布局元素（对应 D3D11 D3D11_INPUT_ELEMENT_DESC）。

**字段**：
- `semantic`：语义类型（默认 Position）
- `semantic_index`：语义索引（多纹理坐标时使用，默认 0）
- `format`：数据格式（默认 Float2）
- `slot`：顶点缓冲绑定槽位（默认 0）
- `offset`：元素在顶点结构内的字节偏移（默认 0）

---

### PipelineDesc（图形管线创建描述符）

```cpp
struct PipelineDesc {
    ShaderDesc   vertex_shader;                  ///< 顶点着色器
    ShaderDesc   pixel_shader;                   ///< 像素着色器
    VertexElement vertex_elements[8]{};          ///< 顶点输入布局（最多 8 个元素）
    uint32_t     vertex_element_count{0};        ///< 实际使用的元素数量
    uint32_t     vertex_stride{0};               ///< 单个顶点字节大小
    bool         enable_blend{false};            ///< 是否启用 Alpha 混合（预乘 Alpha 模式）
    bool         enable_depth{false};            ///< 是否启用深度测试（2D 渲染通常关闭）
    StencilMode  stencil_mode{StencilMode::Off}; ///< 模板模式（裁剪用）
};
```

**描述**：图形管线创建描述符（M0 最小集，支持 2D 纯色渲染）。

**字段**：
- `vertex_shader`：顶点着色器
- `pixel_shader`：像素着色器
- `vertex_elements`：顶点输入布局（最多 8 个元素）
- `vertex_element_count`：实际使用的元素数量（默认 0）
- `vertex_stride`：单个顶点字节大小（默认 0）
- `enable_blend`：是否启用 Alpha 混合（预乘 Alpha 模式，默认 false）
- `enable_depth`：是否启用深度测试（2D 渲染通常关闭，默认 false）
- `stencil_mode`：模板模式（裁剪用，默认 Off）

---

## 使用场景

### 1. 创建设备并选择后端

```cpp
using namespace mine::gfx;

// 创建 D3D11 设备
auto device = create_device(Backend::D3D11);

// 或使用 Auto 自动选择
auto device = create_device(Backend::Auto);
```

---

### 2. 创建交换链

```cpp
// 交换链描述符
SwapchainDesc desc;
desc.native_window = hwnd;  // Win32 窗口句柄
desc.width = 1920;
desc.height = 1080;
desc.format = PixelFormat::BGRA8_UNorm_sRGB;  // sRGB 格式
desc.image_count = 3;  // 三缓冲
desc.vsync = Vsync::On;

// 创建交换链
auto swapchain = device->create_swapchain(desc);
```

---

### 3. 创建纹理

```cpp
// 纹理描述符
TextureDesc desc;
desc.width = 512;
desc.height = 512;
desc.format = PixelFormat::RGBA8_UNorm;
desc.bind_flags = TextureBindFlags::ShaderResource | TextureBindFlags::RenderTarget;

// 创建纹理
auto texture = device->create_texture(desc);
```

---

### 4. 创建缓冲

```cpp
// 顶点缓冲描述符
BufferDesc desc;
desc.size = sizeof(vertices);
desc.stride = sizeof(Vertex);
desc.bind_flags = BufferBindFlags::Vertex;

// 创建顶点缓冲
auto buffer = device->create_buffer(desc, vertices);
```

---

### 5. 创建图形管线

```cpp
// 顶点输入布局
VertexElement elements[2] = {
    {VertexSemantic::Position, 0, VertexElementFormat::Float2, 0, 0},
    {VertexSemantic::Color, 0, VertexElementFormat::Float4, 0, 8},
};

// 管线描述符
PipelineDesc desc;
desc.vertex_shader = {ShaderLanguage::HLSL, vs_source, vs_length, nullptr, 0, "main"};
desc.pixel_shader = {ShaderLanguage::HLSL, ps_source, ps_length, nullptr, 0, "main"};
desc.vertex_elements[0] = elements[0];
desc.vertex_elements[1] = elements[1];
desc.vertex_element_count = 2;
desc.vertex_stride = sizeof(Vertex);
desc.enable_blend = true;

// 创建管线
auto pipeline = device->create_pipeline(desc);
```

---

### 6. 使用位标志组合

```cpp
// 组合纹理绑定标志
auto flags = TextureBindFlags::ShaderResource | TextureBindFlags::RenderTarget;

// 检查标志
if (has_flag(flags, TextureBindFlags::RenderTarget)) {
    // 可以作为渲染目标
}
```

---

## 性能分析

### 枚举大小

**特征**：
- `Backend`, `QueueType`, `PixelFormat`, `Vsync`, `ShaderLanguage`, `VertexSemantic`, `VertexElementFormat`：4 字节（int）
- `TextureBindFlags`, `BufferBindFlags`：4 字节（uint32_t）
- `StencilMode`：1 字节（uint8_t）

---

### 结构体大小

**特征**：
- `Color4f`：16 字节（4 x float）
- `Viewport`：24 字节（6 x float）
- `ScissorRect`：16 字节（4 x int32_t）
- `TextureDesc`：~32 字节
- `BufferDesc`：~24 字节
- `SwapchainDesc`：~32 字节
- `ShaderDesc`：~48 字节
- `VertexElement`：~20 字节
- `PipelineDesc`：~256 字节（含数组）

---

## 最佳实践

### 1. 使用 sRGB 格式作为交换链格式

```cpp
// ✅ 推荐：使用 sRGB 格式
SwapchainDesc desc;
desc.format = PixelFormat::BGRA8_UNorm_sRGB;  // sRGB 格式，正确伽马校正

// ❌ 不推荐：使用线性格式
SwapchainDesc desc;
desc.format = PixelFormat::BGRA8_UNorm;  // 线性格式，颜色偏暗
```

---

### 2. 使用三缓冲提升流畅度

```cpp
// ✅ 推荐：三缓冲
SwapchainDesc desc;
desc.image_count = 3;  // 更流畅

// ✅ 也可以：双缓冲
SwapchainDesc desc;
desc.image_count = 2;  // 节省显存
```

---

### 3. 使用位标志组合

```cpp
// ✅ 推荐：使用位或组合标志
auto flags = TextureBindFlags::ShaderResource | TextureBindFlags::RenderTarget;

// ❌ 不推荐：只使用单一标志（如果需要多个用途）
auto flags = TextureBindFlags::ShaderResource;  // 无法作为渲染目标
```

---

### 4. 按需启用深度测试和混合

```cpp
// ✅ 推荐：2D 渲染关闭深度测试，启用混合
PipelineDesc desc;
desc.enable_blend = true;
desc.enable_depth = false;

// ✅ 也可以：3D 渲染启用深度测试
PipelineDesc desc;
desc.enable_depth = true;
```

---

## 常见陷阱

### 1. 忘记设置 vertex_element_count

```cpp
// ❌ 问题：忘记设置 vertex_element_count
PipelineDesc desc;
desc.vertex_elements[0] = {...};
desc.vertex_elements[1] = {...};
// 缺少 desc.vertex_element_count = 2;

// ✅ 解决：设置 vertex_element_count
PipelineDesc desc;
desc.vertex_elements[0] = {...};
desc.vertex_elements[1] = {...};
desc.vertex_element_count = 2;
```

---

### 2. vertex_stride 与实际顶点大小不符

```cpp
// ❌ 问题：vertex_stride 与实际顶点大小不符
struct Vertex {
    float x, y;     // 8 字节
    float r, g, b, a; // 16 字节
};  // 总计 24 字节

PipelineDesc desc;
desc.vertex_stride = 20;  // 错误：应为 24

// ✅ 解决：使用 sizeof(Vertex)
PipelineDesc desc;
desc.vertex_stride = sizeof(Vertex);  // 24 字节
```

---

### 3. VertexElement offset 计算错误

```cpp
// ❌ 问题：offset 计算错误
struct Vertex {
    float x, y;     // offset 0，8 字节
    float r, g, b, a; // offset 8，16 字节
};

VertexElement elements[2] = {
    {VertexSemantic::Position, 0, VertexElementFormat::Float2, 0, 0},
    {VertexSemantic::Color, 0, VertexElementFormat::Float4, 0, 12},  // 错误：应为 8
};

// ✅ 解决：使用 offsetof 或手动计算
VertexElement elements[2] = {
    {VertexSemantic::Position, 0, VertexElementFormat::Float2, 0, 0},
    {VertexSemantic::Color, 0, VertexElementFormat::Float4, 0, 8},  // 正确
};
```

---

### 4. 混合使用线性和 sRGB 格式

```cpp
// ❌ 问题：混合使用线性和 sRGB 格式
SwapchainDesc desc;
desc.format = PixelFormat::BGRA8_UNorm;  // 线性格式

TextureDesc tex_desc;
tex_desc.format = PixelFormat::RGBA8_UNorm_sRGB;  // sRGB 格式

// 渲染时颜色会偏差

// ✅ 解决：统一使用 sRGB 格式
SwapchainDesc desc;
desc.format = PixelFormat::BGRA8_UNorm_sRGB;  // sRGB 格式

TextureDesc tex_desc;
tex_desc.format = PixelFormat::RGBA8_UNorm_sRGB;  // sRGB 格式
```

---

## 完整示例

```cpp
#include <mine/gfx/Gfx.h>

using namespace mine::gfx;

int main() {
    // 创建设备
    auto device = create_device(Backend::D3D11);
    
    // 创建交换链
    SwapchainDesc swap_desc;
    swap_desc.native_window = hwnd;
    swap_desc.width = 1920;
    swap_desc.height = 1080;
    swap_desc.format = PixelFormat::BGRA8_UNorm_sRGB;
    swap_desc.image_count = 3;
    swap_desc.vsync = Vsync::On;
    auto swapchain = device->create_swapchain(swap_desc);
    
    // 创建顶点缓冲
    struct Vertex {
        float x, y;
        float r, g, b, a;
    };
    Vertex vertices[] = {
        {0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
        {-0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f},
    };
    
    BufferDesc buf_desc;
    buf_desc.size = sizeof(vertices);
    buf_desc.stride = sizeof(Vertex);
    buf_desc.bind_flags = BufferBindFlags::Vertex;
    auto vertex_buffer = device->create_buffer(buf_desc, vertices);
    
    // 创建管线
    VertexElement elements[2] = {
        {VertexSemantic::Position, 0, VertexElementFormat::Float2, 0, 0},
        {VertexSemantic::Color, 0, VertexElementFormat::Float4, 0, 8},
    };
    
    PipelineDesc pipe_desc;
    pipe_desc.vertex_shader = {ShaderLanguage::HLSL, vs_source, vs_length, nullptr, 0, "main"};
    pipe_desc.pixel_shader = {ShaderLanguage::HLSL, ps_source, ps_length, nullptr, 0, "main"};
    pipe_desc.vertex_elements[0] = elements[0];
    pipe_desc.vertex_elements[1] = elements[1];
    pipe_desc.vertex_element_count = 2;
    pipe_desc.vertex_stride = sizeof(Vertex);
    pipe_desc.enable_blend = true;
    auto pipeline = device->create_pipeline(pipe_desc);
    
    // 创建命令列表
    auto cmd = device->create_command_list();
    
    // 渲染循环
    while (running) {
        cmd->begin();
        cmd->set_render_target(swapchain->current_render_target());
        cmd->clear({0.1f, 0.1f, 0.1f, 1.0f});
        cmd->set_pipeline(pipeline.get());
        cmd->set_vertex_buffer(vertex_buffer.get(), 0);
        cmd->draw(3, 0);
        cmd->end();
        
        auto queue = device->create_queue(QueueType::Graphics);
        queue->submit(cmd.get());
        swapchain->present();
    }
    
    return 0;
}
```

---

## 总结

`GfxTypes` 是 `mine.gfx.rhi` 模块的基础类型、枚举与描述符结构体定义，具备：

1. **后端选择**：Backend、QueueType
2. **像素格式**：PixelFormat（8-bit、16-bit、32-bit、深度/模板）
3. **绑定标志**：TextureBindFlags、BufferBindFlags（位标志组合）
4. **基础数据结构**：Color4f、Viewport、ScissorRect
5. **资源描述符**：TextureDesc、BufferDesc、SwapchainDesc
6. **着色器与管线**：ShaderLanguage、ShaderDesc、VertexSemantic、VertexElementFormat、VertexElement、PipelineDesc、StencilMode

**使用建议**：
- 使用 sRGB 格式作为交换链格式
- 使用三缓冲提升流畅度
- 使用位标志组合
- 按需启用深度测试和混合
- 检查 vertex_element_count 和 vertex_stride
- 使用 offsetof 计算 VertexElement offset
- 统一使用 sRGB 格式
